#include "battle.h"
#include "cards.hpp"
#include "overworld.h"
#include "states.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <emscripten.h>
#include <algorithm>
#include <random>
#include <ctime>
#include <cmath>

// Internal battle helper
static std::mt19937& battle_rng() {
    static std::mt19937 rng(time(0));
    return rng;
}

static const char* side_label(BattleSide side) {
    return (side == BattleSide::PLAYER) ? "Player" : "Opponent";
}

static BattleSide side_of_state(const SideState& side) {
    return (&side == &g_battle.player) ? BattleSide::PLAYER : BattleSide::OPPONENT;
}

void shuffle_deck(std::vector<Card>& deck) {
    std::shuffle(deck.begin(), deck.end(), battle_rng());
}

static bool side_has_play_resources(const SideState& side) {
    if (!side.hand.empty()) return true;
    if (!side.deck.empty()) return true;
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            if (side.field[r][c][0].hp > 0) return true;
        }
    }
    return false;
}

static bool is_stalemate(const BattleState& st) {
    return !side_has_play_resources(st.player) && !side_has_play_resources(st.opponent);
}

static std::vector<Card> pick_reward_cards(int difficulty, int count) {
    std::vector<const Card*> candidates;
    for (const Card* card : cards::all()) {
        if (card && card->cost < difficulty) {
            candidates.push_back(card);
        }
    }
    if (candidates.empty()) return {};

    std::sort(candidates.begin(), candidates.end(), [](const Card* a, const Card* b) {
        return a->cost > b->cost;
    });
    if (candidates.size() > 10) candidates.resize(10);

    std::shuffle(candidates.begin(), candidates.end(), battle_rng());
    count = std::min<int>(count, candidates.size());

    std::vector<Card> rewards;
    rewards.reserve(count);
    for (int i = 0; i < count; ++i) {
        rewards.push_back(*candidates[i]);
    }
    return rewards;
}

SideState& get_side_state(BattleState& state, BattleSide side) {
    return (side == BattleSide::PLAYER) ? state.player : state.opponent;
}

const SideState& get_side_state(const BattleState& state, BattleSide side) {
    return (side == BattleSide::PLAYER) ? state.player : state.opponent;
}

static Card make_empty_card() {
    return {"", 0, 0, 0, 0, 0, CardType::DEFAULT, CardKind::NORMAL, {}, std::nullopt, {}};
}

static ImVec4 lighten_color(ImVec4 color, float delta) {
    return ImVec4(
        std::min(color.x + delta, 1.0f),
        std::min(color.y + delta, 1.0f),
        std::min(color.z + delta, 1.0f),
        color.w
    );
}

static const char* card_kind_label(CardKind kind) {
    switch (kind) {
        case CardKind::NORMAL: return "Normal";
        case CardKind::SPECIAL: return "Special";
        case CardKind::IMMEDIATE: return "Immediate";
        case CardKind::FIELD_EFFECT: return "Field";
    }
    return "Normal";
}

static ImVec4 card_kind_color(CardKind kind) {
    switch (kind) {
        case CardKind::NORMAL: return ImVec4(0.30f, 0.35f, 0.45f, 1.0f);
        case CardKind::SPECIAL: return ImVec4(0.20f, 0.55f, 0.75f, 1.0f);
        case CardKind::IMMEDIATE: return ImVec4(0.80f, 0.55f, 0.15f, 1.0f);
        case CardKind::FIELD_EFFECT: return ImVec4(0.55f, 0.30f, 0.75f, 1.0f);
    }
    return ImVec4(0.30f, 0.35f, 0.45f, 1.0f);
}

static void push_card_kind_colors(CardKind kind) {
    ImVec4 base = card_kind_color(kind);
    ImVec4 hover = lighten_color(base, 0.1f);
    ImVec4 active = lighten_color(base, 0.2f);
    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
}

static void pop_card_kind_colors() {
    ImGui::PopStyleColor(3);
}

static void append_log(const std::string& entry) {
    g_battle.action_log.push_back(entry);
    const size_t max_entries = 200;
    if (g_battle.action_log.size() > max_entries) {
        g_battle.action_log.erase(g_battle.action_log.begin());
    }
}

BattleSide opposite_side(BattleSide side) {
    return (side == BattleSide::PLAYER) ? BattleSide::OPPONENT : BattleSide::PLAYER;
}

static void reset_side_damage(SideState& side) {
    side.ship_last_damage = 0;
    side.ship_last_heal = 0;
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            side.slot_last_damage[r][c] = 0;
            side.slot_last_heal[r][c] = 0;
        }
    }
}

static void reset_damage_tracking(BattleState& state) {
    reset_side_damage(state.player);
    reset_side_damage(state.opponent);
}

