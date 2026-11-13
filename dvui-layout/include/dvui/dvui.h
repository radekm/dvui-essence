// dvui-layout – věrný port layoutu z DVUI (https://github.com/david-vanderson/dvui) do C23.
//
// Veřejné API. Struktura odpovídá DVUI: každý widget má strukturu s `wd`
// (WidgetData), funkce `*_init`, `*_deinit` a implementaci rozhraní `dvui_Widget`
// (rect_for, screen_rect_scale, min_size_for_child).
//
// Typické použití (stejné jako v DVUI):
//
//     dvui_window_begin(win, nanoTime);
//     dvui_sdl3gpu_add_all_events(backend, win);
//     dvui_BoxWidget* b = dvui_box(DVUI_SRC, (dvui_BoxInitOptions){0}, (dvui_Options){0});
//     if (dvui_button(DVUI_SRC, (dvui_Size){40, 12}, (dvui_ButtonInitOptions){0}, (dvui_Options){0})) { ... }
//     dvui_box_widget_deinit(b);
//     dvui_window_end(win);
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Základní typy
// ---------------------------------------------------------------------------

// DVUI: dvui.zig – Id. Hodnota 0 slouží zároveň jako „žádné id“ (null).
typedef uint64_t dvui_Id;

// Náhrada za Zig `@src()`. Sloupec nahrazuje `__COUNTER__`, aby dva widgety
// na jednom řádku dostaly různá id (stejně jako v DVUI díky sloupci).
typedef struct dvui_Src {
    const char* file;
    int line;
    int column;
} dvui_Src;

#define DVUI_SRC ((dvui_Src){__FILE__, __LINE__, __COUNTER__})

// DVUI: Point.zig, Size.zig, Rect.zig. DVUI rozlišuje jednotky typem
// (logické, Natural, Physical); v C jsou to aliasy stejného typu.
typedef struct dvui_Point {
    float x, y;
} dvui_Point;

typedef struct dvui_Size {
    float w, h;
} dvui_Size;

typedef struct dvui_Rect {
    float x, y, w, h;
} dvui_Rect;

typedef dvui_Point dvui_PointPhysical;
typedef dvui_Point dvui_PointNatural;
typedef dvui_Size dvui_SizePhysical;
typedef dvui_Size dvui_SizeNatural;
typedef dvui_Rect dvui_RectPhysical;
typedef dvui_Rect dvui_RectNatural;

// DVUI: RectScale.zig – obdélník ve fyzických pixelech a měřítko
// (kolik fyzických pixelů připadá na jeden logický).
typedef struct dvui_RectScale {
    dvui_RectPhysical r;
    float s;
} dvui_RectScale;

// DVUI: Color.zig (bez premultiplikace; PMA se dělá až při vykreslení).
typedef struct dvui_Color {
    uint8_t r, g, b, a;
} dvui_Color;

#define DVUI_RGB(R, G, B) ((dvui_Color){(R), (G), (B), 255})
#define DVUI_RGBA(R, G, B, A) ((dvui_Color){(R), (G), (B), (A)})

// DVUI: dvui.zig – max_float_safe
#define DVUI_MAX_FLOAT_SAFE 2000000.0f

typedef enum dvui_Direction {
    DVUI_VERTICAL = 0,
    DVUI_HORIZONTAL,
} dvui_Direction;

// ---------------------------------------------------------------------------
// Geometrie (DVUI: Rect.zig, Size.zig, Point.zig, RectScale.zig)
// ---------------------------------------------------------------------------

dvui_Rect dvui_rect_all(float v);
dvui_Rect dvui_rect_from_size(dvui_Size s);
dvui_Rect dvui_rect_from_point(dvui_Point p);
dvui_Rect dvui_rect_to_size(dvui_Rect r, dvui_Size s);
dvui_Rect dvui_rect_just_size(dvui_Rect r);
dvui_Size dvui_rect_size(dvui_Rect r);
dvui_Point dvui_rect_top_left(dvui_Rect r);
dvui_Point dvui_rect_top_right(dvui_Rect r);
dvui_Point dvui_rect_bottom_left(dvui_Rect r);
dvui_Point dvui_rect_bottom_right(dvui_Rect r);
dvui_Point dvui_rect_center(dvui_Rect r);
bool dvui_rect_contains(dvui_Rect r, dvui_Point p);
bool dvui_rect_empty(dvui_Rect r);
bool dvui_rect_equals(dvui_Rect a, dvui_Rect b);
bool dvui_rect_non_zero(dvui_Rect r);
dvui_Rect dvui_rect_scale(dvui_Rect r, float s);
dvui_Rect dvui_rect_offset(dvui_Rect r, dvui_Rect by);
dvui_Rect dvui_rect_offset_point(dvui_Rect r, dvui_Point p);
dvui_Rect dvui_rect_offset_neg(dvui_Rect r, dvui_Rect by);
dvui_Rect dvui_rect_offset_neg_point(dvui_Rect r, dvui_Point p);
dvui_Rect dvui_rect_intersect(dvui_Rect a, dvui_Rect b);
dvui_Rect dvui_rect_union(dvui_Rect a, dvui_Rect b);
dvui_Rect dvui_rect_shrink_to_size(dvui_Rect r, dvui_Size s);
dvui_Rect dvui_rect_inset(dvui_Rect r, dvui_Rect by);
dvui_Rect dvui_rect_inset_all(dvui_Rect r, float p);
dvui_Rect dvui_rect_outset(dvui_Rect r, dvui_Rect by);
// DVUI: Rect.Physical.visible – průnik s aktuálním clip obdélníkem není prázdný.
bool dvui_rect_visible(dvui_RectPhysical r);
// DVUI: Rect.Physical.toNatural
dvui_RectNatural dvui_rect_to_natural(dvui_RectPhysical r);

dvui_Size dvui_size_all(float v);
dvui_Size dvui_size_pad(dvui_Size s, dvui_Rect padding);
dvui_Size dvui_size_pad_neg(dvui_Size s, dvui_Rect padding);
dvui_Size dvui_size_max(dvui_Size a, dvui_Size b);
dvui_Size dvui_size_min(dvui_Size a, dvui_Size b);
dvui_Size dvui_size_scale(dvui_Size s, float scale);

dvui_Point dvui_point_plus(dvui_Point a, dvui_Point b);
dvui_Point dvui_point_diff(dvui_Point a, dvui_Point b);
dvui_Point dvui_point_scale(dvui_Point p, float s);
bool dvui_point_non_zero(dvui_Point p);
bool dvui_point_equals(dvui_Point a, dvui_Point b);

dvui_RectScale dvui_rect_scale_rect_to_rect_scale(dvui_RectScale rs, dvui_Rect r);
dvui_RectPhysical dvui_rect_scale_rect_to_physical(dvui_RectScale rs, dvui_Rect r);
dvui_Rect dvui_rect_scale_rect_from_physical(dvui_RectScale rs, dvui_RectPhysical r);
dvui_PointPhysical dvui_rect_scale_point_to_physical(dvui_RectScale rs, dvui_Point p);
dvui_Point dvui_rect_scale_point_from_physical(dvui_RectScale rs, dvui_PointPhysical p);

dvui_Color dvui_color_opacity(dvui_Color c, float mult);
dvui_Color dvui_color_lerp(dvui_Color a, dvui_Color b, float t);
dvui_Color dvui_color_average(dvui_Color a, dvui_Color b);
// DVUI: Color.lighten (HSLuv) – viz odchylky v README.
dvui_Color dvui_color_lighten(dvui_Color c, float delta_l);
dvui_Color dvui_color_from_lightness(float l);

// ---------------------------------------------------------------------------
// Volitelné hodnoty – náhrada Zig `?T`
// ---------------------------------------------------------------------------

#define DVUI_OPT(T, Name) \
    typedef struct Name { \
        bool has; \
        T v; \
    } Name

// Použití: `.margin = DVUI_SOME(dvui_rect_all(4))`
#define DVUI_SOME(...) {.has = true, .v = __VA_ARGS__}

