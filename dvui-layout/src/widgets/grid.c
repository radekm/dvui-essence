// Mřížka (tabulka) s vícepruchodovým layoutem: šířky sloupců a výšky řádků
// se zjišťují z min. velikostí buněk během několika snímků (auto-size).
// DVUI: src/widgets/GridWidget.zig
#include "../internal.h"

#include <stdint.h>
#include <string.h>

dvui_Options dvui_grid_widget_defaults = {
    .name = "Grid",
    .style = {.has = true, .v = DVUI_STYLE_CONTENT},
    .background = {.has = true, .v = true},
    .border = {.has = true, .v = {1, 1, 1, 1}},
};

// hlavička sloupce má jako řádek maximum size_t (DVUI: maxInt(usize))
#define HEADER_ROW SIZE_MAX

// DVUI: GridWidget.RowOffset.gap – po kolika pixelech si cacheujeme offset řádku
static const float row_offset_gap = 1000;

// ---------------------------------------------------------------------------
// vtable
// ---------------------------------------------------------------------------

static dvui_WidgetData* grid_data(void* ptr) { return &((dvui_GridWidget*)ptr)->wd; }

static dvui_Rect grid_rect_for(void* ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    (void)id;
    dvui_GridWidget* self = ptr;
    return dvui_place_in(dvui_rect_just_size(dvui_widget_data_content_rect(&self->wd)), min_size, e, g);
}

static dvui_RectScale grid_screen_rect_scale(void* ptr, dvui_Rect r) {
    dvui_GridWidget* self = ptr;
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_content_rect_scale(&self->wd), r);
}

static void grid_min_size_for_child(void* ptr, dvui_Size s) {
    dvui_GridWidget* self = ptr;
    dvui_widget_data_min_size_max(&self->wd, dvui_options_pad_size(&self->wd.options, s));
}

static const dvui_WidgetVTable grid_vtable = {grid_data, grid_rect_for, grid_screen_rect_scale, grid_min_size_for_child};

dvui_Widget dvui_grid_widget_widget(dvui_GridWidget* self) { return (dvui_Widget){self, &grid_vtable}; }
dvui_WidgetData* dvui_grid_widget_data(dvui_GridWidget* self) { return &self->wd; }

// ---------------------------------------------------------------------------
// Pomocné funkce pro šířky a výšky (DVUI: colWeight, colWidth, …)
// ---------------------------------------------------------------------------

static float col_weight(dvui_GridWidget* self, size_t col) {
    if (col >= self->cols) return 0;
    if (self->shrink && (col >= self->col_resizable_len || !self->col_resizable[col])) return 0;
    if (self->shrink || (col < self->col_expands_len && self->col_expands[col])) {
        const float w = self->col_widths[col];
        return w <= DVUI_GRID_COL_MIN_WIDTH ? 0 : w;
    }
    return 0;
}

static bool col_expanded(dvui_GridWidget* self, size_t col) { return col < self->col_expands_len && self->col_expands[col]; }

float dvui_grid_widget_col_width(dvui_GridWidget* self, size_t col) {
    if (col < self->cols) return DVUI_MAX(DVUI_GRID_COL_MIN_WIDTH, self->col_widths[col] + self->col_expand * col_weight(self, col));
    return 100;
}

float dvui_grid_widget_col_offset(dvui_GridWidget* self, size_t col) {
    float x = 0;
    for (size_t i = 0; i < col; i++) x += dvui_grid_widget_col_width(self, i);
    return x;
}

float dvui_grid_widget_row_height(dvui_GridWidget* self, size_t row) {
    // binární hledání v seřazených výškách
    size_t lo = 0, hi = self->row_heights_len;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (self->row_heights[mid].row == row) return self->row_heights[mid].height;
        if (self->row_heights[mid].row < row) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return self->row_height_default->has ? self->row_height_default->v : self->default_min.h;
}

static void row_offsets_insert(dvui_GridWidget* self, size_t at, dvui_GridRowOffset ro) {
    dvui_GridRowOffsetList* l = &self->row_offsets;
    l->items = dvui_frame_grow(l->items, &l->cap, sizeof(dvui_GridRowOffset), l->len + 1);
    memmove(&l->items[at + 1], &l->items[at], (l->len - at) * sizeof(dvui_GridRowOffset));
    l->items[at] = ro;
    l->len++;
}

