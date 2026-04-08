#include "HttpServer.h"

#include "ImageDownloader.h"
#include "ImageProcessor.h"

#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

HttpServer::HttpServer()
{
    SetupRoutes();
}

void HttpServer::SetupRoutes()
{
    /*
        --------------------------------------------------------------------
        GET /health
        --------------------------------------------------------------------
        서버가 살아있는지 확인하는 테스트용 엔드포인트
    */
    _server.Get("/health", [](const httplib::Request&, httplib::Response& res)
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
    */
    _server.Post("/api/v1/process", [](const httplib::Request& req, httplib::Response& res)
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
            if (!ImageDownloader::DownloadImage(imageUrl, imageData, errMsg))
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
            cv::Mat result = ImageProcessor::ApplyOperations(img, operations);

            // output_format -> 인코딩 확장자, MIME 타입 변환
            std::string extension;
            std::string mimeType;

            if (!ImageProcessor::ResolveOutputFormat(outputFormat, extension, mimeType))
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
            */
            res.set_header("X-Image-Width", std::to_string(result.cols));
            res.set_header("X-Image-Height", std::to_string(result.rows));

            /*
                최종 응답:
                encoded 벡터 안에는 실제 jpg/png 파일 바이트가 들어 있다.
                그걸 그대로 HTTP body에 실어 보낸다.
            */
            res.set_content(
                reinterpret_cast<const char*>(encoded.data()),
                encoded.size(),
                mimeType
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
}

bool HttpServer::Start(const char* host, int port)
{
    return _server.listen(host, port);
}
