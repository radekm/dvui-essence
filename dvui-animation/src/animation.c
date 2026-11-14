// Animace a časovače.
// DVUI: dvui.zig – Animation, animation, animationGet, timer, timerGet,
//       timerDone, timerDoneOrNone, reduce_motion; Window.zig – timer,
//       timerRemove
//
// Stárnutí animací (posun časů) a jejich mazání je ve window.c
// (dvui_window_begin), výpočet času do dalšího snímku také (dvui_window_end).
#include "internal.h"

#include <stdlib.h>
#include <string.h>

// DVUI: dvui.zig – reduce_motion
bool dvui_reduce_motion = false;

// ---------------------------------------------------------------------------
// Animation (DVUI: dvui.zig – Animation)
// ---------------------------------------------------------------------------

// DVUI: dvui.zig – Animation.value
float dvui_animation_value(const dvui_Animation* a) {
    if (a->start_time >= 0) return a->start_val;
    if (dvui_animation_done(a)) return a->end_val;
    // rozdíl počítáme v int64, aby nepřetekl (Zig by při přetečení spadl)
    const float frac = (float)(-(int64_t)a->start_time) / (float)((int64_t)a->end_time - a->start_time);
    dvui_EasingFn* easing = a->easing ? a->easing : dvui_easing_linear;
    const float t = easing(dvui_clampf(frac, 0, 1));
    // std.math.lerp
    return a->start_val + (a->end_val - a->start_val) * t;
}

// DVUI: dvui.zig – Animation.done (true v posledním snímku animace)
bool dvui_animation_done(const dvui_Animation* a) {
    if (a->end_time <= 0) {
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Mapa animací v okně (DVUI: Window.animations – TrackingAutoHashMap
// s režimem .get_and_put: značku `used` nastavuje zápis i čtení)
// ---------------------------------------------------------------------------

// DVUI: TrackingAutoHashMap.put – uloží hodnotu a označí ji jako použitou.
static void animations_put(dvui_Window* win, uint64_t h, dvui_Animation a) {
    dvui_MapEntry* e = dvui_map_get_or_put(&win->animations, h);
    if (!e->ptr) {
        e->ptr = malloc(sizeof(dvui_Animation));
        if (!e->ptr) {
            DVUI_LOG_ERR("out of memory");
            abort();
        }
        e->size = sizeof(dvui_Animation);
    }
    memcpy(e->ptr, &a, sizeof a);
    e->used = true;
}

// DVUI: TrackingAutoHashMap.get (.get_and_put – čtení také nastaví `used`)
static bool animations_get(dvui_Window* win, uint64_t h, dvui_Animation* out) {
    dvui_MapEntry* e = dvui_map_get(&win->animations, h);
    if (!e) return false;
    e->used = true;
    if (out) memcpy(out, e->ptr, sizeof *out);
    return true;
}

// DVUI: dvui.zig – animation
void dvui_animation(dvui_Id id, const char* key, dvui_Animation a) {
    dvui_Window* cw = dvui_current_window();
    const uint64_t h = dvui_id_update(id, key);
    dvui_Animation aa = a;
    if (dvui_reduce_motion) aa.end_time = aa.start_time + 1;
    animations_put(cw, h, aa);
}

// DVUI: dvui.zig – animationGet
bool dvui_animation_get(dvui_Id id, const char* key, dvui_Animation* out) {
    const uint64_t h = dvui_id_update(id, key);
    return animations_get(dvui_current_window(), h, out);
}

// ---------------------------------------------------------------------------
// Časovače (animace s klíčem "_timer")
// ---------------------------------------------------------------------------

// DVUI: Window.zig – timer
static void window_timer(dvui_Window* self, dvui_Id id, int32_t micros) {
    // start_time v budoucnu nevyvolá přívaly snímků – způsobí jeden snímek
    // a pak vyprší
    const dvui_Animation a = {.start_time = micros, .end_time = micros};
    const uint64_t h = dvui_id_update(id, "_timer");
    animations_put(self, h, a);
}

// DVUI: dvui.zig – timer
void dvui_timer(dvui_Id id, int32_t micros) { window_timer(dvui_current_window(), id, micros); }

// DVUI: Window.zig – timerRemove
void dvui_timer_remove(dvui_Id id) {
    const uint64_t h = dvui_id_update(id, "_timer");
    dvui_map_remove(&dvui_current_window()->animations, h);
}

// DVUI: dvui.zig – timerGet
bool dvui_timer_get(dvui_Id id, int32_t* out) {
    dvui_Animation a;
    if (dvui_animation_get(id, "_timer", &a)) {
        if (out) *out = a.end_time;
        return true;
    } else {
        return false;
    }
}

// DVUI: dvui.zig – timerDone
bool dvui_timer_done(dvui_Id id) {
    int32_t end_time;
    if (dvui_timer_get(id, &end_time)) {
        if (end_time <= 0) {
            return true;
        }
    }

    return false;
}

// DVUI: dvui.zig – timerDoneOrNone
bool dvui_timer_done_or_none(dvui_Id id) { return dvui_timer_done(id) || !dvui_timer_get(id, nullptr); }
