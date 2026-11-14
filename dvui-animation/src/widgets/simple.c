// Jednoduché widgety: spacer, separator, zástupce popisku, overlay.
// DVUI: dvui.zig (spacer, separator), widgets/LabelWidget.zig,
//       widgets/OverlayWidget.zig, widgets/ScaleWidget.zig
#include "../internal.h"

// DVUI: dvui.spacer
dvui_WidgetData dvui_spacer_impl(dvui_Src src, dvui_Props props) {
    static const dvui_Prop defaults[] = {DVUI_NAME("Spacer"), {0}};
    dvui_Options opts;
    dvui_props_resolve("Spacer", defaults, props, nullptr, &opts, nullptr);
    dvui_WidgetData wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, opts);
    dvui_widget_data_register(&wd);
    dvui_widget_data_border_and_background(&wd, (dvui_BorderAndBackgroundOptions){0});
    dvui_widget_data_min_size_set_and_refresh(&wd);
    dvui_widget_data_min_size_report_to_parent(&wd);
    return wd;
}

// DVUI: dvui.separator
dvui_WidgetData dvui_separator_impl(dvui_Src src, dvui_Props props) {
    // barva závisí na tématu, proto výchozí hodnoty nejsou statické
    const dvui_Prop defaults[] = {
        DVUI_NAME("Separator"),
        DVUI_BACKGROUND(true),
        DVUI_FILL(dvui_theme_get()->border),
        DVUI_MIN_SIZE(1, 1),
        {0},
    };
    dvui_Options opts;
    dvui_props_resolve("Separator", defaults, props, nullptr, &opts, nullptr);
    dvui_WidgetData wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, opts);
    dvui_widget_data_register(&wd);
    dvui_widget_data_border_and_background(&wd, (dvui_BorderAndBackgroundOptions){0});
    dvui_widget_data_min_size_set_and_refresh(&wd);
    dvui_widget_data_min_size_report_to_parent(&wd);
    return wd;
}

// DVUI: LabelWidget.initNoFmtAllocator + deinit, bez textu.
dvui_WidgetData dvui_label_placeholder_impl(dvui_Src src, dvui_Props props) {
    static const dvui_Prop defaults[] = {DVUI_NAME("Label"), DVUI_PADDING_ALL(6), {0}};
    dvui_Options options;
    dvui_WidgetProps wp;
    dvui_props_resolve("Label", defaults, props, DVUI_TAGS(DVUI_PROP_label_size), &options, &wp);
    const dvui_Size size = dvui_size_max(wp.label_size, options.min_size_content);
    dvui_WidgetData wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0},
                                               dvui_options_with(options, DVUI_P(min_size_content, size)));
    dvui_widget_data_register(&wd);
    dvui_widget_data_border_and_background(&wd, (dvui_BorderAndBackgroundOptions){0});
    dvui_widget_data_min_size_set_and_refresh(&wd);
    dvui_widget_data_min_size_report_to_parent(&wd);
    return wd;
}

// ---------------------------------------------------------------------------
// Overlay (DVUI: widgets/OverlayWidget.zig) – děti se kreslí přes sebe.
// ---------------------------------------------------------------------------

static dvui_WidgetData* overlay_data(void* ptr) { return &((dvui_OverlayWidget*)ptr)->wd; }

static dvui_Rect overlay_rect_for(void* ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    (void)id;
    dvui_OverlayWidget* self = ptr;
    return dvui_place_in(dvui_rect_just_size(dvui_widget_data_content_rect(&self->wd)), min_size, e, g);
}

static dvui_RectScale overlay_screen_rect_scale(void* ptr, dvui_Rect r) {
    dvui_OverlayWidget* self = ptr;
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_content_rect_scale(&self->wd), r);
}

static void overlay_min_size_for_child(void* ptr, dvui_Size s) {
    dvui_OverlayWidget* self = ptr;
    dvui_widget_data_min_size_max(&self->wd, dvui_options_pad_size(&self->wd.options, s));
}

static const dvui_WidgetVTable overlay_vtable = {overlay_data, overlay_rect_for, overlay_screen_rect_scale, overlay_min_size_for_child};

dvui_Widget dvui_overlay_widget_widget(dvui_OverlayWidget* self) { return (dvui_Widget){self, &overlay_vtable}; }

void dvui_overlay_widget_init(dvui_OverlayWidget* self, dvui_Src src, dvui_Props props) {
    static const dvui_Prop defaults[] = {DVUI_NAME("Overlay"), {0}};
    dvui_Options opts;
    dvui_props_resolve("Overlay", defaults, props, nullptr, &opts, nullptr);
    *self = (dvui_OverlayWidget){.wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, opts)};
    dvui_widget_data_register(&self->wd);
    dvui_parent_set(dvui_overlay_widget_widget(self));
}

void dvui_overlay_widget_draw_background(dvui_OverlayWidget* self) {
    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){0});
}

void dvui_overlay_widget_deinit(dvui_OverlayWidget* self) {
    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    dvui_widget_data_min_size_report_to_parent(&self->wd);
    dvui_parent_reset(self->wd.id, self->wd.parent);
    if (dvui_widget_is_allocated(self)) dvui_widget_free(self);
}

dvui_OverlayWidget* dvui_overlay_impl(dvui_Src src, dvui_Props props) {
    dvui_OverlayWidget* ret = dvui_widget_alloc(sizeof *ret);
    dvui_overlay_widget_init(ret, src, props);
    dvui_overlay_widget_draw_background(ret);
    return ret;
}

// ---------------------------------------------------------------------------
// Scale (DVUI: widgets/ScaleWidget.zig) – zvětšuje/zmenšuje obsah.
// ---------------------------------------------------------------------------
