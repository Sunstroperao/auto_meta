#include "carla_zww_stanley_pid_controller/vehicle_longitudinal_controller_pid.h"

#include <math.h>

#include <fstream>

#include "carla_zww_stanley_pid_controller/common.h"

using namespace std;

static const rclcpp::Logger LOGGER = rclcpp::get_logger("carla_zww_stanley_pid_controller_publisher");


VehicleControlPublisher::VehicleControlPublisher()
    : Node("carla_zww_stanley_pid_controller")
/*'''**************************************************************************************
- FunctionName: None
- Function    : None
- Inputs      : None
- Outputs     : None
- Comments    : None
**************************************************************************************'''*/
{
    first_record_ = false;
    V_set_ = 5.0; // m/s
    wheelbase_ = 2.852;
    car_length_ = 2.852;

    pid_controller_longitudinal = std::make_unique<zww::control::PIDController>(0.16, 0.02, 0.01);
    stanely_controller_lateral = std::make_unique<zww::control::StanleyController>();
    stanely_controller_lateral->LoadControlConf();

    
    V_set_ = 30;    // km/h
    first_record_ = true;
    cnt = 0;
    qos = 10;

    localization_data_subscriber = this->create_subscription<nav_msgs::msg::Odometry>("/carla/ego_vehicle/odometry", qos, std::bind(&VehicleControlPublisher::odomCallback, this, _1));
    // localization_data_subscriber = this->create_subscription<nav_msgs::msg::Odometry>("ins_d_of_vehicle_pose", 10, std::bind(&VehicleControlPublisher::odomCallback, this, _1)); // nezha


    vehicle_control_publisher = this->create_publisher<carla_msgs::msg::CarlaEgoVehicleControl>("/carla/ego_vehicle/vehicle_control_cmd", qos);
    control_cmd.header.stamp = this->now();
    control_cmd.gear = 1;
    control_cmd.manual_gear_shift = false;
    control_cmd.reverse = false;
    control_cmd.hand_brake = false;

    auto time_node_start = this->now();
    vehicle_control_target_velocity_publisher = this->create_publisher<carla_msgs::msg::CarlaVehicleTargetVelocity>("/carla/ego_vehicle/target_velocity", qos);
    vehicle_control_target_velocity.header.stamp = this->now();
    vehicle_control_target_velocity.velocity = 0.0;

    carla_status_subscriber = this->create_subscription<carla_msgs::msg::CarlaEgoVehicleStatus>("/carla/ego_vehicle/vehicle_status", qos, std::bind(&VehicleControlPublisher::VehicleStatusCallback, this, _1));

    global_path_publisher_ = this->create_publisher<nav_msgs::msg::Path>("/global_reference_path", 2);

    history_path_visualization_publisher = this->create_publisher<nav_msgs::msg::Path>("/history_path", 2);

    // Initialize the transform broadcaster
    tf_broadcaster_gps_vehicle = std::make_unique<tf2_ros::TransformBroadcaster>(*this); 

    vehicle_control_iteration_timer = this->create_wall_timer(10ms, std::bind(&VehicleControlPublisher::VehicleControlIterationCallback, this));
    global_path_publish_timer = this->create_wall_timer(500ms, std::bind(&VehicleControlPublisher::GlobalPathPublishCallback, this)); 


    // 读取参考线路径
    std::ifstream infile("src/carla_ros_bridge/carla_zww_projects/carla_zww_stanley_pid_controller/data/2022_09_29_16_27_08_ins_data_map_after_preprocess.csv", ios::in);    //将文件流对象与文件连接起来
    assert(infile.is_open());                                                                                                                      //若失败,则输出错误消息,并终止程序运行
    
    while (getline(infile, _line)) {
        std::cout << _line << std::endl;
        //解析每行的数据
        stringstream ss(_line);
        string _sub;
        vector<string> subArray;
        //按照逗号分隔
        while (getline(ss, _sub, ',')) {
            subArray.push_back(_sub);
        }
        double pt_x = std::atof(subArray[2].c_str());
        double pt_y = std::atof(subArray[3].c_str());
        // double pt_v = std::atof(subArray[6].c_str());
        
        v_points.push_back(20.0);
        xy_points.push_back(std::make_pair(pt_x, pt_y));
    }
    infile.close();

    // Construct the reference_line path profile
    std::vector<double> headings;
    std::vector<double> accumulated_s;
    std::vector<double> kappas;
    std::vector<double> dkappas;
    std::unique_ptr<zww::control::ReferenceLine> reference_line = std::make_unique<zww::control::ReferenceLine>(xy_points);
    reference_line->ComputePathProfile(&headings, &accumulated_s, &kappas, &dkappas);

    for (size_t i = 0; i < headings.size(); i++) {
        std::cout << "pt " << i << " heading: " << headings[i] << " acc_s: " << accumulated_s[i] << " kappa: " << kappas[i] << " dkappas: " << dkappas[i] << std::endl;
    }

    size_t _count_points = headings.size();
    size_t _stop_begin_point = ceil(_count_points * 0.85);
    size_t _stop_point = ceil(_count_points * 0.95);
    std::cout << "slow down points:" << _stop_begin_point << "  " << _stop_point << std::endl;

    int _index_before_stop = 0;
    for (size_t i = 0; i < headings.size(); i++) {
        TrajectoryPoint trajectory_pt;
        trajectory_pt.x = xy_points[i].first;
        trajectory_pt.y = xy_points[i].second;
        if (i < _stop_begin_point) {
            trajectory_pt.v = v_points[i];
            _index_before_stop ++;
        } else {
            if (trajectory_pt.v > 1.0) {
                trajectory_pt.v = v_points[_index_before_stop] * ((double)i / ((double)_stop_begin_point - (double)_stop_point) - (double)_stop_point / ((double)_stop_begin_point - (double)_stop_point));
            } else {
                trajectory_pt.v = 0;
            }
        }
        trajectory_pt.a = 0.0;
        trajectory_pt.heading = headings[i];
        trajectory_pt.kappa = kappas[i];

        planning_published_trajectory.trajectory_points.push_back(trajectory_pt);

        this_pose_stamped.header.frame_id = "gps";             
        this_pose_stamped.header.stamp = this->get_clock()->now();
        this_pose_stamped.pose.position.x = xy_points[i].first;
        this_pose_stamped.pose.position.y = xy_points[i].second;
        this_pose_stamped.pose.position.z = 0;
        this_pose_stamped.pose.orientation.x = 0;
        this_pose_stamped.pose.orientation.y = 0;
        this_pose_stamped.pose.orientation.z = 0;
        this_pose_stamped.pose.orientation.w = 0; // 这里实际上是放的frenet坐标系的S
        
        global_path.poses.push_back(this_pose_stamped);
        global_path.header.frame_id = "gps";
    }

    goal_point = planning_published_trajectory.trajectory_points.back();

    trajectory_points_ = planning_published_trajectory.trajectory_points;

    acceleration_cmd = 0.0;
    steer_cmd = 0.0;
}

