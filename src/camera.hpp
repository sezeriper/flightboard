#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace flb {
static const glm::vec3 UP{0.0f, 1.0f, 0.0f};

struct Camera {
public:
  glm::mat4 getViewProjMat() const {
    return calcProjMat() * calcViewMat();
  }

  float fov{75.0f};
  float aspect{1.0f};
  float near{0.01f};
  float far{1000.0f};

  glm::vec3 pos{0.0f};
  float yaw{glm::radians(-90.0f)};
  float pitch{0.0f};


private:
  glm::mat4 calcProjMat() const {
    return glm::perspective(glm::radians(fov), aspect, near, far);
  }

  glm::mat4 calcViewMat() const {
    auto front = glm::vec3(
      glm::cos(yaw) * glm::cos(pitch),
      glm::sin(pitch),
      glm::sin(yaw) * glm::cos(pitch)
    );

    auto right = glm::normalize(glm::cross(front, UP));
    auto up = glm::normalize(glm::cross(right, front));

    return glm::lookAt(pos, pos + front, up);
  }
};
} // namespace flb