// DVUI: GridWidget.rowOffsetInternal
static float row_offset_internal(dvui_GridWidget* self, size_t row, bool add_row_offsets) {
    // první RowOffset s větším řádkem (partitionPoint)
    size_t idx = 0;
    while (idx < self->row_offsets.len && self->row_offsets.items[idx].row <= row) idx++;
    size_t r = 0;
    float ry = 0;
    if (idx > 0) {
        r = self->row_offsets.items[idx - 1].row;
        ry = self->row_offsets.items[idx - 1].offset;
    }
    float last = ry;
    while (r < row) {
        ry += dvui_grid_widget_row_height(self, r);
        r += 1;
        // na existující RowOffset nenarazíme – jinak bychom začali u něj
        if (add_row_offsets && ry > last + row_offset_gap) {
            last = ry;
            row_offsets_insert(self, idx, (dvui_GridRowOffset){.row = r, .offset = ry});
            idx += 1;
        }
    }
    return ry;
}

float dvui_grid_widget_row_offset(dvui_GridWidget* self, size_t row) { return row_offset_internal(self, row, true); }

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

// Načte pole uložené v datech; když nesedí délka, vytvoří nové s výchozí
// hodnotou a zkopíruje staré (DVUI: dataSetSliceCopies + memcpy).
static void* slice_resize(dvui_Id id, const char* key, size_t elem_size, size_t want_len, const void* def, size_t* out_len) {
    size_t bytes;
    void* old = dvui_data_get_slice(nullptr, id, key, &bytes);
    const size_t old_len = old ? bytes / elem_size : 0;
    if (old && old_len == want_len) {
        *out_len = old_len;
        return old;
    }
    uint8_t* tmp = dvui_frame_alloc(want_len * elem_size + 1);
    for (size_t i = 0; i < want_len; i++) memcpy(tmp + i * elem_size, def, elem_size);
    const size_t keep = DVUI_MIN(old_len, want_len);
    if (keep) memcpy(tmp, old, keep * elem_size);
    dvui_data_set(nullptr, id, key, tmp, want_len * elem_size);
    *out_len = want_len;
    return dvui_data_get_slice(nullptr, id, key, &bytes);
}

// DVUI: GridWidget.init
void dvui_grid_widget_init(dvui_GridWidget* self, dvui_Src src, dvui_GridInitOptions init_opts, dvui_Options opts) {
    const dvui_Options options = dvui_options_override(dvui_grid_widget_defaults, opts);
    // DVUI: font.sizeM(4, 1) + padding TextLayoutWidget (6 na každé straně)
    const dvui_Size default_m = dvui_size_pad(dvui_font_size_m(4, 1), dvui_rect_all(6));

    *self = (dvui_GridWidget){
        .wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){.no_scroll_when_focused = true}, options),
        .layout_only = init_opts.layout_only,
        .default_min = default_m,
        .max_seen_col = -1,
        .max_seen_row = -1,
        .csi = {.horizontal = DVUI_SCROLL_AUTO, .vertical = DVUI_SCROLL_NONE},
    };
    const dvui_Id id = self->wd.id;
    self->row_height_default = DVUI_DATA_GET_PTR_DEFAULT(id, "__row_height_default", dvui_OptF32, (dvui_OptF32){0});
    self->col_header_height = DVUI_DATA_GET_PTR_DEFAULT(id, "__col_header_height", float, default_m.h);
    self->mouse_mode = DVUI_DATA_GET_PTR_DEFAULT(id, "__mouse_mode", bool, false);

    size_t n;
    dvui_Event* evts = dvui_events(&n);
    for (size_t i = 0; i < n; i++) {
        const dvui_Event* e = &evts[i];
        if (e->kind == DVUI_EVENT_KEY && e->evt.key.action == DVUI_KEY_ACTION_DOWN) {
            *self->mouse_mode = false;
        } else if (e->kind == DVUI_EVENT_MOUSE && e->evt.mouse.action != DVUI_MOUSE_POSITION) {
            *self->mouse_mode = true;
        }
    }

    dvui_widget_data_register(&self->wd);
    dvui_parent_set(dvui_grid_widget_widget(self));
    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){0});

    const dvui_GridAutoSize* as = DVUI_DATA_GET(id, "__auto_size", dvui_GridAutoSize);
    if (as) {
        self->has_auto_size = true;
        self->auto_size = *as;
        dvui_data_remove(nullptr, id, "__auto_size");
    }

    const size_t* cols = DVUI_DATA_GET(id, "__cols", size_t);
    self->cols = cols ? *cols : 0;
    const size_t* rows = DVUI_DATA_GET(id, "__rows", size_t);
    self->rows = init_opts.rows.has ? init_opts.rows.v : (rows ? *rows : 0);
    self->rows_provided = init_opts.rows.has;

    const float def_width = 100.0f;
    self->col_widths = slice_resize(id, "__col_widths", sizeof(float), self->cols, &def_width, &self->col_widths_len);
    const bool def_resizable = true;
    self->col_resizable = slice_resize(id, "__col_resizable", sizeof(bool), self->cols, &def_resizable, &self->col_resizable_len);

    size_t bytes;
    self->col_expands = dvui_data_get_slice(nullptr, id, "__col_expands", &bytes);
    self->col_expands_len = self->col_expands ? bytes / sizeof(bool) : 0;
    self->col_expands_new = dvui_frame_alloc(self->cols * sizeof(bool) + 1);
    memset(self->col_expands_new, 0, self->cols * sizeof(bool));

    self->row_heights = dvui_data_get_slice(nullptr, id, "__row_heights", &bytes);
    self->row_heights_len = self->row_heights ? bytes / sizeof(dvui_GridRowHeight) : 0;

    if (self->layout_only || dvui_first_frame(id)) dvui_grid_widget_auto_size(self, DVUI_GRID_AUTO_SIZE_BOTH);

    const dvui_ScrollInfo* stored_csi = DVUI_DATA_GET(id, "__csi", dvui_ScrollInfo);
    if (stored_csi) self->csi = *stored_csi;

    dvui_ScrollAreaInitOptions scroll_opts = init_opts.scroll_opts;
    if (!scroll_opts.frame_viewport_out) scroll_opts.frame_viewport_out = &self->frame_viewport;
    scroll_opts.no_container = true;
    dvui_scroll_area_widget_init(&self->scroll, DVUI_SRC, scroll_opts,
                                 (dvui_Options){.name = "GridScrollArea", .expand = DVUI_SOME(DVUI_EXPAND_BOTH), .background = DVUI_SOME(false)});
    self->msi = self->scroll.si;
    self->frame_viewport = *scroll_opts.frame_viewport_out; // jen pokud uživatel předal vlastní

    // roztažení nebo zmenšení sloupců vodorovně
    bool any_expanded = false;
    for (size_t i = 0; i < self->col_expands_len; i++) any_expanded |= self->col_expands[i];
    if ((any_expanded || self->msi->horizontal == DVUI_SCROLL_NONE) && self->cols > 0) {
        float total = 0;
        for (size_t i = 0; i < self->col_widths_len; i++) total += self->col_widths[i];

        // colWeight pak vrací váhu pro všechny sloupce, ne jen roztažené
        if (self->msi->viewport.w < total) self->shrink = true;

        float total_weight = 0;
        for (size_t col = 0; col < self->cols; col++) total_weight += col_weight(self, col);
        if (total_weight > 0) self->col_expand = (self->msi->viewport.w - total) / total_weight;

        // lze scrollovat vodorovně – nezmenšujeme
        if (self->msi->horizontal != DVUI_SCROLL_NONE) self->col_expand = DVUI_MAX(0.0f, self->col_expand);
        // nic není roztažené – jen zmenšujeme
        if (!any_expanded) self->col_expand = DVUI_MIN(0.0f, self->col_expand);
        // nikdy nezmenšíme na nic (chrání výpočet faktoru při změně šířky)
        self->col_expand = DVUI_MAX(-0.99f, self->col_expand);
    }
}

