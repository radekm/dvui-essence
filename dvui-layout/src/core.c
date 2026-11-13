// Geometrie, barvy, Options, téma a placeIn.
// DVUI: Rect.zig, Size.zig, Point.zig, RectScale.zig, Color.zig, Options.zig,
//       Theme.zig, dvui.zig (placeIn)
#include "internal.h"

#include <math.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Rect (DVUI: Rect.zig)
// ---------------------------------------------------------------------------

dvui_Rect dvui_rect_all(float v) { return (dvui_Rect){v, v, v, v}; }
dvui_Rect dvui_rect_from_size(dvui_Size s) { return (dvui_Rect){0, 0, s.w, s.h}; }
dvui_Rect dvui_rect_from_point(dvui_Point p) { return (dvui_Rect){p.x, p.y, 0, 0}; }
dvui_Rect dvui_rect_to_size(dvui_Rect r, dvui_Size s) { return (dvui_Rect){r.x, r.y, s.w, s.h}; }
dvui_Rect dvui_rect_just_size(dvui_Rect r) { return (dvui_Rect){0, 0, r.w, r.h}; }
dvui_Size dvui_rect_size(dvui_Rect r) { return (dvui_Size){r.w, r.h}; }
dvui_Point dvui_rect_top_left(dvui_Rect r) { return (dvui_Point){r.x, r.y}; }
dvui_Point dvui_rect_top_right(dvui_Rect r) { return (dvui_Point){r.x + r.w, r.y}; }
dvui_Point dvui_rect_bottom_left(dvui_Rect r) { return (dvui_Point){r.x, r.y + r.h}; }
dvui_Point dvui_rect_bottom_right(dvui_Rect r) { return (dvui_Point){r.x + r.w, r.y + r.h}; }
dvui_Point dvui_rect_center(dvui_Rect r) { return (dvui_Point){r.x + r.w / 2, r.y + r.h / 2}; }

bool dvui_rect_contains(dvui_Rect r, dvui_Point p) {
    return p.x >= r.x && p.x <= (r.x + r.w) && p.y >= r.y && p.y <= (r.y + r.h);
}

bool dvui_rect_empty(dvui_Rect r) { return r.w == 0 || r.h == 0; }

