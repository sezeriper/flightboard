#pragma once

#include "camera.hpp"
#include "math.hpp"

namespace flb
{

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
