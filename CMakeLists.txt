cmake_minimum_required(VERSION 3.20)
project(neoDAW VERSION 0.0.0 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)
#neo: sdl2
find_package(SDL2 REQUIRED)
#neo: juce
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW TRUE
)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_JUCEAIDE OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JUCE)
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
#neo: midifile
FetchContent_Declare(
  midifile
  GIT_REPOSITORY https://github.com/craigsapp/midifile.git
  GIT_TAG master
)
FetchContent_GetProperties(midifile)
if(NOT midifile_POPULATED)
  FetchContent_Populate(midifile)
  add_library(midifile STATIC
    ${midifile_SOURCE_DIR}/src/Options.cpp
    ${midifile_SOURCE_DIR}/src/Binasc.cpp
    ${midifile_SOURCE_DIR}/src/MidiEvent.cpp
    ${midifile_SOURCE_DIR}/src/MidiEventList.cpp
    ${midifile_SOURCE_DIR}/src/MidiFile.cpp
    ${midifile_SOURCE_DIR}/src/MidiMessage.cpp
  )
  target_include_directories(midifile PUBLIC ${midifile_SOURCE_DIR}/include)
  target_compile_features(midifile PUBLIC cxx_std_11)
endif()
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

find_package(CURL REQUIRED)
target_link_libraries(neodaw PRIVATE
  SDL2::SDL2
  OpenGL::GL
  fluidlite-static
  midifile
  nlohmann_json::nlohmann_json
  juce::juce_audio_basics
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_core
  CURL::libcurl
)

if(UNIX)
  target_link_libraries(neodaw PRIVATE m)
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(neodaw PRIVATE dl rt)
endif()
if(WIN32)
  target_link_libraries(neodaw PRIVATE comdlg32 SDL2::SDL2main)
endif()
