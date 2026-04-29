#pragma once

#include "math.hpp"

#include <SDL3/SDL.h>

#include <array>
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
  proj[2][2] = 0.0f;  // The magic 0.0 that pushes the far plane to infinity
  proj[2][3] = -1.0f; // Right-handed W-divide (w = -z)
  proj[3][2] = zNear; // Maps the near plane to exactly 1.0

  return proj;
}

static glm::mat4 createReversedZOrthographic(float width, float height, float zNear, float zFar)
{
  glm::mat4 proj{1.0f};

  proj[0][0] = 2.0f / width;
  proj[1][1] = 2.0f / height;
  proj[2][2] = 1.0f / (zFar - zNear);
  proj[3][2] = zFar / (zFar - zNear);

  return proj;
}
} // namespace

namespace flb
{

struct Plane
{
  glm::dvec3 normal;
  double distance;
};

using Frustum = std::array<Plane, 5>;

static constexpr Plane createPlane(const glm::dvec3& normal, const glm::dvec3& point)
{
  Plane plane;
  plane.normal = normal;
  plane.distance = -glm::dot(normal, point);
  return plane;
}

enum class CameraMode
{
  Perspective,
  Orthographic,
};

class Camera
{
public:
  virtual ~Camera() = default;

  glm::dvec3 position{0.0};

  float aspect{1.0f};
  float near{1.0f};

  float yaw{0.0f};
  glm::vec3 up{0.0f, 0.0f, 1.0f}; // Default to ECEF Z up

  double speed = 100.0;

  virtual glm::mat4 calcProjMat() const = 0;
  virtual void updateKeyboard(float dt, const bool* keyStates) = 0;
  virtual void updateMouse(float deltaX, float deltaY) { }
  virtual void zoom(float amount) = 0;
  virtual glm::vec3 getFront() const = 0;
  virtual glm::vec3 getRight() const = 0;
  virtual Frustum createFrustum() const = 0;

  virtual glm::vec3 getViewUp() const { return up; }

  void copyCommonStateFrom(const Camera& camera)
  {
    position = camera.position;
    aspect = camera.aspect;
    near = camera.near;
    yaw = camera.yaw;
    up = camera.up;
    speed = camera.speed;
  }

  glm::mat4 getViewProjMat() const { return calcProjMat() * calcViewMat(); }

protected:
  glm::vec3 getSurfaceForward() const
  {
    // 1. Determine a stable 'reference forward' tangent to the surface
    // Using ECEF convention where (0,0,1) is the North Pole.
    glm::vec3 worldNorth(0.0f, 0.0f, 1.0f);
    glm::vec3 surfaceUp = glm::normalize(up);

    // If we're exactly at the poles, pick an arbitrary alternative
    if (glm::abs(glm::dot(surfaceUp, worldNorth)) > 0.999f)
    {
      worldNorth = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // Reference forward points 'North' along the surface
    glm::vec3 refForward = glm::normalize(worldNorth - surfaceUp * glm::dot(worldNorth, surfaceUp));

    // 2. Rotate by 'yaw' around the surface normal ('up' vector)
    glm::quat yawRot = glm::angleAxis(yaw, surfaceUp);
    return glm::normalize(yawRot * refForward);
  }

  glm::vec3 getSurfaceRight() const { return glm::normalize(glm::cross(getSurfaceForward(), glm::normalize(up))); }

  glm::mat4 calcViewMat() const
  {
    // View matrix guarantees 'up' is fixed mathematically to eliminate any
    // possible roll.
    return glm::lookAt(glm::vec3{0.0f}, getFront(), getViewUp());
  }
};

/**
 * Designed to be used with floating origin where world is rendered relative to
 * the camera's position.
 */
class PerspectiveCamera: public Camera
{
public:
  float fov{75.0f};
  float pitch{0.0f};
  float sensitivity = 0.006f;

  glm::mat4 calcProjMat() const override { return createInfiniteReversedZ(glm::radians(fov), aspect, near); }

  void updateMouse(float deltaX, float deltaY) override
  {
    yaw -= deltaX * sensitivity;
    pitch -= deltaY * sensitivity;

    // Clamp the pitch to prevent flipping straight up/down
    pitch = glm::clamp(pitch, glm::radians(-89.0f), glm::radians(89.0f));
  }

