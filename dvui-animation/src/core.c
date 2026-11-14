// Options, téma, placeIn a geometrie závislá na okně.
// DVUI: Options.zig, Theme.zig, dvui.zig (placeIn), Rect.zig (visible, toNatural)
// Ostatní geometrie a barvy jsou ve shared/src/geometry.c.
#include "internal.h"

#include <math.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Geometrie závislá na okně (zbytek je ve shared/src/geometry.c)
// ---------------------------------------------------------------------------

bool dvui_rect_visible(dvui_RectPhysical r) { return !dvui_rect_empty(dvui_rect_intersect(dvui_clip_get(), r)); }

dvui_RectNatural dvui_rect_to_natural(dvui_RectPhysical r) { return dvui_rect_scale(r, 1.0f / dvui_window_natural_scale()); }


// ---------------------------------------------------------------------------
// Theme (DVUI: Theme.zig, themes/Adwaita.zig)
// ---------------------------------------------------------------------------

dvui_Theme dvui_theme_adwaita_light(void) {
    const dvui_Color accent = DVUI_RGB(0x35, 0x84, 0xe4);
    const dvui_Color err = DVUI_RGB(0xe0, 0x1b, 0x24);
    const dvui_Color white = DVUI_RGB(255, 255, 255);
    return (dvui_Theme){
        .name = "Adwaita Light",
        .dark = false,
        .focus = accent,
        .fill = white,
        .fill_hover = dvui_color_from_lightness(82),
        .fill_press = dvui_color_from_lightness(72),
        .text = DVUI_RGB(0, 0, 0),
        .text_select = DVUI_RGB(0x91, 0xbc, 0xf0),
        .border = dvui_color_from_lightness(63),
        .control = {
            .fill = DVUI_RGB(0xe0, 0xe0, 0xe0),
            .fill_hover = dvui_color_from_lightness(82),
            .fill_press = dvui_color_from_lightness(72),
        },
        .window = {
            .fill = DVUI_RGB(0xf0, 0xf0, 0xf0),
        },
        .highlight = {
            .fill = accent,
            .fill_hover = dvui_color_lighten(accent, -11),
            .fill_press = dvui_color_lighten(accent, -16),
            .text = white,
            .border = dvui_color_lighten(accent, -22),
        },
        .err = {
            .fill = err,
            .fill_hover = dvui_color_lighten(err, -10),
            .fill_press = dvui_color_lighten(err, -15),
            .text = white,
            .border = dvui_color_lighten(err, -20),
        },
    };
}

dvui_Color dvui_theme_adjust_color_for_state(const dvui_Theme* t, dvui_Color col, dvui_ColorAsk ask) {
    switch (ask) {
    case DVUI_COLOR_FILL_HOVER: return dvui_color_lighten(col, t->dark ? 10 : -10);
    case DVUI_COLOR_FILL_PRESS: return dvui_color_lighten(col, t->dark ? 20 : -20);
    default: return col;
    }
}

