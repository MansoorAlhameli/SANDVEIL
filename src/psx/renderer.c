#include "renderer.h"

#include <limits.h>

static int32_t clamp_i32(int32_t value, int32_t low, int32_t high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static uint8_t clamp_color(int32_t value)
{
    return (uint8_t)clamp_i32(value, 0, 255);
}

static int32_t dot_q12(PsxVec3 a, PsxVec3 b)
{
    int64_t dot = (int64_t)a.x * b.x + (int64_t)a.y * b.y
        + (int64_t)a.z * b.z;
    return (int32_t)(dot >> 12);
}

static uint32_t integer_sqrt(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = (uint64_t)1 << 62;
    while (bit > value) bit >>= 2;
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)result;
}

static uint32_t length_q12(PsxVec3 value)
{
    uint64_t squared = (uint64_t)((int64_t)value.x * value.x)
        + (uint64_t)((int64_t)value.y * value.y)
        + (uint64_t)((int64_t)value.z * value.z);
    return integer_sqrt(squared);
}

static PsxVec3 normalize_q12(PsxVec3 value)
{
    uint32_t length = length_q12(value);
    if (length == 0) return (PsxVec3){0, 0, 0};
    return (PsxVec3){
        (int32_t)(((int64_t)value.x * PSX_Q12_ONE) / length),
        (int32_t)(((int64_t)value.y * PSX_Q12_ONE) / length),
        (int32_t)(((int64_t)value.z * PSX_Q12_ONE) / length)
    };
}

static uint8_t shade_channel(uint8_t channel, int32_t illumination_percent)
{
    return clamp_color(((int32_t)channel * illumination_percent + 50) / 100);
}

PsxColor psx_shade_vertex(const PsxRenderConfig *config, PsxVec3 position,
    PsxVec3 normal, PsxColor albedo)
{
    PsxVec3 toward_light = normalize_q12((PsxVec3){
        -position.x, -position.y, -position.z
    });
    int32_t cone = dot_q12(normalize_q12(config->light_direction_q12),
        normalize_q12(position));
    if (cone < 0) cone = 0;
    int32_t diffuse = dot_q12(normalize_q12(normal), toward_light);
    if (diffuse < 0) diffuse = 0;

    int32_t radius_q12 = (int32_t)config->flashlight_radius_world * PSX_Q12_ONE;
    int32_t distance = (int32_t)length_q12(position);
    int32_t attenuation = 0;
    if (radius_q12 > 0 && distance < radius_q12) {
        attenuation = ((radius_q12 - distance) * 100) / radius_q12;
    }

    int32_t flashlight = (int32_t)(((int64_t)config->flashlight_percent
        * cone * diffuse * attenuation) / ((int64_t)PSX_Q12_ONE * PSX_Q12_ONE * 100));
    int32_t illumination = (int32_t)config->ambient_percent + flashlight;
    return (PsxColor){
        shade_channel(albedo.r, illumination),
        shade_channel(albedo.g, illumination),
        shade_channel(albedo.b, illumination)
    };
}

static PsxColor apply_fog(const PsxRenderConfig *config, PsxColor color,
    int32_t depth)
{
    if (config->far_clip_q12 <= config->fog_start_q12
        || depth <= config->fog_start_q12) return color;

    int32_t fog = (int32_t)(((int64_t)(depth - config->fog_start_q12) * 255)
        / (config->far_clip_q12 - config->fog_start_q12));
    fog = clamp_i32(fog, 0, 255);
    return (PsxColor){
        (uint8_t)((color.r * (255 - fog) + config->fog_color.r * fog + 127) / 255),
        (uint8_t)((color.g * (255 - fog) + config->fog_color.g * fog + 127) / 255),
        (uint8_t)((color.b * (255 - fog) + config->fog_color.b * fog + 127) / 255)
    };
}

void psx_ot_clear(PsxOrderingTable *ot)
{
    ot->triangle_count = 0;
    for (int i = 0; i < PSX_OT_BUCKETS; i++) {
        ot->bucket_counts[i] = 0;
        ot->bucket_heads[i] = UINT16_MAX;
        ot->bucket_tails[i] = UINT16_MAX;
    }
}

int psx_project_vertex(const PsxRenderConfig *config, const PsxVertex *vertex,
    PsxProjectedVertex *projected)
{
    int32_t depth = vertex->view_position.z;
    if (depth < config->near_clip_q12 || depth > config->far_clip_q12
        || depth <= 0 || config->focal_length_pixels <= 0) return 0;

    int64_t x = (int64_t)vertex->view_position.x * config->focal_length_pixels / depth;
    int64_t y = (int64_t)vertex->view_position.y * config->focal_length_pixels / depth;
    x += PSX_SCREEN_WIDTH / 2;
    y = PSX_SCREEN_HEIGHT / 2 - y;
    if (x < INT16_MIN || x > INT16_MAX || y < INT16_MIN || y > INT16_MAX) return 0;

    PsxColor lit = psx_shade_vertex(config, vertex->view_position,
        vertex->normal, vertex->color);
    projected->x = (int16_t)x;
    projected->y = (int16_t)y;
    projected->depth_q12 = depth;
    projected->color = apply_fog(config, lit, depth);
    return 1;
}

int psx_ot_submit_triangle(PsxOrderingTable *ot, const PsxRenderConfig *config,
    const PsxVertex vertices[3], uint16_t source_id)
{
    if (ot->triangle_count >= PSX_MAX_TRIANGLES) return 0;
    PsxTriangle triangle = {0};
    int64_t depth_sum = 0;
    for (int i = 0; i < 3; i++) {
        if (!psx_project_vertex(config, &vertices[i], &triangle.vertices[i])) return 0;
        depth_sum += vertices[i].view_position.z;
    }

    int all_left = 1;
    int all_right = 1;
    int all_above = 1;
    int all_below = 1;
    for (int i = 0; i < 3; i++) {
        all_left &= triangle.vertices[i].x < 0;
        all_right &= triangle.vertices[i].x >= PSX_SCREEN_WIDTH;
        all_above &= triangle.vertices[i].y < 0;
        all_below &= triangle.vertices[i].y >= PSX_SCREEN_HEIGHT;
    }
    if (all_left || all_right || all_above || all_below) return 0;

    triangle.average_depth_q12 = (int32_t)(depth_sum / 3);
    triangle.source_id = source_id;
    triangle.ot_bucket = (uint8_t)(((int64_t)triangle.average_depth_q12
        * PSX_OT_BUCKETS) / config->far_clip_q12);

    uint16_t index = ot->triangle_count++;
    ot->triangles[index] = triangle;
    uint8_t bucket = triangle.ot_bucket;
    ot->triangle_next[index] = UINT16_MAX;
    if (ot->bucket_tails[bucket] == UINT16_MAX) {
        ot->bucket_heads[bucket] = index;
    } else {
        ot->triangle_next[ot->bucket_tails[bucket]] = index;
    }
    ot->bucket_tails[bucket] = index;
    ot->bucket_counts[bucket]++;
    return 1;
}

uint16_t psx_ot_sorted_indices(const PsxOrderingTable *ot,
    uint16_t *indices, uint16_t capacity)
{
    uint16_t count = 0;
    for (int bucket = PSX_OT_BUCKETS - 1; bucket >= 0; bucket--) {
        uint16_t triangle = ot->bucket_heads[bucket];
        while (triangle != UINT16_MAX && count < capacity) {
            indices[count++] = triangle;
            triangle = ot->triangle_next[triangle];
        }
    }
    return count;
}
