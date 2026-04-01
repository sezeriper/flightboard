#pragma once

#include "../utils.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <glm/glm.hpp>
#include <string>

// Helpers
namespace
{
SDL_GPUShader* createShader(SDL_GPUDevice* device, const std::string& path, SDL_ShaderCross_ShaderStage stage)
{
  const auto shaderSrc = flb::loadFileText(path);
  if (shaderSrc.empty())
  {
    return NULL;
  }

  const SDL_ShaderCross_HLSL_Info hlslInfo{
    .source = shaderSrc.c_str(),
    .entrypoint = "main",
    .include_dir = NULL,
    .defines = NULL,
    .shader_stage = stage,
    .props = 0};

  size_t spirvSize = 0;
  void* spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvSize);
  if (spirv == NULL)
  {
    SDL_Log("ShaderCross_CompileSPIRVFromHLSL failed for %s: %s", path.c_str(), SDL_GetError());
    return NULL;
  }

  const SDL_ShaderCross_SPIRV_Info spirvInfo{
    .bytecode = (const Uint8*)spirv,
    .bytecode_size = spirvSize,
    .entrypoint = "main",
    .shader_stage = stage,
    .props = 0};

  const SDL_ShaderCross_GraphicsShaderMetadata* metadata =
    SDL_ShaderCross_ReflectGraphicsSPIRV((const Uint8*)spirv, spirvSize, 0);

  SDL_GPUShader* shader =
    SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &spirvInfo, &metadata->resource_info, 0);

  SDL_free((void*)metadata);
  SDL_free(spirv);

  return shader;
}
} // namespace

// I defined the vertex and index here because the pipeline directly depends on it
namespace flb
{
namespace gpu
{
struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;
};
using Index = Uint16;

struct PipelineConfig
{
  std::string vertexShaderPath;
  std::string fragmentShaderPath;
  SDL_GPUPrimitiveType primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  SDL_GPUFillMode fillMode = SDL_GPU_FILLMODE_FILL;
};

class Pipeline
{
public:
  SDL_AppResult init(SDL_GPUDevice* device, SDL_Window* window, const PipelineConfig& config)
  {
    SDL_ShaderCross_Init();

    SDL_GPUShader* vertexShader = createShader(device, config.vertexShaderPath, SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
    SDL_GPUShader* fragmentShader =
      createShader(device, config.fragmentShaderPath, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

    SDL_ShaderCross_Quit();

    if (vertexShader == NULL || fragmentShader == NULL)
    {
      return SDL_APP_FAILURE;
    }

    // create the pipeline
    SDL_GPUVertexBufferDescription vertexBufferDescriptions[1]{{
      .slot = 0,
      .pitch = sizeof(Vertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
      .instance_step_rate = 0,
    }};

    SDL_GPUVertexAttribute vertexAttributes[4]{
      {
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(Vertex, position),
      },
      {
        .location = 1,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(Vertex, normal),

      },
      {
        .location = 2,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(Vertex, color),
      },
      {
        .location = 3,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        .offset = offsetof(Vertex, uv),
      }};

    SDL_GPUColorTargetDescription colorTargetDescriptions[1]{{
      .format = SDL_GetGPUSwapchainTextureFormat(device, window),
      .blend_state{
        .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .color_blend_op = SDL_GPU_BLENDOP_ADD,
        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        .enable_blend = true,
      },
    }};

    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo{
      .vertex_shader = vertexShader,
      .fragment_shader = fragmentShader,
      .vertex_input_state{
        .vertex_buffer_descriptions = vertexBufferDescriptions,
        .num_vertex_buffers = 1,
        .vertex_attributes = vertexAttributes,
        .num_vertex_attributes = 4,
      },
      .primitive_type = config.primitiveType,
      .rasterizer_state{
        .fill_mode = config.fillMode,
        .cull_mode = SDL_GPU_CULLMODE_BACK,
        .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
      },
      .depth_stencil_state{
        .compare_op = SDL_GPU_COMPAREOP_GREATER, // reversed-z
        .enable_depth_test = true,
        .enable_depth_write = true,
      },
      .target_info{
        .color_target_descriptions = colorTargetDescriptions,
        .num_color_targets = 1,
        .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        .has_depth_stencil_target = true,
      },
    };

    pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    if (pipeline == NULL)
    {
      SDL_Log("CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
  }

  void cleanup(SDL_GPUDevice* device)
  {
    if (pipeline)
    {
      SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
      pipeline = NULL;
    }
  }

  SDL_GPUGraphicsPipeline* get() const { return pipeline; }

private:
  SDL_GPUGraphicsPipeline* pipeline = NULL;
};

} // namespace gpu
} // namespace flb
