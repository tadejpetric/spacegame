#ifndef OVERWORLD_H
#define OVERWORLD_H

#include <SDL.h>
#include <SDL_opengles2.h>
#include <unordered_map>
#include <vector>
#include "player.h"
#include "battle.h"
#include "states.hpp"

// Constants
extern const float TILE_SIZE;
extern const int GRID_VIEW_RANGE;

struct ZoomState {
    float level = 1.0f;
    const float min = 0.2f;
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
class WorldMap {
    public:
    WorldMap();
    std::unordered_map<Point, Chunk, PointHash> chunks;
    Tiles get_tile_at(int x, int y);
    std::vector<std::pair<Point, Chunk>> get_active_chunks(int center_x, int center_y, int range);
    void generate_chunk(int chunk_x, int chunk_y);
    std::pair<float, float> chunk_to_world(Point chunk_coord) {
        return {chunk_coord.first * Chunk::SIZE * TILE_SIZE, chunk_coord.second * Chunk::SIZE * TILE_SIZE};
    }
};

struct GameState {
    Player player;
    WorldMap world_map;
    bool keys[SDL_NUM_SCANCODES] = {false};
    ZoomState zoom;
    int screen_width = 800;
    int screen_height = 600;
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
