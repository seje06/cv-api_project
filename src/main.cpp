#include <iostream>                 // std::cout 같은 콘솔 출력
#include <string>                   // std::string
#include <vector>                   // std::vector
#include <stdexcept>                // std::runtime_error
#include <httplib.h>                // HTTP 서버 라이브러리
#include <nlohmann/json.hpp>        // JSON 파싱/생성 라이브러리
#include <opencv2/opencv.hpp>       // OpenCV 이미지 처리
#include <curl/curl.h>              // libcurl: URL에서 데이터 다운로드

// nlohmann::json을 매번 길게 쓰기 귀찮으므로 json 별칭으로 사용
using json = nlohmann::json;

/*
    ------------------------------------------------------------------------
    WriteCallback
    ------------------------------------------------------------------------
    libcurl이 인터넷에서 데이터를 받아올 때,
    받아온 바이트를 어디에 저장할지 알려주는 콜백 함수이다.

    libcurl은 다운로드를 한 번에 끝까지 다 주는 것이 아니라,
    내부적으로 여러 번 나눠서 데이터를 전달할 수 있다.

    그때마다 이 함수가 호출되고,
    우리는 전달받은 바이트를 vector<unsigned char>에 계속 이어 붙인다.

    매개변수 설명:
    - contents : 이번에 새로 도착한 데이터 조각의 시작 주소
    - size     : 한 원소의 크기
    - nmemb    : 원소 개수
    - userp    : 사용자가 넘긴 포인터 (여기서는 vector<unsigned char>*)

    실제 받은 총 바이트 수는 size * nmemb 이다.
*/
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    // 이번 콜백에서 전달된 총 바이트 수 계산
    size_t totalSize = size * nmemb;

    // userp는 void*라 타입 정보가 없으므로,
    // 원래 우리가 넘긴 타입인 vector<unsigned char>*로 다시 변환한다.
    auto* buffer = static_cast<std::vector<unsigned char>*>(userp);

    // contents도 void*이므로 실제 바이트 데이터로 보기 위해 unsigned char*로 변환
    unsigned char* data = static_cast<unsigned char*>(contents);

    // 기존 buffer의 맨 뒤에 새로 받은 바이트 조각을 이어 붙인다.
    // [data, data + totalSize) 범위의 데이터를 append
    buffer->insert(buffer->end(), data, data + totalSize);

    // libcurl에게 "이만큼 정상적으로 처리했다"고 알려준다.
    // 보통 totalSize를 그대로 반환해야 한다.
    return totalSize;
}

/*
    ------------------------------------------------------------------------
    DownloadImage
    ------------------------------------------------------------------------
    이미지 URL을 받아서 실제 이미지 바이너리 데이터를 다운로드하는 함수

    매개변수 설명:
    - url      : 다운로드할 이미지 주소
    - outData  : 다운로드된 이미지 바이트를 저장할 벡터
    - errMsg   : 실패 시 원인을 담을 문자열

    반환값:
    - 성공 : true
    - 실패 : false
*/
bool DownloadImage(const std::string& url, std::vector<unsigned char>& outData, std::string& errMsg)
{
    // libcurl 핸들 생성
    // 이 핸들에 URL, 콜백 함수 등 여러 옵션을 설정한 뒤 요청을 수행한다.
    CURL* curl = curl_easy_init();

    // curl 초기화 실패 시 바로 반환
    if (!curl)
    {
        errMsg = "Failed to initialize CURL";
        return false;
    }

    // 어떤 URL로 요청할지 설정
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // 응답 데이터가 도착할 때마다 호출할 콜백 함수 등록
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    // 콜백 함수의 userp 인자로 넘길 값 설정
    // 즉 WriteCallback의 네 번째 인자로 &outData가 전달된다.
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outData);

    // 리다이렉트 허용
    // 예: http -> https, 또는 이미지 CDN 주소로 재이동
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // 너무 오래 걸리는 요청 방지용 타임아웃 (10초)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    // 실제 HTTP 요청 수행
    CURLcode res = curl_easy_perform(curl);

    // 네트워크 수준에서 실패한 경우
    if (res != CURLE_OK)
    {
        // 사람이 읽을 수 있는 에러 문자열 저장
        errMsg = curl_easy_strerror(res);

        // curl 자원 해제
        curl_easy_cleanup(curl);
        return false;
    }

    // HTTP 응답 코드(200, 404, 500 등) 확인
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

    // curl 자원 정리
    curl_easy_cleanup(curl);

    // 200 OK가 아니면 실패 처리
    if (responseCode != 200)
    {
        errMsg = "HTTP response code: " + std::to_string(responseCode);
        return false;
    }

    // 여기까지 왔으면 다운로드 성공
    return true;
}

