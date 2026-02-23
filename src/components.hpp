#pragma once

#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

namespace flb
{
namespace component
{

struct VertexBuffer { SDL_GPUBuffer* value; };
struct IndexBuffer{ SDL_GPUBuffer* value; };
struct Texture { SDL_GPUTexture* value; };

struct Position { glm::dvec3 value; };

}
}
