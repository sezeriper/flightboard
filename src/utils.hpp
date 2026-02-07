#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <SDL3_shadercross/SDL_shadercross.h>

#include <tiny_obj_loader.h>

#include "app.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace flb
{
static std::string readFile(std::filesystem::path path)
{
  std::ifstream stream(path, std::ios::in | std::ios::binary);
  if (!stream.is_open()) {
    SDL_Log("Can't open file %s", path.c_str());
    return {};
  }

  const auto size = std::filesystem::file_size(path);
  std::string result;
  result.resize(size);

  stream.read(result.data(), size);

  return result;
}

static SDL_AppResult createShaders(SDL_GPUDevice* device, SDL_GPUShader** vertexShader, SDL_GPUShader** fragmentShader)
{
  // compile shaders using SDL_shadercross
  // first load the HLSL source code from file
  const auto vertexShaderSrc = readFile("content/shaders/spinning_cube.vert.hlsl");
  if (vertexShaderSrc.empty())
  {
    return SDL_APP_FAILURE;
  }

  const auto fragmentShaderSrc = readFile("content/shaders/color.frag.hlsl");
  if (fragmentShaderSrc.empty())
  {
    return SDL_APP_FAILURE;
  }

  SDL_ShaderCross_Init();

  const SDL_ShaderCross_HLSL_Info vertexShaderInfo {
    .source = vertexShaderSrc.c_str(),
    .entrypoint = "main",
    .include_dir = NULL,
    .defines = NULL,
    .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX,
    .props = 0
  };

  const SDL_ShaderCross_HLSL_Info fragmentShaderInfo {
    .source = fragmentShaderSrc.c_str(),
    .entrypoint = "main",
    .include_dir = NULL,
    .defines = NULL,
    .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT,
    .props = 0
  };

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
  const SDL_ShaderCross_SPIRV_Info vertSPIRVInfo {
    .bytecode = (const Uint8*)vertexSPIRV,
    .bytecode_size = vertexSPIRVSize,
    .entrypoint = "main",
    .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX,
    .props = 0
  };
  const SDL_ShaderCross_SPIRV_Info fragSPIRVInfo {
    .bytecode = (const Uint8*)fragmentSPIRV,
    .bytecode_size = fragmentSPIRVSize,
    .entrypoint = "main",
    .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT,
    .props = 0
  };

  const SDL_ShaderCross_GraphicsShaderMetadata* vertexSPIRVMetadata =
    SDL_ShaderCross_ReflectGraphicsSPIRV(
      (const Uint8*)vertexSPIRV, vertexSPIRVSize, 0);

  const SDL_ShaderCross_GraphicsShaderMetadata* fragmentSPIRVMetadata =
    SDL_ShaderCross_ReflectGraphicsSPIRV(
      (const Uint8*)fragmentSPIRV, fragmentSPIRVSize, 0);

  *vertexShader =
    SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
      device, &vertSPIRVInfo, &vertexSPIRVMetadata->resource_info, 0);

  *fragmentShader =
    SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
      device, &fragSPIRVInfo, &fragmentSPIRVMetadata->resource_info, 0);

  SDL_free((void*)vertexSPIRVMetadata);
  SDL_free((void*)fragmentSPIRVMetadata);
  SDL_free(vertexSPIRV);
  SDL_free(fragmentSPIRV);
  SDL_ShaderCross_Quit();

  return SDL_APP_CONTINUE;
}

static MeshData loadMesh(const std::filesystem::path& path)
{
  tinyobj::ObjReaderConfig reader_config;
  reader_config.mtl_search_path = path.parent_path().string();
  reader_config.triangulate = true;

  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(path.string(), reader_config)) {
    if (!reader.Error().empty()) {
      SDL_Log("TinyObjReader: %s", reader.Error().c_str());
    }
    return {};
  }

  if (!reader.Warning().empty()) {
    SDL_Log("TinyObjReader: %s", reader.Warning().c_str());
  }

  const auto& attrib = reader.GetAttrib();
  const auto& shapes = reader.GetShapes();

  std::vector<Vertex> vertices;
  std::vector<Index> indices;

  vertices.reserve(attrib.vertices.size() / 3);

  for (size_t i = 0; i < attrib.vertices.size(); i += 3) {
    vertices.emplace_back(Vertex{
      .position = {
        attrib.vertices[i + 0],
        attrib.vertices[i + 1],
        attrib.vertices[i + 2]
      },
      .color = {0.7f, 0.1f, 0.1f}
    });
  }

  for (const auto& shape : shapes) {
    for (const auto& index : shape.mesh.indices) {
      indices.push_back(static_cast<Index>(index.vertex_index));
    }
  }

  return { std::move(vertices), std::move(indices) };
}
} // namespace flb
