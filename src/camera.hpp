#pragma once

#include <SDL3/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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

  float yaw{0.0f};
  float pitch{0.0f};
  glm::vec3 up{0.0f, 0.0f, 1.0f}; // Default to ECEF Z up

protected:

  glm::mat4 calcProjMat() const {
    return createInfiniteReversedZ(glm::radians(fov), aspect, near);
  }

  glm::vec3 getFront() const {
    // 1. Determine a stable 'reference forward' tangent to the surface
    // Using ECEF convention where (0,0,1) is the North Pole.
    glm::vec3 worldNorth(0.0f, 0.0f, 1.0f);

    // If we're exactly at the poles, pick an arbitrary alternative
    if (glm::abs(glm::dot(up, worldNorth)) > 0.999f) {
      worldNorth = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // Reference forward points 'North' along the surface
    glm::vec3 refForward = glm::normalize(worldNorth - up * glm::dot(worldNorth, up));

    // 2. Rotate by 'yaw' around the surface normal ('up' vector)
    glm::quat yawRot = glm::angleAxis(yaw, up);
    glm::vec3 yawedForward = yawRot * refForward;

    // 3. Find the right vector to pitch around
    glm::vec3 right = glm::normalize(glm::cross(yawedForward, up));

    // 4. Apply pitch (up/down)
    glm::quat pitchRot = glm::angleAxis(pitch, right);
    return pitchRot * yawedForward;
  }

  glm::vec3 getRight() const {
    return glm::normalize(glm::cross(getFront(), up));
  }
};

/**
 * Designed to be used with floating origin where world is rendered relative to the camera's position.
 */
class FPSCamera : public Camera {
public:
  glm::dvec3 position{0.0};
  double speed = 100.0;
  float sensitivity = 0.006f;

  glm::mat4 getViewProjMat() const {
    return calcProjMat() * calcViewMat();
  }

  void updateMouse(float deltaX, float deltaY) {
    yaw -= deltaX * sensitivity;
    pitch -= deltaY * sensitivity;

    // Clamp the pitch to prevent flipping straight up/down
    pitch = glm::clamp(pitch, glm::radians(-89.0f), glm::radians(89.0f));
  }

  void updateKeyboard(float dt, const bool* keyStates) {
    double moveSpeed = speed * static_cast<double>(dt);
    if (keyStates[SDL_SCANCODE_W]) position += glm::dvec3(getFront()) * moveSpeed;
    if (keyStates[SDL_SCANCODE_A]) position -= glm::dvec3(getRight()) * moveSpeed;
    if (keyStates[SDL_SCANCODE_S]) position -= glm::dvec3(getFront()) * moveSpeed;
    if (keyStates[SDL_SCANCODE_D]) position += glm::dvec3(getRight()) * moveSpeed;
  }

private:
  glm::mat4 calcViewMat() const {
    // View matrix guarantees 'up' is fixed mathematically to eliminate any possible roll.
    return glm::lookAt(glm::vec3{0.0f}, getFront(), up);
  }
};

/**
 * Designed for orbital view around a center point.
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
    return glm::lookAt(-distance * getFront(), glm::vec3{0.0f}, up);
  }
};
}
