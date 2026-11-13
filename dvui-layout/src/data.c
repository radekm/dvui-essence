// Pomocné datové struktury: aréna snímku, hash mapa, seznamy příkazů.
// DVUI: Window.arena, tracking_hash_map.zig, Data.zig
#include "internal.h"

#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Aréna
// ---------------------------------------------------------------------------

static size_t align_up(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }

void* dvui_arena_alloc(dvui_Arena* a, size_t size) {
    size = align_up(size ? size : 1, 16);
    const size_t header = align_up(sizeof(dvui_ArenaChunk), 16);
    // najdi chunk s dostatkem místa (po resetu se chunky znovu používají)
    dvui_ArenaChunk* c = a->head;
    while (c && c->used + size > c->cap) c = c->next;
    if (!c) {
        size_t cap = 64 * 1024;
        while (cap < size) cap *= 2;
        c = malloc(header + cap);
        if (!c) {
            DVUI_LOG_ERR("out of memory");
            abort();
        }
        c->cap = cap;
        c->used = 0;
        c->next = a->head;
        a->head = c;
    }
    void* p = (char*)c + header + c->used;
    c->used += size;
    return p;
}

void dvui_arena_reset(dvui_Arena* a) {
    for (dvui_ArenaChunk* c = a->head; c; c = c->next) c->used = 0;
}

void dvui_arena_free_all(dvui_Arena* a) {
    dvui_ArenaChunk* c = a->head;
    while (c) {
        dvui_ArenaChunk* n = c->next;
        free(c);
        c = n;
    }
    a->head = nullptr;
}

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
// Hash mapa (otevřené adresování, lineární sondování)
// ---------------------------------------------------------------------------

static uint64_t mix(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    return k;
}

static void map_rehash(dvui_Map* m, size_t ncap) {
    dvui_MapEntry* old = m->entries;
    const size_t ocap = m->cap;
    m->entries = calloc(ncap, sizeof(dvui_MapEntry));
    if (!m->entries) {
        DVUI_LOG_ERR("out of memory");
        abort();
    }
    m->cap = ncap;
    m->count = 0;
    m->tombstones = 0;
    for (size_t i = 0; i < ocap; i++) {
        if (old[i].state != 1) continue;
        size_t j = mix(old[i].key) & (ncap - 1);
        while (m->entries[j].state == 1) j = (j + 1) & (ncap - 1);
        m->entries[j] = old[i];
        m->count++;
    }
    free(old);
}

dvui_MapEntry* dvui_map_get(dvui_Map* m, uint64_t key) {
    if (!m->cap) return nullptr;
    size_t j = mix(key) & (m->cap - 1);
    for (;;) {
        dvui_MapEntry* e = &m->entries[j];
        if (e->state == 0) return nullptr;
        if (e->state == 1 && e->key == key) return e;
        j = (j + 1) & (m->cap - 1);
    }
}

dvui_MapEntry* dvui_map_get_or_put(dvui_Map* m, uint64_t key) {
    dvui_MapEntry* e = dvui_map_get(m, key);
    if (e) return e;
    if ((m->count + m->tombstones + 1) * 4 >= m->cap * 3) {
        map_rehash(m, m->cap ? (m->count * 4 >= m->cap ? m->cap * 2 : m->cap) : 64);
    }
    size_t j = mix(key) & (m->cap - 1);
    while (m->entries[j].state == 1) j = (j + 1) & (m->cap - 1);
    if (m->entries[j].state == 2) m->tombstones--;
    e = &m->entries[j];
    *e = (dvui_MapEntry){.key = key, .state = 1};
    m->count++;
    return e;
}

void dvui_map_remove(dvui_Map* m, uint64_t key) {
    dvui_MapEntry* e = dvui_map_get(m, key);
    if (!e) return;
    free(e->ptr);
    *e = (dvui_MapEntry){.state = 2};
    m->count--;
    m->tombstones++;
}

void dvui_map_free(dvui_Map* m) {
    for (size_t i = 0; i < m->cap; i++) {
        if (m->entries[i].state == 1) free(m->entries[i].ptr);
    }
    free(m->entries);
    *m = (dvui_Map){0};
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
