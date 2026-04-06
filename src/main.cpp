#include <iostream>                 // std::cout 같은 입출력용
#include <string>                   // std::string
#include <vector>                   // std::vector
#include <httplib.h>                // HTTP 서버 라이브러리
#include <nlohmann/json.hpp>        // JSON 파싱/생성 라이브러리
#include <opencv2/opencv.hpp>       // OpenCV 이미지 처리
#include <curl/curl.h>              // libcurl: URL로부터 데이터 다운로드

// nlohmann::json을 매번 길게 쓰기 귀찮으니 json이라는 별칭으로 사용
using json = nlohmann::json;

/*
    libcurl이 인터넷에서 데이터를 받아올 때,
    받아온 바이트 데이터를 어디에 저장할지 정하는 콜백 함수.

    contents : 실제로 받아온 데이터 시작 주소
    size     : 한 덩어리 원소 크기
    nmemb    : 원소 개수
    userp    : 우리가 넘겨준 사용자 데이터 포인터
              여기서는 std::vector<unsigned char>* 를 넘겨줌

    반환값:
    실제로 처리한 바이트 수를 리턴해야 함.
    보통 size * nmemb 전체를 처리했으면 그 값을 그대로 리턴.
*/
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    // 이번 콜백에서 받은 총 바이트 수
    size_t totalSize = size * nmemb;

    // userp는 void* 이므로 원래 타입(vector<unsigned char>*)으로 다시 변환
    auto* buffer = static_cast<std::vector<unsigned char>*>(userp);

    // 받아온 원시 데이터도 unsigned char*로 변환
    unsigned char* data = static_cast<unsigned char*>(contents);

    // buffer 벡터 뒤에 새로 받은 데이터(data ~ data + totalSize)를 이어 붙임
    buffer->insert(buffer->end(), data, data + totalSize);

    // 반드시 "몇 바이트 처리했는지" 반환해야 함
    return totalSize;
}

/*
    이미지 URL을 받아서 실제 이미지 바이너리 데이터를 다운로드하는 함수

    url      : 다운로드할 이미지 주소
    outData  : 다운로드된 이미지 바이트를 저장할 벡터
    errMsg   : 실패 시 에러 메시지 저장

    성공하면 true, 실패하면 false 반환
*/
bool DownloadImage(const std::string& url, std::vector<unsigned char>& outData, std::string& errMsg)
{
    // CURL 핸들 생성
    CURL* curl = curl_easy_init();

    // CURL 초기화 실패
    if (!curl)
    {
        errMsg = "Failed to initialize CURL";
        return false;
    }

    // 어떤 URL로 요청할지 설정
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // 응답 데이터를 받을 때 호출할 콜백 함수 설정
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    // 콜백 함수의 네 번째 인자(userp)로 넘길 포인터 설정
    // 여기서는 outData 벡터 주소를 넘김
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outData);

    // 리다이렉트 허용 (예: http -> https 이동)
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // 너무 오래 기다리지 않도록 타임아웃 설정 (10초)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    // 실제 요청 수행
    CURLcode res = curl_easy_perform(curl);

    // 요청 자체가 실패한 경우
    if (res != CURLE_OK)
    {
        errMsg = curl_easy_strerror(res); // 사람이 읽을 수 있는 에러 문자열
        curl_easy_cleanup(curl);          // 자원 해제
        return false;
    }

    // HTTP 상태 코드 확인 (예: 200, 404 등)
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

    // curl 핸들 정리
    curl_easy_cleanup(curl);

    // 200 OK가 아니면 실패 처리
    if (responseCode != 200)
    {
        errMsg = "HTTP response code: " + std::to_string(responseCode);
        return false;
    }

    // 여기까지 왔으면 성공
    return true;
}

