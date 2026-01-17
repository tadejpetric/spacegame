#pragma once

#include "battle.h"

#include <array>
#include <algorithm>
#include <optional>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

namespace cards {

inline std::mt19937& card_rng() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    return rng;
}

inline void apply_damage_to_slot(SideState& side, int row, int col, int dmg) {
    if (dmg <= 0) return;
    Card& target = side.field[row][col][0];
    if (target.hp <= 0) return;
    int applied = std::min(dmg, target.hp);
    target.hp -= applied;
    side.slot_last_damage[row][col] += applied;
    card_took_damage_trigger(side, row, col, applied);
}

inline void apply_heal_to_slot(SideState& side, int row, int col, int amount) {
    if (amount <= 0) return;
    Card& target = side.field[row][col][0];
    if (target.hp <= 0) return;
    int before = target.hp;
    target.hp = std::min(target.max_hp, target.hp + amount);
    int healed = target.hp - before;
    if (healed > 0) {
        side.slot_last_heal[row][col] += healed;
    }
}

inline std::optional<std::pair<int, int>> random_live_slot(SideState& side) {
    std::vector<std::pair<int, int>> slots;
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            if (!side.field[r][c].empty() && side.field[r][c][0].hp > 0) {
                slots.emplace_back(r, c);
            }
        }
    }
    if (slots.empty()) return std::nullopt;
    std::uniform_int_distribution<size_t> dist(0, slots.size() - 1);
    return slots[dist(card_rng())];
}

inline int count_live_cards(const SideState& side) {
    int count = 0;
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            if (!side.field[r][c].empty() && side.field[r][c][0].hp > 0 && !side.field[r][c][0].name.empty()) {
                ++count;
            }
        }
    }
    return count;
}

