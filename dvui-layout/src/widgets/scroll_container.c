// Kontejner se scrollovaným obsahem (děti skládá pod sebe).
// DVUI: src/widgets/ScrollContainerWidget.zig, dvui.zig – scrollTo, scrollDrag
#include "../internal.h"

#include <math.h>
#include <stdlib.h>

// DVUI: ScrollContainerWidget.scroll_current – řetěz vnořených scroll kontejnerů
static dvui_ScrollContainerWidget* scroll_current = nullptr;

dvui_ScrollContainerWidget* dvui_scroll_container_current(void) { return scroll_current; }

dvui_ScrollContainerWidget* dvui_scroll_container_scroll_set(dvui_ScrollContainerWidget* scroll) {
    dvui_ScrollContainerWidget* ret = scroll_current;
    scroll_current = scroll;
    return ret;
}

static const dvui_Prop defaults[] = {
    DVUI_NAME("ScrollContainer"),
    // většinou je uvnitř ScrollArea, která tohle přepíše
    DVUI_BACKGROUND(true),
    DVUI_MIN_SIZE(5, 5),
    DVUI_STYLE(DVUI_STYLE_WINDOW),
    {0},
};

static dvui_WidgetData* sc_data(void* ptr) { return &((dvui_ScrollContainerWidget*)ptr)->wd; }
static dvui_Rect sc_rect_for(void* ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g);
static dvui_RectScale sc_screen_rect_scale(void* ptr, dvui_Rect r) { return dvui_scroll_container_widget_screen_rect_scale(ptr, r); }
static void sc_min_size_for_child(void* ptr, dvui_Size s) { dvui_scroll_container_widget_min_size_for_child(ptr, s); }

static const dvui_WidgetVTable sc_vtable = {sc_data, sc_rect_for, sc_screen_rect_scale, sc_min_size_for_child};

dvui_Widget dvui_scroll_container_widget_widget(dvui_ScrollContainerWidget* self) { return (dvui_Widget){self, &sc_vtable}; }
dvui_WidgetData* dvui_scroll_container_widget_data(dvui_ScrollContainerWidget* self) { return &self->wd; }

// DVUI: ScrollContainerWidget.init
void dvui_scroll_container_widget_init(dvui_ScrollContainerWidget* self, dvui_Src src, dvui_Props props) {
    dvui_Options options;
    dvui_WidgetProps wp;
    dvui_props_resolve("ScrollContainer", defaults, props,
                       DVUI_TAGS(DVUI_PROP_scroll_info, DVUI_PROP_scroll_area, DVUI_PROP_frame_viewport, DVUI_PROP_lock_visible,
                                 DVUI_PROP_event_rect, DVUI_PROP_process_events_after, DVUI_PROP_user_scroll),
                       &options, &wp);
    dvui_ScrollInfo* io_scroll_info = wp.scroll_info;
    if (!io_scroll_info) {
        DVUI_LOG_ERR("ScrollContainer: chybí povinná vlastnost scroll_info");
        abort();
    }
    const dvui_ScrollContainerInitOptions init_options = {
        .scroll_area = wp.scroll_area,
        .has_frame_viewport = wp.has[DVUI_PROP_frame_viewport],
        .frame_viewport = wp.frame_viewport,
        .lock_visible = wp.lock_visible,
        .has_event_rect = wp.has[DVUI_PROP_event_rect],
        .event_rect = wp.event_rect,
        .process_events_after = wp.process_events_after,
        .user_scroll = wp.user_scroll,
    };
    *self = (dvui_ScrollContainerWidget){
        .wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){.no_scroll_when_focused = true}, options),
        .si = io_scroll_info,
        .init_opts = init_options,
        .last_focus = dvui_last_focused_id_in_frame(),
        .subwindow_id = dvui_subwindow_current_id(),
    };

    if (self->init_opts.user_scroll) *self->init_opts.user_scroll = (dvui_Point){0};

    const dvui_Rect crect = dvui_widget_data_content_rect(&self->wd);
    self->si->viewport.w = crect.w;
    self->si->viewport.h = crect.h;

    dvui_widget_data_register(&self->wd);

    if (self->init_opts.scroll_area) dvui_scroll_area_widget_set_container_rect(self->init_opts.scroll_area, crect);

    if (self->si->horizontal == DVUI_SCROLL_NONE) self->si->virtual_size.w = crect.w;
    if (self->si->vertical == DVUI_SCROLL_NONE) self->si->virtual_size.h = crect.h;

    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){0});

    self->prev_clip = dvui_clip(dvui_widget_data_content_rect_scale(&self->wd).r);

    self->frame_viewport = self->init_opts.has_frame_viewport ? self->init_opts.frame_viewport : dvui_rect_top_left(self->si->viewport);

    self->lock_visible = self->init_opts.lock_visible;
    if (self->lock_visible) {
        const dvui_Id* fv = DVUI_DATA_GET(self->wd.id, "_fv_id", dvui_Id);
        if (fv) {
            self->first_visible_id = *fv;
        } else {
            // nemáme první viditelný widget – lock_visible vypneme
            self->lock_visible = false;
            self->first_visible_id = 0;
        }
        const dvui_Point* off = DVUI_DATA_GET(self->wd.id, "_fv_offset", dvui_Point);
        if (off) self->first_visible_offset = *off;
        const dvui_Point* vp = DVUI_DATA_GET(self->wd.id, "_fv_viewport", dvui_Point);
        if (vp) self->first_visible_viewport = *vp;
    }

    dvui_parent_set(dvui_scroll_container_widget_widget(self));
    self->parent_scroll = dvui_scroll_container_scroll_set(self);
}

