#include "../include/CVTracker.h"

CVTracker::CVTracker(){

    // Create KCF tracker
    trackerType = trackerTypes[2];
    tracker = select_tracker(trackerType);
}

cv::Ptr<cv::Tracker> CVTracker::select_tracker(std::string trackerType){
    cv::Ptr<cv::Tracker> t;
    #if (CV_MINOR_VERSION < 3)
    {
        t = cv::Tracker::create(trackerType);
    }
    #else
    {
        if (trackerType == "BOOSTING")
            t = cv::TrackerBoosting::create();
        if (trackerType == "MIL")
            t = cv::TrackerMIL::create();
        if (trackerType == "KCF")
            t = cv::TrackerKCF::create();
        if (trackerType == "TLD")
            t = cv::TrackerTLD::create();
        if (trackerType == "MEDIANFLOW")
            t = cv::TrackerMedianFlow::create();
        if (trackerType == "GOTURN")
            t = cv::TrackerGOTURN::create();
        if (trackerType == "MOSSE")
            t = cv::TrackerMOSSE::create();
        if (trackerType == "CSRT")
            t = cv::TrackerCSRT::create();
    }
    #endif

    return t;
}

void CVTracker::trackClip(openshot::Clip& video){
    // Opencv display window
    cv::namedWindow("Display Image", cv::WINDOW_NORMAL );
    // Create Tracker

    bool trackerInit = false;
    int videoLenght = video.Reader()->info.video_length;
    for (long int frame = 0; frame < videoLenght; frame++)
    {
        std::cout<<"frame: "<<frame<<"\n";
        int frame_number = frame;
        std::shared_ptr<openshot::Frame> f = video.GetFrame(frame_number);
        
        // Grab Mat image
        cv::Mat cvimage = f->GetImageCV();

        if(!trackerInit){
            cv::Rect2d bbox = cv::selectROI("Display Image", cvimage);

            initTracker(bbox, cvimage, frame_number);
            cv::rectangle(cvimage, bbox, cv::Scalar( 255, 0, 0 ), 2, 1 );

            trackerInit = true;
        }
        else{
            trackerInit = trackFrame(cvimage, frame_number);
            
            // Draw box on image
            FrameData fd = GetTrackedData(frame_number);

            cv::Rect2d box(fd.x1, fd.y1, fd.x2-fd.x1, fd.y2-fd.y1);
            cv::rectangle(cvimage, box, cv::Scalar( 255, 0, 0 ), 2, 1 );
        }
        
        cv::imshow("Display Image", cvimage);
        // Press  ESC on keyboard to exit
        char c=(char)cv::waitKey(1);
        if(c==27)
            break;

    }

    
}


bool CVTracker::initTracker(cv::Rect2d initial_bbox, cv::Mat &frame, int frameId){

    bbox = initial_bbox; 
    // rectangle(frame, bbox, Scalar( 255, 0, 0 ), 2, 1 ); 
    // Create new tracker object
    tracker = select_tracker(trackerType);

    tracker->init(frame, bbox);

    // Add new frame data
    trackedDataById[frameId] = FrameData(frameId, 0, bbox.x, bbox.y, bbox.x+bbox.width, bbox.y+bbox.height);

    return true;
}

bool CVTracker::trackFrame(cv::Mat &frame, int frameId){
    // Update the tracking result
    bool ok = tracker->update(frame, bbox);

    if (ok)
    {
        // Add new frame data
        trackedDataById[frameId] = FrameData(frameId, 0, bbox.x, bbox.y, bbox.x+bbox.width, bbox.y+bbox.height);
    }
    else
    {
        // Add new frame data
        trackedDataById[frameId] = FrameData(frameId);
    }

    return ok;
}

bool CVTracker::SaveTrackedData(std::string outputFilePath){
    // Create tracker message
    libopenshottracker::Tracker trackerMessage;

    // Add all frames data
    for(std::map<int,FrameData>::iterator it=trackedDataById.begin(); it!=trackedDataById.end(); ++it){
        
        FrameData fData = it->second;
        libopenshottracker::Frame* pbFrameData;
        AddFrameDataToProto(trackerMessage.add_frame(), fData);
    }

    // Add timestamp
    *trackerMessage.mutable_last_updated() = TimeUtil::SecondsToTimestamp(time(NULL));

    {
        // Write the new message to disk.
        std::fstream output(outputFilePath, ios::out | ios::trunc | ios::binary);
        if (!trackerMessage.SerializeToOstream(&output)) {
        cerr << "Failed to write protobuf message." << endl;
        return false;
        }
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;

}

// Add frame tracked data into protobuf message.
void CVTracker::AddFrameDataToProto(libopenshottracker::Frame* pbFrameData, FrameData& fData) {

  pbFrameData->set_id(fData.frame_id);
  pbFrameData->set_rotation(fData.frame_id);
  pbFrameData->set_rotation(fData.frame_id);

  libopenshottracker::Frame::Box* box = pbFrameData->mutable_bounding_box();
    box->set_x1(fData.x1);
    box->set_y1(fData.y1);
    box->set_x2(fData.x2);
    box->set_y2(fData.y2);

}

bool CVTracker::LoadTrackedData(std::string inputFilePath){

    libopenshottracker::Tracker trackerMessage;

    {
        // Read the existing tracker message.
        fstream input(inputFilePath, ios::in | ios::binary);
        if (!trackerMessage.ParseFromIstream(&input)) {
            cerr << "Failed to parse protobuf message." << endl;
            return false;
        }
    }

    // Make sure the trackedData is empty
    trackedDataById.clear();

    // Iterate over all frames of the saved message
    for (int i = 0; i < trackerMessage.frame_size(); i++) {
        const libopenshottracker::Frame& pbFrameData = trackerMessage.frame(i);

        int id = pbFrameData.id();
        float rotation = pbFrameData.rotation();

        const libopenshottracker::Frame::Box& box = pbFrameData.bounding_box();
        int x1 = box.x1();
        int y1 = box.y1();
        int x2 = box.x2();
        int y2 = box.y2();

        trackedDataById[id] = FrameData(id, rotation, x1, y1, x2, y2);
    }

    if (trackerMessage.has_last_updated()) {
        cout << "  Loaded Data. Saved Time Stamp: " << TimeUtil::ToString(trackerMessage.last_updated()) << endl;
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}

FrameData CVTracker::GetTrackedData(int frameId){

    if ( trackedDataById.find(frameId) == trackedDataById.end() ) {
        
        return FrameData();
    } else {
        
        return trackedDataById[frameId];
    }

}