// Přizpůsobí sloupce/řádky obsahu; trvá několik snímků, než se vše ustálí.
void dvui_grid_widget_auto_size(dvui_GridWidget* self, dvui_GridAutoSize auto_size) {
    self->has_auto_size = true;
    self->auto_size = auto_size;
}

// DVUI: GridWidget.rowsVisible
void dvui_grid_widget_rows_visible(dvui_GridWidget* self, size_t* first, size_t* last) {
    if (!self->rows_provided) {
        DVUI_LOG_ERR("GridWidget: %llx rowsVisible() requires InitOptions.rows", (unsigned long long)self->wd.id);
        *first = 0;
        *last = self->rows;
        return;
    }
    if (self->msi->viewport.h == 0) {
        // první snímek: pustíme řádky, které asi uvidíme
        *first = 0;
        *last = DVUI_MIN((size_t)50, self->rows);
        return;
    }

    // rozšíření o řádek na každou stranu (kvůli klávesové navigaci)
    const size_t extra = 1;
    const float start_y = DVUI_MAX(0.0f, self->frame_viewport.y);
    const float end_y = self->frame_viewport.y + self->msi->viewport.h;
    size_t r = 0;
    float y = 0;
    float rh = dvui_grid_widget_row_height(self, r);
    // první řádek, jehož spodek je vidět
    while (r < self->rows && (y + rh) <= start_y) {
        r += 1;
        y += rh;
        rh = dvui_grid_widget_row_height(self, r);
    }
    for (size_t i = 0; i < extra; i++) {
        if (r == 0) continue;
        r -= 1;
        rh = dvui_grid_widget_row_height(self, r);
        y -= rh;
    }
    const size_t start = r;
    self->row_offsets = (dvui_GridRowOffsetList){0};
    if (y > row_offset_gap) row_offsets_insert(self, 0, (dvui_GridRowOffset){.row = r, .offset = y});

    // do prvního neviditelného řádku (konec je exkluzivní)
    while (r < self->rows && y < end_y) {
        r += 1;
        y += rh;
        rh = dvui_grid_widget_row_height(self, r);
    }
    *first = start;
    *last = DVUI_MIN(r + extra, self->rows);
}

