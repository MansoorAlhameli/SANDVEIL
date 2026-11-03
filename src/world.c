#include "world.h"

#include "game.h"

#include <math.h>
#include <stdlib.h>
#include "raymath.h"

#define TERRAIN_CELLS 128
#define TERRAIN_STEP 4.0f
#define TERRAIN_HALF (TERRAIN_CELLS * TERRAIN_STEP * 0.5f)

static Color color_lerp(Color a, Color b, float amount)
{
    amount = Clamp(amount, 0.0f, 1.0f);
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * amount),
        (unsigned char)(a.g + (b.g - a.g) * amount),
        (unsigned char)(a.b + (b.b - a.b) * amount),
        255
    };
}

static void terrain_upload(World *world, int shifted)
{
    if (world->terrain.vertices != NULL) UnloadMesh(world->terrain);
    Mesh mesh = {0};
    const int side = TERRAIN_CELLS + 1;
    mesh.vertexCount = side * side;
    mesh.triangleCount = TERRAIN_CELLS * TERRAIN_CELLS * 2;
    mesh.vertices = MemAlloc((unsigned int)(mesh.vertexCount * 3 * sizeof(float)));
    mesh.normals = MemAlloc((unsigned int)(mesh.vertexCount * 3 * sizeof(float)));
    mesh.colors = MemAlloc((unsigned int)(mesh.vertexCount * 4));
    mesh.indices = MemAlloc((unsigned int)(mesh.triangleCount * 3 * sizeof(unsigned short)));

    for (int z = 0; z < side; z++) {
        for (int x = 0; x < side; x++) {
            int index = z * side + x;
            float world_x = -TERRAIN_HALF + x * TERRAIN_STEP;
            float world_z = -TERRAIN_HALF + z * TERRAIN_STEP;
            float height = game_world_height(world_x, world_z, shifted);
            mesh.vertices[index * 3] = world_x;
            mesh.vertices[index * 3 + 1] = height;
            mesh.vertices[index * 3 + 2] = world_z;

            float left = game_world_height(world_x - 1.0f, world_z, shifted);
            float right = game_world_height(world_x + 1.0f, world_z, shifted);
            float back = game_world_height(world_x, world_z - 1.0f, shifted);
            float front = game_world_height(world_x, world_z + 1.0f, shifted);
            Vector3 normal = Vector3Normalize((Vector3){left - right, 2.0f, back - front});
            mesh.normals[index * 3] = normal.x;
            mesh.normals[index * 3 + 1] = normal.y;
            mesh.normals[index * 3 + 2] = normal.z;

            float distance = sqrtf(world_x * world_x + world_z * world_z);
            float haze = Clamp((distance - 95.0f) / 170.0f, 0.0f, 0.74f);
            float ridge = Clamp((height + 3.0f) / 20.0f, 0.0f, 1.0f);
            Color sand = color_lerp((Color){183, 105, 55, 255},
                (Color){214, 151, 86, 255}, ridge);
            Color shaded = color_lerp((Color){139, 81, 64, 255}, sand, 0.72f);
            Color fogged = color_lerp(shaded, (Color){197, 135, 101, 255}, haze);
            mesh.colors[index * 4] = fogged.r;
            mesh.colors[index * 4 + 1] = fogged.g;
            mesh.colors[index * 4 + 2] = fogged.b;
            mesh.colors[index * 4 + 3] = 255;
        }
    }

    int cursor = 0;
    for (int z = 0; z < TERRAIN_CELLS; z++) {
        for (int x = 0; x < TERRAIN_CELLS; x++) {
            unsigned short a = (unsigned short)(z * side + x);
            unsigned short b = (unsigned short)(a + 1);
            unsigned short c = (unsigned short)(a + side);
            unsigned short d = (unsigned short)(c + 1);
            mesh.indices[cursor++] = a;
            mesh.indices[cursor++] = c;
            mesh.indices[cursor++] = b;
            mesh.indices[cursor++] = b;
            mesh.indices[cursor++] = c;
            mesh.indices[cursor++] = d;
        }
    }

    UploadMesh(&mesh, false);
    world->terrain = mesh;
}

