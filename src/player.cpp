#include "player.h"
#include <cmath>

void update_player(Player& player, const bool* keys) {
    // Tile-based movement is handled via SDL_KEYDOWN events in main_loop
    // for discrete steps, so this function can remain for continuous logic
    // if needed, or we move logic to main_loop.
    // However, the user wants "I press up, the player moves one tile up".
}
