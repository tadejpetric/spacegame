#include "battle.h"
#include "overworld.h"
#include "states.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <emscripten.h>
#include <algorithm>
#include <random>
#include <ctime>

// Internal battle helper
void shuffle_deck(std::vector<Card>& deck) {
    static std::mt19937 rng(time(0));
    std::shuffle(deck.begin(), deck.end(), rng);
}

SideState& get_side_state(BattleState& state, BattleSide side) {
    return (side == BattleSide::PLAYER) ? state.player : state.opponent;
}

const SideState& get_side_state(const BattleState& state, BattleSide side) {
    return (side == BattleSide::PLAYER) ? state.player : state.opponent;
}

static BattleSide opposite_side(BattleSide side) {
    return (side == BattleSide::PLAYER) ? BattleSide::OPPONENT : BattleSide::PLAYER;
}

static void reset_side_damage(SideState& side) {
    side.ship_last_damage = 0;
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            side.slot_last_damage[r][c] = 0;
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
                card = {"", 0, 0, 0, CardType::DRONE};
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
        Card& attacker_card = attacker_field[r][c][0];
        if (attacker_card.hp <= 0) continue;

        int remaining_dmg = attacker_card.dmg;
        int target_col = find_target_column(defender_field, c);

        if (target_col >= 0) {
            for (int drow = 0; drow < 2 && remaining_dmg > 0; ++drow) {
                Card& defender_card = defender_field[drow][target_col][0];
                if (defender_card.hp <= 0) continue;

                int dmg_to_deal = std::min(remaining_dmg, defender_card.hp);
                defender_card.hp -= dmg_to_deal;
                remaining_dmg -= dmg_to_deal;
                defender.slot_last_damage[drow][target_col] += dmg_to_deal;
            }
        } else if (remaining_dmg > 0 && center_columns_clear(defender_field, c)) {
            defender.hp -= remaining_dmg;
            defender.ship_last_damage += remaining_dmg;
            remaining_dmg = 0;
        }
    }
}

static void apply_attacks_for_column(int c) {
    apply_attacks_for_side(BattleSide::PLAYER, c);
    apply_attacks_for_side(BattleSide::OPPONENT, c);
    cleanup_destroyed_cards();
}

Card create_card(const std::string& name, CardType type) {
    if (type == CardType::SHIELD) return {name, 500, 500, 50, type};
    if (type == CardType::TURRET) return {name, 200, 200, 200, type};
    return {name, 300, 300, 100, type}; // Drone
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
    ImGui::Text("Opponent HP: %d", g_battle.opponent.hp);
    if (g_battle.opponent.ship_last_damage > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "-%d", g_battle.opponent.ship_last_damage);
    }
    ImGui::Text("Player HP: %d", g_battle.player.hp);
    if (g_battle.player.ship_last_damage > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "-%d", g_battle.player.ship_last_damage);
    }
    ImGui::Separator();
}

