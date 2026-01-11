#ifndef BATTLE_H
#define BATTLE_H

#include <string>
#include <vector>

enum class CardType {
    SHIELD,
    TURRET,
    DRONE
};

struct Card {
    std::string name;
    int hp;
    int max_hp;
    int dmg;
    CardType type;
};

struct BattleState {
    int player_hp = 10000;
    int opponent_hp = 10000;
    
    std::vector<Card> player_deck;
    std::vector<Card> player_hand;
    std::vector<Card> player_field[2][6]; // [row][col]
    
    std::vector<Card> opponent_deck;
    std::vector<Card> opponent_hand;
    std::vector<Card> opponent_field[2][6];

    bool is_player_turn = true;
    bool battle_animating = false;
    int selected_card_hand_idx = -1;
};

void battle_loop();
void init_battle(BattleState& state);
void start_random_battle();

#endif
