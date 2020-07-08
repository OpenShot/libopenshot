#include "../include/ClipProcessingJobs.h"


// get the clip to add a preprocessing job

// run the preprocessing job on the clip

// create a new effect with the processed result

// modify the clip to include the correspondent processed effect


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


    // 

}


void ClipProcessingJobs::stabilizeVideo(Clip& video){
	// create CVStabilization object
	CVStabilization stabilizer; 

    // Get total number of frames
    int videoLenght = video.Reader()->info.video_length;

    // Extract and track opticalflow features for each frame
    for (long int frame_number = 0; frame_number <= videoLenght; frame_number++)
    {
        std::shared_ptr<openshot::Frame> f = video.GetFrame(frame_number);
        
        // Grab Mat image
        cv::Mat cvimage = f->GetImageCV();
        cv::cvtColor(cvimage, cvimage, cv::COLOR_RGB2GRAY);
        stabilizer.TrackFrameFeatures(cvimage, frame_number);
    }

    vector <CamTrajectory> trajectory = stabilizer.ComputeFramesTrajectory();

    vector <CamTrajectory> smoothed_trajectory = stabilizer.SmoothTrajectory(trajectory);

	// Get the smoothed trajectory
    std::vector <TransformParam> new_prev_to_cur_transform = stabilizer.GenNewCamPosition(smoothed_trajectory);

	// Will apply the smoothed transformation warp when retrieving a frame
	video.hasStabilization = true;
    video.new_prev_to_cur_transform = new_prev_to_cur_transform;

}