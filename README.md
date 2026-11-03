# SANDVEIL

SANDVEIL is an on-foot, PS1-inspired psychological survival-horror game set in a fictionalized Liwa, UAE, in 1998. Salem is searching for his missing older brother Nasser through an isolated roadside stop and an abandoned settlement. Cars are scenery and story props only; there are no driving controls or vehicle fuel mechanics.

The Mac prototype uses C and raylib. It renders to a 426 × 240 target with nearest-neighbour scaling for a late-1990s console look.

## Requirements

- macOS 12 or later
- Xcode Command Line Tools
- CMake
- raylib 6.x

Install dependencies with Homebrew:

```sh
brew install cmake raylib
```

## Build and launch

From the project directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
open build/sandveil.app
```

Run the gameplay and renderer-core tests:

```sh
ctest --test-dir build --output-on-failure
```

In VS Code, run **Build SANDVEIL** from Tasks, then **Launch SANDVEIL** from Run and Debug.

## Current playable slice

- Three short opening cards; Salem starts on foot beside a parked Land Cruiser.
- A connected Liwa road, deserted petrol station, village approach, and blocked village gate.
- Collision-aware walking and brief sprinting with recovering stamina.
- Third-person outdoor camera, a fixed-angle station camera, and smoothed camera transitions.
- A dark station interior with a flashlight, battery drain, cassette clue, and keypad puzzle.
- Entering `0712` at the service keypad opens a passage to the village alley and its previously blocked gate.
- A paper-map overlay that marks the village after discovery; an inventory panel; a distant figure event.
- Desert haze, the late-evening clock, and a one-time sandstorm that shifts terrain.

## Controls

- **W / A / S / D** — walk
- **Left Shift / Right Shift** — sprint while stamina remains
- **E** — enter/leave the station, examine objects, play the cassette, or confirm the keypad
- **0–9** — enter the keypad code; **Backspace** erases a digit
- **F** — toggle flashlight
- **M** — open/close Salem's paper map
- **I** — open/close inventory
- **Enter / Space** — advance opening cards
- **Esc** — quit

The cassette is on the station counter; examine it to learn the service code. The gate and passage persist as opened after solving the keypad. The map is a sketch, not a GPS minimap.

## Project layout

- `src/game.c` and `src/game.h` — movement, collisions, room transitions, inventory state, puzzle state, and story triggers. This gameplay module has no raylib dependencies.
- `src/world.c` and `src/world.h` — terrain, Liwa roadside/village scene, station interior, Salem, and figure models.
- `src/main.c` — raylib window, keyboard input, camera, HUD, map, and inventory UI.
- `src/psx/renderer.c` and `src/psx/renderer.h` — separate integer-only renderer reference core for future PS1 work.
- `tests/` — gameplay and PS1 renderer-core tests.

The main map is deliberately authored rather than procedurally generated. The current slice uses one outdoor area and one enterable room; the residential compound and buried settlement are not implemented yet.

## PS1 renderer-core prototype

The host-tested `src/psx/renderer.c` demonstrates Q12 fixed-point projection, pixel snapping, near/far and screen culling, per-vertex shading, fog, and far-to-near ordering-table buckets. Test it on Mac with:

```sh
cmake --build build --target psx_renderer_tests
ctest --test-dir build -R psx_renderer_tests --output-on-failure
```

This is not a PS1 executable and does not submit GTE/GPU packets, initialize the console display, decode indexed textures, or stream CD sectors. PSn00bSDK must be installed before a real console build can be attempted. The PS1 port will need a separate renderer and platform-specific controller/audio back end; raylib itself is not intended to run on original hardware.