// Global card definitions (single instances, copied into decks as needed)
inline Card SHIELD{
    "Shield", 500, 500, 50, 50, 80, CardType::SHIELD, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card SHIELD_MK2{
    "Shield Mk2", 900, 900, 80, 80, 300, CardType::SHIELD, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card SHIELD_MK3{
    "Shield Mk3", 1400, 1400, 120, 120, 600, CardType::SHIELD, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card TURRET{
    "Turret", 200, 200, 200, 200, 100, CardType::TURRET, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card TURRET_MK2{
    "Turret Mk2", 260, 260, 320, 320, 340, CardType::TURRET, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card TURRET_MK3{
    "Turret Mk3", 320, 320, 480, 480, 550, CardType::TURRET, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card DRONE{
    "Drone", 300, 300, 100, 100, 50, CardType::DRONE, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card DRONE_MK2{
    "Drone Mk2", 450, 450, 170, 170, 270, CardType::DRONE, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card DRONE_MK3{
    "Drone Mk3", 620, 620, 240, 240, 430, CardType::DRONE, CardKind::NORMAL, {}, std::nullopt, {}
};
inline Card MECHANIC{
    "Mechanic",
    320,
    320,
    80,
    80,
    500,
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
    800,
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
    400,
    400,
    350,
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
    300,
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
inline Card OVERHEAT_MK2{
    "Overheat Mk2",
    300,
    300,
    240,
    240,
    500,
    CardType::DRONE,
    CardKind::SPECIAL,
    [](BattleState& st, BattleSide side, int row, int col) {
        SideState& self = get_side_state(st, side);
        Card& card = self.field[row][col][0];
        if (card.state.times_used >= 3) {
            SideState& enemy = get_side_state(st, opposite_side(side));
            auto target_slot = random_live_slot(enemy);
            if (target_slot) {
                apply_damage_to_slot(enemy, target_slot->first, target_slot->second, 1000);
            }
            card.hp = 0;
        }
    },
    "After 3 attacks, explodes for 1000 damage to a random enemy card",
    {}
};
inline Card OVERHEAT_MK3{
    "Overheat Mk3",
    340,
    340,
    260,
    260,
    650,
    CardType::DRONE,
    CardKind::SPECIAL,
    [](BattleState& st, BattleSide side, int row, int col) {
        SideState& self = get_side_state(st, side);
        Card& card = self.field[row][col][0];
        if (card.state.times_used >= 3) {
            SideState& enemy = get_side_state(st, opposite_side(side));
            deal_damage_to_all_slots(enemy, 1000);
            card.hp = 0;
        }
    },
    "After 3 attacks, explodes for 1000 damage to all enemy cards",
    {}
};
inline Card GREED{
    "Greed",
    0,
    0,
    0,
    0,
    130,
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
inline Card GREED_MK2{
    "Greed Mk2",
    0,
    0,
    0,
    0,
    200,
    CardType::UTILITY,
    CardKind::IMMEDIATE,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& self = get_side_state(st, side);
        for (int i = 0; i < 3 && !self.deck.empty(); ++i) {
            self.hand.push_back(self.deck.back());
            self.deck.pop_back();
        }
    },
    "Draw 3 more cards",
    {}
};
inline Card GREED_MK3{
    "Greed Mk3",
    0,
    0,
    0,
    0,
    260,
    CardType::UTILITY,
    CardKind::IMMEDIATE,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& self = get_side_state(st, side);
        for (int i = 0; i < 4 && !self.deck.empty(); ++i) {
            self.hand.push_back(self.deck.back());
            self.deck.pop_back();
        }
    },
    "Draw 4 more cards",
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
inline Card BOMB_MK2{
    "Bomb Mk2",
    0,
    0,
    0,
    0,
    350,
    CardType::UTILITY,
    CardKind::IMMEDIATE,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& enemy = get_side_state(st, opposite_side(side));
        SideState& self = get_side_state(st, side);
        deal_damage_to_all_slots(enemy, 500);
        deal_damage_to_all_slots(self, 100);
    },
    "Deal 500 to all enemy units and 100 to all friendly units",
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
    2000,
    CardType::TURRET,
    CardKind::FIELD_EFFECT,
    [](BattleState& st, BattleSide side, int, int) {
        get_side_state(st, side).damage_multiplier *= 1.5;
    },
    "Increases your cards' damage by 50% (mult)",
    {}
};
inline Card REINFORCED_HULL{
    "Reinforced Hull",
    0,
    0,
    0,
    0,
    1000,
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
    1000,
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
    1500,
    CardType::UTILITY,
    CardKind::FIELD_EFFECT,
    [](BattleState& st, BattleSide side, int, int) {
        // Reduce incoming damage by lowering the opponent's damage multiplier
        get_side_state(st, opposite_side(side)).damage_multiplier *= 0.85;
    },
    "Reduce incoming damage taken by 15%",
    {}
};
inline Card FRAGILE_LENS{
    "Fragile Lens",
    50,
    50,
    40,
    40,
    300,
    CardType::UTILITY,
    CardKind::SPECIAL,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& self = get_side_state(st, side);
        self.damage_multiplier *= 1.2;
    },
    "Each turn on the field, increase your damage multiplier by 10%",
    {}
};
inline Card BLURRED_LENS{
    "Blurred Lens",
    50,
    50,
    40,
    40,
    320,
    CardType::UTILITY,
    CardKind::SPECIAL,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& enemy = get_side_state(st, opposite_side(side));
        enemy.damage_multiplier *= 0.8;
    },
    "Each turn on the field, reduce the opponent's damage multiplier by 10%",
    {}
};
inline Card PIERCING_BEAM{
    "Piercing Beam",
    240,
    240,
    260,
    260,
    600,
    CardType::TURRET,
    CardKind::SPECIAL,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& enemy = get_side_state(st, opposite_side(side));
        deal_damage_to_ship(enemy, 100);
    },
    "Each attack also deals 100 damage to the enemy ship",
    {}
};
inline Card GUARDIAN_ANGEL{
    "Guardian Angel",
    520,
    520,
    100,
    100,
    380,
    CardType::SHIELD,
    CardKind::SPECIAL,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& self = get_side_state(st, side);
        auto slot = random_live_slot(self);
        if (slot) {
            apply_heal_to_slot(self, slot->first, slot->second, 120);
        }
    },
    "Heals a random friendly unit by 120 each turn it's on the field",
    {}
};
inline Card REACTOR_OVERDRIVE{
    "Reactor Overdrive",
    0,
    0,
    0,
    0,
    2200,
    CardType::UTILITY,
    CardKind::FIELD_EFFECT,
    [](BattleState& st, BattleSide side, int, int) {
        SideState& self = get_side_state(st, side);
        self.damage_multiplier *= 1.25;
        deal_damage_to_ship(self, 400);
    },
    "Boost your damage by 25% but deal 400 damage to your ship",
    {}
};
inline Card LONE_WOLF{
    "Lone Wolf",
    1000,
    1000,
    6000,
    6000,
    1600,
    CardType::TURRET,
    CardKind::SPECIAL,
    [](BattleState& st, BattleSide side, int row, int col) {
        SideState& self = get_side_state(st, side);
        int live = count_live_cards(self);
        int other = std::max(0, live - 1);
        Card& card = self.field[row][col][0];
        int new_dmg = std::max(1, 6000 - 2000 * other);
        card.dmg = new_dmg;
        card.base_dmg = new_dmg;
        int new_hp_cap = std::max(1, 1000 - 333 * live);
        card.max_hp = new_hp_cap;
        if (card.hp > new_hp_cap) card.hp = new_hp_cap;
    },
    "Damage 6000 minus 2000 per other friendly card; HP 1000 minus 333 per friendly card (min 1)",
    {}
};
inline Card MASOCHIST{
    "Masochist",
    800,
    800,
    150,
    150,
    350,
    CardType::SHIELD,
    CardKind::NORMAL,
    {},
    "When it takes damage, heal your ship by 500",
    {}
};
inline Card GLASS_CANNON{
    "Glass Cannon",
    0,
    0,
    0,
    0,
    900,
    CardType::UTILITY,
    CardKind::IMMEDIATE,
    [](BattleState& st, BattleSide side, int, int) {
        get_side_state(st, side).damage_multiplier *= 3.0;
        get_side_state(st, opposite_side(side)).damage_multiplier *= 3.0;
    },
    "Immediately triples both sides' damage multipliers",
    {}
};

inline const std::array ALL{
    &SHIELD,
    &SHIELD_MK2,
    &SHIELD_MK3,
    &TURRET,
    &TURRET_MK2,
    &TURRET_MK3,
    &DRONE,
    &DRONE_MK2,
    &DRONE_MK3,
    &MECHANIC,
    &BOMBARD,
    &ALTERNATOR,
    &OVERHEAT,
    &OVERHEAT_MK2,
    &OVERHEAT_MK3,
    &GREED,
    &GREED_MK2,
    &GREED_MK3,
    &BOMB,
    &BOMB_MK2,
    &CEASEFIRE,
    &BATTLE_DRILLS,
    &REINFORCED_HULL,
    &NANO_SURGE,
    &DAMPENING_FIELD,
    &FRAGILE_LENS,
    &BLURRED_LENS,
    &PIERCING_BEAM,
    &GUARDIAN_ANGEL,
    &REACTOR_OVERDRIVE,
    &LONE_WOLF,
    &MASOCHIST,
    &GLASS_CANNON,
};

inline const std::vector<std::pair<const Card*, int>> DEFAULT_DECKLIST{
    {&SHIELD, 1},
    {&TURRET, 2},
    {&DRONE, 1},
    {&MECHANIC, 1},
    {&OVERHEAT_MK2, 1},
    {&GREED, 1},
    {&BOMB, 1},
    {&BOMB_MK2, 1},
    {&FRAGILE_LENS, 1},
    {&GLASS_CANNON, 1},
};

inline const auto& all() {
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

inline std::vector<Card> generate_deck_with_cost(int cost_limit) {
    std::vector<Card> deck;
    if (cost_limit <= 0 || ALL.empty()) return deck;

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(0, ALL.size() - 1);

    int total_cost = 0;
    while (total_cost <= cost_limit) {
        const Card* drawn = ALL[dist(rng)];
        if (!drawn) continue;
        deck.push_back(*drawn);
        total_cost += drawn->cost;
    }
    return deck;
}

} // namespace cards