/*
    ------------------------------------------------------------------------
    GetIntOption
    ------------------------------------------------------------------------
    options JSON 객체 안에서 int 값을 읽어오는 보조 함수

    예:
    options = { "ksize": 9 }

    GetIntOption(options, "ksize", 5) -> 9
    GetIntOption(options, "width", 100) -> width가 없으면 100 반환

    장점:
    - 값이 없을 때 기본값 사용 가능
    - 타입이 틀리면 예외 발생시켜서 잘못된 요청을 빠르게 잡을 수 있음
*/
int GetIntOption(const json& options, const std::string& key, int defaultValue)
{
    // 해당 키가 없으면 기본값 반환
    if (!options.contains(key))
        return defaultValue;

    // 키는 있는데 정수 타입이 아니면 잘못된 요청으로 간주
    if (!options[key].is_number_integer())
        throw std::runtime_error("Option '" + key + "' must be an integer");

    // 정상적으로 int 값 추출
    return options[key].get<int>();
}

/*
    ------------------------------------------------------------------------
    GetDoubleOption
    ------------------------------------------------------------------------
    options JSON 객체 안에서 double 값을 읽어오는 보조 함수

    예:
    options = { "sigma": 2.0 }

    GetDoubleOption(options, "sigma", 1.0) -> 2.0
*/
double GetDoubleOption(const json& options, const std::string& key, double defaultValue)
{
    // 키가 없으면 기본값 반환
    if (!options.contains(key))
        return defaultValue;

    // 숫자 타입인지 검사
    // 정수/실수 모두 number 판정이 가능하다.
    if (!options[key].is_number())
        throw std::runtime_error("Option '" + key + "' must be a number");

    // double로 변환
    return options[key].get<double>();
}

/*
    ------------------------------------------------------------------------
    MakePositiveOdd
    ------------------------------------------------------------------------
    blur 커널 크기처럼 "양수 홀수"가 필요한 값을 보정하는 함수

    예:
    - 8  -> 9
    - 0  -> 1
    - -3 -> 1

    OpenCV의 GaussianBlur 커널은 보통 1 이상의 홀수 크기가 필요하다.
*/
int MakePositiveOdd(int value)
{
    // 1보다 작으면 최소값 1로 보정
    if (value < 1)
        value = 1;

    // 짝수면 1 더해서 홀수로 보정
    if (value % 2 == 0)
        value += 1;

    return value;
}

