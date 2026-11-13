// Window a globální funkce platné mezi begin a end.
// DVUI: Window.zig, Subwindows.zig, Dragging.zig, Data.zig, render.zig,
//       dvui.zig (focus, capture, eventMatch, clicked, data*, parent*, clip, …)
#include "internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

dvui_Window *dvui_current_window_ptr = nullptr;

float dvui_scroll_speed =
#if defined(__APPLE__)
    40;
#else
    80;
#endif

// DVUI: dvui.hover_fade_secs
static const float hover_fade_secs = 0.12f;

dvui_Window *dvui_current_window(void) {
    if (!dvui_current_window_ptr) {
        DVUI_LOG_ERR("called outside of dvui_window_begin/dvui_window_end");
        abort();
    }
    return dvui_current_window_ptr;
}

// ---------------------------------------------------------------------------
// Id (DVUI: dvui.zig – Id.extendId, Id.update; FNV-1a 64)
// ---------------------------------------------------------------------------

static const uint64_t fnv_offset = 0xcbf29ce484222325ULL;
static const uint64_t fnv_prime = 0x100000001b3ULL;

static uint64_t fnv_update(uint64_t h, const void *data, size_t len) {
    const uint8_t *p = data;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= fnv_prime;
    }
    return h;
}

dvui_Id dvui_id_extend(dvui_Id start, dvui_Src src, size_t id_extra) {
    uint64_t h = start ? start : fnv_offset;
    // DVUI hashuje ukazatel na jméno souboru; zde obsah (ukazatele na stejné
    // řetězce nemusí být v C sloučené).
    h = fnv_update(h, src.file, strlen(src.file));
    h = fnv_update(h, &src.line, sizeof src.line);
    h = fnv_update(h, &src.column, sizeof src.column);
    const uint64_t extra = id_extra;
    h = fnv_update(h, &extra, sizeof extra);
    return h;
}

dvui_Id dvui_id_update(dvui_Id id, const char *name) {
    return fnv_update(id, name, strlen(name));
}

// ---------------------------------------------------------------------------
// Widget (DVUI: Widget.zig)
// ---------------------------------------------------------------------------

dvui_WidgetData *dvui_widget_data(dvui_Widget w) { return w.vtable->data(w.ptr); }

dvui_Id dvui_widget_extend_id(dvui_Widget w, dvui_Src src, size_t id_extra) {
    return dvui_id_extend(dvui_widget_data(w)->id, src, id_extra);
}

dvui_Rect dvui_widget_rect_for(dvui_Widget w, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    return w.vtable->rect_for(w.ptr, id, min_size, e, g);
}

dvui_RectScale dvui_widget_screen_rect_scale(dvui_Widget w, dvui_Rect r) { return w.vtable->screen_rect_scale(w.ptr, r); }

void dvui_widget_min_size_for_child(dvui_Widget w, dvui_Size s) { w.vtable->min_size_for_child(w.ptr, s); }

// Window jako widget (DVUI: Window.widget/rectFor/screenRectScale/minSizeForChild)
static dvui_RectScale window_rect_scale(dvui_Window *win) {
    return (dvui_RectScale){.r = win->rect_pixels, .s = win->natural_scale};
}

static dvui_WidgetData *window_data(void *ptr) { return &((dvui_Window *)ptr)->wd; }

static dvui_Rect window_rect_for(void *ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    dvui_Window *win = ptr;
    return dvui_basic_layout_rect_for(&win->layout, win->wd.rect, id, min_size, e, g);
}

static dvui_RectScale window_screen_rect_scale(void *ptr, dvui_Rect r) {
    return dvui_rect_scale_rect_to_rect_scale(window_rect_scale(ptr), r);
}

static void window_min_size_for_child(void *ptr, dvui_Size s) {
    // okno OS nemění velikost podle dětí
    (void)ptr;
    (void)s;
}

static const dvui_WidgetVTable window_vtable = {
    .data = window_data,
    .rect_for = window_rect_for,
    .screen_rect_scale = window_screen_rect_scale,
    .min_size_for_child = window_min_size_for_child,
};

static dvui_Widget window_widget(dvui_Window *win) { return (dvui_Widget){.ptr = win, .vtable = &window_vtable}; }

// ---------------------------------------------------------------------------
// Subwindows (DVUI: Subwindows.zig)
// ---------------------------------------------------------------------------

void dvui_subwindows_add(dvui_Subwindows *self, dvui_Id id, dvui_Rect rect, dvui_RectPhysical rect_pixels, bool modal, dvui_Id stay_above_parent_window, bool mouse_events) {
    dvui_Subwindow *existing = dvui_subwindows_get(self, id);
    if (existing) {
        if (existing->render_cmds.len > 0 || existing->render_cmds_after.len > 0) {
            DVUI_LOG_ERR("subwindowAdd %llx is clearing some drawing commands (did you try to draw between subwindowCurrentSet and subwindowAdd?)", (unsigned long long)id);
        }
        // okno tu bylo už minule – jen aktualizujeme data, pořadí zůstává
        existing->used = true;
        existing->rect = rect;
        existing->rect_pixels = rect_pixels;
        existing->modal = modal;
        existing->stay_above_parent_window = stay_above_parent_window;
        existing->mouse_events = mouse_events;
        existing->render_cmds = (dvui_RenderCmdList){0};
        existing->render_cmds_after = (dvui_RenderCmdList){0};
        return;
    }

    // nové okno
    const dvui_Subwindow sw = {
        .id = id,
        .rect = rect,
        .rect_pixels = rect_pixels,
        .used = true,
        .modal = modal,
        .stay_above_parent_window = stay_above_parent_window,
        .mouse_events = mouse_events,
    };

    if (self->len == self->cap) {
        self->cap = self->cap ? self->cap * 2 : 8;
        self->stack = realloc(self->stack, self->cap * sizeof(dvui_Subwindow));
    }

    size_t i = self->len;
    if (stay_above_parent_window) {
        // chce být nad oknem stay_above_parent_window
        i = 0;
        while (i < self->len && self->stack[i].id != stay_above_parent_window) i++;
        if (i < self->len) i++;
        // přeskočíme okna, která už chtějí být nad stejným rodičem
        while (i < self->len && self->stack[i].stay_above_parent_window == stay_above_parent_window) i++;
    }
    memmove(&self->stack[i + 1], &self->stack[i], (self->len - i) * sizeof(dvui_Subwindow));
    self->stack[i] = sw;
    self->len++;
}

dvui_SubwindowCurrentSetReturn dvui_subwindows_set_current(dvui_Subwindows *self, dvui_Id id, const dvui_RectNatural *rect) {
    const dvui_SubwindowCurrentSetReturn ret = {.id = self->current_id, .rect = self->current_rect};
    self->current_id = id;
    if (rect) self->current_rect = *rect;
    return ret;
}

bool dvui_subwindows_raise(dvui_Subwindows *self, dvui_Id id) {
    // stack[0] je hlavní okno, to se nepřesouvá
    if (self->len < 2) return false;
    dvui_Subwindow *items = self->stack + 1;
    const size_t n = self->len - 1;
    for (size_t i = 0; i < n; i++) {
        if (items[i].id != id) continue;
        if (items[i].stay_above_parent_window) return true;
        if (i == n - 1) return true; // už je nahoře
        // přesuneme ho na konec i s okny, která nad ním mají zůstat
        bool first = true;
        while (first || items[i].stay_above_parent_window) {
            first = false;
            const dvui_Subwindow item = items[i];
            memmove(&items[i], &items[i + 1], (n - 1 - i) * sizeof(dvui_Subwindow));
            items[n - 1] = item;
        }
        return true;
    }
    return false;
}

