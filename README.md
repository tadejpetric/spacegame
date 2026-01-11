# emscripten download
```
sudo pacman -S emscripten
```
# Imgui download
```
git clone --depth 1 https://github.com/ocornut/imgui.git imgui_tmp

mkdir -p external/imgui && cp imgui_tmp/*.h external/imgui/ && cp imgui_tmp/*.cpp external/imgui/ && cp imgui_tmp/backends/imgui_impl_sdl2.h external/imgui/ && cp imgui_tmp/backends/imgui_impl_sdl2.cpp external/imgui/ && cp imgui_tmp/backends/imgui_impl_opengl3.h external/imgui/ && cp imgui_tmp/backends/imgui_impl_opengl3.cpp external/imgui/ && rm -rf imgui_tmp
```
# build
```
./build.sh
```
# run
```
python -m http.server
```

# Project idea
Idea:
Tile based space game
You are a spaceship in an infinite 2D grid
There is someone hunting you (a strong enemy)
You have to run away from him, because he is much stronger than you

During the gameplay you collect resources, among them cards.
Cards are used during combat.
Eventually your cards are strong enough to beat the enemy.

You also encounter
- minor enemies
- shops
- safe zones
- danger zones
- planets
- resource zones
- asteroids (damage you)
- ...

Some of them are visible on map, some are random encounters.


# Card combat spec

Each player has two rows of 6 spots.
On any spot, a player can place one card. Each turn, every card attacks the card in front of it (starting from the cards at the edges and working towards the center).
If there is no card in front of it, attack the nearest card towards the center.
If there are no cards on the center two columns, you attack the enemy spaceship directly.
First damage the card in the front row, then the back row. If you destroy a card, the damage spills
to the card behind it. If you destroy all the cards in a column, the damage is not spilled to the next column (it is wasted).

You and opponent both place cards at the same time. You only see the updated board after both players have placed their cards.
At the end of the turn, battle phase begins. The attacks are simultaneous for both opponents,
from the side of the board towards the center. First the side columns attack (both for you and opponent).
The damage is computed after the attacks and cards are destroyed accordingly. Then the next column attacks, and so on. The order of battle is such: 123321



There are different types of cards:
- guns (high attack, low defence)
- shields (low attack, high defence)
- drones (balanced)
- support (healing, buffs, etc.)
- Field effect cards (always activate at the start of the game, no need to draw them)
- active cards (Can activate at any point during your turn without placing it, consumes your card)
