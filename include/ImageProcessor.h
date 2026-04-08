#pragma once

#include <string>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/*
    ------------------------------------------------------------------------
    ImageProcessor
    ------------------------------------------------------------------------
    다운로드 및 디코딩이 끝난 OpenCV 이미지에 대해
    여러 operation을 순서대로 적용하는 역할을 담당하는 클래스
*/
class ImageProcessor
{
public:
    static cv::Mat ApplySingleOperation(const cv::Mat& input, const json& op);
    static cv::Mat ApplyOperations(const cv::Mat& original, const json& operations);
    static bool ResolveOutputFormat(const std::string& outputFormat, std::string& extension, std::string& mimeType);
};
