#ifndef EMBROIDERY_UI_H
#define EMBROIDERY_UI_H

#include "model.h"
#include "ops.h"
#include "render.h"

constexpr int32_t WINDOW_INITIAL_WIDTH = 1920;
constexpr int32_t WINDOW_INITIAL_HEIGHT = 1080;
constexpr int32_t UI_FONT_BASE_PIXEL_SIZE = 16;
constexpr int32_t UI_FONT_MIN_PIXEL_SIZE = 12;
constexpr int32_t UI_FONT_MAX_PIXEL_SIZE = 48;
constexpr size_t UI_FONT_ASCII_COUNT = 95U;
constexpr size_t UI_FONT_CYRILLIC_COUNT = 96U;
constexpr size_t UI_FONT_EXTRA_COUNT = 12U;
constexpr size_t UI_FONT_GLYPH_COUNT =
    UI_FONT_ASCII_COUNT + UI_FONT_CYRILLIC_COUNT + UI_FONT_EXTRA_COUNT;
constexpr int32_t TOP_BAR_HEIGHT = 38;
constexpr int32_t LEFT_RAIL_WIDTH = 76;
constexpr int32_t RIGHT_PANEL_WIDTH = 268;
constexpr int32_t BOTTOM_BAR_HEIGHT = 60;
constexpr int32_t COLOR_ROW_HEIGHT = 36;
constexpr size_t MAX_TOP_BUTTONS = 16U;
constexpr size_t MAX_RAIL_BUTTONS = 20U;
constexpr size_t MAX_PROP_BUTTONS = 20U;
constexpr size_t MAX_STRIP_BUTTONS = 36U;
constexpr size_t MAX_STRIP_HISTORY_STATES = 16U;
constexpr size_t OPEN_DIALOG_FILTER_COUNT = 2U;
constexpr size_t SAVE_DIALOG_FILTER_COUNT = 2U;

static_assert(UI_FONT_GLYPH_COUNT == 203U, "UI font atlas must hold 203 pre-baked glyphs");

typedef struct {
    uint32_t atlas_offset;
    uint16_t width;
    uint16_t height;
    int16_t bitmap_left;
    int16_t bitmap_top;
    int16_t advance_x;
    uint16_t valid;
} UiGlyphMetric;

typedef struct {
    uint8_t *alpha_pixels;
    size_t alpha_byte_count;
    UiGlyphMetric glyphs[UI_FONT_GLYPH_COUNT];
    int32_t pixel_size;
    int32_t ascender;
    int32_t descender;
    int32_t cap_height;
    int32_t line_height;
    int32_t fallback_scale;
    bool using_freetype;
} UiFont;

typedef struct {
    const char *name;
    const char *pattern;
} DialogFilterSpec;

extern const DialogFilterSpec open_dialog_filters[OPEN_DIALOG_FILTER_COUNT];
extern const DialogFilterSpec save_dialog_filters[SAVE_DIALOG_FILTER_COUNT];

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} UiRect;

typedef enum : uint8_t {
    TOOL_SELECT_ELEMENT = 0U,
    TOOL_SELECT_CROSS = 1U,
    TOOL_DRAW_CROSS = 2U,
    TOOL_CROP_KEEP = 3U,
    TOOL_CUT_OUT = 4U,
    TOOL_SPLIT_VERTICAL = 5U,
    TOOL_SPLIT_HORIZONTAL = 6U,
    TOOL_CLONE = 7U,
    TOOL_PICK_COLOR = 8U
} EditorTool;

