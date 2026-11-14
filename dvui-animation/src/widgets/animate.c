// Animate – obal, který animuje průhlednost nebo velikost svého obsahu.
// DVUI: src/widgets/AnimateWidget.zig
#include "../internal.h"

static dvui_WidgetData* animate_data(void* ptr) { return &((dvui_AnimateWidget*)ptr)->wd; }

static dvui_Rect animate_rect_for(void* ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    return dvui_animate_widget_rect_for(ptr, id, min_size, e, g);
}

static dvui_RectScale animate_screen_rect_scale(void* ptr, dvui_Rect r) { return dvui_animate_widget_screen_rect_scale(ptr, r); }

static void animate_min_size_for_child(void* ptr, dvui_Size s) { dvui_animate_widget_min_size_for_child(ptr, s); }

static const dvui_WidgetVTable animate_vtable = {animate_data, animate_rect_for, animate_screen_rect_scale, animate_min_size_for_child};

// DVUI: AnimateWidget.widget
dvui_Widget dvui_animate_widget_widget(dvui_AnimateWidget* self) { return (dvui_Widget){self, &animate_vtable}; }
// DVUI: AnimateWidget.data
dvui_WidgetData* dvui_animate_widget_data(dvui_AnimateWidget* self) { return &self->wd; }

// DVUI: AnimateWidget.init
void dvui_animate_widget_init(dvui_AnimateWidget* self, dvui_Src src, dvui_Props props) {
    static const dvui_Prop defaults[] = {DVUI_NAME("Animate"), {0}};
    dvui_Options opts;
    dvui_WidgetProps wp;
    dvui_props_resolve("Animate", defaults, props, DVUI_TAGS(DVUI_PROP_kind, DVUI_PROP_duration, DVUI_PROP_easing), &opts, &wp);
    *self = (dvui_AnimateWidget){
        .wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, opts),
        .init_opts = {.kind = wp.kind, .duration = wp.duration, .easing = wp.easing},
        .prev_alpha = 1.0f,
    };

    if (self->init_opts.kind != DVUI_ANIMATE_NONE) {
        if (dvui_first_frame(self->wd.id)) {
            // spustíme úvodní animaci
            dvui_animate_widget_start(self);
        }

        dvui_Animation a;
        if (dvui_animation_get(self->wd.id, "_end", &a)) {
            self->has_val = true;
            self->val = dvui_animation_value(&a);
        } else if (dvui_animation_get(self->wd.id, "_start", &a)) {
            self->has_val = true;
            self->val = dvui_animation_value(&a);
        }
    }

    if (self->has_val) {
        const float v = self->val;
        dvui_WidgetData* wd = &self->wd;
        dvui_Size ms;
        switch (self->init_opts.kind) {
        case DVUI_ANIMATE_NONE: break;
        case DVUI_ANIMATE_ALPHA:
            self->prev_alpha = dvui_alpha(v);
            break;
        case DVUI_ANIMATE_VERTICAL:
            if (dvui_min_size_get(wd->id, &ms)) {
                if (wd->rect.h > ms.h + 0.001f) {
                    // jsme větší než min. velikost (asi roztažení) – s rezervou
                    // na nepřesnost float
                    const float h = wd->rect.h;
                    wd->rect.h *= DVUI_MAX(v, 0.0f);
                    wd->rect.y += dvui_options_gravity_get(&wd->options).y * (h - wd->rect.h);
                }
            }
            break;
        case DVUI_ANIMATE_HORIZONTAL:
            if (dvui_min_size_get(wd->id, &ms)) {
                if (wd->rect.w > ms.w + 0.001f) {
                    // jsme větší než min. velikost (asi roztažení) – s rezervou
                    // na nepřesnost float
                    const float w = wd->rect.w;
                    wd->rect.w *= DVUI_MAX(v, 0.0f);
                    wd->rect.x += dvui_options_gravity_get(&wd->options).x * (w - wd->rect.w);
                }
            }
            break;
        }
    }

    dvui_parent_set(dvui_animate_widget_widget(self));
    dvui_widget_data_register(&self->wd);
    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){0});
}

// DVUI: AnimateWidget.start
void dvui_animate_widget_start(dvui_AnimateWidget* self) {
    dvui_animation(self->wd.id, "_start",
                   (dvui_Animation){
                       .start_val = 0.0f,
                       .end_val = 1.0f,
                       .end_time = self->init_opts.duration,
                       .easing = self->init_opts.easing ? self->init_opts.easing : dvui_easing_linear,
                   });
}

// DVUI: AnimateWidget.startEnd
void dvui_animate_widget_start_end(dvui_AnimateWidget* self) {
    dvui_animation(self->wd.id, "_end",
                   (dvui_Animation){
                       .start_val = 1.0f,
                       .end_val = 0.0f,
                       .end_time = self->init_opts.duration,
                       .easing = self->init_opts.easing ? self->init_opts.easing : dvui_easing_linear,
                   });
}

// DVUI: AnimateWidget.end
bool dvui_animate_widget_end(dvui_AnimateWidget* self) {
    dvui_Animation a;
    if (dvui_animation_get(self->wd.id, "_end", &a)) {
        return dvui_animation_done(&a);
    }

    return false;
}

// DVUI: AnimateWidget.rectFor
dvui_Rect dvui_animate_widget_rect_for(dvui_AnimateWidget* self, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    return dvui_basic_layout_rect_for(&self->layout, dvui_rect_just_size(dvui_widget_data_content_rect(&self->wd)), id, min_size, e, g);
}

// DVUI: AnimateWidget.screenRectScale
dvui_RectScale dvui_animate_widget_screen_rect_scale(dvui_AnimateWidget* self, dvui_Rect rect) {
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_content_rect_scale(&self->wd), rect);
}

// DVUI: AnimateWidget.minSizeForChild
void dvui_animate_widget_min_size_for_child(dvui_AnimateWidget* self, dvui_Size s) {
    const dvui_Size ms = dvui_basic_layout_min_size_for_child(&self->layout, s);
    dvui_widget_data_min_size_max(&self->wd, dvui_options_pad_size(&self->wd.options, ms));
}

// DVUI: AnimateWidget.deinit
void dvui_animate_widget_deinit(dvui_AnimateWidget* self) {
    if (self->has_val) {
        const float v = self->val;
        switch (self->init_opts.kind) {
        case DVUI_ANIMATE_NONE: break;
        case DVUI_ANIMATE_ALPHA:
            dvui_alpha_set(self->prev_alpha);
            break;
        case DVUI_ANIMATE_VERTICAL:
            // záporná výška by rozbila layout
            self->wd.min_size.h *= DVUI_MAX(v, 0.0f);
            break;
        case DVUI_ANIMATE_HORIZONTAL:
            // záporná šířka by rozbila layout
            self->wd.min_size.w *= DVUI_MAX(v, 0.0f);
            break;
        }
    }

    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    dvui_widget_data_min_size_report_to_parent(&self->wd);
    dvui_parent_reset(self->wd.id, self->wd.parent);
    // DVUI: defer widgetFree (proběhne až na konci)
    if (dvui_widget_is_allocated(self)) dvui_widget_free(self);
}

// DVUI: dvui.zig – animate
dvui_AnimateWidget* dvui_animate_impl(dvui_Src src, dvui_Props props) {
    dvui_AnimateWidget* ret = dvui_widget_alloc(sizeof *ret);
    dvui_animate_widget_init(ret, src, props);
    return ret;
}
