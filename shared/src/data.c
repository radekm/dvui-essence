// Aréna, hash mapa a datové úložiště (sdílené všemi projekty).
// DVUI: Window.arena, tracking_hash_map.zig, Data.zig
#include "dvui/data.h"
#include "dvui/util.h"

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

// DVUI: tracking_hash_map.zig – TrackingAutoHashMap.reset
void dvui_map_reset(dvui_Map* m) {
    for (size_t i = 0; i < m->cap; i++) {
        dvui_MapEntry* e = &m->entries[i];
        if (e->state != 1) continue;
        if (e->used) {
            e->used = false;
        } else {
            dvui_map_remove(m, e->key);
        }
    }
}

void dvui_map_free(dvui_Map* m) {
    for (size_t i = 0; i < m->cap; i++) {
        if (m->entries[i].state == 1) free(m->entries[i].ptr);
    }
    free(m->entries);
    *m = (dvui_Map){0};
}

// ---------------------------------------------------------------------------
// Datové úložiště (DVUI: Data.zig)
// ---------------------------------------------------------------------------

static void data_trash(dvui_DataStore* ds, void* ptr) {
    if (!ptr) return;
    if (ds->trash_len == ds->trash_cap) {
        ds->trash_cap = ds->trash_cap ? ds->trash_cap * 2 : 16;
        ds->trash = realloc(ds->trash, ds->trash_cap * sizeof(void*));
        if (!ds->trash) {
            DVUI_LOG_ERR("out of memory");
            abort();
        }
    }
    ds->trash[ds->trash_len++] = ptr;
}

// DVUI: Data.zig – set (getOrPut s replace_existing)
void dvui_data_store_set(dvui_DataStore* ds, uint64_t key, const void* value, size_t size) {
    dvui_MapEntry* e = dvui_map_get_or_put(&ds->storage, key);
    e->used = true;
    if (e->ptr && e->size == size) {
        // stejná velikost: přepíšeme na místě, ukazatele zůstanou platné
        if (e->ptr != value && size) memmove(e->ptr, value, size);
        return;
    }
    // jiná velikost: starý blob uvolníme až na začátku dalšího snímku
    data_trash(ds, e->ptr);
    e->ptr = malloc(size ? size : 1);
    if (!e->ptr) {
        DVUI_LOG_ERR("out of memory");
        abort();
    }
    e->size = size;
    if (size) memcpy(e->ptr, value, size);
}

// DVUI: Data.zig – getPtr
void* dvui_data_store_get(dvui_DataStore* ds, uint64_t key, size_t size, const char* name) {
    dvui_MapEntry* e = dvui_map_get(&ds->storage, key);
    if (!e) return nullptr;
    e->used = true;
    if (e->size != size) {
        DVUI_LOG_ERR("dataGet key \"%s\" has size %zu, asked for %zu", name ? name : "?", e->size, size);
        return nullptr;
    }
    return e->ptr;
}

// DVUI: Data.zig – getPtrDefault
void* dvui_data_store_get_ptr_default(dvui_DataStore* ds, uint64_t key, size_t size, const void* def, const char* name) {
    void* p = dvui_data_store_get(ds, key, size, name);
    if (p) return p;
    dvui_data_store_set(ds, key, def, size);
    return dvui_data_store_get(ds, key, size, name);
}

// DVUI: Data.zig – getSlice
void* dvui_data_store_get_slice(dvui_DataStore* ds, uint64_t key, size_t* size) {
    dvui_MapEntry* e = dvui_map_get(&ds->storage, key);
    if (!e) {
        *size = 0;
        return nullptr;
    }
    e->used = true;
    *size = e->size;
    return e->ptr;
}

// DVUI: Data.zig – remove
void dvui_data_store_remove(dvui_DataStore* ds, uint64_t key) {
    dvui_MapEntry* e = dvui_map_get(&ds->storage, key);
    if (!e) return;
    // blob může někdo ještě tento snímek používat – jen do koše
    data_trash(ds, e->ptr);
    e->ptr = nullptr;
    dvui_map_remove(&ds->storage, key);
}

// DVUI: Data.zig – reset
void dvui_data_store_reset(dvui_DataStore* ds) {
    dvui_map_reset(&ds->storage);
    for (size_t i = 0; i < ds->trash_len; i++) free(ds->trash[i]);
    ds->trash_len = 0;
}

// DVUI: Data.zig – deinit
void dvui_data_store_deinit(dvui_DataStore* ds) {
    dvui_map_free(&ds->storage);
    for (size_t i = 0; i < ds->trash_len; i++) free(ds->trash[i]);
    free(ds->trash);
    *ds = (dvui_DataStore){0};
}
