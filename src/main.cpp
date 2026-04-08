#include <iostream>     // std::cout 같은 콘솔 출력
#include <curl/curl.h>  // libcurl 전역 초기화/정리

#include "HttpServer.h"

/*
    ------------------------------------------------------------------------
    main
    ------------------------------------------------------------------------
    프로그램 시작점

    여기서는 너무 많은 로직을 넣지 않고,
    전역 라이브러리 초기화와 서버 실행만 담당한다.
*/
int main()
{
    // libcurl 전역 초기화
    // 프로그램 시작 시 한 번 호출
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // HTTP 서버 객체 생성
    HttpServer server;

    // 서버 시작 메시지 출력
    std::cout << "Server started on 0.0.0.0:8080\n";

    /*
        서버 실행
        - 0.0.0.0 : 모든 네트워크 인터페이스에서 요청 받음
        - 8080    : 포트 번호
    */
    server.Start("0.0.0.0", 8080);

    // 서버 종료 후 libcurl 전역 정리
    curl_global_cleanup();

    return 0;
}
