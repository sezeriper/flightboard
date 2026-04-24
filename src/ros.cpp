#include "ros.hpp"
#include "sensor_combined_listener.hpp"

#include <memory>

namespace flb
{
void ROS::init()
{
  rclcpp::init(0, nullptr);
  node = std::make_shared<SensorCombinedListener>();
}

void ROS::update() { rclcpp::spin_some(node); }

void ROS::cleanup() { rclcpp::shutdown(); }

glm::dvec3 ROS::getVehicleCoords() const
{
  return {node->getVehicleLat(), node->getVehicleLon(), node->getVehicleAlt()};
}
} // namespace flb
