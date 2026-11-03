#include "game.h"
#include "world.h"

#include <math.h>
#include <stdio.h>

#include "raymath.h"

#define VIEW_WIDTH 426
#define VIEW_HEIGHT 240

static GameInput read_input(const GameState *game)
{
    GameInput input = {0};
    if (!game->map_open && !game->inventory_open && !game->keypad_active) {
        input.move_z = (float)IsKeyDown(KEY_W) - (float)IsKeyDown(KEY_S);
        input.move_x = (float)IsKeyDown(KEY_D) - (float)IsKeyDown(KEY_A);
        input.sprint = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    }
    input.interact = IsKeyPressed(KEY_E);
    input.toggle_flashlight = IsKeyPressed(KEY_F);
    input.toggle_map = IsKeyPressed(KEY_M);
    input.toggle_inventory = IsKeyPressed(KEY_I);
    input.continue_intro = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    input.backspace = IsKeyPressed(KEY_BACKSPACE);
    input.digit = -1;
    for (int digit = 0; digit <= 9; digit++) {
        if (IsKeyPressed(KEY_ZERO + digit)) input.digit = digit;
    }
    return input;
}

static const char *interaction_hint(const GameState *game)
{
    if (game->room == ROOM_LIWA_ROAD) {
        if (hypotf(game->x - 12.0f, game->z - 19.3f) < 3.4f) {
            return "E  ENTER THE PETROL STATION";
        }
        if (hypotf(game->x + 37.0f, game->z - 76.0f) < 8.0f) {
            return "E  EXAMINE THE VILLAGE SHOP";
        }
        return NULL;
    }
    if (game->keypad_active) return "0-9 ENTER CODE   BACKSPACE ERASES   E CONFIRMS";
    if (hypotf(game->x + 3.3f, game->z - 0.4f) < 2.8f) {
        return "E  PLAY THE CASSETTE";
    }
    if (hypotf(game->x - 3.9f, game->z - 4.4f) < 2.8f) {
        return "E  EXAMINE THE KEYPAD";
    }
    if (hypotf(game->x, game->z - 5.8f) < 2.8f) {
        return game->storeroom_open ? "E  ENTER THE SERVICE PASSAGE"
            : "E  EXAMINE THE LOCKED BACK DOOR";
    }
    if (game->z < -6.0f && fabsf(game->x) < 2.2f) return "E  RETURN TO THE ROAD";
    return NULL;
}

static void draw_gauge(int x, int y, const char *label, float amount, Color color)
{
    DrawText(label, x, y, 6, (Color){224, 214, 196, 255});
    DrawRectangle(x, y + 8, 52, 4, (Color){33, 31, 35, 255});
    DrawRectangle(x, y + 8, (int)(52.0f * Clamp(amount / 100.0f, 0.0f, 1.0f)),
        4, color);
}

static void draw_paper_map(const GameState *game)
{
    DrawRectangle(47, 25, 332, 188, (Color){211, 192, 154, 245});
    DrawRectangleLines(47, 25, 332, 188, (Color){79, 61, 49, 255});
    DrawText("LIWA  /  FIELD SKETCH", 61, 36, 8, (Color){63, 53, 48, 255});
    DrawText("1998   -   NOT TO SCALE", 61, 49, 6, (Color){87, 72, 58, 255});

    DrawLineEx((Vector2){207, 190}, (Vector2){207, 100}, 4.0f, (Color){133, 102, 76, 255});
    DrawLineEx((Vector2){207, 100}, (Vector2){154, 79}, 4.0f, (Color){133, 102, 76, 255});
    DrawLineEx((Vector2){154, 79}, (Vector2){126, 67}, 4.0f, (Color){133, 102, 76, 255});
    DrawLineEx((Vector2){207, 100}, (Vector2){230, 62}, 3.0f, (Color){133, 102, 76, 255});
    DrawCircle(207, 119, 5.0f, (Color){118, 69, 50, 255});
    DrawText("PETROL", 218, 114, 6, (Color){70, 56, 49, 255});

    if (game->village_discovered) {
        DrawCircle(130, 69, 4.0f, (Color){116, 70, 51, 255});
        DrawText("AL FAREEJ", 87, 55, 6, (Color){70, 56, 49, 255});
    } else {
        DrawText("?", 119, 57, 9, (Color){95, 77, 59, 255});
    }
    DrawCircle(230, 62, 3.0f, (Color){114, 93, 66, 255});
    DrawText("WELL?", 238, 57, 6, (Color){82, 68, 56, 255});

    float map_x = 207.0f + game->x * 2.2f;
    float map_y = 190.0f - (game->z + 24.0f) * 1.35f;
    DrawCircle((int)map_x, (int)map_y, 4.0f, (Color){136, 47, 42, 255});
    DrawText("SALEM", (int)map_x + 7, (int)map_y - 4, 6, (Color){88, 47, 42, 255});
    DrawText("M  CLOSE", 319, 193, 6, (Color){79, 61, 49, 255});
}

