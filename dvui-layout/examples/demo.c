// Demo dvui-layout: všechny widgety v jednom okně.
//
// Spuštění: dvui-layout-demo [--frames N]
//   --frames N  ukončí demo po N snímcích (pro automatické testy)
#include "dvui/backend_sdl3gpu.h"
#include "dvui/dvui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// paleta barev pro obdélníky
static const dvui_Color palette[] = {
    {0xe6, 0x4a, 0x19, 255}, {0xf9, 0xa8, 0x25, 255}, {0x7c, 0xb3, 0x42, 255}, {0x00, 0x89, 0x7b, 255},
    {0x1e, 0x88, 0xe5, 255}, {0x5e, 0x35, 0xb1, 255}, {0xd8, 0x1b, 0x60, 255}, {0x6d, 0x4c, 0x41, 255},
};
#define PALETTE_LEN (sizeof palette / sizeof palette[0])

static dvui_Color pal(size_t i) { return palette[i % PALETTE_LEN]; }

typedef struct DemoState {
    size_t clicks[4];
    bool show_window;
    bool show_modal;
    size_t window_clicks;
} DemoState;

// Barevný obdélník dané min. velikosti (spacer s pozadím a okrajem).
static void colored_rect(dvui_Src src, size_t id_extra, dvui_Size size, dvui_Color fill, dvui_Color border, dvui_Expand expand) {
    dvui_spacer(src, (dvui_Options){
                         .id_extra = DVUI_SOME(id_extra),
                         .min_size_content = DVUI_SOME(size),
                         .expand = DVUI_SOME(expand),
                         .margin = DVUI_SOME(dvui_rect_all(2)),
                         .border = DVUI_SOME(dvui_rect_all(2)),
                         .background = DVUI_SOME(true),
                         .color_fill = DVUI_SOME(fill),
                         .color_border = DVUI_SOME(border),
                     });
}

static void top_bar(DemoState *st) {
    // vodorovný box s tlačítky; každé má jinou barvu, kliknutím se mění
    dvui_BoxWidget *row = dvui_box(DVUI_SRC, (dvui_BoxInitOptions){.dir = DVUI_HORIZONTAL},
                                   (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_HORIZONTAL), .background = DVUI_SOME(true), .style = DVUI_SOME(DVUI_STYLE_WINDOW)});
    for (size_t i = 0; i < 4; i++) {
        const dvui_Color c = pal(i * 2 + st->clicks[i]);
        if (dvui_button(DVUI_SRC, (dvui_Size){40 + 10 * (float)i, 12}, (dvui_ButtonInitOptions){0},
                        (dvui_Options){.id_extra = DVUI_SOME(i), .color_fill = DVUI_SOME(c), .color_border = DVUI_SOME(dvui_color_lighten(c, -25)),
                                       .border = DVUI_SOME(dvui_rect_all(2))})) {
            st->clicks[i]++;
        }
    }

    // tlačítko s tooltipem (zarovnané vpravo pomocí gravity_x = 1)
    dvui_ButtonWidget bw;
    dvui_button_widget_init(&bw, DVUI_SRC, (dvui_ButtonInitOptions){0}, (dvui_Options){.gravity_x = DVUI_SOME(1.0f), .style = DVUI_SOME(DVUI_STYLE_HIGHLIGHT)});
    dvui_button_widget_process_events(&bw);
    dvui_button_widget_draw_background(&bw);
    dvui_label_placeholder(DVUI_SRC, (dvui_Size){60, 12}, (dvui_Options){.gravity_x = DVUI_SOME(0.5f), .gravity_y = DVUI_SOME(0.5f)});
    if (dvui_button_widget_clicked(&bw)) st->show_window = !st->show_window;
    dvui_button_widget_draw_focus(&bw);
    const dvui_RectPhysical active = dvui_widget_data_border_rect_scale(&bw.wd).r;
    dvui_button_widget_deinit(&bw);

    // tooltip k tlačítku: obsah je box se třemi obdélníky
    dvui_FloatingTooltipWidget tt;
    dvui_floating_tooltip_widget_init(&tt, DVUI_SRC, (dvui_FloatingTooltipInitOptions){.active_rect = active, .position = DVUI_TOOLTIP_VERTICAL},
                                      (dvui_Options){.padding = DVUI_SOME(dvui_rect_all(6))});
    if (dvui_floating_tooltip_widget_shown(&tt)) {
        dvui_BoxWidget *tb = dvui_box(DVUI_SRC, (dvui_BoxInitOptions){.dir = DVUI_HORIZONTAL}, (dvui_Options){0});
        for (size_t i = 0; i < 3; i++) colored_rect(DVUI_SRC, i, (dvui_Size){20, 20}, pal(i + 4), pal(i + 5), DVUI_EXPAND_NONE);
        dvui_box_widget_deinit(tb);
    }
    dvui_floating_tooltip_widget_deinit(&tt);

    dvui_box_widget_deinit(row);

    // box s equal_space: všechny děti dostanou stejně místa
    dvui_BoxWidget *eq = dvui_box(DVUI_SRC, (dvui_BoxInitOptions){.dir = DVUI_HORIZONTAL, .equal_space = true},
                                  (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_HORIZONTAL)});
    for (size_t i = 0; i < 5; i++) colored_rect(DVUI_SRC, i, (dvui_Size){10 + 15 * (float)i, 14}, pal(i), DVUI_RGB(40, 40, 40), DVUI_EXPAND_NONE);
    dvui_box_widget_deinit(eq);
}

