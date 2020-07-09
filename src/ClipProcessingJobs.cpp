#include "../include/ClipProcessingJobs.h"

// Constructor responsible to choose processing type and apply to clip
ClipProcessingJobs::ClipProcessingJobs(std::string processingType, Clip& videoClip){

    if(processingType == "Stabilize"){
        stabilizeVideo(videoClip);
    }
    if(processingType == "Track")
        trackVideo(videoClip);

}

// Apply object tracking to clip 
void ClipProcessingJobs::trackVideo(Clip& videoClip){

    // Opencv display window
    cv::namedWindow("Display Image", cv::WINDOW_NORMAL );

    // Create CVTracker object
    CVTracker tracker;
    // Start tracking
    tracker.trackClip(videoClip);
    
    // Save tracking data
    tracker.SaveTrackedData("kcf_tracker.data");

    // Create new Tracker Effect
    EffectBase* trackerEffect = new Tracker("kcf_tracker.data");
    // Apply Tracker Effect to clip
    videoClip.AddEffect(trackerEffect);
}

// Apply stabilization to clip
void ClipProcessingJobs::stabilizeVideo(Clip& videoClip){
	// create CVStabilization object
	CVStabilization stabilizer; 
    // Start stabilization process
    stabilizer.ProcessClip(videoClip);

    // Save stabilization data
    stabilizer.SaveStabilizedData("stabilization.data");

    // Create new Stabilizer Effect
    EffectBase* stabilizeEffect = new Stabilizer("stabilization.data");
    // Apply Stabilizer Effect to clip
    videoClip.AddEffect(stabilizeEffect);


}