static void draw_inventory(const GameState *game)
{
    DrawRectangle(76, 40, 274, 159, (Color){17, 18, 24, 240});
    DrawRectangleLines(76, 40, 274, 159, (Color){170, 132, 92, 255});
    DrawText("SALEM'S POCKETS", 94, 54, 9, (Color){224, 182, 131, 255});
    DrawText("01  PAPER MAP", 98, 78, 7, (Color){206, 200, 188, 255});
    DrawText(TextFormat("02  FLASHLIGHT  /  BATTERY %02d%%", (int)game->battery),
        98, 93, 7, (Color){206, 200, 188, 255});
    DrawText(TextFormat("03  STAMINA %02d%%", (int)game->stamina),
        98, 108, 7, (Color){206, 200, 188, 255});
    DrawText(game->recorder_heard ? "04  CASSETTE CLUE: 0712" : "04  CASSETTE CLUE: NOT HEARD",
        98, 123, 7, (Color){206, 200, 188, 255});
    DrawText(game->storeroom_open ? "05  SERVICE DOOR KEY: TURNED" : "05  SERVICE DOOR: LOCKED",
        98, 138, 7, (Color){206, 200, 188, 255});
    DrawText("I  CLOSE", 288, 179, 6, (Color){170, 154, 132, 255});
}

static void draw_hud(const GameState *game)
{
    DrawRectangle(6, 6, 135, 39, (Color){14, 15, 22, 215});
    DrawText(game->room == ROOM_PETROL_STATION
        ? "LIWA  /  AL WUROOD STATION" : "LIWA  /  EMPTY QUARTER", 12, 10, 7,
        (Color){224, 183, 134, 255});
    int hours = (int)game->clock_hours;
    int minutes = (int)((game->clock_hours - (float)hours) * 60.0f);
    DrawText(TextFormat("%s   %02d:%02d",
        game->room == ROOM_PETROL_STATION ? "INDOORS" : "ON FOOT", hours, minutes),
        12, 22, 7, (Color){197, 192, 182, 255});
    draw_gauge(12, 34, "BATTERY", game->battery, (Color){192, 169, 118, 255});
    draw_gauge(73, 34, "STAMINA", game->stamina, (Color){137, 174, 139, 255});
    DrawText(TextFormat("HEALTH %02d", game->health), 348, 10, 7,
        (Color){210, 194, 181, 255});
    DrawText(game->flashlight_on ? "LIGHT  ON" : "LIGHT OFF", 348, 22, 7,
        (Color){210, 194, 181, 255});
    DrawText(game->storm_active ? "SHAMAL" : "FOG", 348, 34, 7,
        (Color){210, 194, 181, 255});

    const char *hint = interaction_hint(game);
    if (hint != NULL && !game->intro_active && !game->map_open && !game->inventory_open) {
        int width = MeasureText(hint, 7);
        DrawRectangle((VIEW_WIDTH - width) / 2 - 7, 181, width + 14, 15,
            (Color){14, 15, 22, 215});
        DrawText(hint, (VIEW_WIDTH - width) / 2, 185, 7,
            (Color){226, 204, 173, 255});
    }

    if (game->message_seconds > 0.0f && game->message != NULL
        && game->message[0] != '\0' && !game->intro_active) {
        int width = MeasureText(game->message, 7);
        if (width > VIEW_WIDTH - 18) width = VIEW_WIDTH - 18;
        DrawRectangle((VIEW_WIDTH - width) / 2 - 5, 202, width + 10, 16,
            (Color){14, 15, 22, 220});
        DrawText(game->message, (VIEW_WIDTH - width) / 2, 207, 7,
            (Color){232, 213, 183, 255});
    }

    const char *controls = "WASD WALK   SHIFT RUN   E EXAMINE   F LIGHT   M MAP   I ITEMS";
    DrawRectangle(0, VIEW_HEIGHT - 12, VIEW_WIDTH, 12, (Color){12, 13, 18, 225});
    DrawText(controls, 24, VIEW_HEIGHT - 8, 6, (Color){193, 186, 172, 255});
}

