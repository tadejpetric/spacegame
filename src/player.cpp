#include "player.h"
#include <cmath>

void update_player(Player& player, const bool* keys) {
    const float rotation_speed = 0.1f;
    const float thrust = 0.0005f;
    const float friction = 0.99f;

    if (keys[SDL_SCANCODE_LEFT]) player.angle += rotation_speed;
    if (keys[SDL_SCANCODE_RIGHT]) player.angle -= rotation_speed;
    if (keys[SDL_SCANCODE_UP]) {
        player.vx += cos(player.angle) * thrust;
        player.vy += sin(player.angle) * thrust;
    }

    player.x += player.vx;
    player.y += player.vy;
    player.vx *= friction;
    player.vy *= friction;
}