dvui_Id dvui_subwindows_window_for(const dvui_Subwindows *self, dvui_PointPhysical p) {
    for (size_t i = self->len; i > 0; i--) {
        const dvui_Subwindow *sw = &self->stack[i - 1];
        if (sw->mouse_events && (sw->modal || dvui_rect_contains(sw->rect_pixels, p))) return sw->id;
    }
    return 0;
}

dvui_Subwindow *dvui_subwindows_get(dvui_Subwindows *self, dvui_Id id) {
    for (size_t i = self->len; i > 0; i--) {
        if (self->stack[i - 1].id == id) return &self->stack[i - 1];
    }
    return nullptr;
}

dvui_Subwindow *dvui_subwindows_current(dvui_Subwindows *self) { return dvui_subwindows_get(self, self->current_id); }
dvui_Subwindow *dvui_subwindows_focused(dvui_Subwindows *self) { return dvui_subwindows_get(self, self->focused_id); }

void dvui_subwindows_reset(dvui_Subwindows *self) {
    for (size_t i = self->len; i > 0; i--) {
        dvui_Subwindow *sw = &self->stack[i - 1];
        if (sw->used) {
            sw->used = false;
        } else {
            memmove(&self->stack[i - 1], &self->stack[i], (self->len - i) * sizeof(dvui_Subwindow));
            self->len--;
        }
    }
}

// ---------------------------------------------------------------------------
// Události – seznam (v aréně snímku)
// ---------------------------------------------------------------------------

static void events_append(dvui_Window *win, dvui_Event e) {
    dvui_EventList *l = &win->events;
    if (l->len == l->cap) {
        const size_t ncap = l->cap ? l->cap * 2 : 32;
        dvui_Event *n = dvui_arena_alloc(&win->arena, ncap * sizeof(dvui_Event));
        if (l->len) memcpy(n, l->items, l->len * sizeof(dvui_Event));
        l->items = n;
        l->cap = ncap;
    }
    l->items[l->len++] = e;
}

static dvui_Id capture_id(const dvui_Window *win) { return win->has_capture ? win->capture.id : 0; }

// DVUI: Window.positionMouseEventAdd
static void position_mouse_event_add(dvui_Window *win) {
    events_append(win, (dvui_Event){
                           .num = (uint16_t)(win->event_num + 1),
                           .target_widget_id = capture_id(win),
                           .kind = DVUI_EVENT_MOUSE,
                           .evt.mouse = {
                               .action = DVUI_MOUSE_POSITION,
                               .button = DVUI_BUTTON_NONE,
                               .mod = win->modifiers,
                               .p = win->mouse_pt,
                               .floating_win = dvui_subwindows_window_for(&win->subwindows, win->mouse_pt),
                           },
                       });
}

// DVUI: Window.positionMouseEventRemove
static void position_mouse_event_remove(dvui_Window *win) {
    if (win->events.len == 0) return;
    const dvui_Event *e = &win->events.items[win->events.len - 1];
    if (e->kind != DVUI_EVENT_MOUSE || e->evt.mouse.action != DVUI_MOUSE_POSITION) {
        DVUI_LOG_ERR("positionMouseEventRemove removed a non-mouse or non-position event");
    }
    win->events.len--;
}

// DVUI: Window.initEvents
static void init_events(dvui_Window *win) {
    win->events = (dvui_EventList){0};
    win->event_num = 0;
    position_mouse_event_add(win);
}

static dvui_Id focused_widget_id_win(dvui_Window *win) {
    dvui_Subwindow *sw = dvui_subwindows_focused(&win->subwindows);
    return sw ? sw->focused_widget_id : 0;
}

void dvui_window_focus_events(dvui_Window *win, uint16_t event_num, dvui_Id widget_id) {
    for (size_t i = 0; i < win->events.len; i++) {
        dvui_Event *e = &win->events.items[i];
        if (e->num > event_num && e->kind == DVUI_EVENT_KEY) e->target_widget_id = widget_id;
    }
}

void dvui_window_capture_events(dvui_Window *win, uint16_t event_num, dvui_Id widget_id) {
    for (size_t i = 0; i < win->events.len; i++) {
        dvui_Event *e = &win->events.items[i];
        if (e->num > event_num && e->kind == DVUI_EVENT_MOUSE) {
            const dvui_MouseActionKind a = e->evt.mouse.action;
            if (a != DVUI_MOUSE_WHEEL_X && a != DVUI_MOUSE_WHEEL_Y) e->target_widget_id = widget_id;
        }
    }
}

// ---------------------------------------------------------------------------
// Window – vytvoření a zrušení (DVUI: Window.init/deinit)
// ---------------------------------------------------------------------------

dvui_Window *dvui_window_create(dvui_Backend backend) {
    dvui_Window *win = calloc(1, sizeof(dvui_Window));
    win->backend = backend;
    win->theme = dvui_theme_adwaita_light();
    win->content_scale = 1.0f;
    win->natural_scale = 1.0f;
    win->alpha = 1.0f;
    win->snap_to_pixels = true;
    win->rendering = true;
    win->extra_frames_needed = 1;
    win->wd = (dvui_WidgetData){
        .id = dvui_id_extend(0, (dvui_Src){"dvui_window", 0, 0}, 0),
        .init_options = {.subwindow = true},
        .options = {.name = "Window"},
        .src = {"dvui_window", 0, 0},
    };
    win->wd.parent = window_widget(win);
    win->subwindows.focused_id = win->wd.id;
    init_events(win);
    return win;
}

void dvui_window_destroy(dvui_Window *win) {
    if (!win) return;
    dvui_map_free(&win->min_sizes);
    dvui_map_free(&win->data_store);
    for (size_t i = 0; i < win->data_trash_len; i++) free(win->data_trash[i]);
    free(win->data_trash);
    for (size_t i = 0; i < win->widget_allocs_len; i++) free(win->widget_allocs[i]);
    free(win->widget_allocs);
    free(win->subwindows.stack);
    dvui_arena_free_all(&win->arena);
    free(win);
}

void dvui_window_theme_set(dvui_Window *win, dvui_Theme theme) { win->theme = theme; }
void dvui_window_open_flag_set(dvui_Window *win, bool *open_flag) { win->open_flag = open_flag; }

// ---------------------------------------------------------------------------
// Datové úložiště (DVUI: Data.zig, dvui.dataGet/dataSet …)
// ---------------------------------------------------------------------------

static dvui_Window *win_or_current(dvui_Window *win) { return win ? win : dvui_current_window(); }

static void data_trash(dvui_Window *win, void *ptr) {
    if (!ptr) return;
    if (win->data_trash_len == win->data_trash_cap) {
        win->data_trash_cap = win->data_trash_cap ? win->data_trash_cap * 2 : 16;
        win->data_trash = realloc(win->data_trash, win->data_trash_cap * sizeof(void *));
    }
    win->data_trash[win->data_trash_len++] = ptr;
}

