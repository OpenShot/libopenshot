/**
 * @file
 * @brief Source file for CVObjectMask class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "CVObjectMask.h"

#include "Exceptions.h"
#include "ZmqLogger.h"
#include "objdetectdata.pb.h"

#define int64 int64_t
#define uint64 uint64_t
#include <opencv2/core/ocl.hpp>
#if CV_VERSION_MAJOR >= 5
#include <opencv2/geometry/2d.hpp>
#else
#include <opencv2/imgproc.hpp>
#endif
#undef uint64
#undef int64

#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <fstream>
#include <iostream>
#include <cstring>
#include <limits>
#include <numeric>

#include <google/protobuf/util/time_util.h>

using namespace openshot;
using google::protobuf::util::TimeUtil;

namespace {

std::string LoadONNXModel(const std::string& modelPath, cv::dnn::Net* net)
{
    try {
        cv::dnn::Net loadedNet = cv::dnn::readNetFromONNX(modelPath);
        if (net)
            *net = loadedNet;
        return "";
    } catch (const cv::Exception& e) {
        return std::string("Failed to load ONNX model: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("Failed to load ONNX model: ") + e.what();
    }
}

std::vector<uint32_t> EncodeBinaryMaskRLE(const cv::Mat& mask)
{
    std::vector<uint32_t> rle;
    if (mask.empty())
        return rle;

    uint8_t current = 0;
    uint32_t count = 0;
    for (int y = 0; y < mask.rows; ++y) {
        const uint8_t* row = mask.ptr<uint8_t>(y);
        for (int x = 0; x < mask.cols; ++x) {
            const uint8_t value = row[x] ? 1 : 0;
            if (value == current) {
                ++count;
            } else {
                rle.push_back(count);
                current = value;
                count = 1;
            }
        }
    }
    rle.push_back(count);
    return rle;
}

struct EfficientSamPreprocessResult {
    cv::Mat blob;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

EfficientSamPreprocessResult MakeEfficientSamBlob(const cv::Mat& bgr, int modelSize)
{
    EfficientSamPreprocessResult result;
    result.scaleX = static_cast<float>(modelSize) / static_cast<float>(bgr.cols);
    result.scaleY = static_cast<float>(modelSize) / static_cast<float>(bgr.rows);

    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(modelSize, modelSize), 0, 0, cv::INTER_LINEAR);

    const int shape[] = {1, 3, modelSize, modelSize};
    result.blob = cv::Mat(4, shape, CV_32F);
    float* dst = result.blob.ptr<float>();

    for (int y = 0; y < resized.rows; ++y) {
        const cv::Vec3b* row = resized.ptr<cv::Vec3b>(y);
        for (int x = 0; x < resized.cols; ++x) {
            const float rgb[] = {
                static_cast<float>(row[x][2]) / 255.0f,
                static_cast<float>(row[x][1]) / 255.0f,
                static_cast<float>(row[x][0]) / 255.0f,
            };
            for (int c = 0; c < 3; ++c)
                dst[(c * modelSize + y) * modelSize + x] = rgb[c];
        }
    }

    return result;
}

cv::Rect_<float> NormalizedBoundingBox(const cv::Mat& mask)
{
    std::vector<cv::Point> points;
    cv::findNonZero(mask, points);
    if (points.empty())
        return {};

    cv::Rect rect = cv::boundingRect(points);
    return cv::Rect_<float>(
        rect.x / static_cast<float>(mask.cols),
        rect.y / static_cast<float>(mask.rows),
        rect.width / static_cast<float>(mask.cols),
        rect.height / static_cast<float>(mask.rows));
}

cv::Mat EfficientSamMaskToFrameMask(const cv::Mat& modelMask, const cv::Size& frameSize, float maskThreshold)
{
    cv::Mat fullSize;
    cv::resize(modelMask, fullSize, frameSize, 0, 0, cv::INTER_LINEAR);

    cv::Mat binary;
    cv::threshold(fullSize, binary, maskThreshold, 255.0, cv::THRESH_BINARY);
    if (cv::countNonZero(binary) == 0) {
        double maxValue = 0.0;
        cv::minMaxLoc(fullSize, nullptr, &maxValue);
        if (maxValue > 0.0) {
            cv::threshold(fullSize, binary, maxValue * 0.5, 255.0, cv::THRESH_BINARY);
        }
    }
    binary.convertTo(binary, CV_8U);
    return binary;
}

cv::Mat MakeEfficientSamPromptBlob(
    const CVObjectMaskPromptSet& prompts,
    const EfficientSamPreprocessResult& prep,
    int promptSlots,
    std::vector<cv::Point>& backgroundPoints,
    std::vector<cv::Rect>& backgroundRects)
{
    const int coordsShape[] = {1, 1, promptSlots, 2};
    cv::Mat pointCoords(4, coordsShape, CV_32F, cv::Scalar(0.0f));

    float* coords = pointCoords.ptr<float>();
    int promptIndex = 0;
    for (const auto& rect : prompts.positiveRects) {
        if (promptIndex + 1 >= promptSlots)
            break;
        coords[promptIndex * 2] = rect.x * prep.scaleX;
        coords[promptIndex * 2 + 1] = rect.y * prep.scaleY;
        ++promptIndex;
        coords[promptIndex * 2] = (rect.x + rect.width) * prep.scaleX;
        coords[promptIndex * 2 + 1] = (rect.y + rect.height) * prep.scaleY;
        ++promptIndex;
    }
    for (const auto& point : prompts.positivePoints) {
        if (promptIndex >= promptSlots)
            break;
        coords[promptIndex * 2] = point.x * prep.scaleX;
        coords[promptIndex * 2 + 1] = point.y * prep.scaleY;
        ++promptIndex;
    }
    for (const auto& point : prompts.negativePoints) {
        backgroundPoints.emplace_back(
            static_cast<int>(std::lround(point.x * prep.scaleX)),
            static_cast<int>(std::lround(point.y * prep.scaleY)));
    }
    for (const auto& rect : prompts.negativeRects) {
        const int x1 = static_cast<int>(std::floor(rect.x * prep.scaleX));
        const int y1 = static_cast<int>(std::floor(rect.y * prep.scaleY));
        const int x2 = static_cast<int>(std::ceil((rect.x + rect.width) * prep.scaleX));
        const int y2 = static_cast<int>(std::ceil((rect.y + rect.height) * prep.scaleY));
        const int modelWidth = prep.blob.size[3];
        const int modelHeight = prep.blob.size[2];
        const int left = std::max(0, std::min(modelWidth - 1, x1));
        const int top = std::max(0, std::min(modelHeight - 1, y1));
        const int right = std::max(left + 1, std::min(modelWidth, x2));
        const int bottom = std::max(top + 1, std::min(modelHeight, y2));
        backgroundRects.emplace_back(left, top, right - left, bottom - top);
    }

    return pointCoords;
}

cv::Mat MakeEfficientSamLabelBlob(const CVObjectMaskPromptSet& prompts, int promptSlots)
{
    const int labelsShape[] = {1, 1, promptSlots, 1};
    cv::Mat pointLabels(4, labelsShape, CV_32F, cv::Scalar(-1.0f));

    float* labels = pointLabels.ptr<float>();
    int promptIndex = 0;
    for (size_t i = 0; i < prompts.positiveRects.size() && promptIndex + 1 < promptSlots; ++i) {
        labels[promptIndex++] = 2.0f;
        labels[promptIndex++] = 3.0f;
    }
    for (size_t i = 0; i < prompts.positivePoints.size() && promptIndex < promptSlots; ++i, ++promptIndex)
        labels[promptIndex] = 1.0f;

    return pointLabels;
}

cv::Mat SelectEfficientSamMask(const cv::Mat& outputMasks, const cv::Mat& iouPredictions,
                               const std::vector<cv::Point>& backgroundPoints,
                               const std::vector<cv::Rect>& backgroundRects,
                               float maskThreshold)
{
    if (outputMasks.dims != 5 || iouPredictions.empty())
        return cv::Mat();

    const int candidateCount = outputMasks.size[2];
    const int maskHeight = outputMasks.size[3];
    const int maskWidth = outputMasks.size[4];
    const float* ious = iouPredictions.ptr<float>();

    const float* masks = outputMasks.ptr<float>();
    const size_t candidatePixels = static_cast<size_t>(maskHeight) * static_cast<size_t>(maskWidth);
    cv::Mat bestMask;
    float bestScore = -std::numeric_limits<float>::infinity();
    for (int candidate = 0; candidate < candidateCount; ++candidate) {
        cv::Mat mask(maskHeight, maskWidth, CV_32F,
                     const_cast<float*>(masks + static_cast<size_t>(candidate) * candidatePixels));

        int backgroundHits = 0;
        for (const cv::Point& point : backgroundPoints) {
            const int x = std::max(0, std::min(maskWidth - 1, point.x));
            const int y = std::max(0, std::min(maskHeight - 1, point.y));
            if (mask.at<float>(y, x) >= maskThreshold)
                ++backgroundHits;
        }

        float rectOverlapPenalty = 0.0f;
        for (const cv::Rect& rect : backgroundRects) {
            const cv::Rect clipped = rect & cv::Rect(0, 0, maskWidth, maskHeight);
            const int area = clipped.area();
            if (area <= 0)
                continue;
            int overlap = 0;
            for (int y = clipped.y; y < clipped.y + clipped.height; ++y) {
                const float* row = mask.ptr<float>(y);
                for (int x = clipped.x; x < clipped.x + clipped.width; ++x) {
                    if (row[x] >= maskThreshold)
                        ++overlap;
                }
            }
            rectOverlapPenalty += static_cast<float>(overlap) / static_cast<float>(area);
        }

        const float pointPenalty = backgroundPoints.empty()
            ? 0.0f
            : static_cast<float>(backgroundHits) / static_cast<float>(backgroundPoints.size());
        if (!backgroundRects.empty())
            rectOverlapPenalty /= static_cast<float>(backgroundRects.size());

        const float score = ious[candidate] - (0.35f * pointPenalty) - (0.75f * rectOverlapPenalty);
        if (bestMask.empty() || score > bestScore) {
            bestScore = score;
            bestMask = mask.clone();
        }
    }
    return bestMask;
}

CVObjectMaskFrameData FrameDataFromMask(const cv::Mat& mask, size_t frameId, float score)
{
    CVObjectMaskFrameData frameData;
    frameData.frameId = frameId;
    frameData.objectId = 1;
    if (mask.empty())
        return frameData;

    frameData.score = score;
    frameData.width = mask.cols;
    frameData.height = mask.rows;
    frameData.rle = EncodeBinaryMaskRLE(mask);
    frameData.box = NormalizedBoundingBox(mask);
    return frameData;
}

cv::Point2f JsonPoint(const Json::Value& value)
{
    if (!value.isObject() || value["x"].isNull() || value["y"].isNull())
        return cv::Point2f(-1.0f, -1.0f);
    return cv::Point2f(value["x"].asFloat(), value["y"].asFloat());
}

bool IsValidPoint(const cv::Point2f& point)
{
    return point.x >= 0.0f && point.y >= 0.0f;
}

void AppendJsonPoints(const Json::Value& values, std::vector<cv::Point2f>& points)
{
    if (!values.isArray())
        return;
    for (const auto& value : values) {
        cv::Point2f point = JsonPoint(value);
        if (IsValidPoint(point))
            points.push_back(point);
    }
}

size_t JsonFrameNumber(const std::string& frameName)
{
    try {
        return static_cast<size_t>(std::max(0, std::stoi(frameName)));
    } catch (...) {
        return 0;
    }
}

bool RectFromJson(const Json::Value& rect, cv::Rect_<float>& output)
{
    if (!rect.isObject() || rect["x1"].isNull() || rect["y1"].isNull() ||
        rect["x2"].isNull() || rect["y2"].isNull()) {
        return false;
    }

    const float x1 = std::min(rect["x1"].asFloat(), rect["x2"].asFloat());
    const float y1 = std::min(rect["y1"].asFloat(), rect["y2"].asFloat());
    const float x2 = std::max(rect["x1"].asFloat(), rect["x2"].asFloat());
    const float y2 = std::max(rect["y1"].asFloat(), rect["y2"].asFloat());
    cv::Point2f topLeft(x1, y1);
    cv::Point2f bottomRight(x2, y2);
    if (!IsValidPoint(topLeft) || !IsValidPoint(bottomRight) || x2 <= x1 || y2 <= y1)
        return false;

    output = cv::Rect_<float>(x1, y1, x2 - x1, y2 - y1);
    return true;
}

void AppendJsonRects(const Json::Value& values, std::vector<cv::Rect_<float>>& rects)
{
    if (!values.isArray())
        return;
    for (const auto& rect : values) {
        cv::Rect_<float> parsed;
        if (RectFromJson(rect, parsed))
            rects.push_back(parsed);
    }
}

CVObjectMaskPromptSet PromptSetFromJson(const Json::Value& framePayload)
{
    CVObjectMaskPromptSet prompts;
    AppendJsonPoints(framePayload["positive_points"], prompts.positivePoints);
    AppendJsonPoints(framePayload["negative_points"], prompts.negativePoints);
    AppendJsonRects(framePayload["positive_rects"], prompts.positiveRects);
    AppendJsonRects(framePayload["negative_rects"], prompts.negativeRects);
    return prompts;
}

cv::Mat MakeBlob(const std::vector<int>& shape, float value = 0.0f)
{
    cv::Mat output(static_cast<int>(shape.size()), shape.data(), CV_32F);
    output.setTo(value);
    return output;
}

std::string SetNetDevice(cv::dnn::Net& net, const std::string& processingDevice)
{
    if (processingDevice == "CPU") {
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        return "CPU";
    }

    if (processingDevice == "GPU" || processingDevice == "GPU_AUTO" || processingDevice == "GPU_CUDA") {
        try {
            const std::vector<cv::dnn::Target> targets = cv::dnn::getAvailableTargets(cv::dnn::DNN_BACKEND_CUDA);
            if (std::find(targets.begin(), targets.end(), cv::dnn::DNN_TARGET_CUDA) != targets.end()) {
                net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
                net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
                return "CUDA";
            }
        } catch (const cv::Exception&) {
        }
    }

    if (processingDevice == "GPU_OPENCL") {
        try {
            const std::vector<cv::dnn::Target> targets = cv::dnn::getAvailableTargets(cv::dnn::DNN_BACKEND_OPENCV);
            if (std::find(targets.begin(), targets.end(), cv::dnn::DNN_TARGET_OPENCL) != targets.end()) {
                cv::ocl::setUseOpenCL(true);
                net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
                net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);
                return "OpenCL";
            }
        } catch (const cv::Exception&) {
        }
    }

    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    return "CPU";
}

class CutiePropagator {
private:
    static constexpr int memorySlots = 6;
    int modelWidth = 640;
    int modelHeight = 368;
    int stride16Width = modelWidth / 16;
    int stride16Height = modelHeight / 16;

    struct MemoryFrame {
        cv::Mat key;
        cv::Mat shrinkage;
        cv::Mat value;
        cv::Mat valid;
    };

    struct LetterboxTransform {
        cv::Size originalSize;
        cv::Rect contentRect;
    };

    cv::dnn::Net encodeKey;
    cv::dnn::Net encodeValue;
    cv::dnn::Net memoryReadout;
    cv::dnn::Net decode;
    cv::Mat sensory;
    cv::Mat lastMask;
    cv::Mat objectMemory;
    MemoryFrame permanentMemory;
    bool hasPermanentMemory = false;
    std::deque<MemoryFrame> workingMemoryFrames;
    int frameIndex = 0;
    int lastMemoryFrame = -1000000;
    int memEvery = 5;
    int maxMemoryFrames = memorySlots;

    static bool ParseModelSize(const std::string& modelPath, int& width, int& height)
    {
        size_t xPos = modelPath.find('x');
        while (xPos != std::string::npos) {
            size_t widthStart = xPos;
            while (widthStart > 0 && std::isdigit(static_cast<unsigned char>(modelPath[widthStart - 1])))
                --widthStart;

            size_t heightEnd = xPos + 1;
            while (heightEnd < modelPath.size() && std::isdigit(static_cast<unsigned char>(modelPath[heightEnd])))
                ++heightEnd;

            if (widthStart != xPos && heightEnd != xPos + 1) {
                width = std::stoi(modelPath.substr(widthStart, xPos - widthStart));
                height = std::stoi(modelPath.substr(xPos + 1, heightEnd - xPos - 1));
                if (width > 0 && height > 0 && width % 16 == 0 && height % 16 == 0)
                    return true;
            }
            xPos = modelPath.find('x', xPos + 1);
        }
        return false;
    }

    void ConfigureModelSize(const std::string& modelPath)
    {
        int width = modelWidth;
        int height = modelHeight;
        if (!ParseModelSize(modelPath, width, height))
            return;
        modelWidth = width;
        modelHeight = height;
        stride16Width = modelWidth / 16;
        stride16Height = modelHeight / 16;
    }

    LetterboxTransform ComputeLetterbox(const cv::Size& sourceSize) const
    {
        LetterboxTransform transform;
        transform.originalSize = sourceSize;
        if (sourceSize.width <= 0 || sourceSize.height <= 0) {
            transform.contentRect = cv::Rect(0, 0, modelWidth, modelHeight);
            return transform;
        }

        const float scaleX = static_cast<float>(modelWidth) / static_cast<float>(sourceSize.width);
        const float scaleY = static_cast<float>(modelHeight) / static_cast<float>(sourceSize.height);
        const float scale = std::min(scaleX, scaleY);

        const int resizedWidth = std::max(1, std::min(
            modelWidth, static_cast<int>(std::lround(sourceSize.width * scale))));
        const int resizedHeight = std::max(1, std::min(
            modelHeight, static_cast<int>(std::lround(sourceSize.height * scale))));
        const int offsetX = (modelWidth - resizedWidth) / 2;
        const int offsetY = (modelHeight - resizedHeight) / 2;
        transform.contentRect = cv::Rect(offsetX, offsetY, resizedWidth, resizedHeight);
        return transform;
    }

    cv::Mat MakeImageBlob(const cv::Mat& bgr, const LetterboxTransform& transform) const
    {
        cv::Mat resized;
        cv::resize(bgr, resized, transform.contentRect.size(), 0, 0, cv::INTER_LINEAR);
        cv::Mat canvas(modelHeight, modelWidth, bgr.type(), cv::Scalar::all(0));
        resized.copyTo(canvas(transform.contentRect));

        const int shape[] = {1, 3, modelHeight, modelWidth};
        cv::Mat blob(4, shape, CV_32F);
        float* dst = blob.ptr<float>();
        for (int y = 0; y < canvas.rows; ++y) {
            const cv::Vec3b* row = canvas.ptr<cv::Vec3b>(y);
            for (int x = 0; x < canvas.cols; ++x) {
                dst[(0 * modelHeight + y) * modelWidth + x] = static_cast<float>(row[x][2]) / 255.0f;
                dst[(1 * modelHeight + y) * modelWidth + x] = static_cast<float>(row[x][1]) / 255.0f;
                dst[(2 * modelHeight + y) * modelWidth + x] = static_cast<float>(row[x][0]) / 255.0f;
            }
        }
        return blob;
    }

    cv::Mat MakeMaskBlob(const cv::Mat& mask, const LetterboxTransform& transform) const
    {
        cv::Mat resized;
        cv::resize(mask, resized, transform.contentRect.size(), 0, 0, cv::INTER_NEAREST);
        cv::Mat canvas(modelHeight, modelWidth, CV_8U, cv::Scalar(0));
        resized.copyTo(canvas(transform.contentRect));

        const int shape[] = {1, 1, modelHeight, modelWidth};
        cv::Mat blob(4, shape, CV_32F, cv::Scalar(0.0f));
        float* dst = blob.ptr<float>();
        for (int y = 0; y < canvas.rows; ++y) {
            const uint8_t* row = canvas.ptr<uint8_t>(y);
            for (int x = 0; x < canvas.cols; ++x)
                dst[y * modelWidth + x] = row[x] ? 1.0f : 0.0f;
        }
        return blob;
    }

    cv::Mat ForegroundFromProb(const cv::Mat& prob) const
    {
        const int shape[] = {1, 1, modelHeight, modelWidth};
        cv::Mat foreground(4, shape, CV_32F);
        const float* src = prob.ptr<float>();
        float* dst = foreground.ptr<float>();
        const int plane = modelWidth * modelHeight;
        std::memcpy(dst, src + plane, sizeof(float) * plane);
        return foreground;
    }

    cv::Mat BinaryMaskFromForeground(const cv::Mat& foreground, const LetterboxTransform& transform) const
    {
        cv::Mat modelMask(modelHeight, modelWidth, CV_8U, cv::Scalar(0));
        const float* src = foreground.ptr<float>();
        for (int y = 0; y < modelMask.rows; ++y) {
            uint8_t* row = modelMask.ptr<uint8_t>(y);
            for (int x = 0; x < modelMask.cols; ++x)
                row[x] = src[y * modelWidth + x] >= 0.5f ? 255 : 0;
        }

        cv::Mat cropped = modelMask(transform.contentRect);
        cv::Mat restored;
        cv::resize(cropped, restored, transform.originalSize, 0, 0, cv::INTER_NEAREST);
        return restored;
    }

    cv::Mat ValidMaskFromLetterbox(const LetterboxTransform& transform) const
    {
        cv::Mat valid(stride16Height, stride16Width, CV_32F, cv::Scalar(0.0f));
        for (int y = 0; y < stride16Height; ++y) {
            float* row = valid.ptr<float>(y);
            const int centerY = y * 16 + 8;
            for (int x = 0; x < stride16Width; ++x) {
                const int centerX = x * 16 + 8;
                if (transform.contentRect.contains(cv::Point(centerX, centerY)))
                    row[x] = 1.0f;
            }
        }

        const int shape[] = {1, 1, stride16Height, stride16Width};
        cv::Mat blob(4, shape, CV_32F);
        std::memcpy(blob.ptr<float>(), valid.ptr<float>(), sizeof(float) * valid.total());
        return blob;
    }

    void CopyKeySlot(const cv::Mat& src, cv::Mat& dst, int slot, int channels) const
    {
        const float* in = src.ptr<float>();
        float* out = dst.ptr<float>();
        const int plane = stride16Width * stride16Height;
        for (int c = 0; c < channels; ++c) {
            std::memcpy(out + (c * memorySlots + slot) * plane,
                        in + c * plane,
                        sizeof(float) * plane);
        }
    }

    void CopyValueSlot(const cv::Mat& src, cv::Mat& dst, int slot) const
    {
        const float* in = src.ptr<float>();
        float* out = dst.ptr<float>();
        const int plane = stride16Width * stride16Height;
        for (int c = 0; c < 256; ++c) {
            std::memcpy(out + (c * memorySlots + slot) * plane,
                        in + c * plane,
                        sizeof(float) * plane);
        }
    }

    cv::Mat MemoryKeyBlob() const
    {
        cv::Mat output = MakeBlob({1, 64, memorySlots, stride16Height, stride16Width});
        int slot = 0;
        if (hasPermanentMemory)
            CopyKeySlot(permanentMemory.key, output, slot++, 64);
        for (int index = 0;
             index < static_cast<int>(workingMemoryFrames.size()) && slot < memorySlots;
             ++index, ++slot)
            CopyKeySlot(workingMemoryFrames[index].key, output, slot, 64);
        return output;
    }

    cv::Mat MemoryShrinkageBlob() const
    {
        cv::Mat output = MakeBlob({1, 1, memorySlots, stride16Height, stride16Width});
        int slot = 0;
        if (hasPermanentMemory)
            CopyKeySlot(permanentMemory.shrinkage, output, slot++, 1);
        for (int index = 0;
             index < static_cast<int>(workingMemoryFrames.size()) && slot < memorySlots;
             ++index, ++slot)
            CopyKeySlot(workingMemoryFrames[index].shrinkage, output, slot, 1);
        return output;
    }

    cv::Mat MemoryValueBlob() const
    {
        cv::Mat output = MakeBlob({1, 1, 256, memorySlots, stride16Height, stride16Width});
        int slot = 0;
        if (hasPermanentMemory)
            CopyValueSlot(permanentMemory.value, output, slot++);
        for (int index = 0;
             index < static_cast<int>(workingMemoryFrames.size()) && slot < memorySlots;
             ++index, ++slot)
            CopyValueSlot(workingMemoryFrames[index].value, output, slot);
        return output;
    }

    cv::Mat MemoryValidBlob() const
    {
        cv::Mat output = MakeBlob({1, 1, memorySlots, stride16Height, stride16Width});
        float* data = output.ptr<float>();
        const int plane = stride16Width * stride16Height;
        auto copyValidSlot = [&](const cv::Mat& valid, int slot) {
            std::memcpy(data + slot * plane, valid.ptr<float>(), sizeof(float) * plane);
        };

        int slot = 0;
        if (hasPermanentMemory)
            copyValidSlot(permanentMemory.valid, slot++);
        for (int index = 0;
             index < static_cast<int>(workingMemoryFrames.size()) && slot < memorySlots;
             ++index, ++slot)
            copyValidSlot(workingMemoryFrames[index].valid, slot);
        return output;
    }

    void AddMemory(const cv::Mat& key, const cv::Mat& shrinkage, const cv::Mat& value,
                   const cv::Mat& valid, bool asPermanent)
    {
        MemoryFrame frame;
        frame.key = key.clone();
        frame.shrinkage = shrinkage.clone();
        frame.value = value.clone();
        frame.valid = valid.clone();

        if (asPermanent || !hasPermanentMemory) {
            permanentMemory = frame;
            hasPermanentMemory = true;
            return;
        }

        workingMemoryFrames.push_back(frame);
        const int workingCapacity = std::max(0, maxMemoryFrames - 1);
        while (static_cast<int>(workingMemoryFrames.size()) > workingCapacity)
            workingMemoryFrames.pop_front();
    }

    void AddObjectMemory(const cv::Mat& value)
    {
        if (objectMemory.empty()) {
            objectMemory = MakeBlob({1, 1, 1, 16, 257});
            std::memcpy(objectMemory.ptr<float>(), value.ptr<float>(), sizeof(float) * value.total());
            return;
        }

        float* dst = objectMemory.ptr<float>();
        const float* src = value.ptr<float>();
        for (size_t i = 0; i < value.total(); ++i)
            dst[i] += src[i];
    }

public:
    void Load(const std::string& encodeKeyPath, const std::string& encodeValuePath,
              const std::string& memoryReadoutPath, const std::string& decodePath)
    {
        ConfigureModelSize(encodeKeyPath);
        encodeKey = cv::dnn::readNetFromONNX(encodeKeyPath);
        encodeValue = cv::dnn::readNetFromONNX(encodeValuePath);
        memoryReadout = cv::dnn::readNetFromONNX(memoryReadoutPath);
        decode = cv::dnn::readNetFromONNX(decodePath);
        sensory = MakeBlob({1, 1, 256, stride16Height, stride16Width});
    }

    std::string SetDevice(const std::string& processingDevice)
    {
        std::string selected = SetNetDevice(encodeKey, processingDevice);
        const std::string valueDevice = SetNetDevice(encodeValue, processingDevice);
        const std::string readoutDevice = SetNetDevice(memoryReadout, processingDevice);
        const std::string decodeDevice = SetNetDevice(decode, processingDevice);
        if (selected != valueDevice || selected != readoutDevice || selected != decodeDevice)
            return "Mixed";
        return selected;
    }

    void Reset()
    {
        sensory = MakeBlob({1, 1, 256, stride16Height, stride16Width});
        lastMask.release();
        objectMemory.release();
        permanentMemory = MemoryFrame();
        hasPermanentMemory = false;
        workingMemoryFrames.clear();
        frameIndex = 0;
        lastMemoryFrame = -1000000;
    }

    bool HasMemory() const
    {
        return hasPermanentMemory || !workingMemoryFrames.empty();
    }

    cv::Mat Step(const cv::Mat& frame, const cv::Mat& seedMask = cv::Mat())
    {
        const LetterboxTransform transform = ComputeLetterbox(frame.size());
        const cv::Mat validMask = ValidMaskFromLetterbox(transform);
        cv::Mat image = MakeImageBlob(frame, transform);

        encodeKey.setInput(image, "image");
        std::vector<cv::Mat> keyOutputs;
        encodeKey.forward(keyOutputs, std::vector<cv::String>{"f16", "f8", "f4", "pix_feat", "key", "shrinkage", "selection"});
        cv::Mat f8 = keyOutputs[1];
        cv::Mat f4 = keyOutputs[2];
        cv::Mat pixFeat = keyOutputs[3];
        cv::Mat key = keyOutputs[4];
        cv::Mat shrinkage = keyOutputs[5];
        cv::Mat selection = keyOutputs[6];

        cv::Mat foreground;
        if (!seedMask.empty()) {
            foreground = MakeMaskBlob(seedMask, transform);
        } else if (HasMemory()) {
            memoryReadout.setInput(key, "query_key");
            memoryReadout.setInput(selection, "query_selection");
            memoryReadout.setInput(MemoryKeyBlob(), "memory_key");
            memoryReadout.setInput(MemoryShrinkageBlob(), "memory_shrinkage");
            memoryReadout.setInput(MemoryValueBlob(), "memory_value");
            memoryReadout.setInput(MemoryValidBlob(), "memory_valid");
            memoryReadout.setInput(objectMemory, "object_memory");
            memoryReadout.setInput(pixFeat, "pix_feat");
            memoryReadout.setInput(sensory, "sensory");
            memoryReadout.setInput(lastMask, "last_mask");
            std::vector<cv::Mat> readoutOutputs;
            memoryReadout.forward(readoutOutputs, std::vector<cv::String>{"memory_readout"});

            decode.setInput(f8, "f8");
            decode.setInput(f4, "f4");
            decode.setInput(readoutOutputs[0], "memory_readout");
            decode.setInput(sensory, "sensory");
            std::vector<cv::Mat> decodeOutputs;
            decode.forward(decodeOutputs, std::vector<cv::String>{"new_sensory", "logits", "prob"});
            sensory = decodeOutputs[0].clone();
            foreground = ForegroundFromProb(decodeOutputs[2]);
        } else {
            ++frameIndex;
            return cv::Mat();
        }

        const bool isMemoryFrame = !seedMask.empty() || frameIndex - lastMemoryFrame >= memEvery;
        if (isMemoryFrame) {
            encodeValue.setInput(image, "image");
            encodeValue.setInput(pixFeat, "pix_feat");
            encodeValue.setInput(sensory, "sensory");
            encodeValue.setInput(foreground, "mask");
            std::vector<cv::Mat> valueOutputs;
            encodeValue.forward(valueOutputs, std::vector<cv::String>{"mask_value", "new_sensory", "object_memory"});
            sensory = valueOutputs[1].clone();
            AddObjectMemory(valueOutputs[2]);
            AddMemory(key, shrinkage, valueOutputs[0], validMask, !seedMask.empty());
            lastMemoryFrame = frameIndex;
        }

        lastMask = foreground.clone();
        cv::Mat outputMask = BinaryMaskFromForeground(foreground, transform);
        ++frameIndex;
        return outputMask;
    }
};

}

CVObjectMask::CVObjectMask(std::string processInfoJson, ProcessingController& controller)
    : processingController(&controller)
{
    SetJson(processInfoJson);
}

std::string CVObjectMask::ValidateONNXModel(std::string modelPath)
{
    return LoadONNXModel(modelPath, nullptr);
}

std::shared_ptr<Frame> CVObjectMask::PreviewSeedMask(std::shared_ptr<Frame> frame)
{
    if (!frame || efficientSamModelPath.empty() || promptKeyframes.empty())
        return std::shared_ptr<Frame>();

    std::string loadError = LoadONNXModel(efficientSamModelPath, &efficientSam);
    if (!loadError.empty())
        return std::shared_ptr<Frame>();
    SetProcessingDevice();

    CVObjectMaskPromptSet prompts = promptKeyframes.begin()->second;
    cv::Mat frameImage = frame->GetImageCV();
    cv::Mat seedMask = CreateEfficientSAMSeedMask(frameImage, prompts);
    if (seedMask.empty())
        return std::shared_ptr<Frame>();

    auto maskImage = std::make_shared<QImage>(
        seedMask.cols, seedMask.rows, QImage::Format_RGBA8888_Premultiplied);
    maskImage->fill(Qt::transparent);
    for (int y = 0; y < seedMask.rows; ++y) {
        const uint8_t* src = seedMask.ptr<uint8_t>(y);
        QRgb* dst = reinterpret_cast<QRgb*>(maskImage->scanLine(y));
        for (int x = 0; x < seedMask.cols; ++x)
            dst[x] = src[x] ? qRgba(255, 255, 255, 255) : qRgba(0, 0, 0, 0);
    }

    auto result = std::make_shared<Frame>(frame->number, seedMask.cols, seedMask.rows, "#000000");
    result->AddImage(maskImage);
    return result;
}

void CVObjectMask::SetProcessingDevice()
{
    const std::string requestedDevice = processingDevice;
    if (processingDevice == "CPU") {
        efficientSam.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        efficientSam.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        ZmqLogger::Instance()->Log("Object Mask EfficientSAM DNN device: requested CPU, selected CPU");
        return;
    }

    if (processingDevice == "GPU" || processingDevice == "GPU_AUTO" || processingDevice == "GPU_CUDA") {
        try {
            const std::vector<cv::dnn::Target> targets = cv::dnn::getAvailableTargets(cv::dnn::DNN_BACKEND_CUDA);
            if (std::find(targets.begin(), targets.end(), cv::dnn::DNN_TARGET_CUDA) != targets.end()) {
                efficientSam.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
                efficientSam.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
                ZmqLogger::Instance()->Log("Object Mask EfficientSAM DNN device: requested " + requestedDevice + ", selected CUDA");
                return;
            }
        } catch (const cv::Exception&) {
        }
    }

    if (processingDevice == "GPU_OPENCL") {
        try {
            const std::vector<cv::dnn::Target> targets = cv::dnn::getAvailableTargets(cv::dnn::DNN_BACKEND_OPENCV);
            if (std::find(targets.begin(), targets.end(), cv::dnn::DNN_TARGET_OPENCL) != targets.end()) {
                cv::ocl::setUseOpenCL(true);
                efficientSam.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
                efficientSam.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);
                ZmqLogger::Instance()->Log("Object Mask EfficientSAM DNN device: requested " + requestedDevice + ", selected OpenCL");
                return;
            }
        } catch (const cv::Exception&) {
        }
    }

    processingDevice = "CPU";
    efficientSam.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    efficientSam.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    ZmqLogger::Instance()->Log("Object Mask EfficientSAM DNN device: requested " + requestedDevice + ", selected CPU");
}

void CVObjectMask::maskClip(openshot::Clip& video, size_t _start, size_t _end, bool process_interval)
{
    start = _start;
    end = _end;

    video.Open();
    processingController->SetError(false, "");

    if (efficientSamModelPath.empty()) {
        processingController->SetError(true, "Missing path to EfficientSAM ONNX model file");
        error = true;
        return;
    }
    if (protobufDataPath.empty()) {
        processingController->SetError(true, "Missing path to object mask protobuf data file");
        error = true;
        return;
    }
    if (promptKeyframes.empty()) {
        processingController->SetError(true, "Missing positive prompt point for Object Mask preprocessing");
        error = true;
        return;
    }

    std::string loadError = LoadONNXModel(efficientSamModelPath, &efficientSam);
    if (!loadError.empty()) {
        processingController->SetError(true, loadError);
        error = true;
        return;
    }
    SetProcessingDevice();

    CutiePropagator cutie;
    if (cutieEncodeKeyModelPath.empty() && !cutieModelDir.empty())
        cutieEncodeKeyModelPath = cutieModelDir + "/cutie-encode-key-640x368.onnx";
    if (cutieEncodeValueModelPath.empty() && !cutieModelDir.empty())
        cutieEncodeValueModelPath = cutieModelDir + "/cutie-encode-value-640x368.onnx";
    if (cutieMemoryReadoutModelPath.empty() && !cutieModelDir.empty())
        cutieMemoryReadoutModelPath = cutieModelDir + "/cutie-memory-readout-floatmask-valid-640x368-m6-topk30-opencv.onnx";
    if (cutieDecodeModelPath.empty() && !cutieModelDir.empty())
        cutieDecodeModelPath = cutieModelDir + "/cutie-decode-640x368.onnx";
    if (cutieEncodeKeyModelPath.empty() || cutieEncodeValueModelPath.empty() ||
        cutieMemoryReadoutModelPath.empty() || cutieDecodeModelPath.empty()) {
        processingController->SetError(true, "Missing path to Cutie ONNX model files");
        error = true;
        return;
    }
    try {
        cutie.Load(cutieEncodeKeyModelPath, cutieEncodeValueModelPath, cutieMemoryReadoutModelPath, cutieDecodeModelPath);
        const std::string cutieDevice = cutie.SetDevice(processingDevice);
        ZmqLogger::Instance()->Log("Object Mask Cutie DNN device: requested " + processingDevice + ", selected " + cutieDevice);
    } catch (const cv::Exception& e) {
        processingController->SetError(true, std::string("Failed to load Cutie ONNX models: ") + e.what());
        error = true;
        return;
    } catch (const std::exception& e) {
        processingController->SetError(true, std::string("Failed to load Cutie ONNX models: ") + e.what());
        error = true;
        return;
    }

    if (!process_interval || end <= 1 || end - start == 0) {
        start = static_cast<size_t>(video.Start() * video.Reader()->info.fps.ToFloat());
        end = static_cast<size_t>(video.End() * video.Reader()->info.fps.ToFloat());
    }
    if (end < start)
        end = start;

    CVObjectMaskPromptSet activePrompts;
    auto promptBeforeStart = promptKeyframes.upper_bound(start);
    if (promptBeforeStart != promptKeyframes.begin()) {
        --promptBeforeStart;
        activePrompts = promptBeforeStart->second;
    }
    auto firstPromptAtOrAfterStart = promptKeyframes.lower_bound(start);

    for (size_t frameNumber = start; frameNumber <= end; ++frameNumber) {
        if (processingController->ShouldStop())
            return;

        std::shared_ptr<openshot::Frame> frame = video.GetFrame(frameNumber);
        if (!frame)
            continue;

        auto promptIt = promptKeyframes.find(frameNumber);
        bool isPromptKeyframe = promptIt != promptKeyframes.end();
        if (promptIt != promptKeyframes.end()) {
            activePrompts = promptIt->second;
            cutie.Reset();
        } else if (!activePrompts.HasPositivePrompt()) {
            if (firstPromptAtOrAfterStart != promptKeyframes.end() && frameNumber >= firstPromptAtOrAfterStart->first) {
                activePrompts = firstPromptAtOrAfterStart->second;
                isPromptKeyframe = true;
                cutie.Reset();
            } else {
                CVObjectMaskFrameData emptyFrame;
                emptyFrame.frameId = frameNumber;
                masksData[frameNumber] = emptyFrame;
                continue;
            }
        }

        const cv::Mat frameImage = frame->GetImageCV();
        cv::Mat seedMask;
        if (isPromptKeyframe || !cutie.HasMemory()) {
            seedMask = CreateEfficientSAMSeedMask(frameImage, activePrompts);
            if (seedMask.empty()) {
                CVObjectMaskFrameData emptyFrame;
                emptyFrame.frameId = frameNumber;
                masksData[frameNumber] = emptyFrame;
                continue;
            }
            if (!isPromptKeyframe)
                cutie.Reset();
        }

        cv::Mat propagatedMask;
        try {
            propagatedMask = cutie.Step(frameImage, seedMask);
        } catch (const cv::Exception& e) {
            processingController->SetError(true, std::string("Failed to propagate Object Mask with Cutie: ") + e.what());
            error = true;
            return;
        }

        cv::Mat outputMask;
        if (!seedMask.empty()) {
            outputMask = seedMask;
        } else if (!propagatedMask.empty()) {
            cv::resize(propagatedMask, outputMask, frameImage.size(), 0, 0, cv::INTER_NEAREST);
        }
        masksData[frameNumber] = FrameDataFromMask(outputMask, frameNumber, 1.0f);

        const size_t range = std::max<size_t>(1, end - start);
        processingController->SetProgress(uint(100 * (frameNumber - start) / range));
    }
}

cv::Mat CVObjectMask::CreateEfficientSAMSeedMask(const cv::Mat& frame, const CVObjectMaskPromptSet& prompts)
{
    EfficientSamPreprocessResult prep = MakeEfficientSamBlob(frame, modelSize);

    auto runPromptSet = [&](const CVObjectMaskPromptSet& promptSet) -> cv::Mat {
        std::vector<cv::Point> backgroundPoints;
        std::vector<cv::Rect> backgroundRects;
        cv::Mat pointCoords = MakeEfficientSamPromptBlob(promptSet, prep, promptSlots, backgroundPoints, backgroundRects);
        cv::Mat pointLabels = MakeEfficientSamLabelBlob(promptSet, promptSlots);

        efficientSam.setInput(prep.blob, "batched_images");
        efficientSam.setInput(pointCoords, "batched_point_coords");
        efficientSam.setInput(pointLabels, "batched_point_labels");

        std::vector<cv::Mat> outputs;
        efficientSam.forward(outputs, std::vector<cv::String>{"output_masks", "iou_predictions"});
        if (outputs.size() != 2)
            return cv::Mat();

        cv::Mat modelMask = SelectEfficientSamMask(outputs[0], outputs[1], backgroundPoints, backgroundRects, maskThreshold);
        if (modelMask.empty())
            return cv::Mat();
        return EfficientSamMaskToFrameMask(modelMask, frame.size(), maskThreshold);
    };

    if (prompts.positiveRects.size() <= 1)
        return runPromptSet(prompts);

    cv::Mat combinedMask(frame.rows, frame.cols, CV_8U, cv::Scalar(0));
    bool hasMask = false;
    for (const auto& rect : prompts.positiveRects) {
        CVObjectMaskPromptSet rectPrompt;
        rectPrompt.positiveRects.push_back(rect);
        rectPrompt.negativePoints = prompts.negativePoints;
        rectPrompt.negativeRects = prompts.negativeRects;
        cv::Mat rectMask = runPromptSet(rectPrompt);
        if (rectMask.empty())
            continue;
        cv::bitwise_or(combinedMask, rectMask, combinedMask);
        hasMask = true;
    }

    if (!prompts.positivePoints.empty()) {
        CVObjectMaskPromptSet pointPrompt;
        pointPrompt.positivePoints = prompts.positivePoints;
        pointPrompt.negativePoints = prompts.negativePoints;
        pointPrompt.negativeRects = prompts.negativeRects;
        cv::Mat pointMask = runPromptSet(pointPrompt);
        if (!pointMask.empty()) {
            cv::bitwise_or(combinedMask, pointMask, combinedMask);
            hasMask = true;
        }
    }

    return hasMask ? combinedMask : cv::Mat();
}

bool CVObjectMask::SaveObjMaskData()
{
    if (protobufDataPath.empty()) {
        std::cerr << "Missing path to object mask protobuf data file." << std::endl;
        return false;
    }
    if (error)
        return false;

    pb_objdetect::ObjDetect objMessage;
    objMessage.add_classnames()->assign("object mask");

    for (const auto& frameData : masksData)
        AddFrameDataToProto(objMessage.add_frame(), frameData.second);

    *objMessage.mutable_last_updated() = TimeUtil::SecondsToTimestamp(time(NULL));

    std::fstream output(protobufDataPath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!objMessage.SerializeToOstream(&output)) {
        std::cerr << "Failed to write object mask protobuf message." << std::endl;
        return false;
    }

    return true;
}

void CVObjectMask::AddFrameDataToProto(pb_objdetect::Frame* pbFrameData, const CVObjectMaskFrameData& frameData)
{
    pbFrameData->set_id(frameData.frameId);
    if (!frameData.HasMask())
        return;

    pb_objdetect::Frame_Box* box = pbFrameData->add_bounding_box();
    box->set_x(frameData.box.x);
    box->set_y(frameData.box.y);
    box->set_w(frameData.box.width);
    box->set_h(frameData.box.height);
    box->set_classid(0);
    box->set_confidence(frameData.score);
    box->set_objectid(frameData.objectId);

    pb_objdetect::Frame_Box_Mask* mask = box->mutable_mask();
    mask->set_width(frameData.width);
    mask->set_height(frameData.height);
    for (uint32_t count : frameData.rle)
        mask->add_rle(count);
}

void CVObjectMask::SetJson(const std::string value)
{
    try {
        SetJsonValue(openshot::stringToJson(value));
    } catch (const std::exception&) {
        std::cout << "JSON is invalid (missing keys or invalid data types)" << std::endl;
    }
}

void CVObjectMask::SetJsonValue(const Json::Value root)
{
    if (!root["protobuf_data_path"].isNull())
        protobufDataPath = root["protobuf_data_path"].asString();
    if (!root["efficient_sam_model"].isNull())
        efficientSamModelPath = root["efficient_sam_model"].asString();
    if (!root["efficient_sam_model_path"].isNull())
        efficientSamModelPath = root["efficient_sam_model_path"].asString();
    if (!root["sam_model"].isNull())
        efficientSamModelPath = root["sam_model"].asString();
    if (!root["sam_model_path"].isNull())
        efficientSamModelPath = root["sam_model_path"].asString();
    if (!root["encoder_model"].isNull())
        efficientSamModelPath = root["encoder_model"].asString();
    if (!root["encoder_model_path"].isNull())
        efficientSamModelPath = root["encoder_model_path"].asString();
    if (!root["cutie_model_dir"].isNull())
        cutieModelDir = root["cutie_model_dir"].asString();
    if (!root["cutie_encode_key_model"].isNull())
        cutieEncodeKeyModelPath = root["cutie_encode_key_model"].asString();
    if (!root["cutie_encode_key_model_path"].isNull())
        cutieEncodeKeyModelPath = root["cutie_encode_key_model_path"].asString();
    if (!root["cutie_encode_value_model"].isNull())
        cutieEncodeValueModelPath = root["cutie_encode_value_model"].asString();
    if (!root["cutie_encode_value_model_path"].isNull())
        cutieEncodeValueModelPath = root["cutie_encode_value_model_path"].asString();
    if (!root["cutie_memory_readout_model"].isNull())
        cutieMemoryReadoutModelPath = root["cutie_memory_readout_model"].asString();
    if (!root["cutie_memory_readout_model_path"].isNull())
        cutieMemoryReadoutModelPath = root["cutie_memory_readout_model_path"].asString();
    if (!root["cutie_decode_model"].isNull())
        cutieDecodeModelPath = root["cutie_decode_model"].asString();
    if (!root["cutie_decode_model_path"].isNull())
        cutieDecodeModelPath = root["cutie_decode_model_path"].asString();
    if (!root["processing-device"].isNull())
        processingDevice = root["processing-device"].asString();
    if (!root["processing_device"].isNull())
        processingDevice = root["processing_device"].asString();
    if (!root["prompt_slots"].isNull())
        promptSlots = std::max(1, std::min(6, root["prompt_slots"].asInt()));
    if (!root["mask_threshold"].isNull())
        maskThreshold = root["mask_threshold"].asFloat();
    if (!root["model_size"].isNull())
        modelSize = root["model_size"].asInt();
    promptKeyframes.clear();
    if (!root["object_mask_selection"].isNull()) {
        const Json::Value& selection = root["object_mask_selection"];
        const Json::Value& frames = selection["frames"];
        if (frames.isObject()) {
            for (const auto& frameName : frames.getMemberNames()) {
                const size_t frameNumber = JsonFrameNumber(frameName);
                if (frameNumber == 0)
                    continue;
                CVObjectMaskPromptSet prompts = PromptSetFromJson(frames[frameName]);
                if (prompts.HasPositivePrompt())
                    promptKeyframes[frameNumber] = prompts;
            }
        }
    }

    CVObjectMaskPromptSet legacyPrompts;
    if (!root["positive_points"].isNull())
        AppendJsonPoints(root["positive_points"], legacyPrompts.positivePoints);
    if (!root["negative_points"].isNull())
        AppendJsonPoints(root["negative_points"], legacyPrompts.negativePoints);

    if (!root["positive_x"].isNull() && !root["positive_y"].isNull()) {
        cv::Point2f point(root["positive_x"].asFloat(), root["positive_y"].asFloat());
        if (IsValidPoint(point) && legacyPrompts.positivePoints.empty())
            legacyPrompts.positivePoints.push_back(point);
    }
    if (!root["negative_x"].isNull() && !root["negative_y"].isNull()) {
        cv::Point2f point(root["negative_x"].asFloat(), root["negative_y"].asFloat());
        if (IsValidPoint(point) && legacyPrompts.negativePoints.empty())
            legacyPrompts.negativePoints.push_back(point);
    }
    if (!root["rect_x1"].isNull() && !root["rect_y1"].isNull() &&
        !root["rect_x2"].isNull() && !root["rect_y2"].isNull()) {
        Json::Value rect;
        rect["x1"] = root["rect_x1"];
        rect["y1"] = root["rect_y1"];
        rect["x2"] = root["rect_x2"];
        rect["y2"] = root["rect_y2"];
        cv::Rect_<float> parsed;
        if (RectFromJson(rect, parsed))
            legacyPrompts.positiveRects.push_back(parsed);
    }
    if (legacyPrompts.HasPositivePrompt() && promptKeyframes.empty())
        promptKeyframes[1] = legacyPrompts;
}
