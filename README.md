# emscripten download

sudo pacman -S emscripten

# Imgui download
git clone --depth 1 https://github.com/ocornut/imgui.git imgui_tmp

mkdir -p external/imgui && cp imgui_tmp/*.h external/imgui/ && cp imgui_tmp/*.cpp external/imgui/ && cp imgui_tmp/backends/imgui_impl_sdl2.h external/imgui/ && cp imgui_tmp/backends/imgui_impl_sdl2.cpp external/imgui/ && cp imgui_tmp/backends/imgui_impl_opengl3.h external/imgui/ && cp imgui_tmp/backends/imgui_impl_opengl3.cpp external/imgui/ && rm -rf imgui_tmp

# compile
emcc src/main.cpp external/imgui/imgui*.cpp -Iexternal/imgui -s USE_SDL=2 -s FULL_ES2=1 -o index.html

# run
python -m http.server


# Project idea
Idea:
making a space game. Some planets, some resource harvesting. To get to new planets you need
new resources and upgrades, ...