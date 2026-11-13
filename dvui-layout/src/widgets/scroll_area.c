// Oblast se scrollováním: hbox [ vbox [ ScrollContainer, hbar ], vbar ].
// DVUI: src/widgets/ScrollAreaWidget.zig
#include "../internal.h"

const dvui_PropTag dvui_scroll_area_widget_tags[] = {
    DVUI_PROP_scroll_info,  DVUI_PROP_vertical,           DVUI_PROP_vertical_bar,         DVUI_PROP_horizontal,
    DVUI_PROP_horizontal_bar, DVUI_PROP_focus_id,         DVUI_PROP_frame_viewport,       DVUI_PROP_frame_viewport_out,
    DVUI_PROP_lock_visible, DVUI_PROP_process_events_after, DVUI_PROP_container,          DVUI_PROP_user_scroll,
    DVUI_PROP_END,
};

const dvui_Prop dvui_scroll_area_widget_defaults[] = {
    DVUI_NAME("ScrollAreaWidget"),
    DVUI_BACKGROUND(true),
    DVUI_STYLE(DVUI_STYLE_WINDOW),
    {0},
};

// DVUI: ScrollAreaWidget.init
void dvui_scroll_area_widget_init(dvui_ScrollAreaWidget* self, dvui_Src src, dvui_Props props) {
    *self = (dvui_ScrollAreaWidget){0};

    dvui_Options opts;
    dvui_WidgetProps init_opts;
    dvui_props_resolve("ScrollArea", dvui_scroll_area_widget_defaults, props, dvui_scroll_area_widget_tags, &opts, &init_opts);

    dvui_box_widget_init(&self->hbox, src, DVUI_PROPS(DVUI_OPTIONS(&opts), DVUI_DIR(DVUI_HORIZONTAL)));
    const dvui_Id hbox_id = self->hbox.wd.id;

    if (init_opts.scroll_info) {
        self->si = init_opts.scroll_info;
    } else {
        const dvui_ScrollInfo def = dvui_scroll_info_default();
        self->si = dvui_data_get_ptr_default(nullptr, hbox_id, "_scroll_info", sizeof(dvui_ScrollInfo), &def);
        // kód mimo widget mohl změnit směr scrollování
        self->si->vertical = init_opts.vertical;
        self->si->horizontal = init_opts.horizontal;
    }

    if (init_opts.frame_viewport_out) *init_opts.frame_viewport_out = dvui_rect_top_left(self->si->viewport);

    dvui_box_widget_draw_background(&self->hbox);

    dvui_Id focus_target = init_opts.focus_id;
    if (!focus_target) {
        const dvui_Id* sid = DVUI_DATA_GET(hbox_id, "_scroll_id", dvui_Id);
        if (sid) focus_target = *sid;
    }

    const dvui_Rect crect = dvui_widget_data_content_rect(&self->hbox.wd);

    // nejdřív předpokládáme, že ScrollContainer dostane všechno místo
    self->si->viewport.w = crect.w;
    self->si->viewport.h = crect.h;

    // a upravíme podle odsazení z minulého snímku
    const dvui_Size* inset = DVUI_DATA_GET(hbox_id, "_linsets", dvui_Size);
    if (inset) {
        self->si->viewport.w -= inset->w;
        self->si->viewport.h -= inset->h;
    }

    const dvui_Options bar_defaults = dvui_options_apply(dvui_options_default(), (dvui_Props){dvui_scroll_bar_widget_defaults});
    const float bar_w = dvui_options_min_size_get(&bar_defaults).w;
    const float bar_h = dvui_options_min_size_get(&bar_defaults).h;

    // kvůli nepřesnosti float si necháme malou rezervu (0.001)
    bool do_vbar = false;
    bool do_hbar = false;
    if (self->si->vertical != DVUI_SCROLL_NONE) {
        if (init_opts.vertical_bar == DVUI_SCROLL_BAR_SHOW ||
            (dvui_scroll_bar_mode_auto_any(init_opts.vertical_bar) && self->si->virtual_size.h > self->si->viewport.h + 0.001f)) {
            do_vbar = true;
            if (init_opts.vertical_bar != DVUI_SCROLL_BAR_AUTO_OVERLAY) self->si->viewport.w -= bar_w;
        }
    }

    if (self->si->horizontal != DVUI_SCROLL_NONE) {
        if (init_opts.horizontal_bar == DVUI_SCROLL_BAR_SHOW ||
            (dvui_scroll_bar_mode_auto_any(init_opts.horizontal_bar) && self->si->virtual_size.w > self->si->viewport.w + 0.001f)) {
            do_hbar = true;
            if (init_opts.horizontal_bar != DVUI_SCROLL_BAR_AUTO_OVERLAY) self->si->viewport.h -= bar_h;
        }
    }

    // vbar znovu – hbar nám mohl ubrat místo
    if (!do_vbar && do_hbar && self->si->vertical != DVUI_SCROLL_NONE) {
        if (dvui_scroll_bar_mode_auto_any(init_opts.vertical_bar) && self->si->virtual_size.h > self->si->viewport.h + 0.001f) {
            do_vbar = true;
            if (init_opts.vertical_bar != DVUI_SCROLL_BAR_AUTO_OVERLAY) self->si->viewport.w -= bar_w;
        }
    }

    if (do_vbar) {
        // posuvníky nejdřív, aby byly vidět i při nedostatku místa
        const bool overlay = init_opts.vertical_bar == DVUI_SCROLL_BAR_AUTO_OVERLAY;
        dvui_ScrollBarWidget vbar;
        const dvui_Options strip = dvui_options_strip(self->hbox.wd.options);
        dvui_scroll_bar_widget_init(&vbar, DVUI_SRC,
                                    DVUI_PROPS(DVUI_SCROLL_INFO(self->si), DVUI_P(focus_id, focus_target), DVUI_OPTIONS(&strip),
                                               DVUI_GRAVITY_X(overlay ? 0.999999f : 1.0f), DVUI_EXPAND(DVUI_EXPAND_VERTICAL)));
        if (overlay) {
            self->has_vbar_grab = true;
            self->vbar_grab = dvui_scroll_bar_widget_grab(&vbar);
        } else {
            dvui_scroll_bar_grab_draw(dvui_scroll_bar_widget_grab(&vbar));
        }
        dvui_scroll_bar_widget_deinit(&vbar);
    }

    const dvui_Options strip = dvui_options_strip(self->hbox.wd.options);
    dvui_box_widget_init(&self->vbox, DVUI_SRC,
                         DVUI_PROPS(DVUI_DIR(DVUI_VERTICAL), DVUI_OPTIONS(&strip), DVUI_EXPAND(DVUI_EXPAND_BOTH),
                                    DVUI_NAME("ScrollAreaWidget vbox")));
    dvui_box_widget_draw_background(&self->vbox);

    if (do_hbar) {
        const bool overlay = init_opts.horizontal_bar == DVUI_SCROLL_BAR_AUTO_OVERLAY;
        dvui_ScrollBarWidget hbar;
        dvui_scroll_bar_widget_init(&hbar, DVUI_SRC,
                                    DVUI_PROPS(DVUI_DIR(DVUI_HORIZONTAL), DVUI_SCROLL_INFO(self->si), DVUI_P(focus_id, focus_target),
                                               DVUI_OPTIONS(&strip), DVUI_EXPAND(DVUI_EXPAND_HORIZONTAL),
                                               DVUI_GRAVITY_Y(overlay ? 0.999999f : 1.0f)));
        if (overlay) {
            self->has_hbar_grab = true;
            self->hbar_grab = dvui_scroll_bar_widget_grab(&hbar);
        } else {
            dvui_scroll_bar_grab_draw(dvui_scroll_bar_widget_grab(&hbar));
        }
        dvui_scroll_bar_widget_deinit(&hbar);
    }

    if (init_opts.container) {
        self->has_scroll = true;
        // frame_viewport předáme jen, když byl zadán (DVUI: ?Point)
        const dvui_Prop container_props[] = {
            DVUI_OPTIONS(&strip),
            DVUI_EXPAND(DVUI_EXPAND_BOTH),
            DVUI_SCROLL_INFO(self->si),
            DVUI_P(scroll_area, self),
            DVUI_P(lock_visible, init_opts.lock_visible),
            DVUI_P(user_scroll, init_opts.user_scroll),
            DVUI_P(process_events_after, init_opts.process_events_after),
            init_opts.has[DVUI_PROP_frame_viewport] ? (dvui_Prop)DVUI_P(frame_viewport, init_opts.frame_viewport) : (dvui_Prop){0},
            {0},
        };
        dvui_scroll_container_widget_init(&self->scroll, DVUI_SRC, (dvui_Props){container_props});
        dvui_scroll_container_widget_process_events(&self->scroll);
    }
}