bool dvui_rect_equals(dvui_Rect a, dvui_Rect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool dvui_rect_non_zero(dvui_Rect r) { return r.x != 0 || r.y != 0 || r.w != 0 || r.h != 0; }

dvui_Rect dvui_rect_scale(dvui_Rect r, float s) { return (dvui_Rect){r.x * s, r.y * s, r.w * s, r.h * s}; }
dvui_Rect dvui_rect_offset(dvui_Rect r, dvui_Rect by) { return (dvui_Rect){r.x + by.x, r.y + by.y, r.w, r.h}; }
dvui_Rect dvui_rect_offset_point(dvui_Rect r, dvui_Point p) { return (dvui_Rect){r.x + p.x, r.y + p.y, r.w, r.h}; }
dvui_Rect dvui_rect_offset_neg(dvui_Rect r, dvui_Rect by) { return (dvui_Rect){r.x - by.x, r.y - by.y, r.w, r.h}; }
dvui_Rect dvui_rect_offset_neg_point(dvui_Rect r, dvui_Point p) { return (dvui_Rect){r.x - p.x, r.y - p.y, r.w, r.h}; }

dvui_Rect dvui_rect_intersect(dvui_Rect a, dvui_Rect b) {
    const float ax2 = a.x + a.w, ay2 = a.y + a.h;
    const float bx2 = b.x + b.w, by2 = b.y + b.h;
    const float x = DVUI_MAX(a.x, b.x), y = DVUI_MAX(a.y, b.y);
    const float x2 = DVUI_MIN(ax2, bx2), y2 = DVUI_MIN(ay2, by2);
    return (dvui_Rect){x, y, DVUI_MAX(0.0f, x2 - x), DVUI_MAX(0.0f, y2 - y)};
}

dvui_Rect dvui_rect_union(dvui_Rect a, dvui_Rect b) {
    const float ax2 = a.x + a.w, ay2 = a.y + a.h;
    const float bx2 = b.x + b.w, by2 = b.y + b.h;
    const float x = DVUI_MIN(a.x, b.x), y = DVUI_MIN(a.y, b.y);
    const float x2 = DVUI_MAX(ax2, bx2), y2 = DVUI_MAX(ay2, by2);
    return (dvui_Rect){x, y, DVUI_MAX(0.0f, x2 - x), DVUI_MAX(0.0f, y2 - y)};
}

dvui_Rect dvui_rect_shrink_to_size(dvui_Rect r, dvui_Size s) {
    return (dvui_Rect){r.x, r.y, DVUI_MIN(r.w, s.w), DVUI_MIN(r.h, s.h)};
}

dvui_Rect dvui_rect_inset(dvui_Rect r, dvui_Rect by) {
    return (dvui_Rect){r.x + by.x, r.y + by.y, DVUI_MAX(0.0f, r.w - by.x - by.w), DVUI_MAX(0.0f, r.h - by.y - by.h)};
}

dvui_Rect dvui_rect_inset_all(dvui_Rect r, float p) { return dvui_rect_inset(r, dvui_rect_all(p)); }

dvui_Rect dvui_rect_outset(dvui_Rect r, dvui_Rect by) {
    return (dvui_Rect){r.x - by.x, r.y - by.y, r.w + by.x + by.w, r.h + by.y + by.h};
}

bool dvui_rect_visible(dvui_RectPhysical r) { return !dvui_rect_empty(dvui_rect_intersect(dvui_clip_get(), r)); }

dvui_RectNatural dvui_rect_to_natural(dvui_RectPhysical r) { return dvui_rect_scale(r, 1.0f / dvui_window_natural_scale()); }

// ---------------------------------------------------------------------------
// Size, Point (DVUI: Size.zig, Point.zig)
// ---------------------------------------------------------------------------

dvui_Size dvui_size_all(float v) { return (dvui_Size){v, v}; }
dvui_Size dvui_size_pad(dvui_Size s, dvui_Rect p) { return (dvui_Size){s.w + p.x + p.w, s.h + p.y + p.h}; }
dvui_Size dvui_size_pad_neg(dvui_Size s, dvui_Rect p) {
    return (dvui_Size){DVUI_MAX(0.0f, s.w - p.x - p.w), DVUI_MAX(0.0f, s.h - p.y - p.h)};
}
dvui_Size dvui_size_max(dvui_Size a, dvui_Size b) { return (dvui_Size){DVUI_MAX(a.w, b.w), DVUI_MAX(a.h, b.h)}; }
dvui_Size dvui_size_min(dvui_Size a, dvui_Size b) { return (dvui_Size){DVUI_MIN(a.w, b.w), DVUI_MIN(a.h, b.h)}; }
dvui_Size dvui_size_scale(dvui_Size s, float scale) { return (dvui_Size){s.w * scale, s.h * scale}; }

dvui_Point dvui_point_plus(dvui_Point a, dvui_Point b) { return (dvui_Point){a.x + b.x, a.y + b.y}; }
dvui_Point dvui_point_diff(dvui_Point a, dvui_Point b) { return (dvui_Point){a.x - b.x, a.y - b.y}; }
dvui_Point dvui_point_scale(dvui_Point p, float s) { return (dvui_Point){p.x * s, p.y * s}; }
bool dvui_point_non_zero(dvui_Point p) { return p.x != 0 || p.y != 0; }
bool dvui_point_equals(dvui_Point a, dvui_Point b) { return a.x == b.x && a.y == b.y; }

// ---------------------------------------------------------------------------
// RectScale (DVUI: RectScale.zig)
// ---------------------------------------------------------------------------

dvui_RectScale dvui_rect_scale_rect_to_rect_scale(dvui_RectScale rs, dvui_Rect r) {
    return (dvui_RectScale){.r = dvui_rect_offset(dvui_rect_scale(r, rs.s), rs.r), .s = rs.s};
}

dvui_RectPhysical dvui_rect_scale_rect_to_physical(dvui_RectScale rs, dvui_Rect r) {
    return dvui_rect_offset(dvui_rect_scale(r, rs.s), rs.r);
}

dvui_Rect dvui_rect_scale_rect_from_physical(dvui_RectScale rs, dvui_RectPhysical r) {
    return dvui_rect_scale(dvui_rect_offset_neg(r, rs.r), 1.0f / rs.s);
}

dvui_PointPhysical dvui_rect_scale_point_to_physical(dvui_RectScale rs, dvui_Point p) {
    return dvui_point_plus(dvui_point_scale(p, rs.s), dvui_rect_top_left(rs.r));
}

dvui_Point dvui_rect_scale_point_from_physical(dvui_RectScale rs, dvui_PointPhysical p) {
    return dvui_point_scale(dvui_point_diff(p, dvui_rect_top_left(rs.r)), 1.0f / rs.s);
}

// ---------------------------------------------------------------------------
// Color (DVUI: Color.zig)
// ---------------------------------------------------------------------------

dvui_Color dvui_color_opacity(dvui_Color c, float mult) {
    c.a = (uint8_t)(dvui_clampf((float)c.a * mult, 0, 255));
    return c;
}

dvui_Color dvui_color_lerp(dvui_Color a, dvui_Color b, float t) {
    // DVUI: Color.lerp – interpolace po složkách
    return (dvui_Color){
        (uint8_t)((float)a.r * (1 - t) + (float)b.r * t),
        (uint8_t)((float)a.g * (1 - t) + (float)b.g * t),
        (uint8_t)((float)a.b * (1 - t) + (float)b.b * t),
        (uint8_t)((float)a.a * (1 - t) + (float)b.a * t),
    };
}

dvui_Color dvui_color_average(dvui_Color a, dvui_Color b) {
    return (dvui_Color){
        (uint8_t)(((unsigned)a.r + b.r) / 2),
        (uint8_t)(((unsigned)a.g + b.g) / 2),
        (uint8_t)(((unsigned)a.b + b.b) / 2),
        (uint8_t)(((unsigned)a.a + b.a) / 2),
    };
}

// Převod sRGB ↔ CIELUV. DVUI zesvětluje v HSLuv (mění L při zachování
// relativní sytosti); zde se mění L v CIE LCh(uv) při zachování chroma.
// Pro šedé barvy (s = 0) dávají oba postupy stejný výsledek.
static float srgb_to_linear(float c) { return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f); }
static float linear_to_srgb(float c) { return c <= 0.0031308f ? 12.92f * c : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f; }

