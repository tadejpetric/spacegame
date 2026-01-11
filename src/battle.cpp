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

static void reset_damage_tracking(BattleState& state) {
    state.player_ship_last_damage = 0;
    state.opponent_ship_last_damage = 0;
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            state.player_slot_last_damage[r][c] = 0;
            state.opponent_slot_last_damage[r][c] = 0;
        }
    }
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

static void cleanup_destroyed_cards() {
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 6; ++c) {
            if (g_battle.player_field[r][c][0].hp <= 0 && g_battle.player_field[r][c][0].name != "") {
                g_battle.player_field[r][c][0] = {"", 0, 0, 0, CardType::DRONE};
            }
            if (g_battle.opponent_field[r][c][0].hp <= 0 && g_battle.opponent_field[r][c][0].name != "") {
                g_battle.opponent_field[r][c][0] = {"", 0, 0, 0, CardType::DRONE};
            }
        }
    }
}

static void apply_attacks_for_column(int c) {
    // Player attacks opponent
    for (int r = 0; r < 2; r++) {
        Card& p_card = g_battle.player_field[r][c][0];
        if (p_card.hp > 0) {
            int remaining_dmg = p_card.dmg;
            int target_col = find_target_column(g_battle.opponent_field, c);
            if (target_col >= 0) {
                for (int orow = 0; orow < 2 && remaining_dmg > 0; ++orow) {
                    Card& o_card = g_battle.opponent_field[orow][target_col][0];
                    if (o_card.hp > 0) {
                        int dmg_to_deal = std::min(remaining_dmg, o_card.hp);
                        o_card.hp -= dmg_to_deal;
                        remaining_dmg -= dmg_to_deal;
                        g_battle.opponent_slot_last_damage[orow][target_col] += dmg_to_deal;
                    }
                }
            } else if (remaining_dmg > 0 && center_columns_clear(g_battle.opponent_field, c)) {
                g_battle.opponent_hp -= remaining_dmg;
                g_battle.opponent_ship_last_damage += remaining_dmg;
                remaining_dmg = 0;
            }
        }
    }

    // Opponent attacks player
    for (int r = 0; r < 2; r++) {
        Card& o_card = g_battle.opponent_field[r][c][0];
        if (o_card.hp > 0) {
            int remaining_dmg = o_card.dmg;
            int target_col = find_target_column(g_battle.player_field, c);
            if (target_col >= 0) {
                for (int prow = 0; prow < 2 && remaining_dmg > 0; ++prow) {
                    Card& p_card = g_battle.player_field[prow][target_col][0];
                    if (p_card.hp > 0) {
                        int dmg_to_deal = std::min(remaining_dmg, p_card.hp);
                        p_card.hp -= dmg_to_deal;
                        remaining_dmg -= dmg_to_deal;
                        g_battle.player_slot_last_damage[prow][target_col] += dmg_to_deal;
                    }
                }
            } else if (remaining_dmg > 0 && center_columns_clear(g_battle.player_field, c)) {
                g_battle.player_hp -= remaining_dmg;
                g_battle.player_ship_last_damage += remaining_dmg;
                remaining_dmg = 0;
            }
        }
    }

    cleanup_destroyed_cards();
}

Card create_card(const std::string& name, CardType type) {
    if (type == CardType::SHIELD) return {name, 500, 500, 50, type};
    if (type == CardType::TURRET) return {name, 200, 200, 200, type};
    return {name, 300, 300, 100, type}; // Drone
}

void init_battle(BattleState& state) {
    state.player_hp = 10000;
    state.opponent_hp = 10000;
    state.player_deck.clear();
    state.opponent_deck.clear();
    state.player_hand.clear();
    state.opponent_hand.clear();
    for(int r=0; r<2; r++) {
        for(int c=0; c<6; c++) {
            state.player_field[r][c].clear();
            state.player_field[r][c].push_back({"", 0, 0, 0, CardType::DRONE});
            state.opponent_field[r][c].clear();
            state.opponent_field[r][c].push_back({"", 0, 0, 0, CardType::DRONE});
        }
    }
    state.selected_card_hand_idx = -1;
    reset_damage_tracking(state);

    // Fill decks
    for(int i=0; i<10; i++) {
        state.player_deck.push_back(create_card("Shield", CardType::SHIELD));
        state.player_deck.push_back(create_card("Turret", CardType::TURRET));
        state.player_deck.push_back(create_card("Drone", CardType::DRONE));
        state.opponent_deck.push_back(create_card("Shield", CardType::SHIELD));
        state.opponent_deck.push_back(create_card("Turret", CardType::TURRET));
        state.opponent_deck.push_back(create_card("Drone", CardType::DRONE));
    }
    shuffle_deck(state.player_deck);
    shuffle_deck(state.opponent_deck);

    // Initial draw
    for(int i=0; i<5; i++) {
        if(!state.player_deck.empty()) {
            state.player_hand.push_back(state.player_deck.back());
            state.player_deck.pop_back();
        }
        if(!state.opponent_deck.empty()) {
            state.opponent_hand.push_back(state.opponent_deck.back());
            state.opponent_deck.pop_back();
        }
    }
    state.is_player_turn = true;
    state.battle_animating = false;
    state.anim_step_index = -1;
}