static void draw_damage_marker(int damage) {
    if (damage <= 0) return;
    std::string text = "-" + std::to_string(damage);
    ImVec2 rect_min = ImGui::GetItemRectMin();
    ImVec2 rect_max = ImGui::GetItemRectMax();
    ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
    ImVec2 pos(
        rect_min.x + ((rect_max.x - rect_min.x) - text_size.x) * 0.5f,
        rect_min.y - text_size.y - 2.0f
    );
    ImGui::GetWindowDrawList()->AddText(pos, IM_COL32(255, 64, 64, 255), text.c_str());
}

static void draw_heal_marker(int heal) {
    if (heal <= 0) return;
    std::string text = "+" + std::to_string(heal);
    ImVec2 rect_min = ImGui::GetItemRectMin();
    ImVec2 rect_max = ImGui::GetItemRectMax();
    ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
    ImVec2 pos(
        rect_min.x + ((rect_max.x - rect_min.x) - text_size.x) * 0.5f,
        rect_min.y - text_size.y - 2.0f
    );
    ImGui::GetWindowDrawList()->AddText(pos, IM_COL32(80, 220, 120, 255), text.c_str());
}

static bool column_has_card(const std::vector<Card> (&field)[2][6], int col) {
    for (int r = 0; r < 2; ++r) {
        if (field[r][col][0].hp > 0) return true;
    }
    return false;
}

static int find_target_column(const std::vector<Card> (&field)[2][6], int start_col) {
    // restrict targeting to the same half of the board; columns 0-2 and 3-5
    // center columns (2 and 3) only target straight ahead
    int min_col, max_col;
    if (start_col == 2 || start_col == 3) {
        min_col = max_col = start_col;
    } else {
        min_col = (start_col < 3) ? 0 : 3;
        max_col = (start_col < 3) ? 2 : 5;
    }

    if (column_has_card(field, start_col)) return start_col;

    if (start_col < 3) {
        for (int col = start_col + 1; col <= max_col; ++col) {
            if (column_has_card(field, col)) return col;
        }
    } else {
        for (int col = start_col - 1; col >= min_col; --col) {
            if (column_has_card(field, col)) return col;
        }
    }
    return -1;
}

static bool center_columns_clear(const std::vector<Card> (&field)[2][6], int start_col) {
    // Only require the near-center column to be empty for ship hits
    if (start_col < 3) return !column_has_card(field, 2);
    return !column_has_card(field, 3);
}

static void cleanup_destroyed_cards();

static int effective_damage(const Card& card, const SideState& side) {
    return static_cast<int>(std::lround(card.dmg * side.damage_multiplier));
}

void card_took_damage_trigger(SideState& side, int row, int col, int amount) {
    if (amount <= 0) return;
    if (row < 0 || row >= 2 || col < 0 || col >= 6) return;
    if (side.field[row][col].empty()) return;
    Card& card = side.field[row][col][0];
    if (card.name == "Masochist") {
        heal_ship(side, 500);
    }
}

static void deal_damage_to_slot(SideState& side, int row, int col, int dmg) {
    if (dmg <= 0) return;
    Card& target = side.field[row][col][0];
    if (target.hp <= 0) return;
    int applied = std::min(dmg, target.hp);
    target.hp -= applied;
    side.slot_last_damage[row][col] += applied;
    card_took_damage_trigger(side, row, col, applied);
}

void deal_damage_to_ship(SideState& side, int dmg) {
    if (dmg <= 0) return;
    int applied = std::min(dmg, side.hp);
    side.hp -= applied;
    side.ship_last_damage += applied;
    BattleSide bside = side_of_state(side);
    append_log(std::string("Ship of ") + side_label(bside) + " took " + std::to_string(applied) + " damage");
}

void heal_ship(SideState& side, int amount) {
    if (amount <= 0) return;
    side.hp += amount;
    side.ship_last_heal += amount;
    BattleSide bside = side_of_state(side);
    append_log(std::string("Ship of ") + side_label(bside) + " healed " + std::to_string(amount));
}

static void heal_slot(SideState& side, int row, int col, int amount) {
    Card& target = side.field[row][col][0];
    if (target.hp <= 0 || amount <= 0) return;
    int before = target.hp;
    target.hp = std::min(target.max_hp, target.hp + amount);
    int healed = target.hp - before;
    if (healed > 0) {
        side.slot_last_heal[row][col] += healed;
        BattleSide bside = side_of_state(side);
        append_log(std::string("Card ") + target.name + " from " + side_label(bside) + " at row " +
                   std::to_string(row) + " col " + std::to_string(col) + " healed " + std::to_string(healed));
    }
}

