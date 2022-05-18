/*
 * Copyright (c) 2022, Aaron Marburg,  University of Washington
 *
 * Based on rqt_image_view plugin:
 * Copyright (c) 2011, Dirk Thomas, TU Darmstadt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the TU Darmstadt or University of Washington
 *     nor the names of its contributors may be used to endorse or
 *     promote products derived from this software without specific
 *     prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <rqt_sonar_image_view/image_view.h>

#include <pluginlib/class_list_macros.h>
#include <ros/master.h>
#include <sensor_msgs/image_encodings.h>

#include <cv_bridge/cv_bridge.h>
#include <opencv2/imgproc/imgproc.hpp>

#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>

#include <sonar_image_proc/ColorMaps.h>
#include <sonar_image_proc/log_sonar_image_msg_interface.h>
#include <sonar_image_proc/sonar_image_msg_interface.h>

namespace rqt_sonar_image_view {

using sonar_image_proc::LogScaleSonarImageMsgInterface;
using sonar_image_proc::SonarImageMsgInterface;

ImageView::ImageView() : rqt_gui_cpp::Plugin(), widget_(0) {
  setObjectName("SonarImageView");
}

void ImageView::initPlugin(qt_gui_cpp::PluginContext &context) {
  widget_ = new QWidget();
  ui_.setupUi(widget_);

  if (context.serialNumber() > 1) {
    widget_->setWindowTitle(widget_->windowTitle() + " (" +
                            QString::number(context.serialNumber()) + ")");
  }
  context.addWidget(widget_);

  ui_.min_db_input->setValidator(new QDoubleValidator(-225, 0, 1, this));
  ui_.max_db_input->setValidator(new QDoubleValidator(-225, 0, 1, this));

  // setColorSchemeList();
  // ui_.color_scheme_combo_box->setCurrentIndex(
  //     ui_.color_scheme_combo_box->findText("Gray"));

  updateTopicList();
  ui_.topics_combo_box->setCurrentIndex(ui_.topics_combo_box->findText(""));
  connect(ui_.topics_combo_box, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onTopicChanged(int)));

  ui_.refresh_topics_push_button->setIcon(QIcon::fromTheme("view-refresh"));
  connect(ui_.refresh_topics_push_button, SIGNAL(pressed()), this,
          SLOT(updateTopicList()));

  // Don't need a callback on OSD, just use its state
  //  connect(ui_.osd_check_box, SIGNAL(toggled(bool)), this,
  //  SLOT(onOsd(bool)));

  ui_.save_as_image_push_button->setIcon(QIcon::fromTheme("document-save-as"));
  connect(ui_.save_as_image_push_button, SIGNAL(pressed()), this,
          SLOT(saveImage()));

  // set topic name if passed in as argument
  const QStringList &argv = context.argv();
  if (!argv.empty()) {
    arg_topic_name = argv[0];
    selectTopic(arg_topic_name);
  }
  pub_topic_custom_ = false;

  ui_.image_frame->setOuterLayout(ui_.image_layout);

  QRegExp rx(
      "([a-zA-Z/][a-zA-Z0-9_/]*)?"); // see
                                     // http://www.ros.org/wiki/ROS/Concepts#Names.Valid_Names
                                     // (but also accept an empty field)
  // ui_.publish_click_location_topic_line_edit->setValidator(
  //     new QRegExpValidator(rx, this));
  // connect(ui_.publish_click_location_check_box, SIGNAL(toggled(bool)), this,
  //         SLOT(onMousePublish(bool)));
  // connect(ui_.image_frame, SIGNAL(mouseLeft(int, int)), this,
  //         SLOT(onMouseLeft(int, int)));
  // connect(ui_.publish_click_location_topic_line_edit,
  // SIGNAL(editingFinished()),
  //         this, SLOT(onPubTopicChanged()));

  hide_toolbar_action_ = new QAction(tr("Hide toolbar"), this);
  hide_toolbar_action_->setCheckable(true);
  ui_.image_frame->addAction(hide_toolbar_action_);
  connect(hide_toolbar_action_, SIGNAL(toggled(bool)), this,
          SLOT(onHideToolbarChanged(bool)));
}

void ImageView::shutdownPlugin() {
  subscriber_.shutdown();
  pub_mouse_left_.shutdown();
}

void ImageView::saveSettings(qt_gui_cpp::Settings &plugin_settings,
                             qt_gui_cpp::Settings &instance_settings) const {
  QString topic = ui_.topics_combo_box->currentText();
  // qDebug("ImageView::saveSettings() topic '%s'",
  // topic.toStdString().c_str());
  instance_settings.setValue("topic", topic);
  instance_settings.setValue("draw_overlay", ui_.osd_check_box->isChecked());

  // instance_settings.setValue("publish_click_location",
  //                            ui_.publish_click_location_check_box->isChecked());
  // instance_settings.setValue(
  //     "mouse_pub_topic", ui_.publish_click_location_topic_line_edit->text());
  instance_settings.setValue("toolbar_hidden",
                             hide_toolbar_action_->isChecked());
  // instance_settings.setValue("color_scheme",
  //                            ui_.color_scheme_combo_box->currentIndex());
}

void ImageView::restoreSettings(const qt_gui_cpp::Settings &plugin_settings,
                                const qt_gui_cpp::Settings &instance_settings) {
  bool draw_overlay = instance_settings.value("draw_overlay", false).toBool();
  ui_.osd_check_box->setChecked(draw_overlay);

  QString topic = instance_settings.value("topic", "").toString();
  // don't overwrite topic name passed as command line argument
  if (!arg_topic_name.isEmpty()) {
    arg_topic_name = "";
  } else {
    // qDebug("ImageView::restoreSettings() topic '%s'",
    // topic.toStdString().c_str());
    selectTopic(topic);
  }

  // bool publish_click_location =
  //     instance_settings.value("publish_click_location", false).toBool();
  // ui_.publish_click_location_check_box->setChecked(publish_click_location);

  // QString pub_topic = instance_settings.value("mouse_pub_topic",
  // "").toString();
  // ui_.publish_click_location_topic_line_edit->setText(pub_topic);

  bool toolbar_hidden =
      instance_settings.value("toolbar_hidden", false).toBool();
  hide_toolbar_action_->setChecked(toolbar_hidden);

  // int color_scheme =
  //     instance_settings
  //         .value("color_scheme", ui_.color_scheme_combo_box->currentIndex())
  //         .toInt();
  // ui_.color_scheme_combo_box->setCurrentIndex(color_scheme);
}

// void ImageView::setColorSchemeList() {
//   static const std::map<std::string, int> COLOR_SCHEME_MAP{
//       {"Gray", -1}, // Special case: no color map
//       {"Autumn", cv::COLORMAP_AUTUMN},
//       {"Bone", cv::COLORMAP_BONE},
//       {"Cool", cv::COLORMAP_COOL},
//       {"Hot", cv::COLORMAP_HOT},
//       {"Hsv", cv::COLORMAP_HSV},
//       {"Jet", cv::COLORMAP_JET},
//       {"Ocean", cv::COLORMAP_OCEAN},
//       {"Pink", cv::COLORMAP_PINK},
//       {"Rainbow", cv::COLORMAP_RAINBOW},
//       {"Spring", cv::COLORMAP_SPRING},
//       {"Summer", cv::COLORMAP_SUMMER},
//       {"Winter", cv::COLORMAP_WINTER}};

//   for (const auto &kv : COLOR_SCHEME_MAP) {
//     ui_.color_scheme_combo_box->addItem(QString::fromStdString(kv.first),
//                                         QVariant(kv.second));
//   }
// }

void ImageView::updateTopicList() {
  QSet<QString> message_types;
  message_types.insert("acoustic_msgs/SonarImage");

  QString selected = ui_.topics_combo_box->currentText();

  // fill combo box
  QList<QString> topics = getTopics(message_types).values();
  topics.append("");
  qSort(topics);
  ui_.topics_combo_box->clear();
  for (QList<QString>::const_iterator it = topics.begin(); it != topics.end();
       it++) {
    QString label(*it);
    label.replace(" ", "/");
    ui_.topics_combo_box->addItem(label, QVariant(*it));
  }

  // restore previous selection
  selectTopic(selected);
}

QSet<QString> ImageView::getTopics(const QSet<QString> &message_types) {
  ros::master::V_TopicInfo topic_info;
  ros::master::getTopics(topic_info);

  QSet<QString> all_topics;
  for (ros::master::V_TopicInfo::const_iterator it = topic_info.begin();
       it != topic_info.end(); it++) {
    all_topics.insert(it->name.c_str());
  }

  QSet<QString> topics;
  for (ros::master::V_TopicInfo::const_iterator it = topic_info.begin();
       it != topic_info.end(); it++) {
    if (message_types.contains(it->datatype.c_str())) {
      QString topic = it->name.c_str();
      topics.insert(topic);
    }
  }
  return topics;
}

void ImageView::selectTopic(const QString &topic) {
  int index = ui_.topics_combo_box->findText(topic);
  if (index == -1) {
    // add topic name to list if not yet in
    QString label(topic);
    label.replace(" ", "/");
    ui_.topics_combo_box->addItem(label, QVariant(topic));
    index = ui_.topics_combo_box->findText(topic);
  }
  ui_.topics_combo_box->setCurrentIndex(index);
}

void ImageView::onTopicChanged(int index) {
  conversion_mat_.release();

  subscriber_.shutdown();

  // reset image on topic change
  ui_.image_frame->setImage(QImage());

  QStringList parts =
      ui_.topics_combo_box->itemData(index).toString().split(" ");
  QString topic = parts.first();
  // QString transport = parts.length() == 2 ? parts.last() : "raw";

  if (!topic.isEmpty()) {
    subscriber_ = getNodeHandle().subscribe(topic.toStdString(), 1,
                                            &ImageView::callbackImage, this);
  }

  // onMousePublish(ui_.publish_click_location_check_box->isChecked());
}

void ImageView::saveImage() {
  // take a snapshot before asking for the filename
  QImage img = ui_.image_frame->getImageCopy();

  QString file_name =
      QFileDialog::getSaveFileName(widget_, tr("Save as image"), "image.png",
                                   tr("Image (*.bmp *.jpg *.png *.tiff)"));
  if (file_name.isEmpty()) {
    return;
  }

  img.save(file_name);
}

void ImageView::onMousePublish(bool checked) {
  // std::string topicName;
  // if (pub_topic_custom_) {
  //   topicName =
  //       ui_.publish_click_location_topic_line_edit->text().toStdString();
  // } else {
  //   if (!subscriber_.getTopic().empty()) {
  //     topicName = subscriber_.getTopic() + "_mouse_left";
  //   } else {
  //     topicName = "mouse_left";
  //   }
  //   ui_.publish_click_location_topic_line_edit->setText(
  //       QString::fromStdString(topicName));
  // }

  // if (checked) {
  //   pub_mouse_left_ =
  //       getNodeHandle().advertise<geometry_msgs::Point>(topicName, 1000);
  // } else {
  //   pub_mouse_left_.shutdown();
  // }
}

void ImageView::onMouseLeft(int x, int y) {
  // if (ui_.publish_click_location_check_box->isChecked() &&
  //     !ui_.image_frame->getImage().isNull()) {
  //   geometry_msgs::Point clickCanvasLocation;
  //   // Publish click location in pixel coordinates
  //   clickCanvasLocation.x = round((double)x /
  //   (double)ui_.image_frame->width() *
  //                                 (double)ui_.image_frame->getImage().width());
  //   clickCanvasLocation.y =
  //       round((double)y / (double)ui_.image_frame->height() *
  //             (double)ui_.image_frame->getImage().height());
  //   clickCanvasLocation.z = 0;

  //   geometry_msgs::Point clickLocation = clickCanvasLocation;

  //   pub_mouse_left_.publish(clickLocation);
  // }
}

void ImageView::onPubTopicChanged() {
  // pub_topic_custom_ =
  //     !(ui_.publish_click_location_topic_line_edit->text().isEmpty());
  // onMousePublish(ui_.publish_click_location_check_box->isChecked());
}

void ImageView::onHideToolbarChanged(bool hide) {
  ui_.toolbar_widget->setVisible(!hide);
}

void ImageView::callbackImage(const acoustic_msgs::SonarImage::ConstPtr &msg) {
  SonarImageMsgInterface ping(msg);

  if (ping.data_type() ==
      sonar_image_proc::AbstractSonarInterface::TYPE_UINT32) {
    const auto min_db = ui_.min_db_input->text().toDouble();
    const auto max_db = ui_.max_db_input->text().toDouble();
    LogScaleSonarImageMsgInterface log_ping(msg, min_db, max_db);

    conversion_mat_ = sonar_drawer_.drawSonar(
        log_ping, sonar_image_proc::InfernoColorMap(), cv::Mat(0, 0, CV_8UC3),
        ui_.osd_check_box->isChecked());

    QImage image(conversion_mat_.data, conversion_mat_.cols,
                 conversion_mat_.rows, conversion_mat_.step[0],
                 QImage::Format_RGB888);

    ui_.min_db_label->show();
    ui_.min_db_input->show();
    ui_.max_db_label->show();
    ui_.max_db_input->show();

    ui_.image_frame->setImage(image);
  } else {
    conversion_mat_ = sonar_drawer_.drawSonar(
        ping, sonar_image_proc::InfernoColorMap(), cv::Mat(0, 0, CV_8UC3),
        ui_.osd_check_box->isChecked());

    ui_.min_db_label->hide();
    ui_.min_db_input->hide();
    ui_.max_db_label->hide();
    ui_.max_db_input->hide();

    QImage image(conversion_mat_.data, conversion_mat_.cols,
                 conversion_mat_.rows, conversion_mat_.step[0],
                 QImage::Format_RGB888);
    ui_.image_frame->setImage(image);
  }
}
} // namespace rqt_sonar_image_view

PLUGINLIB_EXPORT_CLASS(rqt_sonar_image_view::ImageView, rqt_gui_cpp::Plugin)
