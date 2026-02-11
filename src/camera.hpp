#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace flb {
static const glm::vec3 UP{0.0f, 1.0f, 0.0f};

class Camera {
public:
  float fov{75.0f};
  float aspect{1.0f};
  float near{0.01f};
  float far{1000.0f};
  float yaw{glm::radians(-90.0f)};
  float pitch{0.0f};
protected:

  glm::mat4 calcProjMat() const {
    return glm::perspective(glm::radians(fov), aspect, near, far);
  }

  glm::vec3 getFront() const {
    return glm::vec3(
      glm::cos(yaw) * glm::cos(pitch),
      glm::sin(pitch),
      glm::sin(yaw) * glm::cos(pitch)
    );
  }

  glm::vec3 getRight() const {
    return glm::normalize(glm::cross(getFront(), UP));
  }

  glm::vec3 getUp() const {
    return glm::normalize(glm::cross(getRight(), getFront()));
  }
};

class FpsCamera : public Camera {
public:
  glm::vec3 pos{0.0f};

  glm::mat4 getViewProjMat() const {
    return calcProjMat() * calcViewMat();
  }

private:
  glm::mat4 calcViewMat() const {
    auto front = getFront();
    auto up = getUp();
    return glm::lookAt(pos, pos + front, up);
  }
};

class OrbitalCamera : public Camera {
public:
  glm::vec3 center{0.0f};
  float distance{10.0f};

  glm::mat4 getViewProjMat() const {
    return calcProjMat() * calcViewMat();
  }

private:
  glm::mat4 calcViewMat() const {
    auto front = getFront();
    auto up = getUp();
    return glm::lookAt(center - distance * front, center, up);
  }
};
};
