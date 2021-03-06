#include "kpoBaseApp.h"

#include "BlobFinder.h"


kpoBaseApp::kpoBaseApp (pcl::OpenNIGrabber& grabber)
    : grabber_(grabber)
    , mtx_ ()
    , thread_pool(8)
    , osc_sender (new kpoOscSender())
    , boundingbox_ptr (new Cloud)
    , bb_hull_cloud_ (new Cloud)
{
    // Start the OpenNI data acquision
    boost::function<void (const CloudConstPtr&)> f = boost::bind (&kpoBaseApp::cloud_callback, this, _1);
    boost::signals2::connection c = grabber_.registerCallback (f);

    boost::function<void (const boost::shared_ptr<openni_wrapper::Image>&)> ic = boost::bind (&kpoBaseApp::image_callback, this, _1);
    boost::signals2::connection d = grabber_.registerCallback (ic);

    boost::function<void (const boost::shared_ptr<openni_wrapper::DepthImage>&)> dc = boost::bind (&kpoBaseApp::depth_callback, this, _1);
    boost::signals2::connection e = grabber_.registerCallback (dc);


    // Set defaults
    depth_filter_.setFilterFieldName ("z");
    depth_filter_.setFilterLimits (0.5, 5.0);

    depth_image_threshold_ = 128;

    grabber_downsampling_radius_ = .005f;
    build_bounding_box();

    QDir dir;
    m_sSettingsFile = dir.absolutePath() + "/settings.ini";

    model_index = 0;

    std::cout <<  m_sSettingsFile.toStdString() << endl;
    loadSettings();

    thread_load = 12;
    analyze_thread_count = 0;

    last_cloud_size = 0;
    last_snapshot_time = QDateTime::currentMSecsSinceEpoch();
    need_image_cap = false;

    grabber_.start ();
}


void kpoBaseApp::pause()
{
    paused_ = true;
}


void kpoBaseApp::loadSettings()
{
    std::cout << "loadSettings()" << std::endl;

    QSettings settings(m_sSettingsFile, QSettings::NativeFormat);

    depth_threshold_ = settings.value("depth_threshold_", 1.031).toDouble();
    depth_image_threshold_ = settings.value("depth_image_threshold_", 128).toInt();
    std::cout << "depth_image_threshold_ = " << depth_image_threshold_ << std::endl;

    keypoint_downsampling_radius_ = settings.value("keypoint_downsampling_radius_", .0075).toDouble();

    models_folder_ = settings.value("models_folder_", "/myshare/pointclouds/objects").toString();
    contours_folder_ = settings.value("contours_folder_", "/myshare/contours").toString();

    osc_sender_ip_ = settings.value("osc_sender_ip_", "192.168.0.48").toString();
    osc_sender_port_ = settings.value("osc_sender_port_", 7000).toInt();
    std::cout << "loaded ip " << osc_sender_ip_.toStdString() << ":" << osc_sender_port_ << std::endl;
    osc_sender->setNetworkTarget(osc_sender_ip_.toStdString().c_str(), osc_sender_port_);

    process_scene_ = true;
    match_models_ = false;

    loadModelFiles();
    loadContourFiles();

    process_scene_ = settings.value("process_scene_", false).toBool();
    match_models_ = settings.value("match_models_", false).toBool();

}
void kpoBaseApp::saveSettings()
{
    std::cout << "saveSettings()" << std::endl;

    QSettings settings(m_sSettingsFile, QSettings::NativeFormat);

    std::cout << "depth_threshold_ = " << depth_threshold_ << std::endl;
    settings.setValue("depth_threshold_", depth_threshold_);
    settings.setValue("depth_image_threshold_", depth_image_threshold_);

    settings.setValue("keypoint_downsampling_radius_", keypoint_downsampling_radius_);

    settings.setValue("models_folder_", models_folder_);
    settings.setValue("contours_folder_", contours_folder_);

    settings.setValue("process_scene_", process_scene_);
    settings.setValue("match_models_", match_models_);

    settings.setValue("osc_sender_ip_", osc_sender_ip_);
    settings.setValue("osc_sender_port_", osc_sender_port_);

    settings.sync();
}


