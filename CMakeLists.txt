cmake_minimum_required(VERSION 3.10)
project(spacegame)

set(CMAKE_CXX_STANDARD 17)

# Source files
file(GLOB IMGUI_SOURCES "external/imgui/imgui*.cpp")
set(SOURCES
    src/main.cpp
    src/geometry.cpp
    src/player.cpp
    ${IMGUI_SOURCES}
)

add_executable(spacegame ${SOURCES})
target_include_directories(spacegame PRIVATE src external/imgui)

# Emscripten specific settings
if(EMSCRIPTEN)
    set_target_properties(spacegame PROPERTIES SUFFIX ".html")
    set_target_properties(spacegame PROPERTIES OUTPUT_NAME "index")

    target_compile_options(spacegame PRIVATE
        "-sUSE_SDL=2"
    )

    target_link_options(spacegame PRIVATE
        "-sUSE_SDL=2"
        "-sFULL_ES2=1"
        "--shell-file" "${CMAKE_SOURCE_DIR}/shell.html"
    )
endif()