static void apply_card_effect(BattleState& state, BattleSide side, Card& card, int row, int col) {
    if (card.effect) {
        std::string loc = (row >= 0 && col >= 0) ? (" at row " + std::to_string(row) + " col " + std::to_string(col)) : "";
        std::string desc = card.effect_description ? (": " + *card.effect_description) : "";
        append_log(std::string("Card ") + card.name + " (" + card_kind_label(card.kind) + ") from " + side_label(side) + " activated" + loc + desc);
        card.state.times_used++;
        card.effect(state, side, row, col);
        if (card.hp <= 0 && card.name != "") {
            append_log(std::string("Card ") + card.name + " from " + side_label(side) + " was destroyed after activation");
        }
        cleanup_destroyed_cards();
    }
}

// Screen-space helpers for animation
static ImVec2 g_player_slot_centers[2][6];
static ImVec2 g_opponent_slot_centers[2][6];
static ImVec2 g_player_ship_pos(0, 0);
static ImVec2 g_opponent_ship_pos(0, 0);

static constexpr float k_anim_wait = 1.0f;
static constexpr float k_anim_bolt_time = 0.35f;
static constexpr float k_anim_post_wait = 1.0f;
static constexpr float k_anim_step_time = k_anim_bolt_time + k_anim_post_wait;
static constexpr int k_attack_phase_cols[3][2] = {{0, 5}, {1, 4}, {2, 3}};

static void cleanup_destroyed_cards_for_side(SideState& side) {
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            Card& card = side.field[r][c][0];
            if (card.hp <= 0 && card.name != "") {
                BattleSide side_enum = side_of_state(side);
                append_log(std::string("Card ") + card.name + " from " + side_label(side_enum)
                           + " on row " + std::to_string(r) + " col " + std::to_string(c) + " destroyed");
                card = make_empty_card();
            }
        }
    }
}

static void cleanup_destroyed_cards() {
    cleanup_destroyed_cards_for_side(g_battle.player);
    cleanup_destroyed_cards_for_side(g_battle.opponent);
}

static void apply_attacks_for_side(BattleSide attacker_side, int c) {
    SideState& attacker = get_side_state(g_battle, attacker_side);
    SideState& defender = get_side_state(g_battle, opposite_side(attacker_side));
    auto& attacker_field = attacker.field;
    auto& defender_field = defender.field;

    for (int r = 0; r < 2; r++) {
        if (g_battle.skip_attack_phase) return;

        Card& attacker_card = attacker_field[r][c][0];
        if (attacker_card.hp <= 0) continue;

        apply_card_effect(g_battle, attacker_side, attacker_card, r, c);
        if (attacker_card.hp <= 0) continue;
        if (attacker_card.state.skip_this_turn) {
            attacker_card.state.skip_this_turn = false;
            continue;
        }

        int remaining_dmg = effective_damage(attacker_card, attacker);
        if (remaining_dmg <= 0) continue;

        int target_col = find_target_column(defender_field, c);
        int total_unit_damage = 0;
        int total_ship_damage = 0;

        if (target_col >= 0) {
            int row_order[2] = {0, 1};
            if (attacker_side == BattleSide::PLAYER) {
                row_order[0] = 1; row_order[1] = 0; // prioritize opponent bottom row
            }
            for (int i = 0; i < 2 && remaining_dmg > 0; ++i) {
                int drow = row_order[i];
                Card& defender_card = defender_field[drow][target_col][0];
                if (defender_card.hp <= 0) continue;

                int dmg_to_deal = std::min(remaining_dmg, defender_card.hp);
                defender_card.hp -= dmg_to_deal;
                remaining_dmg -= dmg_to_deal;
                defender.slot_last_damage[drow][target_col] += dmg_to_deal;
                total_unit_damage += dmg_to_deal;
            }
        } else if (remaining_dmg > 0 && center_columns_clear(defender_field, c)) {
            defender.hp -= remaining_dmg;
            defender.ship_last_damage += remaining_dmg;
            total_ship_damage = remaining_dmg;
            remaining_dmg = 0;
        }

        std::string attack_log = std::string("Card ") + attacker_card.name + " (" + card_kind_label(attacker_card.kind) + ") from " +
            side_label(attacker_side) + " at row " + std::to_string(r) + " col " + std::to_string(c);
        if (target_col >= 0) {
            attack_log += " attacked column " + std::to_string(target_col) + " for " + std::to_string(total_unit_damage) + " damage";
        } else if (total_ship_damage > 0) {
            attack_log += " attacked the ship for " + std::to_string(total_ship_damage) + " damage";
        } else {
            attack_log += " had no available target";
        }
        std::string special = attacker_card.effect_description ? *attacker_card.effect_description : "None";
        attack_log += " (special effect: " + special + ")";
        append_log(attack_log);
    }
}

static void apply_attacks_for_column(int c) {
    apply_attacks_for_side(BattleSide::PLAYER, c);
    apply_attacks_for_side(BattleSide::OPPONENT, c);
    cleanup_destroyed_cards();
}

// --- Card effect helpers ----------------------------------------------------