void dvui_data_set(dvui_Window *win, dvui_Id id, const char *key, const void *value, size_t size) {
    win = win_or_current(win);
    dvui_MapEntry *e = dvui_map_get_or_put(&win->data_store, dvui_id_update(id, key));
    e->used = true;
    if (e->ptr && e->size == size) {
        // stejná velikost: přepíšeme na místě, ukazatele zůstanou platné
        if (e->ptr != value && size) memmove(e->ptr, value, size);
        return;
    }
    // jiná velikost: starý blob uvolníme až na začátku dalšího snímku
    data_trash(win, e->ptr);
    e->ptr = malloc(size ? size : 1);
    e->size = size;
    if (size) memcpy(e->ptr, value, size);
}

void *dvui_data_get(dvui_Window *win, dvui_Id id, const char *key, size_t size) {
    win = win_or_current(win);
    dvui_MapEntry *e = dvui_map_get(&win->data_store, dvui_id_update(id, key));
    if (!e) return nullptr;
    e->used = true;
    if (e->size != size) {
        DVUI_LOG_ERR("dataGet key \"%s\" has size %zu, asked for %zu", key, e->size, size);
        return nullptr;
    }
    return e->ptr;
}

void *dvui_data_get_ptr_default(dvui_Window *win, dvui_Id id, const char *key, size_t size, const void *def) {
    win = win_or_current(win);
    void *p = dvui_data_get(win, id, key, size);
    if (p) return p;
    dvui_data_set(win, id, key, def, size);
    return dvui_data_get(win, id, key, size);
}

void *dvui_data_get_slice(dvui_Window *win, dvui_Id id, const char *key, size_t *size) {
    win = win_or_current(win);
    dvui_MapEntry *e = dvui_map_get(&win->data_store, dvui_id_update(id, key));
    if (!e) {
        *size = 0;
        return nullptr;
    }
    e->used = true;
    *size = e->size;
    return e->ptr;
}

void dvui_data_remove(dvui_Window *win, dvui_Id id, const char *key) {
    win = win_or_current(win);
    const uint64_t k = dvui_id_update(id, key);
    dvui_MapEntry *e = dvui_map_get(&win->data_store, k);
    if (!e) return;
    // blob může někdo ještě tento snímek používat – jen do koše
    data_trash(win, e->ptr);
    e->ptr = nullptr;
    dvui_map_remove(&win->data_store, k);
}

// DVUI: Data.reset – zahodí data nepoužitá od minulého resetu.
static void data_store_reset(dvui_Window *win) {
    dvui_Map *m = &win->data_store;
    for (size_t i = 0; i < m->cap; i++) {
        dvui_MapEntry *e = &m->entries[i];
        if (e->state != 1) continue;
        if (e->used) {
            e->used = false;
        } else {
            dvui_map_remove(m, e->key);
        }
    }
    for (size_t i = 0; i < win->data_trash_len; i++) free(win->data_trash[i]);
    win->data_trash_len = 0;
}

// DVUI: TrackingAutoHashMap(.put_only).reset – min. velikosti
static void min_sizes_reset(dvui_Window *win) {
    dvui_Map *m = &win->min_sizes;
    for (size_t i = 0; i < m->cap; i++) {
        dvui_MapEntry *e = &m->entries[i];
        if (e->state != 1) continue;
        if (e->used) {
            e->used = false;
        } else {
            dvui_map_remove(m, e->key);
        }
    }
}

bool dvui_min_size_get(dvui_Id id, dvui_Size *out) {
    dvui_MapEntry *e = dvui_map_get(&dvui_current_window()->min_sizes, id);
    if (!e) return false;
    if (out) memcpy(out, e->ptr, sizeof(dvui_Size));
    return true;
}

bool dvui_first_frame(dvui_Id id) { return !dvui_min_size_get(id, nullptr); }

// ---------------------------------------------------------------------------
// Window.begin (DVUI: Window.begin)
// ---------------------------------------------------------------------------

void dvui_window_begin(dvui_Window *win, int64_t time_ns) {
    // Když čas skočí dozadu, zůstaneme na aktuálním frame_time_ns a pro
    // animace použijeme 10 ms.
    uint32_t micros_since_last = 10000;
    if (time_ns > win->frame_time_ns) {
        int64_t nanos = time_ns - win->frame_time_ns;
        const int64_t max_nanos = (int64_t)UINT32_MAX * 1000;
        if (nanos > max_nanos) nanos = max_nanos;
        micros_since_last = (uint32_t)(nanos / 1000);
        if (micros_since_last < 1) micros_since_last = 1;
        win->frame_time_ns = time_ns;
    }

    win->previous_window = dvui_current_window_ptr;
    dvui_current_window_ptr = win;
    if (win->previous_window == win) {
        DVUI_LOG_ERR("Window.begin() window is already the current_window - ensure Window.end() is called for each Window.begin()");
    }

    win->has_cursor_requested = false;
    win->last_focused_id_this_frame = 0;
    win->last_focused_id_in_subwindow = 0;

    data_store_reset(win);
    min_sizes_reset(win);

    // backend.begin se volá před pixelSize/windowSize
    if (win->backend.begin) win->backend.begin(win->backend.ctx);

    const dvui_SizePhysical ps = win->backend.pixel_size(win->backend.ctx);
    win->rect_pixels = (dvui_RectPhysical){0, 0, ps.w, ps.h};
    dvui_clip_set(win->rect_pixels);

    const float sys_content_scale = win->backend.content_scale ? win->backend.content_scale(win->backend.ctx) : 1.0f;
    const dvui_SizeNatural ws = win->backend.window_size(win->backend.ctx);
    win->wd.rect = dvui_rect_scale((dvui_Rect){0, 0, ws.w, ws.h}, 1.0f / sys_content_scale / win->content_scale);
    win->natural_scale = win->wd.rect.w == 0 ? 1.0f : win->rect_pixels.w / win->wd.rect.w;

    // zaokrouhlení kvůli nepřesnostem při content_scale jako 1.25
    win->wd.rect.w = roundf(win->wd.rect.w * 100.0f) / 100.0f;
    win->wd.rect.h = roundf(win->wd.rect.h * 100.0f) / 100.0f;
    win->natural_scale = roundf(win->natural_scale * 100.0f) / 100.0f;

    dvui_subwindows_add(&win->subwindows, win->wd.id, win->wd.rect, win->rect_pixels, false, 0, true);
    dvui_subwindows_set_current(&win->subwindows, win->wd.id, &win->wd.rect);

    if (win->extra_frames_needed > 0) win->extra_frames_needed--;
    win->secs_since_last_frame = (float)micros_since_last / 1000000.0f;

    // widget s capture zmizel
    if (!win->captured_last_frame) win->has_capture = false;
    win->captured_last_frame = false;

    win->wd.parent = window_widget(win);
    win->current_parent = window_widget(win);
    dvui_widget_data_register(&win->wd);

    win->layout = (dvui_BasicLayout){0};
}

// ---------------------------------------------------------------------------
// Přidávání událostí (DVUI: Window.addEvent*)
// ---------------------------------------------------------------------------

void dvui_window_modifiers_set(dvui_Window *win, unsigned mod) { win->modifiers = mod; }

