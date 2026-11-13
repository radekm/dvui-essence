// Pomocníci pro layout.
// DVUI: src/layout.zig
#include "internal.h"

// DVUI: layout.BasicLayout.rectFor
//
// Skládá děti svisle nebo vodorovně. Pokud je některé dítě roztažené ve směru
// skládání, dostane zbytek místa a za ním už žádné dítě být nemá.
dvui_Rect dvui_basic_layout_rect_for(dvui_BasicLayout* self, dvui_Rect content_rect, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    if (self->seen_expanded) {
        // Dítě po roztaženém dítěti nedostane žádné místo. Pokud to má
        // fungovat, obal děti do vertikálního boxu.
        DVUI_LOG_ERR("rectFor() got child %llx after expanded child", (unsigned long long)id);
    }

    dvui_Rect r = content_rect;
    switch (self->dir) {
    case DVUI_VERTICAL:
        if (dvui_expand_is_vertical(e)) self->seen_expanded = true;
        r.y += self->pos;
        r.h = DVUI_MAX(0.0f, r.h - self->pos);
        break;
    case DVUI_HORIZONTAL:
        if (dvui_expand_is_horizontal(e)) self->seen_expanded = true;
        r.x += self->pos;
        r.w = DVUI_MAX(0.0f, r.w - self->pos);
        break;
    }

    const dvui_Rect ret = dvui_place_in(r, min_size, e, g);

    switch (self->dir) {
    case DVUI_VERTICAL: self->pos += ret.h; break;
    case DVUI_HORIZONTAL: self->pos += ret.w; break;
    }
    return ret;
}

// DVUI: layout.BasicLayout.minSizeForChild
dvui_Size dvui_basic_layout_min_size_for_child(dvui_BasicLayout* self, dvui_Size s) {
    switch (self->dir) {
    case DVUI_VERTICAL:
        self->min_size_children.h += s.h;                                     // sčítáme výšky
        self->min_size_children.w = DVUI_MAX(self->min_size_children.w, s.w); // maximum šířek
        break;
    case DVUI_HORIZONTAL:
        self->min_size_children.w += s.w;
        self->min_size_children.h = DVUI_MAX(self->min_size_children.h, s.h);
        break;
    }
    return self->min_size_children;
}

// DVUI: layout.placeOnScreen
//
// Posune `start` tak, aby byl vidět na obrazovce, a případně se vyhne
// obdélníku `spawner` (např. tooltip vedle widgetu).
dvui_RectNatural dvui_place_on_screen(dvui_RectNatural screen, dvui_RectNatural spawner, dvui_PlaceOnScreenAvoid avoid, dvui_RectNatural start) {
    dvui_RectNatural r = start;

    // nejdřív se vyhneme spawneru
    if (!dvui_rect_empty(dvui_rect_intersect(r, spawner))) {
        switch (avoid) {
        case DVUI_AVOID_NONE: break;
        case DVUI_AVOID_HORIZONTAL: r.x = spawner.x + spawner.w; break;
        case DVUI_AVOID_VERTICAL: r.y = spawner.y + spawner.h; break;
        }
    }

    // přetečení vpravo
    switch (avoid) {
    case DVUI_AVOID_NONE:
    case DVUI_AVOID_VERTICAL:
        if ((r.x + r.w) > (screen.x + screen.w)) r.x = (screen.x + screen.w) - r.w;
        if (r.x < screen.x) r.x = screen.x;
        // pořád přetéká vpravo – zmenšíme (ne na nulu, jinak by okno šlo ztratit)
        if ((r.x + r.w) > (screen.x + screen.w)) r.w = DVUI_MAX(24.0f, (screen.x + screen.w) - r.x);
        break;
    case DVUI_AVOID_HORIZONTAL:
        if ((r.x + r.w) > (screen.x + screen.w)) {
            if ((spawner.x - screen.x) > (screen.x + screen.w - (spawner.x + spawner.w))) {
                // vlevo je víc místa, přeskočíme tam
                r.x = spawner.x - r.w;
                if (r.x < screen.x) {
                    r.x = screen.x;
                    r.w = spawner.x - screen.x;
                }
            } else {
                r.w = DVUI_MAX(24.0f, (screen.x + screen.w) - r.x);
            }
        }
        break;
    }

    // přetečení dole
    switch (avoid) {
    case DVUI_AVOID_NONE:
    case DVUI_AVOID_HORIZONTAL:
        if ((r.y + r.h) > (screen.y + screen.h)) r.y = (screen.y + screen.h) - r.h;
        if (r.y < screen.y) r.y = screen.y;
        if ((r.y + r.h) > (screen.y + screen.h)) r.h = DVUI_MAX(24.0f, (screen.y + screen.h) - r.y);
        break;
    case DVUI_AVOID_VERTICAL:
        if ((r.y + r.h) > (screen.y + screen.h)) {
            if ((spawner.y - screen.y) > (screen.y + screen.h - (spawner.y + spawner.h))) {
                // nahoře je víc místa
                r.y = spawner.y - r.h;
                if (r.y < screen.y) {
                    r.y = screen.y;
                    r.h = spawner.y - screen.y;
                }
            } else {
                r.h = DVUI_MAX(24.0f, (screen.y + screen.h) - r.y);
            }
        }
        break;
    }

    return r;
}