static void render_opponent_field() {
    ImGui::Text("Opponent Field");
    SideState& opponent = g_battle.opponent;
    if (ImGui::BeginTable("OpponentField", 6)) {
        for (int r = 0; r < 2; r++) {
            ImGui::TableNextRow();
            for (int c = 0; c < 6; c++) {
                ImGui::TableSetColumnIndex(c);
                Card& card = opponent.field[r][c][0];
                if (card.hp > 0) {
                    ImGui::Button((card.name + "\nHP:" + std::to_string(card.hp) + "\nDMG:" + std::to_string(card.dmg)).c_str(), ImVec2(80, 100));
                } else {
                    ImGui::Button("Empty", ImVec2(80, 100));
                }
                draw_damage_marker(opponent.slot_last_damage[r][c]);
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
    ImGui::Text("Player Field");
    SideState& player = g_battle.player;
    if (ImGui::BeginTable("PlayerField", 6)) {
        for (int r = 0; r < 2; r++) {
            ImGui::TableNextRow();
            for (int c = 0; c < 6; c++) {
                ImGui::TableSetColumnIndex(c);
                Card& card = player.field[r][c][0];
                if (card.hp > 0) {
                    ImGui::Button((card.name + "\nHP:" + std::to_string(card.hp) + "\nDMG:" + std::to_string(card.dmg) + "##p" + std::to_string(r) + std::to_string(c)).c_str(), ImVec2(80, 100));
                } else {
                    if (g_battle.selected_card_hand_idx != -1 && !g_battle.battle_animating) {
                        if (ImGui::Button(("Place Here##p" + std::to_string(r) + std::to_string(c)).c_str(), ImVec2(80, 100))) {
                            player.field[r][c][0] = player.hand[g_battle.selected_card_hand_idx];
                            player.hand.erase(player.hand.begin() + g_battle.selected_card_hand_idx);
                            g_battle.selected_card_hand_idx = -1;
                        }
                    } else {
                        ImGui::Button(("Empty##p" + std::to_string(r) + std::to_string(c)).c_str(), ImVec2(80, 100));
                    }
                }
                draw_damage_marker(player.slot_last_damage[r][c]);
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
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.2f, 1.0f));

        if (ImGui::Button((card.name + "##h" + std::to_string(i)).c_str(), ImVec2(100, 140))) {
            if (selected) g_battle.selected_card_hand_idx = -1;
            else g_battle.selected_card_hand_idx = (int)i;
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
        for(auto it = opponent.hand.begin(); it != opponent.hand.end(); ) {
            bool placed = false;
            for(int r=0; r<2 && !placed; r++) {
                for(int c=0; c<6 && !placed; c++) {
                    if (opponent.field[r][c][0].hp <= 0) {
                        opponent.field[r][c][0] = *it;
                        it = opponent.hand.erase(it);
                        placed = true;
                    }
                }
            }
            if(!placed) ++it;
        }

        reset_damage_tracking(g_battle);
        g_battle.battle_animating = true;
        g_battle.anim_initial_wait = true;
        g_battle.anim_step_index = -1;
        g_battle.anim_step_start_time = now;
        g_battle.anim_damage_applied = false;
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

    ImGui::SetNextWindowPos(ImVec2(g_state.screen_width / 2.0f - 100.0f, g_state.screen_height / 2.0f - 50.0f), ImGuiCond_Always);
    ImGui::Begin("Battle Over", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    if (g_battle.player.hp <= 0) ImGui::Text("DEFEAT...");
    else ImGui::Text("VICTORY!");

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

static void clear_side_field(SideState& side) {
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            side.field[r][c].clear();
            side.field[r][c].push_back({"", 0, 0, 0, CardType::DRONE});
        }
    }
}

static void fill_deck(std::vector<Card>& deck) {
    deck.clear();
    for (int i = 0; i < 10; ++i) {
        deck.push_back(create_card("Shield", CardType::SHIELD));
        deck.push_back(create_card("Turret", CardType::TURRET));
        deck.push_back(create_card("Drone", CardType::DRONE));
    }
    shuffle_deck(deck);
}

static void initial_draw(std::vector<Card>& deck, std::vector<Card>& hand) {
    for (int i = 0; i < 5; ++i) {
        if (deck.empty()) break;
        hand.push_back(deck.back());
        deck.pop_back();
    }
}

void init_battle(BattleState& state) {
    state.player = SideState{};
    state.opponent = SideState{};

    clear_side_field(state.player);
    clear_side_field(state.opponent);

    fill_deck(state.player.deck);
    fill_deck(state.opponent.deck);

    state.player.hand.clear();
    state.opponent.hand.clear();
    initial_draw(state.player.deck, state.player.hand);
    initial_draw(state.opponent.deck, state.opponent.hand);

    state.selected_card_hand_idx = -1;
    state.is_player_turn = true;
    state.battle_animating = false;
    state.anim_step_index = -1;
    reset_damage_tracking(state);
}

void start_random_battle() {
    init_battle(g_battle);
    set_mode(GameMode::BATTLE);
}

void battle_loop() {
    const double now = ImGui::GetTime();
    if (!process_battle_events()) return;

    begin_battle_frame();

    render_hp_bars();
    render_opponent_field();
    render_player_field();
    render_player_hand();
    handle_end_turn_button(now);
    update_battle_animation(now);
    render_battle_bolts(now);
    render_battle_outcome_window();
    render_debug_controls();

    end_battle_frame();
}
