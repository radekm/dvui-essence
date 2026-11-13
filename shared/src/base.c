// Id a pomocné funkce pro vstup.
// DVUI: dvui.zig (Id), enums.zig
#include "dvui/base.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Id (DVUI: dvui.zig – Id.extendId, Id.update; FNV-1a 64)
// ---------------------------------------------------------------------------

static const uint64_t fnv_offset = 0xcbf29ce484222325ULL;
static const uint64_t fnv_prime = 0x100000001b3ULL;

static uint64_t fnv_update(uint64_t h, const void* data, size_t len) {
    const uint8_t* p = data;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= fnv_prime;
    }
    return h;
}

dvui_Id dvui_id_extend(dvui_Id start, dvui_Src src, size_t id_extra) {
    uint64_t h = start ? start : fnv_offset;
    // DVUI hashuje ukazatel na jméno souboru, řádek a sloupec; zde adresu
    // literálu, který je pro každé místo ve zdrojáku jiný (viz DVUI_SRC).
    const uintptr_t anchor = (uintptr_t)src.anchor;
    h = fnv_update(h, &anchor, sizeof anchor);
    const uint64_t extra = id_extra;
    h = fnv_update(h, &extra, sizeof extra);
    return h;
}

dvui_Id dvui_id_update(dvui_Id id, const char* name) {
    return fnv_update(id, name, strlen(name));
}

// ---------------------------------------------------------------------------
// Vstup – pomocné funkce (DVUI: enums.zig)
// ---------------------------------------------------------------------------

bool dvui_button_pointer(dvui_Button b) { return b == DVUI_BUTTON_LEFT; }

bool dvui_mod_shift(unsigned mod) { return (mod & (DVUI_MOD_LSHIFT | DVUI_MOD_RSHIFT)) != 0; }

bool dvui_mod_shift_only(unsigned mod) {
    return mod != 0 && (mod & ~(unsigned)(DVUI_MOD_LSHIFT | DVUI_MOD_RSHIFT)) == 0;
}