DVUI_OPT(float, dvui_OptF32);
DVUI_OPT(bool, dvui_OptBool);
DVUI_OPT(size_t, dvui_OptUsize);
DVUI_OPT(dvui_Rect, dvui_OptRect);
DVUI_OPT(dvui_Size, dvui_OptSize);
DVUI_OPT(dvui_Point, dvui_OptPoint);
DVUI_OPT(dvui_Color, dvui_OptColor);

// ---------------------------------------------------------------------------
// Options (DVUI: Options.zig)
// ---------------------------------------------------------------------------

typedef enum dvui_Expand {
    DVUI_EXPAND_NONE = 0,
    DVUI_EXPAND_HORIZONTAL,
    DVUI_EXPAND_VERTICAL,
    DVUI_EXPAND_BOTH,
    // Zachovat poměr stran z min. velikosti a roztáhnout co nejvíc.
    DVUI_EXPAND_RATIO,
} dvui_Expand;
DVUI_OPT(dvui_Expand, dvui_OptExpand);

bool dvui_expand_is_horizontal(dvui_Expand e);
bool dvui_expand_is_vertical(dvui_Expand e);

typedef struct dvui_Gravity {
    float x, y;
} dvui_Gravity;

// DVUI: Theme.Style.Name
typedef enum dvui_StyleName {
    DVUI_STYLE_CONTENT = 0,
    DVUI_STYLE_CONTROL,
    DVUI_STYLE_WINDOW,
    DVUI_STYLE_HIGHLIGHT,
    DVUI_STYLE_ERR,
} dvui_StyleName;
DVUI_OPT(dvui_StyleName, dvui_OptStyle);

// DVUI: Options.ColorAsk
typedef enum dvui_ColorAsk {
    DVUI_COLOR_TEXT,
    DVUI_COLOR_TEXT_HOVER,
    DVUI_COLOR_TEXT_PRESS,
    DVUI_COLOR_FILL,
    DVUI_COLOR_FILL_HOVER,
    DVUI_COLOR_FILL_PRESS,
    DVUI_COLOR_BORDER,
} dvui_ColorAsk;

typedef struct dvui_Options {
    // Odliší widgety vytvořené na stejném místě ve zdrojáku (např. ve smyčce).
    dvui_OptUsize id_extra;
    // Jméno pro ladění (NULL = nenastaveno).
    const char* name;

    // Pokud je nastaveno, widget se nevkládá do layoutu rodiče, ale umístí se
    // přímo sem (v souřadnicích rodiče).
    dvui_OptRect rect;
    dvui_OptExpand expand;
    dvui_OptF32 gravity_x;
    dvui_OptF32 gravity_y;

    dvui_OptColor color_fill;
    dvui_OptColor color_fill_hover;
    dvui_OptColor color_fill_press;
    dvui_OptColor color_text;
    dvui_OptColor color_text_hover;
    dvui_OptColor color_text_press;
    dvui_OptColor color_border;

    dvui_OptStyle style;

    // Vnější okraj, rámeček a vnitřní okraj (x = vlevo, y = nahoře,
    // w = vpravo, h = dole).
    dvui_OptRect margin;
    dvui_OptRect border;
    dvui_OptRect padding;

    // Min. a max. velikost obsahu (bez padding/border/margin).
    dvui_OptSize min_size_content;
    dvui_OptSize max_size_content;

    // Kreslit pozadí (výplň) v background rectu.
    dvui_OptBool background;
} dvui_Options;

// DVUI: Options.override – nastavené položky `over` přepíší položky `self`.
dvui_Options dvui_options_override(dvui_Options self, dvui_Options over);
// DVUI: Options.strip – ponechá jen styl (barvy), vynuluje layout.
dvui_Options dvui_options_strip(dvui_Options self);
// DVUI: Options.styleOnly
dvui_Options dvui_options_style_only(dvui_Options self);

size_t dvui_options_id_extra(const dvui_Options* o);
dvui_Expand dvui_options_expand_get(const dvui_Options* o);
dvui_Gravity dvui_options_gravity_get(const dvui_Options* o);
dvui_Rect dvui_options_margin_get(const dvui_Options* o);
dvui_Rect dvui_options_border_get(const dvui_Options* o);
dvui_Rect dvui_options_padding_get(const dvui_Options* o);
bool dvui_options_background_get(const dvui_Options* o);
dvui_StyleName dvui_options_style_get(const dvui_Options* o);
dvui_Size dvui_options_min_size_get(const dvui_Options* o);
dvui_Size dvui_options_min_size_content_get(const dvui_Options* o);
dvui_Size dvui_options_max_size_get(const dvui_Options* o);
dvui_Size dvui_options_max_size_content_get(const dvui_Options* o);
dvui_Size dvui_options_pad_size(const dvui_Options* o, dvui_Size s);
// DVUI: Options.color – barva z Options, jinak z tématu.
dvui_Color dvui_options_color(const dvui_Options* o, dvui_ColorAsk ask);

// ---------------------------------------------------------------------------
// Téma (DVUI: Theme.zig, zjednodušené – viz README)
// ---------------------------------------------------------------------------

typedef struct dvui_ThemeStyle {
    dvui_OptColor fill;
    dvui_OptColor fill_hover;
    dvui_OptColor fill_press;
    dvui_OptColor text;
    dvui_OptColor text_hover;
    dvui_OptColor text_press;
    dvui_OptColor border;
} dvui_ThemeStyle;

typedef struct dvui_Theme {
    const char* name;
    bool dark;
    dvui_Color focus;
    dvui_Color fill;
    dvui_OptColor fill_hover;
    dvui_OptColor fill_press;
    dvui_Color text;
    dvui_OptColor text_hover;
    dvui_OptColor text_press;
    dvui_Color text_select;
    dvui_Color border;
    dvui_ThemeStyle control;
    dvui_ThemeStyle window;
    dvui_ThemeStyle highlight;
    dvui_ThemeStyle err;
} dvui_Theme;

// Výchozí téma podle DVUI Adwaita Light.
dvui_Theme dvui_theme_adwaita_light(void);
// DVUI: Theme.color
dvui_Color dvui_theme_color(const dvui_Theme* t, dvui_StyleName style, dvui_ColorAsk ask);
// DVUI: Theme.adjustColorForState
dvui_Color dvui_theme_adjust_color_for_state(const dvui_Theme* t, dvui_Color col, dvui_ColorAsk ask);

// Velikost písmene „M“ výchozího fontu. DVUI ji počítá z fontu; zde je to
// konstanta, protože dvui-layout nemá text (používá ji grid).
dvui_Size dvui_font_size_m(float wide, float tall);

// ---------------------------------------------------------------------------
// Widget a WidgetData (DVUI: Widget.zig, WidgetData.zig)
// ---------------------------------------------------------------------------

typedef struct dvui_WidgetData dvui_WidgetData;

typedef struct dvui_WidgetVTable {
    dvui_WidgetData* (*data)(void* ptr);
    dvui_Rect (*rect_for)(void* ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g);
    dvui_RectScale (*screen_rect_scale)(void* ptr, dvui_Rect r);
    void (*min_size_for_child)(void* ptr, dvui_Size s);
} dvui_WidgetVTable;

// DVUI: Widget.zig – rozhraní rodiče (ukazatel + vtable).
typedef struct dvui_Widget {
    void* ptr;
    const dvui_WidgetVTable* vtable;
} dvui_Widget;

dvui_WidgetData* dvui_widget_data(dvui_Widget w);
dvui_Id dvui_widget_extend_id(dvui_Widget w, dvui_Src src, size_t id_extra);
dvui_Rect dvui_widget_rect_for(dvui_Widget w, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g);
dvui_RectScale dvui_widget_screen_rect_scale(dvui_Widget w, dvui_Rect r);
void dvui_widget_min_size_for_child(dvui_Widget w, dvui_Size s);

// DVUI: WidgetData.InitOptions
typedef struct dvui_WidgetDataInitOptions {
    // Widget není uvnitř rodiče (floating okna) – rect se nepočítá přes rodiče.
    bool subwindow;
    // DVUI výchozí true; zde se zadává opačně, aby {0} odpovídalo výchozímu stavu.
    bool no_scroll_when_focused;
    dvui_Id id_override;
} dvui_WidgetDataInitOptions;

