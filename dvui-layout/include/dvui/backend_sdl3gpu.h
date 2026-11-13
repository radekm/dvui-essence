// Backend nad SDL 3 GPU.
// DVUI: src/backends/sdl3gpu.zig
#pragma once

#include "dvui.h"

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dvui_Sdl3GpuBackend dvui_Sdl3GpuBackend;

typedef struct dvui_Sdl3GpuInitOptions {
    const char *title;
    // Velikost okna v přirozených (natural) pixelech.
    dvui_SizeNatural size;
    dvui_OptSize min_size;
    // DVUI výchozí vsync = true; zde opačně kvůli {0}.
    bool no_vsync;
    bool hidden;
    // Barva, kterou se maže okno na začátku snímku (výchozí: pozadí okna
    // tématu Adwaita Light, #f0f0f0).
    dvui_OptColor clear_color;
} dvui_Sdl3GpuInitOptions;

// DVUI: SDLBackend.initWindow – vytvoří okno, GPU zařízení a pipeline.
// Vrací NULL při chybě (viz SDL_GetError).
dvui_Sdl3GpuBackend *dvui_sdl3gpu_create(dvui_Sdl3GpuInitOptions options);
void dvui_sdl3gpu_destroy(dvui_Sdl3GpuBackend *b);
dvui_Backend dvui_sdl3gpu_backend(dvui_Sdl3GpuBackend *b);
SDL_Window *dvui_sdl3gpu_window(dvui_Sdl3GpuBackend *b);
SDL_GPUDevice *dvui_sdl3gpu_device(dvui_Sdl3GpuBackend *b);

// DVUI: SDLBackend.addEvent – předá jednu událost SDL do dvui.
// Vrací true, pokud dvui událost „spotřebovalo“ (je nad floating oknem).
bool dvui_sdl3gpu_add_event(dvui_Sdl3GpuBackend *b, dvui_Window *win, const SDL_Event *event);
// DVUI: SDLBackend.addAllEvents – zpracuje všechny čekající události.
// Vrací true, pokud přišel požadavek na ukončení (SDL_EVENT_QUIT).
bool dvui_sdl3gpu_add_all_events(dvui_Sdl3GpuBackend *b, dvui_Window *win);
// DVUI: SDLBackend.waitEventTimeout – `micros` z dvui_window_end
// (0 = nečekat, -1 = čekat na událost).
void dvui_sdl3gpu_wait_event_timeout(dvui_Sdl3GpuBackend *b, int64_t micros);

#ifdef __cplusplus
}
#endif