// ---------------------------------------------------------------------------
// CellWidget (DVUI: GridWidget.CellWidget)
// ---------------------------------------------------------------------------

static dvui_WidgetData* cell_data(void* ptr) { return &((dvui_GridCellWidget*)ptr)->wd; }

static dvui_Rect cell_rect_for(void* ptr, dvui_Id id, dvui_Size min_size, dvui_Expand e, dvui_Gravity g) {
    dvui_GridCellWidget* self = ptr;
    return dvui_basic_layout_rect_for(&self->layout, dvui_rect_just_size(dvui_widget_data_content_rect(&self->wd)), id, min_size, e, g);
}

static dvui_RectScale cell_screen_rect_scale(void* ptr, dvui_Rect r) {
    dvui_GridCellWidget* self = ptr;
    return dvui_rect_scale_rect_to_rect_scale(dvui_widget_data_content_rect_scale(&self->wd), r);
}

static void cell_min_size_for_child(void* ptr, dvui_Size s) {
    dvui_GridCellWidget* self = ptr;
    const dvui_Size ms = dvui_basic_layout_min_size_for_child(&self->layout, s);
    dvui_widget_data_min_size_max(&self->wd, dvui_options_pad_size(&self->wd.options, ms));
}

static const dvui_WidgetVTable cell_vtable = {cell_data, cell_rect_for, cell_screen_rect_scale, cell_min_size_for_child};

dvui_Widget dvui_grid_cell_widget_widget(dvui_GridCellWidget* self) { return (dvui_Widget){self, &cell_vtable}; }
dvui_WidgetData* dvui_grid_cell_widget_data(dvui_GridCellWidget* self) { return &self->wd; }

static void cell_widget_init(dvui_GridCellWidget* self, dvui_Src src, dvui_GridWidget* grid, size_t col, size_t row, dvui_Options opts) {
    if (col < grid->cols && dvui_expand_is_horizontal(dvui_options_expand_get(&opts))) grid->col_expands_new[col] = true;

    // expand řídí sloupce/řádky, tady ho vypneme – rect je zadaný
    dvui_Options local_opts = opts;
    local_opts.expand = (dvui_OptExpand)DVUI_SOME(DVUI_EXPAND_NONE);

    const dvui_Options defs = {.name = "Cell"};
    *self = (dvui_GridCellWidget){
        .grid = grid,
        .col = col,
        .row = row,
        .wd = dvui_widget_data_init(src, (dvui_WidgetDataInitOptions){0}, dvui_options_override(defs, local_opts)),
    };
    dvui_parent_set(dvui_grid_cell_widget_widget(self));
    dvui_widget_data_register(&self->wd);
    dvui_widget_data_border_and_background(&self->wd, (dvui_BorderAndBackgroundOptions){0});
}

// DVUI: CellWidget.deinit – buňka nehlásí min. velikost rodiči, ale gridu.
void dvui_grid_cell_widget_deinit(dvui_GridCellWidget* self) {
    self->wd.min_size = dvui_size_min(self->wd.min_size, dvui_options_max_size_get(&self->wd.options));
    dvui_grid_widget_cell_min_size(self->grid, self->col, self->row, self->wd.min_size);
    dvui_parent_reset(self->wd.id, self->wd.parent);
}

// ---------------------------------------------------------------------------
// Hlavičky a buňky
// ---------------------------------------------------------------------------

static bool parent_is_scroll_vbox(dvui_GridWidget* self, const char* what) {
    if (dvui_widget_data(dvui_parent_get())->id != self->scroll.vbox.wd.id) {
        DVUI_LOG_ERR("grid %s is not direct child of grid scrollArea, did you try to wrap it in another widget?", what);
        return false;
    }
    return true;
}