struct dvui_WidgetData {
    dvui_Id id;
    dvui_Widget parent;
    dvui_WidgetDataInitOptions init_options;
    // Obdélník v souřadnicích rodiče (logické pixely rodiče).
    dvui_Rect rect;
    dvui_Size min_size;
    dvui_Options options;
    dvui_Src src;
    bool has_rect_scale;
    dvui_RectScale rect_scale;
};

dvui_WidgetData dvui_widget_data_init(dvui_Src src, dvui_WidgetDataInitOptions init_options, dvui_Options opts);
void dvui_widget_data_rect_set(dvui_WidgetData* wd, dvui_Rect r);
void dvui_widget_data_register(dvui_WidgetData* wd);
bool dvui_widget_data_visible(const dvui_WidgetData* wd);

typedef struct dvui_BorderAndBackgroundOptions {
    dvui_OptColor fill_color;
} dvui_BorderAndBackgroundOptions;
void dvui_widget_data_border_and_background(const dvui_WidgetData* wd, dvui_BorderAndBackgroundOptions opts);
void dvui_widget_data_focus_border(const dvui_WidgetData* wd);

dvui_RectScale dvui_widget_data_rect_scale_from_parent(const dvui_WidgetData* wd);
dvui_RectScale dvui_widget_data_rect_scale(const dvui_WidgetData* wd);
dvui_Rect dvui_widget_data_border_rect(const dvui_WidgetData* wd);
dvui_RectScale dvui_widget_data_border_rect_scale(const dvui_WidgetData* wd);
dvui_Rect dvui_widget_data_background_rect(const dvui_WidgetData* wd);
dvui_RectScale dvui_widget_data_background_rect_scale(const dvui_WidgetData* wd);
dvui_Rect dvui_widget_data_content_rect(const dvui_WidgetData* wd);
dvui_RectScale dvui_widget_data_content_rect_scale(const dvui_WidgetData* wd);
void dvui_widget_data_min_size_max(dvui_WidgetData* wd, dvui_Size s);
void dvui_widget_data_min_size_set_and_refresh(dvui_WidgetData* wd);
void dvui_widget_data_min_size_report_to_parent(const dvui_WidgetData* wd);
bool dvui_widget_data_is_root(const dvui_WidgetData* wd);

// ---------------------------------------------------------------------------
// Layout pomocníci (DVUI: layout.zig)
// ---------------------------------------------------------------------------

// DVUI: dvui.placeIn – umístí obdélník o min. velikosti do `avail`.
dvui_Rect dvui_place_in(dvui_Rect avail, dvui_Size min_size, dvui_Expand e, dvui_Gravity g);

// DVUI: layout.BasicLayout – skládá děti pod sebe nebo vedle sebe.
typedef struct dvui_BasicLayout {
    dvui_Direction dir;
    float pos;
    bool seen_expanded;
    dvui_Size min_size_children;
} dvui_BasicLayout;

dvui_Rect dvui_basic_layout_rect_for(dvui_BasicLayout* self, dvui_Rect content_rect, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g);
dvui_Size dvui_basic_layout_min_size_for_child(dvui_BasicLayout* self, dvui_Size s);

// DVUI: layout.PlaceOnScreenAvoid
typedef enum dvui_PlaceOnScreenAvoid {
    DVUI_AVOID_NONE = 0,
    DVUI_AVOID_HORIZONTAL,
    DVUI_AVOID_VERTICAL,
} dvui_PlaceOnScreenAvoid;

dvui_RectNatural dvui_place_on_screen(dvui_RectNatural screen, dvui_RectNatural spawner, dvui_PlaceOnScreenAvoid avoid, dvui_RectNatural start);

// ---------------------------------------------------------------------------
// Události (DVUI: Event.zig, enums.zig)
// ---------------------------------------------------------------------------

typedef enum dvui_Button {
    DVUI_BUTTON_NONE = 0,
    DVUI_BUTTON_LEFT,
    DVUI_BUTTON_RIGHT,
    DVUI_BUTTON_MIDDLE,
    DVUI_BUTTON_FOUR,
    DVUI_BUTTON_FIVE,
} dvui_Button;

// DVUI: enums.Button.pointer – levé tlačítko nebo dotyk (dotyk zde není).
bool dvui_button_pointer(dvui_Button b);

typedef enum dvui_Mod {
    DVUI_MOD_NONE = 0,
    DVUI_MOD_LSHIFT = 1 << 0,
    DVUI_MOD_RSHIFT = 1 << 1,
    DVUI_MOD_LCONTROL = 1 << 2,
    DVUI_MOD_RCONTROL = 1 << 3,
    DVUI_MOD_LALT = 1 << 4,
    DVUI_MOD_RALT = 1 << 5,
    DVUI_MOD_LCOMMAND = 1 << 6,
    DVUI_MOD_RCOMMAND = 1 << 7,
} dvui_Mod;

bool dvui_mod_shift(unsigned mod);
bool dvui_mod_shift_only(unsigned mod);

typedef enum dvui_MouseActionKind {
    // Přichází těsně před press; widget si podle ní bere fokus.
    DVUI_MOUSE_FOCUS,
    DVUI_MOUSE_PRESS,
    DVUI_MOUSE_RELEASE,
    // Pohyb myši (motion = posun ve fyzických pixelech).
    DVUI_MOUSE_MOTION,
    // Jedna událost na konci každého snímku s aktuální pozicí (hover).
    DVUI_MOUSE_POSITION,
    DVUI_MOUSE_WHEEL_X,
    DVUI_MOUSE_WHEEL_Y,
} dvui_MouseActionKind;

typedef struct dvui_EventMouse {
    dvui_MouseActionKind action;
    dvui_PointPhysical motion; // jen pro DVUI_MOUSE_MOTION
    float wheel;               // jen pro DVUI_MOUSE_WHEEL_*
    dvui_Button button;
    unsigned mod;
    dvui_PointPhysical p;
    // Subwindow, které je pod myší (nebo modální).
    dvui_Id floating_win;
} dvui_EventMouse;

typedef enum dvui_Key {
    DVUI_KEY_UNKNOWN = 0,
    DVUI_KEY_ENTER,
    DVUI_KEY_SPACE,
    DVUI_KEY_ESCAPE,
    DVUI_KEY_TAB,
    DVUI_KEY_UP,
    DVUI_KEY_DOWN,
    DVUI_KEY_LEFT,
    DVUI_KEY_RIGHT,
    DVUI_KEY_PAGE_UP,
    DVUI_KEY_PAGE_DOWN,
    DVUI_KEY_HOME,
    DVUI_KEY_END,
} dvui_Key;

typedef enum dvui_KeyAction {
    DVUI_KEY_ACTION_DOWN,
    DVUI_KEY_ACTION_REPEAT,
    DVUI_KEY_ACTION_UP,
} dvui_KeyAction;

typedef struct dvui_EventKey {
    dvui_Key code;
    dvui_KeyAction action;
    unsigned mod;
} dvui_EventKey;

typedef enum dvui_WindowAction {
    DVUI_WINDOW_FOCUS,
    DVUI_WINDOW_CLOSE,
    DVUI_WINDOW_LEAVE,
} dvui_WindowAction;

typedef enum dvui_EventKind {
    DVUI_EVENT_MOUSE,
    DVUI_EVENT_KEY,
    DVUI_EVENT_WINDOW,
    DVUI_EVENT_APP_QUIT,
} dvui_EventKind;

typedef struct dvui_Event {
    bool handled;
    // Pro klávesy fokus, pro myš capture (0 = nikdo).
    dvui_Id target_widget_id;
    uint16_t num;
    dvui_EventKind kind;
    union {
        dvui_EventMouse mouse;
        dvui_EventKey key;
        dvui_WindowAction window;
    } evt;
} dvui_Event;