static void left_scroll(void) {
    // svislá scroll oblast s obdélníky různé výšky; vodorovně se také scrolluje
    dvui_ScrollAreaWidget *sa = dvui_scroll_area(DVUI_SRC, (dvui_ScrollAreaInitOptions){.horizontal = DVUI_SOME(DVUI_SCROLL_AUTO)},
                                                 (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_VERTICAL), .min_size_content = DVUI_SOME(((dvui_Size){180, 100})),
                                                                .border = DVUI_SOME(dvui_rect_all(1)), .margin = DVUI_SOME(dvui_rect_all(4))});
    for (size_t i = 0; i < 25; i++) {
        const float w = 60 + (float)((i * 37) % 140);
        const float h = 12 + (float)((i * 13) % 30);
        colored_rect(DVUI_SRC, i, (dvui_Size){w, h}, pal(i), dvui_color_lighten(pal(i), -30), DVUI_EXPAND_NONE);
    }
    dvui_scroll_area_widget_deinit(sa);
}

static void right_grid(void) {
    // grid: sloupce a řádky se přizpůsobí obsahu během několika snímků
    const size_t cols = 5, rows = 40;
    dvui_GridWidget *g = dvui_grid(DVUI_SRC, (dvui_GridInitOptions){.rows = DVUI_SOME(rows), .scroll_opts = {.horizontal = DVUI_SOME(DVUI_SCROLL_AUTO)}},
                                   (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_BOTH), .margin = DVUI_SOME(dvui_rect_all(4))});
    for (size_t c = 0; c < cols; c++) {
        dvui_GridCellWidget *h = dvui_grid_widget_col_header(g, (dvui_GridColHeaderInitOptions){.col = c},
                                                             (dvui_Options){.background = DVUI_SOME(true), .style = DVUI_SOME(DVUI_STYLE_CONTROL),
                                                                            .border = DVUI_SOME(((dvui_Rect){0, 0, 1, 1}))});
        colored_rect(DVUI_SRC, c, (dvui_Size){30 + 10 * (float)c, 10}, pal(c), DVUI_RGB(30, 30, 30), DVUI_EXPAND_NONE);
        dvui_grid_cell_widget_deinit(h);
    }

    size_t first, last;
    dvui_grid_widget_rows_visible(g, &first, &last);
    dvui_GridCell hovered;
    const bool has_hover = dvui_grid_widget_cell_hovered(g, &hovered);
    for (size_t r = first; r < last; r++) {
        for (size_t c = 0; c < cols; c++) {
            const bool hov = has_hover && hovered.row == r;
            dvui_GridCellWidget *cell = dvui_grid_widget_cell(g, (dvui_GridCellOptions){.col = c, .row = r},
                                                              (dvui_Options){.background = DVUI_SOME(hov || r % 2 == 1),
                                                                             .color_fill = DVUI_SOME(hov ? DVUI_RGB(0xcf, 0xe3, 0xfc) : DVUI_RGB(0xf4, 0xf4, 0xf4)),
                                                                             .expand = DVUI_SOME(c == cols - 1 ? DVUI_EXPAND_HORIZONTAL : DVUI_EXPAND_NONE)});
            const float w = 20 + (float)(((r + 1) * (c + 3) * 7) % 50);
            const float h = 8 + (float)(((r + 2) * (c + 1) * 5) % 18);
            colored_rect(DVUI_SRC, 0, (dvui_Size){w, h}, pal(r + c), dvui_color_lighten(pal(r + c), -30), DVUI_EXPAND_NONE);
            dvui_grid_cell_widget_deinit(cell);
        }
    }
    dvui_grid_widget_deinit(g);
}