/*
    ------------------------------------------------------------------------
    ApplySingleOperation
    ------------------------------------------------------------------------
    operations 배열에 들어있는 단일 작업 1개를 적용하는 함수

    입력:
    - input : 현재 단계의 이미지
    - op    : JSON 작업 객체
             예) { "type": "blur", "options": { "ksize": 9, "sigma": 2.0 } }

    반환:
    - 처리 결과 이미지(cv::Mat)

    지원 작업:
    - grayscale
    - blur
    - canny_edge
    - resize
*/
cv::Mat ApplySingleOperation(const cv::Mat& input, const json& op)
{
    // operation에는 최소한 type 문자열이 있어야 한다.
    if (!op.contains("type") || !op["type"].is_string())
    {
        throw std::runtime_error("Each operation must contain string field 'type'");
    }

    // 실제 operation 이름 추출
    std::string type = op["type"].get<std::string>();

    // options는 없어도 되므로 기본적으로 빈 객체로 시작
    json options = json::object();

    // options가 들어왔다면 객체 타입인지 확인 후 복사
    if (op.contains("options"))
    {
        if (!op["options"].is_object())
        {
            throw std::runtime_error("'options' must be an object");
        }

        options = op["options"];
    }

    // 처리 결과를 담을 이미지
    cv::Mat result;

    /*
        --------------------------------------------------------------------
        grayscale
        --------------------------------------------------------------------
        컬러 이미지를 흑백으로 바꾸는 작업
    */
    if (type == "grayscale")
    {
        // 이미 1채널 흑백 이미지면 그대로 복사
        if (input.channels() == 1)
        {
            result = input.clone();
        }
        else
        {
            // BGR 컬러 -> GRAY 흑백 변환
            cv::cvtColor(input, result, cv::COLOR_BGR2GRAY);
        }
    }
    /*
        --------------------------------------------------------------------
        blur
        --------------------------------------------------------------------
        가우시안 블러 적용

        options:
        - ksize : 커널 크기 (기본값 9)
        - sigma : 블러 강도에 관련된 sigma 값 (기본값 2.0)
    */
    else if (type == "blur")
    {
        // JSON에서 옵션 읽기
        int ksize = GetIntOption(options, "ksize", 9);
        double sigma = GetDoubleOption(options, "sigma", 2.0);

        // OpenCV에서 쓰기 좋도록 양수 홀수로 보정
        ksize = MakePositiveOdd(ksize);

        // 가우시안 블러 적용
        cv::GaussianBlur(input, result, cv::Size(ksize, ksize), sigma);
    }
    /*
        --------------------------------------------------------------------
        canny_edge
        --------------------------------------------------------------------
        캐니 엣지 검출

        options:
        - threshold1 : 첫 번째 임계값 (기본값 100.0)
        - threshold2 : 두 번째 임계값 (기본값 200.0)

        캐니 엣지는 보통 흑백 이미지 기준으로 수행되므로,
        입력이 컬러면 먼저 그레이스케일로 변환한다.
    */
    else if (type == "canny_edge")
    {
        // threshold 옵션 읽기
        double threshold1 = GetDoubleOption(options, "threshold1", 100.0);
        double threshold2 = GetDoubleOption(options, "threshold2", 200.0);

        // 캐니 입력용 그레이스케일 이미지
        cv::Mat gray;

        // 이미 흑백이면 그대로 사용
        if (input.channels() == 1)
        {
            gray = input;
        }
        else
        {
            // 컬러 -> 흑백 변환
            cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
        }

        // 캐니 엣지 적용
        cv::Canny(gray, result, threshold1, threshold2);
    }
    /*
        --------------------------------------------------------------------
        resize
        --------------------------------------------------------------------
        이미지 크기 변경

        options:
        - width  : 목표 너비
        - height : 목표 높이
    */
    else if (type == "resize")
    {
        // width/height는 resize에선 사실상 필수라고 보는 게 맞다.
        int width = GetIntOption(options, "width", 0);
        int height = GetIntOption(options, "height", 0);

        // 0 이하이면 잘못된 값
        if (width <= 0 || height <= 0)
        {
            throw std::runtime_error("resize requires positive integer options 'width' and 'height'");
        }

        // 이미지 크기 변경
        cv::resize(input, result, cv::Size(width, height));
    }
    /*
        --------------------------------------------------------------------
        지원하지 않는 operation
        --------------------------------------------------------------------
    */
    else
    {
        throw std::runtime_error("Unsupported operation: " + type);
    }

    // 처리 결과 반환
    return result;
}