// DVUI: Event.handle
void dvui_event_handle(dvui_Event* e, const dvui_WidgetData* wd);

// DVUI: dvui.events – události aktuálního snímku.
dvui_Event* dvui_events(size_t* count);

typedef struct dvui_EventMatchOptions {
    dvui_Id id;
    dvui_Id focus_id;
    dvui_RectPhysical r;
    const char* drag_name;
} dvui_EventMatchOptions;

// DVUI: dvui.eventMatch – má widget zpracovat událost `e`?
bool dvui_event_match(dvui_Event* e, dvui_EventMatchOptions opts);
// DVUI: dvui.eventMatchSimple
bool dvui_event_match_simple(dvui_Event* e, dvui_WidgetData* wd);

typedef enum dvui_Cursor {
    DVUI_CURSOR_ARROW = 0,
    DVUI_CURSOR_HAND,
    DVUI_CURSOR_ARROW_ALL,
    DVUI_CURSOR_ARROW_NW_SE,
    DVUI_CURSOR_ARROW_NE_SW,
    DVUI_CURSOR_ARROW_N_S,
    DVUI_CURSOR_ARROW_W_E,
} dvui_Cursor;

typedef struct dvui_ClickOptions {
    bool* hovered;
    // DVUI výchozí .hand; zde `no_hover_cursor` vypne nastavení kurzoru.
    bool no_hover_cursor;
    dvui_OptRect rect;
} dvui_ClickOptions;

// DVUI: dvui.clicked
bool dvui_clicked(const dvui_WidgetData* wd, dvui_ClickOptions opts);

// ---------------------------------------------------------------------------
// Backend (DVUI: Backend.zig)
// ---------------------------------------------------------------------------

// DVUI: Vertex.zig – pozice ve fyzických pixelech, barva premultiplikovaná.
typedef struct dvui_Vertex {
    dvui_PointPhysical pos;
    dvui_Color col;
    float uv[2];
} dvui_Vertex;

typedef struct dvui_Backend {
    void* ctx;
    void (*begin)(void* ctx);
    void (*end)(void* ctx);
    dvui_SizePhysical (*pixel_size)(void* ctx);
    dvui_SizeNatural (*window_size)(void* ctx);
    float (*content_scale)(void* ctx);
    // `clip` může být NULL (bez ořezu).
    void (*draw_clipped_triangles)(void* ctx, const dvui_Vertex* vtx, size_t vtx_count, const uint32_t* idx, size_t idx_count, const dvui_RectPhysical* clip);
    void (*set_cursor)(void* ctx, dvui_Cursor cursor);
    void (*render_present)(void* ctx);
    int64_t (*nano_time)(void* ctx);
} dvui_Backend;

// ---------------------------------------------------------------------------
// Window (DVUI: Window.zig)
// ---------------------------------------------------------------------------

typedef struct dvui_Window dvui_Window;

dvui_Window* dvui_window_create(dvui_Backend backend);
void dvui_window_destroy(dvui_Window* win);
void dvui_window_theme_set(dvui_Window* win, dvui_Theme theme);
// Ukazatel na proměnnou, do které se při zavření okna zapíše false.
void dvui_window_open_flag_set(dvui_Window* win, bool* open_flag);

// DVUI: Window.begin
void dvui_window_begin(dvui_Window* win, int64_t time_ns);
// DVUI: Window.end – vrací, za kolik mikrosekund chce další snímek
// (0 = hned, -1 = čekat na událost).
int64_t dvui_window_end(dvui_Window* win);

// Přidávání událostí (DVUI: Window.addEvent*). Volat po begin, před widgety.
// Vrací true, pokud je událost nad floating oknem.
bool dvui_window_add_event_mouse_motion(dvui_Window* win, dvui_PointPhysical pt);
bool dvui_window_add_event_mouse_button(dvui_Window* win, dvui_Button b, dvui_MouseActionKind action);
bool dvui_window_add_event_mouse_wheel(dvui_Window* win, float ticks, dvui_Direction dir);
bool dvui_window_add_event_key(dvui_Window* win, dvui_Key code, dvui_KeyAction action, unsigned mod);
void dvui_window_add_event_window(dvui_Window* win, dvui_WindowAction action);
void dvui_window_add_event_app_quit(dvui_Window* win);
void dvui_window_modifiers_set(dvui_Window* win, unsigned mod);

// DVUI: Window.cursorRequested
dvui_Cursor dvui_window_cursor_requested(const dvui_Window* win);

// Rychlost kolečka myši (DVUI: dvui.scroll_speed).
extern float dvui_scroll_speed;

// ---------------------------------------------------------------------------
// Globální funkce platné mezi begin a end (DVUI: dvui.zig)
// ---------------------------------------------------------------------------

dvui_Window* dvui_current_window(void);
dvui_Id dvui_id_extend(dvui_Id start, dvui_Src src, size_t id_extra);
dvui_Id dvui_id_update(dvui_Id id, const char* name);

dvui_Widget dvui_parent_get(void);
void dvui_parent_set(dvui_Widget w);
void dvui_parent_reset(dvui_Id id, dvui_Widget prev_parent);

void dvui_refresh(dvui_Window* win, dvui_Src src, dvui_Id id);
bool dvui_first_frame(dvui_Id id);
// Min. velikost z minulého snímku; false, pokud widget minule nebyl.
bool dvui_min_size_get(dvui_Id id, dvui_Size* out);

dvui_RectNatural dvui_window_rect(void);
dvui_RectPhysical dvui_window_rect_pixels(void);
dvui_RectScale dvui_window_rect_scale(void);
float dvui_window_natural_scale(void);
float dvui_seconds_since_last_frame(void);
const dvui_Theme* dvui_theme_get(void);

dvui_RectPhysical dvui_clip_get(void);
dvui_RectPhysical dvui_clip(dvui_RectPhysical r);
void dvui_clip_set(dvui_RectPhysical r);
float dvui_alpha(float mult);
void dvui_alpha_set(float a);
bool dvui_snap_to_pixels(void);

// Datové úložiště (DVUI: dvui.dataGet/dataSet …). Hodnoty, které se během
// snímku nepoužijí, se na začátku dalšího snímku zahodí.
void dvui_data_set(dvui_Window* win, dvui_Id id, const char* key, const void* value, size_t size);
// Vrací ukazatel na uloženou hodnotu, nebo NULL. Ukazatel platí do dalšího
// dvui_data_set se stejným klíčem a jinou velikostí.
void* dvui_data_get(dvui_Window* win, dvui_Id id, const char* key, size_t size);
void* dvui_data_get_ptr_default(dvui_Window* win, dvui_Id id, const char* key, size_t size, const void* def);
// Pro pole proměnné délky (DVUI: dataGetSlice); `*size` je délka v bajtech.
void* dvui_data_get_slice(dvui_Window* win, dvui_Id id, const char* key, size_t* size);
void dvui_data_remove(dvui_Window* win, dvui_Id id, const char* key);

#define DVUI_DATA_GET(id, key, T) ((T*)dvui_data_get(nullptr, (id), (key), sizeof(T)))
#define DVUI_DATA_SET(id, key, T, val) \
    do { \
        T dvui_tmp_ = (val); \
        dvui_data_set(nullptr, (id), (key), &dvui_tmp_, sizeof(T)); \
    } while (0)
#define DVUI_DATA_GET_PTR_DEFAULT(id, key, T, def) \
    ((T*)dvui_data_get_ptr_default(nullptr, (id), (key), sizeof(T), (T[1]){def}))

// Subwindows a fokus (DVUI: dvui.zig)
typedef struct dvui_SubwindowCurrentSetReturn {
    dvui_Id id;
    dvui_RectNatural rect;
} dvui_SubwindowCurrentSetReturn;

