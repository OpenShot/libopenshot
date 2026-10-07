/**
 * @file
 * @brief Source file for CVObjectDetection class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

#include "CVObjectDetection.h"
#include "Exceptions.h"
#include "Logger.h"

#define int64 int64_t
#define uint64 uint64_t
#include <opencv2/core/ocl.hpp>
#undef uint64
#undef int64
#include "objdetectdata.pb.h"
#include <google/protobuf/util/time_util.h>

using namespace std;
using namespace openshot;
using google::protobuf::util::TimeUtil;

namespace {

bool LooksLikeTransposedYoloOutput(const cv::Mat& out, size_t classCount)
{
    // YOLO26 segmentation exports without end-to-end postprocessing use
    // [1, attributes, candidates], e.g. [1, 116, 8400]:
    // 4 box channels + class scores + optional mask coefficients.
    return out.dims == 3 && out.size[0] == 1 && out.size[1] >= 4 &&
        out.size[2] > out.size[1] &&
        (classCount == 0 || out.size[1] >= 4 + static_cast<int>(classCount));
}

cv::Rect ScaledXYWHBox(
    float centerX,
    float centerY,
    float width,
    float height,
    const cv::Size& frameDims,
    int inputWidth,
    int inputHeight)
{
    if (centerX <= 1.0f && centerY <= 1.0f && width <= 1.0f && height <= 1.0f) {
        centerX *= static_cast<float>(frameDims.width);
        width *= static_cast<float>(frameDims.width);
        centerY *= static_cast<float>(frameDims.height);
        height *= static_cast<float>(frameDims.height);
    } else {
        const float xFactor = static_cast<float>(frameDims.width) / static_cast<float>(inputWidth);
        const float yFactor = static_cast<float>(frameDims.height) / static_cast<float>(inputHeight);
        centerX *= xFactor;
        width *= xFactor;
        centerY *= yFactor;
        height *= yFactor;
    }

    float left = centerX - width / 2.0f;
    float top = centerY - height / 2.0f;
    float right = centerX + width / 2.0f;
    float bottom = centerY + height / 2.0f;

    left = std::max(0.0f, std::min(left, static_cast<float>(frameDims.width - 1)));
    top = std::max(0.0f, std::min(top, static_cast<float>(frameDims.height - 1)));
    right = std::max(0.0f, std::min(right, static_cast<float>(frameDims.width)));
    bottom = std::max(0.0f, std::min(bottom, static_cast<float>(frameDims.height)));

    return cv::Rect(
        static_cast<int>(left),
        static_cast<int>(top),
        std::max(0, static_cast<int>(right - left)),
        std::max(0, static_cast<int>(bottom - top)));
}

std::vector<uint32_t> EncodeBinaryMaskRLE(const std::vector<uint8_t>& mask)
{
    std::vector<uint32_t> rle;
    if (mask.empty())
        return rle;

    uint8_t current = 0;
    uint32_t count = 0;
    for (uint8_t value : mask) {
        value = value ? 1 : 0;
        if (value == current) {
            ++count;
        } else {
            rle.push_back(count);
            current = value;
            count = 1;
        }
    }
    rle.push_back(count);
    return rle;
}

cv::Mat DecodeBinaryMaskRLE(const CVObjectMaskData& mask)
{
    cv::Mat image(mask.height, mask.width, CV_8UC1, cv::Scalar(0));
    if (!mask.HasData())
        return image;

    const int total = mask.width * mask.height;
    int offset = 0;
    bool value = false;
    uint8_t* data = image.ptr<uint8_t>();
    for (uint32_t count : mask.rle) {
        const int end = std::min(total, offset + static_cast<int>(count));
        if (value) {
            std::fill(data + offset, data + end, static_cast<uint8_t>(1));
        }
        offset = end;
        value = !value;
        if (offset >= total)
            break;
    }
    return image;
}

CVObjectMaskData TransformMaskToBox(
    const CVObjectMaskData& sourceMask,
    const cv::Rect_<float>& sourceBox,
    const cv::Rect_<float>& targetBox,
    const cv::Size& frameDims)
{
    CVObjectMaskData result;
    if (!sourceMask.HasData() || sourceBox.width <= 0.0f || sourceBox.height <= 0.0f ||
        targetBox.width <= 0.0f || targetBox.height <= 0.0f ||
        frameDims.width <= 0 || frameDims.height <= 0) {
        return result;
    }

    const float scaleX = sourceMask.width / static_cast<float>(frameDims.width);
    const float scaleY = sourceMask.height / static_cast<float>(frameDims.height);
    const cv::Rect_<float> sourceMaskBox(
        sourceBox.x * scaleX,
        sourceBox.y * scaleY,
        sourceBox.width * scaleX,
        sourceBox.height * scaleY);
    const cv::Rect_<float> targetMaskBox(
        targetBox.x * scaleX,
        targetBox.y * scaleY,
        targetBox.width * scaleX,
        targetBox.height * scaleY);
    if (sourceMaskBox.width <= 0.0f || sourceMaskBox.height <= 0.0f)
        return result;

    const double xScale = targetMaskBox.width / sourceMaskBox.width;
    const double yScale = targetMaskBox.height / sourceMaskBox.height;
    cv::Mat transform = (cv::Mat_<double>(2, 3) <<
        xScale, 0.0, targetMaskBox.x - xScale * sourceMaskBox.x,
        0.0, yScale, targetMaskBox.y - yScale * sourceMaskBox.y);

    cv::Mat source = DecodeBinaryMaskRLE(sourceMask);
    cv::Mat transformed;
    cv::warpAffine(
        source, transformed, transform, source.size(),
        cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(0));
    if (cv::countNonZero(transformed) == 0)
        return result;

    result.width = sourceMask.width;
    result.height = sourceMask.height;
    result.rle = EncodeBinaryMaskRLE(
        std::vector<uint8_t>(transformed.data, transformed.data + transformed.total()));
    return result;
}

CVObjectMaskData BuildMaskFromPrototype(
    const cv::Mat& prototype,
    const std::vector<float>& coefficients,
    const cv::Rect& box,
    const cv::Size& frameDims)
{
    CVObjectMaskData result;
    if (prototype.dims != 4 || prototype.size[0] != 1 ||
        prototype.size[1] != static_cast<int>(coefficients.size()))
        return result;

    const int channels = prototype.size[1];
    const int maskHeight = prototype.size[2];
    const int maskWidth = prototype.size[3];
    const int maskPixels = maskWidth * maskHeight;
    const float* protoData = reinterpret_cast<const float*>(prototype.data);

    const int left = std::max(0, static_cast<int>(box.x * maskWidth / static_cast<float>(frameDims.width)));
    const int top = std::max(0, static_cast<int>(box.y * maskHeight / static_cast<float>(frameDims.height)));
    const int right = std::min(maskWidth, static_cast<int>((box.x + box.width) * maskWidth / static_cast<float>(frameDims.width)));
    const int bottom = std::min(maskHeight, static_cast<int>((box.y + box.height) * maskHeight / static_cast<float>(frameDims.height)));
    if (left >= right || top >= bottom)
        return result;

    std::vector<uint8_t> binary(maskPixels, 0);
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const int pixel = y * maskWidth + x;
            float value = 0.0f;
            for (int channel = 0; channel < channels; ++channel) {
                value += coefficients[channel] * protoData[channel * maskPixels + pixel];
            }
            binary[pixel] = value > 0.0f ? 1 : 0;
        }
    }

    result.width = maskWidth;
    result.height = maskHeight;
    result.rle = EncodeBinaryMaskRLE(binary);
    return result;
}

std::string LoadONNXModel(std::string modelPath, cv::dnn::Net *net)
{
#if CV_VERSION_MAJOR < 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 3)
    return std::string("Failed to load ONNX model: YOLO requires OpenCV 4.3.0 or newer. "
                       "This OpenCV build is ") + CV_VERSION + ".";
#else
    try {
        cv::dnn::Net loaded_net = cv::dnn::readNetFromONNX(modelPath);
        if (net) {
            *net = loaded_net;
        }
        return "";
    } catch (const cv::Exception& e) {
        std::string error_text = std::string("Failed to load ONNX model: ") + e.what();
        if (error_text.find("Unsupported data type: FLOAT16") != std::string::npos) {
            error_text = "Failed to load ONNX model: FLOAT16 is not supported by this OpenCV build. "
                         "Please use an FP32 ONNX model.";
        }
        return error_text;
    } catch (const std::exception& e) {
        return std::string("Failed to load ONNX model: ") + e.what();
    } catch (...) {
        return "Failed to load ONNX model: unknown error";
    }
#endif
}

}

CVObjectDetection::CVObjectDetection(std::string processInfoJson, ProcessingController &processingController)
: processingController(&processingController), processingDevice("CPU"), inpWidth(640), inpHeight(640), generateMasks(true){
    confThreshold = 0.10;
    nmsThreshold = 0.1;
    SetJson(processInfoJson);
}

std::string CVObjectDetection::ValidateONNXModel(std::string modelPath)
{
    return LoadONNXModel(modelPath, nullptr);
}

void CVObjectDetection::setProcessingDevice(){
    const std::string requestedDevice = processingDevice;
    if (processingDevice == "CPU") {
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        Logger::Instance()->Log("Object Detection DNN device: requested CPU, selected CPU");
        return;
    }

    if(processingDevice == "GPU" || processingDevice == "GPU_AUTO" || processingDevice == "GPU_CUDA"){
        try {
            const std::vector<cv::dnn::Target> targets = cv::dnn::getAvailableTargets(cv::dnn::DNN_BACKEND_CUDA);
            if (std::find(targets.begin(), targets.end(), cv::dnn::DNN_TARGET_CUDA) != targets.end()) {
                net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
                net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
                Logger::Instance()->Log("Object Detection DNN device: requested " + requestedDevice + ", selected CUDA");
                return;
            }
        } catch (const cv::Exception&) {
        }
    }

    if(processingDevice == "GPU_OPENCL"){
        try {
            const std::vector<cv::dnn::Target> targets = cv::dnn::getAvailableTargets(cv::dnn::DNN_BACKEND_OPENCV);
            if (std::find(targets.begin(), targets.end(), cv::dnn::DNN_TARGET_OPENCL) != targets.end()) {
                cv::ocl::setUseOpenCL(true);
                net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
                net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);
                Logger::Instance()->Log("Object Detection DNN device: requested " + requestedDevice + ", selected OpenCL");
                return;
            }
        } catch (const cv::Exception&) {
        }
    }

    processingDevice = "CPU";
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    Logger::Instance()->Log("Object Detection DNN device: requested " + requestedDevice + ", selected CPU");
}

void CVObjectDetection::detectObjectsClip(openshot::Clip &video, size_t _start, size_t _end, bool process_interval)
{

    start = _start; end = _end;

    video.Open();

    if(error){
        return;
    }

    processingController->SetError(false, "");

    if(modelPath.empty()) {
        processingController->SetError(true, "Missing path to YOLO ONNX model file");
        error = true;
        return;
    }
    if(classesFile.empty()) {
        processingController->SetError(true, "Missing path to class name file");
        error = true;
        return;
    }

    std::ifstream model_file(modelPath);
    if(!model_file.good()){
        processingController->SetError(true, "Incorrect path to YOLO ONNX model file");
        error = true;
        return;
    }
    std::ifstream classes_file(classesFile);
    if(!classes_file.good()){
        processingController->SetError(true, "Incorrect path to class name file");
        error = true;
        return;
    }

    // Load names of classes
    classNames.clear();
    std::string line;
    while (std::getline(classes_file, line)) classNames.push_back(line);

    // Load the network
    std::string error_text = LoadONNXModel(modelPath, &net);
    if (!error_text.empty()) {
        processingController->SetError(true, error_text);
        error = true;
        return;
    }
    setProcessingDevice();

    size_t frame_number;
    if(!process_interval || end <= 1 || end-start == 0){
        // Get total number of frames in video
        start = (int)(video.Start() * video.Reader()->info.fps.ToFloat());
        end = (int)(video.End() * video.Reader()->info.fps.ToFloat());
    }

    for (frame_number = start; frame_number <= end; frame_number++)
    {
         // Stop the feature tracker process
        if(processingController->ShouldStop()){
            return;
        }

        std::shared_ptr<openshot::Frame> f = video.GetFrame(frame_number);

        // Grab OpenCV Mat image
        cv::Mat cvimage = f->GetImageCV();

        DetectObjects(cvimage, frame_number);

        // Update progress
        processingController->SetProgress(uint(100*(frame_number-start)/(end-start)));

    }
}

void CVObjectDetection::DetectObjects(const cv::Mat &frame, size_t frameId){
    // Get frame as OpenCV Mat
    cv::Mat blob;

    // Create a 4D blob from the frame.
    cv::dnn::blobFromImage(frame, blob, 1/255.0, cv::Size(inpWidth, inpHeight), cv::Scalar(0,0,0), true, false);

    std::vector<cv::Mat> outs;
    try {
        // Sets the input to the network
        net.setInput(blob);
        // Runs the forward pass to get output of the output layers
        net.forward(outs, getOutputsNames(net));
    } catch (const cv::Exception& e) {
        processingController->SetError(true, std::string("Object detection inference failed: ") + e.what());
        error = true;
        return;
    }

    // Remove the bounding boxes with low confidence
    postprocess(frame.size(), outs, frameId);

}


// Remove the bounding boxes with low confidence using non-maxima suppression
void CVObjectDetection::postprocess(const cv::Size &frameDims, const std::vector<cv::Mat>& outs, size_t frameId)
{
    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;
    std::vector<std::vector<ClassScore>> detectionClassScores;
    std::vector<CVObjectMaskData> detectionMasks;
    std::vector<int> objectIds;
    const int maxClassCandidates = 5;

    for (size_t i = 0; i < outs.size(); ++i) {
        cv::Mat det = outs[i];

        if (LooksLikeTransposedYoloOutput(det, classNames.size())) {
            const int attributes = det.size[1];
            const int candidates = det.size[2];
            const int classCount = !classNames.empty()
                ? static_cast<int>(classNames.size())
                : attributes - 4;
            const int maskCoefficientCount = attributes - 4 - classCount;
            const cv::Mat* prototype = nullptr;
            if (generateMasks && maskCoefficientCount > 0) {
                auto prototypeIt = std::find_if(outs.begin(), outs.end(),
                    [maskCoefficientCount](const cv::Mat& out) {
                        return out.dims == 4 && out.size[0] == 1 && out.size[1] == maskCoefficientCount;
                    });
                if (prototypeIt != outs.end()) {
                    prototype = &(*prototypeIt);
                }
            }
            const float* data = reinterpret_cast<const float*>(det.data);

            for (int candidateIndex = 0; candidateIndex < candidates; ++candidateIndex) {
                std::vector<ClassScore> rowClassScores;
                rowClassScores.reserve(maxClassCandidates);

                for (int classIndex = 0; classIndex < classCount; ++classIndex) {
                    const float classConfidence = data[(4 + classIndex) * candidates + candidateIndex];
                    if (rowClassScores.size() < static_cast<size_t>(maxClassCandidates)) {
                        rowClassScores.emplace_back(classIndex, classConfidence);
                        std::sort(rowClassScores.begin(), rowClassScores.end(),
                            [](const ClassScore& a, const ClassScore& b) { return a.score > b.score; });
                    } else if (classConfidence > rowClassScores.back().score) {
                        rowClassScores.back() = ClassScore(classIndex, classConfidence);
                        std::sort(rowClassScores.begin(), rowClassScores.end(),
                            [](const ClassScore& a, const ClassScore& b) { return a.score > b.score; });
                    }
                }

                if (rowClassScores.empty() || rowClassScores.front().score <= confThreshold) {
                    continue;
                }

                cv::Rect box = ScaledXYWHBox(
                    data[candidateIndex],
                    data[candidates + candidateIndex],
                    data[2 * candidates + candidateIndex],
                    data[3 * candidates + candidateIndex],
                    frameDims, inpWidth, inpHeight);
                if (box.width <= 0 || box.height <= 0) {
                    continue;
                }

                classIds.push_back(rowClassScores.front().classId);
                confidences.push_back(rowClassScores.front().score);
                boxes.push_back(box);
                detectionClassScores.push_back(rowClassScores);
                if (prototype) {
                    std::vector<float> coefficients;
                    coefficients.reserve(maskCoefficientCount);
                    for (int coefficientIndex = 0; coefficientIndex < maskCoefficientCount; ++coefficientIndex) {
                        coefficients.push_back(data[(4 + classCount + coefficientIndex) * candidates + candidateIndex]);
                    }
                    detectionMasks.push_back(BuildMaskFromPrototype(*prototype, coefficients, box, frameDims));
                } else {
                    detectionMasks.push_back({});
                }
            }
            continue;
        }

        // YOLOv5-style ONNX output is usually [1, num_boxes, num_classes + 5].
        if (det.dims == 3) {
            det = det.reshape(1, det.size[1]);
        }
        if (det.dims != 2 || det.cols < 6) {
            continue;
        }

        const float xFactor = static_cast<float>(frameDims.width) / static_cast<float>(inpWidth);
        const float yFactor = static_cast<float>(frameDims.height) / static_cast<float>(inpHeight);

        float* data = reinterpret_cast<float*>(det.data);
        for (int j = 0; j < det.rows; ++j, data += det.cols) {
            std::vector<ClassScore> rowClassScores;
            rowClassScores.reserve(maxClassCandidates);
            int classScoresEnd = det.cols;
            if (!classNames.empty()) {
                classScoresEnd = std::min(det.cols, 5 + static_cast<int>(classNames.size()));
            }
            for (int classIndex = 5; classIndex < classScoresEnd; ++classIndex) {
                const float classConfidence = data[classIndex] * data[4];
                if (rowClassScores.size() < static_cast<size_t>(maxClassCandidates)) {
                    rowClassScores.emplace_back(classIndex - 5, classConfidence);
                    std::sort(rowClassScores.begin(), rowClassScores.end(),
                        [](const ClassScore& a, const ClassScore& b) { return a.score > b.score; });
                } else if (classConfidence > rowClassScores.back().score) {
                    rowClassScores.back() = ClassScore(classIndex - 5, classConfidence);
                    std::sort(rowClassScores.begin(), rowClassScores.end(),
                        [](const ClassScore& a, const ClassScore& b) { return a.score > b.score; });
                }
            }
            if (rowClassScores.empty()) {
                continue;
            }

            float confidence = rowClassScores.front().score;

            if (confidence > confThreshold) {
                int centerX = 0;
                int centerY = 0;
                int width = 0;
                int height = 0;

                if (data[0] > 1.0f || data[1] > 1.0f || data[2] > 1.0f || data[3] > 1.0f) {
                    centerX = static_cast<int>(data[0] * xFactor);
                    centerY = static_cast<int>(data[1] * yFactor);
                    width = static_cast<int>(data[2] * xFactor);
                    height = static_cast<int>(data[3] * yFactor);
                } else {
                    centerX = static_cast<int>(data[0] * frameDims.width);
                    centerY = static_cast<int>(data[1] * frameDims.height);
                    width = static_cast<int>(data[2] * frameDims.width);
                    height = static_cast<int>(data[3] * frameDims.height);
                }

                int left = centerX - width / 2;
                int top = centerY - height / 2;

                classIds.push_back(rowClassScores.front().classId);
                confidences.push_back(confidence);
                boxes.push_back(cv::Rect(left, top, width, height));
                detectionClassScores.push_back(rowClassScores);
                detectionMasks.push_back({});
            }
        }
    }

    // Perform non maximum suppression to eliminate redundant overlapping boxes with
    // lower confidences
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

    // Pass boxes to SORT algorithm
    std::vector<cv::Rect> sortBoxes;
    std::vector<float> sortConfidences;
    std::vector<int> sortClassIds;
    std::vector<std::vector<ClassScore>> sortClassScores;
    std::vector<CVObjectMaskData> sortMasks;
    for(auto index : indices) {
        sortBoxes.push_back(boxes[index]);
        sortConfidences.push_back(confidences[index]);
        sortClassIds.push_back(classIds[index]);
        sortClassScores.push_back(detectionClassScores[index]);
        sortMasks.push_back(index < static_cast<int>(detectionMasks.size()) ? detectionMasks[index] : CVObjectMaskData());
    }
    sort.update(sortBoxes, frameId, sqrt(pow(frameDims.width,2) + pow(frameDims.height, 2)), sortConfidences, sortClassIds, sortClassScores);

    // Clear data vectors
    boxes.clear(); confidences.clear(); classIds.clear(); objectIds.clear();
    std::vector<CVObjectMaskData> masks;
    // Get SORT predicted boxes
    for(auto TBox : sort.frameTrackingResult){
        if(TBox.frame == frameId){
            boxes.push_back(TBox.box);
            confidences.push_back(TBox.confidence);
            classIds.push_back(TBox.classId);
            objectIds.push_back(TBox.id);
            CVObjectMaskData mask;
            double bestIoU = 0.0;
            for (size_t maskIndex = 0; maskIndex < sortMasks.size(); ++maskIndex) {
                if (!sortMasks[maskIndex].HasData() || sortClassIds[maskIndex] != TBox.classId)
                    continue;
                double score = SortTracker::GetIOU(cv::Rect_<float>(sortBoxes[maskIndex]), TBox.box);
                if (score > bestIoU) {
                    bestIoU = score;
                    mask = sortMasks[maskIndex];
                }
            }
            if (mask.HasData()) {
                recentObjectMasks[TBox.id] = CVTrackedMaskData{frameId, mask, TBox.box};
            } else {
                const auto recentMask = recentObjectMasks.find(TBox.id);
                if (recentMask != recentObjectMasks.end() &&
                    frameId > recentMask->second.frameId &&
                    frameId - recentMask->second.frameId <= 5) {
                    mask = TransformMaskToBox(
                        recentMask->second.mask,
                        recentMask->second.box,
                        TBox.box,
                        frameDims);
                    if (mask.HasData()) {
                        recentObjectMasks[TBox.id] = CVTrackedMaskData{frameId, mask, TBox.box};
                    }
                }
            }
            masks.push_back(mask);
        }
    }

    // Remove boxes based on controids distance
    for(uint i = 0; i<boxes.size(); i++){
        for(uint j = i+1; j<boxes.size(); j++){
            int xc_1 = boxes[i].x + (int)(boxes[i].width/2), yc_1 = boxes[i].y + (int)(boxes[i].height/2);
            int xc_2 = boxes[j].x + (int)(boxes[j].width/2), yc_2 = boxes[j].y + (int)(boxes[j].height/2);

            if(fabs(xc_1 - xc_2) < 10 && fabs(yc_1 - yc_2) < 10){
                if(classIds[i] == classIds[j]){
                    if(confidences[i] >= confidences[j]){
                        boxes.erase(boxes.begin() + j);
                        classIds.erase(classIds.begin() + j);
                        confidences.erase(confidences.begin() + j);
                        objectIds.erase(objectIds.begin() + j);
                        masks.erase(masks.begin() + j);
                        break;
                    }
                    else{
                        boxes.erase(boxes.begin() + i);
                        classIds.erase(classIds.begin() + i);
                        confidences.erase(confidences.begin() + i);
                        objectIds.erase(objectIds.begin() + i);
                        masks.erase(masks.begin() + i);
                        i = 0;
                        break;
                    }
                }
            }
        }
    }

    // Remove boxes based in IOU score
    for(uint i = 0; i<boxes.size(); i++){
        for(uint j = i+1; j<boxes.size(); j++){

            if( iou(boxes[i], boxes[j])){
                if(classIds[i] == classIds[j]){
                    if(confidences[i] >= confidences[j]){
                        boxes.erase(boxes.begin() + j);
                        classIds.erase(classIds.begin() + j);
                        confidences.erase(confidences.begin() + j);
                        objectIds.erase(objectIds.begin() + j);
                        masks.erase(masks.begin() + j);
                        break;
                    }
                    else{
                        boxes.erase(boxes.begin() + i);
                        classIds.erase(classIds.begin() + i);
                        confidences.erase(confidences.begin() + i);
                        objectIds.erase(objectIds.begin() + i);
                        masks.erase(masks.begin() + i);
                        i = 0;
                        break;
                    }
                }
            }
        }
    }

    // Normalize boxes coordinates
    std::vector<cv::Rect_<float>> normalized_boxes;
    for(auto box : boxes){
        cv::Rect_<float> normalized_box;
        normalized_box.x = (box.x)/(float)frameDims.width;
        normalized_box.y = (box.y)/(float)frameDims.height;
        normalized_box.width = (box.width)/(float)frameDims.width;
        normalized_box.height = (box.height)/(float)frameDims.height;
        normalized_boxes.push_back(normalized_box);
    }

    detectionsData[frameId] = CVDetectionData(classIds, confidences, normalized_boxes, frameId, objectIds, masks);
}

// Compute IOU between 2 boxes
bool CVObjectDetection::iou(cv::Rect pred_box, cv::Rect sort_box){
    // Determine the (x, y)-coordinates of the intersection rectangle
	int xA = std::max(pred_box.x, sort_box.x);
	int yA = std::max(pred_box.y, sort_box.y);
	int xB = std::min(pred_box.x + pred_box.width, sort_box.x + sort_box.width);
	int yB = std::min(pred_box.y + pred_box.height, sort_box.y + sort_box.height);

	// Compute the area of intersection rectangle
	int interArea = std::max(0, xB - xA + 1) * std::max(0, yB - yA + 1);

	// Compute the area of both the prediction and ground-truth rectangles
	int boxAArea = (pred_box.width + 1) * (pred_box.height + 1);
	int boxBArea = (sort_box.width + 1) * (sort_box.height + 1);

	// Compute the intersection over union by taking the intersection
	float iou = interArea / (float)(boxAArea + boxBArea - interArea);

    // If IOU is above this value the boxes are very close (probably a variation of the same bounding box)
    if(iou > 0.5)
        return true;
    return false;
}

// Get the names of the output layers
std::vector<cv::String> CVObjectDetection::getOutputsNames(const cv::dnn::Net& net)
{
    //Get the indices of the output layers, i.e. the layers with unconnected outputs
    std::vector<int> outLayers = net.getUnconnectedOutLayers();

    //get the names of all the layers in the network
    std::vector<cv::String> layersNames = net.getLayerNames();

    // Get the names of the output layers in names
    std::vector<cv::String> names;
    names.resize(outLayers.size());
    for (size_t i = 0; i < outLayers.size(); ++i)
        names[i] = layersNames[outLayers[i] - 1];
    return names;
}

CVDetectionData CVObjectDetection::GetDetectionData(size_t frameId){
    // Check if the stabilizer info for the requested frame exists
    if ( detectionsData.find(frameId) == detectionsData.end() ) {

        return CVDetectionData();
    } else {

        return detectionsData[frameId];
    }
}

void CVObjectDetection::NormalizeTrackedClasses()
{
    struct ClassEvidence {
        float confidenceSum = 0.0f;
        size_t count = 0;
    };

    std::map<int, std::map<int, ClassEvidence>> objectClassEvidence;
    for (const auto& frameData : detectionsData) {
        const CVDetectionData& detections = frameData.second;
        const size_t detectionCount = std::min(detections.objectIds.size(), detections.classIds.size());
        for (size_t i = 0; i < detectionCount; ++i) {
            const float confidence = i < detections.confidences.size() ? detections.confidences[i] : 1.0f;
            ClassEvidence& evidence = objectClassEvidence[detections.objectIds[i]][detections.classIds[i]];
            evidence.confidenceSum += confidence;
            ++evidence.count;
        }
    }

    std::map<int, int> dominantClassByObject;
    for (const auto& objectEvidence : objectClassEvidence) {
        const int objectId = objectEvidence.first;
        int bestClassId = -1;
        ClassEvidence bestEvidence;
        for (const auto& classEvidence : objectEvidence.second) {
            const int classId = classEvidence.first;
            const ClassEvidence& evidence = classEvidence.second;
            if (bestClassId < 0 ||
                evidence.confidenceSum > bestEvidence.confidenceSum ||
                (evidence.confidenceSum == bestEvidence.confidenceSum && evidence.count > bestEvidence.count)) {
                bestClassId = classId;
                bestEvidence = evidence;
            }
        }
        if (bestClassId >= 0) {
            dominantClassByObject[objectId] = bestClassId;
        }
    }

    for (auto& frameData : detectionsData) {
        CVDetectionData& detections = frameData.second;
        const size_t detectionCount = std::min(detections.objectIds.size(), detections.classIds.size());
        for (size_t i = 0; i < detectionCount; ++i) {
            const auto dominantClass = dominantClassByObject.find(detections.objectIds[i]);
            if (dominantClass != dominantClassByObject.end()) {
                detections.classIds[i] = dominantClass->second;
            }
        }
    }
}

bool CVObjectDetection::SaveObjDetectedData(){
    if(protobuf_data_path.empty()) {
        cerr << "Missing path to object detection protobuf data file." << endl;
        return false;
    }

    NormalizeTrackedClasses();

    // Create tracker message
    pb_objdetect::ObjDetect objMessage;

    //Save class names in protobuf message
    for(int i = 0; i<classNames.size(); i++){
        std::string* className = objMessage.add_classnames();
        className->assign(classNames.at(i));
    }

    // Iterate over all frames data and save in protobuf message
    for(std::map<size_t,CVDetectionData>::iterator it=detectionsData.begin(); it!=detectionsData.end(); ++it){
        CVDetectionData dData = it->second;
        AddFrameDataToProto(objMessage.add_frame(), dData);
    }

    // Add timestamp
    *objMessage.mutable_last_updated() = TimeUtil::SecondsToTimestamp(time(NULL));

    {
        // Write the new message to disk.
        std::fstream output(protobuf_data_path, ios::out | ios::trunc | ios::binary);
        if (!objMessage.SerializeToOstream(&output)) {
        cerr << "Failed to write protobuf message." << endl;
        return false;
        }
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;

}

// Add frame object detection into protobuf message.
void CVObjectDetection::AddFrameDataToProto(pb_objdetect::Frame* pbFrameData, CVDetectionData& dData) {

    // Save frame number and rotation
    pbFrameData->set_id(dData.frameId);

    for(size_t i = 0; i < dData.boxes.size(); i++){
        pb_objdetect::Frame_Box* box = pbFrameData->add_bounding_box();

        // Save bounding box data
        box->set_x(dData.boxes.at(i).x);
        box->set_y(dData.boxes.at(i).y);
        box->set_w(dData.boxes.at(i).width);
        box->set_h(dData.boxes.at(i).height);
        box->set_classid(dData.classIds.at(i));
        box->set_confidence(dData.confidences.at(i));
        box->set_objectid(dData.objectIds.at(i));

        if (i < dData.masks.size() && dData.masks.at(i).HasData()) {
            pb_objdetect::Frame_Box_Mask* mask = box->mutable_mask();
            mask->set_width(dData.masks.at(i).width);
            mask->set_height(dData.masks.at(i).height);
            for (uint32_t count : dData.masks.at(i).rle) {
                mask->add_rle(count);
            }
        }
    }
}

// Load JSON string into this object
void CVObjectDetection::SetJson(const std::string value) {
	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match

		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		// throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
        std::cout<<"JSON is invalid (missing keys or invalid data types)"<<std::endl;
	}
}

// Load Json::Value into this object
void CVObjectDetection::SetJsonValue(const Json::Value root) {

	// Set data from Json (if key is found)
	if (!root["protobuf_data_path"].isNull()){
		protobuf_data_path = (root["protobuf_data_path"].asString());
	}

    if (!root["processing-device"].isNull()){
		processingDevice = (root["processing-device"].asString());
	}
    if (!root["processing_device"].isNull()){
		processingDevice = (root["processing_device"].asString());
	}
    if (!root["class-names"].isNull()){
		classesFile = (root["class-names"].asString());
	}
    if (!root["classes_file"].isNull()){
		classesFile = (root["classes_file"].asString());
	}
    if (!root["model"].isNull()){
        modelPath = (root["model"].asString());
    }
    if (!root["model_path"].isNull()){
        modelPath = (root["model_path"].asString());
    }
    if (!root["input-width"].isNull()){
        inpWidth = root["input-width"].asInt();
    }
    if (!root["input_width"].isNull()){
        inpWidth = root["input_width"].asInt();
    }
    if (!root["input-height"].isNull()){
        inpHeight = root["input-height"].asInt();
    }
    if (!root["input_height"].isNull()){
        inpHeight = root["input_height"].asInt();
    }
    if (!root["confidence-threshold"].isNull()){
        confThreshold = root["confidence-threshold"].asFloat();
    }
    if (!root["confidence_threshold"].isNull()){
        confThreshold = root["confidence_threshold"].asFloat();
    }
    if (!root["nms-threshold"].isNull()){
        nmsThreshold = root["nms-threshold"].asFloat();
    }
    if (!root["nms_threshold"].isNull()){
        nmsThreshold = root["nms_threshold"].asFloat();
    }
    if (!root["generate-masks"].isNull()){
        generateMasks = root["generate-masks"].asBool();
    }
    if (!root["generate_masks"].isNull()){
        generateMasks = root["generate_masks"].asBool();
    }
}

/*
||||||||||||||||||||||||||||||||||||||||||||||||||
                ONLY FOR MAKE TEST
||||||||||||||||||||||||||||||||||||||||||||||||||
*/

