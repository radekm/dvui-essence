// Easing funkce – mění rychlost změny hodnoty v animaci.
// DVUI: src/easing.zig (převzato z https://gist.github.com/Kryzarel/bba64622057f21a1d6d44879f9cd7bd4)
//
// Některé funkce (elastic, back) vracejí pro t z [0, 1] i hodnoty mimo [0, 1].
// Ukázky průběhů: https://easings.net/
#include "internal.h"

#include <math.h>

static const float pi = 3.14159265358979323846f;

// DVUI: easing.zig – linear
float dvui_easing_linear(float t) { return t; }

// DVUI: easing.zig – inQuad
float dvui_easing_in_quad(float t) { return t * t; }
// DVUI: easing.zig – outQuad
float dvui_easing_out_quad(float t) { return 1 - dvui_easing_in_quad(1 - t); }
// DVUI: easing.zig – inOutQuad
float dvui_easing_in_out_quad(float t) {
    if (t < 0.5f) return dvui_easing_in_quad(t * 2) / 2;
    return 1 - dvui_easing_in_quad((1 - t) * 2) / 2;
}

// DVUI: easing.zig – inCubic
float dvui_easing_in_cubic(float t) { return t * t * t; }
// DVUI: easing.zig – outCubic
float dvui_easing_out_cubic(float t) { return 1 - dvui_easing_in_cubic(1 - t); }
// DVUI: easing.zig – inOutCubic
float dvui_easing_in_out_cubic(float t) {
    if (t < 0.5f) return dvui_easing_in_cubic(t * 2) / 2;
    return 1 - dvui_easing_in_cubic((1 - t) * 2) / 2;
}

// DVUI: easing.zig – inQuart
float dvui_easing_in_quart(float t) { return t * t * t * t; }
// DVUI: easing.zig – outQuart
float dvui_easing_out_quart(float t) { return 1 - dvui_easing_in_quart(1 - t); }
// DVUI: easing.zig – inOutQuart
float dvui_easing_in_out_quart(float t) {
    if (t < 0.5f) return dvui_easing_in_quart(t * 2) / 2;
    return 1 - dvui_easing_in_quart((1 - t) * 2) / 2;
}

// DVUI: easing.zig – inQuint
float dvui_easing_in_quint(float t) { return t * t * t * t * t; }
// DVUI: easing.zig – outQuint
float dvui_easing_out_quint(float t) { return 1 - dvui_easing_in_quint(1 - t); }
// DVUI: easing.zig – inOutQuint
float dvui_easing_in_out_quint(float t) {
    if (t < 0.5f) return dvui_easing_in_quint(t * 2) / 2;
    return 1 - dvui_easing_in_quint((1 - t) * 2) / 2;
}

// DVUI: easing.zig – inSine
float dvui_easing_in_sine(float t) { return 1 - cosf(t * pi / 2); }
// DVUI: easing.zig – outSine
float dvui_easing_out_sine(float t) { return sinf(t * pi / 2); }
// DVUI: easing.zig – inOutSine
float dvui_easing_in_out_sine(float t) { return (cosf(t * pi) - 1) / -2; }

// DVUI: easing.zig – inExpo
float dvui_easing_in_expo(float t) { return powf(2, 10 * (t - 1)); }
// DVUI: easing.zig – outExpo
float dvui_easing_out_expo(float t) { return 1 - dvui_easing_in_expo(1 - t); }
// DVUI: easing.zig – inOutExpo
float dvui_easing_in_out_expo(float t) {
    if (t < 0.5f) return dvui_easing_in_expo(t * 2) / 2;
    return 1 - dvui_easing_in_expo((1 - t) * 2) / 2;
}

// DVUI: easing.zig – inCirc
float dvui_easing_in_circ(float t) { return -(sqrtf(1 - t * t) - 1); }
// DVUI: easing.zig – outCirc
float dvui_easing_out_circ(float t) { return 1 - dvui_easing_in_circ(1 - t); }
// DVUI: easing.zig – inOutCirc
float dvui_easing_in_out_circ(float t) {
    if (t < 0.5f) return dvui_easing_in_circ(t * 2) / 2;
    return 1 - dvui_easing_in_circ((1 - t) * 2) / 2;
}

// DVUI: easing.zig – inElastic (přesahuje 0 a 1)
float dvui_easing_in_elastic(float t) { return 1 - dvui_easing_out_elastic(1 - t); }
// DVUI: easing.zig – outElastic (přesahuje 0 a 1)
float dvui_easing_out_elastic(float t) {
    const float p = 0.3f;
    return powf(2, -10 * t) * sinf((t - p / 4) * (2 * pi) / p) + 1;
}
// DVUI: easing.zig – inOutElastic (přesahuje 0 a 1)
float dvui_easing_in_out_elastic(float t) {
    if (t < 0.5f) return dvui_easing_in_elastic(t * 2) / 2;
    return 1 - dvui_easing_in_elastic((1 - t) * 2) / 2;
}

// DVUI: easing.zig – inBack (přesahuje 0 a 1)
float dvui_easing_in_back(float t) {
    const float s = 1.70158f;
    return t * t * ((s + 1) * t - s);
}
// DVUI: easing.zig – outBack (přesahuje 0 a 1)
float dvui_easing_out_back(float t) { return 1 - dvui_easing_in_back(1 - t); }
// DVUI: easing.zig – inOutBack (přesahuje 0 a 1)
float dvui_easing_in_out_back(float t) {
    if (t < 0.5f) return dvui_easing_in_back(t * 2) / 2;
    return 1 - dvui_easing_in_back((1 - t) * 2) / 2;
}

// DVUI: easing.zig – inBounce
float dvui_easing_in_bounce(float t) { return 1 - dvui_easing_out_bounce(1 - t); }
// DVUI: easing.zig – outBounce
float dvui_easing_out_bounce(float t) {
    const float div = 2.75f;
    const float mult = 7.5625f;

    if (t < 1 / div) {
        return mult * t * t;
    } else if (t < 2 / div) {
        const float x = t - 1.5f / div;
        return mult * x * x + 0.75f;
    } else if (t < 2.5f / div) {
        const float x = t - 2.25f / div;
        return mult * x * x + 0.9375f;
    } else {
        const float x = t - 2.625f / div;
        return mult * x * x + 0.984375f;
    }
}
// DVUI: easing.zig – inOutBounce
float dvui_easing_in_out_bounce(float t) {
    if (t < 0.5f) return dvui_easing_in_bounce(t * 2) / 2;
    return 1 - dvui_easing_in_bounce((1 - t) * 2) / 2;
}

// Seznam všech easingů (DVUI Examples si ho skládá v době překladu přes
// std.meta.declarations).
const dvui_EasingInfo dvui_easings[] = {
#define DVUI_X(name) {#name, dvui_easing_##name},
    DVUI_EASINGS(DVUI_X)
#undef DVUI_X
};
const size_t dvui_easings_len = sizeof dvui_easings / sizeof dvui_easings[0];
