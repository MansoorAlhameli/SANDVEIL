#include "game.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void start_game(GameState *game)
{
    for (int page = 0; page < 3; page++) {
        game_update(game, (GameInput){.continue_intro = 1, .digit = -1}, 1.0f / 60.0f);
    }
}

static void test_initial_state_is_on_foot(void)
{
    GameState game;
    game_init(&game);
    assert(game.intro_active);
    assert(game.room == ROOM_LIWA_ROAD);
    assert(game.x == 0.0f);
    assert(game.z == -24.0f);
    assert(game.battery == 100.0f);
    assert(game.stamina == 100.0f);
    assert(game.health == 100);
}

static void test_intro_pauses_movement(void)
{
    GameState game;
    game_init(&game);
    game_update(&game, (GameInput){.move_z = 1.0f, .digit = -1}, 1.0f);
    assert(game.intro_active);
    assert(game.elapsed_seconds == 0.0f);
    assert(game.z == -24.0f);
    start_game(&game);
    game_update(&game, (GameInput){.move_z = 1.0f, .digit = -1}, 1.0f / 60.0f);
    assert(game.z > -24.0f);
}

static void test_walking_sprinting_and_stamina(void)
{
    GameState game;
    game_init(&game);
    start_game(&game);
    float initial_z = game.z;
    for (int i = 0; i < 60; i++) {
        game_update(&game, (GameInput){.move_z = 1.0f, .digit = -1}, 1.0f / 60.0f);
    }
    assert(game.z > initial_z + 2.0f);
    float initial_stamina = game.stamina;
    for (int i = 0; i < 60; i++) {
        game_update(&game, (GameInput){.move_z = 1.0f, .sprint = 1, .digit = -1}, 1.0f / 60.0f);
    }
    assert(game.stamina < initial_stamina);
    assert(game.z > initial_z + 5.0f);
}

static void walk_to_station_door(GameState *game)
{
    while (game->z < 15.9f) {
        game_update(game, (GameInput){.move_z = 1.0f, .digit = -1}, 1.0f / 60.0f);
    }
    while (game->x < 12.0f) {
        game_update(game, (GameInput){.move_x = 1.0f, .digit = -1}, 1.0f / 60.0f);
    }
    while (game->z < 18.0f) {
        game_update(game, (GameInput){.move_z = 1.0f, .digit = -1}, 1.0f / 60.0f);
    }
}

static void enter_station(GameState *game)
{
    walk_to_station_door(game);
    game_update(game, (GameInput){.interact = 1, .digit = -1}, 1.0f / 60.0f);
    assert(game->room == ROOM_PETROL_STATION);
    assert(game->station_discovered);
}

static void walk_inside_to(GameState *game, float x, float z)
{
    while (fabsf(game->x - x) > 0.25f) {
        game_update(game, (GameInput){.move_x = game->x < x ? 1.0f : -1.0f, .digit = -1},
            1.0f / 60.0f);
    }
    while (fabsf(game->z - z) > 0.25f) {
        game_update(game, (GameInput){.move_z = game->z < z ? 1.0f : -1.0f, .digit = -1},
            1.0f / 60.0f);
    }
}

static void test_station_recording_keypad_and_shortcut(void)
{
    GameState game;
    game_init(&game);
    start_game(&game);
    enter_station(&game);

    walk_inside_to(&game, -3.3f, 0.4f);
    game_update(&game, (GameInput){.interact = 1, .digit = -1}, 1.0f / 60.0f);
    assert(game.recorder_heard);

    walk_inside_to(&game, 3.9f, 4.4f);
    game_update(&game, (GameInput){.interact = 1, .digit = -1}, 1.0f / 60.0f);
    assert(game.keypad_active);
    const int digits[] = {0, 7, 1, 2};
    for (int i = 0; i < 4; i++) {
        game_update(&game, (GameInput){.digit = digits[i]}, 1.0f / 60.0f);
    }
    game_update(&game, (GameInput){.interact = 1, .digit = -1}, 1.0f / 60.0f);
    assert(!game.keypad_active);
    assert(game.storeroom_open);

    walk_inside_to(&game, 0.0f, 5.6f);
    game_update(&game, (GameInput){.interact = 1, .digit = -1}, 1.0f / 60.0f);
    assert(game.room == ROOM_LIWA_ROAD);
    assert(game.village_discovered);
    assert(game.z == 42.0f);
}