void dvui_subwindow_add(dvui_Id id, dvui_Rect rect, dvui_RectPhysical rect_pixels, bool modal, dvui_Id stay_above_parent_window, bool mouse_events);
dvui_SubwindowCurrentSetReturn dvui_subwindow_current_set(dvui_Id id, const dvui_RectNatural* rect);
dvui_Id dvui_subwindow_current_id(void);
void dvui_focus_subwindow(dvui_Id subwindow_id, const uint16_t* event_num);
void dvui_raise_subwindow(dvui_Id subwindow_id);
dvui_Id dvui_focused_subwindow_id(void);
void dvui_focus_widget(dvui_Id id, dvui_Id subwindow_id, const uint16_t* event_num);
dvui_Id dvui_focused_widget_id(void);
dvui_Id dvui_focused_widget_id_in_current_subwindow(void);
dvui_Id dvui_last_focused_id_in_frame(void);
dvui_Id dvui_last_focused_id_in_frame_since(dvui_Id prev);
void dvui_cursor_set(dvui_Cursor cursor);

// Capture myši (DVUI: dvui.captureMouse …)
typedef struct dvui_CaptureMouse {
    dvui_Id id;
    dvui_RectPhysical rect;
    dvui_Id subwindow_id;
} dvui_CaptureMouse;

void dvui_capture_mouse(const dvui_WidgetData* wd, uint16_t event_num);
void dvui_capture_mouse_maintain(dvui_CaptureMouse cm);
bool dvui_captured(dvui_Id id);

// Tažení myší (DVUI: Dragging.zig)
typedef struct dvui_DragStartOptions {
    dvui_Cursor cursor;
    bool has_cursor;
    dvui_PointPhysical offset;
    const char* name;
} dvui_DragStartOptions;

void dvui_drag_pre_start(dvui_Button b, dvui_PointPhysical p, dvui_DragStartOptions options);
void dvui_drag_start(dvui_Button b, dvui_PointPhysical p, dvui_DragStartOptions options);
dvui_PointPhysical dvui_drag_offset(dvui_Button b);
// Vrací true a posun, pokud probíhá tažení.
bool dvui_dragging(dvui_EventMouse me, const char* name, dvui_PointPhysical* out);
void dvui_drag_end(dvui_Button b);

// DVUI: dvui.stateFade / hoverFade
float dvui_state_fade(dvui_Id id, const char* key, bool on, float secs);
float dvui_hover_fade(dvui_Id id, bool hovered);

// Alokace widgetů pro „pohodlné“ funkce (DVUI: widgetAlloc/widgetFree).
void* dvui_widget_alloc(size_t size);
void dvui_widget_free(void* ptr);
bool dvui_widget_is_allocated(const void* ptr);

// Kreslení (náhrada DVUI Path/Triangles pro obdélníky s ostrými rohy).
void dvui_render_fill_rect(dvui_RectPhysical r, dvui_Color color, bool after);
void dvui_render_stroke_rect(dvui_RectPhysical r, float thickness, dvui_Color color, bool after);

// DVUI: dvui.RenderFrontToBack
typedef struct dvui_RenderFrontToBack {
    void* render_cmds;
    void* render_cmds_after;
    bool prev_rendering;
    void* prev_dr_cmds;
    void* prev_dr_cmds_after;
} dvui_RenderFrontToBack;
void dvui_render_front_to_back_init_reset(dvui_RenderFrontToBack* self);
void dvui_render_front_to_back_deinit(dvui_RenderFrontToBack* self);

// Scrollování (DVUI: dvui.scrollTo / scrollDrag)
typedef struct dvui_ScrollToOptions {
    dvui_RectPhysical screen_rect;
    bool over_scroll;
} dvui_ScrollToOptions;
void dvui_scroll_to(dvui_ScrollToOptions st);

typedef struct dvui_ScrollDragOptions {
    dvui_PointPhysical mouse_pt;
    dvui_RectPhysical screen_rect;
} dvui_ScrollDragOptions;
void dvui_scroll_drag(dvui_ScrollDragOptions sd);

// ---------------------------------------------------------------------------
// Widgety
// ---------------------------------------------------------------------------

// --- Spacer, Separator, Label (DVUI: dvui.spacer, dvui.separator, LabelWidget)

// Prázdný widget zabírající místo (min_size_content).
dvui_WidgetData dvui_spacer(dvui_Src src, dvui_Options opts);
// Tenká čára (min_size_content 1×1, barva okraje tématu).
dvui_WidgetData dvui_separator(dvui_Src src, dvui_Options opts);
// Zástupce za LabelWidget: chová se v layoutu jako popisek s textem velikosti
// `text_size` (výchozí padding 6), ale text nekreslí.
dvui_WidgetData dvui_label_placeholder(dvui_Src src, dvui_Size text_size, dvui_Options opts);

// --- Overlay (DVUI: OverlayWidget.zig)

typedef struct dvui_OverlayWidget {
    dvui_WidgetData wd;
} dvui_OverlayWidget;

void dvui_overlay_widget_init(dvui_OverlayWidget* self, dvui_Src src, dvui_Options opts);
void dvui_overlay_widget_draw_background(dvui_OverlayWidget* self);
dvui_Widget dvui_overlay_widget_widget(dvui_OverlayWidget* self);
void dvui_overlay_widget_deinit(dvui_OverlayWidget* self);
dvui_OverlayWidget* dvui_overlay(dvui_Src src, dvui_Options opts);

// --- Box (DVUI: BoxWidget.zig)

typedef struct dvui_BoxInitOptions {
    dvui_Direction dir;
    bool equal_space;
    // Přepíše počet roztažených dětí z minulého snímku.
    bool has_num_packed_expanded;
    uint32_t num_packed_expanded;
} dvui_BoxInitOptions;

typedef struct dvui_BoxData {
    float packed_children;
    float total_weight;
    float min_space_taken;
} dvui_BoxData;

typedef struct dvui_BoxWidget {
    dvui_WidgetData wd;
    dvui_BoxInitOptions init_opts;
    bool first_child;
    float max_space;
    float max_thick;
    bool has_data_prev;
    dvui_BoxData data_prev;
    float min_space_taken;
    float packed_children;
    float total_weight;
    dvui_Rect child_rect;
    bool child_positioned;
    dvui_Id child_id;
    float ratio_extra;
    bool ran_off;
    float pixels_per_w;
} dvui_BoxWidget;

void dvui_box_widget_init(dvui_BoxWidget* self, dvui_Src src, dvui_BoxInitOptions init_options, dvui_Options opts);
void dvui_box_widget_draw_background(dvui_BoxWidget* self);
dvui_Widget dvui_box_widget_widget(dvui_BoxWidget* self);
dvui_WidgetData* dvui_box_widget_data(dvui_BoxWidget* self);
dvui_Rect dvui_box_widget_rect_for(dvui_BoxWidget* self, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g);
dvui_RectScale dvui_box_widget_screen_rect_scale(dvui_BoxWidget* self, dvui_Rect rect);
void dvui_box_widget_min_size_for_child(dvui_BoxWidget* self, dvui_Size s);
void dvui_box_widget_deinit(dvui_BoxWidget* self);
// DVUI: dvui.box – alokuje widget, zavolá init a drawBackground.
dvui_BoxWidget* dvui_box(dvui_Src src, dvui_BoxInitOptions init_opts, dvui_Options opts);

// --- Button (DVUI: ButtonWidget.zig)

typedef struct dvui_ButtonInitOptions {
    // DVUI výchozí draw_focus = true; zde opačně kvůli {0}.
    bool no_draw_focus;
    bool grayed;
} dvui_ButtonInitOptions;

typedef struct dvui_ButtonWidget {
    dvui_WidgetData wd;
    dvui_ButtonInitOptions init_options;
    bool hover;
    bool focus;
    bool click;
} dvui_ButtonWidget;

extern dvui_Options dvui_button_widget_defaults;

