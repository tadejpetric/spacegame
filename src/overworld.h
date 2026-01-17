#ifndef OVERWORLD_H
#define OVERWORLD_H

#include <SDL.h>
#include <SDL_opengles2.h>
#include <unordered_map>
#include <vector>
#include "player.h"
#include "battle.h"
#include "states.hpp"
#include <FastNoiseLite.h>
// Constants
extern const float TILE_SIZE;
extern const int GRID_VIEW_RANGE;

struct ZoomState {
    float level = 1.0f;
    const float min = 0.02f;
    const float max = 5.0f;
    const float speed = 0.1f;
};

struct Planet {
    float x, y;
    float radius;
    float r, g, b;
};

using Point = std::pair<int, int>;

enum class Tiles {
    EMPTY,
    DANGEROUS,
    PLANET,
    ASTEROID,
    SHOP,
    RESOURCES
};
class Chunk {
public:
    static const int SIZE = 16;
    Tiles tiles[SIZE][SIZE];

    Chunk();
    Tiles get_tile(int x, int y) const;
};
struct PointHash {
    inline size_t operator()(const Point & v) const {
        // A simple hash combination algorithm
        return std::hash<int>{}(v.first) ^ (std::hash<int>{}(v.second) << 1);
    }
};
class PlanetGenerator {
    static const int cell_size = Chunk::SIZE * 6; // Each cell covers multiple chunks
    public:
    int hash(int x, int y) const;
    Point tile_to_cell(int tile_x, int tile_y) const {
        int cell_x = std::floor((float)(tile_x * TILE_SIZE) / (float)cell_size);
        int cell_y = std::floor((float)(tile_y * TILE_SIZE) / (float)cell_size);
        return {cell_x, cell_y};
    }
    Point get_planet_in_cell(int cell_x, int cell_y) const;
    bool is_planet_at(int tile_x, int tile_y) const;
};
class WorldMap {
    int seed;
    FastNoiseLite terrainNoise;
    FastNoiseLite asteroidNoise;
    FastNoiseLite pathNoise;
    PlanetGenerator pl_gen;
    std::vector<std::pair<Point, Chunk>> active_chunks;
    public:
    WorldMap(int seed = 1);
    std::unordered_map<Point, Chunk, PointHash> chunks;
    Tiles get_tile_at(int x, int y);
    void set_active_chunks();
    std::pair<Point, Point> get_visible_tile_range(float camX, float camY, float aspect, float zoom);
    void generate_chunk(int chunk_x, int chunk_y);
    std::pair<float, float> chunk_to_world(Point chunk_coord) {
        return {chunk_coord.first * Chunk::SIZE * TILE_SIZE, chunk_coord.second * Chunk::SIZE * TILE_SIZE};
    }
    std::vector<std::pair<Point, Chunk>> get_active_chunks() {
        return active_chunks;
    }
    
};

struct GameState {
    Player player;
    WorldMap world_map;
    bool keys[SDL_NUM_SCANCODES] = {false};
    ZoomState zoom;
    int screen_width = 800;
    int screen_height = 600;
    int seed = 123;
};
extern GameState g_state;
extern BattleState g_battle;
extern SDL_Window* window;
extern GLuint program;
extern GLuint triangleVbo;
extern GLuint circleVbo;
extern GLuint squareVbo;
extern GLuint lineVbo;


void overworld_loop();
void handle_events();
void render_ui();
void render_game();

#endif // OVERWORLD_H
