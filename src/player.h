#ifndef PLAYER_H
#define PLAYER_H

#include <SDL.h>

struct Player {
    float x = 0.5f; // Center of tile (0,0)
    float y = 0.5f;
    float angle = 0.0f;
};

void update_player(Player& player, const bool* keys);

#endif
