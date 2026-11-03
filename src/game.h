#ifndef SANDVEIL_GAME_H
#define SANDVEIL_GAME_H

typedef enum GameRoom {
    ROOM_LIWA_ROAD,
    ROOM_PETROL_STATION
} GameRoom;

typedef struct GameInput {
    float move_x;
    float move_z;
    int sprint;
    int interact;
    int toggle_flashlight;
    int toggle_map;
    int toggle_inventory;
    int continue_intro;
    int backspace;
    int digit;
} GameInput;

typedef struct GameState {
    float x;
    float z;
    float yaw;
    float clock_hours;
    float elapsed_seconds;
    float message_seconds;
    float storm_seconds;
    float battery;
    float stamina;
    float fog_amount;
    int health;
    int flashlight_on;
    int storm_active;
    int landscape_shifted;
    int intro_active;
    int intro_page;
    int map_open;
    int inventory_open;
    int recorder_heard;
    int keypad_active;
    int keypad_code;
    int keypad_digits;
    int storeroom_open;
    int station_discovered;
    int village_discovered;
    int figure_gone;
    GameRoom room;
    const char *message;
} GameState;

void game_init(GameState *game);
void game_update(GameState *game, GameInput input, float delta_time);
float game_world_height(float x, float z, int shifted);
float game_daylight(float clock_hours);

#endif
