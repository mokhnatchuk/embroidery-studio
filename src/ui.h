#ifndef EMBROIDERY_UI_H
#define EMBROIDERY_UI_H

#include "model.h"
#include "ops.h"
#include "render.h"

constexpr int32_t WINDOW_INITIAL_WIDTH = 1920;
constexpr int32_t WINDOW_INITIAL_HEIGHT = 1080;
constexpr int32_t FONT_SCALE = 2;
constexpr int32_t FONT_GLYPH_WIDTH = 8 * FONT_SCALE;
constexpr int32_t FONT_GLYPH_HEIGHT = 8 * FONT_SCALE;
constexpr int32_t LEFT_PANEL_WIDTH = 360;
constexpr int32_t COLOR_ROW_HEIGHT = 40;
constexpr int32_t BOTTOM_PANEL_HEIGHT = 104;
constexpr int32_t TOOLBAR_BUTTON_HEIGHT = 30;
constexpr int32_t TOOLBAR_BUTTON_PAD = 8;
constexpr int32_t TOOLBAR_BUTTON_GAP = 3;
constexpr int32_t TOOLBAR_ROW_GAP = 6;
constexpr int32_t TOOLBAR_PADDING = 6;
constexpr size_t MAX_TOOLBAR_BUTTONS = 44U;
constexpr size_t MAX_STRIP_BUTTONS = 36U;
constexpr size_t MAX_DIALOG_BUTTONS = 12U;
constexpr size_t MAX_STRIP_HISTORY_STATES = 16U;
constexpr int32_t FILE_ROW_HEIGHT = 30;
constexpr uint32_t DOUBLE_CLICK_MS = 400U;

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
    COMMAND_CYCLE_THREAD_THICKNESS = 35U
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
    FILE_DIALOG_CLOSED = 0U,
    FILE_DIALOG_OPEN = 1U,
    FILE_DIALOG_MERGE = 2U,
    FILE_DIALOG_SAVE_AS = 3U,
    FILE_DIALOG_STRIP_SAVE_AS = 4U
} FileDialogMode;

typedef enum : uint8_t {
    DIALOG_CMD_NONE = 0U,
    DIALOG_CMD_PARENT = 1U,
    DIALOG_CMD_HOME = 2U,
    DIALOG_CMD_DOWNLOADS = 3U,
    DIALOG_CMD_CONFIRM = 4U,
    DIALOG_CMD_CANCEL = 5U
} FileDialogCommand;

typedef enum : uint8_t {
    LEFT_PANEL_HIT_NONE = 0U,
    LEFT_PANEL_HIT_PANEL_EMPTY = 1U,
    LEFT_PANEL_HIT_ADD_COLOR = 2U,
    LEFT_PANEL_HIT_SELECT_BLOCK = 3U,
    LEFT_PANEL_HIT_TOGGLE_VISIBILITY = 4U,
    LEFT_PANEL_HIT_MOVE_UP = 5U,
    LEFT_PANEL_HIT_MOVE_DOWN = 6U,
    LEFT_PANEL_HIT_RECOLOR_BLOCK = 7U,
    LEFT_PANEL_HIT_DELETE_BLOCK = 8U
} LeftPanelHitKind;

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
    StripPattern states[MAX_STRIP_HISTORY_STATES];
    size_t state_count;
} StripHistory;

typedef struct {
    char name[256];
    bool is_directory;
    uint64_t file_size_bytes;
} FileBrowserEntry;

