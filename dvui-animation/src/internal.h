// Vnitřní struktury dvui-animation (nejsou součástí veřejného API).
#pragma once

#include "dvui/dvui.h"

#include "dvui/data.h"
#include "dvui/util.h"

// Seznam značek, kterým widget rozumí (pro dvui_props_resolve):
//     DVUI_TAGS(DVUI_PROP_dir, DVUI_PROP_equal_space)
#define DVUI_TAGS(...) ((const dvui_PropTag[]){__VA_ARGS__ __VA_OPT__(, ) DVUI_PROP_END})

// ---------------------------------------------------------------------------
// Data snímku (aréna, hash mapa a datové úložiště jsou ve shared – dvui/data.h)
// ---------------------------------------------------------------------------

// Alokace v aréně aktuálního okna.
void* dvui_frame_alloc(size_t size);
// Zvětší pole v aréně (zkopíruje obsah, staré místo nechá ležet).
void* dvui_frame_grow(void* items, size_t* cap, size_t elem_size, size_t needed);

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

    // DVUI: Window.loop_wait_target, loop_wait_target_can_interrupt,
    // loop_target_slop, loop_target_slop_frames, max_fps (waitTime/beginWait)
    bool has_loop_wait_target;
    int64_t loop_wait_target;
    bool loop_wait_target_can_interrupt;
    int32_t loop_target_slop;
    int32_t loop_target_slop_frames;
    float max_fps; // 0 = bez omezení (DVUI null)

    dvui_Arena arena;
    dvui_Map min_sizes;
    // DVUI: Window.animations – id.update(key) → dvui_Animation
    // (TrackingAutoHashMap .get_and_put: `used` nastaví zápis i čtení)
    dvui_Map animations;
    // DVUI: Window.data_store (sdílené – dvui/data.h)
    dvui_DataStore data_store;

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
