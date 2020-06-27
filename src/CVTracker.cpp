#include "../include/CVTracker.h"

using namespace cv;


CVTracker::CVTracker(){
    // List of tracker types in OpenCV 3.4.1
    std::string trackerTypes[8] = {"BOOSTING", "MIL", "KCF", "TLD","MEDIANFLOW", "GOTURN", "MOSSE", "CSRT"};
    // vector <string> trackerTypes(types, std::end(types));

    // Create a tracker
    trackerType = trackerTypes[2];

    tracker = select_tracker(trackerType);

}

Ptr<Tracker> CVTracker::select_tracker(std::string trackerType){
    Ptr<Tracker> t;
    #if (CV_MINOR_VERSION < 3)
    {
        t = Tracker::create(trackerType);
    }
    #else
    {
        if (trackerType == "BOOSTING")
            t = TrackerBoosting::create();
        if (trackerType == "MIL")
            t = TrackerMIL::create();
        if (trackerType == "KCF")
            t = TrackerKCF::create();
        if (trackerType == "TLD")
            t = TrackerTLD::create();
        if (trackerType == "MEDIANFLOW")
            t = TrackerMedianFlow::create();
        if (trackerType == "GOTURN")
            t = TrackerGOTURN::create();
        if (trackerType == "MOSSE")
            t = TrackerMOSSE::create();
        if (trackerType == "CSRT")
            t = TrackerCSRT::create();
    }
    #endif

    return t;
}


bool CVTracker::initTracker(Rect2d initial_bbox, Mat &frame){
    // Rect2d bbox(287, 23, 86, 320); 
    bbox = initial_bbox;

    // Uncomment the line below to select a different bounding box 
    // bbox = selectROI(frame, false); 
    // Display bounding box. 
    rectangle(frame, bbox, Scalar( 255, 0, 0 ), 2, 1 ); 
    
    tracker = select_tracker(trackerType);

    tracker->init(frame, bbox);

    return true;
}

bool CVTracker::trackFrame(Mat &frame){
    // Update the tracking result
    bool ok = tracker->update(frame, bbox);

    if (ok)
    {
        // Tracking success : Draw the tracked object
        rectangle(frame, bbox, Scalar( 255, 0, 0 ), 2, 1 );
    }
    else
    {
        // Tracking failure detected.
        putText(frame, "Tracking failure detected", Point(100,80), FONT_HERSHEY_SIMPLEX, 0.75, Scalar(0,0,255),2);
    }

    return ok;
}