static void test_gate_stays_closed_until_puzzle_is_solved(void)
{
    GameState game;
    game_init(&game);
    start_game(&game);
    game.x = 0.0f;
    game.z = 43.0f;
    for (int i = 0; i < 120; i++) {
        game_update(&game, (GameInput){.move_z = 1.0f, .digit = -1}, 1.0f / 60.0f);
    }
    assert(game.z < 46.0f);
    game.storeroom_open = 1;
    for (int i = 0; i < 120; i++) {
        game_update(&game, (GameInput){.move_z = 1.0f, .digit = -1}, 1.0f / 60.0f);
    }
    assert(game.z > 46.0f);
}

static void test_wrong_keypad_code_does_not_open_shortcut(void)
{
    GameState game;
    game_init(&game);
    start_game(&game);
    enter_station(&game);
    walk_inside_to(&game, -3.3f, 0.4f);
    game_update(&game, (GameInput){.interact = 1, .digit = -1}, 1.0f / 60.0f);
    walk_inside_to(&game, 3.9f, 4.4f);
    game_update(&game, (GameInput){.interact = 1, .digit = -1}, 1.0f / 60.0f);
    const int wrong_digits[] = {0, 7, 1, 3};
    for (int i = 0; i < 4; i++) {
        game_update(&game, (GameInput){.digit = wrong_digits[i]}, 1.0f / 60.0f);
    }
    game_update(&game, (GameInput){.interact = 1, .digit = -1}, 1.0f / 60.0f);
    assert(game.keypad_active);
    assert(!game.storeroom_open);
    assert(game.keypad_code == 0);
}

static void test_collision_stops_at_station_wall(void)
{
    GameState game;
    game_init(&game);
    start_game(&game);
    game.x = 12.0f;
    game.z = 18.0f;
    for (int i = 0; i < 120; i++) {
        game_update(&game, (GameInput){.move_z = 1.0f, .digit = -1}, 1.0f / 60.0f);
    }
    assert(game.z < 21.0f);
}

static void test_map_inventory_flashlight_and_battery(void)
{
    GameState game;
    game_init(&game);
    start_game(&game);
    game_update(&game, (GameInput){.toggle_map = 1, .digit = -1}, 1.0f / 60.0f);
    assert(game.map_open);
    game_update(&game, (GameInput){.toggle_inventory = 1, .digit = -1}, 1.0f / 60.0f);
    assert(!game.map_open && game.inventory_open);
    game_update(&game, (GameInput){.toggle_inventory = 1, .digit = -1}, 1.0f / 60.0f);
    game_update(&game, (GameInput){.toggle_flashlight = 1, .digit = -1}, 1.0f / 60.0f);
    assert(game.flashlight_on);
    float battery = game.battery;
    for (int i = 0; i < 120; i++) {
        game_update(&game, (GameInput){.digit = -1}, 1.0f / 60.0f);
    }
    assert(game.battery < battery);
}

int main(void)
{
    test_initial_state_is_on_foot();
    test_intro_pauses_movement();
    test_walking_sprinting_and_stamina();
    test_station_recording_keypad_and_shortcut();
    test_gate_stays_closed_until_puzzle_is_solved();
    test_wrong_keypad_code_does_not_open_shortcut();
    test_collision_stops_at_station_wall();
    test_map_inventory_flashlight_and_battery();
    puts("All on-foot survival-horror tests passed.");
    return 0;
}
