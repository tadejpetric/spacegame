#pragma once

#include "battle.h"

#include <array>
#include <string_view>
#include <vector>

namespace cards {

// Global card definitions (single instances, copied into decks as needed)
inline Card SHIELD{
    "Shield", 500, 500, 50, 50, 80, CardType::SHIELD, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card TURRET{
    "Turret", 200, 200, 200, 200, 130, CardType::TURRET, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card DRONE{
    "Drone", 300, 300, 100, 100, 50, CardType::DRONE, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card MECHANIC{
    "Mechanic",
    320,
    320,
    80,
    80,
    130,
    CardType::UTILITY,
    CardKind::SPECIAL,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& self = get_side_state(st, side);
        heal_all_slots(self, 100);
    },
    "Heal all friendly units by 100 HP",
    {}
};
inline Card BOMBARD{
    "Bombard",
    220,
    220,
    150,
    150,
    200,
    CardType::TURRET,
    CardKind::SPECIAL,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& enemy = get_side_state(st, opposite_side(side));
        deal_damage_to_all_slots(enemy, 100);
    },
    "Damage all enemy units by 100 HP",
    {}
};
inline Card ALTERNATOR{
    "Alternator",
    240,
    240,
    220,
    220,
    180,
    CardType::TURRET,
    CardKind::SPECIAL,
    [](BattleState& st, BattleSide side, int row, int col) {
        SideState& self = get_side_state(st, side);
        Card& card = self.field[row][col][0];
        if ((card.state.times_used % 2) == 0) {
            card.state.skip_this_turn = true; // fires every other turn
        }
    },
    "Fires only every other turn",
    {}
};
inline Card OVERHEAT{
    "Overheat",
    260,
    260,
    180,
    180,
    160,
    CardType::DRONE,
    CardKind::SPECIAL,
    [](BattleState& st, BattleSide side, int row, int col) {
        SideState& self = get_side_state(st, side);
        Card& card = self.field[row][col][0];
        if (card.state.times_used >= 3) {
            card.hp = 0; // destroy after 3 uses
        }
    },
    "Self-destructs after attacking 3 times",
    {}
};
inline Card GREED{
    "Greed",
    0,
    0,
    0,
    0,
    90,
    CardType::UTILITY,
    CardKind::IMMEDIATE,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& self = get_side_state(st, side);
        for (int i = 0; i < 2 && !self.deck.empty(); ++i) {
            self.hand.push_back(self.deck.back());
            self.deck.pop_back();
        }
    },
    "Draw 2 more cards",
    {}
};
inline Card BOMB{
    "Bomb",
    0,
    0,
    0,
    0,
    170,
    CardType::UTILITY,
    CardKind::IMMEDIATE,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& enemy = get_side_state(st, opposite_side(side));
        deal_damage_to_all_slots(enemy, 100);
        deal_damage_to_ship(enemy, 50);
    },
    "Deal 100 to all enemy units and 50 to the enemy ship",
    {}
};
inline Card CEASEFIRE{
    "Ceasefire",
    0,
    0,
    0,
    0,
    80,
    CardType::UTILITY,
    CardKind::IMMEDIATE,
    [](BattleState& st, BattleSide, int, int) {
        st.skip_attack_phase = true;
    },
    "End the attack phase for this turn",
    {}
};
inline Card BATTLE_DRILLS{
    "Battle Drills",
    0,
    0,
    0,
    0,
    250,
    CardType::TURRET,
    CardKind::FIELD_EFFECT,
    [](BattleState& st, BattleSide side, int, int) {
        get_side_state(st, side).damage_multiplier *= 2.0;
    },
    "Double your cards' damage",
    {}
};
inline Card REINFORCED_HULL{
    "Reinforced Hull",
    0,
    0,
    0,
    0,
    180,
    CardType::SHIELD,
    CardKind::FIELD_EFFECT,
    [](BattleState& st, BattleSide side, int, int) {
        heal_ship(get_side_state(st, side), 1000);
    },
    "Increase ship HP by 1000",
    {}
};
inline Card NANO_SURGE{
    "Nano Surge",
    0,
    0,
    0,
    0,
    140,
    CardType::UTILITY,
    CardKind::IMMEDIATE,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& self = get_side_state(st, side);
        heal_ship(self, 300);
        heal_all_slots(self, 50);
    },
    "Heal ship by 300 and all friendly units by 50",
    {}
};
inline Card DAMPENING_FIELD{
    "Dampening Field",
    0,
    0,
    0,
    0,
    160,
    CardType::UTILITY,
    CardKind::FIELD_EFFECT,
    [](BattleState& st, BattleSide side, int, int) {
        get_side_state(st, side).damage_multiplier *= 0.85;
    },
    "Reduce incoming damage taken by 15%",
    {}
};

inline const std::array<const Card*, 14> ALL = {
    &SHIELD,
    &TURRET,
    &DRONE,
    &MECHANIC,
    &BOMBARD,
    &ALTERNATOR,
    &OVERHEAT,
    &GREED,
    &BOMB,
    &CEASEFIRE,
    &BATTLE_DRILLS,
    &REINFORCED_HULL,
    &NANO_SURGE,
    &DAMPENING_FIELD,
};

inline const std::vector<std::pair<const Card*, int>> DEFAULT_DECKLIST = {
    {&SHIELD, 4},
    {&TURRET, 4},
    {&DRONE, 4},
    {&MECHANIC, 2},
    {&BOMBARD, 2},
    {&ALTERNATOR, 2},
    {&OVERHEAT, 2},
    {&GREED, 2},
    {&BOMB, 2},
    {&CEASEFIRE, 1},
    {&BATTLE_DRILLS, 1},
    {&REINFORCED_HULL, 1},
    {&NANO_SURGE, 2},
    {&DAMPENING_FIELD, 1},
};

inline const std::array<const Card*, 14>& all() {
    return ALL;
}

inline const Card* find_by_name(std::string_view name) {
    for (const Card* card : ALL) {
        if (card->name == name) return card;
    }
    return nullptr;
}

inline const std::vector<std::pair<const Card*, int>>& default_decklist() {
    return DEFAULT_DECKLIST;
}

} // namespace cards