typedef enum : uint8_t {
    COMMAND_NONE = 0U,
    COMMAND_TOOL_SELECT_ELEMENT = 1U,
    COMMAND_TOOL_SELECT_CROSS = 2U,
    COMMAND_TOOL_DRAW_CROSS = 3U,
    COMMAND_OPEN_STRIP_MODAL = 4U,
    COMMAND_TOOL_CROP = 5U,
    COMMAND_TOOL_CUT = 6U,
    COMMAND_TOOL_SPLIT_V = 7U,
    COMMAND_TOOL_SPLIT_H = 8U,
    COMMAND_TOOL_CLONE = 9U,
    COMMAND_TOOL_PICK = 10U,
    COMMAND_ROTATE_90 = 11U,
    COMMAND_FLIP_X = 12U,
    COMMAND_FLIP_Y = 13U,
    COMMAND_CENTER = 14U,
    COMMAND_MERGE_COLORS = 15U,
    COMMAND_RECOLOR_SELECTION = 16U,
    COMMAND_DELETE_SELECTION = 17U,
    COMMAND_UNDO = 18U,
    COMMAND_REDO = 19U,
    COMMAND_SAVE = 20U,
    COMMAND_TOGGLE_DROP_MODE = 21U,
    COMMAND_CYCLE_HOOP = 22U,
    COMMAND_ROTATE_HOOP = 23U,
    COMMAND_TOGGLE_GRID = 24U,
    COMMAND_TOGGLE_JUMPS = 25U,
    COMMAND_TOGGLE_LOCKS = 26U,
    COMMAND_PITCH_DECREASE = 27U,
    COMMAND_PITCH_INCREASE = 28U,
    COMMAND_CYCLE_PASSES = 29U,
    COMMAND_FIT_VIEW = 30U,
    COMMAND_OPEN_FILE_DIALOG = 31U,
    COMMAND_MERGE_FILE_DIALOG = 32U,
    COMMAND_SAVE_AS_DIALOG = 33U,
    COMMAND_CYCLE_VIEW_MODE = 34U,
    COMMAND_CYCLE_THREAD_THICKNESS = 35U,
    COMMAND_TOGGLE_RIGHT_PANEL = 36U,
    COMMAND_ADD_COLOR_BLOCK = 37U
} ToolbarCommand;

typedef enum : uint8_t {
    STRIP_CMD_NONE = 0U,
    STRIP_CMD_WIDTH_DEC = 1U,
    STRIP_CMD_WIDTH_INC = 2U,
    STRIP_CMD_HEIGHT_DEC = 3U,
    STRIP_CMD_HEIGHT_INC = 4U,
    STRIP_CMD_REPEAT_X_DEC = 5U,
    STRIP_CMD_REPEAT_X_INC = 6U,
    STRIP_CMD_REPEAT_Y_DEC = 7U,
    STRIP_CMD_REPEAT_Y_INC = 8U,
    STRIP_CMD_PITCH_DEC = 9U,
    STRIP_CMD_PITCH_INC = 10U,
    STRIP_CMD_CYCLE_PASSES = 11U,
    STRIP_CMD_PICK_COLOR = 12U,
    STRIP_CMD_CLEAR = 13U,
    STRIP_CMD_MIRROR_X = 14U,
    STRIP_CMD_MIRROR_Y = 15U,
    STRIP_CMD_UNDO = 16U,
    STRIP_CMD_INSERT = 17U,
    STRIP_CMD_SAVE_JEF = 18U,
    STRIP_CMD_SAVE_AS = 19U,
    STRIP_CMD_CLOSE = 20U
} StripCommand;

typedef enum : uint8_t {
    PALETTE_TARGET_NONE = 0U,
    PALETTE_TARGET_ADD_BLOCK = 1U,
    PALETTE_TARGET_RECOLOR_BLOCK = 2U,
    PALETTE_TARGET_RECOLOR_SELECTION = 3U,
    PALETTE_TARGET_STRIP_COLOR = 4U
} PaletteModalTarget;

typedef enum : uint8_t {
    COLOR_LIST_HIT_NONE = 0U,
    COLOR_LIST_HIT_PANEL_EMPTY = 1U,
    COLOR_LIST_HIT_SELECT_BLOCK = 2U,
    COLOR_LIST_HIT_TOGGLE_VISIBILITY = 3U,
    COLOR_LIST_HIT_MOVE_UP = 4U,
    COLOR_LIST_HIT_MOVE_DOWN = 5U,
    COLOR_LIST_HIT_RECOLOR_BLOCK = 6U,
    COLOR_LIST_HIT_DELETE_BLOCK = 7U
} ColorListHitKind;

typedef struct {
    int32_t left_x;
    int32_t top_y;
    int32_t width;
    int32_t height;
    uint8_t command_id;
    char label[32];
    bool is_active;
    bool is_accent;
    bool is_danger;
    bool is_readout;
    bool has_swatch;
    RgbColor swatch_color;
} UiButton;