int main()
{
    // libcurl 전역 초기화
    // 프로그램 시작 시 한 번 호출
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // HTTP 서버 객체 생성
    httplib::Server svr;

    /*
        GET /health
        서버가 살아있는지 확인하는 테스트용 엔드포인트

        브라우저나 curl로:
        http://localhost:8080/health
        로 요청하면 JSON 응답을 돌려줌
    */
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res)
    {
        // 응답용 JSON 생성
        json response = {
            {"status", "ok"},
            {"message", "server is running"}
        };

        // JSON 문자열로 직렬화해서 응답 본문에 넣음
        // MIME 타입은 application/json
        res.set_content(response.dump(4), "application/json");
    });

    /*
        POST /api/v1/process
        핵심 API

        요청 예시(JSON):
        {
          "image_url": "https://....jpg",
          "action": "grayscale"
        }

        동작 순서:
        1. JSON 파싱
        2. image_url, action 있는지 검사
        3. URL에서 이미지 다운로드
        4. OpenCV로 디코딩
        5. action에 따라 처리
        6. 처리 결과를 jpg로 인코딩
        7. image/jpeg로 응답
    */
    svr.Post("/api/v1/process", [](const httplib::Request& req, httplib::Response& res)
    {
        try
        {
            // 클라이언트가 보낸 본문(req.body)을 JSON으로 파싱
            json requestBody = json::parse(req.body);

            // image_url 또는 action이 없으면 잘못된 요청
            if (!requestBody.contains("image_url") || !requestBody.contains("action"))
            {
                json error = {
                    {"error", "image_url and action are required"}
                };

                res.status = 400; // Bad Request
                res.set_content(error.dump(4), "application/json");
                return;
            }

            // JSON에서 실제 값 꺼내기
            std::string imageUrl = requestBody["image_url"].get<std::string>();
            std::string action = requestBody["action"].get<std::string>();

            // 다운로드된 이미지 바이트를 담을 벡터
            std::vector<unsigned char> imageData;

            // 실패 사유를 담을 문자열
            std::string errMsg;

            // URL에서 이미지 다운로드
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
                다운로드된 imageData는 그냥 "바이트 배열"일 뿐이라
                OpenCV가 다룰 수 있는 이미지 객체(cv::Mat)로 변환해야 함.

                cv::IMREAD_COLOR:
                흑백 이미지든 뭐든 일단 컬러 형태로 읽으라는 옵션
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

            // 처리 결과를 저장할 이미지 객체
            cv::Mat result;

            /*
                action 값에 따라 다른 영상 처리 수행
            */
            if (action == "grayscale")
            {
                // 컬러(BGR) -> 흑백 변환
                cv::cvtColor(img, result, cv::COLOR_BGR2GRAY);
            }
            else if (action == "blur")
            {
                // 가우시안 블러 적용
                // Size(9, 9): 커널 크기
                // 2.0: sigma 값
                cv::GaussianBlur(img, result, cv::Size(9, 9), 2.0);
            }
            else if (action == "canny_edge")
            {
                // 캐니 엣지는 보통 흑백 이미지 대상으로 수행
                cv::Mat gray;

                // 먼저 컬러 -> 흑백 변환
                cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

                // 에지 검출
                // 100, 200은 threshold 값
                cv::Canny(gray, result, 100, 200);
            }
            else
            {
                // 지원하지 않는 action일 경우
                json error = {
                    {"error", "unsupported action"},
                    {"action", action}
                };

                res.status = 400;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            /*
                처리 결과(result)는 아직 OpenCV 내부 이미지 객체(cv::Mat)임.
                이걸 HTTP 응답으로 보내려면 jpg 바이트로 다시 바꿔야 함.
            */
            std::vector<unsigned char> encoded;

            // result를 .jpg 형식으로 인코딩
            bool ok = cv::imencode(".jpg", result, encoded);

            if (!ok)
            {
                json error = {
                    {"error", "failed to encode result image"}
                };

                res.status = 500; // 서버 내부 처리 실패
                res.set_content(error.dump(4), "application/json");
                return;
            }

            /*
                최종 응답:
                encoded 벡터 안에는 jpg 파일 바이트가 들어 있음.
                그걸 그대로 HTTP 응답 본문으로 내려줌.
                MIME 타입은 image/jpeg
            */
            res.set_content(
                reinterpret_cast<const char*>(encoded.data()), // 바이트 시작 주소
                encoded.size(),                                // 바이트 길이
                "image/jpeg"                                   // 응답 타입
            );
        }
        catch (const std::exception& e)
        {
            // JSON 파싱 실패 등 예외 처리
            json error = {
                {"error", "invalid request"},
                {"detail", e.what()}
            };

            res.status = 400;
            res.set_content(error.dump(4), "application/json");
        }
    });

    // 콘솔에 서버 시작 메시지 출력
    std::cout << "Server started on 0.0.0.0:8080\n";

    /*
        서버 실행 시작
        0.0.0.0 : 외부에서 접근 가능하도록 모든 인터페이스에서 대기
        8080    : 포트 번호
    */
    svr.listen("0.0.0.0", 8080);

    // 서버 종료 후 libcurl 전역 정리
    curl_global_cleanup();

    return 0;
}