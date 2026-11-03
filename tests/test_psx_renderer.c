#include "psx/renderer.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

static PsxRenderConfig test_config(void)
{
    return (PsxRenderConfig){
        .focal_length_pixels = 120,
        .near_clip_q12 = PSX_Q12_ONE,
        .far_clip_q12 = 80 * PSX_Q12_ONE,
        .fog_start_q12 = 30 * PSX_Q12_ONE,
        .fog_color = {128, 128, 128},
        .light_direction_q12 = {0, 0, PSX_Q12_ONE},
        .ambient_percent = 24,
        .flashlight_percent = 76,
        .flashlight_radius_world = 20
    };
}

static PsxVertex vertex_at(int32_t x, int32_t y, int32_t z, PsxColor color)
{
    return (PsxVertex){
        .view_position = {x, y, z},
        .normal = {0, 0, -PSX_Q12_ONE},
        .color = color
    };
}

static void test_integer_projection_and_pixel_snapping(void)
{
    PsxRenderConfig config = test_config();
    PsxVertex vertex = vertex_at(PSX_Q12_ONE, -PSX_Q12_ONE,
        10 * PSX_Q12_ONE, (PsxColor){220, 160, 100});
    PsxProjectedVertex projected;
    assert(psx_project_vertex(&config, &vertex, &projected));
    assert(projected.x == 172);
    assert(projected.y == 124);
    assert(projected.depth_q12 == 10 * PSX_Q12_ONE);
}

static void test_near_and_far_clipping(void)
{
    PsxRenderConfig config = test_config();
    PsxProjectedVertex projected;
    PsxVertex near_vertex = vertex_at(0, 0, PSX_Q12_ONE / 2, (PsxColor){255, 255, 255});
    PsxVertex far_vertex = vertex_at(0, 0, 81 * PSX_Q12_ONE, (PsxColor){255, 255, 255});
    assert(!psx_project_vertex(&config, &near_vertex, &projected));
    assert(!psx_project_vertex(&config, &far_vertex, &projected));
}

static void test_depth_cue_fog(void)
{
    PsxRenderConfig config = test_config();
    PsxProjectedVertex projected;
    PsxVertex far = vertex_at(0, 0, config.far_clip_q12, (PsxColor){240, 120, 20});
    assert(psx_project_vertex(&config, &far, &projected));
    assert(projected.color.r == config.fog_color.r);
    assert(projected.color.g == config.fog_color.g);
    assert(projected.color.b == config.fog_color.b);
}

static void test_ordering_table_is_back_to_front(void)
{
    PsxRenderConfig config = test_config();
    PsxOrderingTable ot;
    PsxVertex triangles[3][3] = {
        {
            vertex_at(-PSX_Q12_ONE, -PSX_Q12_ONE, 12 * PSX_Q12_ONE, (PsxColor){200, 170, 140}),
            vertex_at(PSX_Q12_ONE, -PSX_Q12_ONE, 12 * PSX_Q12_ONE, (PsxColor){200, 170, 140}),
            vertex_at(0, PSX_Q12_ONE, 12 * PSX_Q12_ONE, (PsxColor){200, 170, 140})
        },
        {
            vertex_at(-PSX_Q12_ONE, -PSX_Q12_ONE, 60 * PSX_Q12_ONE, (PsxColor){200, 170, 140}),
            vertex_at(PSX_Q12_ONE, -PSX_Q12_ONE, 60 * PSX_Q12_ONE, (PsxColor){200, 170, 140}),
            vertex_at(0, PSX_Q12_ONE, 60 * PSX_Q12_ONE, (PsxColor){200, 170, 140})
        },
        {
            vertex_at(-PSX_Q12_ONE, -PSX_Q12_ONE, 35 * PSX_Q12_ONE, (PsxColor){200, 170, 140}),
            vertex_at(PSX_Q12_ONE, -PSX_Q12_ONE, 35 * PSX_Q12_ONE, (PsxColor){200, 170, 140}),
            vertex_at(0, PSX_Q12_ONE, 35 * PSX_Q12_ONE, (PsxColor){200, 170, 140})
        }
    };
    uint16_t indices[3];
    psx_ot_clear(&ot);
    for (uint16_t i = 0; i < 3; i++) {
        assert(psx_ot_submit_triangle(&ot, &config, triangles[i], i));
    }
    assert(psx_ot_sorted_indices(&ot, indices, 3) == 3);
    assert(ot.triangles[indices[0]].source_id == 1);
    assert(ot.triangles[indices[1]].source_id == 2);
    assert(ot.triangles[indices[2]].source_id == 0);
}

static void test_triangle_rejected_if_vertex_outside_far_clip(void)
{
    PsxRenderConfig config = test_config();
    PsxOrderingTable ot;
    PsxVertex triangle[3] = {
        vertex_at(-PSX_Q12_ONE, -PSX_Q12_ONE, 10 * PSX_Q12_ONE, (PsxColor){200, 170, 140}),
        vertex_at(PSX_Q12_ONE, -PSX_Q12_ONE, 10 * PSX_Q12_ONE, (PsxColor){200, 170, 140}),
        vertex_at(0, PSX_Q12_ONE, 81 * PSX_Q12_ONE, (PsxColor){200, 170, 140})
    };
    psx_ot_clear(&ot);
    assert(!psx_ot_submit_triangle(&ot, &config, triangle, 7));
    assert(ot.triangle_count == 0);
}

static void test_triangle_outside_screen_is_culled(void)
{
    PsxRenderConfig config = test_config();
    PsxOrderingTable ot;
    PsxVertex triangle[3] = {
        vertex_at(100 * PSX_Q12_ONE, 0, 10 * PSX_Q12_ONE, (PsxColor){200, 170, 140}),
        vertex_at(101 * PSX_Q12_ONE, PSX_Q12_ONE, 10 * PSX_Q12_ONE, (PsxColor){200, 170, 140}),
        vertex_at(102 * PSX_Q12_ONE, -PSX_Q12_ONE, 10 * PSX_Q12_ONE, (PsxColor){200, 170, 140})
    };
    psx_ot_clear(&ot);
    assert(!psx_ot_submit_triangle(&ot, &config, triangle, 8));
    assert(ot.triangle_count == 0);
}

static void test_ordering_table_memory_budget(void)
{
    assert(sizeof(PsxOrderingTable) < 64 * 1024);
}

int main(void)
{
    test_integer_projection_and_pixel_snapping();
    test_near_and_far_clipping();
    test_depth_cue_fog();
    test_ordering_table_is_back_to_front();
    test_triangle_rejected_if_vertex_outside_far_clip();
    test_triangle_outside_screen_is_culled();
    test_ordering_table_memory_budget();
    puts("All PS1 renderer-core tests passed.");
    return 0;
}
