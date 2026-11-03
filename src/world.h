#ifndef SANDVEIL_WORLD_H
#define SANDVEIL_WORLD_H

#include "raylib.h"

typedef struct World {
    Mesh terrain;
    Material terrain_material;
    Model cube;
    Model sphere;
} World;

void world_init(World *world, int shifted);
void world_rebuild_terrain(World *world, int shifted);
void world_draw(World *world, Vector3 focus, float yaw, float daylight,
    int storm_active, float storm_amount);
void world_draw_map(World *world, int shifted, float daylight,
    int show_figure, int figure_gone, int gate_open);
void world_draw_station_interior(World *world, int storeroom_open, int flashlight_on);
void world_draw_car(World *world, Vector3 position, float yaw, int abandoned);
void world_draw_salem(World *world, Vector3 position, float yaw, int flashlight_on);
void world_draw_figure(World *world, Vector3 position, float daylight);
void world_draw_camp(World *world, int shifted, float daylight);
void world_unload(World *world);

#endif