bool dvui_window_add_event_key(dvui_Window *win, dvui_Key code, dvui_KeyAction action, unsigned mod) {
    position_mouse_event_remove(win);
    win->modifiers = mod;
    win->event_num++;
    events_append(win, (dvui_Event){
                           .num = win->event_num,
                           .target_widget_id = focused_widget_id_win(win),
                           .kind = DVUI_EVENT_KEY,
                           .evt.key = {.code = code, .action = action, .mod = mod},
                       });
    position_mouse_event_add(win);
    return win->wd.id != win->subwindows.focused_id;
}

// DVUI: Window.addEventFocus
static bool add_event_focus(dvui_Window *win, dvui_PointPhysical pt, dvui_Button button) {
    position_mouse_event_remove(win);
    const dvui_Id win_id = dvui_subwindows_window_for(&win->subwindows, pt);

    // Hlavní okno vzniká před přidáním událostí, proto ho fokusujeme hned,
    // aby se další klávesové události správně směrovaly.
    if (win_id == win->wd.id) {
        if (win->subwindows.focused_id != win->wd.id) {
            win->subwindows.focused_id = win->wd.id;
            win->extra_frames_needed = 1;
        }
    }

    win->event_num++;
    events_append(win, (dvui_Event){
                           .num = win->event_num,
                           .target_widget_id = capture_id(win),
                           .kind = DVUI_EVENT_MOUSE,
                           .evt.mouse = {.action = DVUI_MOUSE_FOCUS, .button = button, .mod = win->modifiers, .p = pt, .floating_win = win_id},
                       });
    position_mouse_event_add(win);
    return win->wd.id != win_id;
}

bool dvui_window_add_event_mouse_motion(dvui_Window *win, dvui_PointPhysical pt) {
    position_mouse_event_remove(win);
    const dvui_PointPhysical dp = dvui_point_diff(pt, win->mouse_pt);
    win->mouse_pt = pt;
    const dvui_Id win_id = dvui_subwindows_window_for(&win->subwindows, win->mouse_pt);

    win->event_num++;
    events_append(win, (dvui_Event){
                           .num = win->event_num,
                           .target_widget_id = capture_id(win),
                           .kind = DVUI_EVENT_MOUSE,
                           .evt.mouse = {.action = DVUI_MOUSE_MOTION, .motion = dp, .button = DVUI_BUTTON_NONE, .mod = win->modifiers, .p = win->mouse_pt, .floating_win = win_id},
                       });
    position_mouse_event_add(win);
    return win->wd.id != win_id;
}

bool dvui_window_add_event_mouse_button(dvui_Window *win, dvui_Button b, dvui_MouseActionKind action) {
    const dvui_Id target_id = capture_id(win);
    const dvui_Id win_id = dvui_subwindows_window_for(&win->subwindows, win->mouse_pt);

    if (action == DVUI_MOUSE_PRESS && dvui_button_pointer(b)) {
        add_event_focus(win, win->mouse_pt, b);
    }

    position_mouse_event_remove(win);
    win->event_num++;
    events_append(win, (dvui_Event){
                           .num = win->event_num,
                           .target_widget_id = target_id,
                           .kind = DVUI_EVENT_MOUSE,
                           .evt.mouse = {.action = action, .button = b, .mod = win->modifiers, .p = win->mouse_pt, .floating_win = win_id},
                       });
    position_mouse_event_add(win);
    return win->wd.id != win_id;
}

bool dvui_window_add_event_mouse_wheel(dvui_Window *win, float ticks, dvui_Direction dir) {
    position_mouse_event_remove(win);
    const dvui_Id win_id = dvui_subwindows_window_for(&win->subwindows, win->mouse_pt);
    dvui_MouseActionKind action = DVUI_MOUSE_WHEEL_X;
    if (dir == DVUI_VERTICAL) action = dvui_mod_shift_only(win->modifiers) ? DVUI_MOUSE_WHEEL_X : DVUI_MOUSE_WHEEL_Y;
    win->event_num++;
    events_append(win, (dvui_Event){
                           .num = win->event_num,
                           .kind = DVUI_EVENT_MOUSE,
                           .evt.mouse = {.action = action, .wheel = ticks, .button = DVUI_BUTTON_NONE, .mod = win->modifiers, .p = win->mouse_pt, .floating_win = win_id},
                       });
    position_mouse_event_add(win);
    return win->wd.id != win_id;
}

void dvui_window_add_event_window(dvui_Window *win, dvui_WindowAction action) {
    position_mouse_event_remove(win);
    win->event_num++;
    events_append(win, (dvui_Event){.num = win->event_num, .kind = DVUI_EVENT_WINDOW, .evt.window = action});
    position_mouse_event_add(win);
}

void dvui_window_add_event_app_quit(dvui_Window *win) {
    position_mouse_event_remove(win);
    win->event_num++;
    events_append(win, (dvui_Event){.num = win->event_num, .kind = DVUI_EVENT_APP_QUIT});
    position_mouse_event_add(win);
}

dvui_Cursor dvui_window_cursor_requested(const dvui_Window *win) {
    if (win->dragging.has_cursor && win->dragging.state == DVUI_DRAG_STATE_DRAGGING) return win->dragging.cursor;
    return win->has_cursor_requested ? win->cursor_requested : DVUI_CURSOR_ARROW;
}

// ---------------------------------------------------------------------------
// Renderování (DVUI: render.zig, Window.renderCommands, Window.addRenderCommand)
// ---------------------------------------------------------------------------

void dvui_window_add_render_command(dvui_Window *win, dvui_RenderCommand cmd, bool after) {
    dvui_Subwindow *sw = dvui_subwindows_current(&win->subwindows);
    if (!sw) sw = &win->subwindows.stack[0];
    if (after) {
        dvui_render_cmd_list_append(win->defer_render_cmds_after ? win->defer_render_cmds_after : &sw->render_cmds_after, cmd);
    } else {
        dvui_render_cmd_list_append(win->defer_render_cmds ? win->defer_render_cmds : &sw->render_cmds, cmd);
    }
}

// DVUI: Rect.Physical.snapToPixels
static dvui_RectPhysical snap_rect(dvui_RectPhysical r) {
    return (dvui_RectPhysical){roundf(r.x), roundf(r.y), DVUI_MAX(0.0f, roundf(r.w)), DVUI_MAX(0.0f, roundf(r.h))};
}

