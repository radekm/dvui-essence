// Plovoucí widgety (subwindows): Floating, FloatingTooltip, FloatingWindow,
// a lišta okna (windowHeader).
// DVUI: src/widgets/FloatingWidget.zig, FloatingTooltipWidget.zig,
//       FloatingWindowWidget.zig, dvui.zig – tooltip, floatingWindow, windowHeader
#include "../internal.h"

#include <math.h>

// Společný vtable pro plovoucí widgety: děti umísťuje do content rectu
// (DVUI: rectFor/screenRectScale/minSizeForChild jsou u všech tří stejné).
#define FLOATING_VTABLE(Type, prefix) \
    static dvui_WidgetData* prefix##_data(void* ptr) { return &((Type*)ptr)->wd; } \
    static dvui_Rect prefix##_rect_for(void* ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) { \
        (void)id; \
        return dvui_place_in(dvui_rect_just_size(dvui_widget_data_content_rect(&((Type*)ptr)->wd)), min_size, e, g); \
    } \
    static dvui_RectScale prefix##_screen_rect_scale(void* ptr, dvui_Rect r) { \
        return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_content_rect_scale(&((Type*)ptr)->wd), r); \
    } \
    static void prefix##_min_size_for_child(void* ptr, dvui_Size s) { \
        Type* self = ptr; \
        dvui_widget_data_min_size_max(&self->wd, dvui_options_pad_size(&self->wd.options, s)); \
    } \
    static const dvui_WidgetVTable prefix##_vtable = {prefix##_data, prefix##_rect_for, prefix##_screen_rect_scale, prefix##_min_size_for_child};

// ===========================================================================
// FloatingWidget (DVUI: FloatingWidget.zig)
// ===========================================================================

FLOATING_VTABLE(dvui_FloatingWidget, floating)

dvui_Widget dvui_floating_widget_widget(dvui_FloatingWidget* self) { return (dvui_Widget){self, &floating_vtable}; }

// Subwindow pro libovolnou dočasnou plovoucí věc. Sám se nefokusuje; kdy se
// ukazuje, rozhoduje volající kód.
void dvui_floating_widget_init(dvui_FloatingWidget* self, dvui_Src src, dvui_Props props) {
    static const dvui_Prop defaults[] = {DVUI_NAME("Floating"), {0}};
    dvui_Options opts;
    dvui_WidgetProps wp;
    dvui_props_resolve("Floating", defaults, props,
                       DVUI_TAGS(DVUI_PROP_mouse_events, DVUI_PROP_from, DVUI_PROP_from_gravity_x, DVUI_PROP_from_gravity_y,
                                 DVUI_PROP_stay_above_parent),
                       &opts, &wp);
    const dvui_FloatingInitOptions init_opts = {
        .mouse_events = wp.mouse_events,
        .has_from = wp.has[DVUI_PROP_from],
        .from = wp.from,
        .from_gravity_x = wp.from_gravity_x,
        .from_gravity_y = wp.from_gravity_y,
        .stay_above_parent = wp.stay_above_parent,
    };

    // měřítko podle rodiče
    const float scale_val = dvui_widget_screen_rect_scale(dvui_parent_get(), (dvui_Rect){0}).s / dvui_window_natural_scale();
    if (dvui_options_has(&opts, DVUI_BIT(min_size_content))) opts.min_size_content = dvui_size_scale(opts.min_size_content, scale_val);

    // Zadaný options.rect zabrání volání rectFor/minSizeForChild rodiče –
    // jsme mimo normální layout (nezadaný rect = {0}).
    opts.set |= DVUI_BIT(rect);
    *self = (dvui_FloatingWidget){
        .scale_val = scale_val,
        .init_opts = init_opts,
        .wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){.subwindow = true}, opts),
    };

    if (init_opts.has_from) {
        dvui_Size ms;
        if (dvui_min_size_get(self->wd.id, &ms)) {
            const float gx = init_opts.from_gravity_x;
            const float gy = init_opts.from_gravity_y;
            const dvui_Point npt = dvui_rect_scale_point_from_physical(dvui_window_rect_scale(), init_opts.from);
            dvui_Rect start = dvui_rect_to_size(dvui_rect_from_point(npt), ms);
            start.x -= start.w * (1.0f - gx);
            start.y -= start.h * (1.0f - gy);
            self->wd.rect = dvui_place_on_screen(dvui_window_rect(), (dvui_Rect){0}, DVUI_AVOID_NONE, start);
        } else {
            // na min. velikost potřebujeme další snímek
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        }
    }

    dvui_widget_data_register(&self->wd);
    dvui_parent_set(dvui_floating_widget_widget(self));

    // standardní kroky subwindow
    {
        const dvui_RectScale rs = dvui_widget_data_rect_scale(&self->wd);
        dvui_render_front_to_back_init_reset(&self->render_ftb);
        self->prev_window_info = dvui_subwindow_current_set(self->wd.id, nullptr);
        dvui_subwindow_add(self->wd.id, self->wd.rect, rs.r, false, init_opts.stay_above_parent ? self->prev_window_info.id : 0, init_opts.mouse_events);
        dvui_capture_mouse_maintain((dvui_CaptureMouse){.id = self->wd.id, .rect = rs.r, .subwindow_id = self->wd.id});
        self->prev_clip = dvui_clip_get();
        dvui_clip_set(dvui_window_rect_pixels()); // vystoupíme z ořezu rodiče
        self->prev_scroll = dvui_scroll_container_scroll_set(nullptr);
    }

    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){0});
    dvui_scale_widget_init(&self->scaler, DVUI_SRC, DVUI_PROPS(DVUI_P(scale, &self->scale_val), DVUI_EXPAND(DVUI_EXPAND_BOTH)));
}

