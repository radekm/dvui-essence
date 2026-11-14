// Testy layoutu bez GPU (testovací backend). Kontrolují obdélníky widgetů po
// ustálení layoutu (DVUI potřebuje víc snímků, než se min. velikosti ustálí).
#include "dvui/backend_testing.h"
#include "dvui/dvui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) \
    do { \
        checks++; \
        if (!(cond)) { \
            failures++; \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define CHECK_NEAR(a, b) \
    do { \
        checks++; \
        const double va_ = (a), vb_ = (b); \
        if (fabs(va_ - vb_) > 0.01) { \
            failures++; \
            fprintf(stderr, "%s:%d: CHECK_NEAR failed: %s = %g, %s = %g\n", __FILE__, __LINE__, #a, va_, #b, vb_); \
        } \
    } while (0)

#define CHECK_RECT(r, X, Y, W, H) \
    do { \
        CHECK_NEAR((r).x, X); \
        CHECK_NEAR((r).y, Y); \
        CHECK_NEAR((r).w, W); \
        CHECK_NEAR((r).h, H); \
    } while (0)

// ---------------------------------------------------------------------------
// Pomocník: okno s testovacím backendem
// ---------------------------------------------------------------------------

typedef struct Harness {
    dvui_TestingBackend tb;
    dvui_Window* win;
    int64_t time;
} Harness;

static void harness_init(Harness* h, float w, float hgt) {
    dvui_testing_backend_init(&h->tb, (dvui_Size){w, hgt});
    h->win = dvui_window_create(dvui_testing_backend(&h->tb));
    h->time = 1000000;
}

static void harness_deinit(Harness* h) {
    dvui_window_destroy(h->win);
    dvui_testing_backend_deinit(&h->tb);
}

typedef void (*FrameFn)(void* ctx);
typedef void (*EventsFn)(dvui_Window* win, void* ctx);

// Jeden snímek; vrací výsledek dvui_window_end.
static int32_t step(Harness* h, EventsFn events, FrameFn frame, void* ctx) {
    h->time += 16000000; // 16 ms
    dvui_window_begin(h->win, h->time);
    if (events) events(h->win, ctx);
    frame(ctx);
    return dvui_window_end(h->win);
}

// Opakuje snímky, dokud dvui nechce další (DVUI: testing.settle).
static int settle(Harness* h, FrameFn frame, void* ctx) {
    for (int i = 1; i <= 50; i++) {
        if (step(h, nullptr, frame, ctx) != 0) return i;
    }
    return -1;
}

static dvui_RectPhysical wd_rect(const dvui_WidgetData* wd) { return dvui_widget_data_rect_scale(wd).r; }

// obdélník dané min. velikosti a roztažení
#define RECT_PROPS(w, h, e) DVUI_MIN_SIZE((w), (h)), DVUI_EXPAND(e)

// ---------------------------------------------------------------------------
// placeIn
// ---------------------------------------------------------------------------

static void test_place_in(void) {
    const dvui_Rect avail = {10, 20, 100, 50};
    CHECK_RECT(dvui_place_in(avail, (dvui_Size){30, 10}, DVUI_EXPAND_NONE, (dvui_Gravity){0, 0}), 10, 20, 30, 10);
    CHECK_RECT(dvui_place_in(avail, (dvui_Size){30, 10}, DVUI_EXPAND_NONE, (dvui_Gravity){1, 1}), 80, 60, 30, 10);
    CHECK_RECT(dvui_place_in(avail, (dvui_Size){30, 10}, DVUI_EXPAND_NONE, (dvui_Gravity){0.5f, 0.5f}), 45, 40, 30, 10);
    CHECK_RECT(dvui_place_in(avail, (dvui_Size){30, 10}, DVUI_EXPAND_HORIZONTAL, (dvui_Gravity){0, 0}), 10, 20, 100, 10);
    CHECK_RECT(dvui_place_in(avail, (dvui_Size){30, 10}, DVUI_EXPAND_BOTH, (dvui_Gravity){0, 0}), 10, 20, 100, 50);
    // nikdy víc, než je k dispozici
    CHECK_RECT(dvui_place_in(avail, (dvui_Size){300, 100}, DVUI_EXPAND_NONE, (dvui_Gravity){0, 0}), 10, 20, 100, 50);
    // ratio: 2:1 do 100×50 → 100×50
    CHECK_RECT(dvui_place_in(avail, (dvui_Size){20, 10}, DVUI_EXPAND_RATIO, (dvui_Gravity){0, 0}), 10, 20, 100, 50);
    // ratio 1:1 do 100×50 → 50×50, vystředěno
    CHECK_RECT(dvui_place_in(avail, (dvui_Size){10, 10}, DVUI_EXPAND_RATIO, (dvui_Gravity){0.5f, 0}), 35, 20, 50, 50);
}

static void test_place_on_screen(void) {
    const dvui_Rect screen = {0, 0, 200, 100};
    // přetéká vpravo → posun doleva
    CHECK_RECT(dvui_place_on_screen(screen, (dvui_Rect){0}, DVUI_AVOID_NONE, (dvui_Rect){180, 10, 50, 20}), 150, 10, 50, 20);
    // vyhnout se spawneru vodorovně
    CHECK_RECT(dvui_place_on_screen(screen, (dvui_Rect){10, 10, 30, 20}, DVUI_AVOID_HORIZONTAL, (dvui_Rect){10, 10, 50, 20}), 40, 10, 50, 20);
}

// ---------------------------------------------------------------------------
// Box
// ---------------------------------------------------------------------------

typedef struct BoxCtx {
    dvui_RectPhysical a, b, c, box;
} BoxCtx;

static void frame_vbox(void* p) {
    BoxCtx* ctx = p;
    dvui_BoxWidget* b = dvui_box();
    dvui_WidgetData a = dvui_spacer(RECT_PROPS(30, 10, DVUI_EXPAND_NONE));
    dvui_WidgetData bb = dvui_spacer(RECT_PROPS(50, 20, DVUI_EXPAND_NONE));
    dvui_WidgetData c = dvui_spacer(DVUI_MIN_SIZE(10, 10), DVUI_GRAVITY_X(1.0f));
    ctx->a = wd_rect(&a);
    ctx->b = wd_rect(&bb);
    ctx->c = wd_rect(&c);
    ctx->box = wd_rect(&b->wd);
    dvui_box_widget_deinit(b);
}

static void test_box_vertical(void) {
    Harness h;
    harness_init(&h, 400, 300);
    BoxCtx ctx = {0};
    const int frames = settle(&h, frame_vbox, &ctx);
    CHECK(frames > 1 && frames < 10);
    // box není roztažený: těsně kolem dětí (max šířka 50, součet výšek 40)
    CHECK_RECT(ctx.box, 0, 0, 50, 40);
    CHECK_RECT(ctx.a, 0, 0, 30, 10);
    CHECK_RECT(ctx.b, 0, 10, 50, 20);
    // gravity_x = 1 → vpravo
    CHECK_RECT(ctx.c, 40, 30, 10, 10);
    harness_deinit(&h);
}

static void frame_hbox_expand(void* p) {
    BoxCtx* ctx = p;
    dvui_BoxWidget* b = dvui_box(DVUI_DIR(DVUI_HORIZONTAL), DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL));
    dvui_WidgetData a = dvui_spacer(RECT_PROPS(30, 10, DVUI_EXPAND_HORIZONTAL));
    dvui_WidgetData bb = dvui_spacer(RECT_PROPS(50, 20, DVUI_EXPAND_NONE));
    dvui_WidgetData c = dvui_spacer(RECT_PROPS(10, 10, DVUI_EXPAND_BOTH));
    ctx->a = wd_rect(&a);
    ctx->b = wd_rect(&bb);
    ctx->c = wd_rect(&c);
    ctx->box = wd_rect(&b->wd);
    dvui_box_widget_deinit(b);
}

static void test_box_horizontal_expand(void) {
    Harness h;
    harness_init(&h, 400, 300);
    BoxCtx ctx = {0};
    settle(&h, frame_hbox_expand, &ctx);
    CHECK_RECT(ctx.box, 0, 0, 400, 20);
    // volné místo 400 - (30 + 50 + 10) = 310 se dělí mezi dvě roztažené děti
    CHECK_RECT(ctx.a, 0, 0, 30 + 155, 10);
    CHECK_RECT(ctx.b, 185, 0, 50, 20);
    // expand both → i na výšku boxu
    CHECK_RECT(ctx.c, 235, 0, 10 + 155, 20);
    harness_deinit(&h);
}

static void frame_equal_space(void* p) {
    BoxCtx* ctx = p;
    dvui_BoxWidget* b = dvui_box(DVUI_DIR(DVUI_HORIZONTAL), DVUI_EQUAL_SPACE(true), DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL));
    dvui_WidgetData a = dvui_spacer(RECT_PROPS(30, 10, DVUI_EXPAND_NONE));
    dvui_WidgetData bb = dvui_spacer(RECT_PROPS(50, 20, DVUI_EXPAND_HORIZONTAL));
    dvui_WidgetData c = dvui_spacer(RECT_PROPS(10, 10, DVUI_EXPAND_NONE));
    ctx->a = wd_rect(&a);
    ctx->b = wd_rect(&bb);
    ctx->c = wd_rect(&c);
    ctx->box = wd_rect(&b->wd);
    dvui_box_widget_deinit(b);
}

static void test_box_equal_space(void) {
    Harness h;
    harness_init(&h, 300, 300);
    BoxCtx ctx = {0};
    settle(&h, frame_equal_space, &ctx);
    // každé dítě dostane 100 px
    CHECK_RECT(ctx.a, 0, 0, 30, 10);
    CHECK_RECT(ctx.b, 100, 0, 100, 20);
    CHECK_RECT(ctx.c, 200, 0, 10, 10);
    harness_deinit(&h);
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------

typedef struct ButtonCtx {
    dvui_RectPhysical rect;
    int clicks;
    bool hovered;
} ButtonCtx;

static void frame_button(void* p) {
    ButtonCtx* ctx = p;
    dvui_ButtonWidget bw;
    dvui_button_widget_init(&bw, DVUI_SRC, DVUI_PROPS());
    dvui_button_widget_process_events(&bw);
    dvui_button_widget_draw_background(&bw);
    dvui_label_placeholder(DVUI_LABEL_SIZE(40, 12), DVUI_GRAVITY_X(0.5f), DVUI_GRAVITY_Y(0.5f));
    if (dvui_button_widget_clicked(&bw)) ctx->clicks++;
    ctx->hovered = dvui_button_widget_hovered(&bw);
    ctx->rect = dvui_widget_data_border_rect_scale(&bw.wd).r;
    dvui_button_widget_deinit(&bw);
}

// kliknutí na bod: pohyb myši, stisk a puštění v jednom snímku
typedef struct ClickTest {
    dvui_Point p;
    ButtonCtx* button;
} ClickTest;

static void events_click(dvui_Window* win, void* p) {
    ClickTest* c = p;
    dvui_window_add_event_mouse_motion(win, c->p);
    dvui_window_add_event_mouse_button(win, DVUI_BUTTON_LEFT, DVUI_MOUSE_PRESS);
    dvui_window_add_event_mouse_button(win, DVUI_BUTTON_LEFT, DVUI_MOUSE_RELEASE);
}

static void frame_button_click(void* p) { frame_button(((ClickTest*)p)->button); }

static void test_button(void) {
    Harness h;
    harness_init(&h, 400, 300);
    ButtonCtx ctx = {0};
    settle(&h, frame_button, &ctx);
    // label 40×12 + padding labelu 6 + padding tlačítka 6 → 64×36; margin 4
    CHECK_RECT(ctx.rect, 4, 4, 64, 36);
    CHECK(ctx.clicks == 0);

    // kliknutí doprostřed tlačítka
    ClickTest click = {.p = {30, 20}, .button = &ctx};
    step(&h, events_click, frame_button_click, &click);
    CHECK(ctx.clicks == 1);
    CHECK(ctx.hovered);

    // kliknutí mimo
    click.p = (dvui_Point){200, 200};
    step(&h, events_click, frame_button_click, &click);
    CHECK(ctx.clicks == 1);
    CHECK(!ctx.hovered);
    harness_deinit(&h);
}

// ---------------------------------------------------------------------------
// ScrollArea
// ---------------------------------------------------------------------------

typedef struct ScrollCtx {
    dvui_ScrollInfo si;
    dvui_RectPhysical area;
    dvui_RectPhysical first;
    dvui_RectPhysical last;
    float wheel;
} ScrollCtx;

static void frame_scroll(void* p) {
    ScrollCtx* ctx = p;
    dvui_ScrollAreaWidget* sa = dvui_scroll_area(DVUI_SCROLL_INFO(&ctx->si), DVUI_MIN_SIZE(100, 100), DVUI_MAX_SIZE(100, 100));
    for (size_t i = 0; i < 10; i++) {
        dvui_WidgetData wd = dvui_spacer(DVUI_ID_EXTRA(i), DVUI_MIN_SIZE(50, 30));
        if (i == 0) ctx->first = wd_rect(&wd);
        if (i == 9) ctx->last = wd_rect(&wd);
    }
    ctx->area = wd_rect(dvui_scroll_area_widget_data(sa));
    dvui_scroll_area_widget_deinit(sa);
}

static void events_wheel(dvui_Window* win, void* p) {
    ScrollCtx* ctx = p;
    dvui_window_add_event_mouse_motion(win, (dvui_Point){20, 20});
    dvui_window_add_event_mouse_wheel(win, ctx->wheel, DVUI_VERTICAL);
}

static void test_scroll_area(void) {
    Harness h;
    harness_init(&h, 400, 300);
    ScrollCtx ctx = {.si = dvui_scroll_info_default()};
    settle(&h, frame_scroll, &ctx);
    // Min. velikost scroll oblasti roste s obsahem (jako v DVUI), proto ji
    // omezujeme max_size_content.
    CHECK_RECT(ctx.area, 0, 0, 100, 100);
    // virtuální výška = 10 × 30
    CHECK_NEAR(ctx.si.virtual_size.h, 300);
    // svislý posuvník (10 px) ubere šířku viewportu
    CHECK_NEAR(ctx.si.viewport.w, 90);
    CHECK_NEAR(ctx.si.viewport.h, 100);
    CHECK_RECT(ctx.first, 0, 0, 50, 30);
    CHECK_RECT(ctx.last, 0, 270, 50, 30);

    // kolečko dolů o 80 px
    ctx.wheel = -80;
    step(&h, events_wheel, frame_scroll, &ctx);
    CHECK_NEAR(ctx.si.viewport.y, 80);
    settle(&h, frame_scroll, &ctx);
    CHECK_RECT(ctx.first, 0, -80, 50, 30);

    // nelze přejet za konec (max 300 - 100 = 200)
    ctx.wheel = -1000;
    step(&h, events_wheel, frame_scroll, &ctx);
    CHECK_NEAR(ctx.si.viewport.y, 200);
    harness_deinit(&h);
}

// ---------------------------------------------------------------------------
// Grid (vícepruchodový layout)
// ---------------------------------------------------------------------------

typedef struct GridCtx {
    dvui_RectPhysical cells[3][3];
    dvui_RectPhysical headers[3];
    float col_width[3];
} GridCtx;

static void frame_grid(void* p) {
    GridCtx* ctx = p;
    dvui_GridWidget* g = dvui_grid(DVUI_MIN_SIZE(300, 200), DVUI_BORDER_ALL(0));
    for (size_t c = 0; c < 3; c++) {
        dvui_GridCellWidget* hdr = dvui_grid_col_header(g, DVUI_COL(c));
        dvui_spacer(DVUI_MIN_SIZE(10, 15));
        ctx->headers[c] = wd_rect(&hdr->wd);
        dvui_grid_cell_widget_deinit(hdr);
    }
    for (size_t r = 0; r < 3; r++) {
        for (size_t c = 0; c < 3; c++) {
            dvui_GridCellWidget* cell = dvui_grid_cell(g, DVUI_COL(c), DVUI_ROW(r));
            // šířka buňky závisí na sloupci i řádku; výška na řádku
            const dvui_Size s = {20.0f + 10.0f * (float)c + 5.0f * (float)r, 10.0f + 4.0f * (float)r};
            dvui_spacer(DVUI_P(min_size_content, s));
            ctx->cells[r][c] = wd_rect(&cell->wd);
            dvui_grid_cell_widget_deinit(cell);
        }
    }
    for (size_t c = 0; c < 3; c++) ctx->col_width[c] = dvui_grid_widget_col_width(g, c);
    dvui_grid_widget_deinit(g);
}

static void test_grid_auto_size(void) {
    Harness h;
    harness_init(&h, 400, 300);
    GridCtx ctx = {0};
    const int frames = settle(&h, frame_grid, &ctx);
    // první snímek nezná sloupce ani řádky → víc průchodů
    CHECK(frames >= 3 && frames < 20);

    // šířka sloupce = max. šířka buněk ve sloupci (řádek 2 je nejširší)
    for (size_t c = 0; c < 3; c++) CHECK_NEAR(ctx.col_width[c], 20.0f + 10.0f * (float)c + 10.0f);
    // hlavičky mají výšku 15, buňky pod nimi; výška řádku = max výška v řádku
    CHECK_RECT(ctx.headers[0], 0, 0, 30, 15);
    CHECK_RECT(ctx.headers[1], 30, 0, 40, 15);
    CHECK_RECT(ctx.cells[0][0], 0, 15, 30, 10);
    CHECK_RECT(ctx.cells[1][0], 0, 25, 30, 14);
    CHECK_RECT(ctx.cells[2][2], 70, 15 + 10 + 14, 50, 18);
    harness_deinit(&h);
}

// ---------------------------------------------------------------------------
// FloatingWindow
// ---------------------------------------------------------------------------

typedef struct FwCtx {
    dvui_RectPhysical win_rect;
    dvui_RectPhysical content;
    bool open;
    dvui_Point drag_from, drag_to;
    int drag_phase;
} FwCtx;

static void frame_fw(void* p) {
    FwCtx* ctx = p;
    if (!ctx->open) return;
    dvui_FloatingWindowWidget* fw = dvui_floating_window(DVUI_OPEN_FLAG(&ctx->open));
    dvui_WidgetData wd = dvui_spacer(DVUI_MIN_SIZE(100, 50));
    ctx->content = wd_rect(&wd);
    ctx->win_rect = wd_rect(&fw->wd);
    dvui_floating_window_widget_deinit(fw);
}

static void events_drag(dvui_Window* win, void* p) {
    FwCtx* ctx = p;
    switch (ctx->drag_phase) {
    case 0:
        dvui_window_add_event_mouse_motion(win, ctx->drag_from);
        dvui_window_add_event_mouse_button(win, DVUI_BUTTON_LEFT, DVUI_MOUSE_PRESS);
        break;
    case 1: dvui_window_add_event_mouse_motion(win, ctx->drag_to); break;
    case 2: dvui_window_add_event_mouse_button(win, DVUI_BUTTON_LEFT, DVUI_MOUSE_RELEASE); break;
    }
    ctx->drag_phase++;
}

static void test_floating_window(void) {
    Harness h;
    harness_init(&h, 400, 300);
    FwCtx ctx = {.open = true};
    settle(&h, frame_fw, &ctx);
    // auto-size: obsah 100×50 + border 1 + margin 2 → 106×56, vystředěno
    CHECK_RECT(ctx.win_rect, (400 - 106) / 2.0f, (300 - 56) / 2.0f, 106, 56);
    CHECK_RECT(ctx.content, (400 - 106) / 2.0f + 3, (300 - 56) / 2.0f + 3, 100, 50);

    // přetažení okna za střed o (+30, +20)
    const dvui_RectPhysical before = ctx.win_rect;
    ctx.drag_from = (dvui_Point){before.x + 50, before.y + 25};
    ctx.drag_to = (dvui_Point){before.x + 80, before.y + 45};
    for (int i = 0; i < 3; i++) step(&h, events_drag, frame_fw, &ctx);
    settle(&h, frame_fw, &ctx);
    CHECK_RECT(ctx.win_rect, before.x + 30, before.y + 20, 106, 56);
    harness_deinit(&h);
}

// ---------------------------------------------------------------------------
// Tooltip
// ---------------------------------------------------------------------------

typedef struct TtCtx {
    bool shown;
    dvui_RectPhysical tt_rect;
    dvui_Point mouse;
} TtCtx;

static void frame_tt(void* p) {
    TtCtx* ctx = p;
    dvui_WidgetData wd = dvui_spacer(DVUI_MIN_SIZE(50, 20));
    dvui_FloatingTooltipWidget tt;
    dvui_floating_tooltip_widget_init(&tt, DVUI_SRC, DVUI_PROPS(DVUI_ACTIVE_RECT(wd_rect(&wd))));
    ctx->shown = dvui_floating_tooltip_widget_shown(&tt);
    if (ctx->shown) {
        dvui_spacer(DVUI_MIN_SIZE(30, 10));
        ctx->tt_rect = wd_rect(&tt.wd);
    }
    dvui_floating_tooltip_widget_deinit(&tt);
}

static void events_move(dvui_Window* win, void* p) {
    TtCtx* ctx = p;
    dvui_window_add_event_mouse_motion(win, ctx->mouse);
}

static void test_tooltip(void) {
    Harness h;
    harness_init(&h, 400, 300);
    // myš začíná v 0,0 – nejdřív ji odsuneme mimo spouštěcí obdélník
    TtCtx ctx = {.mouse = {300, 200}};
    step(&h, events_move, frame_tt, &ctx);
    settle(&h, frame_tt, &ctx);
    CHECK(!ctx.shown);

    // myš nad spouštěcím obdélníkem
    ctx.mouse = (dvui_Point){10, 10};
    step(&h, events_move, frame_tt, &ctx);
    CHECK(ctx.shown);
    settle(&h, frame_tt, &ctx);
    CHECK(ctx.shown);
    // vpravo od spouštěcího obdélníku (position = horizontal); obsah 30×10 + border 1
    CHECK_RECT(ctx.tt_rect, 50, 0, 32, 12);

    // myš pryč → tooltip zmizí
    ctx.mouse = (dvui_Point){300, 200};
    step(&h, events_move, frame_tt, &ctx);
    settle(&h, frame_tt, &ctx);
    CHECK(!ctx.shown);
    harness_deinit(&h);
}

int main(void) {
    test_place_in();
    test_place_on_screen();
    test_box_vertical();
    test_box_horizontal_expand();
    test_box_equal_space();
    test_button();
    test_scroll_area();
    test_grid_auto_size();
    test_floating_window();
    test_tooltip();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
