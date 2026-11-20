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

CPMAddPackage(
  NAME glm
  GITHUB_REPOSITORY g-truc/glm
  GIT_TAG 1.0.1
)
CPMAddPackage(
  NAME stb
  GITHUB_REPOSITORY nothings/stb
  GIT_TAG 6b5799bae8a1fe3792691ef4b72ae26434c3e799
  DOWNLOAD_ONLY YES
)
CPMAddPackage(
  NAME nlohmann_json
  GITHUB_REPOSITORY nlohmann/json
  GIT_TAG v3.12.0
  OPTIONS "JSON_BuildTests OFF" "JSON_Install OFF"
)
CPMAddPackage(
  NAME doctest
  GITHUB_REPOSITORY doctest/doctest
  GIT_TAG v2.4.12
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
add_library(sim STATIC
  src/sim/input_state.cpp
  src/sim/settings.cpp
  src/sim/BallPhysics.cpp
  src/sim/Prediction.cpp
  src/sim/ShotModel.cpp
  src/sim/Wall.cpp
  src/sim/Keeper.cpp
  src/sim/Collisions.cpp
  src/sim/challenge.cpp
  src/sim/layout.cpp
  src/sim/Sim.cpp
  src/sim/config_io.cpp)
target_include_directories(sim PUBLIC src)
target_link_libraries(sim PUBLIC nlohmann_json::nlohmann_json)

# ---- platform: window, input mapping, timing ----
add_library(platform STATIC
  src/platform/keymap.cpp
  src/platform/input.cpp
  src/platform/window.cpp)
target_include_directories(platform PUBLIC src)
target_link_libraries(platform PUBLIC sim glad SDL3::SDL3-static)

# ---- gfx + render: GL wrapper and the graybox scene (reads the sim, never writes it) ----
add_library(render STATIC
  src/gfx/gfx.cpp
  src/render/shapes.cpp
  src/render/camera_rig.cpp
  src/render/figures.cpp
  src/render/game_view.cpp)
target_include_directories(render PUBLIC src)
target_link_libraries(render PUBLIC sim glad glm::glm)
target_compile_definitions(render PUBLIC GLM_ENABLE_EXPERIMENTAL GLM_FORCE_DEPTH_ZERO_TO_ONE=0)

# ---- game: app loop, prefs, HUD and menus ----
add_library(game STATIC
  src/game/prefs.cpp
  src/game/ui_hud.cpp
  src/game/ui_sheets.cpp
  src/game/app.cpp)
target_include_directories(game PUBLIC src ${stb_SOURCE_DIR})
target_link_libraries(game PUBLIC render platform imgui)
target_compile_definitions(game PUBLIC
  SETPIECE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
  SETPIECE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data")

add_executable(setpiece src/app/main.cpp src/app/stb_impl.cpp)
target_include_directories(setpiece PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(setpiece PRIVATE game)

# ---- tests (doctest): golden parity + ported unit tests ----
option(SETPIECE_BUILD_TESTS "Build the doctest suite" ON)
if(SETPIECE_BUILD_TESTS)
  enable_testing()
  file(GLOB UNIT_TESTS CONFIGURE_DEPENDS tests/*.cpp)
  add_executable(sim_tests ${UNIT_TESTS})
  target_include_directories(sim_tests PRIVATE ${doctest_SOURCE_DIR} ${stb_SOURCE_DIR})
  target_link_libraries(sim_tests PRIVATE game)
  target_compile_definitions(sim_tests PRIVATE SETPIECE_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
  if(NOT MSVC)
    target_compile_options(sim_tests PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME sim_tests COMMAND sim_tests)
endif()

if(MSVC)
  target_compile_options(platform PRIVATE /W4)
  target_compile_options(sim PRIVATE /fp:precise)  # no FMA contraction: keeps parity with V8
else()
  target_compile_options(sim PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
  target_compile_options(platform PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(setpiece PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(render PRIVATE -Wall -Wextra)
  target_compile_options(game PRIVATE -Wall -Wextra)
endif()
