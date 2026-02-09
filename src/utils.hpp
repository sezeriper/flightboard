#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <SDL3_shadercross/SDL_shadercross.h>

#include <tiny_obj_loader.h>

#include "app.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <map>
#include <tuple>
#include <limits>

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
  const auto vertexShaderSrc = readFile("content/shaders/lighting_basic.vert.hlsl");
  if (vertexShaderSrc.empty())
  {
    return SDL_APP_FAILURE;
  }

  const auto fragmentShaderSrc = readFile("content/shaders/lighting_basic.frag.hlsl");
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

// TODO: very unoptimized i dont like it
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
  std::map<std::tuple<int, int, int>, Index> uniqueVertices;

  for (const auto& shape : shapes) {
    for (const auto& index : shape.mesh.indices) {
      std::tuple<int, int, int> key = {index.vertex_index, index.normal_index, index.texcoord_index};

      if (uniqueVertices.find(key) == uniqueVertices.end()) {
        if (vertices.size() >= std::numeric_limits<Index>::max()) {
           SDL_Log("Mesh too large for 16-bit indices!");
           return { vertices, indices };
        }

        Vertex vertex{};

        vertex.position.x = attrib.vertices[3 * index.vertex_index + 0];
        vertex.position.y = attrib.vertices[3 * index.vertex_index + 1];
        vertex.position.z = attrib.vertices[3 * index.vertex_index + 2];

        vertex.color.r = attrib.colors[3 * index.vertex_index + 0];
        vertex.color.g = attrib.colors[3 * index.vertex_index + 1];
        vertex.color.b = attrib.colors[3 * index.vertex_index + 2];

        if (index.normal_index >= 0) {
          vertex.normal.x = attrib.normals[3 * index.normal_index + 0];
          vertex.normal.y = attrib.normals[3 * index.normal_index + 1];
          vertex.normal.z = attrib.normals[3 * index.normal_index + 2];
        } else {
             vertex.normal = {0.0f, 0.0f, 0.0f};
        }

        uniqueVertices[key] = static_cast<Index>(vertices.size());
        vertices.push_back(vertex);
      }

      indices.push_back(uniqueVertices[key]);
    }
  }

  return { vertices, indices };
}
} // namespace flb