static void draw_keypad(const GameState *game)
{
    if (!game->keypad_active) return;
    DrawRectangle(131, 70, 164, 68, (Color){13, 14, 20, 235});
    DrawRectangleLines(131, 70, 164, 68, (Color){172, 128, 82, 255});
    DrawText("SERVICE LOCK", 166, 79, 8, (Color){222, 181, 132, 255});
    DrawText(TextFormat("%04d", game->keypad_code), 184, 98, 13, WHITE);
    DrawText("DIGITS  /  E ENTER  /  BACKSPACE", 145, 124, 6,
        (Color){196, 186, 166, 255});
}

static void draw_intro(const GameState *game)
{
    static const char *headings[] = {
        "LIWA OASIS  /  1998",
        "A VOICE LEFT ON TAPE",
        "THE ROAD THAT MOVED"
    };
    static const char *body[] = {
        "Two nights ago, Salem's older brother Nasser drove south and vanished.",
        "At dusk, Salem follows his brother's fresh tyre marks into the dunes.",
        "Nasser's last cassette: \"The well road isn't where it was.\"",
        "A white Land Cruiser rests beside the road. Salem continues on foot.",
        "At Al Wurood station, a cassette waits beneath a coat of dust.",
        "Beyond the village, a motionless figure stands against the last light."
    };
    int page = game->intro_page;
    DrawRectangle(17, 35, VIEW_WIDTH - 34, 169, (Color){15, 14, 24, 238});
    DrawRectangle(17, 35, VIEW_WIDTH - 34, 2, (Color){199, 130, 88, 255});
    DrawText("S A N D V E I L", 34, 50, 10, (Color){224, 177, 126, 255});
    DrawText(headings[page], 34, 76, 8, (Color){231, 220, 199, 255});
    DrawText(body[page * 2], 34, 102, 7, (Color){199, 194, 187, 255});
    DrawText(body[page * 2 + 1], 34, 119, 7, (Color){199, 194, 187, 255});
    DrawText("A SURVIVAL MYSTERY IN THE EMPTY QUARTER", 34, 158, 6,
        (Color){149, 142, 148, 255});
    DrawText("ENTER / SPACE  —  CONTINUE", 34, 181, 7,
        (Color){228, 192, 148, 255});
}