void dvui_button_widget_init(dvui_ButtonWidget* self, dvui_Src src, dvui_ButtonInitOptions init_options, dvui_Options opts);
void dvui_button_widget_process_events(dvui_ButtonWidget* self);
void dvui_button_widget_draw_background(dvui_ButtonWidget* self);
void dvui_button_widget_draw_focus(dvui_ButtonWidget* self);
dvui_Options dvui_button_widget_style(dvui_ButtonWidget* self);
bool dvui_button_widget_focused(dvui_ButtonWidget* self);
bool dvui_button_widget_hovered(dvui_ButtonWidget* self);
bool dvui_button_widget_pressed(dvui_ButtonWidget* self);
bool dvui_button_widget_clicked(dvui_ButtonWidget* self);
dvui_Widget dvui_button_widget_widget(dvui_ButtonWidget* self);
dvui_WidgetData* dvui_button_widget_data(dvui_ButtonWidget* self);
void dvui_button_widget_deinit(dvui_ButtonWidget* self);
// DVUI: dvui.button – tlačítko bez textu; místo popisku je zástupce o velikosti
// `label_size`. Vrací true, pokud bylo v tomto snímku kliknuto.
bool dvui_button(dvui_Src src, dvui_Size label_size, dvui_ButtonInitOptions init_opts, dvui_Options opts);

// --- Scale (DVUI: ScaleWidget.zig)

typedef struct dvui_ScaleWidget {
    dvui_WidgetData wd;
    float* scale;
    dvui_BasicLayout layout;
} dvui_ScaleWidget;

void dvui_scale_widget_init(dvui_ScaleWidget* self, dvui_Src src, float* scale, dvui_Options opts);
dvui_Widget dvui_scale_widget_widget(dvui_ScaleWidget* self);
void dvui_scale_widget_deinit(dvui_ScaleWidget* self);

// --- ScrollInfo (DVUI: ScrollInfo.zig)

typedef enum dvui_ScrollMode {
    DVUI_SCROLL_NONE = 0,
    DVUI_SCROLL_AUTO,
    DVUI_SCROLL_GIVEN,
} dvui_ScrollMode;
DVUI_OPT(dvui_ScrollMode, dvui_OptScrollMode);

typedef enum dvui_ScrollBarMode {
    DVUI_SCROLL_BAR_AUTO = 0,
    DVUI_SCROLL_BAR_HIDE,
    DVUI_SCROLL_BAR_AUTO_OVERLAY,
    DVUI_SCROLL_BAR_SHOW,
} dvui_ScrollBarMode;

bool dvui_scroll_bar_mode_auto_any(dvui_ScrollBarMode m);

typedef struct dvui_ScrollInfo {
    dvui_ScrollMode vertical;
    dvui_ScrollMode horizontal;
    dvui_Size virtual_size;
    dvui_Rect viewport;
    dvui_Point velocity;
} dvui_ScrollInfo;

// DVUI výchozí: vertical = .auto, horizontal = .none
dvui_ScrollInfo dvui_scroll_info_default(void);
float dvui_scroll_info_scroll_max(const dvui_ScrollInfo* si, dvui_Direction dir);
float dvui_scroll_info_visible_fraction(const dvui_ScrollInfo* si, dvui_Direction dir);
float dvui_scroll_info_offset(const dvui_ScrollInfo* si, dvui_Direction dir);
float dvui_scroll_info_offset_from_max(const dvui_ScrollInfo* si, dvui_Direction dir);
float dvui_scroll_info_offset_fraction(const dvui_ScrollInfo* si, dvui_Direction dir);
void dvui_scroll_info_scroll_by_offset(dvui_ScrollInfo* si, dvui_Direction dir, float off);
void dvui_scroll_info_scroll_to_offset(dvui_ScrollInfo* si, dvui_Direction dir, float off);
void dvui_scroll_info_scroll_to_fraction(dvui_ScrollInfo* si, dvui_Direction dir, float fin);
void dvui_scroll_info_scroll_page(dvui_ScrollInfo* si, dvui_Direction dir, bool up);
void dvui_scroll_info_scroll_page_up(dvui_ScrollInfo* si, dvui_Direction dir);
void dvui_scroll_info_scroll_page_down(dvui_ScrollInfo* si, dvui_Direction dir);

// --- ScrollBar (DVUI: ScrollBarWidget.zig)

typedef struct dvui_ScrollBarInitOptions {
    dvui_ScrollInfo* scroll_info;
    dvui_Direction direction;
    dvui_Id focus_id;
} dvui_ScrollBarInitOptions;

typedef struct dvui_ScrollBarWidget {
    dvui_WidgetData wd;
    dvui_Rect grab_rect;
    dvui_ScrollInfo* si;
    dvui_Id focus_id;
    dvui_Direction dir;
    bool highlight;
} dvui_ScrollBarWidget;

// DVUI: ScrollBarWidget.Grab
typedef struct dvui_ScrollBarGrab {
    dvui_RectPhysical rect;
    dvui_Color color;
} dvui_ScrollBarGrab;

extern dvui_Options dvui_scroll_bar_widget_defaults;

void dvui_scroll_bar_widget_init(dvui_ScrollBarWidget* self, dvui_Src src, dvui_ScrollBarInitOptions init_opts, dvui_Options opts);
dvui_ScrollBarGrab dvui_scroll_bar_widget_grab(dvui_ScrollBarWidget* self);
void dvui_scroll_bar_grab_draw(dvui_ScrollBarGrab g);
void dvui_scroll_bar_widget_deinit(dvui_ScrollBarWidget* self);

// --- ScrollContainer (DVUI: ScrollContainerWidget.zig)

typedef struct dvui_ScrollAreaWidget dvui_ScrollAreaWidget;

typedef struct dvui_ScrollContainerInitOptions {
    dvui_ScrollAreaWidget* scroll_area;
    dvui_OptPoint frame_viewport;
    bool lock_visible;
    dvui_OptRect event_rect;
    // DVUI výchozí process_events_after = true; zde opačně kvůli {0}.
    bool no_process_events_after;
    dvui_Point* user_scroll;
} dvui_ScrollContainerInitOptions;

typedef struct dvui_ScrollContainerWidget dvui_ScrollContainerWidget;
struct dvui_ScrollContainerWidget {
    dvui_WidgetData wd;
    dvui_ScrollInfo* si;
    dvui_ScrollContainerInitOptions init_opts;
    dvui_Id last_focus;
    dvui_ScrollContainerWidget* parent_scroll;
    dvui_Id subwindow_id;
    dvui_Point frame_viewport;
    dvui_RectPhysical prev_clip;
    dvui_Size next_virtual_size;
    bool seen_expanded_child;
    bool lock_visible;
    dvui_Id first_visible_id;
    dvui_Point first_visible_offset;
    dvui_Point first_visible_viewport;
    bool seen_scroll_drag;
};

dvui_ScrollContainerWidget* dvui_scroll_container_current(void);
dvui_ScrollContainerWidget* dvui_scroll_container_scroll_set(dvui_ScrollContainerWidget* scroll);

void dvui_scroll_container_widget_init(dvui_ScrollContainerWidget* self, dvui_Src src, dvui_ScrollInfo* io_scroll_info, dvui_ScrollContainerInitOptions init_options, dvui_Options opts);
bool dvui_scroll_container_widget_match_event(dvui_ScrollContainerWidget* self, dvui_Event* e);
void dvui_scroll_container_widget_process_events(dvui_ScrollContainerWidget* self);
void dvui_scroll_container_widget_process_velocity_after(dvui_ScrollContainerWidget* self);
dvui_Widget dvui_scroll_container_widget_widget(dvui_ScrollContainerWidget* self);
dvui_WidgetData* dvui_scroll_container_widget_data(dvui_ScrollContainerWidget* self);
dvui_Point dvui_scroll_container_widget_point_from_physical(dvui_ScrollContainerWidget* self, dvui_PointPhysical p);
dvui_RectScale dvui_scroll_container_widget_screen_rect_scale(dvui_ScrollContainerWidget* self, dvui_Rect rect);
void dvui_scroll_container_widget_min_size_for_child(dvui_ScrollContainerWidget* self, dvui_Size s);
void dvui_scroll_container_widget_process_scroll_drag(dvui_ScrollContainerWidget* self, dvui_ScrollDragOptions sd);
void dvui_scroll_container_widget_process_scroll_to(dvui_ScrollContainerWidget* self, dvui_ScrollToOptions st);
void dvui_scroll_container_widget_process_events_after(dvui_ScrollContainerWidget* self);
void dvui_scroll_container_widget_process_event_after(dvui_ScrollContainerWidget* self, dvui_Event* e);
void dvui_scroll_container_widget_deinit(dvui_ScrollContainerWidget* self);