bool dvui_scroll_container_widget_match_event(dvui_ScrollContainerWidget* self, dvui_Event* e) {
    const dvui_RectPhysical r = self->init_opts.has_event_rect ? self->init_opts.event_rect : dvui_widget_data_border_rect_scale(&self->wd).r;
    return dvui_event_match(e, (dvui_EventMatchOptions){.id = self->wd.id, .r = r});
}

// DVUI: ScrollContainerWidget.processEvents (zpracování doteků zde odpadá)
void dvui_scroll_container_widget_process_events(dvui_ScrollContainerWidget* self) {
    // mohlo se změnit událostmi
    self->frame_viewport = self->init_opts.has_frame_viewport ? self->init_opts.frame_viewport : dvui_rect_top_left(self->si->viewport);
}

// DVUI: ScrollContainerWidget.processVelocityAfter
void dvui_scroll_container_widget_process_velocity_after(dvui_ScrollContainerWidget* self) {
    dvui_ScrollInfo* si = self->si;
    const float secs = dvui_seconds_since_last_frame();
    // rychlost je jen od doteku – tlumení (prst je vždy nahoře)
    if (dvui_point_non_zero(si->velocity)) {
        const dvui_Point before = dvui_rect_top_left(si->viewport);
        {
            const float damping = 0.0001f + DVUI_MIN(1.0f, fabsf(si->velocity.x) / 50.0f) * (0.7f - 0.0001f);
            si->velocity.x *= expf(logf(damping) * secs);
            if (fabsf(si->velocity.x) > 1) {
                si->viewport.x += si->velocity.x * 50 * secs;
                dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
            } else {
                si->velocity.x = 0;
            }
        }
        {
            const float damping = 0.0001f + DVUI_MIN(1.0f, fabsf(si->velocity.y) / 50.0f) * (0.7f - 0.0001f);
            si->velocity.y *= expf(logf(damping) * secs);
            if (fabsf(si->velocity.y) > 1) {
                si->viewport.y += si->velocity.y * 50 * secs;
                dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
            } else {
                si->velocity.y = 0;
            }
        }
        if (self->init_opts.user_scroll) {
            *self->init_opts.user_scroll = dvui_point_plus(*self->init_opts.user_scroll, dvui_point_diff(dvui_rect_top_left(si->viewport), before));
        }
    }

    // návrat, pokud jsme za hranicí (např. obsah se zmenšil)
    {
        const float max_scroll = dvui_scroll_info_scroll_max(si, DVUI_HORIZONTAL);
        if (si->viewport.x < 0) {
            si->velocity.x = 0;
            si->viewport.x = DVUI_MAX(-20.0f, si->viewport.x);
            si->viewport.x = DVUI_MIN(0.0f, DVUI_MAX(-20.0f, si->viewport.x + 250 * secs));
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        } else if (si->viewport.x > max_scroll) {
            si->velocity.x = 0;
            si->viewport.x = DVUI_MIN(max_scroll + 20, si->viewport.x);
            si->viewport.x = DVUI_MAX(max_scroll, DVUI_MIN(max_scroll + 20, si->viewport.x - 250 * secs));
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        }
    }
    {
        const float max_scroll = dvui_scroll_info_scroll_max(si, DVUI_VERTICAL);
        if (si->viewport.y < 0) {
            si->velocity.y = 0;
            si->viewport.y = DVUI_MAX(-20.0f, si->viewport.y);
            si->viewport.y = DVUI_MIN(0.0f, DVUI_MAX(-20.0f, si->viewport.y + 250 * secs));
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        } else if (si->viewport.y > max_scroll) {
            si->velocity.y = 0;
            si->viewport.y = DVUI_MIN(max_scroll + 20, si->viewport.y);
            si->viewport.y = DVUI_MAX(max_scroll, DVUI_MIN(max_scroll + 20, si->viewport.y - 250 * secs));
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        }
    }
}

