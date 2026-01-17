#ifndef BATTLE_H
#define BATTLE_H

#include <string>
#include <vector>
#include <optional>
#include <functional>

enum class CardType {
    SHIELD,
    TURRET,
    DRONE,
    UTILITY,
    DEFAULT
};

enum class BattleSide {
    PLAYER,
    OPPONENT
};

enum class CardKind {
    NORMAL,
    SPECIAL,
    IMMEDIATE,
    FIELD_EFFECT
};

struct CardState {
    int times_used = 0;
    int cooldown = 0;
    bool skip_this_turn = false;
};

using CardEffect = std::function<void(struct BattleState&, BattleSide, int row, int col)>;

struct Card {
    std::string name;
    int hp;
    int max_hp;
    int dmg;
    int base_dmg;
    int cost = 0;
    CardType type;
    CardKind kind = CardKind::NORMAL;
    CardEffect effect;
    std::optional<std::string> effect_description;
    CardState state;
};

struct SideState {
    int hp = 10000;
    double damage_multiplier = 1.0;
    std::vector<Card> deck;
    std::vector<Card> hand;
    std::vector<Card> field[2][6];
    std::vector<Card> immediate_queue;
    int ship_last_damage = 0;
    int ship_last_heal = 0;
    int slot_last_damage[2][6] = {};
    int slot_last_heal[2][6] = {};
};

struct BattleState {
    SideState player;
    SideState opponent;

    bool is_player_turn = true;
    int difficulty = 1;
    bool battle_animating = false;
    bool skip_attack_phase = false;
    int selected_card_hand_idx = -1;
    std::vector<Card>* player_deck_ref = nullptr;
    std::optional<Card> reward_card;
    bool reward_added = false;

    // Animation state
    int anim_step_index = -1;
    double anim_step_start_time = 0.0;
    bool anim_damage_applied = false;
    bool anim_initial_wait = false;

    std::vector<std::string> action_log;
};

SideState& get_side_state(BattleState& state, BattleSide side);
const SideState& get_side_state(const BattleState& state, BattleSide side);
BattleSide opposite_side(BattleSide side);
void deal_damage_to_all_slots(SideState& side, int amount);
void heal_all_slots(SideState& side, int amount);
void deal_damage_to_ship(SideState& side, int dmg);
void heal_ship(SideState& side, int amount);

void battle_loop();
void init_battle(BattleState& state, std::vector<Card>& player_deck, int difficulty);
void start_random_battle(std::vector<Card>& player_deck, int difficulty);

#endif
