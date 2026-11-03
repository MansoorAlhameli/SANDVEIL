#ifndef SANDVEIL_PSX_RENDERER_H
#define SANDVEIL_PSX_RENDERER_H

#include <stdint.h>

#define PSX_Q12_ONE 4096
#define PSX_OT_BUCKETS 256
#define PSX_MAX_TRIANGLES 512
#define PSX_SCREEN_WIDTH 320
#define PSX_SCREEN_HEIGHT 224

typedef struct PsxVec3 {
    int32_t x;
    int32_t y;
    int32_t z;
} PsxVec3;

typedef struct PsxColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} PsxColor;

typedef struct PsxVertex {
    PsxVec3 view_position;
    PsxVec3 normal;
    PsxColor color;
} PsxVertex;

typedef struct PsxProjectedVertex {
    int16_t x;
    int16_t y;
    int32_t depth_q12;
    PsxColor color;
} PsxProjectedVertex;

typedef struct PsxTriangle {
    PsxProjectedVertex vertices[3];
    int32_t average_depth_q12;
    uint16_t source_id;
    uint8_t ot_bucket;
} PsxTriangle;

typedef struct PsxRenderConfig {
    int32_t focal_length_pixels;
    int32_t near_clip_q12;
    int32_t far_clip_q12;
    int32_t fog_start_q12;
    PsxColor fog_color;
    PsxVec3 light_direction_q12;
    uint8_t ambient_percent;
    uint8_t flashlight_percent;
    uint8_t flashlight_radius_world;
} PsxRenderConfig;

typedef struct PsxOrderingTable {
    PsxTriangle triangles[PSX_MAX_TRIANGLES];
    uint16_t bucket_counts[PSX_OT_BUCKETS];
    uint16_t bucket_heads[PSX_OT_BUCKETS];
    uint16_t bucket_tails[PSX_OT_BUCKETS];
    uint16_t triangle_next[PSX_MAX_TRIANGLES];
    uint16_t triangle_count;
} PsxOrderingTable;

void psx_ot_clear(PsxOrderingTable *ot);
int psx_project_vertex(const PsxRenderConfig *config, const PsxVertex *vertex,
    PsxProjectedVertex *projected);
PsxColor psx_shade_vertex(const PsxRenderConfig *config, PsxVec3 position,
    PsxVec3 normal, PsxColor albedo);
int psx_ot_submit_triangle(PsxOrderingTable *ot, const PsxRenderConfig *config,
    const PsxVertex vertices[3], uint16_t source_id);
uint16_t psx_ot_sorted_indices(const PsxOrderingTable *ot,
    uint16_t *indices, uint16_t capacity);

#endif