void dvui_floating_widget_deinit(dvui_FloatingWidget* self) {
    dvui_scale_widget_deinit(&self->scaler);
    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    // mimo normální layout – rodiči min. velikost nehlásíme
    dvui_parent_reset(self->wd.id, self->wd.parent);

    // standardní kroky subwindow
    dvui_scroll_container_scroll_set(self->prev_scroll);
    dvui_subwindow_current_set(self->prev_window_info.id, &self->prev_window_info.rect);
    dvui_clip_set(self->prev_clip);
    dvui_render_front_to_back_deinit(&self->render_ftb);
    if (dvui_widget_is_allocated(self)) dvui_widget_free(self);
}

// ===========================================================================
// FloatingTooltipWidget (DVUI: FloatingTooltipWidget.zig)
// ===========================================================================

// řetěz vnořených tooltipů
static dvui_FloatingTooltipWidget* tooltip_current = nullptr;

static dvui_FloatingTooltipWidget* tooltip_set(dvui_FloatingTooltipWidget* tt) {
    dvui_FloatingTooltipWidget* ret = tooltip_current;
    tooltip_current = tt;
    return ret;
}

const dvui_Prop dvui_floating_tooltip_widget_defaults[] = {
    DVUI_NAME("Tooltip"),
    DVUI_STYLE(DVUI_STYLE_CONTENT),
    DVUI_BORDER_ALL(1),
    DVUI_BACKGROUND(true),
    {0},
};

static const dvui_PropTag tooltip_tags[] = {
    DVUI_PROP_active_rect, DVUI_PROP_position, DVUI_PROP_interactive, DVUI_PROP_tooltip_scale, DVUI_PROP_END,
};

FLOATING_VTABLE(dvui_FloatingTooltipWidget, tooltip)

dvui_Widget dvui_floating_tooltip_widget_widget(dvui_FloatingTooltipWidget* self) { return (dvui_Widget){self, &tooltip_vtable}; }
dvui_WidgetData* dvui_floating_tooltip_widget_data(dvui_FloatingTooltipWidget* self) { return &self->wd; }

// Ukáže se, když je myš v active_rect. Zmizí, když je myš mimo active_rect,
// mimo tooltip a neukazuje se žádný vnořený tooltip.
void dvui_floating_tooltip_widget_init(dvui_FloatingTooltipWidget* self, dvui_Src src, dvui_Props props) {
    dvui_Options options;
    dvui_WidgetProps wp;
    dvui_props_resolve("FloatingTooltip", dvui_floating_tooltip_widget_defaults, props, tooltip_tags, &options, &wp);
    const dvui_FloatingTooltipInitOptions init_opts = {
        .active_rect = wp.active_rect,
        .position = wp.position,
        .interactive = wp.interactive,
        .has_scale = wp.has[DVUI_PROP_tooltip_scale],
        .scale = wp.tooltip_scale,
    };

    // Do layoutu jde jen identita (id_extra) a rect; zbytek stylizuje obsah
    // (viz self->options).
    const dvui_Options wd_opts = dvui_options_with(dvui_options_default(), DVUI_NAME("FloatingTooltip"), DVUI_RECT(options.rect),
                                                   DVUI_ID_EXTRA(options.id_extra));
    *self = (dvui_FloatingTooltipWidget){
        .wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){.subwindow = true}, wd_opts),
        // měřítko podle rodiče
        .scale_val = init_opts.has_scale ? init_opts.scale
                                         : dvui_widget_screen_rect_scale(dvui_parent_get(), (dvui_Rect){0}).s / dvui_window_natural_scale(),
        .options = options,
        .init_options = init_opts,
    };

    if (dvui_options_has(&self->options, DVUI_BIT(min_size_content))) {
        self->options.min_size_content = dvui_size_scale(self->options.min_size_content, self->scale_val);
    }

    // rect nepředáváme scaleru
    self->options.rect = (dvui_Rect){0};
    self->options.set &= ~DVUI_BIT(rect);

    const bool* showing = DVUI_DATA_GET(self->wd.id, "_showing", bool);
    if (showing) self->showing = *showing;
}

static void tooltip_install(dvui_FloatingTooltipWidget* self) {
    self->installed = true;
    dvui_widget_data_register(&self->wd);
    dvui_parent_set(dvui_floating_tooltip_widget_widget(self));

    // standardní kroky subwindow
    {
        const dvui_RectScale rs = dvui_widget_data_rect_scale(&self->wd);
        dvui_render_front_to_back_init_reset(&self->render_ftb);
        self->prev_window_info = dvui_subwindow_current_set(self->wd.id, nullptr);
        dvui_subwindow_add(self->wd.id, self->wd.rect, rs.r, false, self->prev_window_info.id, true);
        dvui_capture_mouse_maintain((dvui_CaptureMouse){.id = self->wd.id, .rect = rs.r, .subwindow_id = self->wd.id});
        self->prev_clip = dvui_clip_get();
        dvui_clip_set(dvui_window_rect_pixels());
        self->prev_scroll = dvui_scroll_container_scroll_set(nullptr);
    }

    self->parent_tooltip = tooltip_set(self);

    // pozadí, okraj a padding kreslí scaler
    dvui_scale_widget_init(&self->scaler, DVUI_SRC,
                          DVUI_PROPS(DVUI_P(scale, &self->scale_val), DVUI_OPTIONS(&self->options), DVUI_EXPAND(DVUI_EXPAND_BOTH)));
}