void start_random_battle() {
    init_battle(g_battle);
    set_mode(GameMode::BATTLE);
}

void battle_loop() {
    const double now = ImGui::GetTime();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) {
            emscripten_cancel_main_loop();
            return;
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(g_state.screen_width, g_state.screen_height));
    ImGui::Begin("Battle", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // HP Bars
    ImGui::Text("Opponent HP: %d", g_battle.opponent_hp);
    if (g_battle.opponent_ship_last_damage > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "-%d", g_battle.opponent_ship_last_damage);
    }
    ImGui::Text("Player HP: %d", g_battle.player_hp);
    if (g_battle.player_ship_last_damage > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "-%d", g_battle.player_ship_last_damage);
    }

    ImGui::Separator();

    // Opponent Field
    ImGui::Text("Opponent Field");
    if (ImGui::BeginTable("OpponentField", 6)) {
        for (int r = 0; r < 2; r++) {
            ImGui::TableNextRow();
            for (int c = 0; c < 6; c++) {
                ImGui::TableSetColumnIndex(c);
                Card& card = g_battle.opponent_field[r][c][0];
                if (card.hp > 0) {
                    ImGui::Button((card.name + "\nHP:" + std::to_string(card.hp) + "\nDMG:" + std::to_string(card.dmg)).c_str(), ImVec2(80, 100));
                } else {
                    ImGui::Button("Empty", ImVec2(80, 100));
                }
                draw_damage_marker(g_battle.opponent_slot_last_damage[r][c]);
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

    // Player Field
    ImGui::Text("Player Field");
    if (ImGui::BeginTable("PlayerField", 6)) {
        for (int r = 0; r < 2; r++) {
            ImGui::TableNextRow();
            for (int c = 0; c < 6; c++) {
                ImGui::TableSetColumnIndex(c);
                Card& card = g_battle.player_field[r][c][0];
                if (card.hp > 0) {
                    ImGui::Button((card.name + "\nHP:" + std::to_string(card.hp) + "\nDMG:" + std::to_string(card.dmg) + "##p" + std::to_string(r) + std::to_string(c)).c_str(), ImVec2(80, 100));
                } else {
                    if (g_battle.selected_card_hand_idx != -1 && !g_battle.battle_animating) {
                        if (ImGui::Button(("Place Here##p" + std::to_string(r) + std::to_string(c)).c_str(), ImVec2(80, 100))) {
                            g_battle.player_field[r][c][0] = g_battle.player_hand[g_battle.selected_card_hand_idx];
                            g_battle.player_hand.erase(g_battle.player_hand.begin() + g_battle.selected_card_hand_idx);
                            g_battle.selected_card_hand_idx = -1;
                        }
                    } else {
                        ImGui::Button(("Empty##p" + std::to_string(r) + std::to_string(c)).c_str(), ImVec2(80, 100));
                    }
                }
                draw_damage_marker(g_battle.player_slot_last_damage[r][c]);
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

    // Player Hand
    ImGui::Text("Your Hand");
    for (size_t i = 0; i < g_battle.player_hand.size(); i++) {
        Card& card = g_battle.player_hand[i];
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

    bool end_turn_disabled = g_battle.battle_animating;
    if (end_turn_disabled) ImGui::BeginDisabled();
    if (ImGui::Button("End Turn", ImVec2(120, 40))) {
        // Simple end turn logic: draw 2 cards
        for(int i=0; i<2; i++) {
            if(!g_battle.player_deck.empty()) {
                g_battle.player_hand.push_back(g_battle.player_deck.back());
                g_battle.player_deck.pop_back();
            }
            if(!g_battle.opponent_deck.empty()) {
                g_battle.opponent_hand.push_back(g_battle.opponent_deck.back());
                g_battle.opponent_deck.pop_back();
            }
        }
        // Simple AI: opponent places all cards in hand to random spots
        for(auto it = g_battle.opponent_hand.begin(); it != g_battle.opponent_hand.end(); ) {
            bool placed = false;
            for(int r=0; r<2 && !placed; r++) {
                for(int c=0; c<6 && !placed; c++) {
                    if (g_battle.opponent_field[r][c][0].hp <= 0) {
                        g_battle.opponent_field[r][c][0] = *it;
                        it = g_battle.opponent_hand.erase(it);
                        placed = true;
                    }
                }
            }
            if(!placed) ++it;
        }

        // --- Battle Phase Animation Setup ---
        reset_damage_tracking(g_battle);
        g_battle.battle_animating = true;
        g_battle.anim_initial_wait = true;
        g_battle.anim_step_index = -1;
        g_battle.anim_step_start_time = now;
        g_battle.anim_damage_applied = false;
    }
    if (end_turn_disabled) ImGui::EndDisabled();

    // Animation state machine
    if (g_battle.battle_animating) {
        if (g_battle.anim_initial_wait) {
            if (now - g_battle.anim_step_start_time >= k_anim_wait) {
                g_battle.anim_initial_wait = false;
                g_battle.anim_step_index = 0;
                g_battle.anim_step_start_time = now;
                g_battle.anim_damage_applied = false;
            }
        } else if (g_battle.anim_step_index >= 0 && g_battle.anim_step_index < 3) {
            double step_elapsed = now - g_battle.anim_step_start_time;
            if (!g_battle.anim_damage_applied && step_elapsed >= k_anim_bolt_time) {
                // Apply both columns for this phase simultaneously
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
    }

    // Bolt rendering during animation
    if (g_battle.battle_animating && !g_battle.anim_initial_wait && g_battle.anim_step_index >= 0 && g_battle.anim_step_index < 3) {
        double step_elapsed = now - g_battle.anim_step_start_time;
        if (step_elapsed <= k_anim_bolt_time) {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();

            auto draw_bolt = [&](const ImVec2& from, const ImVec2& to, ImU32 color) {
                draw_list->AddLine(from, to, color, 3.0f);
            };

            for (int i = 0; i < 2; ++i) {
                int c = k_attack_phase_cols[g_battle.anim_step_index][i];

                // Player bolt from column c
                for (int r = 0; r < 2; ++r) {
                    Card& p_card = g_battle.player_field[r][c][0];
                    if (p_card.hp > 0 && p_card.dmg > 0) {
                        int target_col = find_target_column(g_battle.opponent_field, c);
                        bool can_hit_ship = (target_col < 0) && center_columns_clear(g_battle.opponent_field, c);
                        if (target_col >= 0 || can_hit_ship) {
                            ImVec2 from = g_player_slot_centers[r][c];
                            // offset slightly to avoid overlap unless center shot
                            if (target_col >= 0 && !(target_col == 2 || target_col == 3)) {
                                from.x -= 6.0f;
                            }
                            ImVec2 to = g_opponent_ship_pos;
                            if (target_col >= 0) {
                                int target_row = g_battle.opponent_field[1][target_col][0].hp > 0 ? 1 : 0;
                                to = g_opponent_slot_centers[target_row][target_col];
                            }
                            draw_bolt(from, to, IM_COL32(90, 220, 120, 255)); // player: green
                            break; // only one bolt per card
                        }
                    }
                }

                // Opponent bolt from column c
                for (int r = 0; r < 2; ++r) {
                    Card& o_card = g_battle.opponent_field[r][c][0];
                    if (o_card.hp > 0 && o_card.dmg > 0) {
                        int target_col = find_target_column(g_battle.player_field, c);
                        bool can_hit_ship = (target_col < 0) && center_columns_clear(g_battle.player_field, c);
                        if (target_col >= 0 || can_hit_ship) {
                            ImVec2 from = g_opponent_slot_centers[r][c];
                            // offset slightly to avoid overlap unless center shot
                            if (target_col >= 0 && !(target_col == 2 || target_col == 3)) {
                                from.x += 6.0f;
                            }
                            ImVec2 to = g_player_ship_pos;
                            if (target_col >= 0) {
                                int target_row = g_battle.player_field[0][target_col][0].hp > 0 ? 0 : 1;
                                to = g_player_slot_centers[target_row][target_col];
                            }
                            draw_bolt(from, to, IM_COL32(220, 90, 90, 255)); // opponent: red
                            break;
                        }
                    }
                }
            }
        }
    }

    if (g_battle.player_hp <= 0 || g_battle.opponent_hp <= 0) {
        ImGui::SetNextWindowPos(ImVec2(g_state.screen_width / 2.0f - 100.0f, g_state.screen_height / 2.0f - 50.0f), ImGuiCond_Always);
        ImGui::Begin("Battle Over", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
        if (g_battle.player_hp <= 0) ImGui::Text("DEFEAT...");
        else ImGui::Text("VICTORY!");
        
        if (ImGui::Button("Back to Overworld", ImVec2(200, 50))) {
            set_mode(GameMode::OVERWORLD);
        }
        ImGui::End();
    }

    if (ImGui::Button("Leave Battle (Debug)")) {
        set_mode(GameMode::OVERWORLD);
    }

    ImGui::End();

    ImGui::Render();

    SDL_GetWindowSize(window, &g_state.screen_width, &g_state.screen_height);
    glViewport(0, 0, g_state.screen_width, g_state.screen_height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
