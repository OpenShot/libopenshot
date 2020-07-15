#include "../include/ClipProcessingJobs.h"

// Constructor responsible to choose processing type and apply to clip
ClipProcessingJobs::ClipProcessingJobs(std::string processingType, Clip& videoClip){

    // if(processingType == "Stabilize"){
    //     std::cout<<"Stabilize";
    //     stabilizeVideo(videoClip);
    // }
    // if(processingType == "Track"){
    //     std::cout<<"Track";
    //     trackVideo(videoClip);
    // }

}

// Apply object tracking to clip 
std::string ClipProcessingJobs::trackVideo(Clip& videoClip){

    // Opencv display window
    cv::namedWindow("Display Image", cv::WINDOW_NORMAL );

    // Create CVTracker object
    CVTracker tracker;
    // Start tracking
    tracker.trackClip(videoClip);
    
    // Save tracking data
    tracker.SaveTrackedData("/media/brenno/Data/projects/openshot/kcf_tracker.data");

    // Return path to protobuf saved data
    return "/media/brenno/Data/projects/openshot/kcf_tracker.data";
}

// Apply stabilization to clip
std::string ClipProcessingJobs::stabilizeVideo(Clip& videoClip){
	// create CVStabilization object
	CVStabilization stabilizer; 
    // Start stabilization process
    stabilizer.ProcessClip(videoClip);

    // Save stabilization data
    stabilizer.SaveStabilizedData("/media/brenno/Data/projects/openshot/stabilization.data");

    // Return path to protobuf saved data
    return "/media/brenno/Data/projects/openshot/stabilization.data";
}


int ClipProcessingJobs::GetProgress(){
    return processingProgress;
}


void ClipProcessingJobs::CancelProcessing(){
    stopProcessing = true;
}
