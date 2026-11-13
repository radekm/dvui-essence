// Tlačítko (bez textu).
// DVUI: src/widgets/ButtonWidget.zig, dvui.zig – button
#include "../internal.h"

dvui_Options dvui_button_widget_defaults = {
    .name = "Button",
    .margin = {.has = true, .v = {4, 4, 4, 4}},
    .padding = {.has = true, .v = {6, 6, 6, 6}},
    .background = {.has = true, .v = true},
    .style = {.has = true, .v = DVUI_STYLE_CONTROL},
};

static dvui_WidgetData *button_data(void *ptr) { return &((dvui_ButtonWidget *)ptr)->wd; }

// DVUI: ButtonWidget.rectFor
static dvui_Rect button_rect_for(void *ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    (void)id;
    dvui_ButtonWidget *self = ptr;
    return dvui_place_in(dvui_rect_just_size(dvui_widget_data_content_rect(&self->wd)), min_size, e, g);
}

static dvui_RectScale button_screen_rect_scale(void *ptr, dvui_Rect r) {
    dvui_ButtonWidget *self = ptr;
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_content_rect_scale(&self->wd), r);
}

static void button_min_size_for_child(void *ptr, dvui_Size s) {
    dvui_ButtonWidget *self = ptr;
    dvui_widget_data_min_size_max(&self->wd, dvui_options_pad_size(&self->wd.options, s));
}

static const dvui_WidgetVTable button_vtable = {button_data, button_rect_for, button_screen_rect_scale, button_min_size_for_child};

dvui_Widget dvui_button_widget_widget(dvui_ButtonWidget *self) { return (dvui_Widget){self, &button_vtable}; }
dvui_WidgetData *dvui_button_widget_data(dvui_ButtonWidget *self) { return &self->wd; }

// DVUI: ButtonWidget.init
void dvui_button_widget_init(dvui_ButtonWidget *self, dvui_Src src, dvui_ButtonInitOptions init_options, dvui_Options opts) {
    dvui_Options options = dvui_options_override(dvui_button_widget_defaults, opts);
    if (init_options.grayed) {
        options.color_text = (dvui_OptColor)DVUI_SOME(dvui_color_average(dvui_options_color(&options, DVUI_COLOR_TEXT), dvui_options_color(&options, DVUI_COLOR_FILL)));
    }
    *self = (dvui_ButtonWidget){
        .wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, options),
        .init_options = init_options,
    };
    dvui_widget_data_register(&self->wd);
    dvui_parent_set(dvui_button_widget_widget(self));
}

// DVUI: ButtonWidget.processEvents
void dvui_button_widget_process_events(dvui_ButtonWidget *self) {
    self->click = dvui_clicked(&self->wd, (dvui_ClickOptions){.hovered = &self->hover});
}

// DVUI: ButtonWidget.style – barvy pro hover a stisk
dvui_Options dvui_button_widget_style(dvui_ButtonWidget *self) {
    const dvui_Options *o = &self->wd.options;
    dvui_Options opts = dvui_options_style_only(*o);
    const float t = dvui_hover_fade(self->wd.id, self->hover);
    if (dvui_captured(self->wd.id)) {
        opts.color_fill = (dvui_OptColor)DVUI_SOME(dvui_options_color(o, DVUI_COLOR_FILL_PRESS));
        opts.color_text = (dvui_OptColor)DVUI_SOME(dvui_options_color(o, DVUI_COLOR_TEXT_PRESS));
    } else if (t > 0) {
        opts.color_fill = (dvui_OptColor)DVUI_SOME(dvui_color_lerp(dvui_options_color(o, DVUI_COLOR_FILL), dvui_options_color(o, DVUI_COLOR_FILL_HOVER), t));
        opts.color_text = (dvui_OptColor)DVUI_SOME(dvui_color_lerp(dvui_options_color(o, DVUI_COLOR_TEXT), dvui_options_color(o, DVUI_COLOR_TEXT_HOVER), t));
    }
    return opts;
}

// DVUI: ButtonWidget.drawBackground
void dvui_button_widget_draw_background(dvui_ButtonWidget *self) {
    const dvui_Options style_opt = dvui_button_widget_style(self);
    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){.fill_color = style_opt.color_fill});
}

void dvui_button_widget_draw_focus(dvui_ButtonWidget *self) {
    if (!self->init_options.no_draw_focus && dvui_button_widget_focused(self)) dvui_widget_data_focus_border(&self->wd);
}

bool dvui_button_widget_focused(dvui_ButtonWidget *self) { return self->wd.id == dvui_focused_widget_id(); }
bool dvui_button_widget_hovered(dvui_ButtonWidget *self) { return self->hover; }
bool dvui_button_widget_pressed(dvui_ButtonWidget *self) { return dvui_captured(self->wd.id); }
bool dvui_button_widget_clicked(dvui_ButtonWidget *self) { return self->click; }

// DVUI: ButtonWidget.deinit
void dvui_button_widget_deinit(dvui_ButtonWidget *self) {
    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    dvui_widget_data_min_size_report_to_parent(&self->wd);
    dvui_parent_reset(self->wd.id, self->wd.parent);
    if (dvui_widget_is_allocated(self)) dvui_widget_free(self);
}

// DVUI: dvui.button – místo LabelWidget je zástupce popisku.
bool dvui_button(dvui_Src src, dvui_Size label_size, dvui_ButtonInitOptions init_opts, dvui_Options opts) {
    // init: dostaneme obdélník od rodiče a staneme se novým rodičem
    dvui_ButtonWidget bw;
    dvui_button_widget_init(&bw, src, init_opts, opts);

    // události (myš a klávesnice)
    dvui_button_widget_process_events(&bw);

    // pozadí a okraj
    dvui_button_widget_draw_background(&bw);

    const bool click = dvui_button_widget_clicked(&bw);

    // dítě (popisek): dostane obdélník od tlačítka a nahlásí mu min. velikost
    const dvui_Options label_opts = dvui_options_override(dvui_options_override(dvui_options_strip(opts), dvui_button_widget_style(&bw)),
                                                          (dvui_Options){.gravity_x = DVUI_SOME(0.5f), .gravity_y = DVUI_SOME(0.5f)});
    dvui_label_placeholder(DVUI_SRC, label_size, label_opts);

    dvui_button_widget_draw_focus(&bw);

    // obnoví rodiče a pošle mu naši min. velikost
    dvui_button_widget_deinit(&bw);
    return click;
}
