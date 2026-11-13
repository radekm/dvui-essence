// Vnitřní struktury dvui-layout (nejsou součástí veřejného API).
#pragma once

#include "dvui/dvui.h"

#include <stdio.h>

// ---------------------------------------------------------------------------
// Logování (DVUI: dvui.log)
// ---------------------------------------------------------------------------

#define DVUI_LOG_ERR(...) (fprintf(stderr, "dvui error: " __VA_ARGS__), fputc('\n', stderr))
#define DVUI_LOG_DEBUG(...) ((void)0)

// Seznam značek, kterým widget rozumí (pro dvui_props_resolve):
//     DVUI_TAGS(DVUI_PROP_dir, DVUI_PROP_equal_space)
#define DVUI_TAGS(...) ((const dvui_PropTag[]){__VA_ARGS__ __VA_OPT__(, ) DVUI_PROP_END})

#define DVUI_MIN(a, b) ((a) < (b) ? (a) : (b))
#define DVUI_MAX(a, b) ((a) > (b) ? (a) : (b))

static inline float dvui_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// ---------------------------------------------------------------------------
// Aréna pro data jednoho snímku (DVUI: Window.arena)
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
// Alokace v aréně aktuálního okna.
void* dvui_frame_alloc(size_t size);
// Zvětší pole v aréně (zkopíruje obsah, staré místo nechá ležet).
void* dvui_frame_grow(void* items, size_t* cap, size_t elem_size, size_t needed);

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
void dvui_map_remove(dvui_Map* m, uint64_t key);
void dvui_map_free(dvui_Map* m);

// ---------------------------------------------------------------------------
// Renderovací příkazy (DVUI: RenderCommand – zde jen obdélníky)
// ---------------------------------------------------------------------------

typedef struct dvui_RenderCommand {
    dvui_RectPhysical rect;
    dvui_Color color;
    dvui_RectPhysical clip;
    float alpha;
    bool snap;
} dvui_RenderCommand;

typedef struct dvui_RenderCmdList {
    dvui_RenderCommand* items;
    size_t len, cap;
} dvui_RenderCmdList;

void dvui_render_cmd_list_append(dvui_RenderCmdList* list, dvui_RenderCommand cmd);
void dvui_render_cmd_list_insert_slice(dvui_RenderCmdList* list, size_t at, const dvui_RenderCommand* items, size_t n);

// ---------------------------------------------------------------------------
// Subwindows (DVUI: Subwindows.zig)
// ---------------------------------------------------------------------------

typedef struct dvui_Subwindow {
    dvui_Id id;
    dvui_Rect rect;
    dvui_RectPhysical rect_pixels;
    dvui_Id focused_widget_id;
    dvui_RenderCmdList render_cmds;
    dvui_RenderCmdList render_cmds_after;
    bool used;
    bool modal;
    dvui_Id stay_above_parent_window;
    bool mouse_events;
} dvui_Subwindow;

typedef struct dvui_Subwindows {
    dvui_Subwindow* stack;
    size_t len, cap;
    dvui_Id current_id;
    dvui_RectNatural current_rect;
    dvui_Id focused_id;
} dvui_Subwindows;

void dvui_subwindows_add(dvui_Subwindows* self, dvui_Id id, dvui_Rect rect, dvui_RectPhysical rect_pixels, bool modal, dvui_Id stay_above_parent_window, bool mouse_events);
dvui_SubwindowCurrentSetReturn dvui_subwindows_set_current(dvui_Subwindows* self, dvui_Id id, const dvui_RectNatural* rect);
bool dvui_subwindows_raise(dvui_Subwindows* self, dvui_Id id);
dvui_Id dvui_subwindows_window_for(const dvui_Subwindows* self, dvui_PointPhysical p);
dvui_Subwindow* dvui_subwindows_get(dvui_Subwindows* self, dvui_Id id);
dvui_Subwindow* dvui_subwindows_current(dvui_Subwindows* self);
dvui_Subwindow* dvui_subwindows_focused(dvui_Subwindows* self);
void dvui_subwindows_reset(dvui_Subwindows* self);

// ---------------------------------------------------------------------------
// Dragging (DVUI: Dragging.zig – jen jeden „prst“, myš)
// ---------------------------------------------------------------------------

typedef enum dvui_DragState {
    DVUI_DRAG_STATE_NONE,
    DVUI_DRAG_STATE_PRESTART,
    DVUI_DRAG_STATE_DRAGGING,
} dvui_DragState;

typedef struct dvui_Dragging {
    dvui_DragState state;
    dvui_Button button;
    dvui_PointPhysical pt;
    dvui_PointPhysical offset;
    const char* name;
    bool has_cursor;
    dvui_Cursor cursor;
} dvui_Dragging;

// ---------------------------------------------------------------------------
// Window (DVUI: Window.zig)
// ---------------------------------------------------------------------------

typedef struct dvui_EventList {
    dvui_Event* items;
    size_t len, cap;
} dvui_EventList;

struct dvui_Window {
    dvui_Backend backend;
    dvui_WidgetData wd;
    dvui_Theme theme;
    bool* open_flag;

    dvui_RectPhysical rect_pixels;
    float natural_scale;
    float content_scale;

    int64_t frame_time_ns;
    float secs_since_last_frame;
    uint8_t extra_frames_needed;

    dvui_Arena arena;
    dvui_Map min_sizes;
    dvui_Map data_store;
    // Bloby nahrazené během snímku; uvolní se na začátku dalšího snímku.
    void** data_trash;
    size_t data_trash_len, data_trash_cap;

    dvui_Subwindows subwindows;

    dvui_EventList events;
    uint16_t event_num;
    dvui_PointPhysical mouse_pt;
    dvui_PointPhysical mouse_pt_prev;
    unsigned modifiers;
    bool inject_motion_event;

    bool has_capture;
    dvui_CaptureMouse capture;
    bool captured_last_frame;

    dvui_Dragging dragging;

    bool has_cursor_requested;
    dvui_Cursor cursor_requested;

    dvui_Widget current_parent;
    dvui_BasicLayout layout;

    dvui_RectPhysical clip_rect;
    float alpha;
    bool snap_to_pixels;
    bool rendering;
    dvui_RenderCmdList* defer_render_cmds;
    dvui_RenderCmdList* defer_render_cmds_after;

    dvui_Id last_focused_id_this_frame;
    dvui_Id last_focused_id_in_subwindow;
    dvui_Id last_registered_id_this_frame;
    bool scroll_to_focused;

    // Widgety alokované přes dvui_widget_alloc (DVUI: _widget_stack).
    void** widget_allocs;
    size_t widget_allocs_len, widget_allocs_cap;

    dvui_Window* previous_window;
};

extern dvui_Window* dvui_current_window_ptr;

void dvui_window_add_render_command(dvui_Window* win, dvui_RenderCommand cmd, bool after);
// DVUI: Window.focusEvents / captureEvents
void dvui_window_focus_events(dvui_Window* win, uint16_t event_num, dvui_Id widget_id);
void dvui_window_capture_events(dvui_Window* win, uint16_t event_num, dvui_Id widget_id);
