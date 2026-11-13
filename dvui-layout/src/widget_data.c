// WidgetData – společná data a chování všech widgetů.
// DVUI: src/WidgetData.zig
#include "internal.h"

#include <stdlib.h>
#include <string.h>

// DVUI: WidgetData.init
dvui_WidgetData dvui_widget_data_init(dvui_Src src, dvui_WidgetDataInitOptions init_options, dvui_Options opts) {
    const dvui_Widget parent = dvui_parent_get();
    const dvui_Id id = init_options.id_override ? init_options.id_override : dvui_widget_extend_id(parent, src, dvui_options_id_extra(&opts));

    const dvui_Options options = opts;

    const dvui_Size min_size = dvui_options_min_size_get(&options);
    dvui_Size ms = min_size;
    dvui_Size min_last_frame;
    if (dvui_min_size_get(id, &min_last_frame)) {
        // Bereme maximum ze zadané a minulé min. velikosti. ScrollArea může
        // dostat např. {w = 0, h = 200}, tj. šířku převezme z minula.
        ms = dvui_size_max(ms, min_last_frame);
    }

    const dvui_Expand expand = dvui_options_expand_get(&options);
    dvui_Rect rect;
    if (options.rect.has) {
        const dvui_Rect r = options.rect.v;
        const dvui_Rect parent_content = dvui_widget_data_content_rect(dvui_widget_data(parent));
        rect = dvui_rect_to_size(r, (dvui_Size){
                                        .w = dvui_expand_is_horizontal(expand) ? parent_content.w : (r.w == 0 ? ms.w : r.w),
                                        .h = dvui_expand_is_vertical(expand) ? parent_content.h : (r.h == 0 ? ms.h : r.h),
                                    });
    } else {
        rect = dvui_widget_rect_for(parent, id, ms, expand, dvui_options_gravity_get(&options));
    }

    return (dvui_WidgetData){
        .id = id,
        .parent = parent,
        .init_options = init_options,
        .min_size = min_size,
        .rect = rect,
        .options = options,
        .src = src,
    };
}

// DVUI: WidgetData.rectSet
void dvui_widget_data_rect_set(dvui_WidgetData *wd, dvui_Rect r) {
    wd->rect = r;
    wd->rect_scale = dvui_widget_data_rect_scale_from_parent(wd);
    wd->has_rect_scale = true;
}

// DVUI: WidgetData.register
void dvui_widget_data_register(dvui_WidgetData *wd) {
    dvui_widget_data_rect_set(wd, wd->rect);

    // U běžných widgetů stačí tady; subwindows musí volat
    // captureMouseMaintain až po subwindowCurrentSet a subwindowAdd.
    if (!wd->init_options.subwindow) {
        dvui_capture_mouse_maintain((dvui_CaptureMouse){
            .id = wd->id,
            .rect = dvui_widget_data_border_rect_scale(wd).r,
            .subwindow_id = dvui_subwindow_current_id(),
        });
    }

    dvui_Window *cw = dvui_current_window();
    cw->last_registered_id_this_frame = wd->id;

    const dvui_Id focused_widget_id = dvui_focused_widget_id();
    if (wd->id == focused_widget_id) {
        cw->last_focused_id_this_frame = wd->id;
        if (cw->scroll_to_focused) {
            cw->scroll_to_focused = false;
            if (!wd->init_options.no_scroll_when_focused) {
                dvui_scroll_to((dvui_ScrollToOptions){.screen_rect = dvui_widget_data_rect_scale(wd).r});
            }
        }
    }

    if (wd->id == dvui_focused_widget_id_in_current_subwindow()) {
        cw->last_focused_id_in_subwindow = wd->id;
    }

    // duplicitní id: min. velikost už byla tento snímek uložena
    dvui_MapEntry *e = dvui_map_get(&cw->min_sizes, wd->id);
    if (e && e->used) {
        DVUI_LOG_ERR("%s:%d duplicate widget id %llx (widget \"%s\"); you may need to pass .id_extra = <loop index> as widget options",
                     wd->src.file, wd->src.line, (unsigned long long)wd->id, wd->options.name ? wd->options.name : "???");
    }
}

// DVUI: WidgetData.visible
bool dvui_widget_data_visible(const dvui_WidgetData *wd) { return dvui_rect_visible(dvui_widget_data_border_rect_scale(wd).r); }

// DVUI: WidgetData.borderAndBackground (bez stínu, ninepatch a zaoblení)
void dvui_widget_data_border_and_background(const dvui_WidgetData *wd, dvui_BorderAndBackgroundOptions opts) {
    if (!dvui_widget_data_visible(wd)) return;

    bool bg = dvui_options_background_get(&wd->options);
    const dvui_Rect b = dvui_options_border_get(&wd->options);

    if (dvui_rect_non_zero(b)) {
        const bool uniform = b.x == b.y && b.x == b.w && b.x == b.h;
        if (uniform) {
            // stejnoměrný okraj kreslíme jako tah po obvodu
            const dvui_Rect r = dvui_rect_inset(dvui_widget_data_border_rect(wd), dvui_rect_scale(b, 0.5f));
            const dvui_RectScale rs = dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_rect_scale(wd), dvui_rect_offset_neg(r, wd->rect));
            dvui_render_stroke_rect(rs.r, b.x * rs.s, dvui_options_color(&wd->options, DVUI_COLOR_BORDER), false);
        } else {
            // nestejnoměrný okraj: celý border rect barvou okraje, pozadí přes něj
            if (!bg) bg = true;
            const dvui_RectScale rs = dvui_widget_data_border_rect_scale(wd);
            if (!dvui_rect_empty(rs.r)) {
                dvui_render_fill_rect(rs.r, dvui_options_color(&wd->options, DVUI_COLOR_BORDER), false);
            }
        }
    }

    if (bg) {
        const dvui_RectScale rs = dvui_widget_data_background_rect_scale(wd);
        if (!dvui_rect_empty(rs.r)) {
            const dvui_Color fill = opts.fill_color.has ? opts.fill_color.v : dvui_options_color(&wd->options, DVUI_COLOR_FILL);
            dvui_render_fill_rect(rs.r, fill, false);
        }
    }
}

