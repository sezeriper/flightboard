#pragma once

#include "indicator_model.hpp"
#include "model.hpp"
#include "texture_manager.hpp"

#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

#include "culling.hpp"

namespace flb
{
namespace component
{

struct VertexBuffer
{
  SDL_GPUBuffer* value;
};
struct IndexBuffer
{
  SDL_GPUBuffer* value;
};
struct IndexCount
{
  Uint32 value;
};
struct Texture
{
  SDL_GPUTexture* value;
};

struct Position
{
  glm::dvec3 value;
};

struct Transform
{
  glm::mat3 value{1.0f};
};

struct TextureHandle
{
  flb::TextureHandle value;
};

struct MeshHandle
{
  flb::MeshHandle value;
};

struct Model
{
  flb::Model value;
};

struct IndicatorModel
{
  flb::IndicatorModel value;
};

struct Visible
{
};

struct BoundingSphere
{
  flb::BoundingSphere value;
};

struct HorizonCullingPoint
{
  glm::dvec3 value;
};

} // namespace component
} // namespace flb
