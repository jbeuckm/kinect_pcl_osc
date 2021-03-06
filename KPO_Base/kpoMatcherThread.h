#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/kdtree/impl/kdtree_flann.hpp>
#include <pcl/search/kdtree.h>
#include <pcl/recognition/cg/hough_3d.h>

#include "kpo_types.h"


typedef boost::function<void(unsigned, Eigen::Vector3f, Eigen::Matrix3f)> MatchCallback;

class kpoMatcherThread {

public:
    unsigned object_id;
    std::string filename;

    kpoMatcherThread(Cloud::Ptr model_keypoints_, DescriptorCloud::Ptr model_descriptors_, RFCloud::Ptr model_refs_);

    Cloud::Ptr model_keypoints;
    DescriptorCloud::Ptr model_descriptors;
    RFCloud::Ptr model_refs;

    Cloud::Ptr scene_keypoints;
    DescriptorCloud::Ptr scene_descriptors;
    RFCloud::Ptr scene_refs;

    float cg_size_;
    float cg_thresh_;

    void copySceneClouds(Cloud scene_keypoints_, DescriptorCloud scene_descriptors_, RFCloud scene_refs_);

    // to be called what matches are found
    MatchCallback callback_;
    void setMatchCallback(MatchCallback callback);

    void operator ()();

};

