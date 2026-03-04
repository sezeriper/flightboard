#pragma once

#include "camera.hpp"
#include "math.hpp"

namespace flb
{

struct Plane
{
  glm::dvec3 normal;
  double distance;
};

static constexpr Plane createPlane(const glm::dvec3& normal, const glm::dvec3& point)
{
  Plane plane;
  plane.normal = normal;
  plane.distance = -glm::dot(normal, point);
  return plane;
}

using Frustum = std::array<Plane, 5>;

static constexpr Frustum createFrustum(const Camera& camera)
{
  // 1. Compute orthogonal camera local axes
  glm::dvec3 front = camera.getFront();
  glm::dvec3 right = camera.getRight();
  // Since right and front are orthogonal unit vectors, their cross product is naturally a unit vector
  glm::dvec3 camUp = glm::cross(right, front);

  // 2. Compute half FOVs in radians
  double halfV = glm::radians(camera.fov) * 0.5;
  double halfH = glm::atan(camera.aspect * glm::tan(halfV));

  double cosV = glm::cos(halfV), sinV = glm::sin(halfV);
  double cosH = glm::cos(halfH), sinH = glm::sin(halfH);

  std::array<Plane, 5> planes;

  // 3. Near Plane (Normal points strictly forward)
  planes[0] = createPlane(front, camera.position + front * static_cast<double>(camera.near));

  // 4. Left & Right Planes (Normals point inward towards center)
  // We find a vector along the left/right edge, and cross it with camUp
  glm::dvec3 vLeft = front * cosH - right * sinH;
  glm::dvec3 vRight = front * cosH + right * sinH;

  // (Cross product of orthogonal unit vectors results in a unit vector; no normalize() needed!)
  planes[1] = createPlane(glm::cross(vLeft, camUp), camera.position);
  planes[2] = createPlane(glm::cross(camUp, vRight), camera.position);

  // 5. Bottom & Top Planes (Normals point inward towards center)
  glm::dvec3 vBottom = front * cosV - camUp * sinV;
  glm::dvec3 vTop = front * cosV + camUp * sinV;

  planes[3] = createPlane(glm::cross(right, vBottom), camera.position);
  planes[4] = createPlane(glm::cross(vTop, right), camera.position);

  return planes;
}

struct BoundingSphere
{
  glm::dvec3 position;
  double radius;
};

static constexpr bool isOccludedByFrustum(const Frustum frustum, const BoundingSphere sphere)
{
  for (const Plane& plane : frustum)
  {
    double dist = glm::dot(plane.normal, sphere.position) + plane.distance;

    if (dist < -sphere.radius)
    {
      return true;
    }
  }
  return false;
}

namespace Ellipsoid
{
static constexpr glm::dvec3 radii{SEMI_MAJOR, SEMI_MAJOR, SEMI_MINOR};
static constexpr glm::dvec3 invRadii = 1.0 / radii;

static constexpr glm::dvec3 scaleToUnitSphere(const glm::dvec3& point) { return point * invRadii; }
} // namespace Ellipsoid

static constexpr bool isOccludedByHorizon(const glm::dvec3& cameraPosition, const glm::dvec3& occlusionPoint)
{
  const auto cameraPositionScaled = Ellipsoid::scaleToUnitSphere(cameraPosition);

  const auto vt = occlusionPoint - cameraPositionScaled;
  const auto vtDotVc = -glm::dot(vt, cameraPositionScaled);

  const auto vhMagnitudeSquared = glm::dot(cameraPositionScaled, cameraPositionScaled) - 1.0;
  if (vhMagnitudeSquared < 0.0)
  {
    return vtDotVc > 0.0;
  }

  const auto vtMagnitudeSquared = glm::dot(vt, vt);

  const bool isOccluded =
    (vtDotVc > vhMagnitudeSquared) && (((vtDotVc * vtDotVc) / vtMagnitudeSquared) > vhMagnitudeSquared);

  return isOccluded;
}

static constexpr bool isOccluded(
  const glm::dvec3& cameraPosition,
  const Frustum& frustum,
  const BoundingSphere& sphere,
  const glm::dvec3& horizonCullingPoint)
{
  if (isOccludedByFrustum(frustum, sphere))
    return true;

  if (horizonCullingPoint == glm::dvec3{0.0})
    return false;

  return isOccludedByHorizon(cameraPosition, horizonCullingPoint);
}

} // namespace flb
