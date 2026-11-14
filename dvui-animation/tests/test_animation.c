// Testy dvui-animation bez GPU (testovací backend). Každý snímek posune čas
// o `dt` (výchozí 16 ms), takže animace a časovače stárnou předvídatelně.
// Na začátku jsou základní testy layoutu převzaté z dvui-layout (placeIn,
// box, button), pak testy animací.
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
    int32_t dt_micros; // posun času na snímek
} Harness;

static void harness_init(Harness* h, float w, float hgt) {
    dvui_testing_backend_init(&h->tb, (dvui_Size){w, hgt});
    h->win = dvui_window_create(dvui_testing_backend(&h->tb));
    h->time = 1000000;
    h->dt_micros = 16000;
}

static void harness_deinit(Harness* h) {
    dvui_window_destroy(h->win);
    dvui_testing_backend_deinit(&h->tb);
}

typedef void (*FrameFn)(void* ctx);
typedef void (*EventsFn)(dvui_Window* win, void* ctx);

// Jeden snímek; vrací výsledek dvui_window_end.
static int32_t step(Harness* h, EventsFn events, FrameFn frame, void* ctx) {
    h->time += (int64_t)h->dt_micros * 1000;
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
// Easing (DVUI: easing.zig)
// ---------------------------------------------------------------------------

static void test_easing(void) {
    // všechny easingy začínají u 0 a končí u 1 (expo a elastic jen přibližně,
    // odchylka 2^-10)
    CHECK(dvui_easings_len == 31);
    for (size_t i = 0; i < dvui_easings_len; i++) {
        CHECK_NEAR(dvui_easings[i].fn(0.0f), 0.0f);
        CHECK_NEAR(dvui_easings[i].fn(1.0f), 1.0f);
    }
    CHECK(strcmp(dvui_easings[0].name, "linear") == 0);
    CHECK_NEAR(dvui_easing_linear(0.25f), 0.25f);
    CHECK_NEAR(dvui_easing_in_quad(0.5f), 0.25f);
    CHECK_NEAR(dvui_easing_out_quad(0.5f), 0.75f);
    CHECK_NEAR(dvui_easing_in_out_cubic(0.25f), 0.0625f);
    CHECK_NEAR(dvui_easing_in_out_cubic(0.75f), 0.9375f);
    CHECK_NEAR(dvui_easing_in_sine(0.5f), 1.0f - cosf(3.14159265f / 4));
    CHECK_NEAR(dvui_easing_in_expo(0.0f), 1.0f / 1024.0f);
    CHECK_NEAR(dvui_easing_out_bounce(0.5f), 7.5625f * (0.5f - 1.5f / 2.75f) * (0.5f - 1.5f / 2.75f) + 0.75f);
    // back a elastic přesahují interval [0, 1]
    CHECK(dvui_easing_in_back(0.2f) < 0.0f);
    CHECK(dvui_easing_out_back(0.8f) > 1.0f);
    CHECK(dvui_easing_out_elastic(0.1f) > 1.0f);
}

// ---------------------------------------------------------------------------
// Animation.value / done (DVUI: dvui.zig – Animation)
// ---------------------------------------------------------------------------

static void test_animation_value(void) {
    dvui_Animation a = {.start_val = 10, .end_val = 20, .start_time = -250, .end_time = 750};
    CHECK_NEAR(dvui_animation_value(&a), 12.5f); // easing NULL = linear
    CHECK(!dvui_animation_done(&a));
    a.easing = dvui_easing_in_quad;
    CHECK_NEAR(dvui_animation_value(&a), 10.0f + 10.0f * 0.0625f);
    // ještě nezačala (start_time >= 0) → start_val
    a.start_time = 0;
    CHECK_NEAR(dvui_animation_value(&a), 10.0f);
    a.start_time = 500;
    a.end_time = 1500;
    CHECK_NEAR(dvui_animation_value(&a), 10.0f);
    // skončila (end_time <= 0) → done a end_val
    a.start_time = -1000;
    a.end_time = 0;
    CHECK(dvui_animation_done(&a));
    CHECK_NEAR(dvui_animation_value(&a), 20.0f);
    a.end_time = -5;
    CHECK(dvui_animation_done(&a));
    CHECK_NEAR(dvui_animation_value(&a), 20.0f);
    // přesah elastic se neořezává
    a = (dvui_Animation){.easing = dvui_easing_out_back, .start_val = 0, .end_val = 1, .start_time = -800, .end_time = 200};
    CHECK(dvui_animation_value(&a) > 1.0f);
}

// ---------------------------------------------------------------------------
// Mapa animací: životnost a návratová hodnota window_end
// ---------------------------------------------------------------------------

typedef struct AnimCtx {
    dvui_Id id;
    // co se má v tomto snímku udělat
    bool put;
    dvui_Animation put_anim;
    bool get;
    // výsledky
    bool found;
    dvui_Animation got;
} AnimCtx;

static void frame_anim(void* p) {
    AnimCtx* ctx = p;
    dvui_BoxWidget* b = dvui_box(DVUI_MIN_SIZE(10, 10));
    ctx->id = b->wd.id;
    if (ctx->put) dvui_animation(ctx->id, "x", ctx->put_anim);
    ctx->found = ctx->get ? dvui_animation_get(ctx->id, "x", &ctx->got) : false;
    dvui_box_widget_deinit(b);
}

static void test_animation_lifetime(void) {
    Harness h;
    harness_init(&h, 200, 100);
    AnimCtx ctx = {0};
    CHECK(settle(&h, frame_anim, &ctx) > 0);
    // bez animací: čekat na událost
    CHECK(step(&h, nullptr, frame_anim, &ctx) == -1);

    // animace na 32 ms, snímky po 16 ms
    ctx.put = true;
    ctx.get = true;
    ctx.put_anim = (dvui_Animation){.start_val = 0, .end_val = 1, .end_time = 32000};
    CHECK(step(&h, nullptr, frame_anim, &ctx) == 0); // běží → hned další snímek
    CHECK(ctx.found);
    CHECK_NEAR(dvui_animation_value(&ctx.got), 0.0f);
    ctx.put = false;

    CHECK(step(&h, nullptr, frame_anim, &ctx) == 0);
    CHECK(ctx.found);
    CHECK(ctx.got.start_time == -16000);
    CHECK(ctx.got.end_time == 16000);
    CHECK_NEAR(dvui_animation_value(&ctx.got), 0.5f);
    CHECK(!dvui_animation_done(&ctx.got));

    // poslední snímek: end_time == 0 → done, hodnota end_val; end vrací −1
    CHECK(step(&h, nullptr, frame_anim, &ctx) == -1);
    CHECK(ctx.found);
    CHECK(dvui_animation_done(&ctx.got));
    CHECK_NEAR(dvui_animation_value(&ctx.got), 1.0f);

    // další snímek: animace zmizela
    CHECK(step(&h, nullptr, frame_anim, &ctx) == -1);
    CHECK(!ctx.found);

    // animace, na kterou se jeden snímek nikdo nezeptá, zmizí
    ctx.put = true;
    ctx.put_anim = (dvui_Animation){.start_val = 0, .end_val = 1, .end_time = 1000000};
    CHECK(step(&h, nullptr, frame_anim, &ctx) == 0);
    ctx.put = false;
    ctx.get = false;
    // nikdo se neptá: neuvidí ji ani window_end (není použitá) → −1
    CHECK(step(&h, nullptr, frame_anim, &ctx) == -1);
    ctx.get = true;
    step(&h, nullptr, frame_anim, &ctx);
    CHECK(!ctx.found);

    // animace v budoucnu: window_end vrací čas do jejího začátku
    ctx.put = true;
    ctx.put_anim = (dvui_Animation){.start_val = 0, .end_val = 1, .start_time = 50000, .end_time = 150000};
    CHECK(step(&h, nullptr, frame_anim, &ctx) == 50000);
    ctx.put = false;
    CHECK(step(&h, nullptr, frame_anim, &ctx) == 34000);
    CHECK(step(&h, nullptr, frame_anim, &ctx) == 18000);
    CHECK(step(&h, nullptr, frame_anim, &ctx) == 2000);
    CHECK_NEAR(dvui_animation_value(&ctx.got), 0.0f);
    // teď už běží (start_time < 0)
    CHECK(step(&h, nullptr, frame_anim, &ctx) == 0);
    CHECK_NEAR(dvui_animation_value(&ctx.got), 14000.0f / 100000.0f);

    // refresh má přednost: vrací 0, i když animace začíná později
    harness_deinit(&h);
}

// ---------------------------------------------------------------------------
// Časovače (DVUI: dvui.timer, timerGet, timerDone, timerDoneOrNone)
// ---------------------------------------------------------------------------

typedef struct TimerCtx {
    dvui_Id id;
    int32_t start; // > 0: v tomto snímku nastavit časovač
    bool periodic; // timer_done_or_none → nový časovač na `period`
    int32_t period;
    // výsledky
    bool has;
    int32_t left;
    bool done;
    bool done_or_none;
    int done_count;
} TimerCtx;

static void frame_timer(void* p) {
    TimerCtx* ctx = p;
    dvui_WidgetData wd = dvui_spacer(DVUI_MIN_SIZE(10, 10));
    ctx->id = wd.id;
    if (ctx->periodic) {
        if (dvui_timer_done_or_none(ctx->id)) {
            ctx->done_count++;
            dvui_timer(ctx->id, ctx->period);
        }
        return;
    }
    if (ctx->start > 0) {
        dvui_timer(ctx->id, ctx->start);
        ctx->start = 0;
    }
    ctx->has = dvui_timer_get(ctx->id, &ctx->left);
    ctx->done = dvui_timer_done(ctx->id);
    ctx->done_or_none = dvui_timer_done_or_none(ctx->id);
    if (ctx->done) ctx->done_count++;
}

static void test_timer(void) {
    Harness h;
    harness_init(&h, 200, 100);
    TimerCtx ctx = {0};
    settle(&h, frame_timer, &ctx);
    CHECK(!ctx.has);
    CHECK(!ctx.done);
    CHECK(ctx.done_or_none);

    // časovač na 40 ms: window_end vrací čas do vypršení (nespouští snímky)
    ctx.start = 40000;
    CHECK(step(&h, nullptr, frame_timer, &ctx) == 40000);
    CHECK(ctx.has && ctx.left == 40000 && !ctx.done && !ctx.done_or_none);
    CHECK(step(&h, nullptr, frame_timer, &ctx) == 24000);
    CHECK(ctx.left == 24000);
    CHECK(step(&h, nullptr, frame_timer, &ctx) == 8000);
    CHECK(ctx.left == 8000);
    // první snímek po vypršení: done, left = o kolik jsme přetáhli
    CHECK(step(&h, nullptr, frame_timer, &ctx) == -1);
    CHECK(ctx.has && ctx.done && ctx.done_or_none);
    CHECK(ctx.left == -8000);
    // pak časovač zmizí
    CHECK(step(&h, nullptr, frame_timer, &ctx) == -1);
    CHECK(!ctx.has && !ctx.done && ctx.done_or_none);
    step(&h, nullptr, frame_timer, &ctx);
    CHECK(ctx.done_count == 1); // timer_done přesně jednou

    // reduce_motion se časovačů netýká
    dvui_reduce_motion = true;
    ctx.start = 40000;
    CHECK(step(&h, nullptr, frame_timer, &ctx) == 40000);
    dvui_reduce_motion = false;
    harness_deinit(&h);

    // periodický časovač (jako hodiny v DVUI Examples): po 50 ms
    harness_init(&h, 200, 100);
    TimerCtx per = {.periodic = true, .period = 50000};
    for (int i = 0; i < 40; i++) step(&h, nullptr, frame_timer, &per);
    // 40 snímků × 16 ms = 640 ms; časovač se obnoví v prvním snímku po
    // vypršení (každých 4 snímky = 64 ms): 1 + 9 = 10 spuštění
    CHECK(per.done_count == 10);
    harness_deinit(&h);
}

// ---------------------------------------------------------------------------
// reduce_motion (DVUI: dvui.reduce_motion)
// ---------------------------------------------------------------------------

typedef struct FadeCtx {
    bool on;
    float v;
} FadeCtx;

static void frame_fade(void* p) {
    FadeCtx* ctx = p;
    dvui_WidgetData wd = dvui_spacer(DVUI_MIN_SIZE(10, 10));
    ctx->v = dvui_state_fade(wd.id, "_f", ctx->on, 1.0f);
}

static void test_reduce_motion(void) {
    Harness h;
    harness_init(&h, 200, 100);
    AnimCtx ctx = {0};
    settle(&h, frame_anim, &ctx);

    dvui_reduce_motion = true;
    ctx.put = true;
    ctx.get = true;
    ctx.put_anim = (dvui_Animation){.start_val = 0, .end_val = 1, .end_time = 1000000};
    // end_time se přepíše na start_time + 1
    CHECK(step(&h, nullptr, frame_anim, &ctx) == 0);
    CHECK(ctx.got.start_time == 0 && ctx.got.end_time == 1);
    ctx.put = false;
    // hned další snímek je poslední (done)
    CHECK(step(&h, nullptr, frame_anim, &ctx) == -1);
    CHECK(ctx.found && dvui_animation_done(&ctx.got));
    CHECK_NEAR(dvui_animation_value(&ctx.got), 1.0f);
    step(&h, nullptr, frame_anim, &ctx);
    CHECK(!ctx.found);

    // stateFade skočí rovnou na cíl
    FadeCtx f = {.on = true};
    step(&h, nullptr, frame_fade, &f);
    CHECK_NEAR(f.v, 1.0f);
    dvui_reduce_motion = false;
    f.on = false;
    step(&h, nullptr, frame_fade, &f);
    // s reduce_motion se stav neukládal, takže přechod k 0 nezačíná
    CHECK_NEAR(f.v, 0.0f);
    f.on = true;
    step(&h, nullptr, frame_fade, &f);
    CHECK_NEAR(f.v, 0.0f); // bez reduce_motion začíná přechod od 0
    harness_deinit(&h);
}

// ---------------------------------------------------------------------------
// AnimateWidget (DVUI: widgets/AnimateWidget.zig)
// ---------------------------------------------------------------------------

typedef struct AnimateCtx {
    dvui_AnimateKind kind;
    float gravity;
    bool start_end;
    // výsledky
    dvui_RectPhysical rect;
    bool has_val;
    float val;
    bool end_done;
    dvui_RectPhysical child;
    dvui_RectPhysical after;
} AnimateCtx;

#define CHILD_FILL DVUI_RGB(0, 0, 255)

static void frame_animate(void* p) {
    AnimateCtx* ctx = p;
    dvui_AnimateWidget aw;
    dvui_animate_widget_init(&aw, DVUI_SRC,
                             DVUI_PROPS(DVUI_KIND(ctx->kind), DVUI_DURATION(160000), DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL),
                                        DVUI_MIN_SIZE(0, 60), DVUI_GRAVITY_X(ctx->gravity), DVUI_GRAVITY_Y(ctx->gravity)));
    if (ctx->start_end) {
        dvui_animate_widget_start_end(&aw);
        ctx->start_end = false;
    }
    ctx->rect = wd_rect(&aw.wd);
    ctx->has_val = aw.has_val;
    ctx->val = aw.val;
    ctx->end_done = dvui_animate_widget_end(&aw);
    dvui_WidgetData c = dvui_spacer(DVUI_MIN_SIZE(50, 20), DVUI_BACKGROUND(true), DVUI_FILL(CHILD_FILL));
    ctx->child = wd_rect(&c);
    dvui_animate_widget_deinit(&aw);
    // po deinit se alfa vrátí
    dvui_WidgetData d = dvui_spacer(DVUI_MIN_SIZE(30, 10), DVUI_BACKGROUND(true), DVUI_FILL(CHILD_FILL));
    ctx->after = wd_rect(&d);
}

