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

## Card list (names + effects)

- Shield: Basic shield.
- Shield Mk2 / Mk3: Same as Shield with higher HP and damage at higher cost.
- Turret / Turret Mk2 / Turret Mk3: Higher damage and sturdier upgrades at higher cost.
- Drone / Drone Mk2 / Drone Mk3: Balanced unit; Mk2/3 raise HP and damage at higher cost.
- Mechanic: Heals all friendly units by 100 HP.
- Bombard: Deal 100 damage to all enemy units.
- Alternator: Fires every other turn (skips alternate turns).
- Overheat: Self-destructs after attacking 3 times.
- Overheat Mk2: After 3 attacks, destroys itself and deals 1000 damage to a random enemy card.
- Overheat Mk3: After 3 attacks, destroys itself and deals 1000 damage to all enemy cards.
- Greed: Draw 2 more cards.
- Greed Mk2: Draw 3 more cards.
- Greed Mk3: Draw 4 more cards.
- Bomb: Deal 100 to all enemy units and 50 to the enemy ship.
- Bomb Mk2: Deal 500 to all enemy units and 100 to all friendly units.
- Ceasefire: End the attack phase for this turn.
- Battle Drills (Field Effect): Double your cards' damage.
- Reinforced Hull (Field Effect): Increase ship HP by 1000.
- Nano Surge: Heal ship by 300 and all friendly units by 50.
- Dampening Field (Field Effect): Reduce incoming damage taken by 15% (enemy damage multiplier ×0.85).
- Fragile Lens: Each turn on the field, increase your damage multiplier by 10%.
- Blurred Lens: Each turn on the field, reduce the opponent's damage multiplier by 10%.
- Piercing Beam: Each attack also deals 100 damage to the enemy ship.
- Guardian Angel: Heals a random friendly unit by 120 each turn it's on the field.
- Reactor Overdrive (Field Effect): Boost your damage by 25% but deal 400 damage to your ship.
- Lone Wolf: Damage 6000 minus 2000 per other friendly card; HP 1000 minus 333 per friendly card (min 1).
- Masochist: When it takes damage, heal your ship by 500.
- Glass Cannon: Immediate — triples both sides' damage multipliers.
