cmake_minimum_required(VERSION 3.16)
project(flightboard)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")


# set the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# for building sdl statically
set(BUILD_SHARED_LIBS OFF)

# for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This assumes the SDL source is available in external/SDL
set(SDL_DYNAMIC_API OFF)
set(SDL_X11_XTEST OFF)
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

# Add SDL_shadercross
set(SDLSHADERCROSS_VENDORED ON CACHE BOOL "Use vendored dependencies" FORCE)
set(SDLSHADERCROSS_SPIRVCROSS_SHARED OFF CACHE BOOL "Link to shared library variants of dependencies" FORCE)
set(SDLSHADERCROSS_DXC ON CACHE BOOL "Enable HLSL compilation via DXC" FORCE)
add_subdirectory(external/SDL_shadercross EXCLUDE_FROM_ALL)

# add glm
add_subdirectory(external/glm)

# Add imgui
set(imgui_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
add_library(imgui
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PRIVATE SDL3::SDL3)

set(ENTT_NO_MIXIN ON)
# disable asserts and exceptions if Release build
if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(ENTT_DISABLE_ASSERT ON)
  set(ENTT_NO_EXCEPTION ON)
endif()
add_subdirectory(external/entt)

add_subdirectory(external/zlib)
include_directories(
    "${CMAKE_CURRENT_BINARY_DIR}/external/zlib"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib"
)
set(ZLIB_INCLUDE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/external/zlib"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib"
)
set(ZLIB_LIBRARY zlibstatic)
add_subdirectory(external/libpng)

# Create your game executable target as usual
add_executable(flightboard src/main.cpp src/app.cpp)
target_link_libraries(flightboard PRIVATE glm imgui SDL3_shadercross::SDL3_shadercross EnTT png_static)
target_compile_definitions(flightboard PRIVATE GLM_FORCE_INTRINSICS)