static const dvui_TestingDraw* find_draw(const Harness* h, dvui_RectPhysical r) {
    for (size_t i = 0; i < h->tb.draws_len; i++) {
        const dvui_RectPhysical d = h->tb.draws[i].rect;
        if (fabsf(d.x - r.x) < 0.01f && fabsf(d.y - r.y) < 0.01f && fabsf(d.w - r.w) < 0.01f && fabsf(d.h - r.h) < 0.01f) {
            return &h->tb.draws[i];
        }
    }
    return nullptr;
}

static void test_animate_vertical(void) {
    Harness h;
    harness_init(&h, 200, 100);
    AnimateCtx ctx = {.kind = DVUI_ANIMATE_VERTICAL};
    // snímek 0: první snímek – spustí „_start“ (0 → 1 za 160 ms), hodnota 0;
    // min. velikost ještě neznáme, takže výška se nemění
    step(&h, nullptr, frame_animate, &ctx);
    CHECK(ctx.has_val);
    CHECK_NEAR(ctx.val, 0.0f);
    CHECK_NEAR(ctx.rect.h, 60.0f);
    // snímky 1..9: výška = 60 · k/10 (rect je větší než min. velikost,
    // která se v deinit násobí hodnotou)
    for (int k = 1; k <= 9; k++) {
        step(&h, nullptr, frame_animate, &ctx);
        CHECK_NEAR(ctx.val, k / 10.0f);
        CHECK_NEAR(ctx.rect.h, 60.0f * k / 10.0f);
        CHECK_NEAR(ctx.rect.w, 200.0f);
    }
    // snímek 10: done, hodnota 1
    step(&h, nullptr, frame_animate, &ctx);
    CHECK_NEAR(ctx.val, 1.0f);
    CHECK_NEAR(ctx.rect.h, 60.0f);
    // pak animace zmizí: žádná hodnota, plná výška, nic nežádá snímky
    CHECK(step(&h, nullptr, frame_animate, &ctx) == -1);
    CHECK(!ctx.has_val);
    CHECK_NEAR(ctx.rect.h, 60.0f);

    // startEnd: 1 → 0; poslední snímek end() == true
    ctx.start_end = true;
    step(&h, nullptr, frame_animate, &ctx);
    CHECK(!ctx.end_done);
    int frames = 0;
    while (!ctx.end_done && frames < 20) {
        step(&h, nullptr, frame_animate, &ctx);
        frames++;
    }
    CHECK(frames == 10);
    CHECK(ctx.end_done);
    CHECK_NEAR(ctx.val, 0.0f);
    harness_deinit(&h);
}

