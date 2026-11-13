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
//     dvui_BoxWidget* b = dvui_box(DVUI_DIR(DVUI_HORIZONTAL), DVUI_EXPAND(DVUI_EXPAND_BOTH));
//     if (dvui_button(DVUI_LABEL_SIZE(40, 12), DVUI_FILL(dvui_color_rgb(200, 80, 80)))) { ... }
//     dvui_box_widget_deinit(b);
//     dvui_window_end(win);
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

// DVUI: ScrollInfo.ScrollMode
typedef enum dvui_ScrollMode {
    DVUI_SCROLL_NONE = 0,
    DVUI_SCROLL_AUTO,
    DVUI_SCROLL_GIVEN,
} dvui_ScrollMode;

// DVUI: ScrollAreaWidget.ScrollBarMode
typedef enum dvui_ScrollBarMode {
    DVUI_SCROLL_BAR_AUTO = 0,
    DVUI_SCROLL_BAR_HIDE,
    DVUI_SCROLL_BAR_AUTO_OVERLAY,
    DVUI_SCROLL_BAR_SHOW,
} dvui_ScrollBarMode;

// DVUI: FloatingTooltipWidget.Position
typedef enum dvui_TooltipPosition {
    DVUI_TOOLTIP_HORIZONTAL = 0,
    DVUI_TOOLTIP_VERTICAL,
    DVUI_TOOLTIP_STICKY,
    DVUI_TOOLTIP_ABSOLUTE,
} dvui_TooltipPosition;

// DVUI: FloatingWindowWidget.InitOptions.resize
typedef enum dvui_FloatingWindowResize {
    DVUI_RESIZE_ALL = 0,
    DVUI_RESIZE_NONE,
} dvui_FloatingWindowResize;

// DVUI: FloatingWindowWidget.InitOptions.window_avoid
typedef enum dvui_WindowAvoid {
    DVUI_WINDOW_AVOID_NUDGE_ONCE = 0,
    DVUI_WINDOW_AVOID_NONE,
    DVUI_WINDOW_AVOID_NUDGE,
} dvui_WindowAvoid;

typedef struct dvui_ScrollInfo dvui_ScrollInfo;
typedef struct dvui_ScrollAreaWidget dvui_ScrollAreaWidget;
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
    /* Box, ScrollBar */                                                             \
    X(dir, dvui_Direction, DVUI_VERTICAL)                                            \
    X(equal_space, bool, false)                                                      \
    X(num_packed_expanded, uint32_t, 0)                                              \
    /* Button, Label, tooltip, WindowHeader: velikost „textu“ */                    \
    X(label_size, dvui_Size, ((dvui_Size){0}))                                       \
    X(draw_focus, bool, true)                                                        \
    X(grayed, bool, false)                                                           \
    /* ScrollBar, ScrollContainer, ScrollArea, Grid */                               \
    X(scroll_info, dvui_ScrollInfo*, nullptr)                                        \
    X(focus_id, dvui_Id, 0)                                                          \
    X(vertical, dvui_ScrollMode, DVUI_SCROLL_AUTO)                                   \
    X(vertical_bar, dvui_ScrollBarMode, DVUI_SCROLL_BAR_AUTO)                        \
    X(horizontal, dvui_ScrollMode, DVUI_SCROLL_NONE)                                 \
    X(horizontal_bar, dvui_ScrollBarMode, DVUI_SCROLL_BAR_AUTO)                      \
    X(frame_viewport, dvui_Point, ((dvui_Point){0}))                                 \
    X(frame_viewport_out, dvui_Point*, nullptr)                                      \
    X(lock_visible, bool, false)                                                     \
    X(process_events_after, bool, true)                                              \
    X(container, bool, true)                                                         \
    X(user_scroll, dvui_Point*, nullptr)                                             \
    X(scroll_area, dvui_ScrollAreaWidget*, nullptr)                                  \
    X(event_rect, dvui_RectPhysical, ((dvui_RectPhysical){0}))                                       \
    /* Scale */                                                                      \
    X(scale, float*, nullptr)                                                        \
    /* Floating */                                                                   \
    X(mouse_events, bool, true)                                                      \
    X(from, dvui_PointPhysical, ((dvui_PointPhysical){0}))                                           \
    X(from_gravity_x, float, 0.5f)                                                   \
    X(from_gravity_y, float, 0.5f)                                                   \
    X(stay_above_parent, bool, true)                                                 \
    /* FloatingTooltip */                                                            \
    X(active_rect, dvui_RectPhysical, ((dvui_RectPhysical){0}))                                      \
    X(position, dvui_TooltipPosition, DVUI_TOOLTIP_HORIZONTAL)                       \
    X(interactive, bool, false)                                                      \
    X(tooltip_scale, float, 1.0f)                                                    \
    /* FloatingWindow, WindowHeader */                                               \
    X(modal, bool, false)                                                            \
    X(modal_alpha, uint8_t, 80)                                                      \
    X(rect_io, dvui_Rect*, nullptr)                                                  \
    X(center_on, dvui_Rect, ((dvui_Rect){0}))                                        \
    X(open_flag, bool*, nullptr)                                                     \
    X(resize, dvui_FloatingWindowResize, DVUI_RESIZE_ALL)                            \
    X(stay_above_parent_window, bool, false)                                         \
    X(window_avoid, dvui_WindowAvoid, DVUI_WINDOW_AVOID_NUDGE_ONCE)                  \
    /* Grid */                                                                       \
    X(rows, size_t, 0)                                                               \
    X(layout_only, bool, false)                                                      \
    X(col, size_t, 0)                                                                \
    X(row, size_t, 0)                                                                \
    X(resizable, bool, true)                                                         \
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
#define DVUI_SCROLL_INFO(p) DVUI_P(scroll_info, (p))
#define DVUI_SCROLL_VERTICAL(m) DVUI_P(vertical, (m))
#define DVUI_SCROLL_HORIZONTAL(m) DVUI_P(horizontal, (m))
#define DVUI_VERTICAL_BAR(m) DVUI_P(vertical_bar, (m))
#define DVUI_HORIZONTAL_BAR(m) DVUI_P(horizontal_bar, (m))
#define DVUI_ACTIVE_RECT(...) DVUI_P(active_rect, __VA_ARGS__)
#define DVUI_TOOLTIP_POSITION(p) DVUI_P(position, (p))
#define DVUI_MODAL(b) DVUI_P(modal, (b))
#define DVUI_OPEN_FLAG(p) DVUI_P(open_flag, (p))
#define DVUI_TITLE_SIZE(w, h) DVUI_P(label_size, {(w), (h)})
#define DVUI_ROWS(n) DVUI_P(rows, (n))
#define DVUI_COL(c) DVUI_P(col, (c))
#define DVUI_ROW(r) DVUI_P(row, (r))
#define DVUI_RESIZABLE(b) DVUI_P(resizable, (b))

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