VehicleControlPublisher::~VehicleControlPublisher() {}
/*'''**************************************************************************************
- FunctionName: None
- Function    : None
- Inputs      : None
- Outputs     : None
- Comments    : None
**************************************************************************************'''*/

void VehicleControlPublisher::VehicleStatusCallback(carla_msgs::msg::CarlaEgoVehicleStatus::SharedPtr msg)
/*'''**************************************************************************************
- FunctionName: None
- Function    : None
- Inputs      : None
- Outputs     : None
- Comments    : 为了在rqt里面，一个plot里面查看目标速度和实际速度，需要两个速度有关的消息都使用
**************************************************************************************'''*/
{
    vehicle_control_target_velocity.header.stamp = msg->header.stamp;
}

double VehicleControlPublisher::PointDistanceSquare(const TrajectoryPoint &point, const double x, const double y)
/*'''**************************************************************************************
- FunctionName: None
- Function    : 两点之间的距离
- Inputs      : None
- Outputs     : None
- Comments    : None
**************************************************************************************'''*/
{
    double dx = point.x - x;
    double dy = point.y - y;
    return dx * dx + dy * dy;
}

TrajectoryPoint VehicleControlPublisher::QueryNearestPointByPosition(const double x, const double y)
/*'''**************************************************************************************
- FunctionName: None
- Function    : None
- Inputs      : None
- Outputs     : None
- Comments    : None
**************************************************************************************'''*/
{
    
    double d_min = PointDistanceSquare(trajectory_points_.front(), x, y);
    size_t index_min = 0;

    for (size_t i = 1; i < trajectory_points_.size(); ++i) {
        double d_temp = PointDistanceSquare(trajectory_points_[i], x, y);
        if (d_temp < d_min) {
            d_min = d_temp;
            index_min = i;
        }
    }
    // cout << "vehicle.x: " << x << " "
    //      << "vehicle.y: " << y << endl;
    // cout << "trajectory_points.x: " << trajectory_points_[index_min].x << " "
    //      << "trajectory_points.y: " << trajectory_points_[index_min].y;

    // cout << endl;

    return trajectory_points_[index_min];
}