static void test_animate_horizontal(void) {
    Harness h;
    harness_init(&h, 200, 100);
    // gravity 0.5: zmenšený obdélník je vystředěný
    AnimateCtx ctx = {.kind = DVUI_ANIMATE_HORIZONTAL, .gravity = 0.5f};
    step(&h, nullptr, frame_animate, &ctx);
    for (int k = 1; k <= 4; k++) step(&h, nullptr, frame_animate, &ctx);
    CHECK_NEAR(ctx.val, 0.4f);
    CHECK_NEAR(ctx.rect.w, 80.0f);
    CHECK_NEAR(ctx.rect.x, 0.5f * (200.0f - 80.0f));
    CHECK_NEAR(ctx.rect.h, 60.0f);
    harness_deinit(&h);
}

static void test_animate_alpha(void) {
    Harness h;
    harness_init(&h, 200, 100);
    AnimateCtx ctx = {.kind = DVUI_ANIMATE_ALPHA};
    step(&h, nullptr, frame_animate, &ctx);
    for (int k = 1; k <= 10; k++) {
        step(&h, nullptr, frame_animate, &ctx);
        const float v = k < 10 ? k / 10.0f : 1.0f;
        CHECK_NEAR(ctx.val, v);
        // velikost se nemění
        CHECK_NEAR(ctx.rect.h, 60.0f);
        // dítě se kreslí s alfou okna (barva je premultiplikovaná)
        const dvui_TestingDraw* d = find_draw(&h, ctx.child);
        CHECK(d != nullptr);
        if (d) {
            const int a = (int)(255.0f * v);
            CHECK(abs((int)d->color.a - a) <= 1);
            CHECK(abs((int)d->color.b - a) <= 1);
        }
        // po deinit je alfa zase 1
        const dvui_TestingDraw* d2 = find_draw(&h, ctx.after);
        CHECK(d2 != nullptr && d2->color.a == 255);
    }
    harness_deinit(&h);
}

