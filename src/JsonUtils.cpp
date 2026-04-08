#include "JsonUtils.h"

#include <stdexcept>

int JsonUtils::GetIntOption(const json& options, const std::string& key, int defaultValue)
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

double JsonUtils::GetDoubleOption(const json& options, const std::string& key, double defaultValue)
{
    // 키가 없으면 기본값 반환
    if (!options.contains(key))
        return defaultValue;

    // 숫자 타입인지 검사
    if (!options[key].is_number())
        throw std::runtime_error("Option '" + key + "' must be a number");

    // double로 변환
    return options[key].get<double>();
}

int JsonUtils::MakePositiveOdd(int value)
{
    // 1보다 작으면 최소값 1로 보정
    if (value < 1)
        value = 1;

    // 짝수면 1 더해서 홀수로 보정
    if (value % 2 == 0)
        value += 1;

    return value;
}