void world_init(World *world, int shifted)
{
    world->terrain = (Mesh){0};
    world->terrain_material = LoadMaterialDefault();
    world->terrain_material.maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    world->cube = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    world->sphere = LoadModelFromMesh(GenMeshSphere(1.0f, 6, 4));
    world_rebuild_terrain(world, shifted);
}

void world_rebuild_terrain(World *world, int shifted)
{
    terrain_upload(world, shifted);
}

static void draw_box(World *world, Vector3 position, Vector3 scale,
    float yaw_degrees, Color color)
{
    DrawModelEx(world->cube, position, (Vector3){0.0f, 1.0f, 0.0f},
        yaw_degrees, scale, color);
}

void world_draw_car(World *world, Vector3 position, float yaw, int abandoned)
{
    Color body = abandoned ? (Color){132, 107, 86, 255} : (Color){223, 220, 198, 255};
    Color window = abandoned ? (Color){77, 75, 71, 255} : (Color){53, 73, 83, 255};
    float angle = yaw * RAD2DEG;
    draw_box(world, (Vector3){position.x, position.y + 0.55f, position.z},
        (Vector3){2.1f, 0.62f, 4.2f}, angle, body);
    draw_box(world, (Vector3){position.x, position.y + 1.02f, position.z - 0.15f},
        (Vector3){1.72f, 0.72f, 2.15f}, angle, body);
    draw_box(world, (Vector3){position.x, position.y + 1.12f, position.z + 0.38f},
        (Vector3){1.56f, 0.46f, 0.08f}, angle, window);
    draw_box(world, (Vector3){position.x, position.y + 1.12f, position.z - 0.83f},
        (Vector3){1.56f, 0.46f, 0.08f}, angle, window);
    for (int side = -1; side <= 1; side += 2) {
        for (int axle = -1; axle <= 1; axle += 2) {
            float local_x = (float)side * 1.02f;
            float local_z = (float)axle * 1.3f;
            float world_x = position.x + local_x * cosf(yaw) + local_z * sinf(yaw);
            float world_z = position.z - local_x * sinf(yaw) + local_z * cosf(yaw);
            draw_box(world, (Vector3){world_x, position.y + 0.34f, world_z},
                (Vector3){0.34f, 0.62f, 0.78f}, angle, (Color){31, 29, 29, 255});
        }
    }
    draw_box(world, (Vector3){position.x, position.y + 0.43f,
        position.z + 2.12f}, (Vector3){1.75f, 0.22f, 0.16f}, angle,
        (Color){162, 57, 45, 255});
}

void world_draw_camp(World *world, int shifted, float daylight)
{
    float ground = game_world_height(31.0f, 29.0f, shifted);
    Color canvas = color_lerp((Color){116, 70, 53, 255},
        (Color){190, 139, 91, 255}, daylight);
    draw_box(world, (Vector3){31.0f, ground + 0.72f, 29.0f},
        (Vector3){3.4f, 1.35f, 2.7f}, -12.0f, canvas);
    draw_box(world, (Vector3){29.7f, ground + 1.02f, 29.0f},
        (Vector3){0.12f, 1.6f, 2.95f}, -12.0f, (Color){89, 61, 49, 255});
    draw_box(world, (Vector3){34.1f, ground + 0.34f, 31.3f},
        (Vector3){0.75f, 0.72f, 0.75f}, 0.0f, (Color){77, 67, 57, 255});
    draw_box(world, (Vector3){27.2f, ground + 0.3f, 32.4f},
        (Vector3){0.42f, 0.58f, 0.42f}, 0.0f, (Color){139, 106, 65, 255});
    DrawSphere((Vector3){33.7f, ground + 0.18f, 28.7f}, 0.35f,
        (Color){239, 115, 46, 255});
    DrawSphere((Vector3){33.7f, ground + 0.22f, 28.7f}, 0.18f,
        (Color){255, 205, 99, 255});
}

