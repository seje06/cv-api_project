#include "ImageProcessor.h"

#include "JsonUtils.h"

#include <stdexcept>

cv::Mat ImageProcessor::ApplySingleOperation(const cv::Mat& input, const json& op)
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
            result = input;
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
    */
    else if (type == "blur")
    {
        // JSON에서 옵션 읽기
        int ksize = JsonUtils::GetIntOption(options, "ksize", 9);
        double sigma = JsonUtils::GetDoubleOption(options, "sigma", 2.0);

        // OpenCV에서 쓰기 좋도록 양수 홀수로 보정
        ksize = JsonUtils::MakePositiveOdd(ksize);

        // 가우시안 블러 적용
        cv::GaussianBlur(input, result, cv::Size(ksize, ksize), sigma);
    }
    /*
        --------------------------------------------------------------------
        canny_edge
        --------------------------------------------------------------------
        캐니 엣지 검출
    */
    else if (type == "canny_edge")
    {
        // threshold 옵션 읽기
        double threshold1 = JsonUtils::GetDoubleOption(options, "threshold1", 100.0);
        double threshold2 = JsonUtils::GetDoubleOption(options, "threshold2", 200.0);

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
    */
    else if (type == "resize")
    {
        // width/height는 resize에선 사실상 필수라고 보는 게 맞다.
        int width = JsonUtils::GetIntOption(options, "width", 0);
        int height = JsonUtils::GetIntOption(options, "height", 0);

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

cv::Mat ImageProcessor::ApplyOperations(const cv::Mat& original, const json& operations)
{
    // operations는 비어 있지 않은 배열이어야 한다.
    if (!operations.is_array() || operations.empty())
    {
        throw std::runtime_error("'operations' must be a non-empty array");
    }

    // 얕은 복사
    cv::Mat current = original;

    // 배열에 들어 있는 작업을 순서대로 적용
    for (const auto& op : operations)
    {
        current = ApplySingleOperation(current, op);
    }

    // 최종 결과 반환
    return current;
}

bool ImageProcessor::ResolveOutputFormat(const std::string& outputFormat, std::string& extension, std::string& mimeType)
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