// DVUI: GridWidget.colHeader
dvui_GridCellWidget* dvui_grid_widget_col_header(dvui_GridWidget* self, dvui_GridColHeaderInitOptions init_opts, dvui_Options opts) {
    if (!self->has_cscroll) {
        if (self->has_bscroll) {
            DVUI_LOG_ERR("GridWidget %llx colHeader called after cell", (unsigned long long)self->wd.id);
        } else {
            parent_is_scroll_vbox(self, "column header");
            self->has_cscroll = true;
            dvui_scroll_area_widget_init(&self->cscroll, DVUI_SRC,
                                         (dvui_ScrollAreaInitOptions){
                                             .horizontal_bar = DVUI_SCROLL_BAR_HIDE,
                                             .vertical_bar = DVUI_SCROLL_BAR_HIDE,
                                             .scroll_info = &self->csi,
                                             .frame_viewport = DVUI_SOME(((dvui_Point){.x = self->frame_viewport.x})),
                                             .no_process_events_after = true,
                                         },
                                         (dvui_Options){.name = "GridColumnHeaderScroll", .expand = DVUI_SOME(DVUI_EXPAND_HORIZONTAL)});
        }
    }

    if ((ptrdiff_t)init_opts.col > self->max_seen_col) self->max_seen_col = (ptrdiff_t)init_opts.col;

    // DVUI: hash("col", col, "header")
    const size_t id_extra = (size_t)dvui_id_update((dvui_Id)init_opts.col + 1, "col header");
    const dvui_Rect rect = {
        .x = dvui_grid_widget_col_offset(self, init_opts.col),
        .y = 0,
        .w = dvui_grid_widget_col_width(self, init_opts.col),
        .h = *self->col_header_height,
    };
    const dvui_Options defs = {.rect = DVUI_SOME(rect), .id_extra = DVUI_SOME(id_extra)};
    cell_widget_init(&self->cell_widget, DVUI_SRC, self, init_opts.col, HEADER_ROW, dvui_options_override(defs, opts));

    if (init_opts.col < self->col_resizable_len) self->col_resizable[init_opts.col] = !init_opts.not_resizable;

    if (!init_opts.not_resizable) {
        // změna šířky sloupce tažením za pravý okraj hlavičky
        dvui_RectScale rs = dvui_widget_data_rect_scale(&self->cell_widget.wd);
        rs.r.x = rs.r.x + rs.r.w - DVUI_GRID_COL_MIN_WIDTH * rs.s;
        rs.r.w = DVUI_GRID_COL_MIN_WIDTH * rs.s;
        dvui_WidgetData* wd = &self->cell_widget.wd;
        size_t n;
        dvui_Event* evts = dvui_events(&n);
        for (size_t i = 0; i < n; i++) {
            dvui_Event* e = &evts[i];
            if (!dvui_event_match(e, (dvui_EventMatchOptions){.id = wd->id, .r = rs.r})) continue;
            if (e->kind != DVUI_EVENT_MOUSE) continue;
            const dvui_EventMouse me = e->evt.mouse;
            if (me.action == DVUI_MOUSE_FOCUS) {
                dvui_event_handle(e, wd);
            } else if (me.action == DVUI_MOUSE_PRESS && dvui_button_pointer(me.button)) {
                dvui_event_handle(e, wd);
                dvui_capture_mouse(wd, e->num);
                dvui_drag_pre_start(me.button, me.p, (dvui_DragStartOptions){0});
            } else if (me.action == DVUI_MOUSE_RELEASE && dvui_button_pointer(me.button) && dvui_captured(wd->id)) {
                dvui_event_handle(e, wd);
                dvui_capture_mouse(nullptr, e->num);
                dvui_drag_end(me.button);
            } else if (me.action == DVUI_MOUSE_MOTION) {
                if (dvui_captured(wd->id)) {
                    dvui_event_handle(e, wd);
                    dvui_PointPhysical dp;
                    if (dvui_dragging(me, nullptr, &dp)) {
                        const float dx = dp.x / rs.s;
                        self->has_col_resize = true;
                        self->col_resize = init_opts.col;
                        self->col_resize_amount += dx;
                        dvui_refresh(nullptr, DVUI_SRC, wd->id);
                    }
                }
            } else if (me.action == DVUI_MOUSE_POSITION) {
                dvui_cursor_set(DVUI_CURSOR_ARROW_W_E);
            }
        }
    }
    return &self->cell_widget;
}

// DVUI: GridWidget.ensureBodyScroll – ukončí hlavičky a založí tělo.
static void ensure_body_scroll(dvui_GridWidget* self) {
    if (self->has_cscroll) {
        float tw = 0;
        for (size_t i = 0; i < self->col_widths_len; i++) tw += self->col_widths[i];
        const dvui_Size s = {tw, *self->col_header_height};
        dvui_scroll_container_widget_min_size_for_child(&self->cscroll.scroll, s);
        dvui_scroll_area_widget_deinit(&self->cscroll);
        self->has_cscroll = false;
    }

    if (!self->has_bscroll) {
        parent_is_scroll_vbox(self, "body scrollContainer");
        self->has_bscroll = true;
        dvui_scroll_container_widget_init(&self->bscroll, DVUI_SRC, self->msi,
                                          (dvui_ScrollContainerInitOptions){
                                              .scroll_area = &self->scroll,
                                              .frame_viewport = DVUI_SOME(self->frame_viewport),
                                              .event_rect = DVUI_SOME(dvui_widget_data_border_rect_scale(&self->scroll.hbox.wd).r),
                                          },
                                          (dvui_Options){.name = "GridBodyScroll", .expand = DVUI_SOME(DVUI_EXPAND_BOTH), .background = DVUI_SOME(false)});
        dvui_scroll_container_widget_process_events(&self->bscroll);
        // poslední fokus až tady, aby nezahrnoval hlavičky
        self->last_focus = dvui_last_focused_id_in_frame();
    }
}

