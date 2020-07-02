#define int64 opencv_broken_int
#define uint64 opencv_broken_uint
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#undef uint64
#undef int64
#include <cmath>
#include "Clip.h"

using namespace std;

// struct TransformParam
// {
//     TransformParam() {}
//     TransformParam(double _dx, double _dy, double _da) {
//         dx = _dx;
//         dy = _dy;
//         da = _da;
//     }

//     double dx;
//     double dy;
//     double da; // angle
// };

struct CamTrajectory
{
    CamTrajectory() {}
    CamTrajectory(double _x, double _y, double _a) {
        x = _x;
        y = _y;
        a = _a;
    }

    double x;
    double y;
    double a; // angle
};

class CVStabilization {       
    private:
    cv::Mat last_T;
    cv::Mat cur, cur_grey;
    cv::Mat prev, prev_grey;

    public:
    const int SMOOTHING_RADIUS = 30; // In frames. The larger the more stable the video, but less reactive to sudden panning
    const int HORIZONTAL_BORDER_CROP = 20; // In pixels. Crops the border to reduce the black borders from stabilisation being too noticeable.
    std::vector <TransformParam> prev_to_cur_transform; // previous to current

    CVStabilization();

    void ProcessVideo(openshot::Clip &video);

    // Track current frame features and find the relative transformation
    void TrackFrameFeatures(cv::Mat frame, int frameNum);
    
    std::vector<CamTrajectory> ComputeFramesTrajectory();
    std::vector<CamTrajectory> SmoothTrajectory(std::vector <CamTrajectory> &trajectory);

    // Generate new transformations parameters for each frame to follow the smoothed trajectory
    std::vector<TransformParam> GenNewCamPosition(std::vector <CamTrajectory> &smoothed_trajectory);
    
    // Send smoothed camera transformation to be applyed on clip
    void ApplyNewTrajectoryToClip(openshot::Clip &video, std::vector <TransformParam> &new_prev_to_cur_transform);

};