void world_draw_figure(World *world, Vector3 position, float daylight)
{
    Color silhouette = color_lerp((Color){23, 22, 37, 255},
        (Color){43, 38, 46, 255}, daylight);
    draw_box(world, (Vector3){position.x, position.y + 1.0f, position.z},
        (Vector3){0.34f, 1.05f, 0.25f}, 0.0f, silhouette);
    DrawModelEx(world->sphere, (Vector3){position.x, position.y + 1.72f, position.z},
        (Vector3){0.0f, 1.0f, 0.0f}, 0.0f, (Vector3){0.22f, 0.27f, 0.22f},
        silhouette);
    draw_box(world, (Vector3){position.x - 0.11f, position.y + 0.29f, position.z},
        (Vector3){0.1f, 0.62f, 0.13f}, -2.0f, silhouette);
    draw_box(world, (Vector3){position.x + 0.11f, position.y + 0.29f, position.z},
        (Vector3){0.1f, 0.62f, 0.13f}, 2.0f, silhouette);
}

void world_draw_salem(World *world, Vector3 position, float yaw, int flashlight_on)
{
    float angle = yaw * RAD2DEG;
    Color robe = (Color){197, 190, 170, 255};
    Color cloth = (Color){224, 214, 188, 255};
    Color skin = (Color){133, 91, 68, 255};
    draw_box(world, (Vector3){position.x, position.y + 0.91f, position.z},
        (Vector3){0.58f, 1.12f, 0.37f}, angle, robe);
    DrawModelEx(world->sphere, (Vector3){position.x, position.y + 1.62f, position.z},
        (Vector3){0.0f, 1.0f, 0.0f}, 0.0f, (Vector3){0.2f, 0.25f, 0.2f}, skin);
    draw_box(world, (Vector3){position.x, position.y + 1.76f, position.z},
        (Vector3){0.42f, 0.11f, 0.39f}, angle, cloth);
    for (int side = -1; side <= 1; side += 2) {
        float local_x = (float)side * 0.39f;
        float arm_x = position.x + local_x * cosf(yaw);
        float arm_z = position.z - local_x * sinf(yaw);
        draw_box(world, (Vector3){arm_x, position.y + 0.98f, arm_z},
            (Vector3){0.17f, 0.66f, 0.19f}, angle, robe);
        float foot_x = position.x + local_x * 0.48f * cosf(yaw);
        float foot_z = position.z - local_x * 0.48f * sinf(yaw);
        draw_box(world, (Vector3){foot_x, position.y + 0.1f, foot_z},
            (Vector3){0.17f, 0.19f, 0.32f}, angle, (Color){58, 47, 42, 255});
    }
    if (flashlight_on) {
        Vector3 start = {position.x + sinf(yaw) * 0.25f,
            position.y + 1.35f, position.z + cosf(yaw) * 0.25f};
        Vector3 end = {position.x + sinf(yaw) * 7.0f,
            position.y + 0.04f, position.z + cosf(yaw) * 7.0f};
        DrawCylinderEx(start, end, 0.12f, 1.3f, 8,
            (Color){238, 214, 160, 42});
    }
}

static void draw_mountain(float x, float z, float width, float height, Color color)
{
    Vector3 a = {x - width, 0.0f, z};
    Vector3 b = {x, height, z - 18.0f};
    Vector3 c = {x + width, 0.0f, z};
    Vector3 d = {x, 0.0f, z + 25.0f};
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(c, b, d, color);
    DrawTriangle3D(d, b, a, color);
    DrawTriangle3D(a, c, d, color);
}

