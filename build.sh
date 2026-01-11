#!/bin/bash
# Ensure build directory exists and is clean
rm -rf build
mkdir -p build

# Configure using Emscripten CMake wrapper
# -S . : Source directory is current directory
# -B build : Build directory is 'build'
emcmake cmake -S . -B build

# Build the project
# --build build : Build the target in 'build' directory
cmake --build build

# Copy artifacts to root
cp build/index.html build/index.js build/index.wasm .

# Start server
python -m http.server