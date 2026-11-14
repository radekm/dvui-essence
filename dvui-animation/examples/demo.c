// Demo dvui-animation podle DVUI src/Examples/animations.zig (bez textu).
//
// Části (shora dolů):
//   1. přepínač reduce_motion a tlačítko „wiggle“ (animace posunu x po kliknutí);
//   2. Animate: volba druhu (alpha, horizontal, vertical, none), easingu
//      (31 malých tlačítek) a délky (−/+ a pruh délky), tlačítka start/end,
//      přepínač „center“; uvnitř průběh easingu jako řada obdélníků (náhrada
//      za plotXY);
//   3. hodiny řízené časovačem: deset čtverců (aktuální sekunda mod 10)
//      a pruh „ms do sekundy“ – snímek se kreslí jen na začátku každé sekundy.
//
// Spuštění: dvui-animation-demo [--frames N]
//   --frames N  ukončí demo po N snímcích (pro automatické testy)
#include "dvui/backend_sdl3gpu.h"
#include "dvui/dvui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// paleta barev
static const dvui_Color palette[] = {
    {0xe6, 0x4a, 0x19, 255}, {0xf9, 0xa8, 0x25, 255}, {0x7c, 0xb3, 0x42, 255}, {0x00, 0x89, 0x7b, 255},
    {0x1e, 0x88, 0xe5, 255}, {0x5e, 0x35, 0xb1, 255}, {0xd8, 0x1b, 0x60, 255}, {0x6d, 0x4c, 0x41, 255},
};
#define PALETTE_LEN (sizeof palette / sizeof palette[0])

static dvui_Color pal(size_t i) { return palette[i % PALETTE_LEN]; }

static const dvui_Color gray = {0xc0, 0xc0, 0xc0, 255};
static const dvui_Color dark = {0x30, 0x30, 0x30, 255};

// DVUI Examples: globální stav příkladu animací
typedef struct DemoState {
    size_t animation_choice; // 0 alpha, 1 horizontal, 2 vertical, 3 none
    bool center;
    size_t easing_choice;
    int32_t duration; // µs
    float xs[100];
    float ys[100];
    bool recalc;
} DemoState;

static const dvui_AnimateKind kinds[4] = {DVUI_ANIMATE_ALPHA, DVUI_ANIMATE_HORIZONTAL, DVUI_ANIMATE_VERTICAL, DVUI_ANIMATE_NONE};

// Tlačítko bez textu dané velikosti a barvy; `selected` = silný tmavý okraj.
static bool color_button(dvui_Src src, size_t id_extra, dvui_Size size, dvui_Color fill, bool selected) {
    return dvui_button_impl(src, DVUI_PROPS(DVUI_ID_EXTRA(id_extra), DVUI_LABEL_SIZE(size.w, size.h), DVUI_FILL(fill),
                                            DVUI_BORDER_ALL(selected ? 3 : 1), DVUI_BORDER_COLOR(selected ? dark : gray),
                                            DVUI_PADDING_ALL(2), DVUI_MARGIN_ALL(2)));
}

// Přepínač (náhrada za checkbox): zapnutý je vyplněný barvou `on_color`.
static void toggle(dvui_Src src, bool* value, dvui_Color on_color) {
    if (color_button(src, 0, (dvui_Size){16, 16}, *value ? on_color : DVUI_RGB(255, 255, 255), *value)) *value = !*value;
}

// --- 1. reduce_motion a wiggle

