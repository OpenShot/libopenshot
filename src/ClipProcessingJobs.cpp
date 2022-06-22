/**
 * @file
 * @brief Source file for ClipProcessingJobs class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ClipProcessingJobs.h"

namespace openshot {

// Constructor responsible to choose processing type and apply to clip
ClipProcessingJobs::ClipProcessingJobs(std::string processingType, std::string processInfoJson) :
processingType(processingType), processInfoJson(processInfoJson){
}

void ClipProcessingJobs::processClip(Clip& clip, std::string json){
    processInfoJson = json;

    // Process clip and save processed data
    if(processingType == "Stabilizer"){
        t = std::thread(&ClipProcessingJobs::stabilizeClip, this, std::ref(clip), std::ref(this->processingController));
    }
    if(processingType == "Tracker"){
        t = std::thread(&ClipProcessingJobs::trackClip, this, std::ref(clip), std::ref(this->processingController));
    }
    if(processingType == "ObjectDetection"){
        t = std::thread(&ClipProcessingJobs::detectObjectsClip, this, std::ref(clip), std::ref(this->processingController));
    }
}

// Apply object tracking to clip
void ClipProcessingJobs::trackClip(Clip& clip, ProcessingController& controller){

    // Create CVTracker object
    CVTracker tracker(processInfoJson, controller);
    // Start tracking
    tracker.trackClip(clip);

    // Thread controller. If effect processing is done, save data
    // Else, kill thread
    if(controller.ShouldStop()){
        controller.SetFinished(true);
        return;
    }
    else{
        // Save stabilization data
        tracker.SaveTrackedData();
        // tells to UI that the processing finished
        controller.SetFinished(true);
    }

}

// Apply object detection to clip
void ClipProcessingJobs::detectObjectsClip(Clip& clip, ProcessingController& controller){
	// create CVObjectDetection object
	CVObjectDetection objDetector(processInfoJson, controller);
    // Start object detection process
    objDetector.detectObjectsClip(clip);

    // Thread controller. If effect processing is done, save data
    // Else, kill thread
    if(controller.ShouldStop()){
        controller.SetFinished(true);
        return;
    }
    else{
        // Save object detection data
        objDetector.SaveObjDetectedData();
        // tells to UI that the processing finished
        controller.SetFinished(true);
    }
}

void ClipProcessingJobs::stabilizeClip(Clip& clip, ProcessingController& controller){
    // create CVStabilization object
	CVStabilization stabilizer(processInfoJson, controller);
    // Start stabilization process
    stabilizer.stabilizeClip(clip);

    // Thread controller. If effect processing is done, save data
    // Else, kill thread
    if(controller.ShouldStop()){
        controller.SetFinished(true);
        return;
    }
    else{
        // Save stabilization data
        stabilizer.SaveStabilizedData();
        // tells to UI that the processing finished
        controller.SetFinished(true);
    }
}

// Get processing progress while iterating on the clip
int ClipProcessingJobs::GetProgress(){

    return (int)processingController.GetProgress();
}

// Check if processing finished
bool ClipProcessingJobs::IsDone(){

    if(processingController.GetFinished()){
        t.join();
    }
    return processingController.GetFinished();
}

// stop preprocessing before finishing it
void ClipProcessingJobs::CancelProcessing(){
    processingController.CancelProcessing();
}

// check if there is an error with the config
bool ClipProcessingJobs::GetError(){
    return processingController.GetError();
}

// get the error message
std::string ClipProcessingJobs::GetErrorMessage(){
    return processingController.GetErrorMessage();
}

}  // namespace openshot