// DVUI: render.renderTriangles – vykreslí obdélník s ohledem na clip.
static void render_rect_now(dvui_Window *win, dvui_RectPhysical r, dvui_Color color) {
    if (dvui_rect_empty(r)) return;
    if (dvui_rect_empty(dvui_rect_intersect(win->clip_rect, r))) return;

    // rozšíříme clip na celé pixely
    dvui_RectPhysical clipping = win->clip_rect;
    clipping.w = DVUI_MAX(0.0f, ceilf(clipping.x - floorf(clipping.x) + clipping.w));
    clipping.x = floorf(clipping.x);
    clipping.h = DVUI_MAX(0.0f, ceilf(clipping.y - floorf(clipping.y) + clipping.h));
    clipping.y = floorf(clipping.y);

    // clip nikdy mimo cíl (některé backendy jsou citlivé)
    clipping = dvui_rect_intersect(clipping, win->rect_pixels);
    const dvui_RectPhysical bounds = dvui_rect_intersect(r, win->rect_pixels);
    const dvui_RectPhysical inter = dvui_rect_intersect(bounds, clipping);
    if (dvui_rect_empty(inter)) return;

    const dvui_RectPhysical *clipr = nullptr;
    if (!(inter.x == bounds.x && inter.y == bounds.y && fabsf(inter.w - bounds.w) < 0.001f && fabsf(inter.h - bounds.h) < 0.001f)) {
        clipr = &clipping;
    }

    // premultiplikovaná barva (DVUI: Color.PMA)
    const dvui_Color pma = {
        (uint8_t)(((unsigned)color.r * color.a + 127) / 255),
        (uint8_t)(((unsigned)color.g * color.a + 127) / 255),
        (uint8_t)(((unsigned)color.b * color.a + 127) / 255),
        color.a,
    };
    const dvui_Vertex vtx[4] = {
        {.pos = {r.x, r.y}, .col = pma},
        {.pos = {r.x + r.w, r.y}, .col = pma},
        {.pos = {r.x + r.w, r.y + r.h}, .col = pma},
        {.pos = {r.x, r.y + r.h}, .col = pma},
    };
    static const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    win->backend.draw_clipped_triangles(win->backend.ctx, vtx, 4, idx, 6, clipr);
}

void dvui_render_fill_rect(dvui_RectPhysical r, dvui_Color color, bool after) {
    dvui_Window *win = dvui_current_window();
    if (dvui_rect_empty(dvui_clip_get())) return;
    if (after || !win->rendering) {
        dvui_window_add_render_command(win, (dvui_RenderCommand){.rect = r, .color = color, .clip = win->clip_rect, .alpha = win->alpha, .snap = win->snap_to_pixels}, after);
        return;
    }
    if (win->snap_to_pixels) r = snap_rect(r);
    render_rect_now(win, r, dvui_color_opacity(color, win->alpha));
}

// DVUI: Rect.Physical.stroke – zde jako čtyři pruhy kolem středové čáry `r`.
void dvui_render_stroke_rect(dvui_RectPhysical r, float thickness, dvui_Color color, bool after) {
    const float h = thickness / 2;
    const dvui_RectPhysical outer = dvui_rect_outset(r, dvui_rect_all(h));
    const dvui_RectPhysical inner = dvui_rect_inset_all(r, h);
    // nahoře, dole, vlevo, vpravo
    dvui_render_fill_rect((dvui_RectPhysical){outer.x, outer.y, outer.w, inner.y - outer.y}, color, after);
    dvui_render_fill_rect((dvui_RectPhysical){outer.x, inner.y + inner.h, outer.w, outer.y + outer.h - (inner.y + inner.h)}, color, after);
    dvui_render_fill_rect((dvui_RectPhysical){outer.x, inner.y, inner.x - outer.x, inner.h}, color, after);
    dvui_render_fill_rect((dvui_RectPhysical){inner.x + inner.w, inner.y, outer.x + outer.w - (inner.x + inner.w), inner.h}, color, after);
}

// DVUI: Window.renderCommands
static void render_commands(dvui_Window *win, const dvui_RenderCmdList *queue) {
    const dvui_RectPhysical old_clip = win->clip_rect;
    const float old_alpha = win->alpha;
    for (size_t i = 0; i < queue->len; i++) {
        const dvui_RenderCommand *c = &queue->items[i];
        win->clip_rect = c->clip;
        win->alpha = c->alpha;
        render_rect_now(win, c->snap ? snap_rect(c->rect) : c->rect, dvui_color_opacity(c->color, c->alpha));
    }
    win->clip_rect = old_clip;
    win->alpha = old_alpha;
}

void dvui_render_front_to_back_init_reset(dvui_RenderFrontToBack *self) {
    dvui_Window *win = dvui_current_window();
    dvui_RenderCmdList *cmds = dvui_frame_alloc(sizeof(dvui_RenderCmdList));
    dvui_RenderCmdList *cmds_after = dvui_frame_alloc(sizeof(dvui_RenderCmdList));
    *cmds = (dvui_RenderCmdList){0};
    *cmds_after = (dvui_RenderCmdList){0};
    *self = (dvui_RenderFrontToBack){
        .render_cmds = cmds,
        .render_cmds_after = cmds_after,
        .prev_rendering = win->rendering,
        .prev_dr_cmds = win->defer_render_cmds,
        .prev_dr_cmds_after = win->defer_render_cmds_after,
    };
    win->rendering = false;
    win->defer_render_cmds = nullptr;
    win->defer_render_cmds_after = nullptr;
}

void dvui_render_front_to_back_deinit(dvui_RenderFrontToBack *self) {
    dvui_Window *win = dvui_current_window();
    // obnovíme předchozí fronty
    win->rendering = self->prev_rendering;
    win->defer_render_cmds = self->prev_dr_cmds;
    win->defer_render_cmds_after = self->prev_dr_cmds_after;

    // vše jde do fronty „after“
    dvui_Subwindow *sw = dvui_subwindows_current(&win->subwindows);
    if (!sw) sw = &win->subwindows.stack[0];
    dvui_RenderCmdList *target = win->defer_render_cmds_after ? win->defer_render_cmds_after : &sw->render_cmds_after;
    const dvui_RenderCmdList *after = self->render_cmds_after;
    const dvui_RenderCmdList *normal = self->render_cmds;
    dvui_render_cmd_list_insert_slice(target, 0, after->items, after->len);
    dvui_render_cmd_list_insert_slice(target, 0, normal->items, normal->len);
}

// DVUI: Window.endRendering
static void end_rendering(dvui_Window *win) {
    for (size_t i = 0; i < win->subwindows.len; i++) {
        dvui_Subwindow *sw = &win->subwindows.stack[i];
        render_commands(win, &sw->render_cmds);
        sw->render_cmds = (dvui_RenderCmdList){0};
        render_commands(win, &sw->render_cmds_after);
        sw->render_cmds_after = (dvui_RenderCmdList){0};
    }
}

// ---------------------------------------------------------------------------
// Window.end (DVUI: Window.end)
// ---------------------------------------------------------------------------