  void zoom(float amount) override
  {
    if (amount == 0.0f || glm::dot(position, position) == 0.0)
    {
      return;
    }

    const ECEFCoords surfacePosition = projectToEllipsoidSurface(position);
    const glm::vec3 surfaceUp = getSurfaceNormal(surfacePosition);
    const double currentAltitude = glm::dot(position - surfacePosition, glm::dvec3(surfaceUp));
    constexpr double minAltitude = 10.0;
    constexpr double initialZoomOutAltitude = 1000.0;

    if (amount > 0.0f && currentAltitude <= minAltitude)
    {
      position = surfacePosition + glm::dvec3(surfaceUp) * minAltitude;
      up = surfaceUp;
      return;
    }

    const double startAltitude =
      currentAltitude <= minAltitude ? initialZoomOutAltitude : glm::max(currentAltitude, minAltitude);
    const double zoomFactor = glm::pow(1.2, glm::abs(static_cast<double>(amount)));
    const double nextAltitude = amount > 0.0f ? startAltitude / zoomFactor : startAltitude * zoomFactor;
    const double clampedAltitude = glm::clamp(nextAltitude, minAltitude, SEMI_MAJOR * 4.0);

    position = surfacePosition + glm::dvec3(surfaceUp) * clampedAltitude;
    up = surfaceUp;
  }

  void updateKeyboard(float dt, const bool* keyStates) override
  {
    const double moveSpeed = speed * static_cast<double>(dt);
    if (keyStates[SDL_SCANCODE_W])
      position += glm::dvec3(getFront()) * moveSpeed;
    if (keyStates[SDL_SCANCODE_A])
      position -= glm::dvec3(getRight()) * moveSpeed;
    if (keyStates[SDL_SCANCODE_S])
      position -= glm::dvec3(getFront()) * moveSpeed;
    if (keyStates[SDL_SCANCODE_D])
      position += glm::dvec3(getRight()) * moveSpeed;
  }

  glm::vec3 getFront() const override { return getPerspectiveFront(); }

  glm::vec3 getRight() const override { return glm::normalize(glm::cross(getFront(), up)); }

  Frustum createFrustum() const override
  {
    // 1. Compute orthogonal camera local axes
    glm::dvec3 front = getFront();
    glm::dvec3 right = getRight();
    // Since right and front are orthogonal unit vectors, their cross product is naturally a unit vector
    glm::dvec3 camUp = glm::cross(right, front);

    // 2. Compute half FOVs in radians
    double halfV = glm::radians(fov) * 0.5;
    double halfH = glm::atan(aspect * glm::tan(halfV));

    double cosV = glm::cos(halfV), sinV = glm::sin(halfV);
    double cosH = glm::cos(halfH), sinH = glm::sin(halfH);

    std::array<Plane, 5> planes;

    // 3. Near Plane (Normal points strictly forward)
    planes[0] = createPlane(front, position + front * static_cast<double>(near));

    // 4. Left & Right Planes (Normals point inward towards center)
    // We find a vector along the left/right edge, and cross it with camUp
    glm::dvec3 vLeft = front * cosH - right * sinH;
    glm::dvec3 vRight = front * cosH + right * sinH;

    // (Cross product of orthogonal unit vectors results in a unit vector; no normalize() needed!)
    planes[1] = createPlane(glm::cross(vLeft, camUp), position);
    planes[2] = createPlane(glm::cross(camUp, vRight), position);

    // 5. Bottom & Top Planes (Normals point inward towards center)
    glm::dvec3 vBottom = front * cosV - camUp * sinV;
    glm::dvec3 vTop = front * cosV + camUp * sinV;

    planes[3] = createPlane(glm::cross(right, vBottom), position);
    planes[4] = createPlane(glm::cross(vTop, right), position);

    return planes;
  }

private:
  glm::vec3 getPerspectiveFront() const
  {
    const glm::vec3 yawedForward = getSurfaceForward();

    // Find the right vector to pitch around
    glm::vec3 right = glm::normalize(glm::cross(yawedForward, up));

    // Apply pitch (up/down)
    glm::quat pitchRot = glm::angleAxis(pitch, right);
    return glm::normalize(pitchRot * yawedForward);
  }
};

class OrthographicCamera: public Camera
{
public:
  float orthographicHeight{12000.0f};
  float far{static_cast<float>(SEMI_MAJOR * 4.0)};
  double orthographicAltitude{5000.0};

  glm::mat4 calcProjMat() const override
  {
    return createReversedZOrthographic(orthographicHeight * aspect, orthographicHeight, near, far);
  }

