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

    int player_ship_last_damage = 0;
    int opponent_ship_last_damage = 0;
    int player_slot_last_damage[2][6] = {};
    int opponent_slot_last_damage[2][6] = {};

    // Animation state
    int anim_step_index = -1;
    double anim_step_start_time = 0.0;
    bool anim_damage_applied = false;
    bool anim_initial_wait = false;
};

void battle_loop();
void init_battle(BattleState& state);
void start_random_battle();

#endif