int64_t dvui_window_end(dvui_Window *win) {
    // všechny widgety musí vrátit rodiče
    dvui_parent_reset(win->wd.id, window_widget(win));

    end_rendering(win);

    for (size_t i = 0; i < win->events.len; i++) {
        dvui_Event *e = &win->events.items[i];
        if (e->handled) continue;

        if (e->kind == DVUI_EVENT_MOUSE && e->evt.mouse.action == DVUI_MOUSE_RELEASE && win->dragging.state == DVUI_DRAG_STATE_DRAGGING && win->dragging.button == e->evt.mouse.button) {
            // nezpracované puštění tlačítka ukončí tažení
            dvui_drag_end(e->evt.mouse.button);
            win->extra_frames_needed = 1;
        }

        if (e->kind == DVUI_EVENT_MOUSE) {
            if (!dvui_event_match(e, (dvui_EventMatchOptions){.id = win->wd.id, .r = win->rect_pixels})) continue;
            if (e->evt.mouse.action == DVUI_MOUSE_FOCUS) {
                dvui_event_handle(e, &win->wd);
                dvui_focus_widget(0, 0, nullptr);
            }
        } else if (e->kind == DVUI_EVENT_WINDOW) {
            if (!dvui_event_match(e, (dvui_EventMatchOptions){.id = win->wd.id, .r = win->rect_pixels})) continue;
            if (e->evt.window == DVUI_WINDOW_CLOSE) {
                dvui_event_handle(e, &win->wd);
                if (win->open_flag) *win->open_flag = false;
                win->extra_frames_needed = 1;
            } else if (e->evt.window == DVUI_WINDOW_LEAVE) {
                dvui_event_handle(e, &win->wd);
                win->mouse_pt = (dvui_PointPhysical){-1, -1};
                win->extra_frames_needed = 1;
            }
        } else if (e->kind == DVUI_EVENT_APP_QUIT) {
            dvui_event_handle(e, &win->wd);
            if (win->open_flag) *win->open_flag = false;
            win->extra_frames_needed = 1;
        }
    }

    // fokusované subwindow zmizelo → fokus na nejvyšší „normální“ okno
    dvui_Subwindow *focused_sw = dvui_subwindows_focused(&win->subwindows);
    if (focused_sw && !focused_sw->used) {
        for (size_t i = win->subwindows.len; i > 0; i--) {
            const dvui_Subwindow *sw = &win->subwindows.stack[i - 1];
            if (sw->used && !sw->stay_above_parent_window) {
                dvui_focus_subwindow(sw->id, nullptr);
                break;
            }
        }
        win->extra_frames_needed = 1;
    }

    if (win->backend.end) win->backend.end(win->backend.ctx);

    win->mouse_pt_prev = win->mouse_pt;

    position_mouse_event_remove(win);

    // widgety alokované a neuvolněné (chyba uživatele) uvolníme
    for (size_t i = 0; i < win->widget_allocs_len; i++) free(win->widget_allocs[i]);
    win->widget_allocs_len = 0;

    dvui_arena_reset(&win->arena);
    win->defer_render_cmds = nullptr;
    win->defer_render_cmds_after = nullptr;

    dvui_subwindows_reset(&win->subwindows);

    init_events(win);

    if (win->inject_motion_event) {
        win->inject_motion_event = false;
        dvui_window_add_event_mouse_motion(win, win->mouse_pt);
    }

    dvui_current_window_ptr = win->previous_window;

    if (win->backend.set_cursor) win->backend.set_cursor(win->backend.ctx, dvui_window_cursor_requested(win));
    if (win->backend.render_present) win->backend.render_present(win->backend.ctx);

    if (win->extra_frames_needed > 0) return 0;
    return -1;
}

// ---------------------------------------------------------------------------
// Globální funkce (DVUI: dvui.zig)
// ---------------------------------------------------------------------------

dvui_Widget dvui_parent_get(void) { return dvui_current_window()->current_parent; }
void dvui_parent_set(dvui_Widget w) { dvui_current_window()->current_parent = w; }

void dvui_parent_reset(dvui_Id id, dvui_Widget prev_parent) {
    dvui_Window *cw = dvui_current_window();
    const dvui_WidgetData *cur = dvui_widget_data(cw->current_parent);
    if (id != cur->id) {
        DVUI_LOG_ERR("widget is not closed within its parent. did you forget to call `deinit`?");
        const dvui_WidgetData *wd = cur;
        for (;;) {
            DVUI_LOG_ERR("  %s:%d %s %llx", wd->src.file, wd->src.line, wd->options.name ? wd->options.name : "???", (unsigned long long)wd->id);
            if (dvui_widget_data_is_root(wd)) break;
            wd = dvui_widget_data(wd->parent);
        }
    }
    cw->current_parent = prev_parent;
}

void dvui_refresh(dvui_Window *win, dvui_Src src, dvui_Id id) {
    (void)src;
    (void)id;
    if (win) {
        win->extra_frames_needed = 1;
    } else if (dvui_current_window_ptr) {
        dvui_current_window_ptr->extra_frames_needed = 1;
    } else {
        DVUI_LOG_ERR("%s:%d refresh current_window was null", src.file, src.line);
    }
}

dvui_RectNatural dvui_window_rect(void) { return dvui_current_window()->wd.rect; }
dvui_RectPhysical dvui_window_rect_pixels(void) { return dvui_current_window()->rect_pixels; }
dvui_RectScale dvui_window_rect_scale(void) { return window_rect_scale(dvui_current_window()); }
float dvui_window_natural_scale(void) { return dvui_current_window()->natural_scale; }
float dvui_seconds_since_last_frame(void) { return dvui_current_window()->secs_since_last_frame; }
const dvui_Theme *dvui_theme_get(void) { return &dvui_current_window()->theme; }

dvui_RectPhysical dvui_clip_get(void) { return dvui_current_window()->clip_rect; }

dvui_RectPhysical dvui_clip(dvui_RectPhysical r) {
    dvui_Window *cw = dvui_current_window();
    const dvui_RectPhysical ret = cw->clip_rect;
    dvui_clip_set(dvui_rect_intersect(cw->clip_rect, r));
    return ret;
}

void dvui_clip_set(dvui_RectPhysical r) { dvui_current_window()->clip_rect = r; }

float dvui_alpha(float mult) {
    dvui_Window *cw = dvui_current_window();
    const float ret = cw->alpha;
    dvui_alpha_set(cw->alpha * mult);
    return ret;
}

void dvui_alpha_set(float a) { dvui_current_window()->alpha = dvui_clampf(a, 0, 1); }
bool dvui_snap_to_pixels(void) { return dvui_current_window()->snap_to_pixels; }

// --- Subwindows a fokus

void dvui_subwindow_add(dvui_Id id, dvui_Rect rect, dvui_RectPhysical rect_pixels, bool modal, dvui_Id stay_above_parent_window, bool mouse_events) {
    dvui_subwindows_add(&dvui_current_window()->subwindows, id, rect, rect_pixels, modal, stay_above_parent_window, mouse_events);
}

dvui_SubwindowCurrentSetReturn dvui_subwindow_current_set(dvui_Id id, const dvui_RectNatural *rect) {
    return dvui_subwindows_set_current(&dvui_current_window()->subwindows, id, rect);
}

dvui_Id dvui_subwindow_current_id(void) { return dvui_current_window()->subwindows.current_id; }
dvui_Id dvui_focused_subwindow_id(void) { return dvui_current_window()->subwindows.focused_id; }

void dvui_focus_subwindow(dvui_Id subwindow_id, const uint16_t *event_num) {
    dvui_Window *win = dvui_current_window();
    const dvui_Id win_id = subwindow_id ? subwindow_id : win->subwindows.current_id;
    if (win->subwindows.focused_id == win_id) return;
    win->subwindows.focused_id = win_id;
    win->extra_frames_needed = 1;
    if (event_num) {
        dvui_Subwindow *sw = dvui_subwindows_focused(&win->subwindows);
        if (sw) dvui_window_focus_events(win, *event_num, sw->focused_widget_id);
    }
}

void dvui_raise_subwindow(dvui_Id subwindow_id) {
    if (!dvui_subwindows_raise(&dvui_current_window()->subwindows, subwindow_id)) {
        DVUI_LOG_ERR("raiseSubwindow: could not find subwindow %llx", (unsigned long long)subwindow_id);
    }
}