// DVUI: FloatingTooltipWidget.shown
bool dvui_floating_tooltip_widget_shown(dvui_FloatingTooltipWidget* self) {
    // ochrana proti opakovanému volání
    if (self->installed) return true;

    // je myš v active_rect?
    size_t n;
    dvui_Event* evts = dvui_events(&n);
    for (size_t i = 0; i < n; i++) {
        dvui_Event* e = &evts[i];
        if (!dvui_event_match(e, (dvui_EventMatchOptions){.id = self->wd.id, .r = self->init_options.active_rect})) continue;
        if (e->kind == DVUI_EVENT_MOUSE && e->evt.mouse.action == DVUI_MOUSE_POSITION) {
            self->mouse_good_this_frame = true;
            self->showing = true;
        }
    }

    if (!self->showing) return false;

    switch (self->init_options.position) {
    case DVUI_TOOLTIP_HORIZONTAL:
    case DVUI_TOOLTIP_VERTICAL: {
        const dvui_RectNatural ar = dvui_rect_to_natural(self->init_options.active_rect);
        const dvui_RectNatural r = dvui_rect_to_size(dvui_rect_from_point(dvui_rect_top_left(ar)), dvui_rect_size(self->wd.rect));
        self->wd.rect = dvui_place_on_screen(dvui_window_rect(), ar, self->init_options.position == DVUI_TOOLTIP_HORIZONTAL ? DVUI_AVOID_HORIZONTAL : DVUI_AVOID_VERTICAL, r);
        break;
    }
    case DVUI_TOOLTIP_STICKY:
        if (dvui_first_frame(self->wd.id)) {
            const dvui_Point mp = dvui_point_scale(dvui_current_window()->mouse_pt, 1.0f / dvui_window_natural_scale());
            DVUI_DATA_SET(self->wd.id, "_sticky_pt", dvui_Point, mp);
        } else {
            const dvui_Point* mpp = DVUI_DATA_GET(self->wd.id, "_sticky_pt", dvui_Point);
            const dvui_Point mp = mpp ? *mpp : (dvui_Point){0};
            dvui_RectNatural r = dvui_rect_to_size(dvui_rect_from_point(mp), dvui_rect_size(self->wd.rect));
            r.x += 10;
            r.y -= r.h + 10;
            self->wd.rect = dvui_place_on_screen(dvui_window_rect(), (dvui_Rect){0}, DVUI_AVOID_NONE, r);
        }
        break;
    case DVUI_TOOLTIP_ABSOLUTE: break;
    }

    tooltip_install(self);

    if (self->init_options.interactive) {
        // je myš nad samotným tooltipem?
        for (size_t i = 0; i < n; i++) {
            dvui_Event* e = &evts[i];
            if (!dvui_event_match(e, (dvui_EventMatchOptions){.id = self->wd.id, .r = dvui_widget_data_border_rect_scale(&self->wd).r})) continue;
            if (e->kind == DVUI_EVENT_MOUSE && e->evt.mouse.action == DVUI_MOUSE_POSITION) self->mouse_good_this_frame = true;
        }
    }
    return true;
}

// DVUI: FloatingTooltipWidget.deinit
void dvui_floating_tooltip_widget_deinit(dvui_FloatingTooltipWidget* self) {
    if (!self->installed) {
        if (dvui_widget_is_allocated(self)) dvui_widget_free(self);
        return;
    }

    // máme se ukazovat i příště?
    if (self->mouse_good_this_frame || (self->init_options.interactive && self->tt_child_shown)) {
        DVUI_DATA_SET(self->wd.id, "_showing", bool, true);
        for (dvui_FloatingTooltipWidget* p = self->parent_tooltip; p; p = p->parent_tooltip) p->tt_child_shown = true;
    } else {
        // myš je mimo spoušť i tooltip – příště se zavře
        dvui_data_remove(nullptr, self->wd.id, "_showing");
        dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
    }

    dvui_scale_widget_deinit(&self->scaler);
    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    // mimo normální layout – rodiči min. velikost nehlásíme
    tooltip_set(self->parent_tooltip);
    dvui_parent_reset(self->wd.id, self->wd.parent);

    // standardní kroky subwindow
    dvui_scroll_container_scroll_set(self->prev_scroll);
    dvui_subwindow_current_set(self->prev_window_info.id, &self->prev_window_info.rect);
    dvui_clip_set(self->prev_clip);
    dvui_render_front_to_back_deinit(&self->render_ftb);
    if (dvui_widget_is_allocated(self)) dvui_widget_free(self);
}

// DVUI: dvui.tooltip – místo textu zástupce o velikosti content_size.
void dvui_tooltip_impl(dvui_Src src, dvui_Props props) {
    static const dvui_Prop defaults[] = {DVUI_PADDING_ALL(6), {0}};
    static const dvui_PropTag tags[] = {
        DVUI_PROP_active_rect, DVUI_PROP_position,   DVUI_PROP_interactive,
        DVUI_PROP_tooltip_scale, DVUI_PROP_label_size, DVUI_PROP_END,
    };
    dvui_Options options;
    dvui_WidgetProps wp;
    dvui_props_resolve("tooltip", defaults, props, tags, &options, &wp);

    // tooltipu předáme Options a jeho vlastní vlastnosti (bez label_size)
    dvui_Prop tt_props[DVUI_PROP_COUNT + 2];
    tt_props[0] = (dvui_Prop)DVUI_OPTIONS(&options);
    const size_t n = 1 + dvui_props_forward(&wp, tooltip_tags, tt_props + 1);
    tt_props[n] = (dvui_Prop){0};

    dvui_FloatingTooltipWidget tt;
    dvui_floating_tooltip_widget_init(&tt, src, (dvui_Props){tt_props});
    if (dvui_floating_tooltip_widget_shown(&tt)) {
        const dvui_Options label_opts = dvui_options_strip(options);
        dvui_label_placeholder(DVUI_OPTIONS(&label_opts), DVUI_P(label_size, wp.label_size));
    }
    dvui_floating_tooltip_widget_deinit(&tt);
}

