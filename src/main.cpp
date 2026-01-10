#include <SDL.h>
#include <SDL_opengles2.h>
#include <emscripten.h>
#include <cmath>

// Global variables
SDL_Window* window;
SDL_GLContext context;

// This function runs every frame (simulating a game loop)
void main_loop() {
    // 1. Calculate a color based on time
    double time = emscripten_get_now() / 1000.0;
    float red = (float)(sin(time) + 1.0) / 2.0; // Oscillates between 0.0 and 1.0
    
    // 2. Clear the screen with that color (The WebGL part)
    glClearColor(red, 0.0f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 3. Swap buffers to show the result
    SDL_GL_SwapWindow(window);
}

int main() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return -1;
    }

    // Create the browser window (Canvas)
    // We request an OpenGL ES 2.0 context (compatible with WebGL 1)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    window = SDL_CreateWindow("Wasm Demo", 
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                              800, 600, 
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    context = SDL_GL_CreateContext(window);

    // Tell Emscripten to use our main_loop function
    // 0 = 0 fps (use browser's requestAnimationFrame), 1 = simulate infinite loop
    emscripten_set_main_loop(main_loop, 0, 1);

    return 0;
}