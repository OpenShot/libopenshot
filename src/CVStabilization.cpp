#include "../include/CVStabilization.h"


CVStabilization::CVStabilization():smoothingWindow(30) {}

CVStabilization::CVStabilization(int _smoothingWindow): smoothingWindow(_smoothingWindow){}

void CVStabilization::ProcessClip(openshot::Clip &video){
    // Get total number of frames
    int videoLenght = video.Reader()->info.video_length;

    // Extract and track opticalflow features for each frame
    for (long int frame_number = 0; frame_number <= videoLenght; frame_number++)
    {
        std::shared_ptr<openshot::Frame> f = video.GetFrame(frame_number);
        
        // Grab Mat image
        cv::Mat cvimage = f->GetImageCV();
        cv::cvtColor(cvimage, cvimage, cv::COLOR_RGB2GRAY);
        TrackFrameFeatures(cvimage, frame_number);
    }

    vector <CamTrajectory> trajectory = ComputeFramesTrajectory();

    trajectoryData = SmoothTrajectory(trajectory);

    transformationData = GenNewCamPosition(trajectoryData);
}

// Track current frame features and find the relative transformation
void CVStabilization::TrackFrameFeatures(cv::Mat frame, int frameNum){
    if(prev_grey.empty()){
        prev_grey = frame;
        return;
    }

    // OpticalFlow features vector
    vector <cv::Point2f> prev_corner, cur_corner;
    vector <cv::Point2f> prev_corner2, cur_corner2;
    vector <uchar> status;
    vector <float> err;
    
    // Extract new image teatures
    cv::goodFeaturesToTrack(prev_grey, prev_corner, 200, 0.01, 30);
    // Track features
    cv::calcOpticalFlowPyrLK(prev_grey, frame, prev_corner, cur_corner, status, err);

    // Remove untracked features
    for(size_t i=0; i < status.size(); i++) {
        if(status[i]) {
            prev_corner2.push_back(prev_corner[i]);
            cur_corner2.push_back(cur_corner[i]);
        }
    }
    // translation + rotation only
    cv::Mat T = estimateRigidTransform(prev_corner2, cur_corner2, false); // false = rigid transform, no scaling/shearing

    // If no transform is found. We'll just use the last known good transform.
    if(T.data == NULL) {
        last_T.copyTo(T);
    }

    T.copyTo(last_T);
    // decompose T
    double dx = T.at<double>(0,2);
    double dy = T.at<double>(1,2);
    double da = atan2(T.at<double>(1,0), T.at<double>(0,0));

    prev_to_cur_transform.push_back(TransformParam(dx, dy, da));

    // out_transform << frameNum << " " << dx << " " << dy << " " << da << endl;
    cur.copyTo(prev);
    frame.copyTo(prev_grey);

    cout << "Frame: " << frameNum << " - good optical flow: " << prev_corner2.size() << endl;
}

vector <CamTrajectory> CVStabilization::ComputeFramesTrajectory(){

    // Accumulated frame to frame transform
    double a = 0;
    double x = 0;
    double y = 0;

    vector <CamTrajectory> trajectory; // trajectory at all frames
    
    // Compute global camera trajectory. First frame is the origin 
    for(size_t i=0; i < prev_to_cur_transform.size(); i++) {
        x += prev_to_cur_transform[i].dx;
        y += prev_to_cur_transform[i].dy;
        a += prev_to_cur_transform[i].da;

        trajectory.push_back(CamTrajectory(x,y,a));

        // out_trajectory << (i+1) << " " << x << " " << y << " " << a << endl;
    }

    return trajectory;
}

vector <CamTrajectory> CVStabilization::SmoothTrajectory(vector <CamTrajectory> &trajectory){

    vector <CamTrajectory> smoothed_trajectory; // trajectory at all frames

    for(size_t i=0; i < trajectory.size(); i++) {
        double sum_x = 0;
        double sum_y = 0;
        double sum_a = 0;
        int count = 0;

        for(int j=-smoothingWindow; j <= smoothingWindow; j++) {
            if(i+j >= 0 && i+j < trajectory.size()) {
                sum_x += trajectory[i+j].x;
                sum_y += trajectory[i+j].y;
                sum_a += trajectory[i+j].a;

                count++;
            }
        }

        double avg_a = sum_a / count;
        double avg_x = sum_x / count;
        double avg_y = sum_y / count;

        smoothed_trajectory.push_back(CamTrajectory(avg_x, avg_y, avg_a));

        // out_smoothed_trajectory << (i+1) << " " << avg_x << " " << avg_y << " " << avg_a << endl;
    }
    return smoothed_trajectory;
}

