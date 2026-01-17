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
int PlanetGenerator::hash(int x, int y) const {
    int h = g_state.seed;
    h ^= x * 73856093ULL;
    h ^= y * 19349663ULL;
    h ^= (h >> 13);
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= (h >> 13);
    return h;
}
Point PlanetGenerator::get_planet_in_cell(int cell_x, int cell_y) const {
    int h = hash(cell_x, cell_y);
    uint64_t seed = hash(cell_x, cell_y);
    int offsetX = seed % cell_size;
    int offsetY = (seed >> 16) % cell_size;
    return {cell_x * cell_size + offsetX, cell_y * cell_size + offsetY};
}
bool PlanetGenerator::is_planet_at(int tile_x, int tile_y) const {
    auto [cell_x, cell_y] = tile_to_cell(tile_x, tile_y);
    auto [planet_x, planet_y] = get_planet_in_cell(cell_x, cell_y);
    return (planet_x == tile_x && planet_y == tile_y);
}
WorldMap::WorldMap(int seed) {
    // Initial world generation can be done here if needed
    terrainNoise.SetSeed(seed);
    terrainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    terrainNoise.SetFrequency(0.03f); // Controls how "big" the zones are
    terrainNoise.SetFractalType(FastNoiseLite::FractalType_Ridged);
    terrainNoise.SetFractalOctaves(2);
    terrainNoise.SetFractalLacunarity(1.0f);
    terrainNoise.SetFractalGain(35.0f);
    terrainNoise.SetFractalWeightedStrength(0.07f);
    terrainNoise.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2);
    terrainNoise.SetDomainWarpAmp(2.5f);
   

    asteroidNoise.SetSeed(seed);
    asteroidNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    asteroidNoise.SetFrequency(0.3f); // Controls how "big" the zones are
    // terrainNoise.SetCellularReturnType(FastNoiseLite::CellularReturnType_CellValue);
    // terrainNoise.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Hybrid);

    pathNoise.SetSeed(seed + 123);
    pathNoise.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    pathNoise.SetFractalType(FastNoiseLite::FractalType_Ridged);
    pathNoise.SetFrequency(0.005f); // Controls path density
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
void WorldMap::set_active_chunks() {
    // std::vector<std::pair<Point, Chunk>> active_chunks;
    float camX = g_state.player.x;
    float camY = g_state.player.y;
    float aspect = (float)g_state.screen_width / (float)g_state.screen_height;
    float zoom = g_state.zoom.level;
    auto [start_tile, end_tile] = get_visible_tile_range(camX, camY, aspect, zoom);
    int start_chunk_x = std::floor((TILE_SIZE * start_tile.first) / Chunk::SIZE);
    int end_chunk_x = std::ceil((TILE_SIZE * end_tile.first) / Chunk::SIZE);
    int start_chunk_y = std::floor((TILE_SIZE * start_tile.second) / Chunk::SIZE);
    int end_chunk_y = std::ceil((TILE_SIZE * end_tile.second) / Chunk::SIZE);
    this->active_chunks.clear();
    for (int cx = start_chunk_x; cx <= end_chunk_x; ++cx) {
        for (int cy = start_chunk_y; cy <= end_chunk_y; ++cy) {
            auto it = chunks.find({cx, cy});
            if (it == chunks.end()) {
                generate_chunk(cx, cy);
                it = chunks.find({cx, cy});
            }
            this->active_chunks.push_back({{cx, cy}, it->second});
        }
    }
}

void WorldMap::generate_chunk(int chunk_x, int chunk_y) {
    Chunk new_chunk;
    for (int x = 0; x < Chunk::SIZE; ++x) {
        for (int y = 0; y < Chunk::SIZE; ++y) {
            int world_x = chunk_x * Chunk::SIZE + x;
            int world_y = chunk_y * Chunk::SIZE + y;

            float terrain_value = terrainNoise.GetNoise((float)world_x, (float)world_y);
            float asteroid_value = asteroidNoise.GetNoise((float)world_x, (float)world_y);
            float path_value = pathNoise.GetNoise((float)world_x, (float)world_y);

            static std::mt19937 ruin_rng(time(0));
            std::uniform_int_distribution<int> dist(1, 100);
            if (terrain_value > 0.5f) {
                if (pl_gen.is_planet_at(world_x, world_y)) {
                    new_chunk.tiles[x][y] = Tiles::PLANET;
                } else if (asteroid_value > 0.4f) {
                    new_chunk.tiles[x][y] = Tiles::ASTEROID;
                } else {
                    new_chunk.tiles[x][y] = Tiles::EMPTY;
                }
            } else if (terrain_value < -0.7f && dist(ruin_rng) <= 1) {
                new_chunk.tiles[x][y] = Tiles::RESOURCES;
            } 
            else {
                new_chunk.tiles[x][y] = Tiles::DANGEROUS;
            }
        }
    }
    chunks[{chunk_x, chunk_y}] = new_chunk;
}

std::pair<Point, Point> WorldMap::get_visible_tile_range(float camX, float camY, float aspect, float zoom) {
    int viewRange = (int)(2 / zoom);
    int startX = (int)floor((camX - viewRange * TILE_SIZE) / TILE_SIZE);
    int endX = (int)ceil((camX + viewRange * TILE_SIZE) / TILE_SIZE);
    int startY = (int)floor((camY - viewRange * TILE_SIZE) / TILE_SIZE);
    int endY = (int)ceil((camY + viewRange * TILE_SIZE) / TILE_SIZE);
    return {{startX, startY}, {endX, endY}};
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
                g_state.world_map.set_active_chunks();
            }

            // Zoom controls
            if (event.key.keysym.scancode == SDL_SCANCODE_EQUALS || event.key.keysym.scancode == SDL_SCANCODE_KP_PLUS) {
                g_state.zoom.level += g_state.zoom.speed;
                if (g_state.zoom.level > g_state.zoom.max) g_state.zoom.level = g_state.zoom.max;
                g_state.world_map.set_active_chunks();
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_MINUS || event.key.keysym.scancode == SDL_SCANCODE_KP_MINUS) {
                g_state.zoom.level -= g_state.zoom.speed;
                if (g_state.zoom.level < g_state.zoom.min) g_state.zoom.level = g_state.zoom.min;
                g_state.world_map.set_active_chunks();
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

    auto active_chunks = g_state.world_map.get_active_chunks();
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
    float worldX = ((float)tile_x + 0.5f) * TILE_SIZE;
    float worldY = ((float)tile_y + 0.5f) * TILE_SIZE;
    float screenX = (worldX - camX) / (aspect / zoom);
    float screenY = (worldY - camY) / (1.0f / zoom);

    switch (tile_type) {
        case Tiles::PLANET:
            draw_disc(circleVbo, screenX, screenY, 0.9f * zoom, 0.0f, 0.5f, 1.0f, program, aspect);
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
        case Tiles::RESOURCES:
            // Color the entire tile red
            screenX = (tile_x - camX) / (aspect / zoom);
            screenY = (tile_y - camY) / (1.0f / zoom);
            // draw yellow square
            draw_square(squareVbo, screenX, screenY, TILE_SIZE * zoom, 0.5f, 0.5f, 0.0f, 1.0f, program, aspect);
        
            break;
        default:
            break;
    }

}
void draw_map(float camX, float camY, float aspect, float zoom) {
    auto active_chunks = g_state.world_map.get_active_chunks();
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
                draw_tile(world_chunk_x + x, world_chunk_y + y, tile, camX, camY, aspect, zoom);
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