void deal_damage_to_all_slots(SideState& side, int amount) {
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            deal_damage_to_slot(side, r, c, amount);
        }
    }
}

void heal_all_slots(SideState& side, int amount) {
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            heal_slot(side, r, c, amount);
        }
    }
}

static void draw_cards(SideState& side, int count) {
    for (int i = 0; i < count && !side.deck.empty(); ++i) {
        side.hand.push_back(side.deck.back());
        side.deck.pop_back();
    }
}

// --- Deck construction ------------------------------------------------------

static void add_repeated(std::vector<Card>& deck, const Card& card, int copies) {
    for (int i = 0; i < copies; ++i) deck.push_back(card);
}

static void fill_deck(std::vector<Card>& deck) {
    deck.clear();

    for (const auto& entry : cards::default_decklist()) {
        const Card* card = entry.first;
        if (card) {
            add_repeated(deck, *card, entry.second);
        }
    }

    shuffle_deck(deck);
}

static void apply_field_effect_cards(BattleState& st, BattleSide side) {
    SideState& ss = get_side_state(st, side);
    std::vector<Card> remaining;
    for (Card& c : ss.deck) {
        if (c.kind == CardKind::FIELD_EFFECT && c.effect) {
            Card copy = c;
            apply_card_effect(st, side, copy, -1, -1);
        } else {
            remaining.push_back(c);
        }
    }
    ss.deck = std::move(remaining);
    shuffle_deck(ss.deck);
}

static void apply_immediate_effect_queues(BattleState& st) {
    auto& p_queue = st.player.immediate_queue;
    auto& o_queue = st.opponent.immediate_queue;
    while (!p_queue.empty() || !o_queue.empty()) {
        std::vector<BattleSide> options;
        if (!p_queue.empty()) options.push_back(BattleSide::PLAYER);
        if (!o_queue.empty()) options.push_back(BattleSide::OPPONENT);

        BattleSide chosen = options.size() == 1
            ? options[0]
            : options[std::uniform_int_distribution<int>(0, static_cast<int>(options.size()) - 1)(battle_rng())];

        auto& queue = (chosen == BattleSide::PLAYER) ? p_queue : o_queue;
        Card card = queue.front();
        queue.erase(queue.begin());
        apply_card_effect(st, chosen, card, -1, -1);
        if (st.player.hp <= 0 || st.opponent.hp <= 0) break;
    }
}

static bool process_battle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) {
            emscripten_cancel_main_loop();
            return false;
        }
    }
    return true;
}

static void begin_battle_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(g_state.screen_width, g_state.screen_height));
    ImGui::Begin("Battle", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
}

static void render_hp_bars() {
    ImGui::Text("Opponent HP: %d (DMG x%.2f)", g_battle.opponent.hp, g_battle.opponent.damage_multiplier);
    if (g_battle.opponent.ship_last_damage > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "-%d", g_battle.opponent.ship_last_damage);
    } else if (g_battle.opponent.ship_last_heal > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.3f, 1.0f), "+%d", g_battle.opponent.ship_last_heal);
    }
    ImGui::Text("Player HP: %d (DMG x%.2f)", g_battle.player.hp, g_battle.player.damage_multiplier);
    if (g_battle.player.ship_last_damage > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "-%d", g_battle.player.ship_last_damage);
    } else if (g_battle.player.ship_last_heal > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.3f, 1.0f), "+%d", g_battle.player.ship_last_heal);
    }
    ImGui::Separator();
}

static void render_opponent_field() {
    const char* label = "Opponent Field";
    ImVec2 text_size = ImGui::CalcTextSize(label);
    float avail = ImGui::GetContentRegionAvail().x;
    float offset = (avail - text_size.x) * 0.5f - 40.0f;
    if (offset < 0) offset = 0;
    if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    ImGui::TextUnformatted(label);
    SideState& opponent = g_battle.opponent;
    if (ImGui::BeginTable("OpponentField", 6)) {
        for (int r = 0; r < 2; r++) {
            ImGui::TableNextRow();
            for (int c = 0; c < 6; c++) {
                ImGui::TableSetColumnIndex(c);
                Card& card = opponent.field[r][c][0];
                if (card.hp > 0) {
                    push_card_kind_colors(card.kind);
                    ImGui::Button((card.name + "\nHP:" + std::to_string(card.hp) + "\nDMG:" + std::to_string(card.dmg)).c_str(), ImVec2(96, 100));
                    pop_card_kind_colors();
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("%s", card.name.c_str());
                        ImGui::Text("Type: %s", card_kind_label(card.kind));
                        ImGui::Text("HP: %d / %d", card.hp, card.max_hp);
                        ImGui::Text("DMG: %d", card.dmg);
                        if (card.effect_description) {
                            ImGui::Separator();
                            ImGui::TextWrapped("%s", card.effect_description->c_str());
                        }
                        ImGui::EndTooltip();
                    }
                } else {
                    ImGui::Button("Empty", ImVec2(96, 100));
                }
                draw_damage_marker(opponent.slot_last_damage[r][c]);
                draw_heal_marker(opponent.slot_last_heal[r][c]);
                ImVec2 rect_min = ImGui::GetItemRectMin();
                ImVec2 rect_max = ImGui::GetItemRectMax();
                g_opponent_slot_centers[r][c] = ImVec2(
                    (rect_min.x + rect_max.x) * 0.5f,
                    (rect_min.y + rect_max.y) * 0.5f
                );
            }
        }
        ImGui::EndTable();
        g_opponent_ship_pos = ImVec2(
            (g_opponent_slot_centers[0][2].x + g_opponent_slot_centers[0][3].x) * 0.5f,
            (g_opponent_slot_centers[0][2].y + g_opponent_slot_centers[0][3].y) * 0.5f
        );
    }
    ImGui::Separator();
}