static int in_fixed_camera_zone(const GameState *game)
{
    return game->room == ROOM_PETROL_STATION
        || (game->x > -62.0f && game->x < -16.0f
            && game->z > 60.0f && game->z < 104.0f);
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1024, 576, "SANDVEIL - Liwa");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    GameState game;
    game_init(&game);
    World world;
    world_init(&world, game.landscape_shifted);
    RenderTexture2D scene = LoadRenderTexture(VIEW_WIDTH, VIEW_HEIGHT);
    SetTextureFilter(scene.texture, TEXTURE_FILTER_POINT);

    Camera3D camera = {0};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 54.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    Vector3 camera_position = {0.0f, 3.0f, -32.0f};
    Vector3 camera_target = {0.0f, 0.0f, -24.0f};

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        GameInput input = read_input(&game);
        int was_shifted = game.landscape_shifted;
        game_update(&game, input, dt);
        if (was_shifted != game.landscape_shifted) {
            world_rebuild_terrain(&world, game.landscape_shifted);
        }

        float daylight = game_daylight(game.clock_hours);
        float ground = game.room == ROOM_PETROL_STATION ? 0.0f
            : game_world_height(game.x, game.z, game.landscape_shifted);
        Vector3 focus = {game.x, ground + 1.2f, game.z};
        Vector3 desired_position;
        Vector3 desired_target;
        if (game.room == ROOM_PETROL_STATION) {
            desired_position = (Vector3){0.0f, 8.0f, -12.5f};
            desired_target = (Vector3){0.0f, 0.8f, 0.0f};
        } else {
            Vector3 forward = {sinf(game.yaw), 0.0f, cosf(game.yaw)};
            int fixed = in_fixed_camera_zone(&game);
            desired_target = fixed
                ? (Vector3){focus.x, focus.y - 0.35f, focus.z + 1.5f}
                : focus;
            desired_position = fixed
                ? (Vector3){focus.x, ground + 8.5f, focus.z - 10.5f}
                : (Vector3){focus.x - forward.x * 8.5f, ground + 4.4f,
                    focus.z - forward.z * 8.5f};
        }
        float camera_blend = fminf(1.0f, dt * 2.6f);
        camera_position = Vector3Lerp(camera_position, desired_position, camera_blend);
        camera_target = Vector3Lerp(camera_target, desired_target, camera_blend);
        camera.position = camera_position;
        camera.target = camera_target;
        float storm_amount = game.storm_active
            ? 0.7f + 0.3f * sinf(game.elapsed_seconds * 8.0f) : 0.0f;

        BeginTextureMode(scene);
        BeginMode3D(camera);
        world_draw(&world, focus, game.yaw, daylight, game.storm_active, storm_amount);
        if (game.room == ROOM_LIWA_ROAD) {
            world_draw_map(&world, game.landscape_shifted, daylight,
                game.elapsed_seconds > 24.0f, game.figure_gone, game.storeroom_open);
            world_draw_salem(&world, (Vector3){game.x, ground, game.z},
                game.yaw, game.flashlight_on);
        } else {
            world_draw_station_interior(&world, game.storeroom_open, game.flashlight_on);
            world_draw_salem(&world, (Vector3){game.x, 0.0f, game.z},
                game.yaw, game.flashlight_on);
        }
        EndMode3D();

        if (game.storm_active || game.room == ROOM_PETROL_STATION) {
            Color veil = game.storm_active
                ? (Color){177, 145, 124, 66}
                : (game.flashlight_on ? (Color){16, 15, 22, 25} : (Color){12, 12, 20, 95});
            DrawRectangle(0, 0, VIEW_WIDTH, VIEW_HEIGHT, veil);
        }
        if (game.storm_active) {
            for (int i = 0; i < 75; i++) {
                int x = (int)(fmodf((float)(i * 67) + game.elapsed_seconds
                    * (160.0f + (float)(i % 5) * 19.0f), (float)VIEW_WIDTH));
                int y = (i * 41 + (int)(game.elapsed_seconds * 92.0f)) % VIEW_HEIGHT;
                DrawRectangle(x, y, 2 + i % 3, 1, (Color){235, 210, 187, 100});
            }
        }
        draw_hud(&game);
        if (game.map_open) draw_paper_map(&game);
        if (game.inventory_open) draw_inventory(&game);
        draw_keypad(&game);
        if (game.intro_active) draw_intro(&game);
        EndTextureMode();

        BeginDrawing();
        ClearBackground((Color){8, 8, 13, 255});
        float scale = fminf((float)GetScreenWidth() / (float)VIEW_WIDTH,
            (float)GetScreenHeight() / (float)VIEW_HEIGHT);
        float draw_width = VIEW_WIDTH * scale;
        float draw_height = VIEW_HEIGHT * scale;
        Rectangle source = {0.0f, 0.0f, (float)scene.texture.width,
            -(float)scene.texture.height};
        Rectangle destination = {(GetScreenWidth() - draw_width) * 0.5f,
            (GetScreenHeight() - draw_height) * 0.5f, draw_width, draw_height};
        DrawTexturePro(scene.texture, source, destination, (Vector2){0.0f, 0.0f},
            0.0f, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(scene);
    world_unload(&world);
    CloseWindow();
    return 0;
}