static const float ref_u = 0.19783000664283f;
static const float ref_v = 0.46831999493879f;
static const float kappa = 903.2962962f;
static const float epsilon = 0.0088564516f;

static float y_to_l(float y) { return y <= epsilon ? y * kappa : 116.0f * cbrtf(y) - 16.0f; }
static float l_to_y(float l) {
    if (l <= 8.0f) return l / kappa;
    const float t = (l + 16.0f) / 116.0f;
    return t * t * t;
}

static uint8_t to_u8(float c) { return (uint8_t)truncf(dvui_clampf(c, 0, 1) * 255.99f); }

dvui_Color dvui_color_from_lightness(float l) {
    const float c = linear_to_srgb(l_to_y(l));
    return (dvui_Color){to_u8(c), to_u8(c), to_u8(c), 255};
}

dvui_Color dvui_color_lighten(dvui_Color col, float delta_l) {
    const float r = srgb_to_linear((float)col.r / 255.0f);
    const float g = srgb_to_linear((float)col.g / 255.0f);
    const float b = srgb_to_linear((float)col.b / 255.0f);
    const float x = 0.41239079926595f * r + 0.35758433938387f * g + 0.18048078840183f * b;
    const float y = 0.21263900587151f * r + 0.71516867876775f * g + 0.072192315360733f * b;
    const float z = 0.019330818715591f * r + 0.11919477979462f * g + 0.95053215224966f * b;
    const float l = y_to_l(y);
    if (l == 0) return col;
    const float div = x + 15.0f * y + 3.0f * z;
    const float var_u = div == 0 ? 0 : 4.0f * x / div;
    const float var_v = div == 0 ? 0 : 9.0f * y / div;
    const float u = 13.0f * l * (var_u - ref_u);
    const float v = 13.0f * l * (var_v - ref_v);

    const float l2 = dvui_clampf(l + delta_l, 0, 100);
    if (l2 == 0) return (dvui_Color){0, 0, 0, col.a};
    // zachováme chroma i odstín: u, v se nemění
    const float y2 = l_to_y(l2);
    const float vu = u / (13.0f * l2) + ref_u;
    const float vv = v / (13.0f * l2) + ref_v;
    const float x2 = 0 - (9.0f * y2 * vu) / ((vu - 4.0f) * vv - vu * vv);
    const float z2 = (9.0f * y2 - (15.0f * vv * y2) - (vv * x2)) / (3.0f * vv);
    const float r2 = 3.240969941904521f * x2 - 1.537383177570093f * y2 - 0.498610760293f * z2;
    const float g2 = -0.96924363628087f * x2 + 1.87596750150772f * y2 + 0.041555057407175f * z2;
    const float b2 = 0.055630079696993f * x2 - 0.20397695888897f * y2 + 1.056971514242878f * z2;
    return (dvui_Color){to_u8(linear_to_srgb(r2)), to_u8(linear_to_srgb(g2)), to_u8(linear_to_srgb(b2)), col.a};
}

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
        .fill_hover = DVUI_SOME(dvui_color_from_lightness(82)),
        .fill_press = DVUI_SOME(dvui_color_from_lightness(72)),
        .text = DVUI_RGB(0, 0, 0),
        .text_select = DVUI_RGB(0x91, 0xbc, 0xf0),
        .border = dvui_color_from_lightness(63),
        .control = {
            .fill = DVUI_SOME(DVUI_RGB(0xe0, 0xe0, 0xe0)),
            .fill_hover = DVUI_SOME(dvui_color_from_lightness(82)),
            .fill_press = DVUI_SOME(dvui_color_from_lightness(72)),
        },
        .window = {
            .fill = DVUI_SOME(DVUI_RGB(0xf0, 0xf0, 0xf0)),
        },
        .highlight = {
            .fill = DVUI_SOME(accent),
            .fill_hover = DVUI_SOME(dvui_color_lighten(accent, -11)),
            .fill_press = DVUI_SOME(dvui_color_lighten(accent, -16)),
            .text = DVUI_SOME(white),
            .border = DVUI_SOME(dvui_color_lighten(accent, -22)),
        },
        .err = {
            .fill = DVUI_SOME(err),
            .fill_hover = DVUI_SOME(dvui_color_lighten(err, -10)),
            .fill_press = DVUI_SOME(dvui_color_lighten(err, -15)),
            .text = DVUI_SOME(white),
            .border = DVUI_SOME(dvui_color_lighten(err, -20)),
        },
    };
}