static void render_player_field() {
    const char* label = "Player Field";
    ImVec2 text_size = ImGui::CalcTextSize(label);
    float avail = ImGui::GetContentRegionAvail().x;
    float offset = (avail - text_size.x) * 0.5f - 40.0f;
    if (offset < 0) offset = 0;
    if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    ImGui::TextUnformatted(label);
    SideState& player = g_battle.player;
    if (ImGui::BeginTable("PlayerField", 6)) {
        for (int r = 0; r < 2; r++) {
            ImGui::TableNextRow();
            for (int c = 0; c < 6; c++) {
                ImGui::TableSetColumnIndex(c);
                Card& card = player.field[r][c][0];
                if (card.hp > 0) {
                    push_card_kind_colors(card.kind);
                    ImGui::Button((card.name + "\nHP:" + std::to_string(card.hp) + "\nDMG:" + std::to_string(card.dmg) + "##p" + std::to_string(r) + std::to_string(c)).c_str(), ImVec2(96, 100));
                    pop_card_kind_colors();
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("%s", card.name.c_str());
                        ImGui::Text("Type: %s", card_kind_label(card.kind));
                        ImGui::Text("HP: %d / %d", card.hp, card.max_hp);
                        ImGui::Text("DMG: %d", card.dmg);
                        if (card.effect_description) {
                            ImGui::Separator();
                            ImGui::TextWrapped("%s", card.effect_description->c_str());
                        }
                        ImGui::EndTooltip();
                    }
                } else {
                    if (g_battle.selected_card_hand_idx != -1 && !g_battle.battle_animating) {
                        Card& hand_card = player.hand[g_battle.selected_card_hand_idx];
                        bool placeable = hand_card.kind != CardKind::IMMEDIATE && hand_card.hp > 0;
                        if (!placeable) ImGui::BeginDisabled();
                        if (ImGui::Button(("Place Here##p" + std::to_string(r) + std::to_string(c)).c_str(), ImVec2(96, 100))) {
                            player.field[r][c][0] = player.hand[g_battle.selected_card_hand_idx];
                            player.hand.erase(player.hand.begin() + g_battle.selected_card_hand_idx);
                            g_battle.selected_card_hand_idx = -1;
                            append_log(std::string("Player placed card ") + player.field[r][c][0].name + " at row " + std::to_string(r) + " col " + std::to_string(c));
                        }
                        if (!placeable) ImGui::EndDisabled();
                    } else {
                        ImGui::Button(("Empty##p" + std::to_string(r) + std::to_string(c)).c_str(), ImVec2(96, 100));
                    }
                }
                draw_damage_marker(player.slot_last_damage[r][c]);
                draw_heal_marker(player.slot_last_heal[r][c]);
                ImVec2 rect_min = ImGui::GetItemRectMin();
                ImVec2 rect_max = ImGui::GetItemRectMax();
                g_player_slot_centers[r][c] = ImVec2(
                    (rect_min.x + rect_max.x) * 0.5f,
                    (rect_min.y + rect_max.y) * 0.5f
                );
            }
        }
        ImGui::EndTable();
        g_player_ship_pos = ImVec2(
            (g_player_slot_centers[0][2].x + g_player_slot_centers[0][3].x) * 0.5f,
            (g_player_slot_centers[0][2].y + g_player_slot_centers[0][3].y) * 0.5f
        );
    }
    ImGui::Separator();
}

