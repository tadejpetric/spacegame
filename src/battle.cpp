#include "battle.h"
#include "overworld.h"
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
}

void start_random_battle() {
    init_battle(g_battle);
    set_mode((GameMode)1); // BATTLE
}

void battle_loop() {
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
    ImGui::Text("Player HP: %d", g_battle.player_hp);

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
            }
        }
        ImGui::EndTable();
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
                    if (g_battle.selected_card_hand_idx != -1) {
                        if (ImGui::Button(("Place Here##p" + std::to_string(r) + std::to_string(c)).c_str(), ImVec2(80, 100))) {
                            g_battle.player_field[r][c][0] = g_battle.player_hand[g_battle.selected_card_hand_idx];
                            g_battle.player_hand.erase(g_battle.player_hand.begin() + g_battle.selected_card_hand_idx);
                            g_battle.selected_card_hand_idx = -1;
                        }
                    } else {
                        ImGui::Button(("Empty##p" + std::to_string(r) + std::to_string(c)).c_str(), ImVec2(80, 100));
                    }
                }
            }
        }
        ImGui::EndTable();
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

        // --- Battle Phase ---
        // Order: 123321 (side columns towards center)
        int attack_order[] = {0, 5, 1, 4, 2, 3};
        
        for (int c : attack_order) {
            // Player attacks opponent
            for (int r = 0; r < 2; r++) {
                Card& p_card = g_battle.player_field[r][c][0];
                if (p_card.hp > 0) {
                    int remaining_dmg = p_card.dmg;
                    // Target logic: front row then back row in same column
                    for (int orow = 0; orow < 2 && remaining_dmg > 0; orow++) {
                        Card& o_card = g_battle.opponent_field[orow][c][0];
                        if (o_card.hp > 0) {
                            int dmg_to_deal = std::min(remaining_dmg, o_card.hp);
                            o_card.hp -= dmg_to_deal;
                            remaining_dmg -= dmg_to_deal;
                        }
                    }
                    // If damage remains, hit the opponent spaceship directly (only on center columns 2 & 3)
                    if (remaining_dmg > 0 && (c == 2 || c == 3)) {
                        g_battle.opponent_hp -= remaining_dmg;
                    }
                }
            }
            
            // Opponent attacks player
            for (int r = 0; r < 2; r++) {
                Card& o_card = g_battle.opponent_field[r][c][0];
                if (o_card.hp > 0) {
                    int remaining_dmg = o_card.dmg;
                    // Target logic: front row then back row in same column
                    for (int prow = 0; prow < 2 && remaining_dmg > 0; prow++) {
                        Card& p_card = g_battle.player_field[prow][c][0];
                        if (p_card.hp > 0) {
                            int dmg_to_deal = std::min(remaining_dmg, p_card.hp);
                            p_card.hp -= dmg_to_deal;
                            remaining_dmg -= dmg_to_deal;
                        }
                    }
                    // If damage remains, hit the player spaceship directly (only on center columns 2 & 3)
                    if (remaining_dmg > 0 && (c == 2 || c == 3)) {
                        g_battle.player_hp -= remaining_dmg;
                    }
                }
            }
        }

        // --- Cleanup Phase ---
        // Remove destroyed cards
        for (int r = 0; r < 2; r++) {
            for (int c = 0; c < 6; c++) {
                if (g_battle.player_field[r][c][0].hp <= 0 && g_battle.player_field[r][c][0].name != "") {
                    g_battle.player_field[r][c][0] = {"", 0, 0, 0, CardType::DRONE};
                }
                if (g_battle.opponent_field[r][c][0].hp <= 0 && g_battle.opponent_field[r][c][0].name != "") {
                    g_battle.opponent_field[r][c][0] = {"", 0, 0, 0, CardType::DRONE};
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
            set_mode((GameMode)0); // OVERWORLD
        }
        ImGui::End();
    }

    if (ImGui::Button("Leave Battle (Debug)")) {
        set_mode((GameMode)0); // OVERWORLD
    }

    ImGui::End();

    ImGui::Render();

    SDL_GetWindowSize(window, &g_state.screen_width, &g_state.screen_height);
    glViewport(0, 0, g_state.screen_width, g_state.screen_height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
