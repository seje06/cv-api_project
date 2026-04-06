#include <iostream>
#include <string>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <curl/curl.h>

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t totalSize = size * nmemb;
    auto* buffer = static_cast<std::vector<unsigned char>*>(userp);
    unsigned char* data = static_cast<unsigned char*>(contents);
    buffer->insert(buffer->end(), data, data + totalSize);
    return totalSize;
}

bool DownloadImage(const std::string& url, std::vector<unsigned char>& outData, std::string& errMsg)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        errMsg = "Failed to initialize CURL";
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outData);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        errMsg = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        return false;
    }

    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    curl_easy_cleanup(curl);

    if (responseCode != 200)
    {
        errMsg = "HTTP response code: " + std::to_string(responseCode);
        return false;
    }

    return true;
}

int main()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);

    httplib::Server svr;

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res)
    {
        json response = {
            {"status", "ok"},
            {"message", "server is running"}
        };

        res.set_content(response.dump(4), "application/json");
    });

    svr.Post("/api/v1/process", [](const httplib::Request& req, httplib::Response& res)
    {
        try
        {
            json requestBody = json::parse(req.body);

            if (!requestBody.contains("image_url") || !requestBody.contains("action"))
            {
                json error = {
                    {"error", "image_url and action are required"}
                };
                res.status = 400;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            std::string imageUrl = requestBody["image_url"].get<std::string>();
            std::string action = requestBody["action"].get<std::string>();

            std::vector<unsigned char> imageData;
            std::string errMsg;

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

            cv::Mat img = cv::imdecode(imageData, cv::IMREAD_COLOR);
            if (img.empty())
            {
                json error = {
                    {"error", "failed to decode image"}
                };
                res.status = 400;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            cv::Mat result;

            if (action == "grayscale")
            {
                cv::cvtColor(img, result, cv::COLOR_BGR2GRAY);
            }
            else if (action == "blur")
            {
                cv::GaussianBlur(img, result, cv::Size(9, 9), 2.0);
            }
            else if (action == "canny_edge")
            {
                cv::Mat gray;
                cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
                cv::Canny(gray, result, 100, 200);
            }
            else
            {
                json error = {
                    {"error", "unsupported action"},
                    {"action", action}
                };
                res.status = 400;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            std::vector<unsigned char> encoded;
            bool ok = cv::imencode(".jpg", result, encoded);
            if (!ok)
            {
                json error = {
                    {"error", "failed to encode result image"}
                };
                res.status = 500;
                res.set_content(error.dump(4), "application/json");
                return;
            }

            res.set_content(
                reinterpret_cast<const char*>(encoded.data()),
                encoded.size(),
                "image/jpeg"
            );
        }
        catch (const std::exception& e)
        {
            json error = {
                {"error", "invalid request"},
                {"detail", e.what()}
            };
            res.status = 400;
            res.set_content(error.dump(4), "application/json");
        }
    });

    std::cout << "Server started on 0.0.0.0:8080\n";
    svr.listen("0.0.0.0", 8080);

    curl_global_cleanup();
    return 0;
}