dvui_Color dvui_theme_adjust_color_for_state(const dvui_Theme *t, dvui_Color col, dvui_ColorAsk ask) {
    switch (ask) {
    case DVUI_COLOR_FILL_HOVER: return dvui_color_lighten(col, t->dark ? 10 : -10);
    case DVUI_COLOR_FILL_PRESS: return dvui_color_lighten(col, t->dark ? 20 : -20);
    default: return col;
    }
}

// DVUI: Theme.color. Zig `continue :sw .fill` zachovává původní `ask`, proto
// se zde předává `ask` dál beze změny.
dvui_Color dvui_theme_color(const dvui_Theme *t, dvui_StyleName style, dvui_ColorAsk ask) {
    const dvui_ThemeStyle *cs = nullptr;
    switch (style) {
    case DVUI_STYLE_CONTENT:
        switch (ask) {
        case DVUI_COLOR_BORDER: return t->border;
        case DVUI_COLOR_FILL: return dvui_theme_adjust_color_for_state(t, t->fill, ask);
        case DVUI_COLOR_FILL_HOVER:
            return t->fill_hover.has ? t->fill_hover.v : dvui_theme_adjust_color_for_state(t, t->fill, ask);
        case DVUI_COLOR_FILL_PRESS:
            return t->fill_press.has ? t->fill_press.v : dvui_theme_adjust_color_for_state(t, t->fill, ask);
        case DVUI_COLOR_TEXT: return t->text;
        case DVUI_COLOR_TEXT_HOVER: return t->text_hover.has ? t->text_hover.v : t->text;
        case DVUI_COLOR_TEXT_PRESS: return t->text_press.has ? t->text_press.v : t->text;
        }
        return t->fill;
    case DVUI_STYLE_CONTROL: cs = &t->control; break;
    case DVUI_STYLE_WINDOW: cs = &t->window; break;
    case DVUI_STYLE_HIGHLIGHT: cs = &t->highlight; break;
    case DVUI_STYLE_ERR: cs = &t->err; break;
    }

    switch (ask) {
    case DVUI_COLOR_BORDER: return cs->border.has ? cs->border.v : dvui_theme_color(t, DVUI_STYLE_CONTENT, ask);
    case DVUI_COLOR_FILL_HOVER:
        if (cs->fill_hover.has) return cs->fill_hover.v;
        goto fill;
    case DVUI_COLOR_FILL_PRESS:
        if (cs->fill_press.has) return cs->fill_press.v;
        goto fill;
    case DVUI_COLOR_FILL:
    fill:
        return cs->fill.has ? dvui_theme_adjust_color_for_state(t, cs->fill.v, ask) : dvui_theme_color(t, DVUI_STYLE_CONTENT, ask);
    case DVUI_COLOR_TEXT_HOVER:
        if (cs->text_hover.has) return cs->text_hover.v;
        goto text;
    case DVUI_COLOR_TEXT_PRESS:
        if (cs->text_press.has) return cs->text_press.v;
        goto text;
    case DVUI_COLOR_TEXT:
    text:
        return cs->text.has ? cs->text.v : dvui_theme_color(t, DVUI_STYLE_CONTENT, ask);
    }
    return t->fill;
}

