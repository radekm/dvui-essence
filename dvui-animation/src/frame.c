// Data jednoho snímku: alokace v aréně okna a seznamy renderovacích příkazů.
// DVUI: Window.arena (Window.zig), RenderCommand (render.zig)
#include "internal.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Alokace v aréně snímku (aréna je ve shared/src/data.c)
// ---------------------------------------------------------------------------

void* dvui_frame_alloc(size_t size) { return dvui_arena_alloc(&dvui_current_window()->arena, size); }

void* dvui_frame_grow(void* items, size_t* cap, size_t elem_size, size_t needed) {
    if (needed <= *cap) return items;
    size_t ncap = *cap ? *cap * 2 : 8;
    while (ncap < needed) ncap *= 2;
    void* n = dvui_frame_alloc(ncap * elem_size);
    if (items && *cap) memcpy(n, items, *cap * elem_size);
    *cap = ncap;
    return n;
}

// ---------------------------------------------------------------------------
// Seznam renderovacích příkazů (v aréně snímku)
// ---------------------------------------------------------------------------

void dvui_render_cmd_list_append(dvui_RenderCmdList* list, dvui_RenderCommand cmd) {
    list->items = dvui_frame_grow(list->items, &list->cap, sizeof(dvui_RenderCommand), list->len + 1);
    list->items[list->len++] = cmd;
}

void dvui_render_cmd_list_insert_slice(dvui_RenderCmdList* list, size_t at, const dvui_RenderCommand* items, size_t n) {
    if (n == 0) return;
    list->items = dvui_frame_grow(list->items, &list->cap, sizeof(dvui_RenderCommand), list->len + n);
    memmove(list->items + at + n, list->items + at, (list->len - at) * sizeof(dvui_RenderCommand));
    memcpy(list->items + at, items, n * sizeof(dvui_RenderCommand));
    list->len += n;
}
