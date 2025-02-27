#ifndef MPC_CONTROLLER_H
#define MPC_CONTROLLER_H

#include <vector>
#include <chrono>     // 时间库
#include <string>
#include <memory>
#include <cstdio>
#include <thread>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <functional> // 函数模板库
#include <eigen3/Eigen/QR>
#include <eigen3/Eigen/Core>
#include <eigen3/unsupported/Eigen/Splines>

#include "rclcpp/rate.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/float32.hpp"
#include <visualization_msgs/msg/marker.hpp>

#include "chassis_msg/msg/wvcu_flt_cod.hpp"
#include "chassis_msg/msg/adu_body_cmd.hpp"
#include "chassis_msg/msg/adu_drive_cmd.hpp"
#include "chassis_msg/msg/wvcu_veh_sph_lim.hpp"
#include "chassis_msg/msg/adu_gear_request.hpp"
#include "chassis_msg/msg/wvcu_body_status.hpp"
#include "chassis_msg/msg/wvcu_horizontal_status.hpp"
#include "chassis_msg/msg/wvcu_longitudinal_status.hpp"

#include <math.h>
#include <tf2/convert.h>
#include <cppad/cppad.hpp>
#include <cppad/ipopt/solve.hpp>
#include <nav_msgs/msg/path.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <tf2/transform_datatypes.h>
#include <tf2/LinearMath/Quaternion.h>
#include <geometry_msgs/msg/quaternion.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/twist_stamped.hpp>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

using namespace std::chrono_literals; // 表示时间长度的命名空间
//调用嵌套空间std::chrono_literals下的函数
using std::cout;
using std::endl;
using std::vector;
using std::placeholders::_1;
using CppAD::AD;
using Eigen::VectorXd;

class mpc_controller
{
private:
    /*Indexes on the 1-D vector for readability，
    These indexes are used for "vars"*/
    size_t x_start;
    size_t y_start;
    size_t psi_start;
    size_t v_longitudinal_start;
    size_t v_lateral_start;
    size_t yaw_rate_start;
    size_t cte_start;
    size_t epsi_start;
    size_t front_wheel_angle_start;
    size_t longitudinal_acceleration_start; //控制时域长度为25的时候，控制量一共有24个。
    size_t front_wheel_angle_increment_start;
    size_t longitudinal_acceleration_increment_start;
    size_t Np;
    size_t Nc;
    double dt;
    double a_lateral;
public:
    mpc_controller(/* args */);
    virtual ~mpc_controller();

    // Solve the model given an initial state and polynomial coefficients
    // Retrun the first actuators
    std::vector<double> Solve(const Eigen::VectorXd &state,
                              const Eigen::VectorXd &coeffs,
                              const double &target_v,
                              const int &cte_weight,
                              const int &epsi_weight,
                              const int &v_weight,
                              const int &steer_actuator_cost_weight,
                              const int &acc_actuator_cost_weight,
                              const int &change_steer_cost_weight,
                              const int &change_accel_cost_weight,
                              const int &mpc_control_horizon_length,
                              const double &mpc_control_step_length,
                              const double &kinamatic_para_Lf,
                              const double &a_lateral,
                              const double &steer_ratio);
};

class FG_eval
{
public:
    /*Define weights for different terms of objective*/
    double cte_weight;
    double epsi_weight;
    double v_weight;
    double steer_actuator_cost_weight_fg;
    double acc_actuator_cost_weight_fg;
    double change_steer_cost_weight;
    double change_accel_cost_weight;
    double target_v; // km/h
    size_t mpc_prediction_horizon_length;
    double mpc_control_step_length;
    double kinamatic_para_Lf;
    /*Reference values that will be include in objective function*/
    double ref_cte = 0;
    double ref_epsi = 0;
    double ref_v; // m/s
    /*Set the timestep length and duration*/
    size_t Np;
    size_t Nc;
    double dt;
    double a_lateral;
    double steer_ratio;

    /*
    This value assumes the model presented in the classroom is used.
    It was obtained by measuring the radius formed by running the vehicle in the
    simulator around in a circle with a constant steering angle and velocity on
    a flat terrain.
    Lf was tuned until the the radius formed by the simulating the model
    presented in the classroom matched the previous radius.
    This is the length from front to CoG that has a similar radius.
    */
    double Lf;

    /*Indexes on the 1-D vector for readability，
    These indexes are used for "vars"*/
    size_t x_start;
    size_t y_start;
    size_t psi_start;
    size_t v_longitudinal_start;
    size_t v_lateral_start;
    size_t yaw_rate_start;
    size_t cte_start;
    size_t epsi_start;
    size_t front_wheel_angle_start;
    size_t longitudinal_acceleration_start; //控制时域长度为25的时候，控制量一共有24个。

    size_t front_wheel_angle_increment_start; 
    size_t longitudinal_acceleration_increment_start;

    // AD<double> Ccf = 46900; // 前轮轮胎侧偏刚度
    // AD<double> Ccr = 42700; // 后轮轮胎侧偏刚度
    AD<double> Cf = 46000; // 前轮轮胎纵向刚度
    AD<double> Cr = 46000; // 后轮轮胎纵向刚度
    // AD<double> Sf = 0.2;    // 前轮滑移率
    // AD<double> Sr = 0.0;    // 后轮滑移率
    AD<double> m = 1660;
    // AD<double> v_lateral;
    // AD<double> v_longitudinal;
    AD<double> l = 2.77;
    AD<double> I = 3253;
    AD<double> lf = 1.4;
    AD<double> lr = 1.6 - lf;

    double x;
    double y;
    double psi;
    double v_longitudinal;
    double v_lateral;
    double yaw_rate;
    double cte;
    double epsi;

public:
    VectorXd coeffs; // Fitted polynomial coefficients
    typedef CPPAD_TESTVECTOR(AD<double>) ADvector;

public:
    /* coeffs are the coefficients of the fitted polynomial, will be used by the cross track error and heading error equations.*/
    FG_eval(const Eigen::VectorXd &state,
            VectorXd coeffs,
            const double &target_v,
            const int &cte_weight,
            const int &epsi_weight,
            const int &v_weight,
            const int &steer_actuator_cost_weight_fg,
            const int &acc_actuator_cost_weight,
            const int &change_steer_cost_weight,
            const int &change_accel_cost_weight,
            const int &mpc_prediction_horizon_length,
            const int &mpc_control_horizon_length,
            const double &mpc_control_step_length,
            const double &kinamatic_para_Lf,
            const double &a_lateral,
            const double &steer_ratio);
    ~FG_eval();
    void operator()(ADvector &fg, ADvector &vars);
};



#endif
