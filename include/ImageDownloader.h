#pragma once

#include <string>
#include <vector>

/*
    ------------------------------------------------------------------------
    ImageDownloader
    ------------------------------------------------------------------------
    이미지 URL에서 실제 바이너리 데이터를 다운로드하는 역할을 담당하는 클래스

    현재는 정적 함수만 제공하므로 객체 생성 없이도 사용할 수 있다.
*/
class ImageDownloader
{
public:
    /*
        --------------------------------------------------------------------
        DownloadImage
        --------------------------------------------------------------------
        이미지 URL을 받아서 실제 이미지 바이너리 데이터를 다운로드한다.

        매개변수 설명:
        - url      : 다운로드할 이미지 주소
        - outData  : 다운로드된 이미지 바이트를 저장할 벡터
        - errMsg   : 실패 시 원인을 담을 문자열

        반환값:
        - 성공 : true
        - 실패 : false
    */
    static bool DownloadImage(const std::string& url, std::vector<unsigned char>& outData, std::string& errMsg);
};
