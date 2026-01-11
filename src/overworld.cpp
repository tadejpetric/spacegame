#include "overworld.h"
#include <emscripten.h>
#include <cmath>
#include <random>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "geometry.h"

void overworld_loop() {
    handle_events();
    render_ui();
    render_game();
}

void handle_events() {
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
    update_player(g_state.player, g_state.keys);
}

void draw_grid(float camX, float camY, float aspect, float zoom) {
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
}

void draw_planets(float camX, float camY, float aspect, float zoom) {
    int viewRange = (int)(GRID_VIEW_RANGE / zoom);
    int startX = (int)floor((camX - viewRange * TILE_SIZE) / TILE_SIZE);
    int endX = (int)ceil((camX + viewRange * TILE_SIZE) / TILE_SIZE);
    int startY = (int)floor((camY - viewRange * TILE_SIZE) / TILE_SIZE);
    int endY = (int)ceil((camY + viewRange * TILE_SIZE) / TILE_SIZE);

    for (int x = startX; x <= endX; ++x) {
        for (int y = startY; y <= endY; ++y) {
            std::mt19937 tile_rng(static_cast<unsigned int>(x * 73856093 ^ y * 19349663));
            std::uniform_real_distribution<float> chance_dist(0.0f, 1.0f);
            
            if (chance_dist(tile_rng) < 0.1f) {
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
}

void render_ui() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("Controls");
        ImGui::Text("Arrows to move");
        ImGui::Text("Pos: %.2f, %.2f", g_state.player.x, g_state.player.y);
        ImGui::End();
    }

    ImGui::Render();
}

void render_game() {
    SDL_GetWindowSize(window, &g_state.screen_width, &g_state.screen_height);
    glViewport(0, 0, g_state.screen_width, g_state.screen_height);

    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);

    float camX = g_state.player.x;
    float camY = g_state.player.y;
    float aspect = (float)g_state.screen_width / (float)g_state.screen_height;
    float zoom = g_state.zoom.level;

    draw_grid(camX, camY, aspect, zoom);
    draw_planets(camX, camY, aspect, zoom);

    // Draw Player
    draw_triangle(triangleVbo, 0.0f, 0.0f, 0.05f * zoom, g_state.player.angle, 1.0f, 1.0f, 1.0f, program, aspect);
}