// DVUI: ScrollContainerWidget.rectFor
static dvui_Rect sc_rect_for(void* ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    dvui_ScrollContainerWidget* self = ptr;
    // děti skládáme svisle, problém je jen svislé roztažení
    if (self->seen_expanded_child) {
        // Roztažené dítě dostalo zbytek místa a teď chce místo další dítě.
        // Roztažené ve směru scrollování smí být jen poslední dítě.
        DVUI_LOG_ERR("ScrollContainer got child %llx after expanded child", (unsigned long long)id);
    } else if (dvui_expand_is_vertical(e)) {
        self->seen_expanded_child = true;
    }

    const dvui_ScrollInfo* si = self->si;
    const float y = self->next_virtual_size.h;

    float h = 0;
    switch (si->vertical) {
    case DVUI_SCROLL_NONE: h = si->viewport.h - y; break;                       // jen viditelné místo
    case DVUI_SCROLL_AUTO: h = DVUI_MAX(si->viewport.h - y, min_size.h); break; // potřebné nebo víc
    case DVUI_SCROLL_GIVEN: h = si->virtual_size.h - y; break;                   // zadané místo
    }

    float maxw = 0;
    switch (si->horizontal) {
    case DVUI_SCROLL_NONE: maxw = si->viewport.w; break;
    case DVUI_SCROLL_AUTO: maxw = DVUI_MAX(si->viewport.w, min_size.w); break;
    case DVUI_SCROLL_GIVEN: maxw = si->virtual_size.w; break;
    }

    const dvui_Rect rect = {0, y, maxw, h};
    const dvui_Rect ret = dvui_place_in(rect, min_size, e, g);

    if (self->lock_visible && self->first_visible_id == id) {
        self->first_visible_id = 0;
        self->lock_visible = false;

        const dvui_Point scroll_since_last_frame = dvui_point_diff(self->frame_viewport, self->first_visible_viewport);

        self->frame_viewport.x = 0;
        self->frame_viewport.y = y + self->first_visible_offset.y + scroll_since_last_frame.y;
        self->si->viewport.x = self->frame_viewport.x;
        self->si->viewport.y = self->frame_viewport.y;
    }

    if (!self->lock_visible && self->first_visible_id == 0 && self->frame_viewport.y < (ret.y + ret.h)) {
        self->first_visible_id = id;
        self->first_visible_offset = dvui_point_diff(self->frame_viewport, dvui_rect_top_left(ret));
        // kde byl viewport – pro lock_visible v dalším snímku
        self->first_visible_viewport = self->frame_viewport;
    }

    return ret;
}

dvui_Point dvui_scroll_container_widget_point_from_physical(dvui_ScrollContainerWidget* self, dvui_PointPhysical p) {
    return dvui_point_plus(dvui_rect_scale_point_from_physical(dvui_widget_data_content_rect_scale(&self->wd), p), self->frame_viewport);
}

dvui_RectScale dvui_scroll_container_widget_screen_rect_scale(dvui_ScrollContainerWidget* self, dvui_Rect rect) {
    dvui_Rect r = rect;
    if (self->lock_visible) {
        // první viditelný ještě nebyl nalezen – mimo obrazovku
        r.x = 10000;
        r.y = 10000;
    } else {
        r.y -= self->frame_viewport.y;
        r.x -= self->frame_viewport.x;
    }
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_content_rect_scale(&self->wd), r);
}

void dvui_scroll_container_widget_min_size_for_child(dvui_ScrollContainerWidget* self, dvui_Size s) {
    self->next_virtual_size.h += s.h;
    self->next_virtual_size.w = DVUI_MAX(self->next_virtual_size.w, s.w);
}

