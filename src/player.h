#ifndef PLAYER_H
#define PLAYER_H

#include <SDL.h>
#include <vector>
#include "battle.h"

struct Player {
    float x = 0.5f; // Center of tile (0,0)
    float y = 0.5f;
    float angle = 0.0f;
    std::vector<Card> deck;
    int difficulty = 0;

    Player();
};

void ensure_default_player_deck(Player& player);


#endif
