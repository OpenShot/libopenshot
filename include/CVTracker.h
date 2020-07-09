#ifndef OPENSHOT_CVTRACKER_H
#define OPENSHOT_CVTRACKER_H

#include <google/protobuf/util/time_util.h>

#define int64 opencv_broken_int
#define uint64 opencv_broken_uint
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/core.hpp>
#undef uint64
#undef int64

#include <fstream>
#include "Clip.h"
#include "KeyFrame.h"
#include "Frame.h"
#include "trackerdata.pb.h"

using namespace std;
using google::protobuf::util::TimeUtil;

// Tracking info struct
struct FrameData{
  int frame_id = -1;
  float rotation = 0;
  int x1 = -1;
  int y1 = -1;
  int x2 = -1;
  int y2 = -1;

  // Constructors
  FrameData()
  {}

  FrameData( int _frame_id)
  {frame_id = _frame_id;}

  FrameData( int _frame_id , float _rotation, int _x1, int _y1, int _x2, int _y2)
  {
      frame_id = _frame_id;
      rotation = _rotation;
      x1 = _x1;
      y1 = _y1;
      x2 = _x2;
      y2 = _y2;
  }
};

class CVTracker {       
  public:

    std::map<int, FrameData> trackedDataById; // Save tracked data       
    std::string trackerType; // Name of the chosen tracker
    cv::Ptr<cv::Tracker> tracker; // Pointer of the selected tracker 
    cv::Rect2d bbox; // Bounding box coords 

    /// Class constructors
    // Expects a tracker type, if none is passed, set default as KCF
    CVTracker();
    CVTracker(std::string trackerType);
    
    // Set desirable tracker method
    cv::Ptr<cv::Tracker> select_tracker(std::string trackerType);

    // Track object in the hole clip
    void trackClip(openshot::Clip& video);
    // Initialize the tracker
    bool initTracker(cv::Rect2d bbox, cv::Mat &frame, int frameId);
    // Update the object tracker according to frame 
    bool trackFrame(cv::Mat &frame, int frameId);

    /// Protobuf Save and Load methods
    // Save protobuf file
    bool SaveTrackedData(std::string outputFilePath);
    // Add frame tracked data into protobuf message.
    void AddFrameDataToProto(libopenshottracker::Frame* pbFrameData, FrameData& fData);
    // Load protobuf file
    bool LoadTrackedData(std::string inputFilePath);

    // Get tracked data for a given frame
    FrameData GetTrackedData(int frameId);
};



#endif