// Generate new transformations parameters for each frame to follow the smoothed trajectory
vector <TransformParam> CVStabilization::GenNewCamPosition(vector <CamTrajectory> & smoothed_trajectory){
    vector <TransformParam> new_prev_to_cur_transform;

    // Accumulated frame to frame transform
    double a = 0;
    double x = 0;
    double y = 0;

    for(size_t i=0; i < prev_to_cur_transform.size(); i++) {
        x += prev_to_cur_transform[i].dx;
        y += prev_to_cur_transform[i].dy;
        a += prev_to_cur_transform[i].da;

        // target - current
        double diff_x = smoothed_trajectory[i].x - x;
        double diff_y = smoothed_trajectory[i].y - y;
        double diff_a = smoothed_trajectory[i].a - a;

        double dx = prev_to_cur_transform[i].dx + diff_x;
        double dy = prev_to_cur_transform[i].dy + diff_y;
        double da = prev_to_cur_transform[i].da + diff_a;

        new_prev_to_cur_transform.push_back(TransformParam(dx, dy, da));

        // out_new_transform << (i+1) << " " << dx << " " << dy << " " << da << endl;
    }
    return new_prev_to_cur_transform;
}

// Save protobuf file
bool CVStabilization::SaveStabilizedData(std::string outputFilePath){
    // Create tracker message
    libopenshotstabilize::Stabilization stabilizationMessage;

    // Add all frames data
    std::vector<CamTrajectory>::iterator trajData = trajectoryData.begin();
    std::vector<TransformParam>::iterator transData = transformationData.begin();

    for(long int frame_count = 0; trajData != trajectoryData.end(); ++trajData, ++transData){
        AddFrameDataToProto(stabilizationMessage.add_frame(), *trajData, *transData, frame_count);
    }
    // Add timestamp
    *stabilizationMessage.mutable_last_updated() = TimeUtil::SecondsToTimestamp(time(NULL));

    // Write the new message to disk.
    std::fstream output(outputFilePath, ios::out | ios::trunc | ios::binary);
    if (!stabilizationMessage.SerializeToOstream(&output)) {
        cerr << "Failed to write protobuf message." << endl;
        return false;
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}

void CVStabilization::AddFrameDataToProto(libopenshotstabilize::Frame* pbFrameData, CamTrajectory& trajData, TransformParam& transData, long int frame_number){

    // Save frame number 
    pbFrameData->set_id(frame_number);

    // Save camera trajectory data
    pbFrameData->set_a(trajData.a);
    pbFrameData->set_x(trajData.x);
    pbFrameData->set_y(trajData.y);

    // Save transformation data
    pbFrameData->set_da(transData.da);
    pbFrameData->set_dx(transData.dx);
    pbFrameData->set_dy(transData.dy);
}

// Load protobuf file
bool CVStabilization::LoadStabilizedData(std::string inputFilePath){
    libopenshotstabilize::Stabilization stabilizationMessage;

    // Read the existing tracker message.
    fstream input(inputFilePath, ios::in | ios::binary);
    if (!stabilizationMessage.ParseFromIstream(&input)) {
        cerr << "Failed to parse protobuf message." << endl;
        return false;
    }

    // Make sure the data vectors are empty
    transformationData.clear();
    trajectoryData.clear();

    // Iterate over all frames of the saved message
    for (int i = 0; i < stabilizationMessage.frame_size(); i++) {
        const libopenshotstabilize::Frame& pbFrameData = stabilizationMessage.frame(i);

        int id = pbFrameData.id();

        float x = pbFrameData.x();
        float y = pbFrameData.y();
        float a = pbFrameData.a();

        trajectoryData.push_back(CamTrajectory(x,y,a));

        float dx = pbFrameData.dx();
        float dy = pbFrameData.dy();
        float da = pbFrameData.da();

        transformationData.push_back(TransformParam(dx,dy,da));
    }

    if (stabilizationMessage.has_last_updated()) {
        cout << "  Loaded Data. Saved Time Stamp: " << TimeUtil::ToString(stabilizationMessage.last_updated()) << endl;
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}