dvui_Size dvui_font_size_m(float wide, float tall) {
    // Přibližná velikost „M“ fontu Vera Sans velikosti 10 (výchozí v DVUI).
    const dvui_Size m = {9.0f, 12.0f};
    return (dvui_Size){m.w * wide, m.h * tall};
}

// ---------------------------------------------------------------------------
// Options (DVUI: Options.zig)
// ---------------------------------------------------------------------------

bool dvui_expand_is_horizontal(dvui_Expand e) { return e == DVUI_EXPAND_HORIZONTAL || e == DVUI_EXPAND_BOTH; }
bool dvui_expand_is_vertical(dvui_Expand e) { return e == DVUI_EXPAND_VERTICAL || e == DVUI_EXPAND_BOTH; }

dvui_Options dvui_options_override(dvui_Options self, dvui_Options over) {
    dvui_Options ret = self;
#define OVR(f) \
    if (over.f.has) ret.f = over.f
    OVR(id_extra);
    if (over.name) ret.name = over.name;
    OVR(rect);
    OVR(expand);
    OVR(gravity_x);
    OVR(gravity_y);
    OVR(color_fill);
    OVR(color_fill_hover);
    OVR(color_fill_press);
    OVR(color_text);
    OVR(color_text_hover);
    OVR(color_text_press);
    OVR(color_border);
    OVR(style);
    OVR(margin);
    OVR(border);
    OVR(padding);
    OVR(min_size_content);
    OVR(max_size_content);
    OVR(background);
#undef OVR
    return ret;
}

dvui_Options dvui_options_strip(dvui_Options self) {
    return (dvui_Options){
        .margin = DVUI_SOME((dvui_Rect){0}),
        .border = DVUI_SOME((dvui_Rect){0}),
        .padding = DVUI_SOME((dvui_Rect){0}),
        .background = DVUI_SOME(false),

        // zbytek ponecháme
        .style = self.style,
        .color_fill = self.color_fill,
        .color_fill_hover = self.color_fill_hover,
        .color_fill_press = self.color_fill_press,
        .color_text = self.color_text,
        .color_text_hover = self.color_text_hover,
        .color_text_press = self.color_text_press,
        .color_border = self.color_border,
    };
}

dvui_Options dvui_options_style_only(dvui_Options self) {
    return (dvui_Options){
        .style = self.style,
        .color_fill = self.color_fill,
        .color_fill_hover = self.color_fill_hover,
        .color_fill_press = self.color_fill_press,
        .color_text = self.color_text,
        .color_text_hover = self.color_text_hover,
        .color_text_press = self.color_text_press,
        .color_border = self.color_border,
    };
}