// DVUI: Window.focusWidget (bez klávesové navigace tab_index)
void dvui_focus_widget(dvui_Id id, dvui_Id subwindow_id, const uint16_t *event_num) {
    dvui_Window *win = dvui_current_window();
    win->scroll_to_focused = false;
    const dvui_Id swid = subwindow_id ? subwindow_id : win->subwindows.current_id;
    dvui_Subwindow *sw = dvui_subwindows_get(&win->subwindows, swid);
    if (!sw) return;
    if (sw->focused_widget_id == id) return;
    sw->focused_widget_id = id;
    if (event_num) dvui_window_focus_events(win, *event_num, id);
    win->extra_frames_needed = 1;

    if (id) {
        win->scroll_to_focused = true;
        if (win->last_registered_id_this_frame == id) {
            win->last_focused_id_this_frame = id;
            win->last_focused_id_in_subwindow = id;
        } else {
            // projdeme řetězec rodičů
            const dvui_WidgetData *wd = dvui_widget_data(win->current_parent);
            for (;;) {
                if (wd->id == id) {
                    win->last_focused_id_this_frame = id;
                    win->last_focused_id_in_subwindow = id;
                    break;
                }
                if (dvui_widget_data_is_root(wd)) break;
                wd = dvui_widget_data(wd->parent);
            }
        }
    }
}

dvui_Id dvui_focused_widget_id(void) { return focused_widget_id_win(dvui_current_window()); }

dvui_Id dvui_focused_widget_id_in_current_subwindow(void) {
    dvui_Window *cw = dvui_current_window();
    dvui_Subwindow *sw = dvui_subwindows_current(&cw->subwindows);
    if (!sw) sw = &cw->subwindows.stack[0];
    return sw->focused_widget_id;
}

dvui_Id dvui_last_focused_id_in_frame(void) { return dvui_current_window()->last_focused_id_this_frame; }

dvui_Id dvui_last_focused_id_in_frame_since(dvui_Id prev) {
    const dvui_Id last = dvui_last_focused_id_in_frame();
    return prev != last ? last : 0;
}

void dvui_cursor_set(dvui_Cursor cursor) {
    dvui_Window *cw = dvui_current_window();
    if (!cw->has_cursor_requested) {
        cw->has_cursor_requested = true;
        cw->cursor_requested = cursor;
    }
}

// --- Capture (DVUI: dvui.captureMouse …, jen jeden ukazatel – myš)

void dvui_capture_mouse(const dvui_WidgetData *wd, uint16_t event_num) {
    dvui_Window *cw = dvui_current_window();
    if (wd) {
        cw->has_capture = true;
        cw->capture = (dvui_CaptureMouse){
            .id = wd->id,
            .rect = dvui_widget_data_border_rect_scale(wd).r,
            .subwindow_id = dvui_subwindow_current_id(),
        };
        cw->captured_last_frame = true;
        dvui_window_capture_events(cw, event_num, wd->id);
    } else {
        cw->has_capture = false;
        dvui_window_capture_events(cw, event_num, 0);
    }
}

void dvui_capture_mouse_maintain(dvui_CaptureMouse cm) {
    dvui_Window *cw = dvui_current_window();
    if (!cw->has_capture || cw->capture.id != cm.id) return;
    // capture udržíme jen nad nejvyšším modálním oknem
    for (size_t i = cw->subwindows.len; i > 0; i--) {
        const dvui_Subwindow *sw = &cw->subwindows.stack[i - 1];
        if (sw->id == cw->subwindows.current_id) {
            cw->capture.rect = cm.rect;
            cw->captured_last_frame = true;
            break;
        } else if (sw->modal) {
            // modální okno je výš – capture i tažení zrušíme
            dvui_capture_mouse(nullptr, 0);
            dvui_drag_end(DVUI_BUTTON_LEFT);
            return;
        }
    }
}

bool dvui_captured(dvui_Id id) {
    const dvui_Window *cw = dvui_current_window();
    return cw->has_capture && cw->capture.id == id;
}

static bool capture_mouse_for(dvui_Id id, dvui_CaptureMouse *out) {
    const dvui_Window *cw = dvui_current_window();
    if (cw->has_capture && cw->capture.id == id) {
        *out = cw->capture;
        return true;
    }
    return false;
}

// --- Dragging (DVUI: Dragging.zig)

static const float drag_threshold = 3.0f;

void dvui_drag_pre_start(dvui_Button b, dvui_PointPhysical p, dvui_DragStartOptions options) {
    dvui_Dragging *d = &dvui_current_window()->dragging;
    *d = (dvui_Dragging){.state = DVUI_DRAG_STATE_PRESTART, .button = b, .pt = p, .offset = options.offset, .name = options.name, .has_cursor = options.has_cursor, .cursor = options.cursor};
}

void dvui_drag_start(dvui_Button b, dvui_PointPhysical p, dvui_DragStartOptions options) {
    dvui_Dragging *d = &dvui_current_window()->dragging;
    *d = (dvui_Dragging){.state = DVUI_DRAG_STATE_DRAGGING, .button = b, .pt = p, .offset = options.offset, .name = options.name, .has_cursor = options.has_cursor, .cursor = options.cursor};
}

dvui_PointPhysical dvui_drag_offset(dvui_Button b) {
    (void)b;
    return dvui_current_window()->dragging.offset;
}

bool dvui_dragging(dvui_EventMouse me, const char *name, dvui_PointPhysical *out) {
    dvui_Window *cw = dvui_current_window();
    dvui_Dragging *d = &cw->dragging;
    if (name && strcmp(name, d->name ? d->name : "") != 0) return false;
    switch (d->state) {
    case DVUI_DRAG_STATE_NONE: return false;
    case DVUI_DRAG_STATE_DRAGGING: {
        const dvui_PointPhysical dp = dvui_point_diff(me.p, d->pt);
        d->pt = me.p;
        if (out) *out = dp;
        return true;
    }
    case DVUI_DRAG_STATE_PRESTART: {
        const dvui_PointPhysical dp = dvui_point_diff(me.p, d->pt);
        const dvui_Point dps = dvui_point_scale(dp, 1.0f / cw->natural_scale);
        if (fabsf(dps.x) > drag_threshold || fabsf(dps.y) > drag_threshold) {
            d->pt = me.p;
            d->state = DVUI_DRAG_STATE_DRAGGING;
            if (out) *out = dp;
            return true;
        }
        return false;
    }
    }
    return false;
}

void dvui_drag_end(dvui_Button b) {
    (void)b;
    dvui_Dragging *d = &dvui_current_window()->dragging;
    d->state = DVUI_DRAG_STATE_NONE;
    d->has_cursor = false;
}

// --- Události (DVUI: dvui.events, eventMatch, clicked)

dvui_Event *dvui_events(size_t *count) {
    dvui_Window *cw = dvui_current_window();
    *count = cw->events.len;
    return cw->events.items;
}

void dvui_event_handle(dvui_Event *e, const dvui_WidgetData *wd) {
    (void)wd;
    e->handled = true;
}