void world_draw(World *world, Vector3 focus, float yaw, float daylight,
    int storm_active, float storm_amount)
{
    Color sky_day = (Color){205, 134, 110, 255};
    Color sky_night = (Color){27, 25, 49, 255};
    Color sky = color_lerp(sky_night, sky_day, daylight);
    if (storm_active) sky = color_lerp(sky, (Color){148, 103, 77, 255}, 0.75f);
    ClearBackground(sky);

    Vector3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
    Vector3 right = {forward.z, 0.0f, -forward.x};
    Color mountain = color_lerp((Color){62, 53, 75, 255},
        (Color){147, 98, 77, 255}, daylight);
    mountain = color_lerp(mountain, (Color){176, 125, 93, 255}, storm_amount * 0.7f);
    for (int i = -3; i <= 3; i++) {
        float distance = 215.0f + (float)((i * 37 + 280) % 48);
        float side = (float)i * 46.0f;
        float x = focus.x + forward.x * distance + right.x * side;
        float z = focus.z + forward.z * distance + right.z * side;
        draw_mountain(x, z, 42.0f + (float)((i + 3) % 3) * 12.0f,
            22.0f + (float)((i + 3) % 4) * 7.0f, mountain);
    }

    Color tint = color_lerp((Color){92, 88, 126, 255},
        (Color){255, 245, 226, 255}, daylight);
    if (storm_active) tint = color_lerp(tint, (Color){194, 155, 126, 255}, 0.48f);
    world->terrain_material.maps[MATERIAL_MAP_ALBEDO].color = tint;
    DrawMesh(world->terrain, world->terrain_material, MatrixIdentity());
}

static void draw_house(World *world, float x, float z, float width, float depth,
    float height, Color wall, Color roof)
{
    float ground = game_world_height(x, z, 0);
    draw_box(world, (Vector3){x, ground + height * 0.5f, z},
        (Vector3){width, height, depth}, 0.0f, wall);
    draw_box(world, (Vector3){x, ground + height + 0.16f, z},
        (Vector3){width + 0.6f, 0.32f, depth + 0.6f}, 0.0f, roof);
}

