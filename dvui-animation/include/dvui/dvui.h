// dvui-animation – věrný port animací z DVUI (https://github.com/david-vanderson/dvui) do C23.
//
// Veřejné API. Struktura odpovídá DVUI: každý widget má strukturu s `wd`
// (WidgetData), funkce `*_init`, `*_deinit` a implementaci rozhraní `dvui_Widget`
// (rect_for, screen_rect_scale, min_size_for_child).
//
// Typické použití (stejné jako v DVUI):
//
//     bool interrupted = false;
//     while (open) {
//         dvui_window_begin(win, dvui_window_begin_wait(win, interrupted));
//         dvui_sdl3gpu_add_all_events(backend, win);
//         dvui_AnimateWidget* a = dvui_animate(DVUI_KIND(DVUI_ANIMATE_ALPHA), DVUI_DURATION(500000));
//         if (dvui_button(DVUI_LABEL_SIZE(40, 12), DVUI_FILL(DVUI_RGB(200, 80, 80)))) { ... }
//         dvui_animate_widget_deinit(a);
//         const int32_t end_micros = dvui_window_end(win);
//         interrupted = dvui_sdl3gpu_wait_event_timeout(backend, dvui_window_wait_time(win, end_micros));
//     }
#pragma once

#include "dvui/base.h"