static void top_row(void) {
    dvui_BoxWidget* hbox = dvui_box(DVUI_DIR(DVUI_HORIZONTAL), DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL));

    // DVUI: checkbox „Reduce Motion“
    toggle(DVUI_SRC, &dvui_reduce_motion, pal(6));

    {
        dvui_BoxWidget* hbox2 = dvui_box(DVUI_MIN_SIZE(200, 0));

        dvui_ButtonWidget button_wiggle;
        dvui_button_widget_init(&button_wiggle, DVUI_SRC, DVUI_PROPS(DVUI_GRAVITY_X(0.5f), DVUI_FILL(pal(4))));

        dvui_Animation a;
        if (dvui_animation_get(button_wiggle.wd.id, "xoffset", &a)) {
            dvui_Rect r = button_wiggle.wd.rect;
            const float v = dvui_animation_value(&a);
            r.x += 20 * (1.0f - v) * (1.0f - v) * sinf(v * 3.14159265f * 50);
            dvui_widget_data_rect_set(&button_wiggle.wd, r);
        }

        dvui_button_widget_process_events(&button_wiggle);
        dvui_button_widget_draw_background(&button_wiggle);
        // místo popisku „Wiggle“
        dvui_label_placeholder(DVUI_LABEL_SIZE(50, 12), DVUI_GRAVITY_X(0.5f), DVUI_GRAVITY_Y(0.5f));
        dvui_button_widget_draw_focus(&button_wiggle);

        if (dvui_button_widget_clicked(&button_wiggle)) {
            dvui_animation(button_wiggle.wd.id, "xoffset", (dvui_Animation){.start_val = 0, .end_val = 1.0f, .start_time = 0, .end_time = 500000});
        }
        dvui_button_widget_deinit(&button_wiggle);

        dvui_box_widget_deinit(hbox2);
    }

    dvui_box_widget_deinit(hbox);
}

// --- 2. Animate a easingy

// Průběh easingu: náhrada za dvui.plotXY. Osa x od −0.25 do 1.25, osa y od
// −0.5 do 1.5 (jako v DVUI); každý vzorek je svislý obdélník od 0 k hodnotě.
typedef struct PlotAxes {
    dvui_RectPhysical r;
} PlotAxes;

static float plot_x(const PlotAxes* p, float x) { return p->r.x + (x - -0.25f) / 1.5f * p->r.w; }
static float plot_y(const PlotAxes* p, float y) { return p->r.y + p->r.h - (y - -0.5f) / 2.0f * p->r.h; }

static void easing_plot(const DemoState* st) {
    dvui_WidgetData wd = dvui_spacer(DVUI_EXPAND(DVUI_EXPAND_BOTH), DVUI_MIN_SIZE(200, 100), DVUI_BACKGROUND(true),
                                     DVUI_FILL(DVUI_RGB(255, 255, 255)), DVUI_BORDER_ALL(1), DVUI_BORDER_COLOR(gray));
    const dvui_RectScale rs = dvui_widget_data_content_rect_scale(&wd);
    const PlotAxes p = {rs.r};
    const float s = rs.s;
    const size_t n = sizeof st->xs / sizeof st->xs[0];
    const float bar_w = (plot_x(&p, 1.0f) - plot_x(&p, 0.0f)) / (float)n;
    const dvui_Color c = dvui_theme_color(dvui_theme_get(), DVUI_STYLE_HIGHLIGHT, DVUI_COLOR_FILL);
    for (size_t i = 0; i < n; i++) {
        const float y0 = plot_y(&p, 0.0f);
        const float y1 = plot_y(&p, st->ys[i]);
        dvui_render_fill_rect((dvui_RectPhysical){plot_x(&p, st->xs[i]), fminf(y0, y1), bar_w + s, fmaxf(fabsf(y1 - y0), s)}, c,
                              false);
    }
    // osy: x od 0 do 1 v y = 0, y od 0 do 1 v x = 0
    dvui_render_fill_rect((dvui_RectPhysical){plot_x(&p, 0.0f), plot_y(&p, 0.0f), plot_x(&p, 1.0f) - plot_x(&p, 0.0f), s}, dark, false);
    dvui_render_fill_rect((dvui_RectPhysical){plot_x(&p, 0.0f), plot_y(&p, 1.0f), s, plot_y(&p, 0.0f) - plot_y(&p, 1.0f)}, dark, false);
}