size_t dvui_options_id_extra(const dvui_Options *o) { return o->id_extra.has ? o->id_extra.v : 0; }
dvui_Expand dvui_options_expand_get(const dvui_Options *o) { return o->expand.has ? o->expand.v : DVUI_EXPAND_NONE; }
dvui_Gravity dvui_options_gravity_get(const dvui_Options *o) {
    return (dvui_Gravity){o->gravity_x.has ? o->gravity_x.v : 0.0f, o->gravity_y.has ? o->gravity_y.v : 0.0f};
}
dvui_Rect dvui_options_margin_get(const dvui_Options *o) { return o->margin.has ? o->margin.v : (dvui_Rect){0}; }
dvui_Rect dvui_options_border_get(const dvui_Options *o) { return o->border.has ? o->border.v : (dvui_Rect){0}; }
dvui_Rect dvui_options_padding_get(const dvui_Options *o) { return o->padding.has ? o->padding.v : (dvui_Rect){0}; }
bool dvui_options_background_get(const dvui_Options *o) { return o->background.has ? o->background.v : false; }
dvui_StyleName dvui_options_style_get(const dvui_Options *o) { return o->style.has ? o->style.v : DVUI_STYLE_CONTENT; }

dvui_Size dvui_options_pad_size(const dvui_Options *o, dvui_Size s) {
    return dvui_size_pad(dvui_size_pad(dvui_size_pad(s, dvui_options_padding_get(o)), dvui_options_border_get(o)), dvui_options_margin_get(o));
}

dvui_Size dvui_options_min_size_content_get(const dvui_Options *o) {
    return o->min_size_content.has ? o->min_size_content.v : (dvui_Size){0};
}

dvui_Size dvui_options_max_size_content_get(const dvui_Options *o) {
    return o->max_size_content.has ? o->max_size_content.v : dvui_size_all(DVUI_MAX_FLOAT_SAFE);
}

dvui_Size dvui_options_min_size_get(const dvui_Options *o) {
    return dvui_options_pad_size(o, dvui_size_min(dvui_options_min_size_content_get(o), dvui_options_max_size_content_get(o)));
}

dvui_Size dvui_options_max_size_get(const dvui_Options *o) {
    return dvui_options_pad_size(o, dvui_options_max_size_content_get(o));
}

dvui_Color dvui_options_color(const dvui_Options *o, dvui_ColorAsk ask) {
    const dvui_Theme *t = dvui_theme_get();
    switch (ask) {
    case DVUI_COLOR_BORDER:
        if (o->color_border.has) return o->color_border.v;
        break;
    case DVUI_COLOR_FILL:
        if (o->color_fill.has) return o->color_fill.v;
        break;
    case DVUI_COLOR_FILL_HOVER:
        if (o->color_fill_hover.has) return o->color_fill_hover.v;
        if (o->color_fill.has) return dvui_theme_adjust_color_for_state(t, o->color_fill.v, ask);
        break;
    case DVUI_COLOR_FILL_PRESS:
        if (o->color_fill_press.has) return o->color_fill_press.v;
        if (o->color_fill.has) return dvui_theme_adjust_color_for_state(t, o->color_fill.v, ask);
        break;
    case DVUI_COLOR_TEXT:
        if (o->color_text.has) return o->color_text.v;
        break;
    case DVUI_COLOR_TEXT_HOVER:
        if (o->color_text_hover.has) return o->color_text_hover.v;
        if (o->color_text.has) return dvui_theme_adjust_color_for_state(t, o->color_text.v, ask);
        break;
    case DVUI_COLOR_TEXT_PRESS:
        if (o->color_text_press.has) return o->color_text_press.v;
        if (o->color_text.has) return dvui_theme_adjust_color_for_state(t, o->color_text.v, ask);
        break;
    }
    return dvui_theme_color(t, dvui_options_style_get(o), ask);
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

// ---------------------------------------------------------------------------
// Události – pomocné funkce (DVUI: enums.zig)
// ---------------------------------------------------------------------------

bool dvui_button_pointer(dvui_Button b) { return b == DVUI_BUTTON_LEFT; }

bool dvui_mod_shift(unsigned mod) { return (mod & (DVUI_MOD_LSHIFT | DVUI_MOD_RSHIFT)) != 0; }

bool dvui_mod_shift_only(unsigned mod) {
    return mod != 0 && (mod & ~(unsigned)(DVUI_MOD_LSHIFT | DVUI_MOD_RSHIFT)) == 0;
}