#ifdef __cplusplus
extern "C" {
#endif

// Základní typy, geometrie, barvy, vstupní výčty a rozhraní backendu jsou
// ve sdílené hlavičce dvui/base.h (adresář shared/ v kořeni repozitáře).

// ---------------------------------------------------------------------------
// Geometrie závislá na okně (zbytek je v dvui/base.h)
// ---------------------------------------------------------------------------

// DVUI: Rect.Physical.visible – průnik s aktuálním clip obdélníkem není prázdný.
bool dvui_rect_visible(dvui_RectPhysical r);
// DVUI: Rect.Physical.toNatural
dvui_RectNatural dvui_rect_to_natural(dvui_RectPhysical r);

// ---------------------------------------------------------------------------
// Výčty používané ve vlastnostech widgetů
// ---------------------------------------------------------------------------

typedef enum dvui_Expand {
    DVUI_EXPAND_NONE = 0,
    DVUI_EXPAND_HORIZONTAL,
    DVUI_EXPAND_VERTICAL,
    DVUI_EXPAND_BOTH,
    // Zachovat poměr stran z min. velikosti a roztáhnout co nejvíc.
    DVUI_EXPAND_RATIO,
} dvui_Expand;

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

// ---------------------------------------------------------------------------
// Easing (DVUI: easing.zig)
//
// Funkce mění rychlost změny hodnoty: t z intervalu [0, 1] převádějí na
// „průběh“ animace. Některé (elastic, back) vracejí i hodnoty mimo [0, 1].
// ---------------------------------------------------------------------------

// DVUI: easing.EasingFn
typedef float dvui_EasingFn(float t);

#define DVUI_EASINGS(X)                                                                  \
    X(linear) X(in_quad) X(out_quad) X(in_out_quad) X(in_cubic) X(out_cubic)             \
    X(in_out_cubic) X(in_quart) X(out_quart) X(in_out_quart) X(in_quint) X(out_quint)    \
    X(in_out_quint) X(in_sine) X(out_sine) X(in_out_sine) X(in_expo) X(out_expo)         \
    X(in_out_expo) X(in_circ) X(out_circ) X(in_out_circ) X(in_elastic) X(out_elastic)    \
    X(in_out_elastic) X(in_back) X(out_back) X(in_out_back) X(in_bounce) X(out_bounce)   \
    X(in_out_bounce)

// dvui_easing_linear, dvui_easing_in_quad, … (DVUI: easing.linear, easing.inQuad, …)
#define DVUI_X(name) float dvui_easing_##name(float t);
DVUI_EASINGS(DVUI_X)
#undef DVUI_X

// Všechny easingy v pořadí DVUI_EASINGS (pro výběr v UI, testy).
typedef struct dvui_EasingInfo {
    const char* name;
    dvui_EasingFn* fn;
} dvui_EasingInfo;
extern const dvui_EasingInfo dvui_easings[];
extern const size_t dvui_easings_len;

// ---------------------------------------------------------------------------
// Animace (DVUI: dvui.zig – Animation)
//
// Časy jsou v mikrosekundách **relativně k aktuálnímu snímku**: 0 = teď,
// kladné = v budoucnu, záporné = v minulosti. Okno je v každém
// dvui_window_begin posune o uplynulý čas (viz docs/ANIMACE.md).
//
// Pozor: C nemá výchozí hodnoty polí. DVUI má výchozí `easing = linear`,
// `start_val = 0`, `end_val = 1`, `start_time = 0`; zde `easing == NULL`
// znamená linear, ale `end_val` je třeba zadat vždy (nula není 1).
// ---------------------------------------------------------------------------

typedef struct dvui_Animation {
    dvui_EasingFn* easing; // NULL = dvui_easing_linear
    float start_val;
    float end_val;
    int32_t start_time;
    int32_t end_time;
} dvui_Animation;

// DVUI: Animation.value – hodnota mezi start_val a end_val podle easingu.
float dvui_animation_value(const dvui_Animation* a);
// DVUI: Animation.done – true v posledním snímku animace (end_time <= 0).
bool dvui_animation_done(const dvui_Animation* a);

// DVUI: dvui.reduce_motion – když je true, dvui_animation přepíše end_time
// tak, aby animace skončila v příštím snímku. Časovače se to netýká.
extern bool dvui_reduce_motion;

// DVUI: AnimateWidget.Kind
typedef enum dvui_AnimateKind {
    DVUI_ANIMATE_NONE = 0,
    DVUI_ANIMATE_ALPHA,
    DVUI_ANIMATE_VERTICAL,
    DVUI_ANIMATE_HORIZONTAL,
} dvui_AnimateKind;

typedef struct dvui_Options dvui_Options;

// ---------------------------------------------------------------------------
// Vlastnosti widgetů – seznam značek (tag list)
//
// Widget dostane jeden seznam vlastností, např.
//
//     dvui_box(DVUI_DIR(DVUI_HORIZONTAL), DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL), DVUI_MARGIN_ALL(4));
//
// Co v seznamu není, je „nenastaveno“ a použije se výchozí hodnota widgetu
// (DVUI: `defaults.override(opts)`). Když je vlastnost v seznamu víckrát,
// platí poslední. Vlastnosti se dělí na
//   - společné (DVUI `Options`) – rozumí jim všechny widgety, ukládají se do
//     WidgetData a předávají se dětem,
//   - vlastnosti widgetu (DVUI `InitOptions`) – každý widget čte jen své,
//     cizí ignoruje (a ohlásí je jako chybu).
// ---------------------------------------------------------------------------

// Společné vlastnosti (DVUI: Options.zig).
//   X(jméno, typ, hodnota, když vlastnost nikdo nezadá)
#define DVUI_OPTION_PROPS(X)                                                         \
    /* odliší widgety vytvořené na stejném místě ve zdrojáku (smyčka) */            \
    X(id_extra, size_t, 0)                                                           \
    /* jméno pro ladění */                                                           \
    X(name, const char*, nullptr)                                                    \
    /* widget mimo layout rodiče, umístěný přímo sem (souřadnice rodiče) */         \
    X(rect, dvui_Rect, ((dvui_Rect){0}))                                             \
    X(expand, dvui_Expand, DVUI_EXPAND_NONE)                                         \
    X(gravity_x, float, 0.0f)                                                        \
    X(gravity_y, float, 0.0f)                                                        \
    /* barvy: nezadaná barva se bere z tématu */                                    \
    X(color_fill, dvui_Color, ((dvui_Color){0}))                                     \
    X(color_fill_hover, dvui_Color, ((dvui_Color){0}))                               \
    X(color_fill_press, dvui_Color, ((dvui_Color){0}))                               \
    X(color_text, dvui_Color, ((dvui_Color){0}))                                     \
    X(color_text_hover, dvui_Color, ((dvui_Color){0}))                               \
    X(color_text_press, dvui_Color, ((dvui_Color){0}))                               \
    X(color_border, dvui_Color, ((dvui_Color){0}))                                   \
    X(style, dvui_StyleName, DVUI_STYLE_CONTENT)                                     \
    /* vnější okraj, rámeček, vnitřní okraj (x vlevo, y nahoře, w vpravo, h dole) */ \
    X(margin, dvui_Rect, ((dvui_Rect){0}))                                           \
    X(border, dvui_Rect, ((dvui_Rect){0}))                                           \
    X(padding, dvui_Rect, ((dvui_Rect){0}))                                          \
    /* min. a max. velikost obsahu (bez padding/border/margin) */                    \
    X(min_size_content, dvui_Size, ((dvui_Size){0}))                                 \
    X(max_size_content, dvui_Size, ((dvui_Size){DVUI_MAX_FLOAT_SAFE, DVUI_MAX_FLOAT_SAFE})) \
    /* kreslit pozadí v background rectu */                                         \
    X(background, bool, false)

// Vlastnosti jednotlivých widgetů (DVUI: InitOptions widgetů). Výchozí
// hodnota zde je obecná; widget může mít vlastní (viz jeho dokumentace).
#define DVUI_WIDGET_PROPS(X)                                                         \
    /* Box */                                                                        \
    X(dir, dvui_Direction, DVUI_VERTICAL)                                            \
    X(equal_space, bool, false)                                                      \
    X(num_packed_expanded, uint32_t, 0)                                              \
    /* Button, Label: velikost „textu“ */                                           \
    X(label_size, dvui_Size, ((dvui_Size){0}))                                       \
    X(draw_focus, bool, true)                                                        \
    X(grayed, bool, false)                                                           \
    /* Animate */                                                                    \
    X(kind, dvui_AnimateKind, DVUI_ANIMATE_NONE)                                     \
    X(duration, int32_t, 0)                                                          \
    X(easing, dvui_EasingFn*, nullptr)                                               \
    /* vložení hotových Options (např. předání stylu dítěti) */                     \
    X(options, const dvui_Options*, nullptr)

// Značky.
typedef enum dvui_PropTag {
    DVUI_PROP_END = 0, // konec seznamu
#define DVUI_X(name, type, def) DVUI_PROP_##name,
    DVUI_OPTION_PROPS(DVUI_X)
    DVUI_PROP_OPTIONS_COUNT_, // pomocná: značky před ní jsou společné
    DVUI_WIDGET_PROPS(DVUI_X)
#undef DVUI_X
    DVUI_PROP_COUNT,
} dvui_PropTag;

// Bit společné vlastnosti v dvui_Options.set.
#define DVUI_BIT(name) (1u << DVUI_PROP_##name)

// Společné vlastnosti rozbalené do struktury. Ukládá se do WidgetData a čte
// ji kód widgetů. `set` říká, které vlastnosti byly zadané (DVUI: `?T != null`).
struct dvui_Options {
#define DVUI_X(name, type, def) type name;
    DVUI_OPTION_PROPS(DVUI_X)
#undef DVUI_X
    uint32_t set;
};

// Jedna položka seznamu: značka + hodnota.
typedef struct dvui_Prop {
    dvui_PropTag tag;
    union {
#define DVUI_X(name, type, def) type name;
        DVUI_OPTION_PROPS(DVUI_X)
        DVUI_WIDGET_PROPS(DVUI_X)
#undef DVUI_X
    };
} dvui_Prop;

// Seznam vlastností předávaný widgetům (ukončený DVUI_PROP_END).
typedef struct dvui_Props {
    const dvui_Prop* items;
} dvui_Props;

// Obecný konstruktor: DVUI_P(margin, dvui_rect_all(4)), DVUI_P(dir, DVUI_HORIZONTAL).
// Je to inicializátor, takže funguje i ve statických polích.
#define DVUI_P(name, ...) {.tag = DVUI_PROP_##name, .name = __VA_ARGS__}

// Seznam vlastností jako výraz; prázdný seznam je povolený (C23 __VA_OPT__).
#define DVUI_PROPS(...) ((dvui_Props){(const dvui_Prop[]){__VA_ARGS__ __VA_OPT__(, ) {0}}})

// --- zkratky pro společné vlastnosti
#define DVUI_ID_EXTRA(i) DVUI_P(id_extra, (i))
#define DVUI_NAME(s) DVUI_P(name, (s))
#define DVUI_RECT(...) DVUI_P(rect, __VA_ARGS__)
#define DVUI_EXPAND(e) DVUI_P(expand, (e))
#define DVUI_GRAVITY_X(g) DVUI_P(gravity_x, (g))
#define DVUI_GRAVITY_Y(g) DVUI_P(gravity_y, (g))
#define DVUI_FILL(...) DVUI_P(color_fill, __VA_ARGS__)
#define DVUI_FILL_HOVER(...) DVUI_P(color_fill_hover, __VA_ARGS__)
#define DVUI_FILL_PRESS(...) DVUI_P(color_fill_press, __VA_ARGS__)
#define DVUI_TEXT_COLOR(...) DVUI_P(color_text, __VA_ARGS__)
#define DVUI_BORDER_COLOR(...) DVUI_P(color_border, __VA_ARGS__)
#define DVUI_STYLE(s) DVUI_P(style, (s))
#define DVUI_MARGIN(...) DVUI_P(margin, __VA_ARGS__)
#define DVUI_MARGIN_ALL(v) DVUI_P(margin, {(v), (v), (v), (v)})
#define DVUI_BORDER(...) DVUI_P(border, __VA_ARGS__)
#define DVUI_BORDER_ALL(v) DVUI_P(border, {(v), (v), (v), (v)})
#define DVUI_PADDING(...) DVUI_P(padding, __VA_ARGS__)
#define DVUI_PADDING_ALL(v) DVUI_P(padding, {(v), (v), (v), (v)})
#define DVUI_MIN_SIZE(w, h) DVUI_P(min_size_content, {(w), (h)})
#define DVUI_MAX_SIZE(w, h) DVUI_P(max_size_content, {(w), (h)})
#define DVUI_BACKGROUND(b) DVUI_P(background, (b))
#define DVUI_OPTIONS(p) DVUI_P(options, (p))

// --- zkratky pro vlastnosti widgetů
#define DVUI_DIR(d) DVUI_P(dir, (d))
#define DVUI_EQUAL_SPACE(b) DVUI_P(equal_space, (b))
#define DVUI_LABEL_SIZE(w, h) DVUI_P(label_size, {(w), (h)})
#define DVUI_DRAW_FOCUS(b) DVUI_P(draw_focus, (b))
#define DVUI_GRAYED(b) DVUI_P(grayed, (b))
#define DVUI_KIND(k) DVUI_P(kind, (k))
#define DVUI_DURATION(us) DVUI_P(duration, (us))
#define DVUI_EASING(f) DVUI_P(easing, (f))

// Vlastnosti widgetu rozbalené do struktury (pomocná struktura při init).
typedef struct dvui_WidgetProps {
#define DVUI_X(name, type, def) type name;
    DVUI_WIDGET_PROPS(DVUI_X)
#undef DVUI_X
    bool has[DVUI_PROP_COUNT];
} dvui_WidgetProps;

// Rozbalí seznamy: nejdřív obecné výchozí hodnoty, pak `defaults` widgetu,
// pak `props` uživatele. `allowed` je seznam značek widgetu ukončený
// DVUI_PROP_END (společné značky jsou povolené vždy); cizí značky se ohlásí.
// `wp` smí být NULL, pokud widget žádné vlastní vlastnosti nemá.
void dvui_props_resolve(const char* widget, const dvui_Prop* defaults, dvui_Props props, const dvui_PropTag* allowed,
                        dvui_Options* opts, dvui_WidgetProps* wp);

// Zapíše do `out` zadané vlastnosti widgetu z `wp`, jejichž značky jsou
// v `tags` (předání vlastností vnitřnímu widgetu). Vrací počet zapsaných
// položek; `out` musí mít místo pro všechny značky z `tags`. Konec seznamu
// nepřidává.
size_t dvui_props_forward(const dvui_WidgetProps* wp, const dvui_PropTag* tags, dvui_Prop* out);

// Výchozí stav Options (nic nezadáno).
dvui_Options dvui_options_default(void);
// Aplikuje seznam na kopii `self` a vrátí ji (vlastnosti widgetů ignoruje).
dvui_Options dvui_options_apply(dvui_Options self, dvui_Props props);
// Zkratka: dvui_options_with(o, DVUI_EXPAND(DVUI_EXPAND_BOTH), …)
#define dvui_options_with(self, ...) dvui_options_apply((self), DVUI_PROPS(__VA_ARGS__))
// Byla vlastnost zadána? dvui_options_has(o, DVUI_BIT(color_fill))
bool dvui_options_has(const dvui_Options* o, uint32_t bits);

// DVUI: Options.override – zadané vlastnosti `over` přepíší `self`.
dvui_Options dvui_options_override(dvui_Options self, dvui_Options over);
// DVUI: Options.strip – ponechá jen styl (barvy), vynuluje layout.
dvui_Options dvui_options_strip(dvui_Options self);
// DVUI: Options.styleOnly
dvui_Options dvui_options_style_only(dvui_Options self);

dvui_Gravity dvui_options_gravity_get(const dvui_Options* o);
dvui_Size dvui_options_min_size_get(const dvui_Options* o);
dvui_Size dvui_options_max_size_get(const dvui_Options* o);
dvui_Size dvui_options_pad_size(const dvui_Options* o, dvui_Size s);
// DVUI: Options.color – barva z Options, jinak z tématu.
dvui_Color dvui_options_color(const dvui_Options* o, dvui_ColorAsk ask);

// ---------------------------------------------------------------------------
// Téma (DVUI: Theme.zig, zjednodušené – viz README)
// ---------------------------------------------------------------------------

// Barva {0, 0, 0, 0} v tématu znamená „nenastaveno“ (DVUI: `?Color = null`).
typedef struct dvui_ThemeStyle {
    dvui_Color fill;
    dvui_Color fill_hover;
    dvui_Color fill_press;
    dvui_Color text;
    dvui_Color text_hover;
    dvui_Color text_press;
    dvui_Color border;
} dvui_ThemeStyle;

typedef struct dvui_Theme {
    const char* name;
    bool dark;
    dvui_Color focus;
    dvui_Color fill;
    dvui_Color fill_hover; // nenastaveno = odvodí se z fill
    dvui_Color fill_press;
    dvui_Color text;
    dvui_Color text_hover;
    dvui_Color text_press;
    dvui_Color text_select;
    dvui_Color border;
    dvui_ThemeStyle control;
    dvui_ThemeStyle window;
    dvui_ThemeStyle highlight;
    dvui_ThemeStyle err;
} dvui_Theme;

// Je barva tématu nastavená (není {0, 0, 0, 0})?
bool dvui_color_is_set(dvui_Color c);

// Výchozí téma podle DVUI Adwaita Light.
dvui_Theme dvui_theme_adwaita_light(void);
// DVUI: Theme.color
dvui_Color dvui_theme_color(const dvui_Theme* t, dvui_StyleName style, dvui_ColorAsk ask);
// DVUI: Theme.adjustColorForState
dvui_Color dvui_theme_adjust_color_for_state(const dvui_Theme* t, dvui_Color col, dvui_ColorAsk ask);

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
    // NULL = barva z Options
    const dvui_Color* fill_color;
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

// ---------------------------------------------------------------------------
// Události (DVUI: Event.zig). Tlačítka, klávesy, modifikátory a kurzory
// jsou v dvui/base.h.
// ---------------------------------------------------------------------------

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

typedef struct dvui_EventKey {
    dvui_Key code;
    dvui_KeyAction action;
    unsigned mod;
} dvui_EventKey;

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

typedef struct dvui_ClickOptions {
    bool* hovered;
    // DVUI výchozí .hand; zde `no_hover_cursor` vypne nastavení kurzoru.
    bool no_hover_cursor;
    // NULL = border rect widgetu
    const dvui_RectPhysical* rect;
} dvui_ClickOptions;

// DVUI: dvui.clicked
bool dvui_clicked(const dvui_WidgetData* wd, dvui_ClickOptions opts);

// ---------------------------------------------------------------------------
// Window (DVUI: Window.zig). Typ dvui_Window, rozhraní backendu a funkce
// pro přidávání událostí (dvui_window_add_event_*) jsou v dvui/base.h.
// ---------------------------------------------------------------------------

dvui_Window* dvui_window_create(dvui_Backend backend);
void dvui_window_destroy(dvui_Window* win);
void dvui_window_theme_set(dvui_Window* win, dvui_Theme theme);
// Ukazatel na proměnnou, do které se při zavření okna zapíše false.
void dvui_window_open_flag_set(dvui_Window* win, bool* open_flag);

// DVUI: Window.begin
void dvui_window_begin(dvui_Window* win, int64_t time_ns);
// DVUI: Window.end – vrací, kolik mikrosekund má uplynout mezi tímto
// a dalším dvui_window_begin: 0 = hned (refresh nebo běžící animace),
// kladné = nejbližší animace/časovač začíná za tolik µs, −1 = čekat na
// událost (DVUI `null`). Výsledek se předává do dvui_window_wait_time.
int32_t dvui_window_end(dvui_Window* win);

// DVUI: Window.beginWait – volá se těsně před dvui_window_begin; vrací čas
// snímku v ns pro dvui_window_begin. `interrupted` = čekání v backendu
// přerušila událost (výsledek dvui_sdl3gpu_wait_event_timeout). Dorovnává
// „slop“ – odhad, o kolik se smyčka probouzí pozdě.
int64_t dvui_window_begin_wait(dvui_Window* win, bool interrupted);
// DVUI: Window.waitTime – z výsledku dvui_window_end spočítá, kolik µs má
// backend čekat na událost (UINT32_MAX = bez limitu). Při max_fps tu
// rovnou spí, aby se nepřekročil počet snímků za sekundu.
uint32_t dvui_window_wait_time(dvui_Window* win, int32_t end_micros);
// DVUI: Window.max_fps – omezení snímků za sekundu; 0 = bez omezení (null).
void dvui_window_max_fps_set(dvui_Window* win, float max_fps);
// DVUI: Window.loop_target_slop – odhad režie smyčky v µs (pro ladění).
int32_t dvui_window_loop_target_slop(const dvui_Window* win);

// DVUI: Window.cursorRequested
dvui_Cursor dvui_window_cursor_requested(const dvui_Window* win);

// ---------------------------------------------------------------------------
// Globální funkce platné mezi begin a end (DVUI: dvui.zig)
// ---------------------------------------------------------------------------

dvui_Window* dvui_current_window(void);

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
// DVUI: dvui.frameTimeNS – čas začátku aktuálního snímku v ns.
int64_t dvui_frame_time_ns(void);
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

// ---------------------------------------------------------------------------
// Animace a časovače v okně (DVUI: dvui.animation, animationGet, timer…)
// ---------------------------------------------------------------------------

// DVUI: dvui.animation – uloží animaci pod klíč (id, key). Animace žije,
// dokud se na ni každý snímek někdo ptá (dvui_animation_get) a dokud
// neskončí; dokončená je vidět ještě jeden snímek (done == true).
void dvui_animation(dvui_Id id, const char* key, dvui_Animation a);
// DVUI: dvui.animationGet – false, pokud animace pod klíčem není.
bool dvui_animation_get(dvui_Id id, const char* key, dvui_Animation* out);

// DVUI: dvui.timer – časovač, který bude „done“ v prvním snímku po
// uplynutí `micros` mikrosekund (animace s klíčem "_timer").
void dvui_timer(dvui_Id id, int32_t micros);
// DVUI: dvui.timerGet – zbývající µs (<= 0 po vypršení), false = žádný časovač.
bool dvui_timer_get(dvui_Id id, int32_t* out);
// DVUI: dvui.timerDone – true v prvním snímku po vypršení.
bool dvui_timer_done(dvui_Id id);
// DVUI: dvui.timerDoneOrNone – timerDone nebo žádný časovač (periodické akce).
bool dvui_timer_done_or_none(dvui_Id id);
// DVUI: Window.timerRemove
void dvui_timer_remove(dvui_Id id);

// DVUI: dvui.hover_fade_secs
extern float dvui_hover_fade_secs;

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

// DVUI: dvui.scrollTo – dvui-animation nemá scroll kontejnery, takže funkce
// nic nedělá (volá ji WidgetData.register pro fokusovaný widget).
typedef struct dvui_ScrollToOptions {
    dvui_RectPhysical screen_rect;
    bool over_scroll;
} dvui_ScrollToOptions;
void dvui_scroll_to(dvui_ScrollToOptions st);

// ---------------------------------------------------------------------------
// Widgety
//
// Každý widget má
//   - `dvui_<w>_widget_init(self, src, props)` – inicializace ve vlastní paměti,
//   - `dvui_<w>_widget_deinit(self)`,
//   - u většiny makro `dvui_<w>(…vlastnosti…)`, které widget alokuje, doplní
//     DVUI_SRC a uzavře seznam vlastností.
// U každého widgetu je uvedeno, kterým vlastním vlastnostem rozumí; společným
// (DVUI_OPTION_PROPS) rozumí všechny.
// ---------------------------------------------------------------------------

// --- Spacer, Separator, Label (DVUI: dvui.spacer, dvui.separator, LabelWidget)

// Prázdný widget zabírající místo (min_size_content).
dvui_WidgetData dvui_spacer_impl(dvui_Src src, dvui_Props props);
#define dvui_spacer(...) dvui_spacer_impl(DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

// Tenká čára (min_size_content 1×1, barva okraje tématu).
dvui_WidgetData dvui_separator_impl(dvui_Src src, dvui_Props props);
#define dvui_separator(...) dvui_separator_impl(DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

// Zástupce za LabelWidget: chová se v layoutu jako popisek s textem velikosti
// `label_size` (výchozí padding 6), ale text nekreslí.
// Vlastní vlastnosti: label_size.
dvui_WidgetData dvui_label_placeholder_impl(dvui_Src src, dvui_Props props);
#define dvui_label_placeholder(...) dvui_label_placeholder_impl(DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

// --- Overlay (DVUI: OverlayWidget.zig)

typedef struct dvui_OverlayWidget {
    dvui_WidgetData wd;
} dvui_OverlayWidget;

void dvui_overlay_widget_init(dvui_OverlayWidget* self, dvui_Src src, dvui_Props props);
void dvui_overlay_widget_draw_background(dvui_OverlayWidget* self);
dvui_Widget dvui_overlay_widget_widget(dvui_OverlayWidget* self);
void dvui_overlay_widget_deinit(dvui_OverlayWidget* self);
dvui_OverlayWidget* dvui_overlay_impl(dvui_Src src, dvui_Props props);
#define dvui_overlay(...) dvui_overlay_impl(DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

// --- Box (DVUI: BoxWidget.zig)
// Vlastní vlastnosti: dir (výchozí VERTICAL), equal_space, num_packed_expanded.

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

void dvui_box_widget_init(dvui_BoxWidget* self, dvui_Src src, dvui_Props props);
void dvui_box_widget_draw_background(dvui_BoxWidget* self);
dvui_Widget dvui_box_widget_widget(dvui_BoxWidget* self);
dvui_WidgetData* dvui_box_widget_data(dvui_BoxWidget* self);
dvui_Rect dvui_box_widget_rect_for(dvui_BoxWidget* self, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g);
dvui_RectScale dvui_box_widget_screen_rect_scale(dvui_BoxWidget* self, dvui_Rect rect);
void dvui_box_widget_min_size_for_child(dvui_BoxWidget* self, dvui_Size s);
void dvui_box_widget_deinit(dvui_BoxWidget* self);
// DVUI: dvui.box – alokuje widget, zavolá init a drawBackground.
dvui_BoxWidget* dvui_box_impl(dvui_Src src, dvui_Props props);
#define dvui_box(...) dvui_box_impl(DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

// --- Button (DVUI: ButtonWidget.zig)
// Vlastní vlastnosti: draw_focus (výchozí true), grayed; dvui_button navíc
// label_size (velikost zástupce popisku).

typedef struct dvui_ButtonInitOptions {
    bool draw_focus;
    bool grayed;
} dvui_ButtonInitOptions;

typedef struct dvui_ButtonWidget {
    dvui_WidgetData wd;
    dvui_ButtonInitOptions init_options;
    bool hover;
    bool focus;
    bool click;
} dvui_ButtonWidget;

extern const dvui_Prop dvui_button_widget_defaults[];

void dvui_button_widget_init(dvui_ButtonWidget* self, dvui_Src src, dvui_Props props);
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
bool dvui_button_impl(dvui_Src src, dvui_Props props);
#define dvui_button(...) dvui_button_impl(DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

// --- Animate (DVUI: AnimateWidget.zig)
// Obal pro obsah, který animuje: průhlednost (alpha) nebo výšku / šířku
// (vertical / horizontal). Vlastní vlastnosti: kind (výchozí NONE),
// duration (µs, v DVUI povinné), easing (NULL = linear). Animace „_start“
// začne v prvním snímku sama; „_end“ spustí dvui_animate_widget_start_end.

typedef struct dvui_AnimateInitOptions {
    dvui_AnimateKind kind;
    // délka v mikrosekundách
    int32_t duration;
    // NULL = dvui_easing_linear
    dvui_EasingFn* easing;
} dvui_AnimateInitOptions;

typedef struct dvui_AnimateWidget {
    dvui_WidgetData wd;
    dvui_AnimateInitOptions init_opts;
    // DVUI: val: ?f32
    bool has_val;
    float val;
    float prev_alpha;
    dvui_BasicLayout layout;
} dvui_AnimateWidget;

void dvui_animate_widget_init(dvui_AnimateWidget* self, dvui_Src src, dvui_Props props);
// DVUI: AnimateWidget.start – spustí animaci „_start“ (0 → 1).
void dvui_animate_widget_start(dvui_AnimateWidget* self);
// DVUI: AnimateWidget.startEnd – spustí animaci „_end“ (1 → 0).
void dvui_animate_widget_start_end(dvui_AnimateWidget* self);
// DVUI: AnimateWidget.end – true v posledním snímku animace „_end“.
bool dvui_animate_widget_end(dvui_AnimateWidget* self);
dvui_Widget dvui_animate_widget_widget(dvui_AnimateWidget* self);
dvui_WidgetData* dvui_animate_widget_data(dvui_AnimateWidget* self);
dvui_Rect dvui_animate_widget_rect_for(dvui_AnimateWidget* self, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g);
dvui_RectScale dvui_animate_widget_screen_rect_scale(dvui_AnimateWidget* self, dvui_Rect rect);
void dvui_animate_widget_min_size_for_child(dvui_AnimateWidget* self, dvui_Size s);
void dvui_animate_widget_deinit(dvui_AnimateWidget* self);
// DVUI: dvui.animate – alokuje widget a zavolá init.
dvui_AnimateWidget* dvui_animate_impl(dvui_Src src, dvui_Props props);
#define dvui_animate(...) dvui_animate_impl(DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

#ifdef __cplusplus
}
#endif