// DVUI: WidgetData.focusBorder
void dvui_widget_data_focus_border(const dvui_WidgetData *wd) {
    if (dvui_widget_data_visible(wd)) {
        const dvui_RectScale rs = dvui_widget_data_border_rect_scale(wd);
        const float thick = 2 * rs.s;
        dvui_render_stroke_rect(rs.r, thick, dvui_theme_get()->focus, true);
    }
}

dvui_RectScale dvui_widget_data_rect_scale_from_parent(const dvui_WidgetData *wd) {
    if (wd->init_options.subwindow) return dvui_rect_scale_rect_to_rect_scale(dvui_window_rect_scale(), wd->rect);
    return dvui_widget_screen_rect_scale(wd->parent, wd->rect);
}

dvui_RectScale dvui_widget_data_rect_scale(const dvui_WidgetData *wd) {
    if (wd->has_rect_scale) return wd->rect_scale;
    // widget se ptá před register (např. kvůli přetahování)
    return dvui_widget_data_rect_scale_from_parent(wd);
}

dvui_Rect dvui_widget_data_border_rect(const dvui_WidgetData *wd) {
    return dvui_rect_inset(wd->rect, dvui_options_margin_get(&wd->options));
}

dvui_RectScale dvui_widget_data_border_rect_scale(const dvui_WidgetData *wd) {
    const dvui_Rect r = dvui_rect_offset_neg(dvui_widget_data_border_rect(wd), wd->rect);
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_rect_scale(wd), r);
}

dvui_Rect dvui_widget_data_background_rect(const dvui_WidgetData *wd) {
    return dvui_rect_inset(dvui_rect_inset(wd->rect, dvui_options_margin_get(&wd->options)), dvui_options_border_get(&wd->options));
}

dvui_RectScale dvui_widget_data_background_rect_scale(const dvui_WidgetData *wd) {
    const dvui_Rect r = dvui_rect_offset_neg(dvui_widget_data_background_rect(wd), wd->rect);
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_rect_scale(wd), r);
}

dvui_Rect dvui_widget_data_content_rect(const dvui_WidgetData *wd) {
    return dvui_rect_inset(dvui_widget_data_background_rect(wd), dvui_options_padding_get(&wd->options));
}

dvui_RectScale dvui_widget_data_content_rect_scale(const dvui_WidgetData *wd) {
    const dvui_Rect r = dvui_rect_offset_neg(dvui_widget_data_content_rect(wd), wd->rect);
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_rect_scale(wd), r);
}

void dvui_widget_data_min_size_max(dvui_WidgetData *wd, dvui_Size s) { wd->min_size = dvui_size_max(wd->min_size, s); }

// DVUI: WidgetData.minSizeSetAndRefresh
void dvui_widget_data_min_size_set_and_refresh(dvui_WidgetData *wd) {
    wd->min_size = dvui_size_min(wd->min_size, dvui_options_max_size_get(&wd->options));

    dvui_Size ms;
    if (dvui_min_size_get(wd->id, &ms)) {
        // Pokud jsme dostali přesně minulou min. velikost, byla omezující;
        // když se min. velikost změnila, může se změnit layout.
        if ((wd->rect.w == ms.w && ms.w != wd->min_size.w) || (wd->rect.h == ms.h && ms.h != wd->min_size.h)) {
            dvui_refresh(nullptr, DVUI_SRC, wd->id);
        }
    } else {
        // první snímek widgetu – téměř vždy je potřeba druhý snímek
        dvui_refresh(nullptr, DVUI_SRC, wd->id);
    }

    dvui_Window *cw = dvui_current_window();
    dvui_MapEntry *e = dvui_map_get_or_put(&cw->min_sizes, wd->id);
    if (!e->ptr) {
        e->ptr = malloc(sizeof(dvui_Size));
        e->size = sizeof(dvui_Size);
    }
    memcpy(e->ptr, &wd->min_size, sizeof(dvui_Size));
    e->used = true;
}

// DVUI: WidgetData.minSizeReportToParent
void dvui_widget_data_min_size_report_to_parent(const dvui_WidgetData *wd) {
    if (!wd->options.rect.has) dvui_widget_min_size_for_child(wd->parent, wd->min_size);
}

bool dvui_widget_data_is_root(const dvui_WidgetData *wd) {
    // okno je samo sobě rodičem
    return wd->id == dvui_widget_data(wd->parent)->id;
}
