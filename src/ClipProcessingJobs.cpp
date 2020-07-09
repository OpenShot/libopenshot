#include "../include/ClipProcessingJobs.h"


ClipProcessingJobs::ClipProcessingJobs(std::string processingType, Clip& videoClip){

    if(processingType == "Stabilize"){
        stabilizeVideo(videoClip);
    }
    if(processingType == "Track")
        trackVideo(videoClip);

}

void ClipProcessingJobs::trackVideo(Clip& videoClip){

    // Opencv display window
    cv::namedWindow("Display Image", cv::WINDOW_NORMAL );
    // Create Tracker
    CVTracker tracker;
    tracker.trackClip(videoClip);

    tracker.SaveTrackedData("kcf_tracker.data");

    // Create new Tracker Effect
    EffectBase* trackerEffect = new Tracker("kcf_tracker.data");
    videoClip.AddEffect(trackerEffect);
}

void ClipProcessingJobs::stabilizeVideo(Clip& videoClip){
	// create CVStabilization object
	CVStabilization stabilizer; 
    stabilizer.ProcessClip(videoClip);

    stabilizer.SaveStabilizedData("stabilization.data");

    // Create new Tracker Effect
    EffectBase* stabilizeEffect = new Stabilizer("stabilization.data");
    videoClip.AddEffect(stabilizeEffect);


}