static void floating_window(DemoState *st) {
    if (!st->show_window) return;
    dvui_FloatingWindowWidget *fw = dvui_floating_window(DVUI_SRC, (dvui_FloatingWindowInitOptions){.open_flag = &st->show_window},
                                                         (dvui_Options){.min_size_content = DVUI_SOME(((dvui_Size){260, 200}))});
    dvui_floating_window_widget_drag_area_set(fw, dvui_window_header((dvui_WindowHeaderOptions){.title_size = {120, 12}, .open_flag = &st->show_window}));

    dvui_BoxWidget *row = dvui_box(DVUI_SRC, (dvui_BoxInitOptions){.dir = DVUI_HORIZONTAL}, (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_HORIZONTAL)});
    if (dvui_button(DVUI_SRC, (dvui_Size){50, 12}, (dvui_ButtonInitOptions){0}, (dvui_Options){.color_fill = DVUI_SOME(pal(st->window_clicks))})) {
        st->window_clicks++;
    }
    if (dvui_button(DVUI_SRC, (dvui_Size){50, 12}, (dvui_ButtonInitOptions){0}, (dvui_Options){.style = DVUI_SOME(DVUI_STYLE_ERR), .gravity_x = DVUI_SOME(1.0f)})) {
        st->show_modal = true;
    }
    dvui_box_widget_deinit(row);

    // vnořená scroll oblast s roztaženými obdélníky (expand horizontal);
    // max_size_content omezí její výšku, jinak by okno narostlo podle obsahu
    dvui_ScrollAreaWidget *sa = dvui_scroll_area(DVUI_SRC, (dvui_ScrollAreaInitOptions){0},
                                                 (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_BOTH), .max_size_content = DVUI_SOME(((dvui_Size){DVUI_MAX_FLOAT_SAFE, 150}))});
    for (size_t i = 0; i < 12; i++) colored_rect(DVUI_SRC, i, (dvui_Size){40, 18}, pal(i + st->window_clicks), DVUI_RGB(20, 20, 20), DVUI_EXPAND_HORIZONTAL);
    dvui_scroll_area_widget_deinit(sa);

    dvui_floating_window_widget_deinit(fw);

    if (st->show_modal) {
        // modální okno nad plovoucím oknem
        dvui_FloatingWindowWidget *m = dvui_floating_window(DVUI_SRC, (dvui_FloatingWindowInitOptions){.modal = true, .open_flag = &st->show_modal}, (dvui_Options){0});
        dvui_floating_window_widget_drag_area_set(m, dvui_window_header((dvui_WindowHeaderOptions){.title_size = {80, 12}, .open_flag = &st->show_modal}));
        colored_rect(DVUI_SRC, 0, (dvui_Size){160, 60}, pal(6), pal(7), DVUI_EXPAND_BOTH);
        if (dvui_button(DVUI_SRC, (dvui_Size){40, 12}, (dvui_ButtonInitOptions){0}, (dvui_Options){.gravity_x = DVUI_SOME(0.5f)})) st->show_modal = false;
        dvui_floating_window_widget_deinit(m);
    }
}

static void frame(DemoState *st) {
    dvui_BoxWidget *main_box = dvui_box(DVUI_SRC, (dvui_BoxInitOptions){0}, (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_BOTH)});

    top_bar(st);

    // Spodní lišta se deklaruje před prostřední částí s gravity_y = 1: box ji
    // zabalí ke spodnímu okraji. Prostřední část pak dostane zbytek místa.
    // (Scroll oblast hlásí min. výšku celého obsahu, takže kdyby byla lišta
    // až za ní, nezbylo by na ni místo – stejně jako v DVUI.)
    dvui_BoxWidget *bottom = dvui_box(DVUI_SRC, (dvui_BoxInitOptions){.dir = DVUI_HORIZONTAL},
                                      (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_HORIZONTAL), .gravity_y = DVUI_SOME(1.0f)});
    // roztažený obdélník mezi dvěma pevnými
    colored_rect(DVUI_SRC, 0, (dvui_Size){40, 16}, pal(0), DVUI_RGB(0, 0, 0), DVUI_EXPAND_NONE);
    colored_rect(DVUI_SRC, 1, (dvui_Size){40, 16}, pal(3), DVUI_RGB(0, 0, 0), DVUI_EXPAND_HORIZONTAL);
    colored_rect(DVUI_SRC, 2, (dvui_Size){40, 16}, pal(5), DVUI_RGB(0, 0, 0), DVUI_EXPAND_NONE);
    dvui_box_widget_deinit(bottom);

    dvui_BoxWidget *middle = dvui_box(DVUI_SRC, (dvui_BoxInitOptions){.dir = DVUI_HORIZONTAL}, (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_BOTH)});
    left_scroll();
    right_grid();
    dvui_box_widget_deinit(middle);

    dvui_box_widget_deinit(main_box);

    floating_window(st);
}

int main(int argc, char **argv) {
    long max_frames = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) max_frames = strtol(argv[++i], nullptr, 10);
    }

    dvui_Sdl3GpuBackend *backend = dvui_sdl3gpu_create((dvui_Sdl3GpuInitOptions){
        .title = "dvui-layout demo",
        .size = {900, 600},
        .min_size = DVUI_SOME(((dvui_Size){250, 350})),
    });
    if (!backend) {
        fprintf(stderr, "Nepodařilo se inicializovat SDL3 GPU: %s\n", SDL_GetError());
        return 1;
    }

    dvui_Window *win = dvui_window_create(dvui_sdl3gpu_backend(backend));
    bool open = true;
    dvui_window_open_flag_set(win, &open);

    DemoState st = {.show_window = true};
    long frames = 0;
    while (open) {
        dvui_window_begin(win, (int64_t)SDL_GetTicksNS());
        dvui_sdl3gpu_add_all_events(backend, win);
        frame(&st);
        const int64_t wait = dvui_window_end(win);

        frames++;
        if (max_frames >= 0 && frames >= max_frames) break;
        dvui_sdl3gpu_wait_event_timeout(backend, max_frames >= 0 ? 0 : wait);
    }

    dvui_window_destroy(win);
    dvui_sdl3gpu_destroy(backend);
    return 0;
}
