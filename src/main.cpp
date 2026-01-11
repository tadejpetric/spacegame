#include <SDL.h>
#include <SDL_opengles2.h>
#include <emscripten.h>
#include <cmath>
#include <vector>
#include <iostream>
#include <random>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include "overworld.h"

enum class GameMode {
    OVERWORLD,
    BATTLE
};

GameMode get_mode() {
    return GameMode::OVERWORLD;
}

// Constants
const float TILE_SIZE = 1.0f;
const int GRID_VIEW_RANGE = 20;

GameState g_state;

// Global variables
SDL_Window* window;
SDL_GLContext context;
GLuint program;
GLuint triangleVbo;
GLuint circleVbo;
GLuint lineVbo;

const char* vertex_shader_source = 
    "attribute vec2 position;\n"
    "uniform mat3 transform;\n"
    "void main() {\n"
    "   vec3 pos = transform * vec3(position, 1.0);\n"
    "   gl_Position = vec4(pos.xy, 0.0, 1.0);\n"
    "}\n";

const char* fragment_shader_source = 
    "precision mediump float;\n"
    "uniform vec4 color;\n"
    "void main() {\n"
    "   gl_FragColor = color;\n"
    "}\n";

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

void main_loop() {
    GameMode mode = get_mode();
    
    switch (mode) {
        case GameMode::OVERWORLD:
            overworld_loop();
            break;
        case GameMode::BATTLE:
            // battle_loop();
            break;
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    window = SDL_CreateWindow("Space Game", 
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                              g_state.screen_width, g_state.screen_height, 
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    context = SDL_GL_CreateContext(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init("#version 100");

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // Geometry - Triangle (pointing right at 0 degrees)
    float triangle_verts[] = {
         1.0f,  0.0f,
        -0.6f,  0.6f,
        -0.6f, -0.6f
    };
    glGenBuffers(1, &triangleVbo);
    glBindBuffer(GL_ARRAY_BUFFER, triangleVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_verts), triangle_verts, GL_STATIC_DRAW);

    // Geometry - Circle
    std::vector<float> circle_verts;
    circle_verts.push_back(0.0f); circle_verts.push_back(0.0f);
    for(int i = 0; i <= 30; i++) {
        float angle = 2.0f * M_PI * float(i) / 30.0f;
        circle_verts.push_back(cos(angle));
        circle_verts.push_back(sin(angle));
    }
    glGenBuffers(1, &circleVbo);
    glBindBuffer(GL_ARRAY_BUFFER, circleVbo);
    glBufferData(GL_ARRAY_BUFFER, circle_verts.size() * sizeof(float), circle_verts.data(), GL_STATIC_DRAW);

    // Geometry - Line (unit length from 0,0 to 1,0)
    float line_verts[] = {
        0.0f, 0.0f,
        1.0f, 0.0f
    };
    glGenBuffers(1, &lineVbo);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_verts), line_verts, GL_STATIC_DRAW);

    // Initialize Planets (not needed for infinite grid)
    /*
    std::mt19937 rng(1337);
    ...
    */

    emscripten_set_main_loop(main_loop, 0, 1);
    return 0;
}