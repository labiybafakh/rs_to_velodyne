//#include "utility.h"
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>

std::string output_type;
rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr publisher_velodyne_points;
rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subscriber_robosense_points;

static int RING_ID_MAP_RUBY[] = {
        3, 66, 33, 96, 11, 74, 41, 104, 19, 82, 49, 112, 27, 90, 57, 120,
        35, 98, 1, 64, 43, 106, 9, 72, 51, 114, 17, 80, 59, 122, 25, 88,
        67, 34, 97, 0, 75, 42, 105, 8, 83, 50, 113, 16, 91, 58, 121, 24,
        99, 2, 65, 32, 107, 10, 73, 40, 115, 18, 81, 48, 123, 26, 89, 56,
        7, 70, 37, 100, 15, 78, 45, 108, 23, 86, 53, 116, 31, 94, 61, 124,
        39, 102, 5, 68, 47, 110, 13, 76, 55, 118, 21, 84, 63, 126, 29, 92,
        71, 38, 101, 4, 79, 46, 109, 12, 87, 54, 117, 20, 95, 62, 125, 28,
        103, 6, 69, 36, 111, 14, 77, 44, 119, 22, 85, 52, 127, 30, 93, 60
};
static int RING_ID_MAP_16[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8
};

// rslidar和velodyne的格式有微小的区别
// rslidar的点云格式
struct RsPointXYZIRT {
    PCL_ADD_POINT4D;
    PCL_ADD_INTENSITY;
    uint16_t ring;
    double timestamp;

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
} EIGEN_ALIGN16;
POINT_CLOUD_REGISTER_POINT_STRUCT(RsPointXYZIRT,
                                  (float, x, x)(float, y, y)(float, z, z)(float, intensity, intensity)
                                          (uint16_t, ring, ring)(double, timestamp, timestamp))

// velodyne的点云格式
struct VelodynePointXYZIRT {
    PCL_ADD_POINT4D

    PCL_ADD_INTENSITY;
    uint16_t ring;
    float time;

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
} EIGEN_ALIGN16;

POINT_CLOUD_REGISTER_POINT_STRUCT (VelodynePointXYZIRT,
                                   (float, x, x)(float, y, y)(float, z, z)(float, intensity, intensity)
                                           (uint16_t, ring, ring)(float, time, time)
)

struct VelodynePointXYZIR {
    PCL_ADD_POINT4D

    PCL_ADD_INTENSITY;
    uint16_t ring;

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
} EIGEN_ALIGN16;

POINT_CLOUD_REGISTER_POINT_STRUCT (VelodynePointXYZIR,
                                   (float, x, x)(float, y, y)
                                           (float, z, z)(float, intensity, intensity)
                                           (uint16_t, ring, ring)
)

template<typename T>
bool has_nan(T point) {

    // remove nan point, or the feature assocaion will crash, the surf point will containing nan points
    // pcl remove nan not work normally
    // ROS_ERROR("Containing nan point!");
    if (std::isnan(point.x) || std::isnan(point.y) || std::isnan(point.z)) {
        return true;
    } else {
        return false;
    }
}

template<typename T>
void publish_points(T &new_pc, const sensor_msgs::msg::PointCloud2::SharedPtr old_msg) {
    new_pc->is_dense = true;
    sensor_msgs::msg::PointCloud2 pc_new_msg;
    pcl::toROSMsg(*new_pc, pc_new_msg);
    pc_new_msg.header = old_msg->header;
    pc_new_msg.header.frame_id = "velodyne";
    publisher_velodyne_points->publish(pc_new_msg);
}

void rsHandler_XYZI(const sensor_msgs::msg::PointCloud2::SharedPtr pc_msg) {
    pcl::PointCloud<pcl::PointXYZI>::Ptr pc(new pcl::PointCloud<pcl::PointXYZI>());
    pcl::PointCloud<VelodynePointXYZIR>::Ptr pc_new(new pcl::PointCloud<VelodynePointXYZIR>());
    pcl::fromROSMsg(*pc_msg, *pc);

    for (int point_id = 0; point_id < pc->points.size(); ++point_id) {
        if (has_nan(pc->points[point_id]))
            continue;

        VelodynePointXYZIR new_point;
        new_point.x = pc->points[point_id].x;
        new_point.y = pc->points[point_id].y;
        new_point.z = pc->points[point_id].z;
        new_point.intensity = pc->points[point_id].intensity;
        if (pc->height == 16) {
            new_point.ring = RING_ID_MAP_16[point_id / pc->width];
        } else if (pc->height == 128) {
            new_point.ring = RING_ID_MAP_RUBY[point_id % pc->height];
        }
        pc_new->points.push_back(new_point);
    }

    publish_points(pc_new, pc_msg);
}


template<typename T_in_p, typename T_out_p>
void handle_pc_msg(const typename pcl::PointCloud<T_in_p>::Ptr &pc_in,
                   const typename pcl::PointCloud<T_out_p>::Ptr &pc_out) {

    // to new pointcloud
    for (int point_id = 0; point_id < pc_in->points.size(); ++point_id) {
        if (has_nan(pc_in->points[point_id]))
            continue;
        T_out_p new_point;
//        std::copy(pc->points[point_id].data, pc->points[point_id].data + 4, new_point.data);
        new_point.x = pc_in->points[point_id].x;
        new_point.y = pc_in->points[point_id].y;
        new_point.z = pc_in->points[point_id].z;
        new_point.intensity = pc_in->points[point_id].intensity;
//        new_point.ring = pc->points[point_id].ring;
//        // 计算相对于第一个点的相对时间
//        new_point.time = float(pc->points[point_id].timestamp - pc->points[0].timestamp);
        pc_out->points.push_back(new_point);
    }
}

