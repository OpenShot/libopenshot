#include <google/protobuf/util/time_util.h>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/core.hpp>
#include <fstream>

#include "trackerdata.pb.h"

using namespace cv;
using namespace std;
using google::protobuf::util::TimeUtil;

struct FrameData{
  int frame_id = -1;
  float rotation = 0;
  int x1 = -1;
  int y1 = -1;
  int x2 = -1;
  int y2 = -1;

  // constructor
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
    // List of tracker types in OpenCV
    std::string trackerTypes[8] = {"BOOSTING", "MIL", "KCF", "TLD","MEDIANFLOW", "GOTURN", "MOSSE", "CSRT"};

    std::map<int, FrameData> trackedDataById;             
    std::string trackerType;
    Ptr<Tracker> tracker;  
    Rect2d bbox;    

    CVTracker();
    Ptr<Tracker> select_tracker(std::string trackerType);
    bool initTracker(Rect2d bbox, Mat &frame, int frameId);
    bool trackFrame(Mat &frame, int frameId);

    // Save protobuf file
    bool SaveTrackedData(std::string outputFilePath);
    void AddFrameDataToProto(libopenshottracker::Frame* pbFrameData, FrameData& fData);

    // Load protobuf file
    bool LoadTrackedData(std::string inputFilePath);

    // Get tracked data for a given frame
    FrameData GetTrackedData(int frameId);
};
