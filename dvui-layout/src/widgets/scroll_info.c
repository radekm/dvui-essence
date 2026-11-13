// Informace o scrollování (virtuální velikost, viewport).
// DVUI: src/ScrollInfo.zig
#include "../internal.h"

bool dvui_scroll_bar_mode_auto_any(dvui_ScrollBarMode m) { return m == DVUI_SCROLL_BAR_AUTO || m == DVUI_SCROLL_BAR_AUTO_OVERLAY; }

dvui_ScrollInfo dvui_scroll_info_default(void) { return (dvui_ScrollInfo){.vertical = DVUI_SCROLL_AUTO, .horizontal = DVUI_SCROLL_NONE}; }

// Maximální posun – při něm viewport ukazuje konec obsahu.
float dvui_scroll_info_scroll_max(const dvui_ScrollInfo *si, dvui_Direction dir) {
    if (dir == DVUI_VERTICAL) return DVUI_MAX(0.0f, si->virtual_size.h - si->viewport.h);
    return DVUI_MAX(0.0f, si->virtual_size.w - si->viewport.w);
}

static float viewport_start(const dvui_ScrollInfo *si, dvui_Direction dir) { return dir == DVUI_VERTICAL ? si->viewport.y : si->viewport.x; }
static float viewport_size(const dvui_ScrollInfo *si, dvui_Direction dir) { return dir == DVUI_VERTICAL ? si->viewport.h : si->viewport.w; }
static float virtual_size(const dvui_ScrollInfo *si, dvui_Direction dir) { return dir == DVUI_VERTICAL ? si->virtual_size.h : si->virtual_size.w; }

// Jaký podíl [0-1] obsahu viewport ukazuje.
float dvui_scroll_info_visible_fraction(const dvui_ScrollInfo *si, dvui_Direction dir) {
    const float vstart = viewport_start(si, dir);
    const float vsize = viewport_size(si, dir);
    if (vsize == 0) return 1.0f;
    const float max_hard_scroll = dvui_scroll_info_scroll_max(si, dir);
    float length = DVUI_MAX(vsize, virtual_size(si, dir));
    if (vstart < 0) {
        length += -vstart; // dočasně přičteme prázdné místo, které ukazujeme
    } else if (vstart > max_hard_scroll) {
        length += vstart - max_hard_scroll;
    }
    return vsize / length;
}

float dvui_scroll_info_offset(const dvui_ScrollInfo *si, dvui_Direction dir) { return viewport_start(si, dir); }

float dvui_scroll_info_offset_from_max(const dvui_ScrollInfo *si, dvui_Direction dir) {
    return dvui_scroll_info_scroll_max(si, dir) - dvui_scroll_info_offset(si, dir);
}

float dvui_scroll_info_offset_fraction(const dvui_ScrollInfo *si, dvui_Direction dir) {
    const float vstart = viewport_start(si, dir);
    const float vsize = viewport_size(si, dir);
    if (vsize == 0) return 0;
    const float max_hard_scroll = dvui_scroll_info_scroll_max(si, dir);
    float length = DVUI_MAX(vsize, virtual_size(si, dir));
    if (vstart < 0) {
        length += -vstart;
    } else if (vstart > max_hard_scroll) {
        length += vstart - max_hard_scroll;
    }
    const float max_scroll = DVUI_MAX(0.0f, length - vsize);
    if (max_scroll == 0) return 0;
    return DVUI_MAX(0.0f, DVUI_MIN(1.0f, vstart / max_scroll));
}

void dvui_scroll_info_scroll_by_offset(dvui_ScrollInfo *si, dvui_Direction dir, float off) {
    dvui_scroll_info_scroll_to_offset(si, dir, dvui_scroll_info_offset(si, dir) + off);
}

void dvui_scroll_info_scroll_to_offset(dvui_ScrollInfo *si, dvui_Direction dir, float off) {
    const float v = dvui_clampf(off, 0, dvui_scroll_info_scroll_max(si, dir));
    if (dir == DVUI_VERTICAL) {
        si->viewport.y = v;
    } else {
        si->viewport.x = v;
    }
}

void dvui_scroll_info_scroll_to_fraction(dvui_ScrollInfo *si, dvui_Direction dir, float fin) {
    const float f = DVUI_MAX(0.0f, DVUI_MIN(1.0f, fin));
    if (dir == DVUI_VERTICAL) {
        si->viewport.y = f * dvui_scroll_info_scroll_max(si, dir);
    } else {
        si->viewport.x = f * dvui_scroll_info_scroll_max(si, dir);
    }
}

// Posun o stránku (velikost viewportu). up = nahoru/doleva.
void dvui_scroll_info_scroll_page(dvui_ScrollInfo *si, dvui_Direction dir, bool up) {
    float fi = dvui_scroll_info_visible_fraction(si, dir);
    // poslední stránka má offset fraction 1.0, takže mezi 0 a 1 je o jednu
    // pozici méně
    fi = 1.0f / ((1.0f / fi) - 1);
    const float f = up ? dvui_scroll_info_offset_fraction(si, dir) - fi : dvui_scroll_info_offset_fraction(si, dir) + fi;
    dvui_scroll_info_scroll_to_fraction(si, dir, f);
}

void dvui_scroll_info_scroll_page_up(dvui_ScrollInfo *si, dvui_Direction dir) { dvui_scroll_info_scroll_page(si, dir, true); }
void dvui_scroll_info_scroll_page_down(dvui_ScrollInfo *si, dvui_Direction dir) { dvui_scroll_info_scroll_page(si, dir, false); }