template<typename T_in_p, typename T_out_p>
void add_ring(const typename pcl::PointCloud<T_in_p>::Ptr &pc_in,
              const typename pcl::PointCloud<T_out_p>::Ptr &pc_out) {
    // to new pointcloud
    int valid_point_id = 0;
    for (int point_id = 0; point_id < pc_in->points.size(); ++point_id) {
        if (has_nan(pc_in->points[point_id]))
            continue;
        // 跳过nan点
        pc_out->points[valid_point_id++].ring = pc_in->points[point_id].ring;
    }
}

template<typename T_in_p, typename T_out_p>
void add_time(const typename pcl::PointCloud<T_in_p>::Ptr &pc_in,
              const typename pcl::PointCloud<T_out_p>::Ptr &pc_out) {
    // to new pointcloud
    int valid_point_id = 0;
    for (int point_id = 0; point_id < pc_in->points.size(); ++point_id) {
        if (has_nan(pc_in->points[point_id]))
            continue;
        // 跳过nan点
        pc_out->points[valid_point_id++].time = float(pc_in->points[point_id].timestamp - pc_in->points[0].timestamp);
    }
}

void rsHandler_XYZIRT(const sensor_msgs::msg::PointCloud2::SharedPtr pc_msg) {
    pcl::PointCloud<RsPointXYZIRT>::Ptr pc_in(new pcl::PointCloud<RsPointXYZIRT>());
    pcl::fromROSMsg(*pc_msg, *pc_in);

    if (output_type == "XYZIRT") {
        pcl::PointCloud<VelodynePointXYZIRT>::Ptr pc_out(new pcl::PointCloud<VelodynePointXYZIRT>());
        double base_time = pc_in->points.empty() ? 0.0 : pc_in->points[0].timestamp;
        for (const auto& pt : pc_in->points) {
            if (has_nan(pt)) continue;
            VelodynePointXYZIRT vpt;
            vpt.x = pt.x;
            vpt.y = pt.y;
            vpt.z = pt.z;
            vpt.intensity = pt.intensity;
            vpt.ring = pt.ring;
            vpt.time = static_cast<float>(pt.timestamp - base_time);
            pc_out->points.push_back(vpt);
        }
        pc_out->width = pc_out->points.size();
        pc_out->height = 1;
        pc_out->is_dense = true;
        publish_points(pc_out, pc_msg);
    } else if (output_type == "XYZIR") {
        pcl::PointCloud<VelodynePointXYZIR>::Ptr pc_out(new pcl::PointCloud<VelodynePointXYZIR>());
        for (const auto& pt : pc_in->points) {
            if (has_nan(pt)) continue;
            VelodynePointXYZIR vpt;
            vpt.x = pt.x;
            vpt.y = pt.y;
            vpt.z = pt.z;
            vpt.intensity = pt.intensity;
            vpt.ring = pt.ring;
            pc_out->points.push_back(vpt);
        }
        pc_out->width = pc_out->points.size();
        pc_out->height = 1;
        pc_out->is_dense = true;
        publish_points(pc_out, pc_msg);
    } else if (output_type == "XYZI") {
        pcl::PointCloud<pcl::PointXYZI>::Ptr pc_out(new pcl::PointCloud<pcl::PointXYZI>());
        for (const auto& pt : pc_in->points) {
            if (has_nan(pt)) continue;
            pcl::PointXYZI vpt;
            vpt.x = pt.x;
            vpt.y = pt.y;
            vpt.z = pt.z;
            vpt.intensity = pt.intensity;
            pc_out->points.push_back(vpt);
        }
        pc_out->width = pc_out->points.size();
        pc_out->height = 1;
        pc_out->is_dense = true;
        publish_points(pc_out, pc_msg);
    }
}

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<rclcpp::Node>("rs_converter");

    if (argc < 3) {
        RCLCPP_ERROR(node->get_logger(),
                "Please specify input pointcloud type( XYZI or XYZIRT) and output pointcloud type(XYZI, XYZIR, XYZIRT)!!!");
        return 1;
    } else {
        output_type = argv[2];

        if (std::strcmp("XYZI", argv[1]) == 0) {
            subscriber_robosense_points = node->create_subscription<sensor_msgs::msg::PointCloud2>(
                "/rslidar_points", 1,
                rsHandler_XYZI
            );
        } else if (std::strcmp("XYZIRT", argv[1]) == 0) {
            subscriber_robosense_points = node->create_subscription<sensor_msgs::msg::PointCloud2>(
                "/rslidar_points", 1,
                rsHandler_XYZIRT
            );
        } else {
            RCLCPP_ERROR(node->get_logger(), argv[1]);
            RCLCPP_ERROR(node->get_logger(), "Unsupported input pointcloud type. Currently only support XYZI and XYZIRT.");
            return 1;
        }
    }
    publisher_velodyne_points = node->create_publisher<sensor_msgs::msg::PointCloud2>("/velodyne_points", 1);

    RCLCPP_INFO(node->get_logger(), "Listening to /rslidar_points ......");
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
