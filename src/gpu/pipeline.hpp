#pragma once

#include "../utils.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <glm/glm.hpp>

// Helpers
namespace
{
SDL_AppResult createShaders(SDL_GPUDevice* device, SDL_GPUShader** vertexShader, SDL_GPUShader** fragmentShader)
{
  // compile shaders using SDL_shadercross
  // first load the HLSL source code from file
  const auto vertexShaderSrc = flb::loadFileText("content/shaders/lighting_basic.vert.hlsl");
  if (vertexShaderSrc.empty())
  {
    return SDL_APP_FAILURE;
  }

  const auto fragmentShaderSrc = flb::loadFileText("content/shaders/lighting_basic.frag.hlsl");
  if (fragmentShaderSrc.empty())
  {
    return SDL_APP_FAILURE;
  }

  SDL_ShaderCross_Init();

  const SDL_ShaderCross_HLSL_Info vertexShaderInfo{
    .source = vertexShaderSrc.c_str(),
    .entrypoint = "main",
    .include_dir = NULL,
    .defines = NULL,
    .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX,
    .props = 0};

  const SDL_ShaderCross_HLSL_Info fragmentShaderInfo{
    .source = fragmentShaderSrc.c_str(),
    .entrypoint = "main",
    .include_dir = NULL,
    .defines = NULL,
    .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT,
    .props = 0};

  size_t vertexSPIRVSize = 0;
  void* vertexSPIRV = SDL_ShaderCross_CompileSPIRVFromHLSL(&vertexShaderInfo, &vertexSPIRVSize);
  if (vertexSPIRV == NULL)
  {
    SDL_Log("Failed to compile vertex shader HLSL to SPIR-V");
    return SDL_APP_FAILURE;
  }
  size_t fragmentSPIRVSize = 0;
  void* fragmentSPIRV = SDL_ShaderCross_CompileSPIRVFromHLSL(&fragmentShaderInfo, &fragmentSPIRVSize);
  if (fragmentSPIRV == NULL)
  {
    SDL_Log("Failed to compile fragment shader HLSL to SPIR-V");
    return SDL_APP_FAILURE;
  }

  // compile SPIR-V to backend-specific shader code
  const SDL_ShaderCross_SPIRV_Info vertSPIRVInfo{
    .bytecode = (const Uint8*)vertexSPIRV,
    .bytecode_size = vertexSPIRVSize,
    .entrypoint = "main",
    .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX,
    .props = 0};
  const SDL_ShaderCross_SPIRV_Info fragSPIRVInfo{
    .bytecode = (const Uint8*)fragmentSPIRV,
    .bytecode_size = fragmentSPIRVSize,
    .entrypoint = "main",
    .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT,
    .props = 0};

  const SDL_ShaderCross_GraphicsShaderMetadata* vertexSPIRVMetadata =
    SDL_ShaderCross_ReflectGraphicsSPIRV((const Uint8*)vertexSPIRV, vertexSPIRVSize, 0);

  const SDL_ShaderCross_GraphicsShaderMetadata* fragmentSPIRVMetadata =
    SDL_ShaderCross_ReflectGraphicsSPIRV((const Uint8*)fragmentSPIRV, fragmentSPIRVSize, 0);

  *vertexShader =
    SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &vertSPIRVInfo, &vertexSPIRVMetadata->resource_info, 0);

  *fragmentShader =
    SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &fragSPIRVInfo, &fragmentSPIRVMetadata->resource_info, 0);

  SDL_free((void*)vertexSPIRVMetadata);
  SDL_free((void*)fragmentSPIRVMetadata);
  SDL_free(vertexSPIRV);
  SDL_free(fragmentSPIRV);
  SDL_ShaderCross_Quit();

  return SDL_APP_CONTINUE;
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

class Pipeline
{
public:
  SDL_AppResult init(SDL_GPUDevice* device, SDL_Window* window)
  {
    SDL_GPUShader* vertexShader = NULL;
    SDL_GPUShader* fragmentShader = NULL;
    SDL_AppResult shaderResult = createShaders(device, &vertexShader, &fragmentShader);
    if (shaderResult != SDL_APP_CONTINUE)
    {
      return shaderResult;
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
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .rasterizer_state{
        .fill_mode = SDL_GPU_FILLMODE_FILL,
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
    if (pipeline == NULL)
    {
      SDL_Log("CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    return SDL_APP_CONTINUE;
  }

  void cleanup(SDL_GPUDevice* device) { SDL_ReleaseGPUGraphicsPipeline(device, pipeline); }

  SDL_GPUGraphicsPipeline* getPipeline() const { return pipeline; }

private:
  SDL_GPUGraphicsPipeline* pipeline = NULL;
};

} // namespace gpu
} // namespace flb