// ===========================================================================
// FloatingWindowWidget (DVUI: FloatingWindowWidget.zig)
// ===========================================================================

const dvui_Prop dvui_floating_window_widget_defaults[] = {
    DVUI_NAME("FloatingWindow"),
    DVUI_MARGIN_ALL(2),
    DVUI_BORDER_ALL(1),
    DVUI_BACKGROUND(true),
    DVUI_STYLE(DVUI_STYLE_WINDOW),
    {0},
};

FLOATING_VTABLE(dvui_FloatingWindowWidget, fwin)

dvui_Widget dvui_floating_window_widget_widget(dvui_FloatingWindowWidget* self) { return (dvui_Widget){self, &fwin_vtable}; }
dvui_WidgetData* dvui_floating_window_widget_data(dvui_FloatingWindowWidget* self) { return &self->wd; }

static dvui_Cursor drag_part_cursor(dvui_DragPart p) {
    switch (p) {
    case DVUI_DRAG_MIDDLE: return DVUI_CURSOR_ARROW_ALL;
    case DVUI_DRAG_TOP_LEFT:
    case DVUI_DRAG_BOTTOM_RIGHT: return DVUI_CURSOR_ARROW_NW_SE;
    case DVUI_DRAG_BOTTOM_LEFT:
    case DVUI_DRAG_TOP_RIGHT: return DVUI_CURSOR_ARROW_NE_SW;
    case DVUI_DRAG_BOTTOM:
    case DVUI_DRAG_TOP: return DVUI_CURSOR_ARROW_N_S;
    case DVUI_DRAG_LEFT:
    case DVUI_DRAG_RIGHT: return DVUI_CURSOR_ARROW_W_E;
    }
    return DVUI_CURSOR_ARROW;
}

