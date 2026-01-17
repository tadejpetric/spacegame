cmake_minimum_required(VERSION 3.10)
project(spacegame)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Force Emscripten compilers if not already set
if(EMSCRIPTEN)
    set(CMAKE_C_COMPILER "emcc")
    set(CMAKE_CXX_COMPILER "em++")
endif()

set(CMAKE_CXX_STANDARD 23)

# ImGui library
file(GLOB IMGUI_SOURCES "external/imgui/imgui*.cpp")
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC external/imgui)


# FastNoiseLight library
include(FetchContent)

# 1. Download FastNoiseLite from GitHub
FetchContent_Declare(
  fastnoiselite
  GIT_REPOSITORY https://github.com/Auburn/FastNoiseLite.git
  GIT_TAG        master # Or a specific version/commit hash
)

FetchContent_MakeAvailable(fastnoiselite)

# Source files
set(SOURCES
    src/main.cpp
    src/geometry.cpp
    src/player.cpp
    src/overworld.cpp
    src/battle.cpp
)

add_executable(spacegame ${SOURCES})
target_include_directories(spacegame PRIVATE src)
target_link_libraries(spacegame PRIVATE imgui)
# target_link_libraries(spacegame PRIVATE fastnoiselight)
target_include_directories(spacegame PRIVATE "${fastnoiselite_SOURCE_DIR}/Cpp")

# Emscripten specific settings
if(EMSCRIPTEN)
    set_target_properties(spacegame PROPERTIES SUFFIX ".html")
    set_target_properties(spacegame PROPERTIES OUTPUT_NAME "index")

    # Emscripten handles SDL2 via -sUSE_SDL=2
    target_compile_options(imgui PUBLIC "-sUSE_SDL=2")
    target_compile_options(spacegame PRIVATE "-sUSE_SDL=2")
    
    target_link_options(spacegame PRIVATE
        "-sUSE_SDL=2"
        "-sFULL_ES2=1"
        "--shell-file" "${CMAKE_SOURCE_DIR}/shell.html"
    )
else()
    find_package(SDL2 REQUIRED)
    target_link_libraries(spacegame PRIVATE SDL2::SDL2)
    target_link_libraries(imgui PUBLIC SDL2::SDL2)
endif()