// DVUI: Theme.color. Zig `continue :sw .fill` zachovává původní `ask`, proto
// se zde předává `ask` dál beze změny.
dvui_Color dvui_theme_color(const dvui_Theme* t, dvui_StyleName style, dvui_ColorAsk ask) {
    const dvui_ThemeStyle* cs = nullptr;
    switch (style) {
    case DVUI_STYLE_CONTENT:
        switch (ask) {
        case DVUI_COLOR_BORDER: return t->border;
        case DVUI_COLOR_FILL: return dvui_theme_adjust_color_for_state(t, t->fill, ask);
        case DVUI_COLOR_FILL_HOVER:
            return dvui_color_is_set(t->fill_hover) ? t->fill_hover : dvui_theme_adjust_color_for_state(t, t->fill, ask);
        case DVUI_COLOR_FILL_PRESS:
            return dvui_color_is_set(t->fill_press) ? t->fill_press : dvui_theme_adjust_color_for_state(t, t->fill, ask);
        case DVUI_COLOR_TEXT: return t->text;
        case DVUI_COLOR_TEXT_HOVER: return dvui_color_is_set(t->text_hover) ? t->text_hover : t->text;
        case DVUI_COLOR_TEXT_PRESS: return dvui_color_is_set(t->text_press) ? t->text_press : t->text;
        }
        return t->fill;
    case DVUI_STYLE_CONTROL: cs = &t->control; break;
    case DVUI_STYLE_WINDOW: cs = &t->window; break;
    case DVUI_STYLE_HIGHLIGHT: cs = &t->highlight; break;
    case DVUI_STYLE_ERR: cs = &t->err; break;
    }

    switch (ask) {
    case DVUI_COLOR_BORDER: return dvui_color_is_set(cs->border) ? cs->border : dvui_theme_color(t, DVUI_STYLE_CONTENT, ask);
    case DVUI_COLOR_FILL_HOVER:
        if (dvui_color_is_set(cs->fill_hover)) return cs->fill_hover;
        goto fill;
    case DVUI_COLOR_FILL_PRESS:
        if (dvui_color_is_set(cs->fill_press)) return cs->fill_press;
        goto fill;
    case DVUI_COLOR_FILL:
    fill:
        return dvui_color_is_set(cs->fill) ? dvui_theme_adjust_color_for_state(t, cs->fill, ask) : dvui_theme_color(t, DVUI_STYLE_CONTENT, ask);
    case DVUI_COLOR_TEXT_HOVER:
        if (dvui_color_is_set(cs->text_hover)) return cs->text_hover;
        goto text;
    case DVUI_COLOR_TEXT_PRESS:
        if (dvui_color_is_set(cs->text_press)) return cs->text_press;
        goto text;
    case DVUI_COLOR_TEXT:
    text:
        return dvui_color_is_set(cs->text) ? cs->text : dvui_theme_color(t, DVUI_STYLE_CONTENT, ask);
    }
    return t->fill;
}

// ---------------------------------------------------------------------------
// Options (DVUI: Options.zig)
// ---------------------------------------------------------------------------

bool dvui_expand_is_horizontal(dvui_Expand e) { return e == DVUI_EXPAND_HORIZONTAL || e == DVUI_EXPAND_BOTH; }
bool dvui_expand_is_vertical(dvui_Expand e) { return e == DVUI_EXPAND_VERTICAL || e == DVUI_EXPAND_BOTH; }

bool dvui_color_is_set(dvui_Color c) { return c.r != 0 || c.g != 0 || c.b != 0 || c.a != 0; }

// --- Seznam vlastností

static_assert(DVUI_PROP_OPTIONS_COUNT_ <= 32, "bity společných vlastností se musí vejít do dvui_Options.set");

static const char* const prop_names[DVUI_PROP_COUNT] = {
    [DVUI_PROP_END] = "(konec)",
#define DVUI_X(name, type, def) [DVUI_PROP_##name] = #name,
    DVUI_OPTION_PROPS(DVUI_X) DVUI_WIDGET_PROPS(DVUI_X)
#undef DVUI_X
};

dvui_Options dvui_options_default(void) {
    return (dvui_Options){
#define DVUI_X(name, type, def) .name = def,
        DVUI_OPTION_PROPS(DVUI_X)
#undef DVUI_X
        .set = 0,
    };
}

bool dvui_options_has(const dvui_Options* o, uint32_t bits) { return (o->set & bits) != 0; }

// Aplikuje jednu položku na Options; vrací false, pokud nejde o společnou
// vlastnost (ani o vložené Options).
static bool options_apply_one(dvui_Options* o, const dvui_Prop* p) {
    switch (p->tag) {
#define DVUI_X(name, type, def)    \
    case DVUI_PROP_##name:         \
        o->name = p->name;         \
        o->set |= DVUI_BIT(name);  \
        return true;
        DVUI_OPTION_PROPS(DVUI_X)
#undef DVUI_X
    case DVUI_PROP_options:
        if (p->options) *o = dvui_options_override(*o, *p->options);
        return true;
    default: return false;
    }
}

dvui_Options dvui_options_apply(dvui_Options self, dvui_Props props) {
    for (const dvui_Prop* p = props.items; p && p->tag != DVUI_PROP_END; p++) options_apply_one(&self, p);
    return self;
}

static bool tag_allowed(const dvui_PropTag* allowed, dvui_PropTag tag) {
    for (const dvui_PropTag* a = allowed; a && *a != DVUI_PROP_END; a++) {
        if (*a == tag) return true;
    }
    return false;
}

