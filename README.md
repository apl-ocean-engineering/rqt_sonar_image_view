# rqt_sonar_image_view

![Image of rqt window showing sonar data](images/rqt_sonar_image_view.jpg)

RQT pluging for rendering [ProjectedSonarImage](https://github.com/apl-ocean-engineering/hydrographic_msgs/blob/main/acoustic_msgs/msg/ProjectedSonarImage.msg) messages.

Forked from [rqt_image_view](https://github.com/ros-visualization/rqt_image_view) and uses rendering functions in [sonar_image_proc](https://github.com/apl-ocean-engineering/sonar_image_proc) contains code to postprocess sonar data, including drawing the sonar data to an OpenCV Mat (contains both ROS and non-ROS code).

# Dependencies

This package depends on [sonar_image_proc](https://github.com/apl-ocean-engineering/sonar_image_proc), which must be installed or built in the same catkin workspace.

The simplest way to capture all dependencies (which includes `sonar_image_proc` as well as *it's* dependencies) is to use `rqt_sonar_image_view.rosinstall`:

```
cd <catkin_workspace>/src
vcs import < rqt_sonar_image_view/rqt_sonar_image_view.rosinstall
```

# Related Packages

* [sonar_image_proc](https://github.com/apl-ocean-engineering/sonar_image_proc) contains code to postprocess sonar data, including drawing the sonar data to an OpenCV Mat (contains both ROS and non-ROS code).
* [liboculus](https://github.com/apl-ocean-engineering/liboculus) is a (non-ROS) library which parses the Oculus network protocol and handles sonar network IO.
* [oculus_sonar_driver](https://gitlab.com/apl-ocean-engineering/oculus_sonar_driver) provides a ROS node for interfacing with the Oculus sonar.
* [acoustic_msgs](https://github.com/apl-ocean-engineering/hydrographic_msgs/tree/main/acoustic_msgs) defines the ROS [ProjectedSonarImage](https://github.com/apl-ocean-engineering/hydrographic_msgs/blob/main/acoustic_msgs/msg/ProjectedSonarImage.msg) message type published by [oculus_sonar_driver](https://gitlab.com/apl-ocean-engineering/oculus_sonar_driver).

# License

This repo maintains the BSD license used by [rqt_image_view](https://github.com/ros-visualization/rqt_image_view)
