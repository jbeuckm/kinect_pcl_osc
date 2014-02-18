#ifndef KPOBASEAPP_H
#define KPOBASEAPP_H

// QT4
#include <QMutex>
#include <QTimer>
#include <QObject>
#include <QSettings>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/io/openni_grabber.h>
#include <pcl/compression/octree_pointcloud_compression.h>
#include <pcl/common/time.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/filters/passthrough.h>
#include <pcl/keypoints/uniform_sampling.h>

#include "kpoPclFunctions.h"
#include "kpoObjectDescription.h"
#include "kpo_types.h"
#include "kpoOscSender.h"


class kpoBaseApp
{
public:

    kpoBaseApp (pcl::OpenNIGrabber& grabber);
    ~kpoBaseApp ()
    {
      if (grabber_.isRunning ()) {
        grabber_.stop ();
      }
      saveSettings();
    }

protected:
    pcl::OpenNIGrabber& grabber_;
    std::string device_id_;

    pcl::PassThrough<PointType> depth_filter_;

    CloudPtr scene_cloud_;
    CloudPtr scene_keypoints_;
    NormalCloud::Ptr scene_normals_;
    DescriptorCloud::Ptr scene_descriptors_;
    RFCloud::Ptr scene_rf_;

    pcl::UniformSampling<PointType> uniform_sampling;
    float grabber_downsampling_radius_;
    float keypoint_downsampling_radius_;

    std::vector< boost::shared_ptr<kpoObjectDescription> > models_;
    std::vector< boost::shared_ptr<kpoObjectDescription> > match_queue_;

    QMutex mtx_;

    kpoPclFunctions pcl_functions_;
    bool paused_;
    bool remove_noise_;
    bool estimate_normals_;
    bool compute_descriptors_;
    bool match_models_;

    float depth_threshold_;

    QString m_sSettingsFile;

    QString models_folder_;

    void loadExemplar(string filename);

    void loadSettings();
    void saveSettings();

    void cloud_callback (const CloudConstPtr& cloud);
    void process_cloud (const CloudConstPtr& cloud);

    void pause();

    kpoOscSender oscSender;
    QString osc_sender_ip_;
    int osc_sender_port_;

};

#endif // KPOBASEAPP_H