void world_draw_map(World *world, int shifted, float daylight,
    int show_figure, int figure_gone, int gate_open)
{
    Color road = color_lerp((Color){126, 84, 65, 255},
        (Color){153, 103, 72, 255}, daylight * 0.6f);
    for (int z = -48; z <= 48; z += 24) {
        float center_z = (float)z;
        draw_box(world, (Vector3){0.0f,
            game_world_height(0.0f, center_z, shifted) + 0.16f, center_z},
            (Vector3){8.0f, 0.12f, 24.0f}, 0.0f, road);
    }

    float station_x = 12.0f;
    float station_z = 27.0f;
    float station_ground = game_world_height(station_x, station_z, shifted);
    Color plaster = color_lerp((Color){116, 88, 73, 255},
        (Color){174, 137, 101, 255}, daylight * 0.72f);
    Color roof = color_lerp((Color){63, 56, 58, 255},
        (Color){109, 88, 72, 255}, daylight * 0.55f);

    draw_box(world, (Vector3){6.8f, station_ground + 2.0f, 21.0f},
        (Vector3){3.6f, 4.0f, 0.48f}, 0.0f, plaster);
    draw_box(world, (Vector3){17.2f, station_ground + 2.0f, 21.0f},
        (Vector3){3.6f, 4.0f, 0.48f}, 0.0f, plaster);
    draw_box(world, (Vector3){12.0f, station_ground + 3.65f, 21.0f},
        (Vector3){7.0f, 0.7f, 0.48f}, 0.0f, plaster);
    draw_box(world, (Vector3){12.0f, station_ground + 2.0f, 33.0f},
        (Vector3){14.0f, 4.0f, 0.5f}, 0.0f, plaster);
    draw_box(world, (Vector3){5.0f, station_ground + 2.0f, 27.0f},
        (Vector3){0.5f, 4.0f, 12.0f}, 0.0f, plaster);
    draw_box(world, (Vector3){19.0f, station_ground + 2.0f, 27.0f},
        (Vector3){0.5f, 4.0f, 12.0f}, 0.0f, plaster);
    draw_box(world, (Vector3){12.0f, station_ground + 4.15f, 27.0f},
        (Vector3){15.0f, 0.55f, 13.0f}, 0.0f, roof);
    draw_box(world, (Vector3){12.0f, station_ground + 1.3f, 20.7f},
        (Vector3){1.7f, 2.6f, 0.2f}, 0.0f, (Color){54, 43, 37, 255});
    draw_box(world, (Vector3){12.0f, station_ground + 3.3f, 20.45f},
        (Vector3){5.5f, 0.6f, 0.2f}, 0.0f, roof);
    draw_box(world, (Vector3){11.8f, station_ground + 3.45f, 20.28f},
        (Vector3){4.4f, 0.16f, 0.07f}, 0.0f, (Color){195, 149, 91, 255});
    draw_box(world, (Vector3){-1.0f,
        game_world_height(-1.0f, 22.0f, shifted) + 0.13f, 22.0f},
        (Vector3){19.0f, 0.1f, 4.0f}, 0.0f, road);
    for (int i = 0; i < 4; i++) {
        float z = -21.0f + (float)i * 23.0f;
        float ground = game_world_height(5.6f, z, shifted);
        draw_box(world, (Vector3){5.6f, ground + 3.6f, z},
            (Vector3){0.16f, 7.2f, 0.16f}, 0.0f, (Color){80, 75, 67, 255});
        draw_box(world, (Vector3){5.6f, ground + 6.5f, z},
            (Vector3){2.6f, 0.14f, 0.14f}, 0.0f, (Color){71, 69, 66, 255});
    }

    draw_box(world, (Vector3){-12.0f,
        game_world_height(-12.0f, 53.0f, shifted) + 0.18f, 53.0f},
        (Vector3){24.0f, 0.12f, 5.0f}, -24.0f, road);
    draw_box(world, (Vector3){-25.0f,
        game_world_height(-25.0f, 63.0f, shifted) + 0.18f, 63.0f},
        (Vector3){23.0f, 0.12f, 5.0f}, -38.0f, road);

    Color village_wall = color_lerp((Color){105, 79, 64, 255},
        (Color){163, 122, 88, 255}, daylight * 0.62f);
    Color village_roof = color_lerp((Color){65, 57, 54, 255},
        (Color){105, 83, 64, 255}, daylight * 0.5f);
    draw_house(world, -37.0f, 76.0f, 12.0f, 10.0f, 4.2f, village_wall, village_roof);
    draw_house(world, -52.0f, 88.0f, 10.0f, 9.0f, 3.8f, village_wall, village_roof);
    draw_house(world, -22.0f, 91.0f, 11.0f, 10.0f, 4.0f, village_wall, village_roof);

    for (int i = 0; i < 5; i++) {
        float x = -11.0f + (float)i * 1.45f;
        float z = 70.0f + (float)(i % 2) * 1.2f;
        float ground = game_world_height(x, z, shifted);
        DrawCylinder((Vector3){x, ground + 1.4f, z}, 0.34f, 0.24f, 2.8f, 5,
            (Color){87, 75, 53, 255});
        DrawSphere((Vector3){x, ground + 3.05f, z}, 1.35f,
            (Color){62, 77, 51, 255});
    }

    world_draw_car(world, (Vector3){-8.0f,
        game_world_height(-8.0f, 2.0f, shifted), 2.0f}, 0.18f, 0);
    float pump_ground = game_world_height(7.0f, 17.5f, shifted);
    draw_box(world, (Vector3){7.0f, pump_ground + 1.0f, 17.5f},
        (Vector3){1.1f, 2.0f, 1.0f}, 0.0f, (Color){116, 83, 59, 255});
    draw_box(world, (Vector3){7.0f, pump_ground + 1.75f, 17.5f},
        (Vector3){0.65f, 0.35f, 0.72f}, 0.0f, (Color){172, 130, 91, 255});
    draw_box(world, (Vector3){-5.6f,
        game_world_height(-5.6f, 47.0f, shifted) + 1.8f, 47.0f},
        (Vector3){0.2f, 3.6f, 0.2f}, 0.0f,
        (Color){91, 77, 63, 255});
    draw_box(world, (Vector3){5.6f,
        game_world_height(5.6f, 47.0f, shifted) + 1.8f, 47.0f},
        (Vector3){0.2f, 3.6f, 0.2f}, 0.0f,
        (Color){91, 77, 63, 255});
    if (!gate_open) {
        draw_box(world, (Vector3){0.0f,
            game_world_height(0.0f, 47.0f, shifted) + 1.8f, 47.0f},
            (Vector3){11.4f, 0.25f, 0.2f}, 0.0f,
            (Color){91, 77, 63, 255});
    }

    if (show_figure && !figure_gone) {
        float figure_x = -38.0f;
        float figure_z = 105.0f;
        world_draw_figure(world, (Vector3){figure_x,
            game_world_height(figure_x, figure_z, shifted), figure_z}, daylight);
    }
}