void VehicleControlPublisher::odomCallback(nav_msgs::msg::Odometry::SharedPtr msg)
/*'''**************************************************************************************
- FunctionName: None
- Function    : None
- Inputs      : None
- Outputs     : None
- Comments    : None
**************************************************************************************'''*/
{
    // RCLCPP_INFO(this->get_logger(), "I heard: [%f]", msg->pose.pose.position.x);
    tf2::Quaternion quat_tf;
    tf2::convert(msg->pose.pose.orientation, quat_tf);
    tf2::Matrix3x3(quat_tf).getRPY(vehicle_state_.roll, vehicle_state_.pitch, vehicle_state_.yaw);

    if (first_record_) {
        vehicle_state_.start_point_x = msg->pose.pose.position.x;
        vehicle_state_.start_point_y = msg->pose.pose.position.y;
        vehicle_state_.start_heading = -M_PI / 2;
        first_record_ = false;
    }
    vehicle_state_.x = msg->pose.pose.position.x;
    vehicle_state_.y = msg->pose.pose.position.y;
    vehicle_state_.vx = msg->twist.twist.linear.x;
    vehicle_state_.vy = msg->twist.twist.linear.y;
    vehicle_state_.vz = msg->twist.twist.linear.z;
    vehicle_state_.v = std::sqrt(vehicle_state_.vx * vehicle_state_.vx + vehicle_state_.vy * vehicle_state_.vy + vehicle_state_.vz * vehicle_state_.vz) * 3.6;    // 本车速度
    vehicle_state_.heading = vehicle_state_.yaw; 
    
    /* 将收到的定位信息发布出来,在rviz里显示历史轨迹 */
    history_path.header.stamp = this->get_clock()->now();
    history_path.header.frame_id = "gps";

    history_path_points.header.stamp = this->get_clock()->now();
    history_path_points.header.frame_id = "gps";
    history_path_points.pose.position.x = vehicle_state_.x;
    history_path_points.pose.position.y = vehicle_state_.y;
    history_path_points.pose.position.z = 0;
    history_path_points.pose.orientation = msg->pose.pose.orientation;
    history_path.poses.push_back(history_path_points);

    if (history_path.poses.size() > 2000)
    {
        vector<geometry_msgs::msg::PoseStamped>::iterator k = history_path.poses.begin();
        history_path.poses.erase(k);
    }

    history_path_visualization_publisher->publish(history_path);

    // 将世界坐标系和车辆坐标系的位置关系广播出来
    geometry_msgs::msg::TransformStamped transformStamped;
    transformStamped.header.stamp = this->get_clock()->now();
    transformStamped.header.frame_id = "gps";
    transformStamped.child_frame_id = "vehicle_odometry";
    transformStamped.transform.translation.x = msg->pose.pose.position.x;
    transformStamped.transform.translation.y = msg->pose.pose.position.y;
    transformStamped.transform.translation.z = msg->pose.pose.position.z;

    transformStamped.transform.rotation.x = quat_tf.x();
    transformStamped.transform.rotation.y = quat_tf.y();
    transformStamped.transform.rotation.z = quat_tf.z();
    transformStamped.transform.rotation.w = quat_tf.w();

    tf_broadcaster_gps_vehicle->sendTransform(transformStamped);                                                                                                                 // pose.orientation是四元数
}