void kpoBaseApp::loadModelFiles()
{
    std::cout << "kpoBaseApp::loadModelFiles() with " << models_folder_.toStdString() << std::endl;

    QStringList nameFilter("*.pcd");
    QDir directory(models_folder_);
    QStringList model_files = directory.entryList(nameFilter);

    int count = model_files.length();
    std::cout << "will load " << count << " model files." << std::endl;
    if (count < thread_load) {
        thread_load = count;
    }

    #pragma omp parallel
    {
        #pragma omp for

        for (int i=0; i<count; i++) {

            QString qs_filename = model_files[i];
            string filename = qs_filename.toStdString();

            std::cout << "reading " << filename << std::endl;

            int object_id = qs_filename.replace(QRegExp("[a-z]*.pcd"), "").toInt();

            std::cout << "object_id " << object_id << std::endl;

            load_model_cloud(models_folder_.toStdString() + "/" + filename, object_id);
        }

    }

}



// load a raw model capture and process it into a matchable set of keypoints, descriptors
void kpoBaseApp::load_model_cloud(string filename, int object_id)
{
    pcl::PointCloud<PointType>::Ptr model_cloud_(new pcl::PointCloud<PointType>());

    pcl::PCDReader reader;
    reader.read<PointType> (filename, *model_cloud_);

    if (model_cloud_->size() != 0) {

        kpoAnalyzerThread analyzer;

        analyzer.downsampling_radius_ = keypoint_downsampling_radius_;
        analyzer.copyInputCloud(*model_cloud_, filename, object_id);
        analyzer.setAnalyzerCallback( boost::bind (&kpoBaseApp::modelCloudAnalyzed, this, _1) );

        analyzer();
    }
}

void kpoBaseApp::modelCloudAnalyzed(kpoCloudDescription od)
{
    std::cout << "modelCloudAnalyzed()" << std::endl;

    Cloud *keypoints = new Cloud();
    pcl::copyPointCloud(od.keypoints, *keypoints);
    Cloud::Ptr keypoints_ptr(keypoints);

    DescriptorCloud *descriptors = new DescriptorCloud();
    pcl::copyPointCloud(od.descriptors, *descriptors);
    DescriptorCloud::Ptr descriptors_ptr(descriptors);

    RFCloud *reference_frames= new RFCloud();
    pcl::copyPointCloud(od.reference_frames, *reference_frames);
    RFCloud::Ptr reference_frames_ptr(reference_frames);

    kpoMatcherThread *matcher = new kpoMatcherThread(keypoints_ptr, descriptors_ptr, reference_frames_ptr);

    boost::shared_ptr<kpoMatcherThread> matcher_thread(matcher);
    matcher_thread->object_id = od.object_id;
    matcher_thread->filename = od.filename;

    MatchCallback f = boost::bind (&kpoBaseApp::matchesFound, this, _1, _2, _3);
    matcher_thread->setMatchCallback(f);

    #pragma omp critical(dataupdate)
    {
            matcher_threads.push_back(matcher_thread);
    }

}

void kpoBaseApp::matchesFound(int object_id, Eigen::Vector3f translation, Eigen::Matrix3f rotation)
{
    std::cout << "found object " << object_id << " at ";
    std::cout << translation(0) << "," << translation(1) << "," << translation(2) << std::endl;

    osc_sender->sendObject(object_id, translation(0), translation(1), translation(2));
}


void kpoBaseApp::depth_callback (const boost::shared_ptr< openni_wrapper::DepthImage > &depth_image)
{
    QMutexLocker locker (&mtx_);

    unsigned image_width_ = depth_image->getWidth();
    unsigned image_height_ = depth_image->getHeight();

    const XnDepthPixel* pDepthMap = depth_image->getDepthMetaData().Data();

    cv::Mat depth(480, 640, CV_8UC1);
    int x, y, i = 0;
    for(  y =0; y < 480 ; y++)
    {
        for( x = 0; x < 640; x++)
        {
            depth.at<unsigned char >(y,x) = pDepthMap[i] / 8;

            i++;
        }
    }

    threshold( depth, scene_depth_image_, depth_image_threshold_, 255, THRESH_TOZERO_INV );

    depth_blob_finder.find(scene_depth_image_);

    processDepthBlobs(depth_blob_finder);
}
void kpoBaseApp::processDepthBlobs(BlobFinder bf)
{
    for( int i = 0; i < bf.numBlobs; i++ )
    {
        if (bf.radius[i] > 15) {
            osc_sender->sendBlob(bf.center[i].x, bf.center[i].y, bf.radius[i]);

            findMatchingContours(bf.contours[i]);
        }
    }

    std::cout << std::endl;
}