void world_draw_station_interior(World *world, int storeroom_open, int flashlight_on)
{
    Color plaster = (Color){91, 76, 70, 255};
    Color dark_plaster = (Color){65, 57, 58, 255};
    Color floor = (Color){113, 89, 72, 255};
    draw_box(world, (Vector3){0.0f, -0.22f, 0.0f},
        (Vector3){12.0f, 0.35f, 14.0f}, 0.0f, floor);

    draw_box(world, (Vector3){-3.0f, 2.0f, -7.0f},
        (Vector3){6.0f, 4.0f, 0.35f}, 0.0f, plaster);
    draw_box(world, (Vector3){3.0f, 2.0f, -7.0f},
        (Vector3){6.0f, 4.0f, 0.35f}, 0.0f, plaster);
    draw_box(world, (Vector3){0.0f, 3.8f, -7.0f},
        (Vector3){1.0f, 0.4f, 0.35f}, 0.0f, plaster);

    draw_box(world, (Vector3){-3.0f, 2.0f, 6.8f},
        (Vector3){6.0f, 4.0f, 0.35f}, 0.0f, dark_plaster);
    draw_box(world, (Vector3){3.0f, 2.0f, 6.8f},
        (Vector3){6.0f, 4.0f, 0.35f}, 0.0f, dark_plaster);
    if (storeroom_open) {
        draw_box(world, (Vector3){0.0f, 2.0f, 6.8f},
            (Vector3){2.2f, 4.0f, 0.25f}, 0.0f, (Color){20, 20, 26, 255});
    } else {
        draw_box(world, (Vector3){0.0f, 2.0f, 6.8f},
            (Vector3){2.2f, 4.0f, 0.45f}, 0.0f, (Color){57, 45, 41, 255});
    }
    draw_box(world, (Vector3){-6.0f, 2.0f, 0.0f},
        (Vector3){0.35f, 4.0f, 14.0f}, 0.0f, plaster);
    draw_box(world, (Vector3){6.0f, 2.0f, 0.0f},
        (Vector3){0.35f, 4.0f, 14.0f}, 0.0f, plaster);
    draw_box(world, (Vector3){0.0f, 4.1f, 0.0f},
        (Vector3){12.0f, 0.25f, 14.0f}, 0.0f, (Color){49, 45, 48, 255});

    draw_box(world, (Vector3){-2.5f, 1.05f, 0.8f},
        (Vector3){4.2f, 0.45f, 1.3f}, 0.0f, (Color){72, 54, 45, 255});
    draw_box(world, (Vector3){-3.3f, 1.38f, 0.6f},
        (Vector3){0.8f, 0.12f, 0.62f}, 0.0f, (Color){36, 31, 32, 255});
    draw_box(world, (Vector3){3.9f, 0.7f, 4.5f},
        (Vector3){1.4f, 1.4f, 0.18f}, 0.0f, (Color){56, 49, 46, 255});
    draw_box(world, (Vector3){3.9f, 1.52f, 4.5f},
        (Vector3){0.8f, 0.35f, 0.12f}, 0.0f,
        storeroom_open ? (Color){82, 172, 111, 255} : (Color){177, 123, 65, 255});
    for (int i = 0; i < 3; i++) {
        draw_box(world, (Vector3){4.3f, 0.55f + (float)i * 0.58f, -2.8f},
            (Vector3){1.8f, 0.12f, 0.65f}, 0.0f, (Color){99, 76, 59, 255});
    }

    if (flashlight_on) {
        DrawSphere((Vector3){0.0f, 3.6f, 0.0f}, 0.22f, (Color){255, 215, 139, 255});
    }
}

void world_unload(World *world)
{
    if (world->terrain.vertices != NULL) UnloadMesh(world->terrain);
    UnloadMaterial(world->terrain_material);
    UnloadModel(world->cube);
    UnloadModel(world->sphere);
}