// DVUI: FloatingWindowWidget.init
void dvui_floating_window_widget_init(dvui_FloatingWindowWidget* self, dvui_Src src, dvui_Props props) {
    static const dvui_PropTag tags[] = {
        DVUI_PROP_modal,     DVUI_PROP_modal_alpha, DVUI_PROP_rect_io,                  DVUI_PROP_center_on,
        DVUI_PROP_open_flag, DVUI_PROP_resize,      DVUI_PROP_stay_above_parent_window, DVUI_PROP_window_avoid,
        DVUI_PROP_END,
    };
    dvui_Options options;
    dvui_WidgetProps wp;
    dvui_props_resolve("FloatingWindow", dvui_floating_window_widget_defaults, props, tags, &options, &wp);
    const dvui_FloatingWindowInitOptions init_opts = {
        .modal = wp.modal,
        .has_modal_alpha = wp.has[DVUI_PROP_modal_alpha],
        .modal_alpha = wp.modal_alpha,
        .rect = wp.rect_io,
        .has_center_on = wp.has[DVUI_PROP_center_on],
        .center_on = wp.center_on,
        .open_flag = wp.open_flag,
        .resize = wp.resize,
        .stay_above_parent_window = wp.stay_above_parent_window,
        .window_avoid = wp.window_avoid,
    };

    dvui_Options box_options = options;
    box_options.id_extra = 0;
    box_options.rect = (dvui_Rect){0}; // rect od uživatele nepatří BoxWidgetu
    box_options.set &= ~(DVUI_BIT(id_extra) | DVUI_BIT(rect));

    *self = (dvui_FloatingWindowWidget){
        .options = box_options,
        .wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){.subwindow = true},
                                    dvui_options_with(dvui_options_default(), DVUI_ID_EXTRA(options.id_extra),
                                                      DVUI_RECT((dvui_Rect){0}), DVUI_NAME(options.name))),
        .init_options = init_opts,
    };
    dvui_WidgetData* wd = &self->wd;
    dvui_Window* cw = dvui_current_window();

    const dvui_Rect* stored = DVUI_DATA_GET(wd->id, "_rect", dvui_Rect);
    wd->rect = stored ? *stored : (dvui_Rect){0};
    if (init_opts.rect) wd->rect = *init_opts.rect;
    if (dvui_first_frame(wd->id) && dvui_options_has(&options, DVUI_BIT(rect))) wd->rect = options.rect;

    const bool* as = DVUI_DATA_GET(wd->id, "_auto_size", bool);
    if (as) {
        self->auto_size = *as;
    } else if (wd->rect.w == 0 && wd->rect.h == 0) {
        dvui_floating_window_widget_auto_size(self);
    }

    const bool* ap = DVUI_DATA_GET(wd->id, "_auto_pos", bool);
    if (ap) {
        self->auto_pos = *ap;
    } else {
        self->auto_pos = wd->rect.x == 0 && wd->rect.y == 0;
    }

    dvui_Size min_size;
    if (dvui_min_size_get(wd->id, &min_size)) {
        if (self->auto_size) {
            // při auto-size nechceme zbytečné snímky navíc
            self->has_auto_size_refresh_prev_value = true;
            self->auto_size_refresh_prev_value = cw->extra_frames_needed;
            cw->extra_frames_needed = 0;
            const dvui_Size ms = dvui_size_min(dvui_size_max(min_size, dvui_options_min_size_get(&self->options)), dvui_rect_size(dvui_window_rect()));
            wd->rect.w = ms.w;
            wd->rect.h = ms.h;
        }

        if (self->auto_pos) {
            // vycentrujeme na zadaný obdélník nebo na rodičovské subwindow
            self->auto_pos = false;
            const dvui_RectNatural centering = init_opts.has_center_on ? init_opts.center_on : cw->subwindows.current_rect;
            wd->rect.x = centering.x + (centering.w - wd->rect.w) / 2;
            wd->rect.y = centering.y + (centering.h - wd->rect.h) / 2;

            if (dvui_snap_to_pixels()) {
                const float s = dvui_window_natural_scale();
                wd->rect.x = roundf(wd->rect.x * s) / s;
                wd->rect.y = roundf(wd->rect.y * s) / s;
            }

            if (init_opts.window_avoid != DVUI_WINDOW_AVOID_NONE) {
                if (dvui_point_equals(dvui_rect_top_left(wd->rect), dvui_rect_top_left(centering))) {
                    // přesně na místě rodiče – posuneme, aby šlo poznat, že jde o nové okno
                    wd->rect.x += 24;
                    wd->rect.y += 24;
                }
            }

            if (init_opts.window_avoid == DVUI_WINDOW_AVOID_NUDGE) {
                bool nudge = true;
                while (nudge) {
                    nudge = false;
                    for (size_t i = 1; i < cw->subwindows.len; i++) {
                        const dvui_Subwindow* subw = &cw->subwindows.stack[i];
                        if (subw->id != wd->id && dvui_point_equals(dvui_rect_top_left(subw->rect), dvui_rect_top_left(wd->rect))) {
                            wd->rect.x += 24;
                            wd->rect.y += 24;
                            nudge = true;
                        }
                    }
                }
            }
        }

        // Okno musí zůstat na obrazovce aspoň částečně, aby šlo chytit.
        dvui_RectNatural screen = dvui_window_rect();
        const float offleft = wd->rect.w - 48;
        screen.x -= offleft;
        screen.w += offleft + offleft;
        screen.h += wd->rect.h - 24;
        wd->rect = dvui_place_on_screen(screen, (dvui_Rect){0}, DVUI_AVOID_NONE, wd->rect);
    }

    dvui_widget_data_register(wd);

    if (dvui_first_frame(wd->id)) {
        // první snímek: fokus a uložení rectu; okno zatím nekreslíme (velikost 0)
        dvui_focus_subwindow(wd->id, nullptr);
        dvui_data_set(nullptr, wd->id, "_rect", &wd->rect, sizeof(dvui_Rect));
        if (init_opts.rect) *init_opts.rect = wd->rect;
        dvui_refresh(nullptr, DVUI_SRC, wd->id);
        wd->rect.w = 0;
        wd->rect.h = 0;
    }

    if (dvui_captured(wd->id)) {
        const dvui_DragPart* dp = DVUI_DATA_GET(wd->id, "_drag_part", dvui_DragPart);
        if (dp) {
            self->has_drag_part = true;
            self->drag_part = *dp;
        }
    }

    dvui_parent_set(dvui_floating_window_widget_widget(self));

    // standardní kroky subwindow
    {
        const dvui_RectScale rs = dvui_widget_data_rect_scale(wd);
        dvui_render_front_to_back_init_reset(&self->render_ftb);
        const dvui_RectNatural cur = wd->rect;
        self->prev_window_info = dvui_subwindow_current_set(wd->id, &cur);
        dvui_subwindow_add(wd->id, wd->rect, rs.r, init_opts.modal, init_opts.stay_above_parent_window ? self->prev_window_info.id : 0, true);
        dvui_capture_mouse_maintain((dvui_CaptureMouse){.id = wd->id, .rect = rs.r, .subwindow_id = wd->id});
        self->prev_clip = dvui_clip_get();
        dvui_clip_set(dvui_window_rect_pixels());
        self->prev_scroll = dvui_scroll_container_scroll_set(nullptr);
    }

    self->prev_last_focus = dvui_last_focused_id_in_frame();
    self->drag_area = dvui_widget_data_rect_scale(wd).r;
}

// DVUI: FloatingWindowWidget.drawBackground
void dvui_floating_window_widget_draw_background(dvui_FloatingWindowWidget* self) {
    if (self->init_options.modal && !dvui_first_frame(self->wd.id)) {
        dvui_Color col = dvui_options_color(&self->options, DVUI_COLOR_TEXT);
        col.a = self->init_options.has_modal_alpha ? self->init_options.modal_alpha : (dvui_theme_get()->dark ? 60 : 80);
        dvui_render_fill_rect(dvui_window_rect_pixels(), col, false);
    }
    self->has_layout = true;
    dvui_box_widget_init(&self->layout, DVUI_SRC, DVUI_PROPS(DVUI_OPTIONS(&self->options), DVUI_EXPAND(DVUI_EXPAND_BOTH)));
    dvui_box_widget_draw_background(&self->layout);
}