dvui_WidgetData* dvui_scroll_area_widget_data(dvui_ScrollAreaWidget* self) { return &self->hbox.wd; }

// ukládá se jen levé/horní odsazení – posuvníky jsou vpravo a dole
void dvui_scroll_area_widget_set_container_rect(dvui_ScrollAreaWidget* self, dvui_Rect rect) {
    DVUI_DATA_SET(self->hbox.wd.id, "_linsets", dvui_Size, ((dvui_Size){rect.x, rect.y}));
}

// DVUI: ScrollAreaWidget.deinit
void dvui_scroll_area_widget_deinit(dvui_ScrollAreaWidget* self) {
    if (self->has_scroll) {
        DVUI_DATA_SET(self->hbox.wd.id, "_scroll_id", dvui_Id, self->scroll.wd.id);
        dvui_scroll_container_widget_deinit(&self->scroll);
    }

    if (self->has_hbar_grab) dvui_scroll_bar_grab_draw(self->hbar_grab);

    dvui_box_widget_deinit(&self->vbox);

    if (self->has_vbar_grab) dvui_scroll_bar_grab_draw(self->vbar_grab);

    dvui_data_set(nullptr, self->hbox.wd.id, "_scroll_info", self->si, sizeof(dvui_ScrollInfo));

    // hbox je vnořený (není alokovaný), uvolnění řešíme za ním
    const bool allocated = dvui_widget_is_allocated(self);
    dvui_box_widget_deinit(&self->hbox);
    if (allocated) dvui_widget_free(self);
}

dvui_ScrollAreaWidget* dvui_scroll_area_impl(dvui_Src src, dvui_Props props) {
    dvui_ScrollAreaWidget* ret = dvui_widget_alloc(sizeof *ret);
    dvui_scroll_area_widget_init(ret, src, props);
    return ret;
}
