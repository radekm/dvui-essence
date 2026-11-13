// Geometrie a barvy.
// DVUI: Rect.zig, Size.zig, Point.zig, RectScale.zig, Color.zig
#include "dvui/base.h"
#include "dvui/util.h"

#include <math.h>

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
