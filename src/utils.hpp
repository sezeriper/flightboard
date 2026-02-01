#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>


constexpr const char* BASE_PATH = "../";

static SDL_GPUShader* loadShader(
  SDL_GPUDevice* device,
  const char* shaderFilename,
  Uint32 samplerCount,
  Uint32 uniformBufferCount,
  Uint32 storageBufferCount,
  Uint32 storageTextureCount)
{
  // Auto-detect the shader stage from the file name for convenience
  SDL_GPUShaderStage stage;
  if (SDL_strstr(shaderFilename, ".vert"))
  {
    stage = SDL_GPU_SHADERSTAGE_VERTEX;
  }
  else if (SDL_strstr(shaderFilename, ".frag"))
  {
    stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  }
  else
  {
    SDL_Log("Invalid shader stage!");
    return NULL;
  }

  char fullPath[256];
  SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
  SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
  const char *entrypoint;

  if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV)
  {
    SDL_snprintf(fullPath, sizeof(fullPath), "%scontent/shaders/compiled/SPIRV/%s.spv", BASE_PATH, shaderFilename);
    format = SDL_GPU_SHADERFORMAT_SPIRV;
    entrypoint = "main";
  }
  else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL)
  {
    SDL_snprintf(fullPath, sizeof(fullPath), "%scontent/shaders/compiled/MSL/%s.msl", BASE_PATH, shaderFilename);
    format = SDL_GPU_SHADERFORMAT_MSL;
    entrypoint = "main0";
  }
  else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL)
  {
    SDL_snprintf(fullPath, sizeof(fullPath), "%scontent/shaders/compiled/DXIL/%s.dxil", BASE_PATH, shaderFilename);
    format = SDL_GPU_SHADERFORMAT_DXIL;
    entrypoint = "main";
  }
  else
  {
    SDL_Log("%s", "Unrecognized backend shader format!");
    return NULL;
  }

  size_t codeSize;
  void* code = SDL_LoadFile(fullPath, &codeSize);
  if (code == NULL)
  {
    SDL_Log("Failed to load shader from disk! %s", fullPath);
    return NULL;
  }

  SDL_GPUShaderCreateInfo shaderInfo {
    .code = reinterpret_cast<Uint8*>(code),
    .code_size = codeSize,
    .entrypoint = entrypoint,
    .format = format,
    .stage = stage,
    .num_samplers = samplerCount,
    .num_uniform_buffers = uniformBufferCount,
    .num_storage_buffers = storageBufferCount,
    .num_storage_textures = storageTextureCount,
  };

  SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
  SDL_free(code);

  if (shader == NULL)
  {
    SDL_Log("Failed to create shader!");
    return NULL;
  }

  return shader;
}

