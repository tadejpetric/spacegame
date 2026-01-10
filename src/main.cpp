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

#include "geometry.h"
#include "player.h"

// Constants
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Structures
struct Planet {
    float x, y;
    float radius;
    float r, g, b;
};

struct GameState {
    Player player;
    std::vector<Planet> planets;
    bool keys[SDL_NUM_SCANCODES] = {false};
} g_state;

// Global variables
SDL_Window* window;
SDL_GLContext context;
GLuint program;
GLuint triangleVbo;
GLuint circleVbo;

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
    // Poll events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) {
            emscripten_cancel_main_loop();
            return;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.scancode < SDL_NUM_SCANCODES)
                g_state.keys[event.key.keysym.scancode] = true;
        }
        if (event.type == SDL_KEYUP) {
            if (event.key.keysym.scancode < SDL_NUM_SCANCODES)
                g_state.keys[event.key.keysym.scancode] = false;
        }
    }

    // Update Player logic
    update_player(g_state.player, g_state.keys);

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("Controls");
        ImGui::Text("Arrows to move");
        ImGui::Text("Pos: %.2f, %.2f", g_state.player.x, g_state.player.y);
        ImGui::End();
    }

    // Rendering
    ImGui::Render();

    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);

    float camX = g_state.player.x;
    float camY = g_state.player.y;
    float aspect = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;

    // Draw Planets
    for (const auto& p : g_state.planets) {
        float screenX = (p.x - camX) / aspect;
        float screenY = (p.y - camY);
        draw_disc(circleVbo, screenX, screenY, p.radius, p.r, p.g, p.b, program, aspect);
    }

    // Draw Player (Triangle) - always at center
    draw_triangle(triangleVbo, 0.0f, 0.0f, 0.05f, g_state.player.angle, 1.0f, 1.0f, 1.0f, program, aspect);

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
                              SCREEN_WIDTH, SCREEN_HEIGHT, 
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

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

    // Initialize Planets
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> pos_dist(-5.0f, 5.0f);
    std::uniform_real_distribution<float> size_dist(0.1f, 0.4f);
    std::uniform_real_distribution<float> col_dist(0.3f, 1.0f);

    for(int i = 0; i < 20; ++i) {
        g_state.planets.push_back({
            pos_dist(rng), pos_dist(rng),
            size_dist(rng),
            col_dist(rng), col_dist(rng), col_dist(rng)
        });
    }

    emscripten_set_main_loop(main_loop, 0, 1);
    return 0;
}