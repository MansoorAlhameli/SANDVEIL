#include "game.h"

#include <math.h>

#define PI_F 3.14159265358979323846f

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float smoothstep(float edge0, float edge1, float value)
{
    float t = clampf((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static float distance_to(float x, float z, float target_x, float target_z)
{
    return hypotf(x - target_x, z - target_z);
}

static void set_message(GameState *game, const char *message, float duration)
{
    game->message = message;
    game->message_seconds = duration;
}

void game_init(GameState *game)
{
    *game = (GameState){0};
    game->x = 0.0f;
    game->z = -24.0f;
    game->yaw = 0.0f;
    game->clock_hours = 18.15f;
    game->battery = 100.0f;
    game->stamina = 100.0f;
    game->health = 100;
    game->fog_amount = 0.48f;
    game->room = ROOM_LIWA_ROAD;
    game->intro_active = 1;
    game->message = "";
}

float game_world_height(float x, float z, int shifted)
{
    const float shift = shifted ? 1.0f : 0.0f;
    float broad = sinf(x * 0.010f + shift * 0.73f) * 7.4f
        + sinf(z * 0.014f - x * 0.005f + shift * 0.42f) * 5.0f;
    float crest_a = expf(-((x - 48.0f) * (x - 48.0f) / 5200.0f
        + (z - 94.0f) * (z - 94.0f) / 12000.0f)) * 8.0f;
    float crest_b = expf(-((x + 86.0f) * (x + 86.0f) / 7600.0f
        + (z + 35.0f) * (z + 35.0f) / 9000.0f)) * 6.0f;
    float ripples = sinf(x * 0.035f + z * 0.021f + shift * 1.8f) * 0.8f;
    return broad + crest_a + crest_b + ripples;
}

float game_daylight(float clock_hours)
{
    float sun = sinf(((clock_hours - 6.0f) / 24.0f) * 2.0f * PI_F);
    return smoothstep(-0.12f, 0.26f, sun);
}

static int collides_with_station(float x, float z)
{
    if (x < 5.0f || x > 19.0f || z < 21.0f || z > 33.0f) return 0;
    return 1;
}

static int collides_with_house(float x, float z, float center_x, float center_z,
    float half_width, float half_depth)
{
    return x > center_x - half_width && x < center_x + half_width
        && z > center_z - half_depth && z < center_z + half_depth;
}

static int collides_with_gate(const GameState *game, float x, float z)
{
    return !game->storeroom_open && x > -4.0f && x < 4.0f && z > 46.0f && z < 49.0f;
}

static int collides_outdoors(const GameState *game, float x, float z)
{
    if (x < -74.0f || x > 74.0f || z < -90.0f || z > 148.0f) return 1;
    if (collides_with_station(x, z)) return 1;
    if (collides_with_house(x, z, -37.0f, 76.0f, 6.0f, 5.0f)) return 1;
    if (collides_with_house(x, z, -52.0f, 88.0f, 5.0f, 4.5f)) return 1;
    if (collides_with_house(x, z, -22.0f, 91.0f, 5.5f, 5.0f)) return 1;
    return collides_with_gate(game, x, z);
}

static int collides_in_station(const GameState *game, float x, float z)
{
    if (x < -6.0f || x > 6.0f || z < -7.0f || z > 6.0f) return 1;
    if (x < -4.7f && z > -2.0f && z < 4.5f) return 1;
    if (x > 4.7f && z > -2.0f && z < 4.5f) return 1;
    if (z < -6.0f && (x < -1.2f || x > 1.2f)) return 1;
    if (z > 5.7f && !game->storeroom_open
        && x > -1.3f && x < 1.3f) return 1;
    return 0;
}

static void move_player(GameState *game, GameInput input, float dt)
{
    float move_x = input.move_x;
    float move_z = input.move_z;
    float magnitude = hypotf(move_x, move_z);
    if (magnitude > 1.0f) {
        move_x /= magnitude;
        move_z /= magnitude;
        magnitude = 1.0f;
    }

    if (magnitude > 0.05f) {
        game->yaw = atan2f(move_x, move_z);
        float speed = input.sprint && game->stamina > 0.0f ? 4.5f : 2.55f;
        if (input.sprint && game->stamina > 0.0f) {
            game->stamina = fmaxf(0.0f, game->stamina - dt * 28.0f);
        }
        float next_x = game->x + move_x * speed * dt;
        float next_z = game->z + move_z * speed * dt;
        int collision = game->room == ROOM_LIWA_ROAD
            ? collides_outdoors(game, next_x, next_z)
            : collides_in_station(game, next_x, next_z);
        if (!collision) {
            game->x = next_x;
            game->z = next_z;
        } else {
            int x_collision = game->room == ROOM_LIWA_ROAD
                ? collides_outdoors(game, next_x, game->z)
                : collides_in_station(game, next_x, game->z);
            int z_collision = game->room == ROOM_LIWA_ROAD
                ? collides_outdoors(game, game->x, next_z)
                : collides_in_station(game, game->x, next_z);
            if (!x_collision) game->x = next_x;
            if (!z_collision) game->z = next_z;
        }
    }

    if (!input.sprint || magnitude < 0.05f || game->stamina <= 0.0f) {
        game->stamina = fminf(100.0f, game->stamina + dt * 17.0f);
    }
}

static void update_interaction(GameState *game, GameInput input)
{
    if (game->room == ROOM_LIWA_ROAD) {
        if (distance_to(game->x, game->z, 12.0f, 19.3f) < 2.3f) {
            game->room = ROOM_PETROL_STATION;
            game->x = 0.0f;
            game->z = -5.4f;
            game->yaw = 0.0f;
            game->station_discovered = 1;
            set_message(game, "THE DOOR DRAGS OPEN. DUST LIES UNDISTURBED INSIDE.", 5.0f);
        } else if (distance_to(game->x, game->z, -37.0f, 69.8f) < 2.0f) {
            game->village_discovered = 1;
            set_message(game, "A NARROW TRACK LEADS INTO THE OLD FAREEJ.", 4.0f);
        } else if (distance_to(game->x, game->z, -37.0f, 70.0f) < 5.5f) {
            set_message(game, "THE VILLAGE SHOP IS LOCKED FROM THE OTHER SIDE.", 3.0f);
        }
        return;
    }

    if (game->keypad_active) {
        if (input.backspace) {
            game->keypad_code /= 10;
            if (game->keypad_digits > 0) game->keypad_digits--;
        }
        if (input.digit >= 0 && input.digit <= 9 && game->keypad_digits < 4) {
            game->keypad_code = game->keypad_code * 10 + input.digit;
            game->keypad_digits++;
        }
        if (input.interact) {
            if (game->keypad_digits == 4 && game->keypad_code == 712) {
                game->storeroom_open = 1;
                game->keypad_active = 0;
                game->keypad_code = 0;
                game->keypad_digits = 0;
                set_message(game, "0712. THE LOCK RELEASES. THE BACK DOOR IS OPEN.", 5.0f);
            } else {
                game->keypad_code = 0;
                game->keypad_digits = 0;
                set_message(game, "THE LOCK RESETS. LISTEN TO THE CASSETTE AGAIN.", 3.0f);
            }
        }
        return;
    }

    if (distance_to(game->x, game->z, -3.3f, 0.4f) < 2.2f && input.interact) {
        game->recorder_heard = 1;
        set_message(game, "\"THE OLD TANK NUMBER. ZERO... SEVEN... ONE... TWO.\"", 6.0f);
    } else if (distance_to(game->x, game->z, 3.9f, 4.4f) < 2.2f && input.interact) {
        game->keypad_active = 1;
        game->keypad_code = 0;
        game->keypad_digits = 0;
        set_message(game, game->recorder_heard
            ? "ENTER THE FOUR DIGITS FROM THE CASSETTE. E CONFIRMS."
            : "A FOUR-KEY LOCK. THE CASSETTE PLAYER IS ON THE COUNTER.", 4.0f);
    } else if (distance_to(game->x, game->z, 0.0f, 5.8f) < 2.0f && input.interact) {
        if (game->storeroom_open) {
            game->room = ROOM_LIWA_ROAD;
        game->x = 0.0f;
        game->z = 42.0f;
            game->yaw = 0.0f;
            game->village_discovered = 1;
            set_message(game, "A SERVICE PASSAGE OPENS INTO THE VILLAGE ALLEY.", 5.0f);
        } else {
            set_message(game, "LOCKED. A KEYPAD WAITS BESIDE THE DOOR.", 3.0f);
        }
    } else if (game->z < -6.0f && fabsf(game->x) < 2.2f && input.interact) {
        game->room = ROOM_LIWA_ROAD;
        game->x = 12.0f;
        game->z = 19.4f;
        game->yaw = 3.14159265f;
    }
}

void game_update(GameState *game, GameInput input, float delta_time)
{
    if (game->intro_active) {
        if (input.continue_intro) {
            if (game->intro_page < 2) game->intro_page++;
            else game->intro_active = 0;
        }
        return;
    }

    float dt = clampf(delta_time, 0.0f, 0.05f);
    game->elapsed_seconds += dt;
    game->clock_hours += dt * 0.025f;
    if (game->clock_hours >= 24.0f) game->clock_hours -= 24.0f;
    game->message_seconds = fmaxf(0.0f, game->message_seconds - dt);
    game->fog_amount = game->storm_active
        ? 0.88f + 0.08f * sinf(game->elapsed_seconds * 4.0f) : 0.48f;

    if (input.toggle_map && !game->keypad_active) {
        game->map_open = !game->map_open;
        game->inventory_open = 0;
    }
    if (input.toggle_inventory && !game->keypad_active) {
        game->inventory_open = !game->inventory_open;
        game->map_open = 0;
    }
    if (input.toggle_flashlight && game->battery > 0.0f) {
        game->flashlight_on = !game->flashlight_on;
        set_message(game, game->flashlight_on ? "FLASHLIGHT ON." : "FLASHLIGHT OFF.", 1.8f);
    }
    if (input.continue_intro) {
        game->map_open = 0;
        game->inventory_open = 0;
    }

    if (!game->map_open && !game->inventory_open && !game->keypad_active) {
        move_player(game, input, dt);
        if (input.interact) update_interaction(game, input);
    } else if (game->keypad_active) {
        update_interaction(game, input);
    }

    if (game->flashlight_on) {
        game->battery = fmaxf(0.0f, game->battery - dt * 0.28f);
        if (game->battery <= 0.0f) {
            game->flashlight_on = 0;
            set_message(game, "THE FLASHLIGHT BATTERY IS DEAD.", 3.0f);
        }
    }

    if (!game->figure_gone && game->room == ROOM_LIWA_ROAD
        && game->elapsed_seconds > 24.0f
        && distance_to(game->x, game->z, -38.0f, 105.0f) < 4.0f) {
        game->figure_gone = 1;
        set_message(game, "THE DUNE IS EMPTY. A SECOND SET OF TRACKS LEADS BACK.", 6.0f);
    }

    if (!game->storm_active && !game->landscape_shifted && game->elapsed_seconds > 90.0f) {
        game->storm_active = 1;
        game->storm_seconds = 16.0f;
        set_message(game, "THE WIND STOPS. THE SAND KEEPS FALLING.", 5.0f);
    } else if (game->storm_active) {
        game->storm_seconds -= dt;
        if (game->storm_seconds <= 0.0f) {
            game->storm_active = 0;
            game->landscape_shifted = !game->landscape_shifted;
            set_message(game, "THE STORM PASSES. THE ROAD IS NOT WHERE IT WAS.", 5.0f);
        }
    }
}