  void resetFrom(const Camera& camera)
  {
    copyCommonStateFrom(camera);
    enterFromCurrentPosition();
  }

  void zoom(float amount) override
  {
    if (amount == 0.0f)
    {
      return;
    }

    const double zoomFactor = glm::pow(1.2, glm::abs(static_cast<double>(amount)));
    const double nextHeight =
      amount > 0.0f ? static_cast<double>(orthographicHeight) / zoomFactor : orthographicHeight * zoomFactor;
    orthographicHeight = static_cast<float>(glm::clamp(nextHeight, 100.0, SEMI_MAJOR * 2.0));

    const double nextAltitude = amount > 0.0f ? orthographicAltitude / zoomFactor : orthographicAltitude * zoomFactor;
    orthographicAltitude = glm::clamp(nextAltitude, 10.0, SEMI_MAJOR * 4.0);
    updateOrthographicPose();
  }

  void updateKeyboard(float dt, const bool* keyStates) override
  {
    const double moveSpeed = speed * static_cast<double>(dt);
    glm::dvec3 delta{0.0};

    const glm::dvec3 forward = getSurfaceForward();
    const glm::dvec3 right = getSurfaceRight();

    if (keyStates[SDL_SCANCODE_W])
      delta += forward * moveSpeed;
    if (keyStates[SDL_SCANCODE_A])
      delta -= right * moveSpeed;
    if (keyStates[SDL_SCANCODE_S])
      delta -= forward * moveSpeed;
    if (keyStates[SDL_SCANCODE_D])
      delta += right * moveSpeed;

    if (glm::dot(delta, delta) == 0.0)
    {
      return;
    }

    if (!hasOrthographicCenter)
    {
      orthographicCenter = projectToEllipsoidSurface(position);
      hasOrthographicCenter = true;
    }

    orthographicCenter = projectToEllipsoidSurface(orthographicCenter + delta);
    updateOrthographicPose();
  }

  glm::vec3 getFront() const override { return -glm::normalize(up); }

  glm::vec3 getRight() const override { return getSurfaceRight(); }

  glm::vec3 getViewUp() const override { return getSurfaceForward(); }

  float getOrthographicHalfHeight() const { return orthographicHeight * 0.5f; }
  float getOrthographicHalfWidth() const { return getOrthographicHalfHeight() * aspect; }

  Frustum createFrustum() const override
  {
    // 1. Compute orthogonal camera local axes
    glm::dvec3 front = getFront();
    glm::dvec3 right = getRight();
    // Since right and front are orthogonal unit vectors, their cross product is naturally a unit vector
    glm::dvec3 camUp = glm::cross(right, front);

    const double halfWidth = static_cast<double>(getOrthographicHalfWidth());
    const double halfHeight = static_cast<double>(getOrthographicHalfHeight());

    std::array<Plane, 5> planes;
    planes[0] = createPlane(front, position + front * static_cast<double>(near));
    planes[1] = createPlane(right, position - right * halfWidth);
    planes[2] = createPlane(-right, position + right * halfWidth);
    planes[3] = createPlane(camUp, position - camUp * halfHeight);
    planes[4] = createPlane(-camUp, position + camUp * halfHeight);

    return planes;
  }

private:
  glm::dvec3 orthographicCenter{0.0};
  bool hasOrthographicCenter{false};

  void enterFromCurrentPosition()
  {
    if (glm::dot(position, position) == 0.0)
    {
      return;
    }

    orthographicCenter = projectToEllipsoidSurface(position);
    hasOrthographicCenter = true;

    const glm::vec3 surfaceUp = getSurfaceNormal(orthographicCenter);
    const double currentAltitude = glm::dot(position - orthographicCenter, glm::dvec3(surfaceUp));
    if (currentAltitude > static_cast<double>(near))
    {
      orthographicAltitude = glm::max(orthographicAltitude, currentAltitude);
    }

    up = surfaceUp;
    updateOrthographicPose();
  }

  void updateOrthographicPose()
  {
    up = getSurfaceNormal(orthographicCenter);
    position = orthographicCenter + glm::dvec3(up) * orthographicAltitude;
  }
};

/**
 * Designed for orbital view around a center point.
 */
class OrbitalCamera: public Camera
{
public:
  float distance{10.0f};

  glm::mat4 getViewProjMat() const { return calcProjMat() * calcViewMat(); }

private:
  glm::mat4 calcViewMat() const { return glm::lookAt(-distance * getFront(), glm::vec3{0.0f}, up); }
};
} // namespace flb