static void easings_section(DemoState* st) {
    {
        dvui_BoxWidget* hbox = dvui_box(DVUI_DIR(DVUI_HORIZONTAL));

        // DVUI: dropdown s druhy animace
        for (size_t i = 0; i < 4; i++) {
            if (color_button(DVUI_SRC, i, (dvui_Size){24, 16}, pal(i), st->animation_choice == i)) st->animation_choice = i;
        }

        dvui_spacer(DVUI_MIN_SIZE(12, 0));

        if (dvui_first_frame(hbox->wd.id)) {
            st->recalc = true;
        }

        // DVUI: dropdown s easingy – dvě řady malých tlačítek
        {
            dvui_BoxWidget* vbox = dvui_box();
            for (size_t row = 0; row < 2; row++) {
                dvui_BoxWidget* r = dvui_box(DVUI_DIR(DVUI_HORIZONTAL), DVUI_ID_EXTRA(row));
                for (size_t i = row * 16; i < dvui_easings_len && i < (row + 1) * 16; i++) {
                    if (color_button(DVUI_SRC, i, (dvui_Size){10, 10}, pal(i / 3), st->easing_choice == i)) {
                        st->easing_choice = i;
                        st->recalc = true;
                    }
                }
                dvui_box_widget_deinit(r);
            }
            dvui_box_widget_deinit(vbox);
        }

        dvui_spacer(DVUI_MIN_SIZE(12, 0));

        // DVUI: sliderEntry „Duration“ (50–2000 ms) – zde −/+ a pruh délky
        if (color_button(DVUI_SRC, 0, (dvui_Size){16, 16}, gray, false)) {
            st->duration = st->duration - 100000 < 100000 ? 100000 : st->duration - 100000;
        }
        dvui_spacer(DVUI_MIN_SIZE((float)st->duration / 10000.0f, 12), DVUI_GRAVITY_Y(0.5f), DVUI_BACKGROUND(true), DVUI_FILL(pal(3)));
        dvui_spacer(DVUI_MIN_SIZE((2000000.0f - (float)st->duration) / 10000.0f, 12), DVUI_GRAVITY_Y(0.5f),
                    DVUI_BACKGROUND(true), DVUI_FILL(gray));
        if (color_button(DVUI_SRC, 0, (dvui_Size){16, 16}, gray, false)) {
            st->duration = st->duration + 100000 > 2000000 ? 2000000 : st->duration + 100000;
        }

        if (st->recalc) {
            st->recalc = false;
            const size_t n = sizeof st->xs / sizeof st->xs[0];
            for (size_t i = 0; i < n; i++) {
                st->xs[i] = (float)i / (float)n;
                st->ys[i] = dvui_easings[st->easing_choice].fn(st->xs[i]);
            }
        }

        dvui_box_widget_deinit(hbox);
    }

    {
        bool start = false;
        bool end = false;
        {
            dvui_BoxWidget* hbox = dvui_box(DVUI_DIR(DVUI_HORIZONTAL));

            if (color_button(DVUI_SRC, 0, (dvui_Size){40, 16}, pal(2), false)) start = true; // „start“
            if (color_button(DVUI_SRC, 0, (dvui_Size){40, 16}, pal(0), false)) end = true;   // „end“

            if (st->animation_choice > 0) {
                toggle(DVUI_SRC, &st->center, pal(5)); // „Center“
            }

            dvui_box_widget_deinit(hbox);
        }

        // overlay je tu jen kvůli paddingu a velikosti
        dvui_OverlayWidget* o = dvui_overlay(DVUI_PADDING_ALL(6), DVUI_MIN_SIZE(100, 80), DVUI_EXPAND(DVUI_EXPAND_BOTH));

        const dvui_AnimateKind kind = kinds[st->animation_choice];
        dvui_AnimateWidget* animator =
            dvui_animate(DVUI_KIND(kind), DVUI_DURATION(st->duration), DVUI_EASING(dvui_easings[st->easing_choice].fn),
                         DVUI_EXPAND(DVUI_EXPAND_BOTH), DVUI_GRAVITY_X(st->center ? 0.5f : 0.0f), DVUI_GRAVITY_Y(st->center ? 0.5f : 0.0f));

        if (start) dvui_animate_widget_start(animator);
        if (end) dvui_animate_widget_start_end(animator);

        easing_plot(st);

        dvui_animate_widget_deinit(animator);
        dvui_overlay_widget_deinit(o);
    }
}

