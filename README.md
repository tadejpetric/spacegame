# emscripten download
```
sudo pacman -S emscripten
```
# Imgui download
```
git clone --depth 1 https://github.com/ocornut/imgui.git imgui_tmp

mkdir -p external/imgui && cp imgui_tmp/*.h external/imgui/ && cp imgui_tmp/*.cpp external/imgui/ && cp imgui_tmp/backends/imgui_impl_sdl2.h external/imgui/ && cp imgui_tmp/backends/imgui_impl_sdl2.cpp external/imgui/ && cp imgui_tmp/backends/imgui_impl_opengl3.h external/imgui/ && cp imgui_tmp/backends/imgui_impl_opengl3.cpp external/imgui/ && rm -rf imgui_tmp
```
# build
```
./build.sh
```
# run
```
python -m http.server
```

# Project idea
Idea:
Tile based space game
You are a spaceship in an infinite 2D grid
There is someone hunting you (a strong enemy)
You have to run away from him, because he is much stronger than you

During the gameplay you collect resources, among them cards.
Cards are used during combat.
Eventually your cards are strong enough to beat the enemy.

You also encounter
- minor enemies
- shops
- safe zones
- danger zones
- planets
- resource zones
- asteroids (damage you)
- ...

Some of them are visible on map, some are random encounters.