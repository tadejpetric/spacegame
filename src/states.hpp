#ifndef STATES_HPP
#define STATES_HPP

enum class GameMode {
    OVERWORLD,
    BATTLE
};

GameMode get_mode();
void set_mode(GameMode mode);

#endif // STATES_HPP
