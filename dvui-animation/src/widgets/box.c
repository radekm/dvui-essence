// Box – skládá děti vodorovně nebo svisle.
// DVUI: src/widgets/BoxWidget.zig
#include "../internal.h"

static dvui_WidgetData* box_data(void* ptr) { return &((dvui_BoxWidget*)ptr)->wd; }

static dvui_Rect box_rect_for(void* ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    return dvui_box_widget_rect_for(ptr, id, min_size, e, g);
}

static dvui_RectScale box_screen_rect_scale(void* ptr, dvui_Rect r) { return dvui_box_widget_screen_rect_scale(ptr, r); }

static void box_min_size_for_child(void* ptr, dvui_Size s) { dvui_box_widget_min_size_for_child(ptr, s); }

static const dvui_WidgetVTable box_vtable = {box_data, box_rect_for, box_screen_rect_scale, box_min_size_for_child};

dvui_Widget dvui_box_widget_widget(dvui_BoxWidget* self) { return (dvui_Widget){self, &box_vtable}; }
dvui_WidgetData* dvui_box_widget_data(dvui_BoxWidget* self) { return &self->wd; }

// DVUI: BoxWidget.init
void dvui_box_widget_init(dvui_BoxWidget* self, dvui_Src src, dvui_Props props) {
    static const dvui_Prop defaults[] = {DVUI_NAME("Box"), {0}};
    dvui_Options opts;
    dvui_WidgetProps wp;
    dvui_props_resolve("Box", defaults, props, DVUI_TAGS(DVUI_PROP_dir, DVUI_PROP_equal_space, DVUI_PROP_num_packed_expanded),
                       &opts, &wp);
    const dvui_BoxInitOptions init_options = {
        .dir = wp.dir,
        .equal_space = wp.equal_space,
        .has_num_packed_expanded = wp.has[DVUI_PROP_num_packed_expanded],
        .num_packed_expanded = wp.num_packed_expanded,
    };
    const dvui_WidgetData wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, opts);
    *self = (dvui_BoxWidget){
        .wd = wd,
        .init_opts = init_options,
        .first_child = true,
    };
    const dvui_BoxData* dp = DVUI_DATA_GET(wd.id, "_data", dvui_BoxData);
    if (dp) {
        self->has_data_prev = true;
        self->data_prev = *dp;
    }

    dvui_widget_data_register(&self->wd);
    dvui_parent_set(dvui_box_widget_widget(self));
}

void dvui_box_widget_draw_background(dvui_BoxWidget* self) {
    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){0});
}

static void remove_space(dvui_BoxWidget* self, dvui_Rect r, dvui_Gravity g) {
    if (self->init_opts.dir == DVUI_HORIZONTAL) {
        self->child_rect.w = DVUI_MAX(0.0f, self->child_rect.w - r.w);
        if (g.x <= 0.5f) self->child_rect.x += r.w;
    } else {
        self->child_rect.h = DVUI_MAX(0.0f, self->child_rect.h - r.h);
        if (g.y <= 0.5f) self->child_rect.y += r.h;
    }
}

// DVUI: BoxWidget.rectFor
dvui_Rect dvui_box_widget_rect_for(dvui_BoxWidget* self, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    const dvui_Direction dir = self->init_opts.dir;
    if (self->first_child) {
        self->first_child = false;

        // Líně, aby fungovalo i data().rectSet() po init.
        // Obdélník pro děti začíná v 0,0.
        self->child_rect = dvui_rect_just_size(dvui_widget_data_content_rect(&self->wd));

        if (self->has_data_prev) {
            const dvui_BoxData dp = self->data_prev;
            if (self->init_opts.equal_space) {
                if (dp.packed_children > 0) {
                    self->pixels_per_w = (dir == DVUI_HORIZONTAL ? self->child_rect.w : self->child_rect.h) / dp.packed_children;
                }
            } else {
                float packed_weight = dp.total_weight;
                if (self->init_opts.has_num_packed_expanded) packed_weight = (float)self->init_opts.num_packed_expanded;
                if (packed_weight > 0) {
                    const float avail = dir == DVUI_HORIZONTAL ? self->child_rect.w : self->child_rect.h;
                    self->pixels_per_w = DVUI_MAX(0.0f, avail - dp.min_space_taken) / packed_weight;
                }
            }
        }
    }

    if (self->child_id != 0) {
        DVUI_LOG_ERR("BoxWidget: rectFor called before previous widget called minSizeForChild");
    }
    self->child_id = id;

    self->child_positioned = dir == DVUI_HORIZONTAL ? (g.x > 0 && g.x < 1.0f) : (g.y > 0 && g.y < 1.0f);

    if (self->child_positioned) {
        // nebalíme, chová se jako overlay – dáme ho, kam chce
        return dvui_place_in(dvui_rect_just_size(dvui_widget_data_content_rect(&self->wd)), min_size, e, g);
    }

    self->packed_children += 1;
    float current_weight = 0.0f;
    if ((dir == DVUI_HORIZONTAL && dvui_expand_is_horizontal(e)) || (dir == DVUI_VERTICAL && dvui_expand_is_vertical(e))) {
        current_weight = 1.0f;
    }
    self->total_weight += current_weight;

    const dvui_Rect available = self->child_rect;

    // úprava min. velikosti pro expand ratio (je vynucený)
    dvui_Size ms = min_size;
    self->ratio_extra = 0;
    if (e == DVUI_EXPAND_RATIO && ms.w != 0 && ms.h != 0) {
        if (dir == DVUI_HORIZONTAL) {
            const float ratio = ms.w / ms.h;
            ms.h = available.h;
            ms.w = available.h * ratio;
            self->ratio_extra = ms.w - min_size.w;
        } else {
            const float ratio = ms.h / ms.w;
            ms.h = available.w * ratio;
            ms.w = available.w;
            self->ratio_extra = ms.h - min_size.h;
        }
    }

    // min. velikost po úpravě poměru
    const dvui_Size child_min_size = ms;

    dvui_Rect ret;
    if (self->init_opts.equal_space) {
        // dítě umístíme do místa, které mu přidělíme
        if (dir == DVUI_HORIZONTAL) {
            ms.w = self->pixels_per_w;
            ms.h = available.h;
        } else {
            ms.h = self->pixels_per_w;
            ms.w = available.w;
        }
        const dvui_Rect avail = dvui_place_in(available, ms, DVUI_EXPAND_NONE, g);
        remove_space(self, avail, g);
        ret = dvui_place_in(avail, child_min_size, e, g);
    } else {
        // úprava min. velikosti pro běžné roztažení (dostaneš poměrný díl
        // volného místa); roztažení v druhém směru zůstává
        dvui_Expand ee = DVUI_EXPAND_NONE;
        if (!self->has_data_prev && current_weight > 0) {
            // první snímek: první roztažené dítě dostane všechno místo
            ee = e;
        } else if (dir == DVUI_HORIZONTAL) {
            ms.w += self->pixels_per_w * current_weight;
            if (dvui_expand_is_vertical(e)) ee = DVUI_EXPAND_VERTICAL;
        } else {
            ms.h += self->pixels_per_w * current_weight;
            if (dvui_expand_is_horizontal(e)) ee = DVUI_EXPAND_HORIZONTAL;
        }
        ret = dvui_place_in(available, ms, ee, g);
        remove_space(self, ret, g);
    }

    if (dir == DVUI_HORIZONTAL) {
        if (ret.w + 0.001f < child_min_size.w) self->ran_off = true;
    } else {
        if (ret.h + 0.001f < child_min_size.h) self->ran_off = true;
    }

    return ret;
}

