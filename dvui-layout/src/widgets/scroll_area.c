// Oblast se scrollováním: hbox [ vbox [ ScrollContainer, hbar ], vbar ].
// DVUI: src/widgets/ScrollAreaWidget.zig
#include "../internal.h"

dvui_Options dvui_scroll_area_widget_defaults = {
    .name = "ScrollAreaWidget",
    .background = {.has = true, .v = true},
    .style = {.has = true, .v = DVUI_STYLE_WINDOW},
};

// DVUI: ScrollAreaWidget.init
void dvui_scroll_area_widget_init(dvui_ScrollAreaWidget *self, dvui_Src src, dvui_ScrollAreaInitOptions init_opts, dvui_Options opts) {
    *self = (dvui_ScrollAreaWidget){0};

    dvui_box_widget_init(&self->hbox, src, (dvui_BoxInitOptions){.dir = DVUI_HORIZONTAL}, dvui_options_override(dvui_scroll_area_widget_defaults, opts));
    const dvui_Id hbox_id = self->hbox.wd.id;

    if (init_opts.scroll_info) {
        self->si = init_opts.scroll_info;
    } else {
        const dvui_ScrollInfo def = dvui_scroll_info_default();
        self->si = dvui_data_get_ptr_default(nullptr, hbox_id, "_scroll_info", sizeof(dvui_ScrollInfo), &def);
        // kód mimo widget mohl změnit směr scrollování
        self->si->vertical = init_opts.vertical.has ? init_opts.vertical.v : DVUI_SCROLL_AUTO;
        self->si->horizontal = init_opts.horizontal.has ? init_opts.horizontal.v : DVUI_SCROLL_NONE;
    }

    if (init_opts.frame_viewport_out) *init_opts.frame_viewport_out = dvui_rect_top_left(self->si->viewport);

    dvui_box_widget_draw_background(&self->hbox);

    dvui_Id focus_target = init_opts.focus_id;
    if (!focus_target) {
        const dvui_Id *sid = DVUI_DATA_GET(hbox_id, "_scroll_id", dvui_Id);
        if (sid) focus_target = *sid;
    }

    const dvui_Rect crect = dvui_widget_data_content_rect(&self->hbox.wd);

    // nejdřív předpokládáme, že ScrollContainer dostane všechno místo
    self->si->viewport.w = crect.w;
    self->si->viewport.h = crect.h;

    // a upravíme podle odsazení z minulého snímku
    const dvui_Size *inset = DVUI_DATA_GET(hbox_id, "_linsets", dvui_Size);
    if (inset) {
        self->si->viewport.w -= inset->w;
        self->si->viewport.h -= inset->h;
    }

    const float bar_w = dvui_options_min_size_get(&dvui_scroll_bar_widget_defaults).w;
    const float bar_h = dvui_options_min_size_get(&dvui_scroll_bar_widget_defaults).h;

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
        dvui_scroll_bar_widget_init(&vbar, DVUI_SRC, (dvui_ScrollBarInitOptions){.scroll_info = self->si, .focus_id = focus_target},
                                    dvui_options_override(dvui_options_strip(self->hbox.wd.options),
                                                          (dvui_Options){.gravity_x = DVUI_SOME(overlay ? 0.999999f : 1.0f), .expand = DVUI_SOME(DVUI_EXPAND_VERTICAL)}));
        if (overlay) {
            self->has_vbar_grab = true;
            self->vbar_grab = dvui_scroll_bar_widget_grab(&vbar);
        } else {
            dvui_scroll_bar_grab_draw(dvui_scroll_bar_widget_grab(&vbar));
        }
        dvui_scroll_bar_widget_deinit(&vbar);
    }

    dvui_box_widget_init(&self->vbox, DVUI_SRC, (dvui_BoxInitOptions){.dir = DVUI_VERTICAL},
                         dvui_options_override(dvui_options_strip(self->hbox.wd.options),
                                               (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_BOTH), .name = "ScrollAreaWidget vbox"}));
    dvui_box_widget_draw_background(&self->vbox);

    if (do_hbar) {
        const bool overlay = init_opts.horizontal_bar == DVUI_SCROLL_BAR_AUTO_OVERLAY;
        dvui_ScrollBarWidget hbar;
        dvui_scroll_bar_widget_init(&hbar, DVUI_SRC, (dvui_ScrollBarInitOptions){.direction = DVUI_HORIZONTAL, .scroll_info = self->si, .focus_id = focus_target},
                                    dvui_options_override(dvui_options_strip(self->hbox.wd.options),
                                                          (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_HORIZONTAL), .gravity_y = DVUI_SOME(overlay ? 0.999999f : 1.0f)}));
        if (overlay) {
            self->has_hbar_grab = true;
            self->hbar_grab = dvui_scroll_bar_widget_grab(&hbar);
        } else {
            dvui_scroll_bar_grab_draw(dvui_scroll_bar_widget_grab(&hbar));
        }
        dvui_scroll_bar_widget_deinit(&hbar);
    }

    if (!init_opts.no_container) {
        const dvui_Options container_opts = dvui_options_override(dvui_options_strip(self->hbox.wd.options), (dvui_Options){.expand = DVUI_SOME(DVUI_EXPAND_BOTH)});
        self->has_scroll = true;
        dvui_scroll_container_widget_init(&self->scroll, DVUI_SRC, self->si,
                                          (dvui_ScrollContainerInitOptions){
                                              .scroll_area = self,
                                              .lock_visible = init_opts.lock_visible,
                                              .user_scroll = init_opts.user_scroll,
                                              .frame_viewport = init_opts.frame_viewport,
                                              .no_process_events_after = init_opts.no_process_events_after,
                                          },
                                          container_opts);
        dvui_scroll_container_widget_process_events(&self->scroll);
    }
}

dvui_WidgetData *dvui_scroll_area_widget_data(dvui_ScrollAreaWidget *self) { return &self->hbox.wd; }

// ukládá se jen levé/horní odsazení – posuvníky jsou vpravo a dole
void dvui_scroll_area_widget_set_container_rect(dvui_ScrollAreaWidget *self, dvui_Rect rect) {
    DVUI_DATA_SET(self->hbox.wd.id, "_linsets", dvui_Size, ((dvui_Size){rect.x, rect.y}));
}

// DVUI: ScrollAreaWidget.deinit
void dvui_scroll_area_widget_deinit(dvui_ScrollAreaWidget *self) {
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

dvui_ScrollAreaWidget *dvui_scroll_area(dvui_Src src, dvui_ScrollAreaInitOptions init_opts, dvui_Options opts) {
    dvui_ScrollAreaWidget *ret = dvui_widget_alloc(sizeof *ret);
    dvui_scroll_area_widget_init(ret, src, init_opts, opts);
    return ret;
}
