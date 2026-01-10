sudo pacman -S emscripten

emcc src/main.cpp -s USE_SDL=2 -o index.html

python -m http.server