dvui_RectScale dvui_box_widget_screen_rect_scale(dvui_BoxWidget* self, dvui_Rect rect) {
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_content_rect_scale(&self->wd), rect);
}

// DVUI: BoxWidget.minSizeForChild
void dvui_box_widget_min_size_for_child(dvui_BoxWidget* self, dvui_Size s) {
    self->child_id = 0;

    if (self->child_positioned) {
        dvui_widget_data_min_size_max(&self->wd, dvui_options_pad_size(&self->wd.options, s));
        return;
    }

    if (self->init_opts.dir == DVUI_HORIZONTAL) {
        self->max_space = DVUI_MAX(self->max_space, s.w + self->ratio_extra);
        self->min_space_taken += s.w + self->ratio_extra;
        self->max_thick = DVUI_MAX(self->max_thick, s.h);
    } else {
        self->max_space = DVUI_MAX(self->max_space, s.h + self->ratio_extra);
        self->min_space_taken += s.h + self->ratio_extra;
        self->max_thick = DVUI_MAX(self->max_thick, s.w);
    }
}

// DVUI: BoxWidget.deinit
void dvui_box_widget_deinit(dvui_BoxWidget* self) {
    const bool horizontal = self->init_opts.dir == DVUI_HORIZONTAL;
    const bool extra_space = (horizontal ? self->child_rect.w : self->child_rect.h) > 0.001f;
    const float space = self->init_opts.equal_space ? self->max_space * self->packed_children : self->min_space_taken;
    const dvui_Size ms = horizontal ? (dvui_Size){space, self->max_thick} : (dvui_Size){self->max_thick, space};

    if ((self->init_opts.equal_space && self->packed_children > 0) || self->total_weight > 0) {
        if (extra_space) {
            // Máme roztažené děti, ale nevyužili jsme všechno místo (dítě
            // zmizelo, přestalo být roztažené, zmenšila se min. velikost…).
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        }
        if (!self->init_opts.equal_space && self->pixels_per_w > 0 && self->ran_off) {
            // Mysleli jsme, že je volné místo, ale přetekli jsme.
            dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
        }
        // Při total_weight == 0 jsme těsně kolem dětí a změny min. velikosti
        // řeší minSizeSetAndRefresh().
    }

    dvui_widget_data_min_size_max(&self->wd, dvui_options_pad_size(&self->wd.options, ms));
    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    dvui_widget_data_min_size_report_to_parent(&self->wd);

    const dvui_BoxData d = {.packed_children = self->packed_children, .total_weight = self->total_weight, .min_space_taken = self->min_space_taken};
    dvui_data_set(nullptr, self->wd.id, "_data", &d, sizeof d);

    dvui_parent_reset(self->wd.id, self->wd.parent);
    if (dvui_widget_is_allocated(self)) dvui_widget_free(self);
}

dvui_BoxWidget* dvui_box_impl(dvui_Src src, dvui_Props props) {
    dvui_BoxWidget* ret = dvui_widget_alloc(sizeof *ret);
    dvui_box_widget_init(ret, src, props);
    dvui_box_widget_draw_background(ret);
    return ret;
}
