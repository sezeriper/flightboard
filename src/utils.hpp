#pragma once

#include "device.hpp"

#include <turbojpeg.h>

#include <png.h>
#include <zlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <SDL3_shadercross/SDL_shadercross.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace flb
{
static std::vector<std::byte> loadFileBinary(const std::filesystem::path& path)
  {
  std::ifstream file(path, std::ios::binary | std::ios::ate | std::ios::in);
  if (!file.is_open()) {
    SDL_Log("Can't open file %s", path.c_str());
    return {};
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<std::byte> buffer(size);
  if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
    SDL_Log("Failed to read file %s", path.c_str());
    return {};
  }

  return buffer;
}

/**
 * Read jpg files into Texture struct using turboJPEG.
 */
static Texture loadJPG(const std::filesystem::path& path)
{
  auto fileBuf = loadFileBinary(path);

  tjhandle handle = tj3Init(TJINIT_DECOMPRESS);
  if (handle == NULL) {
    SDL_Log("tj3Init failed: %s", tj3GetErrorStr(handle));
    return { 0, 0, nullptr };
  }

  if (tj3DecompressHeader(handle, reinterpret_cast<unsigned char*>(fileBuf.data()), fileBuf.size()) != 0) {
    SDL_Log("tj3DecompressHeader failed: %s", tj3GetErrorStr(handle));
    tj3Destroy(handle);
    return { 0, 0, nullptr };
  }

  int width = tj3Get(handle, TJPARAM_JPEGWIDTH);
  int height = tj3Get(handle, TJPARAM_JPEGHEIGHT);

  std::unique_ptr<std::byte[]> outBuf = std::make_unique_for_overwrite<std::byte[]>(width * height * 4);

  auto result = tj3Decompress8(handle, reinterpret_cast<unsigned char*>(fileBuf.data()), fileBuf.size(), reinterpret_cast<unsigned char*>(outBuf.get()), 0, TJPF_RGBA);
  if (result != 0) {
    SDL_Log("tj3Decompress8 failed: %s", tj3GetErrorStr(handle));
    tj3Destroy(handle);
    return { 0, 0, nullptr };
  }
  tj3Destroy(handle);

  return { static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), std::move(outBuf) };
}

/**
  * Reads png files into Texture struct using libpng.
  */
static Texture loadPNG(const std::filesystem::path& path)
{
  png_image image{};
  image.version = PNG_IMAGE_VERSION;
  if (png_image_begin_read_from_file(&image, path.string().c_str()) == 0) {
    SDL_Log("png_image_begin_read_from_file failed on file %s: %s", path.string().c_str(), image.message);
    return { 0, 0, nullptr };
  }

  image.format = PNG_FORMAT_RGBA;
  std::unique_ptr<std::byte[]> buffer = std::make_unique_for_overwrite<std::byte[]>(PNG_IMAGE_SIZE(image));

  if (png_image_finish_read(&image, NULL, buffer.get(), 0, NULL) == 0) {
    SDL_Log("png_image_finish_read failed: %s", image.message);
    png_image_free(&image);
    return { 0, 0, nullptr };
  }

  return { image.width, image.height, std::move(buffer) };
}

static std::string loadFileText(const std::filesystem::path& path)
{
  std::ifstream file(path, std::ios::binary | std::ios::ate | std::ios::in);
  if (!file.is_open()) {
    SDL_Log("Can't open file %s", path.c_str());
    return {};
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::string result;
  result.resize(size);
  if (!file.read(result.data(), size)) {
    SDL_Log("Failed to read file %s", path.c_str());
    return {};
  }

  return result;
}

static SDL_AppResult createShaders(SDL_GPUDevice* device, SDL_GPUShader** vertexShader, SDL_GPUShader** fragmentShader)
{
  // compile shaders using SDL_shadercross
  // first load the HLSL source code from file
  const auto vertexShaderSrc = loadFileText("content/shaders/lighting_basic.vert.hlsl");
  if (vertexShaderSrc.empty())
  {
    return SDL_APP_FAILURE;
  }

  const auto fragmentShaderSrc = loadFileText("content/shaders/lighting_basic.frag.hlsl");
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
} // namespace flb
