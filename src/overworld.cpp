#include "overworld.h"
#include <emscripten.h>
#include <cmath>
#include <random>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "geometry.h"

const float TILE_SIZE = 1.0f;
const int GRID_VIEW_RANGE = 20;

void overworld_loop() {
    //ensure_default_player_deck(g_state.player);

    handle_events();
    render_ui();
    render_game();
}

Chunk::Chunk() {
    for (int x = 0; x < SIZE; ++x) {
        for (int y = 0; y < SIZE; ++y) {
            tiles[x][y] = Tiles::EMPTY;
        }
    }
}
Tiles Chunk::get_tile(int x, int y) const {
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) {
        return Tiles::EMPTY;
    }
    return tiles[x][y];
}
WorldMap::WorldMap() {
    // Initial world generation can be done here if needed
}
Tiles WorldMap::get_tile_at(int x, int y) {
    int chunk_x = x / Chunk::SIZE;
    int chunk_y = y / Chunk::SIZE;
    int local_x = x % Chunk::SIZE;
    int local_y = y % Chunk::SIZE;

    auto it = chunks.find({chunk_x, chunk_y});
    if (it == chunks.end()) {
        generate_chunk(chunk_x, chunk_y);
        it = chunks.find({chunk_x, chunk_y});
    }
    return it->second.get_tile(local_x, local_y);
}
std::vector<std::pair<Point, Chunk>> WorldMap::get_active_chunks(int center_x, int center_y, int range) {
    std::vector<std::pair<Point, Chunk>> active_chunks;
    int start_chunk_x = (center_x - range) / Chunk::SIZE;
    int end_chunk_x = (center_x + range) / Chunk::SIZE;
    int start_chunk_y = (center_y - range) / Chunk::SIZE;
    int end_chunk_y = (center_y + range) / Chunk::SIZE;

    for (int cx = start_chunk_x; cx <= end_chunk_x; ++cx) {
        for (int cy = start_chunk_y; cy <= end_chunk_y; ++cy) {
            auto it = chunks.find({cx, cy});
            if (it == chunks.end()) {
                generate_chunk(cx, cy);
                it = chunks.find({cx, cy});
            }
            active_chunks.push_back({{cx, cy}, it->second});
        }
    }
    return active_chunks;
}