// DVUI: layout.PlaceOnScreenAvoid
typedef enum dvui_PlaceOnScreenAvoid {
    DVUI_AVOID_NONE = 0,
    DVUI_AVOID_HORIZONTAL,
    DVUI_AVOID_VERTICAL,
} dvui_PlaceOnScreenAvoid;

dvui_RectNatural dvui_place_on_screen(dvui_RectNatural screen, dvui_RectNatural spawner, dvui_PlaceOnScreenAvoid avoid, dvui_RectNatural start);

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
// DVUI: Window.end – vrací, za kolik mikrosekund chce další snímek
// (0 = hned, -1 = čekat na událost).
int64_t dvui_window_end(dvui_Window* win);

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

// --- Scale (DVUI: ScaleWidget.zig)
// Vlastní vlastnosti: scale (ukazatel; bez něj se měřítko pamatuje v datech).

typedef struct dvui_ScaleWidget {
    dvui_WidgetData wd;
    float* scale;
    dvui_BasicLayout layout;
} dvui_ScaleWidget;

void dvui_scale_widget_init(dvui_ScaleWidget* self, dvui_Src src, dvui_Props props);
dvui_Widget dvui_scale_widget_widget(dvui_ScaleWidget* self);
void dvui_scale_widget_deinit(dvui_ScaleWidget* self);

// --- ScrollInfo (DVUI: ScrollInfo.zig)

bool dvui_scroll_bar_mode_auto_any(dvui_ScrollBarMode m);

struct dvui_ScrollInfo {
    dvui_ScrollMode vertical;
    dvui_ScrollMode horizontal;
    dvui_Size virtual_size;
    dvui_Rect viewport;
    dvui_Point velocity;
};

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
// Vlastní vlastnosti: scroll_info (povinná), dir (výchozí VERTICAL), focus_id.

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

extern const dvui_Prop dvui_scroll_bar_widget_defaults[];

void dvui_scroll_bar_widget_init(dvui_ScrollBarWidget* self, dvui_Src src, dvui_Props props);
dvui_ScrollBarGrab dvui_scroll_bar_widget_grab(dvui_ScrollBarWidget* self);
void dvui_scroll_bar_grab_draw(dvui_ScrollBarGrab g);
void dvui_scroll_bar_widget_deinit(dvui_ScrollBarWidget* self);