typedef struct {
    FileDialogMode mode;
    char current_directory[MAX_PATH_BUFFER];
    char filename_input[256];
    FileBrowserEntry *entries;
    size_t entry_count;
    size_t entry_capacity;
    size_t selected_index;
    int32_t scroll_offset;
    uint32_t last_click_ticks;
    size_t last_click_index;
    bool confirm_overwrite;
    char pending_overwrite_path[MAX_PATH_BUFFER];
    char error_text[128];
} FileDialogState;

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
    int32_t modal_left;
    int32_t modal_top;
    int32_t modal_width;
    int32_t modal_height;
    int32_t list_left;
    int32_t list_top;
    int32_t list_width;
    int32_t list_height;
    int32_t input_left;
    int32_t input_top;
    int32_t input_width;
    int32_t input_height;
    UiButton buttons[MAX_DIALOG_BUTTONS];
    size_t button_count;
} FileDialogLayout;

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
    bool has_hover_cross_box;
    CutRectangle hover_cross_box;

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
    int32_t slider_left_x;
    int32_t slider_right_x;
    int32_t slider_top_y;
    int32_t slider_bottom_y;

    CameraView camera;
    bool is_panning;
    bool space_is_held;
    int32_t pan_last_x;
    int32_t pan_last_y;
    int32_t mouse_screen_x;
    int32_t mouse_screen_y;
    int32_t color_scroll_offset;

    int32_t toolbar_height;
    UiButton toolbar_buttons[MAX_TOOLBAR_BUTTONS];
    size_t toolbar_button_count;

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

    FileDialogState file_dialog;
    bool text_input_active;

    char status_message[256];
    bool needs_redraw;
    bool should_quit;
} EditorState;

bool point_is_in_box(
    int32_t point_x,
    int32_t point_y,
    int32_t box_x,
    int32_t box_y,
    int32_t box_width,
    int32_t box_height
);
void draw_bitmap_text_2x(
    PixelCanvas *canvas,
    int32_t origin_x,
    int32_t origin_y,
    const char *text,
    uint32_t packed_color
);
int32_t measure_text_2x_width(const char *text);
void draw_ui_button(
    PixelCanvas *canvas,
    const UiButton *button,
    int32_t mouse_x,
    int32_t mouse_y
);

void init_default_editor_state(EditorState *state);
void free_editor_state(EditorState *state);
void set_status_message(EditorState *state, const char *message);
void clear_segment_selection(EditorState *state);
void clear_hover_preview(EditorState *state);
void clear_floating_fragment(EditorState *state);
void clear_live_preview(EditorState *state);
bool ensure_selection_mask_allocated(EditorState *state);
void recount_selected_segments(EditorState *state);
void strip_history_push(EditorState *state);

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
void update_slider_from_mouse(EditorState *state, int32_t mouse_x);

const char *basename_of_path(const char *path);
bool directory_exists(const char *directory_path);
bool file_has_embroidery_extension(const char *file_name);
bool load_file_browser_directory(EditorState *state, const char *directory_path);
void close_file_dialog_state(EditorState *state);
void open_file_dialog_state(EditorState *state, FileDialogMode mode);
void file_dialog_navigate_parent(EditorState *state);
void file_dialog_navigate_home(EditorState *state);
void file_dialog_navigate_downloads(EditorState *state);
void file_dialog_append_text_input(EditorState *state, const char *utf8_text);
void file_dialog_backspace_input(EditorState *state);

void layout_toolbar_buttons(EditorState *state, int32_t window_width);
StartScreenLayout compute_start_screen_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
);
PaletteModalLayout compute_palette_modal_layout(int32_t window_width, int32_t window_height);
StripModalLayout compute_strip_modal_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
);
FileDialogLayout compute_file_dialog_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
);

uint8_t hit_test_toolbar(const EditorState *state, int32_t mouse_x, int32_t mouse_y);
bool hit_test_slider(const EditorState *state, int32_t mouse_x, int32_t mouse_y);
LeftPanelHitKind hit_test_left_panel(
    const EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_height,
    size_t *block_index_out
);
bool hit_test_palette_modal(
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
uint8_t hit_test_file_dialog_button(
    const FileDialogLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y
);
bool hit_test_file_dialog_entry(
    const EditorState *state,
    const FileDialogLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y,
    size_t *entry_index_out
);

void render_editor_frame(PixelCanvas *canvas, EditorState *state);

#endif