bool operator <(const kpoObjectContour& a, const kpoObjectContour& b) { return (a.error < b.error); }

void kpoBaseApp::findMatchingContours(Contour scene_contour)
{
    for (int i=0; i<contour_objects_.size(); i++) {
        kpoObjectContour test_object = contour_objects_[i];
        contour_objects_[i].error = cv::matchShapes(scene_contour, test_object.contour, CV_CONTOURS_MATCH_I3, 0);
    }

    // find the match with minimum error
    kpoObjectContour min = *(std::min_element(contour_objects_.begin(), contour_objects_.end()));

    osc_sender->sendContour(min.object_id, min.error);

    std::cout << min.object_id << " ";
}


void kpoBaseApp::image_callback (const boost::shared_ptr<openni_wrapper::Image> &image)
{
    unsigned image_width_ = image->getWidth();
    unsigned image_height_ = image->getHeight();

    static unsigned rgb_array_size = 0;
    static boost::shared_array<unsigned char> rgb_array ((unsigned char*)(NULL));

    static unsigned char* rgb_buffer = 0;

    // here we need exact the size of the point cloud for a one-one correspondence!
    if (rgb_array_size < image_width_ * image_height_ * 3)
    {
      rgb_array_size = image_width_ * image_height_ * 3;
      rgb_array.reset (new unsigned char [rgb_array_size]);
      rgb_buffer = rgb_array.get ();
    }
    image->fillRGB (image_width_, image_height_, rgb_buffer, image_width_ * 3);

    {
        QMutexLocker locker (&mtx_);

        openniImage2opencvMat((XnRGB24Pixel*)rgb_buffer, scene_image_, image_height_, image_width_);

        if (need_image_cap) {
            std::cout << "need_image_cap " << need_image_cap << std::endl;
            cv::Mat rgb_image_;
            cv::cvtColor(scene_image_, rgb_image_, CV_BGR2RGB);

            qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
            if (timestamp - last_snapshot_time > 30000) {
                last_snapshot_time = timestamp;
                QString filename = QString::fromUtf8("/myshare/autonomous_snapshots/");
                filename += QString::number(timestamp);
                filename += QString::fromUtf8(".png");

                cv::imwrite(filename.toStdString().c_str(), rgb_image_);
                need_image_cap = 0;
            }
        }
    }

}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
void kpoBaseApp::cloud_callback (const CloudConstPtr& cloud)
{
    if (paused_) {
        return;
    }


    if (process_scene_) {
        QMutexLocker locker (&mtx_);

        scene_cloud_.reset (new Cloud);

        depth_filter_.setInputCloud (cloud);
        depth_filter_.setFilterLimits(0, depth_threshold_);
        depth_filter_.filter (*scene_cloud_);

        CloudPtr cropped(new Cloud());
        crop_bounding_box_(scene_cloud_, cropped);
        pcl::copyPointCloud(*cropped, *scene_cloud_);

        osc_sender->send("/kinect/pointcloud/size", scene_cloud_->size());
    }

    if (analyze_thread_count < 1) {

        process_cloud(cloud);

    }

}


void kpoBaseApp::process_cloud (const CloudConstPtr& cloud)
{

    if (process_scene_) {

        FPS_CALC ("computation");
        QMutexLocker locker (&mtx_);

        kpoAnalyzerThread analyzer;

        if (scene_cloud_->size() > 35000) {
            analyzer.downsampling_radius_ = (float)scene_cloud_->size() / 500000.0;
        }
        else {
            analyzer.downsampling_radius_ = keypoint_downsampling_radius_;
        }

        std::cout << "abs cloud difference = " << abs((int)scene_cloud_->size() - last_cloud_size) << endl;
        if (abs((int)scene_cloud_->size() - last_cloud_size) > 500) {
            need_image_cap = 1;
        }
        last_cloud_size = scene_cloud_->size();

        analyzer.setAnalyzerCallback( boost::bind (&kpoBaseApp::sceneCloudAnalyzed, this, _1) );

        analyze_thread_count++;

        analyzer.copyInputCloud(*scene_cloud_, "", 0);
        analyze_thread = new boost::thread(boost::bind(&kpoAnalyzerThread::operator(), analyzer));

    }
}

