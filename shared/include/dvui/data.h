// Aréna, hashovací mapa a datové úložiště sdílené všemi projekty.
// DVUI: Window.arena, tracking_hash_map.zig, Data.zig
//
// Není to veřejné API pro uživatele knihovny – používá ho okno projektu.
// Okno obsahuje `dvui_DataStore` a jeho `dvui_data_*` funkce (dvui.dataGet,
// dvui.dataSet, …) z id a jména spočítají klíč (`dvui_id_update`) a zavolají
// funkce `dvui_data_store_*`.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Aréna (DVUI: Window.arena)
// ---------------------------------------------------------------------------

typedef struct dvui_ArenaChunk dvui_ArenaChunk;
struct dvui_ArenaChunk {
    dvui_ArenaChunk* next;
    size_t cap;
    size_t used;
    // za hlavičkou následují data
};

typedef struct dvui_Arena {
    dvui_ArenaChunk* head;
} dvui_Arena;

void* dvui_arena_alloc(dvui_Arena* a, size_t size);
void dvui_arena_reset(dvui_Arena* a);
void dvui_arena_free_all(dvui_Arena* a);

// ---------------------------------------------------------------------------
// Hash mapa u64 → blob se značkou „použito“ (DVUI: TrackingAutoHashMap)
// ---------------------------------------------------------------------------

typedef struct dvui_MapEntry {
    uint64_t key;
    uint8_t state; // 0 = prázdné, 1 = obsazené, 2 = smazané
    bool used;
    void* ptr;
    size_t size;
} dvui_MapEntry;

typedef struct dvui_Map {
    dvui_MapEntry* entries;
    size_t cap;
    size_t count;
    size_t tombstones;
} dvui_Map;

dvui_MapEntry* dvui_map_get(dvui_Map* m, uint64_t key);
// Vrátí položku pro klíč, případně vytvoří prázdnou (ptr = NULL, size = 0).
dvui_MapEntry* dvui_map_get_or_put(dvui_Map* m, uint64_t key);
// Smaže položku a uvolní její blob (free).
void dvui_map_remove(dvui_Map* m, uint64_t key);
// DVUI: TrackingAutoHashMap.reset – smaže položky, které od minulého resetu
// nikdo nepoužil (`used == false`), ostatním značku `used` vynuluje.
void dvui_map_reset(dvui_Map* m);
void dvui_map_free(dvui_Map* m);

// ---------------------------------------------------------------------------
// Datové úložiště (DVUI: Data.zig)
// ---------------------------------------------------------------------------

typedef struct dvui_DataStore {
    // DVUI: Data.storage – klíč je hash id widgetu a jména (dvui_id_update).
    dvui_Map storage;
    // DVUI: Data.trash – bloby nahrazené nebo smazané během snímku; uvolní
    // se až v dvui_data_store_reset (někdo je může tento snímek ještě číst).
    void** trash;
    size_t trash_len, trash_cap;
} dvui_DataStore;

// DVUI: Data.set – uloží kopii `size` bajtů. Při stejné velikosti přepíše
// blob na místě (ukazatele zůstanou platné), jinak starý blob dá do koše.
void dvui_data_store_set(dvui_DataStore* ds, uint64_t key, const void* value, size_t size);
// DVUI: Data.getPtr – ukazatel na uloženou hodnotu, nebo NULL. Při jiné
// velikosti zahlásí chybu (`name` je jen pro hlášku) a vrátí NULL.
void* dvui_data_store_get(dvui_DataStore* ds, uint64_t key, size_t size, const char* name);
// DVUI: Data.getPtrDefault
void* dvui_data_store_get_ptr_default(dvui_DataStore* ds, uint64_t key, size_t size, const void* def, const char* name);
// DVUI: Data.getSlice – pole proměnné délky; `*size` je délka v bajtech.
void* dvui_data_store_get_slice(dvui_DataStore* ds, uint64_t key, size_t* size);
// DVUI: Data.remove – blob jde do koše.
void dvui_data_store_remove(dvui_DataStore* ds, uint64_t key);
// DVUI: Data.reset – zahodí data nepoužitá od minulého resetu a vysype koš.
void dvui_data_store_reset(dvui_DataStore* ds);
// DVUI: Data.deinit
void dvui_data_store_deinit(dvui_DataStore* ds);

#ifdef __cplusplus
}
#endif
