
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/core.hpp>

using namespace cv;


class CVTracker {       
  public:             
    std::string trackerType;
    Ptr<Tracker> tracker;  
    Rect2d bbox;    

    CVTracker();
    Ptr<Tracker> select_tracker(std::string trackerType);
    bool initTracker(Rect2d bbox, Mat &frame);
    bool trackFrame(Mat &frame);

};
