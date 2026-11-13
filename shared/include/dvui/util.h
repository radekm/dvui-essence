// Drobní pomocníci sdílení všemi projekty (logování, min/max).
// Není to veřejné API pro uživatele knihovny – používá ho kód projektů
// a shared/.
#pragma once

#include <stdio.h>

// ---------------------------------------------------------------------------
// Logování (DVUI: dvui.log)
// ---------------------------------------------------------------------------

#define DVUI_LOG_ERR(...) (fprintf(stderr, "dvui error: " __VA_ARGS__), fputc('\n', stderr))
#define DVUI_LOG_DEBUG(...) ((void)0)

#define DVUI_MIN(a, b) ((a) < (b) ? (a) : (b))
#define DVUI_MAX(a, b) ((a) > (b) ? (a) : (b))

static inline float dvui_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
