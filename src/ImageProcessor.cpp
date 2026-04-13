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
        컬러 이미지를 흑백으로 변환

        특징:
        - 입력이 이미 1채널이면 그대로 사용
        - 컬러(BGR) 입력이면 GRAY로 변환
    */
    if (type == "grayscale")
    {
        // 이미 1채널 흑백 이미지면 그대로 참조
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

        options:
        - ksize : 커널 크기 (기본값 9)
        - sigma : 표준편차 값 (기본값 2.0)

        참고:
        - OpenCV의 GaussianBlur는 보통 양수 홀수 커널 크기를 사용
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

        options:
        - threshold1 : 첫 번째 임계값 (기본값 100.0)
        - threshold2 : 두 번째 임계값 (기본값 200.0)

        참고:
        - 캐니는 보통 흑백 이미지 기준으로 수행
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

        options:
        - width         : 목표 너비
        - height        : 목표 높이
        - interpolation : 보간법 (기본값 INTER_LINEAR)

        interpolation 값 예시:
        - 0 : INTER_NEAREST
        - 1 : INTER_LINEAR
        - 2 : INTER_CUBIC
        - 3 : INTER_AREA

        참고:
        - width / height 는 0보다 커야 함
    */
    else if (type == "resize")
    {
        // width/height는 resize에선 사실상 필수라고 보는 게 맞다.
        int width = JsonUtils::GetIntOption(options, "width", 0);
        int height = JsonUtils::GetIntOption(options, "height", 0);

        // 기본 보간법은 선형 보간
        int interpolation = JsonUtils::GetIntOption(options, "interpolation", cv::INTER_LINEAR);

        // 0 이하이면 잘못된 값
        if (width <= 0 || height <= 0)
        {
            throw std::runtime_error("resize requires positive integer options 'width' and 'height'");
        }

        // 이미지 크기 변경
        cv::resize(input, result, cv::Size(width, height), 0.0, 0.0, interpolation);
    }
    /*
        --------------------------------------------------------------------
        crop
        --------------------------------------------------------------------
        이미지 일부 영역 자르기

        options:
        - x      : 시작 x 좌표
        - y      : 시작 y 좌표
        - width  : 자를 영역 너비
        - height : 자를 영역 높이

        참고:
        - ROI는 원본 버퍼를 참조할 수 있으므로 clone()으로 독립 복사
        - 영역이 이미지 범위를 벗어나면 예외 처리
    */
    else if (type == "crop")
    {
        int x = JsonUtils::GetIntOption(options, "x", 0);
        int y = JsonUtils::GetIntOption(options, "y", 0);
        int width = JsonUtils::GetIntOption(options, "width", 0);
        int height = JsonUtils::GetIntOption(options, "height", 0);

        if (width <= 0 || height <= 0)
        {
            throw std::runtime_error("crop requires positive integer options 'width' and 'height'");
        }

        if (x < 0 || y < 0 || x + width > input.cols || y + height > input.rows)
        {
            throw std::runtime_error("crop region is out of image bounds");
        }

        // ROI는 얕은 참조가 될 수 있으므로 clone으로 독립 이미지 생성
        result = input(cv::Rect(x, y, width, height)).clone();
    }
    /*
        --------------------------------------------------------------------
        flip
        --------------------------------------------------------------------
        이미지 반전

        options:
        - mode : 반전 방향
            1  -> 좌우 반전
            0  -> 상하 반전
           -1  -> 좌우 + 상하 모두 반전
    */
    else if (type == "flip")
    {
        int mode = JsonUtils::GetIntOption(options, "mode", 1);

        if (mode != -1 && mode != 0 && mode != 1)
        {
            throw std::runtime_error("flip option 'mode' must be one of -1, 0, 1");
        }

        cv::flip(input, result, mode);
    }
    /*
        --------------------------------------------------------------------
        rotate
        --------------------------------------------------------------------
        이미지 회전

        options:
        - angle : 회전 각도
            90  -> 시계 방향 90도
            180 -> 180도
            270 -> 반시계 방향 90도

        참고:
        - 처음부터 임의 각도까지 지원할 수도 있지만,
          현재는 단순하고 안전한 90/180/270만 지원
    */
    else if (type == "rotate")
    {
        int angle = JsonUtils::GetIntOption(options, "angle", 0);

        if (angle == 90)
        {
            cv::rotate(input, result, cv::ROTATE_90_CLOCKWISE);
        }
        else if (angle == 180)
        {
            cv::rotate(input, result, cv::ROTATE_180);
        }
        else if (angle == 270)
        {
            cv::rotate(input, result, cv::ROTATE_90_COUNTERCLOCKWISE);
        }
        else
        {
            throw std::runtime_error("rotate option 'angle' must be one of 90, 180, 270");
        }
    }
    /*
        --------------------------------------------------------------------
        threshold
        --------------------------------------------------------------------
        이진화 처리

        options:
        - thresh      : 기준 임계값 (기본값 127.0)
        - max_value   : 최대값 (기본값 255.0)
        - threshold_type : threshold 타입 (기본값 THRESH_BINARY)

        threshold_type 값 예시:
        - THRESH_BINARY
        - THRESH_BINARY_INV
        - THRESH_TRUNC
        - THRESH_TOZERO
        - THRESH_TOZERO_INV

        참고:
        - threshold는 보통 흑백 기준으로 처리
    */
    else if (type == "threshold")
    {
        double thresh = JsonUtils::GetDoubleOption(options, "thresh", 127.0);
        double maxValue = JsonUtils::GetDoubleOption(options, "max_value", 255.0);
        int thresholdType = JsonUtils::GetIntOption(options, "threshold_type", cv::THRESH_BINARY);

        cv::Mat gray;

        if (input.channels() == 1)
        {
            gray = input;
        }
        else
        {
            cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
        }

        cv::threshold(gray, result, thresh, maxValue, thresholdType);
    }
    /*
        --------------------------------------------------------------------
        brightness_contrast
        --------------------------------------------------------------------
        밝기 / 대비 조정

        options:
        - alpha : 대비 계수 (기본값 1.0)
        - beta  : 밝기 보정값 (기본값 0.0)

        참고:
        - result = input * alpha + beta 형태
        - convertTo 내부에서 saturate_cast 적용
    */
    else if (type == "brightness_contrast")
    {
        double alpha = JsonUtils::GetDoubleOption(options, "alpha", 1.0);
        double beta = JsonUtils::GetDoubleOption(options, "beta", 0.0);

        input.convertTo(result, -1, alpha, beta);
    }
    /*
        --------------------------------------------------------------------
        sharpen
        --------------------------------------------------------------------
        샤프닝 필터 적용

        특징:
        - 블러와 반대로 경계를 강조하는 효과
        - 현재는 고정 커널 사용
        - 강도 옵션을 나중에 추가 가능
    */
    else if (type == "sharpen")
    {
        cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
             0, -1,  0,
            -1,  5, -1,
             0, -1,  0);

        cv::filter2D(input, result, input.depth(), kernel);
    }
    /*
        --------------------------------------------------------------------
        median_blur
        --------------------------------------------------------------------
        중간값 블러 적용

        options:
        - ksize : 커널 크기 (기본값 5)

        참고:
        - salt-and-pepper noise 제거에 자주 사용
        - 커널 크기는 양수 홀수여야 함
    */
    else if (type == "median_blur")
    {
        int ksize = JsonUtils::GetIntOption(options, "ksize", 5);
        ksize = JsonUtils::MakePositiveOdd(ksize);

        cv::medianBlur(input, result, ksize);
    }
    /*
        --------------------------------------------------------------------
        bilateral_filter
        --------------------------------------------------------------------
        양방향 필터 적용

        options:
        - d           : 필터링에 사용할 이웃 크기 (기본값 9)
        - sigma_color : 색상 공간 sigma (기본값 75.0)
        - sigma_space : 좌표 공간 sigma (기본값 75.0)

        특징:
        - 엣지는 비교적 보존하면서 노이즈를 줄이는 데 사용
    */
    else if (type == "bilateral_filter")
    {
        int d = JsonUtils::GetIntOption(options, "d", 9);
        double sigmaColor = JsonUtils::GetDoubleOption(options, "sigma_color", 75.0);
        double sigmaSpace = JsonUtils::GetDoubleOption(options, "sigma_space", 75.0);

        cv::bilateralFilter(input, result, d, sigmaColor, sigmaSpace);
    }
    /*
        --------------------------------------------------------------------
        invert
        --------------------------------------------------------------------
        이미지 색상 반전

        특징:
        - 각 픽셀 값을 반전
        - 컬러 / 흑백 모두 가능
    */
    else if (type == "invert")
    {
        cv::bitwise_not(input, result);
    }
    /*
        --------------------------------------------------------------------
        erode
        --------------------------------------------------------------------
        침식 연산

        options:
        - ksize      : 커널 크기 (기본값 3)
        - iterations : 반복 횟수 (기본값 1)

        특징:
        - 밝은 영역을 줄이는 방향
        - 작은 노이즈 제거에 활용 가능
    */
    else if (type == "erode")
    {
        int ksize = JsonUtils::GetIntOption(options, "ksize", 3);
        int iterations = JsonUtils::GetIntOption(options, "iterations", 1);

        ksize = JsonUtils::MakePositiveOdd(ksize);

        if (iterations < 1)
        {
            throw std::runtime_error("erode option 'iterations' must be greater than 0");
        }

        cv::Mat kernel = cv::getStructuringElement(
            cv::MORPH_RECT,
            cv::Size(ksize, ksize)
        );

        cv::erode(input, result, kernel, cv::Point(-1, -1), iterations);
    }
    /*
        --------------------------------------------------------------------
        dilate
        --------------------------------------------------------------------
        팽창 연산

        options:
        - ksize      : 커널 크기 (기본값 3)
        - iterations : 반복 횟수 (기본값 1)

        특징:
        - 밝은 영역을 늘리는 방향
        - 형태학 연산의 기본 중 하나
    */
    else if (type == "dilate")
    {
        int ksize = JsonUtils::GetIntOption(options, "ksize", 3);
        int iterations = JsonUtils::GetIntOption(options, "iterations", 1);

        ksize = JsonUtils::MakePositiveOdd(ksize);

        if (iterations < 1)
        {
            throw std::runtime_error("dilate option 'iterations' must be greater than 0");
        }

        cv::Mat kernel = cv::getStructuringElement(
            cv::MORPH_RECT,
            cv::Size(ksize, ksize)
        );

        cv::dilate(input, result, kernel, cv::Point(-1, -1), iterations);
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

    // cv::Mat은 기본적으로 얕은 복사이므로,
    // 여기서는 원본 버퍼를 공유하는 current로 시작한다.
    // 이후 대부분의 operation은 result를 새로 만들어 반환하므로
    // current가 다음 단계에서 새 Mat를 가리키는 방식으로 흐른다.
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
    /*
        --------------------------------------------------------------------
        jpg / jpeg
        --------------------------------------------------------------------
    */
    if (outputFormat == "jpg" || outputFormat == "jpeg")
    {
        extension = ".jpg";
        mimeType = "image/jpeg";
        return true;
    }
    /*
        --------------------------------------------------------------------
        png
        --------------------------------------------------------------------
    */
    else if (outputFormat == "png")
    {
        extension = ".png";
        mimeType = "image/png";
        return true;
    }

    // 지원하지 않는 포맷
    return false;
}