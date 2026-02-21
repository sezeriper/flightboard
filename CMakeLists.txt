cmake_minimum_required(VERSION 3.16)
project(flightboard CXX)

include(ExternalProject)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")


# set the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This assumes the SDL source is available in external/SDL
set(BUILD_SHARED_LIBS OFF)
set(SDL_DYNAMIC_API OFF)
set(SDL_X11_XTEST OFF)
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

# Add SDL_shadercross
set(SDLSHADERCROSS_VENDORED OFF CACHE BOOL "Use vendored dependencies" FORCE)
set(SDLSHADERCROSS_SPIRVCROSS_SHARED ON CACHE BOOL "Link to shared library variants of dependencies" FORCE)
# set(SDLSHADERCROSS_DXC OFF CACHE BOOL "Enable HLSL compilation via DXC" FORCE)
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

# 1. Handle platform-specific naming FIRST so we can use it in BUILD_BYPRODUCTS
if(MSVC)
    set(TURBOJPEG_LIB_NAME "turbojpeg-static.lib")
else()
    set(TURBOJPEG_LIB_NAME "libturbojpeg.a")
endif()

# Store the exact path in a variable to keep things clean
set(LIBJPEG_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo-install")
set(TURBOJPEG_LIB_PATH "${LIBJPEG_INSTALL_DIR}/lib/${TURBOJPEG_LIB_NAME}")

# 2. Add libjpeg-turbo as an external project
ExternalProject_Add(
    libjpeg-turbo_ext
    # You can point this to a local path (SOURCE_DIR) or a Git repository
    GIT_REPOSITORY "https://github.com/libjpeg-turbo/libjpeg-turbo.git"
    GIT_TAG "c96e93b1f21ac4fb9521d5108cb16917717e4da7" # Or whichever specific version you need

    BUILD_BYPRODUCTS "${TURBOJPEG_LIB_PATH}"

    # Configure CMake arguments for the libjpeg-turbo build
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${LIBJPEG_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DENABLE_SHARED=OFF  # Optional: disable shared library if you only want static
        -DENABLE_STATIC=ON   # Optional: explicitly enable static library
        -DWITH_TURBOJPEG=ON  # Optional: includes the TurboJPEG API library
        -DCMAKE_INSTALL_LIBDIR=lib

    # We don't need to specify custom build/install commands because
    # ExternalProject_Add handles standard CMake projects automatically.
)

# 3. Create an IMPORTED target for Ninja to track
add_library(TurboJpeg::TurboJpeg STATIC IMPORTED)

# 4. Point the imported target to the exact file path
set_target_properties(TurboJpeg::TurboJpeg PROPERTIES
    IMPORTED_LOCATION "${TURBOJPEG_LIB_PATH}"
)

# 5. Tell CMake that this imported target cannot be resolved until the external project finishes
add_dependencies(TurboJpeg::TurboJpeg libjpeg-turbo_ext)

add_executable(flightboard src/main.cpp src/app.cpp)
add_dependencies(flightboard libjpeg-turbo_ext)

target_compile_options(flightboard PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native>
)

target_compile_definitions(flightboard PRIVATE GLM_FORCE_INTRINSICS)

target_include_directories(flightboard PRIVATE "${LIBJPEG_INSTALL_DIR}/include")

target_link_libraries(flightboard PRIVATE
  glm
  imgui
  SDL3_shadercross::SDL3_shadercross
  EnTT
  png_static
  TurboJpeg::TurboJpeg
)
