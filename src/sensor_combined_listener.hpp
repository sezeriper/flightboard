#pragma once
#include <px4_msgs/msg/vehicle_global_position.hpp>
#include <rclcpp/rclcpp.hpp>

#include <glm/glm.hpp>

/**
 * @brief Sensor Combined uORB topic data callback
 */
class SensorCombinedListener: public rclcpp::Node
{
public:
  explicit SensorCombinedListener() : Node("sensor_combined_listener")
  {
    rmw_qos_profile_t qos_profile = rmw_qos_profile_sensor_data;
    auto qos = rclcpp::QoS(rclcpp::QoSInitialization(qos_profile.history, 5), qos_profile);

    subGlobalPos = this->create_subscription<px4_msgs::msg::VehicleGlobalPosition>(
      "fmu/out/vehicle_global_position",
      qos,
      [this](const px4_msgs::msg::VehicleGlobalPosition::UniquePtr msg)
      {
        vehicleLat = msg->lat;
        vehicleLon = msg->lon;
        vehicleAlt = msg->alt;
      });
  }

  double getVehicleLat() const { return vehicleLat; }
  double getVehicleLon() const { return vehicleLon; }
  double getVehicleAlt() const { return vehicleAlt; }

private:
  double vehicleLat;
  double vehicleLon;
  double vehicleAlt;
  rclcpp::Subscription<px4_msgs::msg::VehicleGlobalPosition>::SharedPtr subGlobalPos;
};
