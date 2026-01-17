#include "player.h"
#include "cards.hpp"
#include <cmath>

Player::Player() {
    // Default deck uses the predefined decklist
    for (const auto& entry : cards::default_decklist()) {
        const Card* card = entry.first;
        int copies = entry.second;
        if (!card || copies <= 0) continue;
        for (int i = 0; i < copies; ++i) {
            deck.push_back(*card);
        }
    }
}

void ensure_default_player_deck(Player& player) {
    if (!player.deck.empty()) return;
    for (const auto& entry : cards::default_decklist()) {
        const Card* card = entry.first;
        int copies = entry.second;
        if (!card || copies <= 0) continue;
        for (int i = 0; i < copies; ++i) {
            player.deck.push_back(*card);
        }
    }
}