// DVUI: FloatingWindowWidget.dragPart
static dvui_DragPart drag_part(dvui_EventMouse me, dvui_RectScale rs) {
    const float corner_size = rs.s * 15;
    const bool top = me.p.y < rs.r.y + corner_size;
    const bool bottom = me.p.y > rs.r.y + rs.r.h - corner_size;
    const bool left = me.p.x < rs.r.x + corner_size;
    const bool right = me.p.x > rs.r.x + rs.r.w - corner_size;
    if (bottom && right) return DVUI_DRAG_BOTTOM_RIGHT;
    if (top && left) return DVUI_DRAG_TOP_LEFT;
    if (bottom && left) return DVUI_DRAG_BOTTOM_LEFT;
    if (top && right) return DVUI_DRAG_TOP_RIGHT;

    const float side_size = rs.s * 4;
    if (me.p.y > rs.r.y + rs.r.h - side_size) return DVUI_DRAG_BOTTOM;
    if (me.p.y < rs.r.y + side_size) return DVUI_DRAG_TOP;
    if (me.p.x < rs.r.x + side_size) return DVUI_DRAG_LEFT;
    if (me.p.x > rs.r.x + rs.r.w - side_size) return DVUI_DRAG_RIGHT;
    return DVUI_DRAG_MIDDLE;
}

// DVUI: FloatingWindowWidget.dragAdjust (p a dp v natural souřadnicích)
static void drag_adjust(dvui_FloatingWindowWidget* self, dvui_PointNatural p, dvui_PointNatural dp, dvui_DragPart part) {
    dvui_Rect* r = &self->wd.rect;
    switch (part) {
    case DVUI_DRAG_MIDDLE:
        r->x += dp.x;
        r->y += dp.y;
        break;
    case DVUI_DRAG_BOTTOM_RIGHT:
        r->w = DVUI_MAX(40.0f, p.x - r->x);
        r->h = DVUI_MAX(10.0f, p.y - r->y);
        break;
    case DVUI_DRAG_TOP_LEFT: {
        const dvui_Point anchor = dvui_rect_bottom_right(*r);
        const float new_w = DVUI_MAX(40.0f, r->w - (p.x - r->x));
        const float new_h = DVUI_MAX(10.0f, r->h - (p.y - r->y));
        r->x = anchor.x - new_w;
        r->w = new_w;
        r->y = anchor.y - new_h;
        r->h = new_h;
        break;
    }
    case DVUI_DRAG_BOTTOM_LEFT: {
        const dvui_Point anchor = dvui_rect_top_right(*r);
        const float new_w = DVUI_MAX(40.0f, r->w - (p.x - r->x));
        r->x = anchor.x - new_w;
        r->w = new_w;
        r->h = DVUI_MAX(10.0f, p.y - r->y);
        break;
    }
    case DVUI_DRAG_TOP_RIGHT: {
        const dvui_Point anchor = dvui_rect_bottom_left(*r);
        r->w = DVUI_MAX(40.0f, p.x - r->x);
        const float new_h = DVUI_MAX(10.0f, r->h - (p.y - r->y));
        r->y = anchor.y - new_h;
        r->h = new_h;
        break;
    }
    case DVUI_DRAG_BOTTOM: r->h = DVUI_MAX(10.0f, p.y - r->y); break;
    case DVUI_DRAG_TOP: {
        const dvui_Point anchor = dvui_rect_bottom_left(*r);
        const float new_h = DVUI_MAX(10.0f, r->h - (p.y - r->y));
        r->y = anchor.y - new_h;
        r->h = new_h;
        break;
    }
    case DVUI_DRAG_LEFT: {
        const dvui_Point anchor = dvui_rect_top_right(*r);
        const float new_w = DVUI_MAX(40.0f, r->w - (p.x - r->x));
        r->x = anchor.x - new_w;
        r->w = new_w;
        break;
    }
    case DVUI_DRAG_RIGHT: r->w = DVUI_MAX(40.0f, p.x - r->x); break;
    }
}

static dvui_PointNatural to_natural(dvui_PointPhysical p) { return dvui_point_scale(p, 1.0f / dvui_window_natural_scale()); }

// DVUI: FloatingWindowWidget.processEventsBefore – tažení a pravý dolní roh
// musí mít přednost před dětmi.
void dvui_floating_window_widget_process_events_before(dvui_FloatingWindowWidget* self) {
    const dvui_RectScale rs = dvui_widget_data_rect_scale(&self->wd);
    size_t n;
    dvui_Event* evts = dvui_events(&n);
    for (size_t i = 0; i < n; i++) {
        dvui_Event* e = &evts[i];
        if (!dvui_event_match(e, (dvui_EventMatchOptions){.id = self->wd.id, .r = rs.r})) continue;
        if (e->kind != DVUI_EVENT_MOUSE) continue;
        const dvui_EventMouse me = e->evt.mouse;

        if (me.action == DVUI_MOUSE_FOCUS) {
            // pokud uživatel klikl do okna, fokusujeme ho
            dvui_focus_subwindow(self->wd.id, &e->num);
            continue;
        }

        if (dvui_captured(self->wd.id)) {
            if (me.action == DVUI_MOUSE_PRESS) dvui_event_handle(e, &self->wd);

            if (me.action == DVUI_MOUSE_MOTION) {
                dvui_PointPhysical dps;
                if (dvui_dragging(me, nullptr, &dps)) {
                    const dvui_PointNatural p = to_natural(dvui_point_plus(me.p, dvui_drag_offset(me.button)));
                    drag_adjust(self, p, to_natural(dps), self->drag_part);
                    // rect se změnil, obnovíme rect_scale
                    self->wd.rect_scale = dvui_widget_data_rect_scale_from_parent(&self->wd);
                    dvui_event_handle(e, &self->wd);
                    dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
                    continue;
                }
            }

            if (me.action == DVUI_MOUSE_RELEASE && dvui_button_pointer(me.button)) {
                dvui_capture_mouse(nullptr, e->num); // konec tažení a capture
                dvui_drag_end(me.button);
                dvui_event_handle(e, &self->wd);
                continue;
            }
        }

        if (self->init_options.resize == DVUI_RESIZE_ALL && drag_part(me, rs) == DVUI_DRAG_BOTTOM_RIGHT) {
            if (me.action == DVUI_MOUSE_PRESS && dvui_button_pointer(me.button)) {
                // přednost před dětmi (např. tlačítkem v rohu)
                dvui_capture_mouse(&self->wd, e->num);
                self->has_drag_part = true;
                self->drag_part = DVUI_DRAG_BOTTOM_RIGHT;
                dvui_drag_start(me.button, me.p, (dvui_DragStartOptions){.cursor = DVUI_CURSOR_ARROW_NW_SE, .has_cursor = true, .offset = dvui_point_diff(dvui_rect_bottom_right(rs.r), me.p)});
                dvui_event_handle(e, &self->wd);
                continue;
            }
            if (me.action == DVUI_MOUSE_POSITION) {
                dvui_event_handle(e, &self->wd); // widgety pod tím nemají vidět hover
                dvui_cursor_set(DVUI_CURSOR_ARROW_NW_SE);
                continue;
            }
        }
    }
}

