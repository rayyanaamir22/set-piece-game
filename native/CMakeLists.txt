cmake_minimum_required(VERSION 3.25)
project(SetPiece LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CPM.cmake)
# Cache downloads next to the build tree unless the user set CPM_SOURCE_CACHE.

# ---- dependencies (pinned) ----
CPMAddPackage(
  NAME SDL3
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-3.4.8
  OPTIONS "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_TEST_LIBRARY OFF" "SDL_TESTS OFF" "SDL_EXAMPLES OFF" "SDL_INSTALL OFF"
)
CPMAddPackage(
  NAME imgui
  GITHUB_REPOSITORY ocornut/imgui
  GIT_TAG v1.93.0
  DOWNLOAD_ONLY YES
)

# ---- glad: GL 4.1 core loader (generated once with glad2, committed) ----
add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# ---- Dear ImGui (SDL3 + OpenGL3 backends) ----
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)

# ---- sim: pure, no platform/graphics includes (target boundary enforced by what it links) ----
add_library(sim STATIC src/sim/input_state.cpp)
target_include_directories(sim PUBLIC src)

# ---- platform: window, input mapping, timing ----
add_library(platform STATIC
  src/platform/keymap.cpp
  src/platform/input.cpp
  src/platform/window.cpp)
target_include_directories(platform PUBLIC src)
target_link_libraries(platform PUBLIC sim glad SDL3::SDL3-static)

add_executable(setpiece src/app/main.cpp src/app/overlay.cpp)
target_link_libraries(setpiece PRIVATE platform imgui)

if(MSVC)
  target_compile_options(platform PRIVATE /W4)
else()
  target_compile_options(sim PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(platform PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(setpiece PRIVATE -Wall -Wextra -Wpedantic)
endif()