// DVUI: ScrollContainerWidget.processScrollDrag
void dvui_scroll_container_widget_process_scroll_drag(dvui_ScrollContainerWidget* self, dvui_ScrollDragOptions sd) {
    const dvui_RectScale rs = dvui_widget_data_content_rect_scale(&self->wd);
    const float secs = dvui_seconds_since_last_frame();
    dvui_ScrollInfo* si = self->si;
    float scrolly = 0;
    if (sd.mouse_pt.y <= rs.r.y && sd.screen_rect.y < rs.r.y && si->viewport.y > 0) {
        scrolly = !self->seen_scroll_drag ? -200 * secs : -5;
    }
    if (sd.mouse_pt.y >= (rs.r.y + rs.r.h) && (sd.screen_rect.y + sd.screen_rect.h) > (rs.r.y + rs.r.h) && si->viewport.y < dvui_scroll_info_scroll_max(si, DVUI_VERTICAL)) {
        scrolly = !self->seen_scroll_drag ? 200 * secs : 5;
    }
    float scrollx = 0;
    if (sd.mouse_pt.x <= rs.r.x && sd.screen_rect.x < rs.r.x && si->viewport.x > 0) {
        scrollx = !self->seen_scroll_drag ? -200 * secs : -5;
    }
    if (sd.mouse_pt.x >= (rs.r.x + rs.r.w) && (sd.screen_rect.x + sd.screen_rect.w) > (rs.r.x + rs.r.w) && si->viewport.x < dvui_scroll_info_scroll_max(si, DVUI_HORIZONTAL)) {
        scrollx = !self->seen_scroll_drag ? 200 * secs : 5;
    }

    bool propagate = true;
    const dvui_Point before = dvui_rect_top_left(si->viewport);

    if (scrolly != 0 || scrollx != 0) {
        propagate = false;
        if (scrolly != 0) dvui_scroll_info_scroll_by_offset(si, DVUI_VERTICAL, scrolly);
        if (scrollx != 0) dvui_scroll_info_scroll_by_offset(si, DVUI_HORIZONTAL, scrollx);
        dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        // při scrollování potřebujeme pohyb myši i v dalším snímku
        dvui_current_window()->inject_motion_event = true;
    }

    if (self->init_opts.user_scroll) {
        *self->init_opts.user_scroll = dvui_point_plus(*self->init_opts.user_scroll, dvui_point_diff(dvui_rect_top_left(si->viewport), before));
    }

    self->seen_scroll_drag = true;

    if (propagate && self->parent_scroll && self->parent_scroll->subwindow_id == self->subwindow_id) {
        dvui_scroll_container_widget_process_scroll_drag(self->parent_scroll, sd);
    }
}

// DVUI: ScrollContainerWidget.processScrollTo
void dvui_scroll_container_widget_process_scroll_to(dvui_ScrollContainerWidget* self, dvui_ScrollToOptions st) {
    dvui_ScrollInfo* si = self->si;
    const dvui_Point before = dvui_rect_top_left(si->viewport);
    const dvui_RectScale rs = dvui_widget_data_content_rect_scale(&self->wd);

    // Propagujeme, pokud je celý cíl uvnitř našeho obdélníku (velký grid
    // uvnitř ještě větší scroll oblasti stránky).
    const bool propagate = dvui_rect_equals(dvui_rect_intersect(rs.r, st.screen_rect), st.screen_rect);

    if (si->vertical != DVUI_SCROLL_NONE) {
        const float ypx = rs.r.y - st.screen_rect.y;                                   // jak daleko nahoru
        const float ypx2 = (st.screen_rect.y + st.screen_rect.h) - (rs.r.y + rs.r.h); // dolů
        const float yup = DVUI_MIN(ypx, -ypx2);
        if (yup > 0) {
            si->viewport.y = si->viewport.y - (yup / rs.s);
            if (!st.over_scroll) dvui_scroll_info_scroll_to_offset(si, DVUI_VERTICAL, si->viewport.y);
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        }
        const float ydown = DVUI_MIN(ypx2, -ypx);
        if (ydown > 0) {
            si->viewport.y = si->viewport.y + (ydown / rs.s);
            if (!st.over_scroll) dvui_scroll_info_scroll_to_offset(si, DVUI_VERTICAL, si->viewport.y);
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        }
    }

    if (si->horizontal != DVUI_SCROLL_NONE) {
        const float xpx = rs.r.x - st.screen_rect.x;
        const float xpx2 = (st.screen_rect.x + st.screen_rect.w) - (rs.r.x + rs.r.w);
        const float xleft = DVUI_MIN(xpx, -xpx2);
        if (xleft > 0) {
            si->viewport.x = si->viewport.x - (xleft / rs.s);
            if (!st.over_scroll) dvui_scroll_info_scroll_to_offset(si, DVUI_HORIZONTAL, si->viewport.x);
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        }
        const float xright = DVUI_MIN(xpx2, -xpx);
        if (xright > 0) {
            si->viewport.x = si->viewport.x + (xright / rs.s);
            if (!st.over_scroll) dvui_scroll_info_scroll_to_offset(si, DVUI_HORIZONTAL, si->viewport.x);
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        }
    }

    if (self->init_opts.user_scroll) {
        *self->init_opts.user_scroll = dvui_point_plus(*self->init_opts.user_scroll, dvui_point_diff(dvui_rect_top_left(si->viewport), before));
    }

    if (propagate && self->parent_scroll && self->parent_scroll->subwindow_id == self->subwindow_id) {
        // over_scroll dává smysl jen pro nejvnitřnější kontejner
        dvui_ScrollToOptions opts = st;
        opts.over_scroll = false;
        dvui_scroll_container_widget_process_scroll_to(self->parent_scroll, opts);
    }
}