/*
    ------------------------------------------------------------------------
    ApplyOperations
    ------------------------------------------------------------------------
    여러 operation을 순서대로 적용하는 함수

    예를 들어 operations가
    [
      { "type": "grayscale" },
      { "type": "blur", "options": { "ksize": 9 } }
    ]
    라면

    1. 원본 이미지에 grayscale 적용
    2. 그 결과에 blur 적용
    순서로 처리한다.
*/
cv::Mat ApplyOperations(const cv::Mat& original, const json& operations)
{
    // operations는 비어 있지 않은 배열이어야 한다.
    if (!operations.is_array() || operations.empty())
    {
        throw std::runtime_error("'operations' must be a non-empty array");
    }

    // 원본을 바로 손대지 않도록 복사본에서 시작
    cv::Mat current = original.clone();

    // 배열에 들어 있는 작업을 순서대로 적용
    for (const auto& op : operations)
    {
        current = ApplySingleOperation(current, op);
    }

    // 최종 결과 반환
    return current;
}

/*
    ------------------------------------------------------------------------
    ResolveOutputFormat
    ------------------------------------------------------------------------
    요청에서 받은 output_format 문자열을 바탕으로
    OpenCV 인코딩 확장자와 MIME 타입을 결정하는 함수

    예:
    - "jpg"  -> ".jpg", "image/jpeg"
    - "png"  -> ".png", "image/png"

    반환값:
    - 지원되는 포맷이면 true
    - 아니면 false
*/
bool ResolveOutputFormat(const std::string& outputFormat, std::string& extension, std::string& mimeType)
{
    if (outputFormat == "jpg" || outputFormat == "jpeg")
    {
        extension = ".jpg";
        mimeType = "image/jpeg";
        return true;
    }
    else if (outputFormat == "png")
    {
        extension = ".png";
        mimeType = "image/png";
        return true;
    }

    // 지원하지 않는 포맷
    return false;
}

