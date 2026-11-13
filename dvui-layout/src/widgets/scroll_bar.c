// Posuvník.
// DVUI: src/widgets/ScrollBarWidget.zig
#include "../internal.h"

dvui_Options dvui_scroll_bar_widget_defaults = {
    .name = "ScrollBar",
    .min_size_content = {.has = true, .v = {10, 10}},
};

static void process_events(dvui_ScrollBarWidget *self, dvui_RectPhysical grabrs) {
    const dvui_RectScale rs = dvui_widget_data_border_rect_scale(&self->wd);
    size_t n;
    dvui_Event *evts = dvui_events(&n);
    for (size_t i = 0; i < n; i++) {
        dvui_Event *e = &evts[i];
        if (!dvui_event_match_simple(e, &self->wd)) continue;
        if (e->kind != DVUI_EVENT_MOUSE) continue;
        const dvui_EventMouse me = e->evt.mouse;
        const bool vertical = self->dir == DVUI_VERTICAL;

        switch (me.action) {
        case DVUI_MOUSE_FOCUS:
            if (self->focus_id) {
                dvui_event_handle(e, &self->wd);
                dvui_focus_widget(self->focus_id, 0, &e->num);
            }
            break;
        case DVUI_MOUSE_PRESS:
            if (dvui_button_pointer(me.button)) {
                dvui_event_handle(e, &self->wd);
                if (dvui_rect_contains(grabrs, me.p)) {
                    // capture a začátek tažení
                    dvui_capture_mouse(&self->wd, e->num);
                    dvui_DragStartOptions o = {.cursor = DVUI_CURSOR_ARROW, .has_cursor = true};
                    if (vertical) {
                        o.offset.y = me.p.y - (grabrs.y + grabrs.h / 2);
                    } else {
                        o.offset.x = me.p.x - (grabrs.x + grabrs.w / 2);
                    }
                    dvui_drag_pre_start(me.button, me.p, o);
                } else {
                    if (vertical ? (me.p.y < grabrs.y) : (me.p.x < grabrs.x)) {
                        dvui_scroll_info_scroll_page_up(self->si, self->dir); // kliknutí nad úchyt
                    } else {
                        dvui_scroll_info_scroll_page_down(self->si, self->dir); // pod úchyt
                    }
                    dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
                }
            }
            break;
        case DVUI_MOUSE_RELEASE:
            if (dvui_button_pointer(me.button)) {
                dvui_event_handle(e, &self->wd);
                // konec tažení a capture
                dvui_capture_mouse(nullptr, e->num);
                dvui_drag_end(me.button);
            }
            break;
        case DVUI_MOUSE_MOTION:
            if (dvui_captured(self->wd.id)) {
                dvui_event_handle(e, &self->wd);
                if (dvui_dragging(me, nullptr, nullptr)) {
                    const float min = vertical ? rs.r.y + grabrs.h / 2 : rs.r.x + grabrs.w / 2;
                    const float max = vertical ? rs.r.y + rs.r.h - grabrs.h / 2 : rs.r.x + rs.r.w - grabrs.w / 2;
                    const float grabmid = vertical ? me.p.y - dvui_drag_offset(me.button).y : me.p.x - dvui_drag_offset(me.button).x;
                    float f = 0;
                    if (max > min) f = (grabmid - min) / (max - min);
                    dvui_scroll_info_scroll_to_fraction(self->si, self->dir, f);
                    dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
                }
            }
            break;
        case DVUI_MOUSE_POSITION:
            dvui_cursor_set(DVUI_CURSOR_ARROW);
            self->highlight = true;
            break;
        case DVUI_MOUSE_WHEEL_X:
            if (!vertical) {
                dvui_event_handle(e, &self->wd);
                dvui_scroll_info_scroll_by_offset(self->si, self->dir, -me.wheel);
                dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
            }
            break;
        case DVUI_MOUSE_WHEEL_Y:
            // směr nevadí, „normální“ kolečko na vodorovném posuvníku je přirozené
            dvui_event_handle(e, &self->wd);
            dvui_scroll_info_scroll_by_offset(self->si, self->dir, -me.wheel);
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
            break;
        }
    }
}

// DVUI: ScrollBarWidget.init
void dvui_scroll_bar_widget_init(dvui_ScrollBarWidget *self, dvui_Src src, dvui_ScrollBarInitOptions init_opts, dvui_Options opts) {
    *self = (dvui_ScrollBarWidget){
        .si = init_opts.scroll_info,
        .focus_id = init_opts.focus_id,
        .dir = init_opts.direction,
        .wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, dvui_options_override(dvui_scroll_bar_widget_defaults, opts)),
    };

    dvui_widget_data_register(&self->wd);
    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){0});

    const dvui_Rect content = dvui_widget_data_content_rect(&self->wd);
    self->grab_rect = content;
    if (self->dir == DVUI_VERTICAL) {
        self->grab_rect.h = DVUI_MIN(self->grab_rect.h, DVUI_MAX(20.0f, self->grab_rect.h * dvui_scroll_info_visible_fraction(self->si, self->dir)));
        const float inside_h = content.h - self->grab_rect.h;
        self->grab_rect.y += inside_h * dvui_scroll_info_offset_fraction(self->si, self->dir);
    } else {
        self->grab_rect.w = DVUI_MIN(self->grab_rect.w, DVUI_MAX(20.0f, self->grab_rect.w * dvui_scroll_info_visible_fraction(self->si, self->dir)));
        const float inside_w = content.w - self->grab_rect.w;
        self->grab_rect.x += inside_w * dvui_scroll_info_offset_fraction(self->si, self->dir);
    }

    const dvui_RectScale grabrs = dvui_widget_screen_rect_scale(self->wd.parent, self->grab_rect);
    process_events(self, grabrs.r);
}

// DVUI: ScrollBarWidget.Grab.draw (zaoblený úchyt je zde obdélník)
void dvui_scroll_bar_grab_draw(dvui_ScrollBarGrab g) { dvui_render_fill_rect(g.rect, g.color, false); }

// DVUI: ScrollBarWidget.grab
dvui_ScrollBarGrab dvui_scroll_bar_widget_grab(dvui_ScrollBarWidget *self) {
    const dvui_Color text = dvui_options_color(&self->wd.options, DVUI_COLOR_TEXT);
    const float hover_t = dvui_hover_fade(self->wd.id, self->highlight);
    const dvui_Color fill = dvui_captured(self->wd.id) ? dvui_color_opacity(text, 0.3f)
                                                       : dvui_color_lerp(dvui_color_opacity(text, 0.5f), dvui_color_opacity(text, 0.3f), hover_t);
    return (dvui_ScrollBarGrab){
        .rect = dvui_widget_screen_rect_scale(self->wd.parent, dvui_rect_inset_all(self->grab_rect, 2)).r,
        .color = fill,
    };
}

void dvui_scroll_bar_widget_deinit(dvui_ScrollBarWidget *self) {
    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    dvui_widget_data_min_size_report_to_parent(&self->wd);
}
