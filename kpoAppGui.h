
#ifndef PCL_APPS_OPENNI_PASSTHROUGH_3D_
#define PCL_APPS_OPENNI_PASSTHROUGH_3D_

// QT4
#include <QMainWindow>
#include <QApplication>
#include <QMutexLocker>
#include <QEvent>
#include <QFileDialog>
#include <QStringListModel>

// PCL
#include <pcl/visualization/pcl_visualizer.h>


#ifdef __GNUC__
#pragma GCC system_header
#endif

#include <ui_kpoApp.h>
#include "kpoBaseApp.h"


// Useful macros
#define FPS_CALC(_WHAT_) \
do \
{ \
    static unsigned count = 0;\
    static double last = pcl::getTime ();\
    double now = pcl::getTime (); \
    ++count; \
    if (now - last >= 1.0) \
    { \
      std::cout << "Average framerate("<< _WHAT_ << "): " << double(count)/double(now - last) << " Hz" <<  std::endl; \
      count = 0; \
      last = now; \
    } \
}while(false)

namespace Ui
{
  class KinectPclOsc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class kpoAppGui : public QMainWindow, public kpoBaseApp
{
  Q_OBJECT

  public:
    kpoAppGui (pcl::OpenNIGrabber& grabber);


  protected:
    QTimer *vis_timer_;
    boost::shared_ptr<pcl::visualization::PCLVisualizer> vis_;
    pcl::UniformSampling<PointType> uniform_sampling;
    float grabber_downsampling_radius_;

    void loadSettings();
    void pause();

  private:
    Ui::KinectPclOsc *ui_;

    void loadDescriptors(string filename);
    void saveDescriptors(string filename, const DescriptorCloud::Ptr &descriptors);

    QStringListModel *modelListModel;
    void addStringToModelsList(string str);


    void setDepthFromSliderValue(int val);


  public slots:

    void adjustPassThroughValues (int new_value);

  private slots:
    void timeoutSlot ();
    void updateView();
    

    void on_computeNormalsCheckbox_toggled(bool checked);

    void on_pauseCheckBox_toggled(bool checked);

    void on_findSHOTdescriptors_toggled(bool checked);

    void on_saveDescriptorButton_clicked();

    void on_matchModelsCheckbox_toggled(bool checked);

    void on_loadDescriptorButton_clicked();

    void on_presampleRadiusSlider_valueChanged(int value);

    void on_loadRawCloudButton_clicked();

    void on_removeNoiseCheckBox_toggled(bool checked);

    void on_setOscTargetButton_clicked();

    void on_depthThresholdlSlider_valueChanged(int value);

signals:
    void valueChanged (int new_value);
};

#endif    // PCL_APPS_OPENNI_PASSTHROUGH_3D_
