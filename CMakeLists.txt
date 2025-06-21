cmake_minimum_required(VERSION 3.5)
project(rs_to_velodyne)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(PCL REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(pcl_ros REQUIRED)
find_package(FLANN REQUIRED)

include_directories(
  include
  ${PCL_INCLUDE_DIRS}
)

add_executable(rs_to_velodyne src/rs_to_velodyne.cpp)
ament_target_dependencies(rs_to_velodyne
  rclcpp
  sensor_msgs
  std_msgs
  pcl_conversions
  pcl_ros
  PCL
  FLANN
)

install(TARGETS
  rs_to_velodyne
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
