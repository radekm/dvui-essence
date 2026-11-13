// Testovací backend bez GPU – zaznamenává vykreslené obdélníky (sdílený, viz shared/README.md).
// DVUI: src/backends/testing.zig
#pragma once

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dvui_TestingDraw {
    // Ohraničení trojúhelníků (projekty kreslí jen obdélníky).
    dvui_RectPhysical rect;
    dvui_Color color;
    bool clipped;
    dvui_RectPhysical clip;
} dvui_TestingDraw;

typedef struct dvui_TestingBackend {
    dvui_SizePhysical pixel_size;
    dvui_SizeNatural window_size;
    float content_scale;
    int64_t time_ns;
    dvui_Cursor cursor;
    // Vykreslení z posledního snímku.
    dvui_TestingDraw* draws;
    size_t draws_len, draws_cap;
} dvui_TestingBackend;

void dvui_testing_backend_init(dvui_TestingBackend* tb, dvui_Size size);
void dvui_testing_backend_deinit(dvui_TestingBackend* tb);
dvui_Backend dvui_testing_backend(dvui_TestingBackend* tb);

#ifdef __cplusplus
}
#endif