void kpoBaseApp::sceneCloudAnalyzed(kpoCloudDescription od)
{

    std::cout << "sceneCloudAnalyzed()" << std::endl;
    std::cout << "cloud has " << od.cloud.size() << std::endl;

    if (match_models_) {

        QMutexLocker locker (&mtx_);

        int batch = thread_load - thread_pool.pending();

        for (unsigned i=0; i<batch; i++) {

            boost::shared_ptr<kpoMatcherThread> matcher = matcher_threads.at(model_index);

            matcher->copySceneClouds(od.keypoints, od.descriptors, od.reference_frames);

            thread_pool.schedule(boost::ref( *matcher ));

            model_index = (model_index + 1) % matcher_threads.size();

        }
    }

    analyze_thread_count--;
}



void kpoBaseApp::loadContourFiles()
{
    std::cout << "kpoBaseApp::loadContourFiles() with " << contours_folder_.toStdString() << std::endl;

    QStringList nameFilter("*.path");
    QDir directory(contours_folder_);
    QStringList contour_files = directory.entryList(nameFilter);

    int count = contour_files.length();
    std::cout << "will load " << count << " contour files." << std::endl;

    for (int i=0; i<count; i++) {

        QString qs_filename = contour_files[i];
        string filename = qs_filename.toStdString();

        std::cout << "reading " << filename << std::endl;

        kpoObjectContour obj = load_contour_file(contours_folder_.toStdString() + "/" + filename);
        contour_objects_.push_back(obj);
    }
}


kpoObjectContour kpoBaseApp::load_contour_file(string file_path)
{
    kpoObjectContour object_contour;

    {
      // Create and input archive
      std::ifstream ifs(file_path.c_str());
      boost::archive::text_iarchive ar(ifs);

      // Load data
      ar & object_contour;
    }

    return object_contour;
}

void kpoBaseApp::save_contour_file(kpoObjectContour object_contour, string file_path)
{
    {
      // Create an output archive
      std::ofstream ofs(file_path.c_str());
      boost::archive::text_oarchive ar(ofs);

     // Write data
      ar & object_contour;
    }
}

#include <pcl/surface/convex_hull.h>
#include <pcl/filters/crop_hull.h>

void kpoBaseApp::build_bounding_box()
{
    pcl::PointCloud<pcl::PointXYZ> bb;
    bb.push_back(pcl::PointXYZ(-.7, -.7, 1.5));
    bb.push_back(pcl::PointXYZ(.7, -.7, 1.5));
    bb.push_back(pcl::PointXYZ(.7, .7, 1.5));
    bb.push_back(pcl::PointXYZ(-.7, .7, 1.5));
    bb.push_back(pcl::PointXYZ(-.7, -.7, -1.0));
    bb.push_back(pcl::PointXYZ(.7, -.7, -1.0));
    bb.push_back(pcl::PointXYZ(.7, .7, -1.0));
    bb.push_back(pcl::PointXYZ(-.7, .7, -1.0));

    pcl::copyPointCloud(bb, *boundingbox_ptr);

    pcl::ConvexHull<PointType> hull;
    hull.setInputCloud(boundingbox_ptr);
    hull.setDimension(3);

    hull.reconstruct(*bb_hull_cloud_, bb_polygons);
}

void kpoBaseApp::crop_bounding_box_(const CloudConstPtr &input_cloud, CloudPtr &output_cloud)
{
    pcl::CropHull<PointType> bb_filter;

    bb_filter.setDim(3);
    bb_filter.setInputCloud(input_cloud);
    bb_filter.setHullIndices(bb_polygons);
    bb_filter.setHullCloud(boundingbox_ptr);

    bb_filter.filter(*output_cloud);

}