int main()
{
    // libcurl 전역 초기화
    // 프로그램 시작 시 한 번 호출
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // HTTP 서버 객체 생성
    httplib::Server svr;

    /*
        --------------------------------------------------------------------
        GET /health
        --------------------------------------------------------------------
        서버가 살아있는지 확인하는 테스트용 엔드포인트

        브라우저나 curl로:
        http://localhost:8080/health

        요청하면 JSON 응답을 반환한다.
    */
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res)
    {
        // 응답용 JSON 생성
        json response = {
            {"status", "ok"},
            {"message", "server is running"}
        };

        // JSON 문자열로 직렬화해서 응답 본문에 넣음
        res.set_content(response.dump(4), "application/json");
    });

    /*
        --------------------------------------------------------------------
        POST /api/v1/process
        --------------------------------------------------------------------
        핵심 API

        요청 예시(JSON):
        {
          "image_url": "https://....jpg",
          "operations": [
            { "type": "grayscale" },
            {
              "type": "blur",
              "options": {
                "ksize": 9,
                "sigma": 2.0
              }
            },
            {
              "type": "canny_edge",
              "options": {
                "threshold1": 80,
                "threshold2": 160
              }
            }
          ],
          "output_format": "jpg"
        }

        동작 순서:
        1. JSON 파싱
        2. image_url, operations 검사
        3. URL에서 이미지 다운로드
        4. OpenCV로 디코딩
        5. operations를 순서대로 적용
        6. output_format에 맞춰 인코딩
        7. 결과 이미지 바이트를 HTTP 응답으로 반환
    */
    svr.Post("/api/v1/process", [](const httplib::Request& req, httplib::Response& res)
    {
        try
        {
            // 클라이언트가 보낸 POST body 문자열을 JSON으로 파싱
            json requestBody = json::parse(req.body);

            // image_url 존재 여부와 타입 검사
            if (!requestBody.contains("image_url") || !requestBody["image_url"].is_string())
            {
                json error = {
                    {"error", "string field 'image_url' is required"}
                };

                res.status = 400;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            // operations 존재 여부 검사
            if (!requestBody.contains("operations"))
            {
                json error = {
                    {"error", "field 'operations' is required"}
                };

                res.status = 400;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            // 실제 요청 값 추출
            std::string imageUrl = requestBody["image_url"].get<std::string>();
            json operations = requestBody["operations"];

            /*
                output_format은 선택값
                안 오면 jpg를 기본값으로 사용
            */
            std::string outputFormat = "jpg";

            if (requestBody.contains("output_format"))
            {
                // 타입 검증
                if (!requestBody["output_format"].is_string())
                {
                    json error = {
                        {"error", "'output_format' must be a string"}
                    };

                    res.status = 400;
                    res.set_content(error.dump(4), "application/json");
                    return;
                }

                outputFormat = requestBody["output_format"].get<std::string>();
            }

            // 다운로드한 이미지 바이트를 담을 버퍼
            std::vector<unsigned char> imageData;

            // 실패 원인을 담을 문자열
            std::string errMsg;

            // URL에서 이미지 다운로드 시도
            if (!DownloadImage(imageUrl, imageData, errMsg))
            {
                json error = {
                    {"error", "failed to download image"},
                    {"detail", errMsg}
                };

                res.status = 400;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            /*
                다운로드된 imageData는 단순한 "파일 바이트"이므로
                OpenCV가 처리 가능한 cv::Mat 이미지 객체로 바꿔야 한다.

                cv::IMREAD_COLOR:
                어떤 입력이든 우선 컬러 이미지 형태로 읽는다.
            */
            cv::Mat img = cv::imdecode(imageData, cv::IMREAD_COLOR);

            // 디코딩 실패 시 img.empty() == true
            if (img.empty())
            {
                json error = {
                    {"error", "failed to decode image"}
                };

                res.status = 400;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            // 여러 operation을 순서대로 적용
            cv::Mat result = ApplyOperations(img, operations);

            // output_format -> 인코딩 확장자, MIME 타입 변환
            std::string extension;
            std::string mimeType;

            if (!ResolveOutputFormat(outputFormat, extension, mimeType))
            {
                json error = {
                    {"error", "unsupported output_format"},
                    {"output_format", outputFormat}
                };

                res.status = 400;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            /*
                result는 아직 OpenCV 내부 이미지 객체(cv::Mat)이므로,
                HTTP 응답으로 보내기 위해 jpg/png 바이트로 다시 인코딩해야 한다.
            */
            std::vector<unsigned char> encoded;

            bool ok = cv::imencode(extension, result, encoded);

            // 인코딩 실패 시 서버 내부 오류 처리
            if (!ok)
            {
                json error = {
                    {"error", "failed to encode result image"}
                };

                res.status = 500;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            /*
                결과 이미지의 가로/세로 크기를 헤더에 실어 보냄
                SDK가 이 값을 참고해서 후처리하기 편할 수 있다.
            */
            res.set_header("X-Image-Width", std::to_string(result.cols));
            res.set_header("X-Image-Height", std::to_string(result.rows));

            /*
                최종 응답:
                encoded 벡터 안에는 실제 jpg/png 파일 바이트가 들어 있다.
                그걸 그대로 HTTP body에 실어 보낸다.
            */
            res.set_content(
                reinterpret_cast<const char*>(encoded.data()), // 바이트 시작 주소
                encoded.size(),                                // 바이트 길이
                mimeType                                       // MIME 타입
            );
        }
        catch (const std::exception& e)
        {
            /*
                JSON 파싱 실패,
                옵션 타입 오류,
                지원하지 않는 operation 등
                예외가 발생하면 여기서 JSON 에러 응답으로 반환
            */
            json error = {
                {"error", "invalid request"},
                {"detail", e.what()}
            };

            res.status = 400;
            res.set_content(error.dump(4), "application/json");
        }
    });

    // 서버 시작 메시지 출력
    std::cout << "Server started on 0.0.0.0:8080\n";

    /*
        서버 실행
        - 0.0.0.0 : 모든 네트워크 인터페이스에서 요청 받음
        - 8080    : 포트 번호
    */
    svr.listen("0.0.0.0", 8080);

    // 서버 종료 후 libcurl 전역 정리
    curl_global_cleanup();

    return 0;
}