static void render_player_hand() {
    ImGui::Text("Your Hand");
    auto& hand = g_battle.player.hand;
    for (size_t i = 0; i < hand.size(); i++) {
        Card& card = hand[i];
        bool selected = (g_battle.selected_card_hand_idx == (int)i);
        push_card_kind_colors(card.kind);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.2f, 1.0f));

        std::string label = card.name + "##h" + std::to_string(i);
        if (ImGui::Button(label.c_str(), ImVec2(120, 150))) {
            if (selected) g_battle.selected_card_hand_idx = -1;
            else g_battle.selected_card_hand_idx = (int)i;
        }

        pop_card_kind_colors();
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", card.name.c_str());
            ImGui::Text("Type: %s", card_kind_label(card.kind));
            ImGui::Text("HP: %d / %d", card.hp, card.max_hp);
            ImGui::Text("DMG: %d", card.dmg);
            if (card.effect_description) {
                ImGui::Separator();
                ImGui::TextWrapped("%s", card.effect_description->c_str());
            }
            ImGui::EndTooltip();
        }

        if (selected && card.kind == CardKind::IMMEDIATE && !g_battle.battle_animating) {
            ImGui::SameLine();
            if (ImGui::Button(("Activate##" + std::to_string(i)).c_str(), ImVec2(90, 30))) {
                g_battle.player.immediate_queue.push_back(card);
                g_battle.player.hand.erase(g_battle.player.hand.begin() + (int)i);
                g_battle.selected_card_hand_idx = -1;
                append_log(std::string("Player queued immediate card ") + card.name);
                // reset loop after erase
                i--;
            }
        }

        if (selected) ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::NewLine();
}

static void handle_end_turn_button(double now) {
    bool end_turn_disabled = g_battle.battle_animating;
    if (end_turn_disabled) ImGui::BeginDisabled();
    if (ImGui::Button("End Turn", ImVec2(120, 40))) {
        auto& player = g_battle.player;
        auto& opponent = g_battle.opponent;
        g_battle.skip_attack_phase = false;
        for(int i=0; i<2; i++) {
            if(!player.deck.empty()) {
                player.hand.push_back(player.deck.back());
                player.deck.pop_back();
            }
            if(!opponent.deck.empty()) {
                opponent.hand.push_back(opponent.deck.back());
                opponent.deck.pop_back();
            }
        }
        // Move opponent immediate cards to queue immediately after draw
        for (auto it = opponent.hand.begin(); it != opponent.hand.end();) {
            if (it->kind == CardKind::IMMEDIATE) {
                opponent.immediate_queue.push_back(*it);
                it = opponent.hand.erase(it);
            } else {
                ++it;
            }
        }

        for(auto it = opponent.hand.begin(); it != opponent.hand.end(); ) {
            bool placed = false;
            for(int r=0; r<2 && !placed; r++) {
                for(int c=0; c<6 && !placed; c++) {
                    if (opponent.field[r][c][0].hp <= 0 && it->kind != CardKind::IMMEDIATE) {
                        opponent.field[r][c][0] = *it;
                        it = opponent.hand.erase(it);
                        placed = true;
                        append_log(std::string("Opponent placed card ") + opponent.field[r][c][0].name + " at row " + std::to_string(r) + " col " + std::to_string(c));
                    }
                }
            }
            if(!placed) ++it;
        }

        reset_damage_tracking(g_battle);
        apply_immediate_effect_queues(g_battle);
        cleanup_destroyed_cards();
        if (g_battle.player.hp > 0 && g_battle.opponent.hp > 0) {
            g_battle.battle_animating = true;
            g_battle.anim_initial_wait = true;
            g_battle.anim_step_index = -1;
            g_battle.anim_step_start_time = now;
            g_battle.anim_damage_applied = false;
        }
    }
    if (end_turn_disabled) ImGui::EndDisabled();
}

static void update_battle_animation(double now) {
    if (!g_battle.battle_animating) return;

    if (g_battle.anim_initial_wait) {
        if (now - g_battle.anim_step_start_time >= k_anim_wait) {
            g_battle.anim_initial_wait = false;
            g_battle.anim_step_index = 0;
            g_battle.anim_step_start_time = now;
            g_battle.anim_damage_applied = false;
        }
        return;
    }

    if (g_battle.anim_step_index < 0 || g_battle.anim_step_index >= 3) return;

    double step_elapsed = now - g_battle.anim_step_start_time;
    if (!g_battle.anim_damage_applied && step_elapsed >= k_anim_bolt_time) {
        apply_attacks_for_column(k_attack_phase_cols[g_battle.anim_step_index][0]);
        apply_attacks_for_column(k_attack_phase_cols[g_battle.anim_step_index][1]);
        g_battle.anim_damage_applied = true;
    }
    if (step_elapsed >= k_anim_step_time) {
        g_battle.anim_step_index++;
        g_battle.anim_step_start_time = now;
        g_battle.anim_damage_applied = false;
        g_battle.anim_initial_wait = false;
        if (g_battle.anim_step_index >= 3) {
            g_battle.battle_animating = false;
            g_battle.anim_initial_wait = false;
            g_battle.anim_step_index = -1;
            g_battle.anim_damage_applied = false;
        }
    }
}

