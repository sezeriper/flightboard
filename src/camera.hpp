#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
static glm::mat4 createInfiniteReversedZ(float fovRadians, float aspectRatio, float zNear)
{
    float f = 1.0f / glm::tan(fovRadians / 2.0f);

    glm::mat4 proj{0.0f};

    proj[0][0] = f / aspectRatio;
    proj[1][1] = f;
    proj[2][2] = 0.0f;      // The magic 0.0 that pushes the far plane to infinity
    proj[2][3] = -1.0f;     // Right-handed W-divide (w = -z)
    proj[3][2] = zNear;     // Maps the near plane to exactly 1.0

    return proj;
}
}
namespace flb {

class Camera {
public:
  float fov{75.0f};
  float aspect{1.0f};
  float near{1.0f};
  float yaw{glm::radians(-90.0f)};
  float pitch{0.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
protected:

  glm::mat4 calcProjMat() const {
    return createInfiniteReversedZ(glm::radians(fov), aspect, near);
  }

  glm::vec3 getFront() const {
    return glm::vec3(
      glm::cos(yaw) * glm::cos(pitch),
      glm::sin(pitch),
      glm::sin(yaw) * glm::cos(pitch)
    );
  }

  glm::vec3 getRight() const {
    return glm::normalize(glm::cross(getFront(), up));
  }

  glm::vec3 getUp() const {
    return glm::normalize(glm::cross(getRight(), getFront()));
  }
};

/**
 * Designed to be used with floating origin
 * where world is rendered relative to the camera's position.
 * Because of this position is stored as double and view matrix is fixed at origin.
 * Camea stays still and world moves around it, so we don't lose precision when far from the origin.
 */
class FpsCamera : public Camera {
public:
  glm::dvec3 pos{0.0};

  glm::mat4 getViewProjMat() const {
    return calcProjMat() * calcViewMat();
  }

private:
  glm::mat4 calcViewMat() const {
    auto front = getFront();
    auto up = getUp();
    return glm::lookAt(glm::vec3{0.0f}, front, up);
  }
};

/**
 * Designed to be used with floating origin
 * where world is rendered relative to the camera's position.
 * Because of this position is stored as double and view matrix is fixed at origin.
 * Camea stays still and world moves around it, so we don't lose precision when far from the origin.
 */
class OrbitalCamera : public Camera {
public:
  glm::dvec3 center{0.0f};
  float distance{10.0f};

  glm::mat4 getViewProjMat() const {
    return calcProjMat() * calcViewMat();
  }

private:
  glm::mat4 calcViewMat() const {
    auto front = getFront();
    auto up = getUp();
    return glm::lookAt(-distance * front, glm::vec3{0.0f}, up);
  }
};
}
