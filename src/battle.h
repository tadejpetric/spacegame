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

enum class BattleSide {
    PLAYER,
    OPPONENT
};

struct SideState {
    int hp = 10000;
    std::vector<Card> deck;
    std::vector<Card> hand;
    std::vector<Card> field[2][6];
    int ship_last_damage = 0;
    int slot_last_damage[2][6] = {};
};

struct BattleState {
    SideState player;
    SideState opponent;

    bool is_player_turn = true;
    bool battle_animating = false;
    int selected_card_hand_idx = -1;

    // Animation state
    int anim_step_index = -1;
    double anim_step_start_time = 0.0;
    bool anim_damage_applied = false;
    bool anim_initial_wait = false;
};

SideState& get_side_state(BattleState& state, BattleSide side);
const SideState& get_side_state(const BattleState& state, BattleSide side);

void battle_loop();
void init_battle(BattleState& state);
void start_random_battle();

#endif