// void VehicleControlPublisher::VehicleControlIterationCallback(carla_msgs::msg::CarlaStatus::SharedPtr msg)
void VehicleControlPublisher::VehicleControlIterationCallback()
/*'''**************************************************************************************
- FunctionName: None
- Function    : None
- Inputs      : None
- Outputs     : None
- Comments    : None
**************************************************************************************'''*/
{
    TrajectoryPoint target_point_;

    target_point_ = this->QueryNearestPointByPosition(vehicle_state_.x, vehicle_state_.y);

    double v_err = target_point_.v - vehicle_state_.v;                  // 速度误差
    double yaw_err = vehicle_state_.heading - target_point_.heading;    // 横摆角误差

    // if (yaw_err > M_PI / 6) {
        // yaw_err = M_PI / 6;
    // } else if (yaw_err < -M_PI / 6) {
        // yaw_err = -M_PI / 6;
    // }

    stanely_controller_lateral->ComputeControlCmd(this->vehicle_state_, this->planning_published_trajectory, cmd);

    if (cnt % 1 == 0) {
        // cout << "start_heading: " << vehicle_state_.start_heading << endl;
        // cout << "heading: " << vehicle_state_.heading << endl;
        cout << "~~ vehicle_state_.v: " << vehicle_state_.v * 3.6 << ", target_point_.v: " << target_point_.v << ", v_err: " << v_err << endl;
        cout << "yaw_err: " << yaw_err << endl;
    }

    double acceleration_cmd = pid_controller_longitudinal->Control(v_err, 0.01);

    // acceleration_cmd = speed_pid_controller.Control(v_err, 0.05);
    // steer_cmd = yaw_pid_controller.Control(yaw_err, 0.01);
    // steer_cmd = 0;

    control_cmd.header.stamp = this->now();

    if (acceleration_cmd >= 1.0) {
        acceleration_cmd = 1.0;
    }
    if (acceleration_cmd <= -1) {
        acceleration_cmd = -1.0;
    }

    if (acceleration_cmd <= 0) {
        control_cmd.brake = -acceleration_cmd;
        control_cmd.throttle = 0;
    } else {
        control_cmd.throttle = acceleration_cmd;
        control_cmd.brake = 0;
    }
    // std::cout << "acceleration_cmd: " << acceleration_cmd << std::endl;
    control_cmd.steer = cmd.steer_target;
    control_cmd.gear = 1;
    control_cmd.reverse = false;
    control_cmd.hand_brake = false;
    control_cmd.manual_gear_shift = false;
    vehicle_control_publisher->publish(control_cmd);

    vehicle_control_target_velocity.velocity = target_point_.v / 3.6;
    vehicle_control_target_velocity_publisher->publish(vehicle_control_target_velocity);
    cnt++;
}

void VehicleControlPublisher::GlobalPathPublishCallback()
{   
    global_path.header.stamp = this->get_clock()->now();
    global_path_publisher_->publish(global_path);
} 

int main(int argc, char **argv)
/*'''**************************************************************************************
- FunctionName: None
- Function    : None
- Inputs      : None
- Outputs     : None
- Comments    : None
**************************************************************************************'''*/
{
    RCLCPP_INFO(LOGGER, "Initialize node");

    rclcpp::init(argc, argv);

    auto n = std::make_shared<VehicleControlPublisher>();

    rclcpp::spin(n);

    rclcpp::shutdown();

    return 0;
}