void dvui_floating_window_widget_drag_area_set(dvui_FloatingWindowWidget* self, dvui_RectPhysical rect) { self->drag_area = rect; }

// DVUI: FloatingWindowWidget.processEventsAfter
void dvui_floating_window_widget_process_events_after(dvui_FloatingWindowWidget* self) {
    const dvui_RectScale rs = dvui_widget_data_rect_scale(&self->wd);
    size_t n;
    dvui_Event* evts = dvui_events(&n);
    for (size_t i = 0; i < n; i++) {
        dvui_Event* e = &evts[i];
        if (!dvui_event_match(e, (dvui_EventMatchOptions){.id = self->wd.id, .r = rs.r})) continue;
        if (e->kind != DVUI_EVENT_MOUSE) continue;
        const dvui_EventMouse me = e->evt.mouse;

        switch (me.action) {
        case DVUI_MOUSE_FOCUS:
            dvui_event_handle(e, &self->wd);
            // kliknutí do okna, které nezpracoval žádný widget – zrušíme fokus widgetu
            dvui_focus_widget(0, 0, nullptr);
            break;
        case DVUI_MOUSE_PRESS:
            if (dvui_button_pointer(me.button)) {
                const dvui_DragPart dp = drag_part(me, rs);
                if (dp == DVUI_DRAG_MIDDLE) {
                    if (!dvui_rect_contains(self->drag_area, me.p)) continue;
                } else if (self->init_options.resize == DVUI_RESIZE_NONE) {
                    continue;
                }
                // začátek možného tažení
                dvui_event_handle(e, &self->wd);
                dvui_capture_mouse(&self->wd, e->num);
                self->has_drag_part = true;
                self->drag_part = dp;
                dvui_drag_pre_start(me.button, me.p, (dvui_DragStartOptions){.cursor = drag_part_cursor(dp), .has_cursor = true});
            }
            break;
        case DVUI_MOUSE_RELEASE:
            if (dvui_button_pointer(me.button) && dvui_captured(self->wd.id)) {
                dvui_event_handle(e, &self->wd);
                dvui_capture_mouse(nullptr, e->num);
                dvui_drag_end(me.button);
            }
            break;
        case DVUI_MOUSE_MOTION:
            if (dvui_captured(self->wd.id)) {
                dvui_PointPhysical dps;
                if (dvui_dragging(me, nullptr, &dps)) {
                    const dvui_PointNatural p = to_natural(dvui_point_plus(me.p, dvui_drag_offset(me.button)));
                    drag_adjust(self, p, to_natural(dps), self->drag_part);
                    dvui_event_handle(e, &self->wd);
                    dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
                }
            }
            break;
        case DVUI_MOUSE_POSITION: {
            const dvui_DragPart dp = drag_part(me, rs);
            if (dp == DVUI_DRAG_MIDDLE) {
                if (!dvui_rect_contains(self->drag_area, me.p)) continue;
            } else if (self->init_options.resize == DVUI_RESIZE_NONE) {
                continue;
            }
            dvui_cursor_set(drag_part_cursor(dp));
            break;
        }
        default: break;
        }
    }
}

void dvui_floating_window_widget_auto_size(dvui_FloatingWindowWidget* self) { self->auto_size = true; }
void dvui_floating_window_widget_auto_position(dvui_FloatingWindowWidget* self) { self->auto_pos = true; }

void dvui_floating_window_widget_close(dvui_FloatingWindowWidget* self) {
    if (self->init_options.open_flag) {
        *self->init_options.open_flag = false;
    } else {
        DVUI_LOG_ERR("%s:%d FloatingWindowWidget.close() was called but it has no open_flag", self->wd.src.file, self->wd.src.line);
    }
    dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
}

