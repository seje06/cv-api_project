#pragma once

#include <httplib.h>

/*
    ------------------------------------------------------------------------
    HttpServer
    ------------------------------------------------------------------------
    HTTP 라우트 등록과 서버 실행을 담당하는 클래스

    현재는 cpp-httplib의 Server 객체를 내부에 보관하고,
    /health, /api/v1/process 라우트를 등록해 실행한다.
*/
class HttpServer
{
public:
    HttpServer();

    void SetupRoutes();
    bool Start(const char* host, int port);

private:
    httplib::Server _server;
};