static void props_resolve_list(const char* widget, const dvui_Prop* p, const dvui_PropTag* allowed, dvui_Options* opts,
                               dvui_WidgetProps* wp) {
    for (; p && p->tag != DVUI_PROP_END; p++) {
        if (options_apply_one(opts, p)) continue;
        if ((unsigned)p->tag >= DVUI_PROP_COUNT || wp == nullptr || !tag_allowed(allowed, p->tag)) {
            DVUI_LOG_ERR("%s: neznámá vlastnost %s", widget,
                         (unsigned)p->tag < DVUI_PROP_COUNT ? prop_names[p->tag] : "(neplatná značka)");
            continue;
        }
        switch (p->tag) {
#define DVUI_X(name, type, def) \
    case DVUI_PROP_##name: wp->name = p->name; break;
            DVUI_WIDGET_PROPS(DVUI_X)
#undef DVUI_X
        default: break;
        }
        wp->has[p->tag] = true;
    }
}

void dvui_props_resolve(const char* widget, const dvui_Prop* defaults, dvui_Props props, const dvui_PropTag* allowed,
                        dvui_Options* opts, dvui_WidgetProps* wp) {
    *opts = dvui_options_default();
    if (wp) {
        *wp = (dvui_WidgetProps){
#define DVUI_X(name, type, def) .name = def,
            DVUI_WIDGET_PROPS(DVUI_X)
#undef DVUI_X
        };
    }
    props_resolve_list(widget, defaults, allowed, opts, wp);
    props_resolve_list(widget, props.items, allowed, opts, wp);
}

size_t dvui_props_forward(const dvui_WidgetProps* wp, const dvui_PropTag* tags, dvui_Prop* out) {
    size_t n = 0;
    for (const dvui_PropTag* t = tags; t && *t != DVUI_PROP_END; t++) {
        if ((unsigned)*t >= DVUI_PROP_COUNT || !wp->has[*t]) continue;
        dvui_Prop p = {.tag = *t};
        switch (*t) {
#define DVUI_X(name, type, def) \
    case DVUI_PROP_##name: p.name = wp->name; break;
            DVUI_WIDGET_PROPS(DVUI_X)
#undef DVUI_X
        default: continue; // společné vlastnosti WidgetProps nenese
        }
        out[n++] = p;
    }
    return n;
}

// --- Options

dvui_Options dvui_options_override(dvui_Options self, dvui_Options over) {
    dvui_Options ret = self;
#define DVUI_X(name, type, def) \
    if (over.set & DVUI_BIT(name)) ret.name = over.name;
    DVUI_OPTION_PROPS(DVUI_X)
#undef DVUI_X
    ret.set |= over.set;
    return ret;
}

// Bity vlastností, které strip i styleOnly zachovávají.
static const uint32_t style_bits = DVUI_BIT(style) | DVUI_BIT(color_fill) | DVUI_BIT(color_fill_hover) |
                                   DVUI_BIT(color_fill_press) | DVUI_BIT(color_text) | DVUI_BIT(color_text_hover) |
                                   DVUI_BIT(color_text_press) | DVUI_BIT(color_border);

static dvui_Options options_keep_style(dvui_Options self) {
    dvui_Options ret = dvui_options_default();
    ret.style = self.style;
    ret.color_fill = self.color_fill;
    ret.color_fill_hover = self.color_fill_hover;
    ret.color_fill_press = self.color_fill_press;
    ret.color_text = self.color_text;
    ret.color_text_hover = self.color_text_hover;
    ret.color_text_press = self.color_text_press;
    ret.color_border = self.color_border;
    ret.set = self.set & style_bits;
    return ret;
}

dvui_Options dvui_options_strip(dvui_Options self) {
    dvui_Options ret = options_keep_style(self);
    // DVUI: margin, border, padding a background jsou explicitně nastavené
    // (přebijí výchozí hodnoty vnitřních widgetů).
    ret.margin = (dvui_Rect){0};
    ret.border = (dvui_Rect){0};
    ret.padding = (dvui_Rect){0};
    ret.background = false;
    ret.set |= DVUI_BIT(margin) | DVUI_BIT(border) | DVUI_BIT(padding) | DVUI_BIT(background);
    return ret;
}

dvui_Options dvui_options_style_only(dvui_Options self) { return options_keep_style(self); }

dvui_Gravity dvui_options_gravity_get(const dvui_Options* o) { return (dvui_Gravity){o->gravity_x, o->gravity_y}; }

dvui_Size dvui_options_pad_size(const dvui_Options* o, dvui_Size s) {
    return dvui_size_pad(dvui_size_pad(dvui_size_pad(s, o->padding), o->border), o->margin);
}

