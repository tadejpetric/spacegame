#ifndef PLAYER_H
#define PLAYER_H

#include <SDL.h>

struct Player {
    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
};

void update_player(Player& player, const bool* keys);

#endif
