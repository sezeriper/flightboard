# Repository Guidelines

## Project Structure & Module Organization

`src/` contains the C++23 application. `src/main.cpp`, `app.*`, `window.*`, and `camera.*` drive the SDL app loop; `src/gpu/` owns GPU device, pipeline, sampler, allocator, and renderer code; `ros.*` handles ROS 2 integration; tile, culling, quadtree, texture, OBJ, and utility headers live beside the app layer. `content/` stores runtime assets: HLSL shaders in `content/shaders/`, models and textures in `content/models/`, and generated map tiles under ignored `content/tiles/`. `tools/` contains Python/shell map download utilities. `external/` is vendored third-party code; avoid editing it unless updating that dependency intentionally.

## Build, Test, and Development Commands

Source the ROS 2 environment before configuring so `ament_cmake`, `rclcpp`, `std_msgs`, and `px4_msgs` resolve:

```sh
source /opt/ros/<distro>/setup.zsh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target flightboard
./build/Debug/flightboard
```

Use `-DCMAKE_BUILD_TYPE=Release` for optimized local runs. The first build may download `libjpeg-turbo` through CMake `ExternalProject_Add`. For map tooling, create a virtualenv under the relevant tool directory and install `tools/azure_map_downloader/requirements.txt`.

## Coding Style & Naming Conventions

Format C++ with the checked-in `.clang-format`: 2-space indentation, no tabs, Allman braces, 120-column limit, sorted includes, left-aligned pointers, and unindented namespaces. Keep project code in the `flb` namespace and GPU-specific code under `flb::gpu`. Use `PascalCase` for types and `camelCase` for functions, variables, and members, matching the existing source.

## Testing Guidelines

There are no first-party tests currently; vendored tests under `external/` are not the project test suite. Treat a clean configure/build plus a short launch smoke test as the baseline before opening a PR. When adding tests, place them under a top-level `tests/` directory, wire them into CMake/CTest, and run:

```sh
ctest --test-dir build --output-on-failure
```

## Commit & Pull Request Guidelines

Recent commits use short imperative subjects, sometimes with a conventional prefix such as `feat:`. Prefer focused messages like `fix: Release tile textures on cleanup` or `Refactor GPU pipeline setup`. PRs should describe behavior changes, list build/test commands run, note ROS/PX4 or asset requirements, and include screenshots or short clips for rendering/UI changes.

## Security & Configuration Tips

Do not commit local caches, virtualenvs, `imgui.ini`, generated tile outputs, or API keys. Keep large downloaded map data in ignored paths such as `content/tiles/` or tool-specific `outputs/`.
