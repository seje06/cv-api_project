#include "ImageDownloader.h"

#include <curl/curl.h>

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
    buffer->insert(buffer->end(), data, data + totalSize);

    // libcurl에게 "이만큼 정상적으로 처리했다"고 알려준다.
    return totalSize;
}

bool ImageDownloader::DownloadImage(const std::string& url, std::vector<unsigned char>& outData, std::string& errMsg)
{
    // 이전 호출 결과가 남지 않도록 비운다.
    outData.clear();

    // libcurl 핸들 생성
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
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outData);

    // 리다이렉트 허용
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // 너무 오래 걸리는 요청 방지용 타임아웃 (10초)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    // 실제 HTTP 요청 수행
    CURLcode res = curl_easy_perform(curl);

    // 네트워크 수준에서 실패한 경우
    if (res != CURLE_OK)
    {
        errMsg = curl_easy_strerror(res);
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
