// Jednoduché widgety: spacer, separator, zástupce popisku, overlay, scale.
// DVUI: dvui.zig (spacer, separator), widgets/LabelWidget.zig,
//       widgets/OverlayWidget.zig, widgets/ScaleWidget.zig
#include "../internal.h"

// DVUI: dvui.spacer
dvui_WidgetData dvui_spacer(dvui_Src src, dvui_Options opts) {
    const dvui_Options defaults = {.name = "Spacer"};
    dvui_WidgetData wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, dvui_options_override(defaults, opts));
    dvui_widget_data_register(&wd);
    dvui_widget_data_border_and_background(&wd, (dvui_BorderAndBackgroundOptions){0});
    dvui_widget_data_min_size_set_and_refresh(&wd);
    dvui_widget_data_min_size_report_to_parent(&wd);
    return wd;
}

// DVUI: dvui.separator
dvui_WidgetData dvui_separator(dvui_Src src, dvui_Options opts) {
    const dvui_Options defaults = {
        .name = "Separator",
        .background = DVUI_SOME(true),
        .color_fill = DVUI_SOME(dvui_theme_get()->border),
        .min_size_content = DVUI_SOME((dvui_Size){1, 1}),
    };
    dvui_WidgetData wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, dvui_options_override(defaults, opts));
    dvui_widget_data_register(&wd);
    dvui_widget_data_border_and_background(&wd, (dvui_BorderAndBackgroundOptions){0});
    dvui_widget_data_min_size_set_and_refresh(&wd);
    dvui_widget_data_min_size_report_to_parent(&wd);
    return wd;
}

// DVUI: LabelWidget.initNoFmtAllocator + deinit, bez textu.
dvui_WidgetData dvui_label_placeholder(dvui_Src src, dvui_Size text_size, dvui_Options opts) {
    const dvui_Options defaults = {.name = "Label", .padding = DVUI_SOME(dvui_rect_all(6))};
    const dvui_Options options = dvui_options_override(defaults, opts);
    const dvui_Size size = dvui_size_max(text_size, dvui_options_min_size_content_get(&options));
    dvui_WidgetData wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0},
                                               dvui_options_override(options, (dvui_Options){.min_size_content = DVUI_SOME(size)}));
    dvui_widget_data_register(&wd);
    dvui_widget_data_border_and_background(&wd, (dvui_BorderAndBackgroundOptions){0});
    dvui_widget_data_min_size_set_and_refresh(&wd);
    dvui_widget_data_min_size_report_to_parent(&wd);
    return wd;
}

// ---------------------------------------------------------------------------
// Overlay (DVUI: widgets/OverlayWidget.zig) – děti se kreslí přes sebe.
// ---------------------------------------------------------------------------

static dvui_WidgetData *overlay_data(void *ptr) { return &((dvui_OverlayWidget *)ptr)->wd; }

static dvui_Rect overlay_rect_for(void *ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    (void)id;
    dvui_OverlayWidget *self = ptr;
    return dvui_place_in(dvui_rect_just_size(dvui_widget_data_content_rect(&self->wd)), min_size, e, g);
}

static dvui_RectScale overlay_screen_rect_scale(void *ptr, dvui_Rect r) {
    dvui_OverlayWidget *self = ptr;
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_content_rect_scale(&self->wd), r);
}

static void overlay_min_size_for_child(void *ptr, dvui_Size s) {
    dvui_OverlayWidget *self = ptr;
    dvui_widget_data_min_size_max(&self->wd, dvui_options_pad_size(&self->wd.options, s));
}

static const dvui_WidgetVTable overlay_vtable = {overlay_data, overlay_rect_for, overlay_screen_rect_scale, overlay_min_size_for_child};

dvui_Widget dvui_overlay_widget_widget(dvui_OverlayWidget *self) { return (dvui_Widget){self, &overlay_vtable}; }

void dvui_overlay_widget_init(dvui_OverlayWidget *self, dvui_Src src, dvui_Options opts) {
    const dvui_Options defaults = {.name = "Overlay"};
    *self = (dvui_OverlayWidget){.wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, dvui_options_override(defaults, opts))};
    dvui_widget_data_register(&self->wd);
    dvui_parent_set(dvui_overlay_widget_widget(self));
}

void dvui_overlay_widget_draw_background(dvui_OverlayWidget *self) {
    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){0});
}

void dvui_overlay_widget_deinit(dvui_OverlayWidget *self) {
    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    dvui_widget_data_min_size_report_to_parent(&self->wd);
    dvui_parent_reset(self->wd.id, self->wd.parent);
    if (dvui_widget_is_allocated(self)) dvui_widget_free(self);
}

dvui_OverlayWidget *dvui_overlay(dvui_Src src, dvui_Options opts) {
    dvui_OverlayWidget *ret = dvui_widget_alloc(sizeof *ret);
    dvui_overlay_widget_init(ret, src, opts);
    dvui_overlay_widget_draw_background(ret);
    return ret;
}

// ---------------------------------------------------------------------------
// Scale (DVUI: widgets/ScaleWidget.zig) – zvětšuje/zmenšuje obsah.
// ---------------------------------------------------------------------------

static dvui_WidgetData *scale_data(void *ptr) { return &((dvui_ScaleWidget *)ptr)->wd; }

static dvui_Rect scale_rect_for(void *ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    dvui_ScaleWidget *self = ptr;
    // ochrana proti dělení nulou
    const float s = *self->scale > 0 ? 1.0f / *self->scale : 1000000.0f;
    return dvui_basic_layout_rect_for(&self->layout, dvui_rect_scale(dvui_rect_just_size(dvui_widget_data_content_rect(&self->wd)), s), id, min_size, e, g);
}

static dvui_RectScale scale_screen_rect_scale(void *ptr, dvui_Rect r) {
    dvui_ScaleWidget *self = ptr;
    dvui_RectScale rs = dvui_widget_data_content_rect_scale(&self->wd);
    rs.s *= *self->scale;
    return dvui_rect_scale_rect_to_rect_scale(rs, r);
}

static void scale_min_size_for_child(void *ptr, dvui_Size s) {
    dvui_ScaleWidget *self = ptr;
    const dvui_Size ms = dvui_basic_layout_min_size_for_child(&self->layout, dvui_size_scale(s, *self->scale));
    dvui_widget_data_min_size_max(&self->wd, dvui_options_pad_size(&self->wd.options, ms));
}

static const dvui_WidgetVTable scale_vtable = {scale_data, scale_rect_for, scale_screen_rect_scale, scale_min_size_for_child};

dvui_Widget dvui_scale_widget_widget(dvui_ScaleWidget *self) { return (dvui_Widget){self, &scale_vtable}; }

void dvui_scale_widget_init(dvui_ScaleWidget *self, dvui_Src src, float *scale, dvui_Options opts) {
    const dvui_Options defaults = {.name = "Scale"};
    *self = (dvui_ScaleWidget){.wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, dvui_options_override(defaults, opts))};
    self->scale = scale ? scale : DVUI_DATA_GET_PTR_DEFAULT(self->wd.id, "_scale", float, 1.0f);
    dvui_parent_set(dvui_scale_widget_widget(self));
    dvui_widget_data_register(&self->wd);
    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){0});
}

void dvui_scale_widget_deinit(dvui_ScaleWidget *self) {
    DVUI_DATA_SET(self->wd.id, "_scale", float, *self->scale);
    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    dvui_widget_data_min_size_report_to_parent(&self->wd);
    dvui_parent_reset(self->wd.id, self->wd.parent);
    if (dvui_widget_is_allocated(self)) dvui_widget_free(self);
}