// DVUI: FloatingWindowWidget.deinit
void dvui_floating_window_widget_deinit(dvui_FloatingWindowWidget* self) {
    dvui_Window* cw = dvui_current_window();
    if (self->has_layout) dvui_box_widget_deinit(&self->layout);

    if (self->has_auto_size_refresh_prev_value) {
        // nikdo nechtěl další snímek → velikost je ustálená
        if (cw->extra_frames_needed == 0) self->auto_size = false;
        cw->extra_frames_needed = DVUI_MAX(cw->extra_frames_needed, self->auto_size_refresh_prev_value);
    }

    // události, které nezpracovaly děti
    dvui_floating_window_widget_process_events_after(self);

    if (!dvui_first_frame(self->wd.id)) {
        // první snímek byl rect uložen v init
        dvui_data_set(nullptr, self->wd.id, "_rect", &self->wd.rect, sizeof(dvui_Rect));
        if (self->init_options.rect) *self->init_options.rect = self->wd.rect;
    }

    if (dvui_captured(self->wd.id) && self->has_drag_part) {
        DVUI_DATA_SET(self->wd.id, "_drag_part", dvui_DragPart, self->drag_part);
    }

    DVUI_DATA_SET(self->wd.id, "_auto_pos", bool, self->auto_pos);
    DVUI_DATA_SET(self->wd.id, "_auto_size", bool, self->auto_size);
    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    dvui_parent_reset(self->wd.id, self->wd.parent);

    // ostatní widgety neuvidí fokus z tohoto okna
    cw->last_focused_id_this_frame = self->prev_last_focus;

    // standardní kroky subwindow
    dvui_scroll_container_scroll_set(self->prev_scroll);
    dvui_subwindow_current_set(self->prev_window_info.id, &self->prev_window_info.rect);
    dvui_clip_set(self->prev_clip);
    dvui_render_front_to_back_deinit(&self->render_ftb);
    if (dvui_widget_is_allocated(self)) dvui_widget_free(self);
}

dvui_FloatingWindowWidget* dvui_floating_window_impl(dvui_Src src, dvui_Props props) {
    dvui_FloatingWindowWidget* ret = dvui_widget_alloc(sizeof *ret);
    dvui_floating_window_widget_init(ret, src, props);
    dvui_floating_window_widget_process_events_before(ret);
    dvui_floating_window_widget_draw_background(ret);
    return ret;
}

// ===========================================================================
// windowHeader (DVUI: dvui.windowHeader)
// ===========================================================================

// Zavírací tlačítko podle DVUI buttonIcon; místo ikony je čtvercový zástupce.
static bool close_button(dvui_Src src, dvui_Options opts) {
    dvui_ButtonWidget bw;
    dvui_button_widget_init(&bw, src, DVUI_PROPS(DVUI_PADDING_ALL(4), DVUI_OPTIONS(&opts)));
    dvui_button_widget_process_events(&bw);
    dvui_button_widget_draw_background(&bw);

    // „ikona“: plný čtverec v barvě textu, velikost jako výška textu
    const dvui_Options style = dvui_button_widget_style(&bw);
    const dvui_Options icon_style = dvui_options_override(dvui_options_strip(opts), style);
    const float icon = dvui_font_size_m(1, 1).h;
    dvui_spacer(DVUI_OPTIONS(&icon_style), DVUI_GRAVITY_X(0.5f), DVUI_GRAVITY_Y(0.5f), DVUI_MIN_SIZE(icon, icon),
                DVUI_EXPAND(DVUI_EXPAND_RATIO), DVUI_BACKGROUND(true),
                DVUI_FILL(dvui_options_color(&bw.wd.options, DVUI_COLOR_TEXT)));

    const bool click = dvui_button_widget_clicked(&bw);
    dvui_button_widget_draw_focus(&bw);
    dvui_button_widget_deinit(&bw);
    return click;
}

dvui_RectPhysical dvui_window_header_impl(dvui_Props props) {
    dvui_Options options;
    dvui_WidgetProps wp;
    dvui_props_resolve("windowHeader", nullptr, props, DVUI_TAGS(DVUI_PROP_label_size, DVUI_PROP_open_flag), &options, &wp);
    (void)options; // DVUI windowHeader nemá Options

    dvui_OverlayWidget* over = dvui_overlay(DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL), DVUI_NAME("WindowHeader"));

    // titulek (zde jen zástupce o velikosti textu)
    dvui_label_placeholder(DVUI_P(label_size, wp.label_size), DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL), DVUI_PADDING({6, 6, 6, 4}));

    dvui_BoxWidget* hbox = dvui_box(DVUI_DIR(DVUI_HORIZONTAL), DVUI_EXPAND(DVUI_EXPAND_BOTH));

    if (wp.open_flag) {
        // DVUI button_order .cancel_ok → zavírací tlačítko vlevo
        const dvui_Options opts = dvui_options_with(dvui_options_default(), DVUI_PADDING_ALL(2), DVUI_MARGIN_ALL(2),
                                                    DVUI_GRAVITY_Y(0.5f), DVUI_EXPAND(DVUI_EXPAND_RATIO), DVUI_GRAVITY_X(0.0f));
        if (close_button(DVUI_SRC, opts)) *wp.open_flag = false;
    }

    dvui_box_widget_deinit(hbox);

    size_t n;
    dvui_Event* evts = dvui_events(&n);
    for (size_t i = 0; i < n; i++) {
        dvui_Event* e = &evts[i];
        if (!dvui_event_match(e, (dvui_EventMatchOptions){.id = over->wd.id, .r = dvui_widget_data_content_rect_scale(&over->wd).r})) continue;
        if (e->kind != DVUI_EVENT_MOUSE) continue;
        if (e->evt.mouse.action == DVUI_MOUSE_PRESS && dvui_button_pointer(e->evt.mouse.button)) {
            // zvedneme okno, ale press necháme dál – okno začne tažení
            dvui_raise_subwindow(dvui_subwindow_current_id());
        } else if (e->evt.mouse.action == DVUI_MOUSE_FOCUS) {
            // okno už fokus má; tímhle zabráníme zrušení fokusu widgetu
            dvui_event_handle(e, &over->wd);
        }
    }

    dvui_RectPhysical ret = dvui_widget_data_rect_scale(&over->wd).r;
    dvui_overlay_widget_deinit(over);

    const dvui_WidgetData swd = dvui_separator(DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL));
    ret.h += dvui_widget_data_rect_scale(&swd).r.h;
    return ret;
}