struct BoltRenderContext {
    BattleSide side;
    const std::vector<Card> (&field)[2][6];
    const std::vector<Card> (&target_field)[2][6];
    const ImVec2 (&slot_centers)[2][6];
    const ImVec2 (&target_slot_centers)[2][6];
    const ImVec2& ship_pos;
    const ImVec2& target_ship_pos;
    float offset_direction;
    ImU32 color;
};

static void render_bolts_for_side(int column, ImDrawList* draw_list, const BoltRenderContext& ctx) {
    for (int r = 0; r < 2; ++r) {
        const Card& card = ctx.field[r][column][0];
        if (card.hp <= 0 || card.dmg <= 0) continue;

        int target_col = find_target_column(ctx.target_field, column);
        bool can_hit_ship = (target_col < 0) && center_columns_clear(ctx.target_field, column);
        if (target_col < 0 && !can_hit_ship) continue;

        ImVec2 from = ctx.slot_centers[r][column];
        if (target_col >= 0) {
            from.x += 6.0f * ctx.offset_direction;
        }

        ImVec2 to = ctx.target_ship_pos;
        if (target_col >= 0) {
            int preferred_row = (ctx.side == BattleSide::PLAYER) ? 1 : 0;
            int fallback_row = preferred_row ^ 1;
            int target_row = (ctx.target_field[preferred_row][target_col][0].hp > 0) ? preferred_row : fallback_row;
            to = ctx.target_slot_centers[target_row][target_col];
        }

        draw_list->AddLine(from, to, ctx.color, 3.0f);
        break;
    }
}

static void render_battle_bolts(double now) {
    if (!g_battle.battle_animating || g_battle.anim_initial_wait) return;
    if (g_battle.anim_step_index < 0 || g_battle.anim_step_index >= 3) return;

    double step_elapsed = now - g_battle.anim_step_start_time;
    if (step_elapsed > k_anim_bolt_time) return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    BoltRenderContext player_ctx{
        BattleSide::PLAYER,
        g_battle.player.field,
        g_battle.opponent.field,
        g_player_slot_centers,
        g_opponent_slot_centers,
        g_player_ship_pos,
        g_opponent_ship_pos,
        -1.0f,
        IM_COL32(90, 220, 120, 255)
    };
    BoltRenderContext opponent_ctx{
        BattleSide::OPPONENT,
        g_battle.opponent.field,
        g_battle.player.field,
        g_opponent_slot_centers,
        g_player_slot_centers,
        g_opponent_ship_pos,
        g_player_ship_pos,
        1.0f,
        IM_COL32(220, 90, 90, 255)
    };

    for (int i = 0; i < 2; ++i) {
        int c = k_attack_phase_cols[g_battle.anim_step_index][i];
        render_bolts_for_side(c, draw_list, player_ctx);
        render_bolts_for_side(c, draw_list, opponent_ctx);
    }
}