// --- ScrollContainer (DVUI: ScrollContainerWidget.zig)
// Vlastní vlastnosti: scroll_info (povinná), scroll_area, frame_viewport,
// lock_visible, event_rect, process_events_after (výchozí true), user_scroll.

typedef struct dvui_ScrollContainerInitOptions {
    dvui_ScrollAreaWidget* scroll_area;
    bool has_frame_viewport;
    dvui_Point frame_viewport;
    bool lock_visible;
    bool has_event_rect;
    dvui_RectPhysical event_rect;
    bool process_events_after;
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

void dvui_scroll_container_widget_init(dvui_ScrollContainerWidget* self, dvui_Src src, dvui_Props props);
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
// Vlastní vlastnosti: scroll_info, vertical (výchozí AUTO), vertical_bar,
// horizontal (výchozí NONE), horizontal_bar, focus_id, frame_viewport,
// frame_viewport_out, lock_visible, process_events_after (výchozí true),
// container (výchozí true), user_scroll.

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

// Značky, kterým ScrollArea rozumí (používá i grid pro předání dál).
extern const dvui_PropTag dvui_scroll_area_widget_tags[];
extern const dvui_Prop dvui_scroll_area_widget_defaults[];

void dvui_scroll_area_widget_init(dvui_ScrollAreaWidget* self, dvui_Src src, dvui_Props props);
dvui_WidgetData* dvui_scroll_area_widget_data(dvui_ScrollAreaWidget* self);
void dvui_scroll_area_widget_set_container_rect(dvui_ScrollAreaWidget* self, dvui_Rect rect);
void dvui_scroll_area_widget_deinit(dvui_ScrollAreaWidget* self);
dvui_ScrollAreaWidget* dvui_scroll_area_impl(dvui_Src src, dvui_Props props);
#define dvui_scroll_area(...) dvui_scroll_area_impl(DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

// --- Floating (DVUI: FloatingWidget.zig)
// Vlastní vlastnosti: mouse_events (výchozí true), from, from_gravity_x/y
// (výchozí 0.5), stay_above_parent (výchozí true).

typedef struct dvui_FloatingInitOptions {
    bool mouse_events;
    bool has_from;
    dvui_PointPhysical from;
    float from_gravity_x;
    float from_gravity_y;
    bool stay_above_parent;
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

void dvui_floating_widget_init(dvui_FloatingWidget* self, dvui_Src src, dvui_Props props);
dvui_Widget dvui_floating_widget_widget(dvui_FloatingWidget* self);
void dvui_floating_widget_deinit(dvui_FloatingWidget* self);

// --- FloatingTooltip (DVUI: FloatingTooltipWidget.zig)
// Vlastní vlastnosti: active_rect (fyzické pixely; tooltip se ukáže, když je
// v něm myš), position, interactive, tooltip_scale; dvui_tooltip navíc
// label_size (velikost zástupce textu).

typedef struct dvui_FloatingTooltipInitOptions {
    dvui_RectPhysical active_rect;
    dvui_TooltipPosition position;
    bool interactive;
    bool has_scale;
    float scale;
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

extern const dvui_Prop dvui_floating_tooltip_widget_defaults[];

void dvui_floating_tooltip_widget_init(dvui_FloatingTooltipWidget* self, dvui_Src src, dvui_Props props);
// Vrací true, pokud se má tooltip ukázat; pak do něj lze vkládat widgety.
bool dvui_floating_tooltip_widget_shown(dvui_FloatingTooltipWidget* self);
dvui_Widget dvui_floating_tooltip_widget_widget(dvui_FloatingTooltipWidget* self);
dvui_WidgetData* dvui_floating_tooltip_widget_data(dvui_FloatingTooltipWidget* self);
void dvui_floating_tooltip_widget_deinit(dvui_FloatingTooltipWidget* self);
// DVUI: dvui.tooltip – místo textu obsahuje zástupce o velikosti `label_size`.
void dvui_tooltip_impl(dvui_Src src, dvui_Props props);
#define dvui_tooltip(...) dvui_tooltip_impl(DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

// --- FloatingWindow (DVUI: FloatingWindowWidget.zig)
// Vlastní vlastnosti: modal, modal_alpha, rect_io (ukazatel na rect okna, který
// program čte i nastavuje), center_on, open_flag, resize, stay_above_parent_window,
// window_avoid.

typedef struct dvui_FloatingWindowInitOptions {
    bool modal;
    bool has_modal_alpha;
    uint8_t modal_alpha;
    dvui_Rect* rect;
    bool has_center_on;
    dvui_Rect center_on;
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

extern const dvui_Prop dvui_floating_window_widget_defaults[];

void dvui_floating_window_widget_init(dvui_FloatingWindowWidget* self, dvui_Src src, dvui_Props props);
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
dvui_FloatingWindowWidget* dvui_floating_window_impl(dvui_Src src, dvui_Props props);
#define dvui_floating_window(...) dvui_floating_window_impl(DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

// DVUI: dvui.windowHeader – lišta okna se zavíracím tlačítkem (bez titulku,
// místo něj zástupce o velikosti `label_size`). Vrací obdélník pro
// dvui_floating_window_widget_drag_area_set.
// Vlastní vlastnosti: label_size (DVUI_TITLE_SIZE), open_flag.
dvui_RectPhysical dvui_window_header_impl(dvui_Props props);
#define dvui_window_header(...) dvui_window_header_impl(DVUI_PROPS(__VA_ARGS__))

// --- Grid (DVUI: GridWidget.zig)
// Vlastní vlastnosti: rows, layout_only a všechny vlastnosti ScrollArea
// (předají se vnitřní scroll oblasti).

typedef enum dvui_GridAutoSize {
    DVUI_GRID_AUTO_SIZE_ROWS,
    DVUI_GRID_AUTO_SIZE_COLS,
    DVUI_GRID_AUTO_SIZE_BOTH,
} dvui_GridAutoSize;

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

// Výchozí výška řádku (DVUI: ?f32) – ukládá se v datech gridu.
typedef struct dvui_GridRowHeightDefault {
    bool has;
    float height;
} dvui_GridRowHeightDefault;

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
    dvui_GridRowHeightDefault* row_height_default;
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

extern const dvui_Prop dvui_grid_widget_defaults[];

void dvui_grid_widget_init(dvui_GridWidget* self, dvui_Src src, dvui_Props props);
void dvui_grid_widget_auto_size(dvui_GridWidget* self, dvui_GridAutoSize auto_size);
// Vrací první a poslední (exkluzivně) viditelný řádek. Vyžaduje `rows`.
void dvui_grid_widget_rows_visible(dvui_GridWidget* self, size_t* first, size_t* last);
dvui_Widget dvui_grid_widget_widget(dvui_GridWidget* self);
dvui_WidgetData* dvui_grid_widget_data(dvui_GridWidget* self);
// Hlavička sloupce. Vlastní vlastnosti: col, resizable (výchozí true).
dvui_GridCellWidget* dvui_grid_widget_col_header(dvui_GridWidget* self, dvui_Props props);
#define dvui_grid_col_header(grid, ...) dvui_grid_widget_col_header((grid), DVUI_PROPS(__VA_ARGS__))
// Buňka. Vlastní vlastnosti: col, row, resizable (výchozí true – smí se měnit
// šířka sloupce).
dvui_GridCellWidget* dvui_grid_widget_cell(dvui_GridWidget* self, dvui_Props props);
#define dvui_grid_cell(grid, ...) dvui_grid_widget_cell((grid), DVUI_PROPS(__VA_ARGS__))
void dvui_grid_widget_cell_min_size(dvui_GridWidget* self, size_t col, size_t row, dvui_Size min_size);
float dvui_grid_widget_col_width(dvui_GridWidget* self, size_t col);
float dvui_grid_widget_col_offset(dvui_GridWidget* self, size_t col);
float dvui_grid_widget_row_height(dvui_GridWidget* self, size_t row);
float dvui_grid_widget_row_offset(dvui_GridWidget* self, size_t row);
bool dvui_grid_widget_cell_from_point(dvui_GridWidget* self, dvui_PointPhysical p, dvui_GridCell* out);
// Buňka pod myší (DVUI: cellHovered, jen myš).
bool dvui_grid_widget_cell_hovered(dvui_GridWidget* self, dvui_GridCell* out);
void dvui_grid_widget_deinit(dvui_GridWidget* self);
dvui_GridWidget* dvui_grid_impl(dvui_Src src, dvui_Props props);
#define dvui_grid(...) dvui_grid_impl(DVUI_SRC, DVUI_PROPS(__VA_ARGS__))

dvui_Widget dvui_grid_cell_widget_widget(dvui_GridCellWidget* self);
dvui_WidgetData* dvui_grid_cell_widget_data(dvui_GridCellWidget* self);
void dvui_grid_cell_widget_deinit(dvui_GridCellWidget* self);

#ifdef __cplusplus
}
#endif
