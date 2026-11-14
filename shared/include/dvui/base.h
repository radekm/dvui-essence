// Sdílené základní typy všech projektů dvui-essence (viz shared/README.md).
//
// Obsahuje to, co potřebuje backend a co je ve všech projektech stejné:
// id, DVUI_SRC, geometrii, barvy, vstupní výčty (tlačítka, klávesy,
// modifikátory, kurzory), vrchol, rozhraní backendu a funkce okna, které
// backend volá. Okno (`struct dvui_Window`) a tyto funkce implementuje
// každý projekt sám ve svém window.c.
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

// Náhrada za Zig `@src()`.
//
// Do id widgetu vstupuje adresa `anchor` – řetězcového literálu
// "soubor:řádek:čítač", který je pro každé použití DVUI_SRC jiný. Literály
// s různým obsahem nikdy nezačínají na stejné adrese, takže každé místo ve
// zdrojáku má po celý běh programu vlastní stálou adresu (DVUI hashuje
// podobně ukazatel na jméno souboru, řádek a sloupec). `file`, `line`
// a `column` (hodnota `__COUNTER__`) slouží jen pro chybové hlášky.
typedef struct dvui_Src {
    const char* file;
    int line;
    int column;
    const char* anchor;
} dvui_Src;

#define DVUI_STR_(x) #x
#define DVUI_STR(x) DVUI_STR_(x)
// __COUNTER__ se smí rozvinout jen jednou, proto pomocná makra.
#define DVUI_SRC DVUI_SRC_(__COUNTER__)
#define DVUI_SRC_(c) DVUI_SRC__(c)
#define DVUI_SRC__(c) ((dvui_Src){__FILE__, __LINE__, (c), __FILE__ ":" DVUI_STR(__LINE__) ":" #c})

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
// Geometrie a barvy (DVUI: Rect.zig, Size.zig, Point.zig, RectScale.zig, Color.zig)
//
// Funkce, které potřebují okno (dvui_rect_visible, dvui_rect_to_natural),
// deklaruje dvui.h projektu.
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
// Id (DVUI: dvui.zig – Id.extendId, Id.update; FNV-1a 64)
// ---------------------------------------------------------------------------

dvui_Id dvui_id_extend(dvui_Id start, dvui_Src src, size_t id_extra);
dvui_Id dvui_id_update(dvui_Id id, const char* name);

// ---------------------------------------------------------------------------
// Vstup: tlačítka, modifikátory, klávesy, kurzory (DVUI: enums.zig, Event.zig)
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

typedef enum dvui_WindowAction {
    DVUI_WINDOW_FOCUS,
    DVUI_WINDOW_CLOSE,
    DVUI_WINDOW_LEAVE,
} dvui_WindowAction;

typedef enum dvui_Cursor {
    DVUI_CURSOR_ARROW = 0,
    DVUI_CURSOR_HAND,
    DVUI_CURSOR_ARROW_ALL,
    DVUI_CURSOR_ARROW_NW_SE,
    DVUI_CURSOR_ARROW_NE_SW,
    DVUI_CURSOR_ARROW_N_S,
    DVUI_CURSOR_ARROW_W_E,
} dvui_Cursor;

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
    // DVUI: Backend.sleep – uspí vlákno na `ns` nanosekund (0 = jen předá
    // procesor). Volá ji Window.waitTime / beginWait; může být NULL.
    void (*sleep)(void* ctx, uint64_t ns);
} dvui_Backend;

// ---------------------------------------------------------------------------
// Funkce okna volané backendem (DVUI: Window.addEvent*)
//
// Implementuje je každý projekt ve svém okně.
// ---------------------------------------------------------------------------

typedef struct dvui_Window dvui_Window;

// Přidávání událostí (DVUI: Window.addEvent*). Volat po begin, před widgety.
// Vrací true, pokud je událost nad floating oknem.
bool dvui_window_add_event_mouse_motion(dvui_Window* win, dvui_PointPhysical pt);
bool dvui_window_add_event_mouse_button(dvui_Window* win, dvui_Button b, dvui_MouseActionKind action);
bool dvui_window_add_event_mouse_wheel(dvui_Window* win, float ticks, dvui_Direction dir);
bool dvui_window_add_event_key(dvui_Window* win, dvui_Key code, dvui_KeyAction action, unsigned mod);
void dvui_window_add_event_window(dvui_Window* win, dvui_WindowAction action);
void dvui_window_add_event_app_quit(dvui_Window* win);
void dvui_window_modifiers_set(dvui_Window* win, unsigned mod);

// Rychlost kolečka myši (DVUI: dvui.scroll_speed).
extern float dvui_scroll_speed;

#ifdef __cplusplus
}
#endif