bool dvui_event_match(dvui_Event *e, dvui_EventMatchOptions opts) {
    if (e->handled) return false;

    switch (e->kind) {
    case DVUI_EVENT_APP_QUIT: break; // aplikační události pasují vždy
    case DVUI_EVENT_WINDOW:
        if (e->target_widget_id && e->target_widget_id != opts.id) return false;
        break;
    case DVUI_EVENT_KEY:
        if (e->target_widget_id != opts.id && (opts.focus_id == 0 || opts.focus_id != e->target_widget_id)) {
            // není to fokusovaný widget
            return false;
        }
        break;
    case DVUI_EVENT_MOUSE: {
        const dvui_EventMouse *me = &e->evt.mouse;
        bool other_capture = false;
        if (e->target_widget_id) {
            // událost během capture
            if (e->target_widget_id == opts.id) return true; // máme capture
            other_capture = true;
        }

        const dvui_Dragging *d = &dvui_current_window()->dragging;
        if (d->state == DVUI_DRAG_STATE_DRAGGING && d->name && (!opts.drag_name || strcmp(d->name, opts.drag_name) != 0)) {
            // probíhá tažení mezi widgety, o kterém nevíme
            return false;
        }

        if (me->floating_win != dvui_subwindow_current_id()) return false; // nad námi je floating okno
        if (!dvui_rect_contains(opts.r, me->p)) return false;              // myš není v našem obdélníku
        if (!dvui_rect_contains(dvui_clip_get(), me->p)) return false;     // mimo clip (odscrollované)

        if (other_capture) {
            dvui_CaptureMouse cap;
            if (capture_mouse_for(e->target_widget_id, &cap)) {
                // capture má někdo jiný, ale .position může dostat i widget,
                // který se s ním překrývá ve stejném subwindow (zvýraznění pozadí)
                if (me->action == DVUI_MOUSE_POSITION && cap.subwindow_id == dvui_subwindow_current_id() && !dvui_rect_empty(dvui_rect_intersect(cap.rect, opts.r))) {
                    return true;
                }
            }
            return false;
        }
        break;
    }
    }
    return true;
}

bool dvui_event_match_simple(dvui_Event *e, dvui_WidgetData *wd) {
    return dvui_event_match(e, (dvui_EventMatchOptions){.id = wd->id, .r = dvui_widget_data_border_rect_scale(wd).r});
}

bool dvui_clicked(const dvui_WidgetData *wd, dvui_ClickOptions opts) {
    bool click = false;
    const dvui_RectPhysical click_rect = opts.rect.has ? opts.rect.v : dvui_widget_data_border_rect_scale(wd).r;
    size_t n;
    dvui_Event *evts = dvui_events(&n);
    for (size_t i = 0; i < n; i++) {
        dvui_Event *e = &evts[i];
        if (!dvui_event_match(e, (dvui_EventMatchOptions){.id = wd->id, .r = click_rect})) continue;

        if (e->kind == DVUI_EVENT_MOUSE) {
            const dvui_EventMouse me = e->evt.mouse;
            if (me.action == DVUI_MOUSE_FOCUS) {
                dvui_event_handle(e, wd);
                // fokus pro události od e->num dál
                dvui_focus_widget(wd->id, 0, &e->num);
            } else if (me.action == DVUI_MOUSE_PRESS && dvui_button_pointer(me.button)) {
                dvui_event_handle(e, wd);
                dvui_capture_mouse(wd, e->num);
                dvui_drag_pre_start(me.button, me.p, (dvui_DragStartOptions){0});
            } else if (me.action == DVUI_MOUSE_RELEASE && dvui_button_pointer(me.button)) {
                // máme ještě capture?
                if (dvui_captured(wd->id)) {
                    dvui_event_handle(e, wd);
                    dvui_capture_mouse(nullptr, e->num);
                    dvui_drag_end(me.button);
                    // puštění uvnitř rámečku = úspěšné kliknutí
                    if (dvui_rect_contains(click_rect, me.p)) {
                        dvui_refresh(nullptr, DVUI_SRC, wd->id);
                        click = true;
                    }
                }
            } else if (me.action == DVUI_MOUSE_POSITION) {
                // .position se obvykle neoznačuje jako zpracovaná, aby mohlo
                // zvýrazňovat víc widgetů
                if (!opts.no_hover_cursor) dvui_cursor_set(DVUI_CURSOR_HAND);
                if (opts.hovered) *opts.hovered = true;
            }
        } else if (e->kind == DVUI_EVENT_KEY) {
            // DVUI: keybind "activate" = enter nebo mezerník
            const dvui_EventKey ke = e->evt.key;
            if (ke.action == DVUI_KEY_ACTION_DOWN && (ke.code == DVUI_KEY_ENTER || ke.code == DVUI_KEY_SPACE)) {
                dvui_event_handle(e, wd);
                click = true;
                dvui_refresh(nullptr, DVUI_SRC, wd->id);
            }
        }
    }
    return click;
}

// --- stateFade / hoverFade (DVUI: dvui.stateFade)

float dvui_state_fade(dvui_Id id, const char *key, bool on, float secs) {
    const float target = on ? 1.0f : 0.0f;
    if (secs <= 0.0f) return target;

    // -1 = minule cíl 0, -2 = minule cíl 1, >= 0 = právě probíhá
    float *cur = DVUI_DATA_GET_PTR_DEFAULT(id, key, float, -1.0f);

    if ((*cur == -1 && target == 1) || (*cur == -2 && target == 0)) {
        // začíná nový přechod
        *cur = target == 1 ? 0.0f : 1.0f;
        dvui_refresh(nullptr, DVUI_SRC, id);
        return *cur;
    }

    if (*cur < 0) return target; // žádný přechod neprobíhá

    const float step = dvui_seconds_since_last_frame() / secs;
    *cur = *cur < target ? DVUI_MIN(target, *cur + step) : DVUI_MAX(target, *cur - step);
    if (*cur != target) {
        dvui_refresh(nullptr, DVUI_SRC, id);
        return *cur;
    }
    // přechod skončil
    *cur = target == 0 ? -1.0f : -2.0f;
    return target;
}

float dvui_hover_fade(dvui_Id id, bool hovered) { return dvui_state_fade(id, "__hover_fade", hovered, hover_fade_secs); }

// --- Alokace widgetů (DVUI: widgetAlloc/widgetFree/widgetIsAllocated)

void *dvui_widget_alloc(size_t size) {
    dvui_Window *cw = dvui_current_window();
    void *p = calloc(1, size);
    if (cw->widget_allocs_len == cw->widget_allocs_cap) {
        cw->widget_allocs_cap = cw->widget_allocs_cap ? cw->widget_allocs_cap * 2 : 32;
        cw->widget_allocs = realloc(cw->widget_allocs, cw->widget_allocs_cap * sizeof(void *));
    }
    cw->widget_allocs[cw->widget_allocs_len++] = p;
    return p;
}

bool dvui_widget_is_allocated(const void *ptr) {
    const dvui_Window *cw = dvui_current_window();
    for (size_t i = cw->widget_allocs_len; i > 0; i--) {
        if (cw->widget_allocs[i - 1] == ptr) return true;
    }
    return false;
}

void dvui_widget_free(void *ptr) {
    dvui_Window *cw = dvui_current_window();
    for (size_t i = cw->widget_allocs_len; i > 0; i--) {
        if (cw->widget_allocs[i - 1] == ptr) {
            free(ptr);
            memmove(&cw->widget_allocs[i - 1], &cw->widget_allocs[i], (cw->widget_allocs_len - i) * sizeof(void *));
            cw->widget_allocs_len--;
            return;
        }
    }
}
