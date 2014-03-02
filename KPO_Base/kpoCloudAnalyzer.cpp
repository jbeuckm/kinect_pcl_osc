#include "kpoCloudAnalyzer.h"
#include "kpoObjectDescription.h"

kpoCloudAnalyzer::kpoCloudAnalyzer(CloudPtr inputCloud)
    : pcl_functions_(kpoPclFunctions(.01f))
    , scene_cloud_(new Cloud())
{
    std::cout << "will copy cloud with " << inputCloud->size() << std::endl;

    pcl::copyPointCloud(*inputCloud, *scene_cloud_);
}

void kpoCloudAnalyzer::operator ()()
{
    CloudPtr cleanCloud(new Cloud());

    std::cout << "scene_cloud_ " << scene_cloud_->size() << " cleanCloud " << cleanCloud->size() << std::endl;

//    pcl_functions_.removeNoise(scene_cloud_, cleanCloud);


    pcl::StatisticalOutlierRemoval<PointType> statistical_outlier_remover;

    statistical_outlier_remover.setMeanK (50);
    statistical_outlier_remover.setStddevMulThresh (1.0);

    statistical_outlier_remover.setInputCloud (scene_cloud_);
    statistical_outlier_remover.filter (*cleanCloud);



    pcl::copyPointCloud(*cleanCloud, *scene_cloud_);

    if (scene_cloud_->size() < 25) {
        std::cout << "cloud too small" << std::endl;
        return;
    }
    if (scene_cloud_->size() > 35000) {
        std::cout << "cloud too large" << std::endl;
        return;
    }
    std::cout << "cloud has " << scene_cloud_->size() << " points" << std::endl;


    scene_normals_.reset (new NormalCloud ());
    pcl_functions_.estimateNormals(scene_cloud_, scene_normals_);


    scene_keypoints_.reset(new Cloud ());
    pcl_functions_.downSample(scene_cloud_, scene_keypoints_);

    scene_descriptors_.reset(new DescriptorCloud ());
    pcl_functions_.computeShotDescriptors(scene_cloud_, scene_keypoints_, scene_normals_, scene_descriptors_);


    scene_refs_.reset(new RFCloud ());
    pcl_functions_.estimateReferenceFrames(scene_cloud_, scene_normals_, scene_keypoints_, scene_refs_);

    kpoObjectDescription od(scene_cloud_, scene_keypoints_, scene_normals_, scene_descriptors_, scene_refs_);
    od.filename = filename;
    od.object_id = object_id;

    callback_(od);
}