// --- ScrollArea (DVUI: ScrollAreaWidget.zig)

typedef struct dvui_ScrollAreaInitOptions {
    dvui_ScrollInfo* scroll_info;
    dvui_OptScrollMode vertical;   // výchozí .auto
    dvui_ScrollBarMode vertical_bar;
    dvui_OptScrollMode horizontal; // výchozí .none
    dvui_ScrollBarMode horizontal_bar;
    dvui_Id focus_id;
    dvui_OptPoint frame_viewport;
    dvui_Point* frame_viewport_out;
    bool lock_visible;
    // DVUI výchozí process_events_after = true a container = true; zde
    // opačně kvůli {0}.
    bool no_process_events_after;
    bool no_container;
    dvui_Point* user_scroll;
} dvui_ScrollAreaInitOptions;

struct dvui_ScrollAreaWidget {
    // `si` je první, aby adresa hbox nebyla shodná s adresou celé struktury
    // (dvui_widget_is_allocated by jinak hbox považoval za alokovaný).
    dvui_ScrollInfo* si;
    dvui_BoxWidget hbox;
    bool has_vbar_grab;
    dvui_ScrollBarGrab vbar_grab;
    dvui_BoxWidget vbox;
    bool has_hbar_grab;
    dvui_ScrollBarGrab hbar_grab;
    bool has_scroll;
    dvui_ScrollContainerWidget scroll;
};

extern dvui_Options dvui_scroll_area_widget_defaults;

void dvui_scroll_area_widget_init(dvui_ScrollAreaWidget* self, dvui_Src src, dvui_ScrollAreaInitOptions init_opts, dvui_Options opts);
dvui_WidgetData* dvui_scroll_area_widget_data(dvui_ScrollAreaWidget* self);
void dvui_scroll_area_widget_set_container_rect(dvui_ScrollAreaWidget* self, dvui_Rect rect);
void dvui_scroll_area_widget_deinit(dvui_ScrollAreaWidget* self);
dvui_ScrollAreaWidget* dvui_scroll_area(dvui_Src src, dvui_ScrollAreaInitOptions init_opts, dvui_Options opts);

// --- Floating (DVUI: FloatingWidget.zig)

typedef struct dvui_FloatingInitOptions {
    // DVUI výchozí mouse_events = true a stay_above_parent = true; zde opačně.
    bool no_mouse_events;
    dvui_OptPoint from;
    float from_gravity_x; // DVUI výchozí 0.5 – použije se, pokud has_from_gravity
    float from_gravity_y;
    bool has_from_gravity;
    bool no_stay_above_parent;
} dvui_FloatingInitOptions;

typedef struct dvui_FloatingWidget {
    dvui_FloatingInitOptions init_opts;
    dvui_RenderFrontToBack render_ftb;
    dvui_WidgetData wd;
    dvui_SubwindowCurrentSetReturn prev_window_info;
    dvui_ScrollContainerWidget* prev_scroll;
    dvui_RectPhysical prev_clip;
    float scale_val;
    dvui_ScaleWidget scaler;
} dvui_FloatingWidget;

void dvui_floating_widget_init(dvui_FloatingWidget* self, dvui_Src src, dvui_FloatingInitOptions init_opts, dvui_Options opts);
dvui_Widget dvui_floating_widget_widget(dvui_FloatingWidget* self);
void dvui_floating_widget_deinit(dvui_FloatingWidget* self);

// --- FloatingTooltip (DVUI: FloatingTooltipWidget.zig)

typedef enum dvui_TooltipPosition {
    DVUI_TOOLTIP_HORIZONTAL = 0,
    DVUI_TOOLTIP_VERTICAL,
    DVUI_TOOLTIP_STICKY,
    DVUI_TOOLTIP_ABSOLUTE,
} dvui_TooltipPosition;

typedef struct dvui_FloatingTooltipInitOptions {
    // Tooltip se ukáže, když je myš v tomto fyzickém obdélníku.
    dvui_RectPhysical active_rect;
    dvui_TooltipPosition position;
    bool interactive;
    dvui_OptF32 scale;
} dvui_FloatingTooltipInitOptions;

typedef struct dvui_FloatingTooltipWidget dvui_FloatingTooltipWidget;
struct dvui_FloatingTooltipWidget {
    dvui_FloatingTooltipWidget* parent_tooltip;
    dvui_RenderFrontToBack render_ftb;
    dvui_WidgetData wd;
    dvui_SubwindowCurrentSetReturn prev_window_info;
    dvui_ScrollContainerWidget* prev_scroll;
    dvui_RectPhysical prev_clip;
    float scale_val;
    dvui_ScaleWidget scaler;
    dvui_Options options;
    dvui_FloatingTooltipInitOptions init_options;
    bool showing;
    bool mouse_good_this_frame;
    bool installed;
    bool tt_child_shown;
};

extern dvui_Options dvui_floating_tooltip_widget_defaults;

void dvui_floating_tooltip_widget_init(dvui_FloatingTooltipWidget* self, dvui_Src src, dvui_FloatingTooltipInitOptions init_opts, dvui_Options opts);
// Vrací true, pokud se má tooltip ukázat; pak do něj lze vkládat widgety.
bool dvui_floating_tooltip_widget_shown(dvui_FloatingTooltipWidget* self);
dvui_Widget dvui_floating_tooltip_widget_widget(dvui_FloatingTooltipWidget* self);
dvui_WidgetData* dvui_floating_tooltip_widget_data(dvui_FloatingTooltipWidget* self);
void dvui_floating_tooltip_widget_deinit(dvui_FloatingTooltipWidget* self);
// DVUI: dvui.tooltip – místo textu obsahuje zástupce o velikosti `content_size`.
void dvui_tooltip(dvui_Src src, dvui_FloatingTooltipInitOptions init_opts, dvui_Size content_size, dvui_Options opts);

// --- FloatingWindow (DVUI: FloatingWindowWidget.zig)

typedef enum dvui_FloatingWindowResize {
    DVUI_RESIZE_ALL = 0,
    DVUI_RESIZE_NONE,
} dvui_FloatingWindowResize;

typedef enum dvui_WindowAvoid {
    DVUI_WINDOW_AVOID_NUDGE_ONCE = 0,
    DVUI_WINDOW_AVOID_NONE,
    DVUI_WINDOW_AVOID_NUDGE,
} dvui_WindowAvoid;

typedef struct dvui_FloatingWindowInitOptions {
    bool modal;
    bool has_modal_alpha;
    uint8_t modal_alpha;
    dvui_Rect* rect;
    dvui_OptRect center_on;
    bool* open_flag;
    dvui_FloatingWindowResize resize;
    bool stay_above_parent_window;
    dvui_WindowAvoid window_avoid;
} dvui_FloatingWindowInitOptions;

typedef enum dvui_DragPart {
    DVUI_DRAG_MIDDLE,
    DVUI_DRAG_TOP,
    DVUI_DRAG_BOTTOM,
    DVUI_DRAG_LEFT,
    DVUI_DRAG_RIGHT,
    DVUI_DRAG_TOP_LEFT,
    DVUI_DRAG_BOTTOM_RIGHT,
    DVUI_DRAG_TOP_RIGHT,
    DVUI_DRAG_BOTTOM_LEFT,
} dvui_DragPart;

typedef struct dvui_FloatingWindowWidget {
    dvui_RenderFrontToBack render_ftb;
    dvui_WidgetData wd;
    dvui_FloatingWindowInitOptions init_options;
    dvui_Options options;
    dvui_SubwindowCurrentSetReturn prev_window_info;
    dvui_ScrollContainerWidget* prev_scroll;
    dvui_Id prev_last_focus;
    bool has_layout;
    dvui_BoxWidget layout;
    dvui_RectPhysical prev_clip;
    bool auto_pos;
    bool auto_size;
    bool has_auto_size_refresh_prev_value;
    uint8_t auto_size_refresh_prev_value;
    bool has_drag_part;
    dvui_DragPart drag_part;
    dvui_RectPhysical drag_area;
} dvui_FloatingWindowWidget;