// DVUI: ScrollContainerWidget.processEventsAfter
void dvui_scroll_container_widget_process_events_after(dvui_ScrollContainerWidget* self) {
    const dvui_Id focus_id = dvui_last_focused_id_in_frame_since(self->last_focus);
    const dvui_RectPhysical r = self->init_opts.has_event_rect ? self->init_opts.event_rect : dvui_widget_data_border_rect_scale(&self->wd).r;
    size_t n;
    dvui_Event* evts = dvui_events(&n);
    for (size_t i = 0; i < n; i++) {
        dvui_Event* e = &evts[i];
        if (!dvui_event_match(e, (dvui_EventMatchOptions){.id = self->wd.id, .focus_id = focus_id, .r = r})) continue;
        dvui_scroll_container_widget_process_event_after(self, e);
    }
}

static void scroll_key(dvui_ScrollContainerWidget* self, dvui_Event* e, dvui_Direction dir, float amount) {
    dvui_ScrollInfo* si = self->si;
    const dvui_ScrollMode mode = dir == DVUI_VERTICAL ? si->vertical : si->horizontal;
    if (mode != DVUI_SCROLL_NONE && dvui_scroll_info_scroll_max(si, dir) > 0.0f) {
        dvui_event_handle(e, &self->wd);
        dvui_scroll_info_scroll_by_offset(si, dir, amount);
        if (self->init_opts.user_scroll) {
            if (dir == DVUI_VERTICAL) {
                self->init_opts.user_scroll->y += amount;
            } else {
                self->init_opts.user_scroll->x += amount;
            }
        }
        dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
    }
}