// Load protobuf data file
bool CVObjectDetection::_LoadObjDetectdData(){
    if(protobuf_data_path.empty()) {
        cerr << "Missing path to object detection protobuf data file." << endl;
        return false;
    }

    // Create tracker message
    pb_objdetect::ObjDetect objMessage;

    {
        // Read the existing tracker message.
        fstream input(protobuf_data_path, ios::in | ios::binary);
        if (!objMessage.ParseFromIstream(&input)) {
            cerr << "Failed to parse protobuf message." << endl;
            return false;
        }
    }

    // Make sure classNames and detectionsData are empty
    classNames.clear(); detectionsData.clear();

    // Get all classes names and assign a color to them
    for(int i = 0; i < objMessage.classnames_size(); i++){
        classNames.push_back(objMessage.classnames(i));
    }

    // Iterate over all frames of the saved message
    for (size_t i = 0; i < objMessage.frame_size(); i++) {
        // Create protobuf message reader
        const pb_objdetect::Frame& pbFrameData = objMessage.frame(i);

        // Get frame Id
        size_t id = pbFrameData.id();

        // Load bounding box data
        const google::protobuf::RepeatedPtrField<pb_objdetect::Frame_Box > &pBox = pbFrameData.bounding_box();

        // Construct data vectors related to detections in the current frame
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect_<float>> boxes;
        std::vector<int> objectIds;
        std::vector<CVObjectMaskData> masks;

        for(int i = 0; i < pbFrameData.bounding_box_size(); i++){
            // Get bounding box coordinates
            float x = pBox.Get(i).x(); float y = pBox.Get(i).y();
            float w = pBox.Get(i).w(); float h = pBox.Get(i).h();
            // Create OpenCV rectangle with the bouding box info
            cv::Rect_<float> box(x, y, w, h);

            // Get class Id (which will be assign to a class name) and prediction confidence
            int classId = pBox.Get(i).classid(); float confidence = pBox.Get(i).confidence();
            // Get object Id
            int objectId = pBox.Get(i).objectid();

            // Push back data into vectors
            boxes.push_back(box); classIds.push_back(classId); confidences.push_back(confidence);
            objectIds.push_back(objectId);
            CVObjectMaskData mask;
            if (pBox.Get(i).has_mask()) {
                mask.width = pBox.Get(i).mask().width();
                mask.height = pBox.Get(i).mask().height();
                for (int rleIndex = 0; rleIndex < pBox.Get(i).mask().rle_size(); ++rleIndex) {
                    mask.rle.push_back(pBox.Get(i).mask().rle(rleIndex));
                }
            }
            masks.push_back(mask);
        }

        // Assign data to object detector map
        detectionsData[id] = CVDetectionData(classIds, confidences, boxes, id, objectIds, masks);
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}