typedef struct {
    UiRect top_bar;
    UiRect left_rail;
    UiRect right_panel;
    UiRect bottom_bar;
    UiRect viewport;
    UiRect slider_track;
    UiRect colors_list_rect;
    UiButton top_buttons[MAX_TOP_BUTTONS];
    size_t top_button_count;
    UiButton rail_buttons[MAX_RAIL_BUTTONS];
    size_t rail_button_count;
    UiButton prop_buttons[MAX_PROP_BUTTONS];
    size_t prop_button_count;
} EditorLayout;

typedef struct {
    StripPattern states[MAX_STRIP_HISTORY_STATES];
    size_t state_count;
} StripHistory;

typedef struct {
    int32_t modal_left;
    int32_t modal_top;
    int32_t modal_width;
    int32_t modal_height;
    int32_t grid_left;
    int32_t grid_top;
    int32_t cell_size;
    int32_t cell_gap;
    int32_t columns;
    int32_t rows;
} PaletteModalLayout;

typedef struct {
    int32_t modal_left;
    int32_t modal_top;
    int32_t modal_width;
    int32_t modal_height;
    int32_t editor_left;
    int32_t editor_top;
    int32_t editor_width;
    int32_t editor_height;
    int32_t editor_cell_size;
    int32_t preview_left;
    int32_t preview_top;
    int32_t preview_width;
    int32_t preview_height;
    UiButton buttons[MAX_STRIP_BUTTONS];
    size_t button_count;
} StripModalLayout;

typedef struct {
    int32_t card_left;
    int32_t card_top;
    int32_t card_width;
    int32_t card_height;
    UiButton open_button;
    UiButton strip_button;
} StartScreenLayout;

typedef struct {
    char current_file_path[MAX_PATH_BUFFER];
    bool has_file_path;
    StitchDesign design;
    EditHistory history;
    CrossGridInfo grid;
    ScratchArena scratch;
    DesignSpatialIndex spatial_index;
    CrossClusterCache cross_cache;
    StitchLayerCache stitch_cache;
    UiFont ui_font;
    double ui_scale;
    uint64_t design_revision;
    uint64_t mask_revision;
    bool is_dirty;
    bool right_panel_collapsed;

    /* Live preview mask computed by the exact same ops function as Enter apply. */
    uint8_t *edit_preview_keep_mask;
    size_t edit_preview_mask_length;
    bool has_edit_preview_mask;
    bool preview_dirty;

    uint8_t *selection_mask;
    size_t selection_mask_length;
    size_t selected_segment_count;

    uint8_t *hover_mask;
    size_t hover_mask_length;
    size_t hover_seed_chain;
    size_t hover_component_id;
    bool has_hover_cross_box;
    CutRectangle hover_cross_box;
    int32_t last_draw_cursor_column;
    int32_t last_draw_cursor_row;
    bool has_draw_cursor_cell;

    StitchDesign floating_fragment;
    bool has_floating_fragment;
    bool floating_is_grid_origin_aligned;
    int32_t floating_offset_x;
    int32_t floating_offset_y;
    int32_t floating_base_origin_x;
    int32_t floating_base_origin_y;
    int32_t floating_anchor_x;
    int32_t floating_anchor_y;

    EditorTool active_tool;
    StitchViewMode view_mode;
    size_t thread_thickness_index;
    BlockDedupTable block_dedup;
    size_t active_color_block_index;
    bool drop_merge_mode;
    bool show_grid;
    bool show_jumps;
    bool show_locks;
    size_t hoop_preset_index;
    bool hoop_rotated;

    bool has_box_selection;
    bool is_dragging_box;
    int32_t box_start_x;
    int32_t box_start_y;
    int32_t box_end_x;
    int32_t box_end_y;
    int32_t split_coordinate;

    bool is_drawing_crosses;
    bool drawing_erases;
    bool draw_stroke_pushed_undo;
    int32_t last_drawn_column;
    int32_t last_drawn_row;

    double simulator_percent;
    bool is_dragging_slider;

    CameraView camera;
    bool is_panning;
    bool space_is_held;
    int32_t pan_last_x;
    int32_t pan_last_y;
    int32_t mouse_screen_x;
    int32_t mouse_screen_y;
    int32_t color_scroll_offset;

    PaletteModalTarget palette_target;
    size_t palette_target_block_index;
    int32_t hovered_palette_index;

    bool strip_modal_open;
    StripPattern strip_pattern;
    StripHistory strip_history;
    uint8_t strip_active_color;
    bool strip_is_painting;
    bool strip_is_erasing;
    bool strip_stroke_pushed_undo;

    char status_message[256];
    bool needs_redraw;
    bool should_quit;
} EditorState;