static void render_battle_outcome_window() {
    if (g_battle.player.hp > 0 && g_battle.opponent.hp > 0) return;

    bool player_won = g_battle.player.hp > 0 && g_battle.opponent.hp <= 0;
    if (player_won && !g_battle.reward_added && g_battle.reward_options.empty()) {
        g_battle.reward_options = pick_reward_cards(g_battle.difficulty, 3);
        if (g_battle.reward_options.empty()) {
            g_battle.reward_added = true;
        }
    }

    ImGui::SetNextWindowPos(ImVec2(g_state.screen_width / 2.0f - 100.0f, g_state.screen_height / 2.0f - 50.0f), ImGuiCond_Always);
    ImGui::Begin("Battle Over", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    if (g_battle.player.hp <= 0) ImGui::Text("DEFEAT...");
    else {
        ImGui::Text("VICTORY!");
        if (!g_battle.reward_added) {
            ImGui::Separator();
            ImGui::Text("Choose your reward:");
            if (g_battle.reward_options.empty()) {
                ImGui::Text("No rewards available.");
            } else {
                for (size_t i = 0; i < g_battle.reward_options.size(); ++i) {
                    const Card& c = g_battle.reward_options[i];
                    ImGui::Separator();
                    ImGui::Text("%s", c.name.c_str());
                    ImGui::Text("Cost: %d | HP: %d | DMG: %d", c.cost, c.max_hp, c.base_dmg);
                    if (c.effect_description) {
                        ImGui::TextWrapped("Effect: %s", c.effect_description->c_str());
                    }
                    if (ImGui::Button(("Choose##reward" + std::to_string(i)).c_str(), ImVec2(200, 35))) {
                        g_battle.reward_card = c;
                        g_battle.player.deck.push_back(c);
                        if (g_battle.player_deck_ref) {
                            g_battle.player_deck_ref->push_back(c);
                        }
                        append_log(std::string("Reward gained: ") + c.name);
                        g_battle.reward_added = true;
                    }
                }
            }
        } else if (g_battle.reward_card) {
            const Card& c = *g_battle.reward_card;
            ImGui::Separator();
            ImGui::Text("Reward: %s", c.name.c_str());
            ImGui::Text("Cost: %d | HP: %d | DMG: %d", c.cost, c.max_hp, c.base_dmg);
            if (c.effect_description) {
                ImGui::TextWrapped("Effect: %s", c.effect_description->c_str());
            }
        }
    }

    if (ImGui::Button("Back to Overworld", ImVec2(200, 50))) {
        set_mode(GameMode::OVERWORLD);
    }
    ImGui::End();
}

static void render_debug_controls() {
    if (ImGui::Button("Leave Battle (Debug)")) {
        set_mode(GameMode::OVERWORLD);
    }
}

static void end_battle_frame() {
    ImGui::End();
    ImGui::Render();

    SDL_GetWindowSize(window, &g_state.screen_width, &g_state.screen_height);
    glViewport(0, 0, g_state.screen_width, g_state.screen_height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

static void render_action_log() {
    ImGui::Separator();
    ImGui::Text("Action Log");
    ImGui::BeginChild("ActionLog", ImVec2(0, 200), true);
    static size_t last_log_count = 0;
    float prev_scroll = ImGui::GetScrollY();
    float prev_max = ImGui::GetScrollMaxY();
    bool was_at_bottom = prev_scroll >= prev_max - 5.0f;

    for (const auto& line : g_battle.action_log) {
        ImGui::TextWrapped("%s", line.c_str());
    }
    ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight())); // bottom padding so last line fully visible
    if ((was_at_bottom || last_log_count == 0) && !g_battle.action_log.empty()) {
        ImGui::SetScrollHereY(1.0f);
    }
    last_log_count = g_battle.action_log.size();
    ImGui::EndChild();
}

static void clear_side_field(SideState& side) {
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            side.field[r][c].clear();
            side.field[r][c].push_back(make_empty_card());
        }
    }
}

static void initial_draw(std::vector<Card>& deck, std::vector<Card>& hand) {
    for (int i = 0; i < 5; ++i) {
        if (deck.empty()) break;
        hand.push_back(deck.back());
        deck.pop_back();
    }
}

void init_battle(BattleState& state, std::vector<Card>& player_deck, int difficulty) {
    state.player = SideState{};
    state.opponent = SideState{};
    state.action_log.clear();
    state.reward_card.reset();
    state.reward_options.clear();
    state.reward_added = false;
    state.player_deck_ref = &player_deck;
    state.difficulty = difficulty;
    append_log("Battle started");

    clear_side_field(state.player);
    clear_side_field(state.opponent);

    // Player uses their own deck (assumed non-empty and pre-assigned)
    state.player.deck = player_deck;
    shuffle_deck(state.player.deck);

    int opponent_cost_limit = std::max(1, difficulty);
    state.opponent.deck = cards::generate_deck_with_cost(opponent_cost_limit);
    shuffle_deck(state.opponent.deck);
    apply_field_effect_cards(state, BattleSide::PLAYER);
    apply_field_effect_cards(state, BattleSide::OPPONENT);

    state.player.hand.clear();
    state.opponent.hand.clear();
    initial_draw(state.player.deck, state.player.hand);
    initial_draw(state.opponent.deck, state.opponent.hand);
    // AI auto-activates immediate cards as soon as they are drawn
    for (size_t i = 0; i < state.opponent.hand.size();) {
        if (state.opponent.hand[i].kind == CardKind::IMMEDIATE) {
            state.opponent.immediate_queue.push_back(state.opponent.hand[i]);
            state.opponent.hand.erase(state.opponent.hand.begin() + (int)i);
        } else {
            ++i;
        }
    }

    state.selected_card_hand_idx = -1;
    state.is_player_turn = true;
    state.battle_animating = false;
    state.anim_step_index = -1;
    state.skip_attack_phase = false;
    reset_damage_tracking(state);
}

void start_random_battle(std::vector<Card>& player_deck, int difficulty) {
    init_battle(g_battle, player_deck, difficulty);
    set_mode(GameMode::BATTLE);
}

void battle_loop() {
    const double now = ImGui::GetTime();
    if (!process_battle_events()) return;

    if (is_stalemate(g_battle)) {
        append_log("Stalemate detected: ending battle.");
        set_mode(GameMode::OVERWORLD);
        return;
    }

    begin_battle_frame();

    render_hp_bars();
    render_opponent_field();
    render_player_field();
    render_player_hand();
    handle_end_turn_button(now);
    update_battle_animation(now);
    render_battle_bolts(now);
    render_action_log();
    render_battle_outcome_window();
    render_debug_controls();

    end_battle_frame();
}
