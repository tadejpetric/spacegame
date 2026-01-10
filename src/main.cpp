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
const float TILE_SIZE = 1.0f;
const int GRID_VIEW_RANGE = 20;

struct ZoomState {
    float level = 1.0f;
    const float min = 0.2f;
    const float max = 5.0f;
    const float speed = 0.1f;
};

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
    ZoomState zoom;
    int screen_width = 800;
    int screen_height = 600;
} g_state;

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

            // Discrete tile movement
            if (event.key.keysym.scancode == SDL_SCANCODE_UP) {
                g_state.player.y += TILE_SIZE;
                g_state.player.angle = M_PI / 2.0f;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_DOWN) {
                g_state.player.y -= TILE_SIZE;
                g_state.player.angle = -M_PI / 2.0f;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_LEFT) {
                g_state.player.x -= TILE_SIZE;
                g_state.player.angle = M_PI;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
                g_state.player.x += TILE_SIZE;
                g_state.player.angle = 0.0f;
            }

            // Zoom controls
            if (event.key.keysym.scancode == SDL_SCANCODE_EQUALS || event.key.keysym.scancode == SDL_SCANCODE_KP_PLUS) {
                g_state.zoom.level += g_state.zoom.speed;
                if (g_state.zoom.level > g_state.zoom.max) g_state.zoom.level = g_state.zoom.max;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_MINUS || event.key.keysym.scancode == SDL_SCANCODE_KP_MINUS) {
                g_state.zoom.level -= g_state.zoom.speed;
                if (g_state.zoom.level < g_state.zoom.min) g_state.zoom.level = g_state.zoom.min;
            }
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

    SDL_GetWindowSize(window, &g_state.screen_width, &g_state.screen_height);
    glViewport(0, 0, g_state.screen_width, g_state.screen_height);

    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);

    float camX = g_state.player.x;
    float camY = g_state.player.y;
    float aspect = (float)g_state.screen_width / (float)g_state.screen_height;
    float zoom = g_state.zoom.level;

    // Draw Grid
    int viewRange = (int)(GRID_VIEW_RANGE / zoom);
    int startX = (int)floor((camX - viewRange * TILE_SIZE) / TILE_SIZE);
    int endX = (int)ceil((camX + viewRange * TILE_SIZE) / TILE_SIZE);
    int startY = (int)floor((camY - viewRange * TILE_SIZE) / TILE_SIZE);
    int endY = (int)ceil((camY + viewRange * TILE_SIZE) / TILE_SIZE);

    for (int x = startX; x <= endX; ++x) {
        float worldX = x * TILE_SIZE;
        float screenX = (worldX - camX) / (aspect / zoom);
        float screenY1 = (startY * TILE_SIZE - camY) / (1.0f / zoom);
        float screenY2 = (endY * TILE_SIZE - camY) / (1.0f / zoom);
        draw_line(lineVbo, screenX, screenY1, screenX, screenY2, 0.2f, 0.2f, 0.3f, program, aspect);
    }
    for (int y = startY; y <= endY; ++y) {
        float worldY = y * TILE_SIZE;
        float screenY = (worldY - camY) / (1.0f / zoom);
        float screenX1 = (startX * TILE_SIZE - camX) / (aspect / zoom);
        float screenX2 = (endX * TILE_SIZE - camX) / (aspect / zoom);
        draw_line(lineVbo, screenX1, screenY, screenX2, screenY, 0.2f, 0.2f, 0.3f, program, aspect);
    }

    // Draw Planets
    for (int x = startX; x <= endX; ++x) {
        for (int y = startY; y <= endY; ++y) {
            // Seed based on tile coordinates
            std::mt19937 tile_rng(static_cast<unsigned int>(x * 73856093 ^ y * 19349663));
            std::uniform_real_distribution<float> chance_dist(0.0f, 1.0f);
            
            if (chance_dist(tile_rng) < 0.1f) { // 10% chance per tile
                std::uniform_real_distribution<float> size_dist(0.1f, 0.45f);
                std::uniform_real_distribution<float> col_dist(0.3f, 1.0f);
                
                float radius = size_dist(tile_rng);
                float r = col_dist(tile_rng);
                float g = col_dist(tile_rng);
                float b = col_dist(tile_rng);
                
                float worldX = x * TILE_SIZE + TILE_SIZE * 0.5f;
                float worldY = y * TILE_SIZE + TILE_SIZE * 0.5f;
                
                float screenX = (worldX - camX) / (aspect / zoom);
                float screenY = (worldY - camY) / (1.0f / zoom);
                draw_disc(circleVbo, screenX, screenY, radius * zoom, r, g, b, program, aspect);
            }
        }
    }

    // Draw Player (Triangle) - always at center
    draw_triangle(triangleVbo, 0.0f, 0.0f, 0.05f * zoom, g_state.player.angle, 1.0f, 1.0f, 1.0f, program, aspect);

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