// --- 3. hodiny

static void clock_section(void) {
    const int64_t millis = dvui_frame_time_ns() / 1000000;
    const int32_t left = (int32_t)(millis % 1000);
    const int64_t secs = millis / 1000;

    dvui_BoxWidget* hbox = dvui_box(DVUI_DIR(DVUI_HORIZONTAL), DVUI_MARGIN(((dvui_Rect){0, 8, 0, 0})));

    // DVUI: mslabel „{d} ms into second“ – časovač naplánuje snímek na
    // začátek každé sekundy
    {
        dvui_WidgetData mslabel = dvui_spacer(DVUI_MIN_SIZE(100, 16), DVUI_BACKGROUND(true), DVUI_FILL(gray));
        // pruh „ms do sekundy“ (při snímku od časovače skoro prázdný)
        const dvui_RectScale rs = dvui_widget_data_content_rect_scale(&mslabel);
        dvui_render_fill_rect((dvui_RectPhysical){rs.r.x, rs.r.y, rs.r.w * (float)left / 1000.0f, rs.r.h}, pal(1), false);

        if (dvui_timer_done_or_none(mslabel.id)) {
            const int32_t wait = 1000 * (1000 - left);
            dvui_timer(mslabel.id, wait);
        }
    }

    dvui_spacer(DVUI_MIN_SIZE(12, 0));

    // deset čtverců, vyplněný je aktuální sekunda mod 10
    for (size_t i = 0; i < 10; i++) {
        const bool cur = (size_t)(secs % 10) == i;
        dvui_spacer(DVUI_ID_EXTRA(i), DVUI_MIN_SIZE(16, 16), DVUI_MARGIN_ALL(2), DVUI_BORDER_ALL(1), DVUI_BORDER_COLOR(dark),
                    DVUI_BACKGROUND(true), DVUI_FILL(cur ? pal((size_t)(secs / 10)) : DVUI_RGB(255, 255, 255)));
    }

    dvui_box_widget_deinit(hbox);
}

static void frame(DemoState* st) {
    dvui_BoxWidget* box = dvui_box(DVUI_EXPAND(DVUI_EXPAND_BOTH), DVUI_BACKGROUND(true), DVUI_STYLE(DVUI_STYLE_WINDOW),
                                   DVUI_PADDING_ALL(8));
    top_row();
    dvui_separator(DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL), DVUI_MARGIN(((dvui_Rect){0, 6, 0, 6})));
    easings_section(st);
    clock_section();
    dvui_box_widget_deinit(box);
}

int main(int argc, char** argv) {
    long max_frames = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) max_frames = strtol(argv[++i], nullptr, 10);
    }

    dvui_Sdl3GpuBackend* backend = dvui_sdl3gpu_create((dvui_Sdl3GpuInitOptions){
        .title = "dvui-animation demo",
        .size = {800, 500},
        .min_size = &(dvui_SizeNatural){400, 300},
    });
    if (!backend) {
        fprintf(stderr, "Nepodařilo se inicializovat SDL3 GPU: %s\n", SDL_GetError());
        return 1;
    }

    dvui_Window* win = dvui_window_create(dvui_sdl3gpu_backend(backend));
    bool open = true;
    dvui_window_open_flag_set(win, &open);

    DemoState st = {.duration = 500000};
    long frames = 0;
    bool interrupted = false;
    while (open) {
        // DVUI: beginWait spolupracuje s waitTime, aby se snímky kreslily jen
        // když je potřeba
        const int64_t nstime = dvui_window_begin_wait(win, interrupted);
        dvui_window_begin(win, nstime);
        dvui_sdl3gpu_add_all_events(backend, win);
        frame(&st);
        const int32_t end_micros = dvui_window_end(win);

        frames++;
        if (max_frames >= 0 && frames >= max_frames) break;
        const uint32_t wait_event_micros = dvui_window_wait_time(win, end_micros);
        interrupted = dvui_sdl3gpu_wait_event_timeout(backend, wait_event_micros);
    }

    dvui_window_destroy(win);
    dvui_sdl3gpu_destroy(backend);
    return 0;
}
