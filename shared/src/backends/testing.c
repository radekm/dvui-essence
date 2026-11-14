// Testovací backend bez GPU.
// DVUI: src/backends/testing.zig
#include "dvui/backend_testing.h"

#include <stdlib.h>
#include <string.h>

static void tb_begin(void* ctx) {
    dvui_TestingBackend* tb = ctx;
    tb->draws_len = 0;
}

static void tb_end(void* ctx) { (void)ctx; }

static dvui_SizePhysical tb_pixel_size(void* ctx) { return ((dvui_TestingBackend*)ctx)->pixel_size; }
static dvui_SizeNatural tb_window_size(void* ctx) { return ((dvui_TestingBackend*)ctx)->window_size; }
static float tb_content_scale(void* ctx) { return ((dvui_TestingBackend*)ctx)->content_scale; }

static void tb_draw(void* ctx, const dvui_Vertex* vtx, size_t vtx_count, const uint32_t* idx, size_t idx_count, const dvui_RectPhysical* clip) {
    (void)idx;
    (void)idx_count;
    dvui_TestingBackend* tb = ctx;
    if (vtx_count == 0) return;
    float x0 = vtx[0].pos.x, y0 = vtx[0].pos.y, x1 = x0, y1 = y0;
    for (size_t i = 1; i < vtx_count; i++) {
        if (vtx[i].pos.x < x0) x0 = vtx[i].pos.x;
        if (vtx[i].pos.y < y0) y0 = vtx[i].pos.y;
        if (vtx[i].pos.x > x1) x1 = vtx[i].pos.x;
        if (vtx[i].pos.y > y1) y1 = vtx[i].pos.y;
    }
    if (tb->draws_len == tb->draws_cap) {
        tb->draws_cap = tb->draws_cap ? tb->draws_cap * 2 : 64;
        tb->draws = realloc(tb->draws, tb->draws_cap * sizeof(dvui_TestingDraw));
    }
    tb->draws[tb->draws_len++] = (dvui_TestingDraw){
        .rect = {x0, y0, x1 - x0, y1 - y0},
        .color = vtx[0].col,
        .clipped = clip != nullptr,
        .clip = clip ? *clip : (dvui_RectPhysical){0},
    };
}

static void tb_set_cursor(void* ctx, dvui_Cursor cursor) { ((dvui_TestingBackend*)ctx)->cursor = cursor; }
static void tb_render_present(void* ctx) { (void)ctx; }
static int64_t tb_nano_time(void* ctx) { return ((dvui_TestingBackend*)ctx)->time_ns; }
// DVUI: testing.zig – sleep (nic nedělá; čas posouvá test)
static void tb_sleep(void* ctx, uint64_t ns) {
    (void)ctx;
    (void)ns;
}

void dvui_testing_backend_init(dvui_TestingBackend* tb, dvui_Size size) {
    *tb = (dvui_TestingBackend){.pixel_size = size, .window_size = size, .content_scale = 1.0f};
}

void dvui_testing_backend_deinit(dvui_TestingBackend* tb) {
    free(tb->draws);
    tb->draws = nullptr;
}

dvui_Backend dvui_testing_backend(dvui_TestingBackend* tb) {
    return (dvui_Backend){
        .ctx = tb,
        .begin = tb_begin,
        .end = tb_end,
        .pixel_size = tb_pixel_size,
        .window_size = tb_window_size,
        .content_scale = tb_content_scale,
        .draw_clipped_triangles = tb_draw,
        .set_cursor = tb_set_cursor,
        .render_present = tb_render_present,
        .nano_time = tb_nano_time,
        .sleep = tb_sleep,
    };
}