// DVUI: ScrollContainerWidget.processEventAfter
void dvui_scroll_container_widget_process_event_after(dvui_ScrollContainerWidget* self, dvui_Event* e) {
    dvui_ScrollInfo* si = self->si;
    if (e->kind == DVUI_EVENT_MOUSE) {
        const dvui_EventMouse me = e->evt.mouse;
        if (me.action == DVUI_MOUSE_FOCUS) {
            dvui_event_handle(e, &self->wd);
            // fokus, abychom dostávali klávesnici
            dvui_focus_widget(self->wd.id, 0, &e->num);
        } else if (me.action == DVUI_MOUSE_WHEEL_X) {
            if (dvui_scroll_info_scroll_max(si, DVUI_HORIZONTAL) > 0) {
                if ((me.wheel > 0 && si->viewport.x <= 0) || (me.wheel < 0 && si->viewport.x >= dvui_scroll_info_scroll_max(si, DVUI_HORIZONTAL))) {
                    // už jsme na kraji – událost necháme rodiči
                } else {
                    dvui_event_handle(e, &self->wd);
                    dvui_scroll_info_scroll_by_offset(si, DVUI_HORIZONTAL, -me.wheel);
                    if (self->init_opts.user_scroll) self->init_opts.user_scroll->x -= me.wheel;
                    dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
                }
            }
        } else if (me.action == DVUI_MOUSE_WHEEL_Y) {
            if (dvui_scroll_info_scroll_max(si, DVUI_VERTICAL) > 0) {
                if ((me.wheel > 0 && si->viewport.y <= 0) || (me.wheel < 0 && si->viewport.y >= dvui_scroll_info_scroll_max(si, DVUI_VERTICAL))) {
                    // už jsme na kraji – událost necháme rodiči
                } else {
                    dvui_event_handle(e, &self->wd);
                    dvui_scroll_info_scroll_by_offset(si, DVUI_VERTICAL, -me.wheel);
                    if (self->init_opts.user_scroll) self->init_opts.user_scroll->y -= me.wheel;
                    dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
                }
            }
        } else if (me.action == DVUI_MOUSE_RELEASE && dvui_captured(self->wd.id)) {
            dvui_event_handle(e, &self->wd);
            dvui_capture_mouse(nullptr, e->num);
            dvui_drag_end(me.button);
        }
    } else if (e->kind == DVUI_EVENT_KEY) {
        const dvui_EventKey ke = e->evt.key;
        if (ke.action != DVUI_KEY_ACTION_DOWN && ke.action != DVUI_KEY_ACTION_REPEAT) return;
        switch (ke.code) {
        case DVUI_KEY_UP: scroll_key(self, e, DVUI_VERTICAL, -20); break;
        case DVUI_KEY_DOWN: scroll_key(self, e, DVUI_VERTICAL, 20); break;
        case DVUI_KEY_LEFT: scroll_key(self, e, DVUI_HORIZONTAL, -20); break;
        case DVUI_KEY_RIGHT: scroll_key(self, e, DVUI_HORIZONTAL, 20); break;
        case DVUI_KEY_PAGE_UP:
        case DVUI_KEY_PAGE_DOWN: {
            dvui_event_handle(e, &self->wd);
            const dvui_Point before = dvui_rect_top_left(si->viewport);
            dvui_scroll_info_scroll_page(si, DVUI_VERTICAL, ke.code == DVUI_KEY_PAGE_UP);
            if (self->init_opts.user_scroll) {
                *self->init_opts.user_scroll = dvui_point_plus(*self->init_opts.user_scroll, dvui_point_diff(dvui_rect_top_left(si->viewport), before));
            }
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
            break;
        }
        default: break;
        }
    }
}

// DVUI: ScrollContainerWidget.deinit
void dvui_scroll_container_widget_deinit(dvui_ScrollContainerWidget* self) {
    // clip obnovíme před processEventsAfter – event_rect může být mimo clip
    dvui_clip_set(self->prev_clip);

    if (self->init_opts.process_events_after) {
        dvui_scroll_container_widget_process_events_after(self);
        dvui_scroll_container_widget_process_velocity_after(self);
    }

    DVUI_DATA_SET(self->wd.id, "_fv_id", dvui_Id, self->first_visible_id);
    DVUI_DATA_SET(self->wd.id, "_fv_offset", dvui_Point, self->first_visible_offset);
    DVUI_DATA_SET(self->wd.id, "_fv_viewport", dvui_Point, self->first_visible_viewport);

    dvui_ScrollInfo* si = self->si;
    const dvui_Size padded = dvui_options_pad_size(&self->wd.options, self->next_virtual_size);
    switch (si->horizontal) {
    case DVUI_SCROLL_NONE: self->wd.min_size.w = padded.w; break;
    case DVUI_SCROLL_AUTO:
        self->wd.min_size.w = padded.w;
        if (self->next_virtual_size.w != si->virtual_size.w) {
            si->virtual_size.w = self->next_virtual_size.w;
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        }
        break;
    case DVUI_SCROLL_GIVEN: break;
    }
    switch (si->vertical) {
    case DVUI_SCROLL_NONE: self->wd.min_size.h = padded.h; break;
    case DVUI_SCROLL_AUTO:
        self->wd.min_size.h = padded.h;
        if (self->next_virtual_size.h != si->virtual_size.h) {
            si->virtual_size.h = self->next_virtual_size.h;
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        }
        break;
    case DVUI_SCROLL_GIVEN: break;
    }

    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    dvui_widget_data_min_size_report_to_parent(&self->wd);
    dvui_scroll_container_scroll_set(self->parent_scroll);
    dvui_parent_reset(self->wd.id, self->wd.parent);
}

// DVUI: dvui.scrollTo – posune nejbližší scroll kontejner tak, aby byl
// `screen_rect` vidět.
void dvui_scroll_to(dvui_ScrollToOptions st) {
    if (scroll_current) dvui_scroll_container_widget_process_scroll_to(scroll_current, st);
}

// DVUI: dvui.scrollDrag
void dvui_scroll_drag(dvui_ScrollDragOptions sd) {
    if (scroll_current) dvui_scroll_container_widget_process_scroll_drag(scroll_current, sd);
}
