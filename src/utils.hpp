#pragma once

#include <turbojpeg.h>

#include <png.h>
#include <zlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>


#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <span>

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
static void loadJPG(const std::filesystem::path& path, std::span<std::byte>& outTexture)
{
  auto fileBuf = loadFileBinary(path);

  tjhandle handle = tj3Init(TJINIT_DECOMPRESS);
  if (handle == NULL) {
    SDL_Log("tj3Init failed: %s", tj3GetErrorStr(handle));
    return;
  }

  if (tj3DecompressHeader(handle, reinterpret_cast<unsigned char*>(fileBuf.data()), fileBuf.size()) != 0) {
    SDL_Log("tj3DecompressHeader failed: %s", tj3GetErrorStr(handle));
    tj3Destroy(handle);
    return;
  }

  auto result = tj3Decompress8(handle, reinterpret_cast<unsigned char*>(fileBuf.data()), fileBuf.size(), reinterpret_cast<unsigned char*>(outTexture.data()), 0, TJPF_RGBA);
  if (result != 0) {
    SDL_Log("tj3Decompress8 failed: %s", tj3GetErrorStr(handle));
    tj3Destroy(handle);
    return;
  }

  tj3Destroy(handle);
}

/**
  * Reads png files into Texture struct using libpng.
  */
// static Texture loadPNG(const std::filesystem::path& path)
// {
//   png_image image{};
//   image.version = PNG_IMAGE_VERSION;
//   if (png_image_begin_read_from_file(&image, path.string().c_str()) == 0) {
//     SDL_Log("png_image_begin_read_from_file failed on file %s: %s", path.string().c_str(), image.message);
//     return { 0, 0, nullptr };
//   }

//   image.format = PNG_FORMAT_RGBA;
//   std::unique_ptr<std::byte[]> buffer = std::make_unique_for_overwrite<std::byte[]>(PNG_IMAGE_SIZE(image));

//   if (png_image_finish_read(&image, NULL, buffer.get(), 0, NULL) == 0) {
//     SDL_Log("png_image_finish_read failed: %s", image.message);
//     png_image_free(&image);
//     return { 0, 0, nullptr };
//   }

//   return { image.width, image.height, std::move(buffer) };
// }

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

} // namespace flb
