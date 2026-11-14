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