dvui_Size dvui_options_min_size_get(const dvui_Options* o) {
    return dvui_options_pad_size(o, dvui_size_min(o->min_size_content, o->max_size_content));
}

dvui_Size dvui_options_max_size_get(const dvui_Options* o) { return dvui_options_pad_size(o, o->max_size_content); }

dvui_Color dvui_options_color(const dvui_Options* o, dvui_ColorAsk ask) {
    const dvui_Theme* t = dvui_theme_get();
    switch (ask) {
    case DVUI_COLOR_BORDER:
        if (dvui_options_has(o, DVUI_BIT(color_border))) return o->color_border;
        break;
    case DVUI_COLOR_FILL:
        if (dvui_options_has(o, DVUI_BIT(color_fill))) return o->color_fill;
        break;
    case DVUI_COLOR_FILL_HOVER:
        if (dvui_options_has(o, DVUI_BIT(color_fill_hover))) return o->color_fill_hover;
        if (dvui_options_has(o, DVUI_BIT(color_fill))) return dvui_theme_adjust_color_for_state(t, o->color_fill, ask);
        break;
    case DVUI_COLOR_FILL_PRESS:
        if (dvui_options_has(o, DVUI_BIT(color_fill_press))) return o->color_fill_press;
        if (dvui_options_has(o, DVUI_BIT(color_fill))) return dvui_theme_adjust_color_for_state(t, o->color_fill, ask);
        break;
    case DVUI_COLOR_TEXT:
        if (dvui_options_has(o, DVUI_BIT(color_text))) return o->color_text;
        break;
    case DVUI_COLOR_TEXT_HOVER:
        if (dvui_options_has(o, DVUI_BIT(color_text_hover))) return o->color_text_hover;
        if (dvui_options_has(o, DVUI_BIT(color_text))) return dvui_theme_adjust_color_for_state(t, o->color_text, ask);
        break;
    case DVUI_COLOR_TEXT_PRESS:
        if (dvui_options_has(o, DVUI_BIT(color_text_press))) return o->color_text_press;
        if (dvui_options_has(o, DVUI_BIT(color_text))) return dvui_theme_adjust_color_for_state(t, o->color_text, ask);
        break;
    }
    return dvui_theme_color(t, o->style, ask);
}

// ---------------------------------------------------------------------------
// placeIn (DVUI: dvui.zig – placeIn)
// ---------------------------------------------------------------------------

dvui_Rect dvui_place_in(dvui_Rect avail, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    dvui_Size size = min_size;

    // nikdy nedostaneš víc, než je k dispozici
    size.w = DVUI_MIN(size.w, avail.w);
    size.h = DVUI_MIN(size.h, avail.h);

    switch (e) {
    case DVUI_EXPAND_NONE: break;
    case DVUI_EXPAND_HORIZONTAL: size.w = avail.w; break;
    case DVUI_EXPAND_VERTICAL: size.h = avail.h; break;
    case DVUI_EXPAND_BOTH: size = dvui_rect_size(avail); break;
    case DVUI_EXPAND_RATIO:
        if (min_size.w > 0 && min_size.h > 0 && avail.w > 0 && avail.h > 0) {
            const float ratio = min_size.w / min_size.h;
            if (min_size.w > avail.w || min_size.h > avail.h) {
                // zmenšování
                const float wratio = avail.w / min_size.w;
                const float hratio = avail.h / min_size.h;
                if (wratio < hratio) {
                    // omezuje šířka
                    size.w = avail.w;
                    size.h = DVUI_MIN(avail.h, wratio * min_size.h);
                } else {
                    // omezuje výška
                    size.h = avail.h;
                    size.w = DVUI_MIN(avail.w, hratio * min_size.w);
                }
            } else {
                // zvětšování
                const float aratio = (avail.w - size.w) / (avail.h - size.h);
                if (aratio > ratio) {
                    // omezuje výška
                    size.w = DVUI_MIN(avail.w, avail.h * ratio);
                    size.h = avail.h;
                } else {
                    // omezuje šířka
                    size.w = avail.w;
                    size.h = DVUI_MIN(avail.h, avail.w / ratio);
                }
            }
        }
        break;
    }

    dvui_Rect r = dvui_rect_shrink_to_size(avail, size);
    r.x = avail.x + g.x * (avail.w - r.w);
    r.y = avail.y + g.y * (avail.h - r.h);
    return r;
}