// DVUI: GridWidget.cell
dvui_GridCellWidget* dvui_grid_widget_cell(dvui_GridWidget* self, dvui_GridCellOptions cell_opts, dvui_Options opts) {
    ensure_body_scroll(self);
    if ((ptrdiff_t)cell_opts.col > self->max_seen_col) self->max_seen_col = (ptrdiff_t)cell_opts.col;
    if ((ptrdiff_t)cell_opts.row > self->max_seen_row) self->max_seen_row = (ptrdiff_t)cell_opts.row;

    const dvui_Rect rect = {
        .x = dvui_grid_widget_col_offset(self, cell_opts.col),
        .y = dvui_grid_widget_row_offset(self, cell_opts.row),
        .w = dvui_grid_widget_col_width(self, cell_opts.col),
        .h = dvui_grid_widget_row_height(self, cell_opts.row),
    };

    if (cell_opts.col < self->col_resizable_len) self->col_resizable[cell_opts.col] = !cell_opts.not_col_resizable;

    const size_t half = sizeof(size_t) * 8 / 2;
    const size_t id_extra = (cell_opts.col << half) | cell_opts.row;
    const dvui_Options defs = {.rect = DVUI_SOME(rect), .id_extra = DVUI_SOME(id_extra)};
    cell_widget_init(&self->cell_widget, DVUI_SRC, self, cell_opts.col, cell_opts.row, dvui_options_override(defs, opts));
    return &self->cell_widget;
}

// DVUI: GridWidget.cellMinSize
void dvui_grid_widget_cell_min_size(dvui_GridWidget* self, size_t col, size_t row, dvui_Size min_size) {
    dvui_GridFloatList* cw = &self->col_widths_auto;
    while (col >= cw->len) {
        cw->items = dvui_frame_grow(cw->items, &cw->cap, sizeof(float), cw->len + 1);
        cw->items[cw->len++] = 10;
    }
    cw->items[col] = DVUI_MAX(DVUI_MAX(cw->items[col], DVUI_GRID_COL_MIN_WIDTH), min_size.w);

    if (row == HEADER_ROW) {
        self->col_header_height_auto = DVUI_MAX(self->col_header_height_auto, min_size.h);
        return;
    }

    const float h = DVUI_MAX(DVUI_GRID_ROW_MIN_HEIGHT, min_size.h);
    dvui_OptF32* def = self->row_height_default;
    if (def->has) {
        def->v = DVUI_MAX(DVUI_GRID_ROW_MIN_HEIGHT, DVUI_MIN(def->v, h));
    } else {
        *def = (dvui_OptF32){.has = true, .v = DVUI_MAX(DVUI_GRID_ROW_MIN_HEIGHT, h)};
    }

    dvui_GridRowHeightList* rh = &self->row_heights_auto;
    size_t pp = 0;
    while (pp < rh->len && rh->items[pp].row < row) pp++;
    if (pp == rh->len || rh->items[pp].row > row) {
        rh->items = dvui_frame_grow(rh->items, &rh->cap, sizeof(dvui_GridRowHeight), rh->len + 1);
        memmove(&rh->items[pp + 1], &rh->items[pp], (rh->len - pp) * sizeof(dvui_GridRowHeight));
        rh->items[pp] = (dvui_GridRowHeight){.row = row, .height = h};
        rh->len++;
    } else {
        rh->items[pp].height = DVUI_MAX(rh->items[pp].height, h);
    }
}

// DVUI: GridWidget.cellFromPoint
bool dvui_grid_widget_cell_from_point(dvui_GridWidget* self, dvui_PointPhysical p, dvui_GridCell* out) {
    ensure_body_scroll(self);
    const dvui_Point logical = dvui_scroll_container_widget_point_from_physical(&self->bscroll, p);
    if (logical.x < 0 || logical.y < 0) return false;

    size_t col = 0;
    float x = 0;
    while (x < logical.x) {
        x += dvui_grid_widget_col_width(self, col);
        col += 1;
    }

    size_t idx = 0;
    while (idx < self->row_offsets.len && self->row_offsets.items[idx].offset <= logical.y) idx++;
    size_t row = 0;
    float y = 0;
    if (idx > 0) {
        row = self->row_offsets.items[idx - 1].row;
        y = self->row_offsets.items[idx - 1].offset;
    }
    while (y < logical.y) {
        y += dvui_grid_widget_row_height(self, row);
        row += 1;
    }
    *out = (dvui_GridCell){.col = col ? col - 1 : 0, .row = row ? row - 1 : 0};
    return true;
}

