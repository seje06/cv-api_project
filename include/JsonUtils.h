#pragma once

#include <string>
#include <nlohmann/json.hpp>

// nlohmann::json을 매번 길게 쓰기 귀찮으므로 json 별칭으로 사용
using json = nlohmann::json;

/*
    ------------------------------------------------------------------------
    JsonUtils
    ------------------------------------------------------------------------
    options JSON 객체 안에서 숫자 옵션을 읽고 검증하는 보조 함수들을 모아둔 클래스
*/
class JsonUtils
{
public:
    static int GetIntOption(const json& options, const std::string& key, int defaultValue);
    static double GetDoubleOption(const json& options, const std::string& key, double defaultValue);
    static int MakePositiveOdd(int value);
};
