#ifndef OVERWORLD_H
#define OVERWORLD_H

#include <SDL.h>
#include <SDL_opengles2.h>
#include <vector>
#include "player.h"

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

struct GameState {
    Player player;
    std::vector<Planet> planets;
    bool keys[SDL_NUM_SCANCODES] = {false};
    ZoomState zoom;
    int screen_width = 800;
    int screen_height = 600;
};

extern GameState g_state;
extern SDL_Window* window;
extern GLuint program;
extern GLuint triangleVbo;
extern GLuint circleVbo;
extern GLuint lineVbo;

void overworld_loop();
void handle_events();
void render_ui();
void render_game();

#endif // OVERWORLD_H