// DVUI: GridWidget.cellHovered (jen režim myši)
bool dvui_grid_widget_cell_hovered(dvui_GridWidget* self, dvui_GridCell* out) {
    ensure_body_scroll(self);
    if (!*self->mouse_mode) return false;
    size_t n;
    dvui_Event* evts = dvui_events(&n);
    for (size_t i = 0; i < n; i++) {
        dvui_Event* e = &evts[i];
        if (!dvui_event_match_simple(e, &self->wd)) continue;
        if (e->kind == DVUI_EVENT_MOUSE && e->evt.mouse.action == DVUI_MOUSE_POSITION) return dvui_grid_widget_cell_from_point(self, e->evt.mouse.p, out);
    }
    return false;
}

// ---------------------------------------------------------------------------
// deinit
// ---------------------------------------------------------------------------

// DVUI: část GridWidget.deinit – ruční změna šířky sloupce
static void apply_col_resize(dvui_GridWidget* self) {
    const size_t col = self->col_resize;
    if (col >= self->cols) return;
    const float factor = 1 + self->col_expand;
    float resize_expanded = 0;
    if (self->col_resize_amount < 0) {
        // zmenšujeme tento sloupec (a případně sloupce vlevo)
        float resize = self->col_resize_amount;
        size_t col_left = col;
        for (;; col_left -= 1) {
            if (col_left < self->col_resizable_len && !self->col_resizable[col_left]) {
                if (col_left == 0) break;
                continue;
            }
            const bool weight = col_weight(self, col_left) > 0;
            float amt = resize;
            if (weight) amt /= factor;
            amt = DVUI_MAX(amt, DVUI_GRID_COL_MIN_WIDTH - self->col_widths[col_left]);
            self->col_widths[col_left] += amt;
            if (weight) amt *= factor;
            resize -= amt;
            if (!self->shrink && col_expanded(self, col_left)) resize_expanded += amt;
            if (col_left == 0 || resize > -0.01f) break;
        }
    } else {
        // zvětšujeme tento sloupec
        const bool weight = col_weight(self, col) > 0;
        self->col_widths[col] += weight ? self->col_resize_amount / factor : self->col_resize_amount;
        if (!self->shrink && col_expanded(self, col)) resize_expanded += self->col_resize_amount;
    }

    if (resize_expanded != 0) {
        // místo resize_expanded musíme vzít/dát ostatním roztaženým sloupcům
        size_t col_right = (col + 1) % self->cols;
        while (col_right != col) {
            if (col_expanded(self, col_right)) {
                const float old = self->col_widths[col_right];
                self->col_widths[col_right] = DVUI_MAX(DVUI_GRID_COL_MIN_WIDTH, self->col_widths[col_right] - resize_expanded / factor);
                resize_expanded -= (old - self->col_widths[col_right]) * factor;
                if (resize_expanded < 0.01f && resize_expanded > -0.01f) goto done;
            }
            col_right = (col_right + 1) % self->cols;
        }

        // roztažených nebylo dost – rozdělíme podle šířky sloupců
        float total = 0;
        for (size_t i = 0; i < self->col_widths_len; i++) {
            if (i >= self->col_resizable_len || !self->col_resizable[i]) continue;
            total += self->col_widths[i];
        }
        col_right = (col + 1) % self->cols;
        while (col_right != col) {
            if (!(col_right < self->col_resizable_len && !self->col_resizable[col_right])) {
                const float old = self->col_widths[col_right];
                const float r = resize_expanded * old / total;
                self->col_widths[col_right] = DVUI_MAX(DVUI_GRID_COL_MIN_WIDTH, self->col_widths[col_right] - r);
                resize_expanded -= old - self->col_widths[col_right];
                if (resize_expanded < 0.01f && resize_expanded > -0.01f) goto done;
            }
            col_right = (col_right + 1) % self->cols;
        }
    }
done:
    dvui_refresh(nullptr, DVUI_SRC, self->wd.id);
}