extern dvui_Options dvui_floating_window_widget_defaults;

void dvui_floating_window_widget_init(dvui_FloatingWindowWidget* self, dvui_Src src, dvui_FloatingWindowInitOptions init_opts, dvui_Options opts);
void dvui_floating_window_widget_draw_background(dvui_FloatingWindowWidget* self);
void dvui_floating_window_widget_process_events_before(dvui_FloatingWindowWidget* self);
void dvui_floating_window_widget_drag_area_set(dvui_FloatingWindowWidget* self, dvui_RectPhysical rect);
void dvui_floating_window_widget_process_events_after(dvui_FloatingWindowWidget* self);
void dvui_floating_window_widget_auto_size(dvui_FloatingWindowWidget* self);
void dvui_floating_window_widget_auto_position(dvui_FloatingWindowWidget* self);
void dvui_floating_window_widget_close(dvui_FloatingWindowWidget* self);
dvui_Widget dvui_floating_window_widget_widget(dvui_FloatingWindowWidget* self);
dvui_WidgetData* dvui_floating_window_widget_data(dvui_FloatingWindowWidget* self);
void dvui_floating_window_widget_deinit(dvui_FloatingWindowWidget* self);
// DVUI: dvui.floatingWindow – init, processEventsBefore, drawBackground.
dvui_FloatingWindowWidget* dvui_floating_window(dvui_Src src, dvui_FloatingWindowInitOptions init_opts, dvui_Options opts);

// DVUI: dvui.windowHeader – lišta okna se zavíracím tlačítkem (bez titulku,
// místo něj zástupce výšky textu). Vrací obdélník pro dragAreaSet.
typedef struct dvui_WindowHeaderOptions {
    dvui_Size title_size;
    bool* open_flag;
} dvui_WindowHeaderOptions;
dvui_RectPhysical dvui_window_header(dvui_WindowHeaderOptions options);

// --- Grid (DVUI: GridWidget.zig)

typedef enum dvui_GridAutoSize {
    DVUI_GRID_AUTO_SIZE_ROWS,
    DVUI_GRID_AUTO_SIZE_COLS,
    DVUI_GRID_AUTO_SIZE_BOTH,
} dvui_GridAutoSize;

typedef struct dvui_GridInitOptions {
    dvui_ScrollAreaInitOptions scroll_opts;
    dvui_OptUsize rows;
    bool layout_only;
} dvui_GridInitOptions;

typedef struct dvui_GridCell {
    size_t col;
    size_t row;
} dvui_GridCell;

#define DVUI_GRID_COL_MIN_WIDTH 6.0f
#define DVUI_GRID_ROW_MIN_HEIGHT 6.0f

typedef struct dvui_GridRowHeight {
    size_t row;
    float height;
} dvui_GridRowHeight;

typedef struct dvui_GridRowOffset {
    size_t row;
    float offset;
} dvui_GridRowOffset;

typedef struct dvui_GridWidget dvui_GridWidget;

// DVUI: GridWidget.CellWidget
typedef struct dvui_GridCellWidget {
    dvui_GridWidget* grid;
    size_t col;
    size_t row;
    bool grid_focus;
    dvui_WidgetData wd;
    dvui_BasicLayout layout;
} dvui_GridCellWidget;

// Jednoduché dynamické pole (alokované v aréně snímku).
typedef struct dvui_GridFloatList {
    float* items;
    size_t len, cap;
} dvui_GridFloatList;
typedef struct dvui_GridRowHeightList {
    dvui_GridRowHeight* items;
    size_t len, cap;
} dvui_GridRowHeightList;
typedef struct dvui_GridRowOffsetList {
    dvui_GridRowOffset* items;
    size_t len, cap;
} dvui_GridRowOffsetList;

struct dvui_GridWidget {
    dvui_WidgetData wd;
    bool layout_only;
    dvui_Id last_focus;
    size_t cols;
    size_t rows;
    bool rows_provided;
    ptrdiff_t max_seen_col;
    ptrdiff_t max_seen_row;
    dvui_GridRowOffsetList row_offsets;
    dvui_GridCell cursor;
    dvui_GridCellWidget cell_widget;
    bool has_auto_size;
    dvui_GridAutoSize auto_size;
    dvui_Size default_min;
    bool shrink;
    float* col_widths;
    size_t col_widths_len;
    bool* col_expands;
    size_t col_expands_len;
    bool* col_expands_new;
    bool* col_resizable;
    size_t col_resizable_len;
    bool has_col_resize;
    size_t col_resize;
    float col_resize_amount;
    float col_expand;
    dvui_GridFloatList col_widths_auto;
    float* col_header_height;
    float col_header_height_auto;
    dvui_OptF32* row_height_default;
    dvui_GridRowHeight* row_heights;
    size_t row_heights_len;
    dvui_GridRowHeightList row_heights_auto;
    dvui_ScrollInfo* msi;
    dvui_ScrollAreaWidget scroll;
    dvui_ScrollInfo csi;
    bool has_cscroll;
    dvui_ScrollAreaWidget cscroll;
    bool has_bscroll;
    dvui_ScrollContainerWidget bscroll;
    dvui_Point frame_viewport;
    bool* mouse_mode;
};

extern dvui_Options dvui_grid_widget_defaults;

void dvui_grid_widget_init(dvui_GridWidget* self, dvui_Src src, dvui_GridInitOptions init_opts, dvui_Options opts);
void dvui_grid_widget_auto_size(dvui_GridWidget* self, dvui_GridAutoSize auto_size);
// Vrací první a poslední (exkluzivně) viditelný řádek. Vyžaduje `rows`.
void dvui_grid_widget_rows_visible(dvui_GridWidget* self, size_t* first, size_t* last);
dvui_Widget dvui_grid_widget_widget(dvui_GridWidget* self);
dvui_WidgetData* dvui_grid_widget_data(dvui_GridWidget* self);

typedef struct dvui_GridColHeaderInitOptions {
    size_t col;
    // DVUI výchozí resizable = true; zde opačně kvůli {0}.
    bool not_resizable;
} dvui_GridColHeaderInitOptions;

dvui_GridCellWidget* dvui_grid_widget_col_header(dvui_GridWidget* self, dvui_GridColHeaderInitOptions init_opts, dvui_Options opts);

typedef struct dvui_GridCellOptions {
    size_t col;
    size_t row;
    bool not_col_resizable;
} dvui_GridCellOptions;

dvui_GridCellWidget* dvui_grid_widget_cell(dvui_GridWidget* self, dvui_GridCellOptions cell_opts, dvui_Options opts);
void dvui_grid_widget_cell_min_size(dvui_GridWidget* self, size_t col, size_t row, dvui_Size min_size);
float dvui_grid_widget_col_width(dvui_GridWidget* self, size_t col);
float dvui_grid_widget_col_offset(dvui_GridWidget* self, size_t col);
float dvui_grid_widget_row_height(dvui_GridWidget* self, size_t row);
float dvui_grid_widget_row_offset(dvui_GridWidget* self, size_t row);
bool dvui_grid_widget_cell_from_point(dvui_GridWidget* self, dvui_PointPhysical p, dvui_GridCell* out);
// Buňka pod myší (DVUI: cellHovered, jen myš).
bool dvui_grid_widget_cell_hovered(dvui_GridWidget* self, dvui_GridCell* out);
void dvui_grid_widget_deinit(dvui_GridWidget* self);
dvui_GridWidget* dvui_grid(dvui_Src src, dvui_GridInitOptions init_opts, dvui_Options opts);

dvui_Widget dvui_grid_cell_widget_widget(dvui_GridCellWidget* self);
dvui_WidgetData* dvui_grid_cell_widget_data(dvui_GridCellWidget* self);
void dvui_grid_cell_widget_deinit(dvui_GridCellWidget* self);

#ifdef __cplusplus
}
#endif
