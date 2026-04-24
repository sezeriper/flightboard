#pragma once

#include "sensor_combined_listener.hpp"

#include <rclcpp/rclcpp.hpp>

namespace flb
{
class ROS
{
public:
  void init();
  void cleanup();
  void update();

  glm::dvec3 getVehicleCoords() const;

private:
  std::shared_ptr<SensorCombinedListener> node = nullptr;
};
} // namespace flb