void WorldMap::generate_chunk(int chunk_x, int chunk_y) {
    Chunk new_chunk;
    // Simple random generation for demonstration
    std::mt19937 rng(static_cast<unsigned int>(chunk_x * 73856093 ^ chunk_y * 19349663));
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int x = 0; x < Chunk::SIZE; ++x) {
        for (int y = 0; y < Chunk::SIZE; ++y) {
            float chance = dist(rng);
            if (chance < 0.1f) {
                new_chunk.tiles[x][y] = Tiles::ASTEROID;
            } else if (chance < 0.2f) {
                new_chunk.tiles[x][y] = Tiles::DANGEROUS;
            } else {
                new_chunk.tiles[x][y] = Tiles::EMPTY;
            }
        }
    }
    chunks[{chunk_x, chunk_y}] = new_chunk;
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
            bool moved = false;
            if (event.key.keysym.scancode == SDL_SCANCODE_UP) {
                g_state.player.y += TILE_SIZE;
                g_state.player.angle = M_PI / 2.0f;
                moved = true;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_DOWN) {
                g_state.player.y -= TILE_SIZE;
                g_state.player.angle = -M_PI / 2.0f;
                moved = true;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_LEFT) {
                g_state.player.x -= TILE_SIZE;
                g_state.player.angle = M_PI;
                moved = true;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
                g_state.player.x += TILE_SIZE;
                g_state.player.angle = 0.0f;
                moved = true;
            }

            if (moved) {
                // Not a fan of this
                static std::mt19937 battle_rng(time(0));
                std::uniform_int_distribution<int> dist(1, 50);
                g_state.player.difficulty += 20;
                if (dist(battle_rng) == 1) {
                    start_random_battle(g_state.player.deck, g_state.player.difficulty);
                }
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


void debug_chunks() {
    ImGui::Begin("Active Chunks");
    ImGui::Text("Active Chunks:");
    auto active_chunks = g_state.world_map.get_active_chunks(
        (int)(g_state.player.x),
        (int)(g_state.player.y),
        GRID_VIEW_RANGE
    );
    for (const auto& chunk_pair : active_chunks) {
        const Point& coord = chunk_pair.first;
        ImGui::Text("   Chunk (%d, %d)", coord.first, coord.second);
    }
    ImGui::Text("All Chunks:");
    for (const auto& chunk_pair : g_state.world_map.chunks) {
        const Point& coord = chunk_pair.first;
        ImGui::Text("   Chunk (%d, %d)", coord.first, coord.second);
    }
    ImGui::End();
}

void render_ui() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Controls");
    ImGui::Text("Arrows to move");
    ImGui::Text("Pos: %.2f, %.2f", g_state.player.x, g_state.player.y);
    ImGui::End();

    ImGui::Begin("Player Status");
    ImGui::Text("Difficulty: %d", g_state.player.difficulty);
    ImGui::Separator();
    ImGui::Text("Deck (%zu cards):", g_state.player.deck.size());
    if (ImGui::BeginChild("DeckList", ImVec2(0, 150), true)) {
        for (const auto& card : g_state.player.deck) {
            ImGui::TextUnformatted(card.name.c_str());
        }
    }
    ImGui::EndChild();
    ImGui::End();
    
    debug_chunks();
    
    ImGui::Render();
}
void draw_tile(int tile_x, int tile_y, Tiles tile_type, float camX, float camY, float aspect, float zoom) {
    float worldX = tile_x + TILE_SIZE * 0.5f;
    float worldY = tile_y + TILE_SIZE * 0.5f;
    float screenX = (worldX - camX) / (aspect / zoom);
    float screenY = (worldY - camY) / (1.0f / zoom);

    switch (tile_type) {
        case Tiles::PLANET:
            draw_disc(circleVbo, screenX, screenY, 0.3f * zoom, 0.0f, 0.5f, 1.0f, program, aspect);
            break;
        case Tiles::ASTEROID:
            // Draw a couple of small circles to represent an asteroid
            draw_disc(circleVbo, screenX, screenY, 0.1f * zoom, 0.5f, 0.5f, 0.5f, program, aspect);
            break;
        case Tiles::DANGEROUS:
            // Color the entire tile red
            screenX = (tile_x - camX) / (aspect / zoom);
            screenY = (tile_y - camY) / (1.0f / zoom);
            draw_square(squareVbo, screenX, screenY, TILE_SIZE * zoom, 1.0f, 0.0f, 0.0f, 0.3f, program, aspect);
        
            break;
        default:
            break;
    }

}
void draw_map(float camX, float camY, float aspect, float zoom) {
    auto active_chunks = g_state.world_map.get_active_chunks(
        (int)(g_state.player.x),
        (int)(g_state.player.y),
        GRID_VIEW_RANGE
    );
    for (const auto& chunk_pair : active_chunks) {
        const Point& chunk_coord = chunk_pair.first;
        const Chunk& chunk = chunk_pair.second;
        auto [i, j] = chunk_coord;
        auto [world_chunk_x, world_chunk_y] = g_state.world_map.chunk_to_world(chunk_coord);
        for (int x = 0; x < Chunk::SIZE; ++x) {
            for (int y = 0; y < Chunk::SIZE; ++y) {
                Tiles tile = chunk.get_tile(x, y);
                // Render tile based on its type
                // e.g., draw different shapes/colors for different tile types
                draw_tile(world_chunk_x + x * TILE_SIZE, world_chunk_y + y * TILE_SIZE, tile, camX, camY, aspect, zoom);
            }
        }
    }
    
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
    draw_map(camX, camY, aspect, zoom);
    // draw_planets(camX, camY, aspect, zoom);

    // Draw Player
    draw_triangle(triangleVbo, 0.0f, 0.0f, 0.05f * zoom, g_state.player.angle, 1.0f, 1.0f, 1.0f, program, aspect);


}
