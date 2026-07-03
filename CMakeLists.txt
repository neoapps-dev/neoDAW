cmake_minimum_required(VERSION 3.20)
project(neoDAW VERSION 0.0.0 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)
#neo: sdl2
find_package(SDL2 REQUIRED)
#neo: imgui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.8-docking
)
FetchContent_MakeAvailable(imgui)
#neo: fluidlite
FetchContent_Declare(
  fluidlite
  GIT_REPOSITORY https://github.com/divideconcept/FluidLite.git
  GIT_TAG master
)
set(WITH_FLOAT ON)
set(STB_VORBIS ON)
FetchContent_MakeAvailable(fluidlite)
#neo: nlohmann/json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
find_package(OpenGL REQUIRED)
add_executable(neodaw
  src/main.cpp
  src/App.cpp
  src/AudioEngine.cpp
  src/Project.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(neodaw PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${fluidlite_source_DIR}/include
  ${fluidlite_binary_DIR}
)

target_link_libraries(neodaw PRIVATE
  SDL2::SDL2
  OpenGL::GL
  m
  fluidlite-static
  nlohmann_json::nlohmann_json
)

if(UNIX AND NOT APPLE)
  target_link_libraries(neodaw PRIVATE dl rt)
endif()