bool init_ui_font(UiFont *font, int32_t pixel_size);
bool init_ui_font_with_path(UiFont *font, const char *font_path, int32_t pixel_size);
void free_ui_font(UiFont *font);

bool point_is_in_box(
    int32_t point_x,
    int32_t point_y,
    int32_t box_x,
    int32_t box_y,
    int32_t box_width,
    int32_t box_height
);
bool rect_contains_point(UiRect rect, int32_t point_x, int32_t point_y);
int32_t measure_ui_text_width(const UiFont *font, const char *text);
void draw_ui_text(
    PixelCanvas *canvas,
    const UiFont *font,
    int32_t origin_x,
    int32_t origin_y,
    const char *text,
    uint32_t packed_color
);
void draw_ui_text_vcentered(
    PixelCanvas *canvas,
    const UiFont *font,
    int32_t origin_x,
    int32_t box_top,
    int32_t box_height,
    const char *text,
    uint32_t packed_color
);
void draw_ui_text_centered(
    PixelCanvas *canvas,
    const UiFont *font,
    UiRect box,
    const char *text,
    uint32_t packed_color
);
void draw_ui_button(
    PixelCanvas *canvas,
    const UiFont *font,
    const UiButton *button,
    int32_t mouse_x,
    int32_t mouse_y
);

void init_default_editor_state(EditorState *state);
void free_editor_state(EditorState *state);
void editor_set_ui_scale(EditorState *state, double ui_scale);
int32_t get_editor_color_row_height(const EditorState *state);
void editor_mark_design_changed(EditorState *state, bool mark_dirty);
void editor_ensure_spatial_index(EditorState *state);
void editor_update_canvas_hover(
    EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_width,
    int32_t window_height
);
void set_status_message(EditorState *state, const char *message);
void clear_segment_selection(EditorState *state);
void clear_hover_preview(EditorState *state);
void clear_floating_fragment(EditorState *state);
void clear_live_preview(EditorState *state);
bool ensure_selection_mask_allocated(EditorState *state);
void recount_selected_segments(EditorState *state);
void strip_history_push(EditorState *state);

EditorLayout compute_editor_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
);
RenderViewport get_editor_viewport(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
);
int32_t editor_world_to_screen_x(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t world_x
);
int32_t editor_world_to_screen_y(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t world_y
);
int32_t editor_screen_to_world_x(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t screen_x
);
int32_t editor_screen_to_world_y(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t screen_y
);
int32_t snap_coordinate_to_grid(int32_t value, int32_t origin, int32_t pitch);
void fit_camera_to_design(EditorState *state, int32_t window_width, int32_t window_height);

bool tool_uses_box_preview(EditorTool tool);
EditOperationKind tool_to_edit_operation(EditorTool tool);
void refresh_live_preview(EditorState *state);
bool should_show_start_screen(const EditorState *state);
void update_slider_from_layout(EditorState *state, const EditorLayout *layout, int32_t mouse_x);
const char *basename_of_path(const char *path);

StartScreenLayout compute_start_screen_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
);
PaletteModalLayout compute_palette_modal_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
);
StripModalLayout compute_strip_modal_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
);

uint8_t hit_test_editor_buttons(const EditorLayout *layout, int32_t mouse_x, int32_t mouse_y);
bool hit_test_slider_layout(const EditorLayout *layout, int32_t mouse_x, int32_t mouse_y);
ColorListHitKind hit_test_color_list(
    const EditorState *state,
    const EditorLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y,
    size_t *block_index_out
);
bool hit_test_palette_modal(
    const EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_width,
    int32_t window_height,
    int32_t *palette_index_out
);
uint8_t hit_test_strip_modal_button(
    const StripModalLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y
);
bool hit_test_strip_editor_cell(
    const StripPattern *pattern,
    const StripModalLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t *column_out,
    int32_t *row_out
);

void render_editor_frame(PixelCanvas *canvas, EditorState *state);

#endif