// DVUI: GridWidget.deinit (bez klávesové navigace a kurzoru buněk)
void dvui_grid_widget_deinit(dvui_GridWidget* self) {
    const dvui_Id id = self->wd.id;
    ensure_body_scroll(self);

    float tw = 0;
    for (size_t i = 0; i < self->col_widths_len; i++) tw += self->col_widths[i];
    // rowOffsetInternal: tady už není třeba cachovat
    const dvui_Size s = {tw, row_offset_internal(self, self->rows, false)};
    dvui_scroll_container_widget_min_size_for_child(&self->bscroll, s);
    dvui_scroll_container_widget_deinit(&self->bscroll);
    dvui_scroll_area_widget_deinit(&self->scroll);

    // synchronizace hlavičky a těla
    if (self->csi.viewport.x != self->frame_viewport.x) self->msi->viewport.x = self->csi.viewport.x;
    if (self->msi->viewport.x != self->frame_viewport.x) self->csi.viewport.x = self->msi->viewport.x;

    DVUI_DATA_SET(id, "__cols", size_t, (size_t)(self->max_seen_col + 1));
    DVUI_DATA_SET(id, "__rows", size_t, (size_t)(self->max_seen_row + 1));

    if (self->has_auto_size) {
        const dvui_GridAutoSize which = self->auto_size;
        bool auto_size_next_frame = false;
        if (which == DVUI_GRID_AUTO_SIZE_COLS || which == DVUI_GRID_AUTO_SIZE_BOTH) {
            for (size_t col = 0; col < self->col_widths_auto.len; col++) {
                if (col >= self->col_widths_len || self->col_widths_auto.items[col] != self->col_widths[col]) auto_size_next_frame = true;
            }
            dvui_data_set(nullptr, id, "__col_widths", self->col_widths_auto.items, self->col_widths_auto.len * sizeof(float));
        }

        if (which == DVUI_GRID_AUTO_SIZE_ROWS || which == DVUI_GRID_AUTO_SIZE_BOTH) {
            for (size_t i = 0; i < self->row_heights_auto.len; i++) {
                const dvui_GridRowHeight rh = self->row_heights_auto.items[i];
                if (rh.height != dvui_grid_widget_row_height(self, rh.row)) auto_size_next_frame = true;
            }

            // existující výšky řádků, které jsme tento snímek neviděli, zachováme
            dvui_GridRowHeightList* ra = &self->row_heights_auto;
            for (size_t i = 0; i < self->row_heights_len; i++) {
                const dvui_GridRowHeight rh = self->row_heights[i];
                size_t pp = 0;
                while (pp < ra->len && ra->items[pp].row < rh.row) pp++;
                if (pp == ra->len || ra->items[pp].row > rh.row) {
                    ra->items = dvui_frame_grow(ra->items, &ra->cap, sizeof(dvui_GridRowHeight), ra->len + 1);
                    memmove(&ra->items[pp + 1], &ra->items[pp], (ra->len - pp) * sizeof(dvui_GridRowHeight));
                    ra->items[pp] = rh;
                    ra->len++;
                }
            }
            dvui_data_set(nullptr, id, "__row_heights", ra->items, ra->len * sizeof(dvui_GridRowHeight));
            *self->col_header_height = self->col_header_height_auto;
        }

        if (auto_size_next_frame) {
            DVUI_DATA_SET(id, "__auto_size", dvui_GridAutoSize, which);
            dvui_refresh(nullptr, DVUI_SRC, id);
        }
    }

    if (!self->has_auto_size || self->auto_size == DVUI_GRID_AUTO_SIZE_ROWS) {
        // sloupce se nepřizpůsobují automaticky; uživatel může měnit šířku
        if (self->has_col_resize) apply_col_resize(self);

        if (self->max_seen_col >= (ptrdiff_t)self->cols) {
            // nové sloupce dostanou automatickou šířku
            for (size_t i = 0; i < self->col_widths_len; i++) self->col_widths_auto.items[i] = self->col_widths[i];
            dvui_data_set(nullptr, id, "__col_widths", self->col_widths_auto.items, self->col_widths_auto.len * sizeof(float));
        } else {
            dvui_data_set(nullptr, id, "__col_widths", self->col_widths, self->col_widths_len * sizeof(float));
        }
    }

    if (!self->has_auto_size || self->auto_size == DVUI_GRID_AUTO_SIZE_COLS) {
        dvui_data_set(nullptr, id, "__row_heights", self->row_heights, self->row_heights_len * sizeof(dvui_GridRowHeight));
    }

    dvui_data_set(nullptr, id, "__col_expands", self->col_expands_new, self->cols * sizeof(bool));
    dvui_data_set(nullptr, id, "__csi", &self->csi, sizeof(dvui_ScrollInfo));

    dvui_widget_data_min_size_set_and_refresh(&self->wd);
    dvui_widget_data_min_size_report_to_parent(&self->wd);
    dvui_parent_reset(id, self->wd.parent);
    if (dvui_widget_is_allocated(self)) dvui_widget_free(self);
}

dvui_GridWidget* dvui_grid(dvui_Src src, dvui_GridInitOptions init_opts, dvui_Options opts) {
    dvui_GridWidget* ret = dvui_widget_alloc(sizeof *ret);
    dvui_grid_widget_init(ret, src, init_opts, opts);
    return ret;
}