// ---------------------------------------------------------------------------
// waitTime a beginWait (DVUI: Window.waitTime, Window.beginWait)
// ---------------------------------------------------------------------------

static void frame_empty(void* p) {
    (void)p;
    dvui_spacer(DVUI_MIN_SIZE(10, 10));
}

static void test_wait_time(void) {
    Harness h;
    harness_init(&h, 200, 100);
    settle(&h, frame_empty, nullptr);
    // testovací backend vrací nano_time = 0 → „so far“ je 0; slop je 1000 µs
    CHECK(dvui_window_wait_time(h.win, -1) == UINT32_MAX);
    CHECK(dvui_window_wait_time(h.win, 0) == 0);
    CHECK(dvui_window_wait_time(h.win, 100000) == 99000);
    CHECK(dvui_window_loop_target_slop(h.win) == 1000);

    // čas uběhlý od begin se odečte
    h.tb.time_ns = h.time + 30000000; // 30 ms po begin
    CHECK(dvui_window_wait_time(h.win, 100000) == 69000);
    CHECK(dvui_window_wait_time(h.win, 20000) == 0);

    // max_fps 50 → min. 20 ms na snímek; backend „spí“ (testovací nic nedělá)
    h.tb.time_ns = h.time;
    dvui_window_max_fps_set(h.win, 50);
    CHECK(dvui_window_wait_time(h.win, 0) == 0);
    CHECK(dvui_window_wait_time(h.win, -1) == UINT32_MAX);
    // cíl je begin + 20 ms; probuzení o 5 ms později → slop se zvětší o 1
    CHECK(dvui_window_wait_time(h.win, 0) == 0);
    h.tb.time_ns = h.time + 25000000;
    CHECK(dvui_window_begin_wait(h.win, false) == h.time + 25000000);
    CHECK(dvui_window_loop_target_slop(h.win) == 1001);
    // podruhé o 2 (zdvojení kroku); waitTime se volá hned po end (so far = 0)
    h.tb.time_ns = h.time;
    dvui_window_wait_time(h.win, 0);
    h.tb.time_ns = h.time + 25000000;
    CHECK(dvui_window_begin_wait(h.win, false) == h.time + 25000000);
    CHECK(dvui_window_loop_target_slop(h.win) == 1003);

    // podmíněné čekání přerušené událostí slop nemění
    dvui_window_max_fps_set(h.win, 0);
    h.tb.time_ns = h.time;
    CHECK(dvui_window_wait_time(h.win, 100000) == 100000 - 1003);
    h.tb.time_ns = h.time + 1000000;
    CHECK(dvui_window_begin_wait(h.win, true) == h.time + 1000000);
    CHECK(dvui_window_loop_target_slop(h.win) == 1003);
    harness_deinit(&h);
}

int main(void) {
    test_place_in();
    test_box_vertical();
    test_box_horizontal_expand();
    test_box_equal_space();
    test_button();
    test_easing();
    test_animation_value();
    test_animation_lifetime();
    test_timer();
    test_reduce_motion();
    test_animate_vertical();
    test_animate_horizontal();
    test_animate_alpha();
    test_wait_time();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
