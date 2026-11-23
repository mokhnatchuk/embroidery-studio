#define _POSIX_C_SOURCE 200809L

#include "core.h"

#include <SDL.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

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
constexpr size_t MAX_HISTORY_STATES = 32U;
constexpr size_t MAX_STRIP_HISTORY_STATES = 16U;
constexpr size_t HOOP_PRESET_COUNT = 6U;
constexpr size_t THREAD_THICKNESS_PRESET_COUNT = 3U;
constexpr int32_t MAX_THREAD_STROKE_PX = 24;
constexpr size_t BLOCK_DEDUP_CAPACITY = 65536U;
constexpr size_t MAX_PATH_BUFFER = 4096U;
constexpr int32_t FILE_ROW_HEIGHT = 30;
constexpr uint32_t DOUBLE_CLICK_MS = 400U;

typedef enum : uint8_t {
    VIEW_MODE_THREAD = 0U,
    VIEW_MODE_BLOCKS = 1U,
    VIEW_MODE_LINE = 2U
} StitchViewMode;

static const double thread_thickness_units[THREAD_THICKNESS_PRESET_COUNT] = {
    3.5, 4.5, 6.0
};

static const char *thread_thickness_labels[THREAD_THICKNESS_PRESET_COUNT] = {
    "Thk:0.35", "Thk:0.45", "Thk:0.60"
};

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

typedef struct {
    const char *label;
    int32_t width_units;
    int32_t height_units;
} HoopPreset;

static const HoopPreset janome_550e_hoops[HOOP_PRESET_COUNT] = {
    {"Off", 0, 0},
    {"140x140", 1400, 1400},
    {"140x200", 1400, 2000},
    {"200x200", 2000, 2000},
    {"200x280", 2000, 2800},
    {"200x360", 2000, 3600}
};

/* Compact 8x8 ASCII bitmap font for code points 32 (' ') through 127 (DEL). */
static const uint8_t bitmap_font_8x8[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 32 ' ' */
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, /* 33 '!' */
    {0x36, 0x36, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 34 '"' */
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, /* 35 '#' */
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, /* 36 '$' */
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, /* 37 '%' */
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, /* 38 '&' */
    {0x18, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 39 ''' */
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, /* 40 '(' */
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, /* 41 ')' */
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, /* 42 '*' */
    {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00}, /* 43 '+' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x0C}, /* 44 ',' */
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, /* 45 '-' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, /* 46 '.' */
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, /* 47 '/' */
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, /* 48 '0' */
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, /* 49 '1' */
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, /* 50 '2' */
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, /* 51 '3' */
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, /* 52 '4' */
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, /* 53 '5' */
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, /* 54 '6' */
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, /* 55 '7' */
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, /* 56 '8' */
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, /* 57 '9' */
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00}, /* 58 ':' */
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x0C}, /* 59 ';' */
    {0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x00}, /* 60 '<' */
    {0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00}, /* 61 '=' */
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, /* 62 '>' */
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, /* 63 '?' */
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, /* 64 '@' */
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, /* 65 'A' */
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, /* 66 'B' */
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, /* 67 'C' */
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, /* 68 'D' */
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, /* 69 'E' */
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, /* 70 'F' */
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, /* 71 'G' */
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, /* 72 'H' */
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 73 'I' */
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, /* 74 'J' */
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, /* 75 'K' */
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, /* 76 'L' */
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, /* 77 'M' */
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, /* 78 'N' */
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, /* 79 'O' */
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, /* 80 'P' */
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, /* 81 'Q' */
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, /* 82 'R' */
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, /* 83 'S' */
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 84 'T' */
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, /* 85 'U' */
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, /* 86 'V' */
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, /* 87 'W' */
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, /* 88 'X' */
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, /* 89 'Y' */
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, /* 90 'Z' */
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, /* 91 '[' */
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, /* 92 '\' */
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, /* 93 ']' */
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, /* 94 '^' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, /* 95 '_' */
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 96 '`' */
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, /* 97 'a' */
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, /* 98 'b' */
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, /* 99 'c' */
    {0x38, 0x30, 0x30, 0x3E, 0x33, 0x33, 0x6E, 0x00}, /* 100 'd' */
    {0x00, 0x00, 0x1E, 0x33, 0x3F, 0x03, 0x1E, 0x00}, /* 101 'e' */
    {0x1C, 0x36, 0x06, 0x0F, 0x06, 0x06, 0x0F, 0x00}, /* 102 'f' */
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, /* 103 'g' */
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, /* 104 'h' */
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 105 'i' */
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, /* 106 'j' */
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, /* 107 'k' */
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* 108 'l' */
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, /* 109 'm' */
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, /* 110 'n' */
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, /* 111 'o' */
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, /* 112 'p' */
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, /* 113 'q' */
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, /* 114 'r' */
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, /* 115 's' */
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, /* 116 't' */
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, /* 117 'u' */
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, /* 118 'v' */
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, /* 119 'w' */
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, /* 120 'x' */
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, /* 121 'y' */
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, /* 122 'z' */
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, /* 123 '{' */
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, /* 124 '|' */
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, /* 125 '}' */
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 126 '~' */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  /* 127 DEL */
};

typedef struct {
    uint32_t *pixels;
    int32_t width;
    int32_t height;
    int32_t clip_min_x;
    int32_t clip_min_y;
    int32_t clip_max_x;
    int32_t clip_max_y;
} PixelCanvas;

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
    StitchDesign states[MAX_HISTORY_STATES];
    size_t state_count;
    size_t current_index;
} EditHistory;

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
    size_t color_block_index;
    int32_t center_x;
    int32_t center_y;
    uint8_t visual_flags;
    uint32_t generation;
} BlockDedupSlot;

typedef struct {
    char current_file_path[MAX_PATH_BUFFER];
    bool has_file_path;
    StitchDesign design;
    EditHistory history;
    CrossGridInfo grid;

    /* Live preview mask computed by the exact same core function as Enter apply. */
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
    BlockDedupSlot *block_dedup_table;
    uint32_t block_dedup_generation;
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

    double camera_center_x;
    double camera_center_y;
    double pixels_per_unit;
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

    char status_message[256];
    bool needs_redraw;
    bool should_quit;
} EditorState;

static uint32_t pack_rgb_pixel(uint8_t red, uint8_t green, uint8_t blue) {
    return (0xFFU << 24) | ((uint32_t)red << 16) | ((uint32_t)green << 8) | (uint32_t)blue;
}

static bool point_is_in_box(
    int32_t point_x,
    int32_t point_y,
    int32_t box_x,
    int32_t box_y,
    int32_t box_width,
    int32_t box_height
) {
    return point_x >= box_x && point_x < box_x + box_width &&
           point_y >= box_y && point_y < box_y + box_height;
}

static void reset_canvas_clip(PixelCanvas *canvas) {
    canvas->clip_min_x = 0;
    canvas->clip_min_y = 0;
    canvas->clip_max_x = canvas->width;
    canvas->clip_max_y = canvas->height;
}

static void put_pixel_clipped(
    PixelCanvas *canvas,
    int32_t pixel_x,
    int32_t pixel_y,
    uint32_t packed_color
) {
    if (pixel_x < canvas->clip_min_x || pixel_x >= canvas->clip_max_x ||
        pixel_y < canvas->clip_min_y || pixel_y >= canvas->clip_max_y) {
        return;
    }
    canvas->pixels[(size_t)pixel_y * (size_t)canvas->width + (size_t)pixel_x] = packed_color;
}

static void blend_pixel_clipped(
    PixelCanvas *canvas,
    int32_t pixel_x,
    int32_t pixel_y,
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    uint8_t alpha
) {
    if (pixel_x < canvas->clip_min_x || pixel_x >= canvas->clip_max_x ||
        pixel_y < canvas->clip_min_y || pixel_y >= canvas->clip_max_y) {
        return;
    }
    const size_t offset = (size_t)pixel_y * (size_t)canvas->width + (size_t)pixel_x;
    const uint32_t existing = canvas->pixels[offset];
    const uint32_t background_red = (existing >> 16) & 0xFFU;
    const uint32_t background_green = (existing >> 8) & 0xFFU;
    const uint32_t background_blue = existing & 0xFFU;
    const uint32_t alpha_weight = (uint32_t)alpha;
    const uint32_t inverse_weight = 255U - alpha_weight;

    const uint8_t out_red =
        (uint8_t)(((uint32_t)red * alpha_weight + background_red * inverse_weight) / 255U);
    const uint8_t out_green =
        (uint8_t)(((uint32_t)green * alpha_weight + background_green * inverse_weight) / 255U);
    const uint8_t out_blue =
        (uint8_t)(((uint32_t)blue * alpha_weight + background_blue * inverse_weight) / 255U);
    canvas->pixels[offset] = pack_rgb_pixel(out_red, out_green, out_blue);
}

static void draw_filled_rectangle(
    PixelCanvas *canvas,
    int32_t rectangle_x,
    int32_t rectangle_y,
    int32_t rectangle_width,
    int32_t rectangle_height,
    uint32_t packed_color
) {
    int32_t start_x = rectangle_x;
    int32_t start_y = rectangle_y;
    int32_t end_x = rectangle_x + rectangle_width;
    int32_t end_y = rectangle_y + rectangle_height;

    if (start_x < canvas->clip_min_x) {
        start_x = canvas->clip_min_x;
    }
    if (start_y < canvas->clip_min_y) {
        start_y = canvas->clip_min_y;
    }
    if (end_x > canvas->clip_max_x) {
        end_x = canvas->clip_max_x;
    }
    if (end_y > canvas->clip_max_y) {
        end_y = canvas->clip_max_y;
    }

    for (int32_t row_y = start_y; row_y < end_y; row_y += 1) {
        uint32_t *row_pixels = canvas->pixels + (size_t)row_y * (size_t)canvas->width;
        for (int32_t column_x = start_x; column_x < end_x; column_x += 1) {
            row_pixels[column_x] = packed_color;
        }
    }
}

static void draw_blended_rectangle(
    PixelCanvas *canvas,
    int32_t rectangle_x,
    int32_t rectangle_y,
    int32_t rectangle_width,
    int32_t rectangle_height,
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    uint8_t alpha
) {
    int32_t start_x = rectangle_x;
    int32_t start_y = rectangle_y;
    int32_t end_x = rectangle_x + rectangle_width;
    int32_t end_y = rectangle_y + rectangle_height;

    if (start_x < canvas->clip_min_x) {
        start_x = canvas->clip_min_x;
    }
    if (start_y < canvas->clip_min_y) {
        start_y = canvas->clip_min_y;
    }
    if (end_x > canvas->clip_max_x) {
        end_x = canvas->clip_max_x;
    }
    if (end_y > canvas->clip_max_y) {
        end_y = canvas->clip_max_y;
    }

    for (int32_t row_y = start_y; row_y < end_y; row_y += 1) {
        for (int32_t column_x = start_x; column_x < end_x; column_x += 1) {
            blend_pixel_clipped(canvas, column_x, row_y, red, green, blue, alpha);
        }
    }
}

static inline int32_t fast_round_to_int32(double value) {
    if (value > 10000000.0) {
        return 10000000;
    }
    if (value < -10000000.0) {
        return -10000000;
    }
    if (value >= 0.0) {
        return (int32_t)(value + 0.5);
    }
    return (int32_t)(value - 0.5);
}

static bool clip_line_to_canvas(
    const PixelCanvas *canvas,
    int32_t start_x,
    int32_t start_y,
    int32_t end_x,
    int32_t end_y,
    int32_t *clipped_start_x_out,
    int32_t *clipped_start_y_out,
    int32_t *clipped_end_x_out,
    int32_t *clipped_end_y_out
) {
    if (start_x >= canvas->clip_min_x && start_x < canvas->clip_max_x &&
        end_x >= canvas->clip_min_x && end_x < canvas->clip_max_x &&
        start_y >= canvas->clip_min_y && start_y < canvas->clip_max_y &&
        end_y >= canvas->clip_min_y && end_y < canvas->clip_max_y) {
        *clipped_start_x_out = start_x;
        *clipped_start_y_out = start_y;
        *clipped_end_x_out = end_x;
        *clipped_end_y_out = end_y;
        return true;
    }

    int32_t box_min_x = start_x;
    int32_t box_max_x = end_x;
    if (end_x < start_x) {
        box_min_x = end_x;
        box_max_x = start_x;
    }
    int32_t box_min_y = start_y;
    int32_t box_max_y = end_y;
    if (end_y < start_y) {
        box_min_y = end_y;
        box_max_y = start_y;
    }
    if (box_max_x < canvas->clip_min_x || box_min_x >= canvas->clip_max_x ||
        box_max_y < canvas->clip_min_y || box_min_y >= canvas->clip_max_y) {
        return false;
    }

    double parameter_min = 0.0;
    double parameter_max = 1.0;
    const double delta_x_float = (double)end_x - (double)start_x;
    const double delta_y_float = (double)end_y - (double)start_y;
    const double clip_denominator[4] = {
        -delta_x_float,
        delta_x_float,
        -delta_y_float,
        delta_y_float
    };
    const double clip_numerator[4] = {
        (double)start_x - (double)canvas->clip_min_x,
        (double)(canvas->clip_max_x - 1) - (double)start_x,
        (double)start_y - (double)canvas->clip_min_y,
        (double)(canvas->clip_max_y - 1) - (double)start_y
    };

    for (size_t edge_index = 0U; edge_index < 4U; edge_index += 1U) {
        if (clip_denominator[edge_index] == 0.0) {
            if (clip_numerator[edge_index] < 0.0) {
                return false;
            }
            continue;
        }
        const double ratio = clip_numerator[edge_index] / clip_denominator[edge_index];
        if (clip_denominator[edge_index] < 0.0) {
            if (ratio > parameter_max) {
                return false;
            }
            if (ratio > parameter_min) {
                parameter_min = ratio;
            }
        } else {
            if (ratio < parameter_min) {
                return false;
            }
            if (ratio < parameter_max) {
                parameter_max = ratio;
            }
        }
    }

    *clipped_start_x_out = fast_round_to_int32((double)start_x + parameter_min * delta_x_float);
    *clipped_start_y_out = fast_round_to_int32((double)start_y + parameter_min * delta_y_float);
    *clipped_end_x_out = fast_round_to_int32((double)start_x + parameter_max * delta_x_float);
    *clipped_end_y_out = fast_round_to_int32((double)start_y + parameter_max * delta_y_float);
    return true;
}

static void draw_line_clipped(
    PixelCanvas *canvas,
    int32_t start_x,
    int32_t start_y,
    int32_t end_x,
    int32_t end_y,
    uint32_t packed_color,
    int32_t dash_period
) {
    int32_t clipped_start_x = 0;
    int32_t clipped_start_y = 0;
    int32_t clipped_end_x = 0;
    int32_t clipped_end_y = 0;
    if (!clip_line_to_canvas(
            canvas,
            start_x,
            start_y,
            end_x,
            end_y,
            &clipped_start_x,
            &clipped_start_y,
            &clipped_end_x,
            &clipped_end_y
        )) {
        return;
    }

    int32_t delta_x = clipped_end_x - clipped_start_x;
    int32_t step_x = 1;
    if (delta_x < 0) {
        delta_x = -delta_x;
        step_x = -1;
    }

    int32_t delta_y = clipped_end_y - clipped_start_y;
    int32_t step_y = 1;
    if (delta_y < 0) {
        delta_y = -delta_y;
        step_y = -1;
    }

    int32_t error_term = delta_x - delta_y;
    int32_t current_x = clipped_start_x;
    int32_t current_y = clipped_start_y;
    const size_t stride = (size_t)canvas->width;

    if (dash_period <= 0) {
        while (true) {
            if (current_x >= canvas->clip_min_x && current_x < canvas->clip_max_x &&
                current_y >= canvas->clip_min_y && current_y < canvas->clip_max_y) {
                canvas->pixels[(size_t)current_y * stride + (size_t)current_x] = packed_color;
            }
            if (current_x == clipped_end_x && current_y == clipped_end_y) {
                break;
            }
            const int32_t doubled_error = error_term * 2;
            if (doubled_error > -delta_y) {
                error_term -= delta_y;
                current_x += step_x;
            }
            if (doubled_error < delta_x) {
                error_term += delta_x;
                current_y += step_y;
            }
        }
        return;
    }

    int32_t dash_counter = 0;
    while (true) {
        if ((dash_counter / dash_period) % 2 == 0) {
            put_pixel_clipped(canvas, current_x, current_y, packed_color);
        }
        if (current_x == clipped_end_x && current_y == clipped_end_y) {
            break;
        }
        const int32_t doubled_error = error_term * 2;
        if (doubled_error > -delta_y) {
            error_term -= delta_y;
            current_x += step_x;
        }
        if (doubled_error < delta_x) {
            error_term += delta_x;
            current_y += step_y;
        }
        dash_counter += 1;
    }
}

static void draw_thick_line_clipped(
    PixelCanvas *canvas,
    int32_t start_x,
    int32_t start_y,
    int32_t end_x,
    int32_t end_y,
    uint32_t packed_color
) {
    int32_t clipped_start_x = 0;
    int32_t clipped_start_y = 0;
    int32_t clipped_end_x = 0;
    int32_t clipped_end_y = 0;
    if (!clip_line_to_canvas(
            canvas,
            start_x,
            start_y,
            end_x,
            end_y,
            &clipped_start_x,
            &clipped_start_y,
            &clipped_end_x,
            &clipped_end_y
        )) {
        return;
    }

    int32_t delta_x = clipped_end_x - clipped_start_x;
    int32_t step_x = 1;
    if (delta_x < 0) {
        delta_x = -delta_x;
        step_x = -1;
    }

    int32_t delta_y = clipped_end_y - clipped_start_y;
    int32_t step_y = 1;
    if (delta_y < 0) {
        delta_y = -delta_y;
        step_y = -1;
    }

    int32_t error_term = delta_x - delta_y;
    int32_t current_x = clipped_start_x;
    int32_t current_y = clipped_start_y;
    const size_t stride = (size_t)canvas->width;
    const int32_t clip_min_x = canvas->clip_min_x;
    const int32_t clip_max_x = canvas->clip_max_x;
    const int32_t clip_min_y = canvas->clip_min_y;
    const int32_t clip_max_y = canvas->clip_max_y;

    while (true) {
        if (current_y >= clip_min_y && current_y < clip_max_y) {
            uint32_t *row = canvas->pixels + (size_t)current_y * stride;
            if (current_x >= clip_min_x && current_x < clip_max_x) {
                row[current_x] = packed_color;
            }
            if (current_x + 1 >= clip_min_x && current_x + 1 < clip_max_x) {
                row[current_x + 1] = packed_color;
            }
        }
        if (current_y + 1 >= clip_min_y && current_y + 1 < clip_max_y &&
            current_x >= clip_min_x && current_x < clip_max_x) {
            canvas->pixels[(size_t)(current_y + 1) * stride + (size_t)current_x] = packed_color;
        }
        if (current_x == clipped_end_x && current_y == clipped_end_y) {
            break;
        }
        const int32_t doubled_error = error_term * 2;
        if (doubled_error > -delta_y) {
            error_term -= delta_y;
            current_x += step_x;
        }
        if (doubled_error < delta_x) {
            error_term += delta_x;
            current_y += step_y;
        }
    }
}

static void draw_rectangle_outline(
    PixelCanvas *canvas,
    int32_t rectangle_x,
    int32_t rectangle_y,
    int32_t rectangle_width,
    int32_t rectangle_height,
    uint32_t packed_color,
    int32_t dash_period
) {
    if (rectangle_width <= 0 || rectangle_height <= 0) {
        return;
    }
    const int32_t right_x = rectangle_x + rectangle_width - 1;
    const int32_t bottom_y = rectangle_y + rectangle_height - 1;
    draw_line_clipped(canvas, rectangle_x, rectangle_y, right_x, rectangle_y, packed_color, dash_period);
    draw_line_clipped(canvas, right_x, rectangle_y, right_x, bottom_y, packed_color, dash_period);
    draw_line_clipped(canvas, right_x, bottom_y, rectangle_x, bottom_y, packed_color, dash_period);
    draw_line_clipped(canvas, rectangle_x, bottom_y, rectangle_x, rectangle_y, packed_color, dash_period);
}

static void draw_bitmap_text_2x(
    PixelCanvas *canvas,
    int32_t origin_x,
    int32_t origin_y,
    const char *text,
    uint32_t packed_color
) {
    int32_t cursor_x = origin_x;
    for (size_t text_index = 0U; text[text_index] != '\0'; text_index += 1U) {
        unsigned char character = (unsigned char)text[text_index];
        if (character < 32U || character > 127U) {
            character = '?';
        }
        const uint8_t *glyph_rows = bitmap_font_8x8[character - 32U];
        for (int32_t row = 0; row < 8; row += 1) {
            const uint8_t row_bits = glyph_rows[row];
            for (int32_t column = 0; column < 8; column += 1) {
                if ((row_bits & (1U << column)) != 0U) {
                    const int32_t base_x = cursor_x + column * FONT_SCALE;
                    const int32_t base_y = origin_y + row * FONT_SCALE;
                    put_pixel_clipped(canvas, base_x, base_y, packed_color);
                    put_pixel_clipped(canvas, base_x + 1, base_y, packed_color);
                    put_pixel_clipped(canvas, base_x, base_y + 1, packed_color);
                    put_pixel_clipped(canvas, base_x + 1, base_y + 1, packed_color);
                }
            }
        }
        cursor_x += FONT_GLYPH_WIDTH;
    }
}

static int32_t measure_text_2x_width(const char *text) {
    return (int32_t)strlen(text) * FONT_GLYPH_WIDTH;
}

static void draw_ui_button(
    PixelCanvas *canvas,
    const UiButton *button,
    int32_t mouse_x,
    int32_t mouse_y
) {
    const bool is_hovered =
        !button->is_readout &&
        point_is_in_box(mouse_x, mouse_y, button->left_x, button->top_y, button->width, button->height);

    uint32_t background = pack_rgb_pixel(52U, 56U, 64U);
    uint32_t border = pack_rgb_pixel(90U, 96U, 108U);
    uint32_t text_color = pack_rgb_pixel(236U, 240U, 246U);

    if (button->is_readout) {
        background = pack_rgb_pixel(30U, 33U, 38U);
        border = pack_rgb_pixel(70U, 75U, 85U);
        text_color = pack_rgb_pixel(190U, 224U, 255U);
    } else if (button->is_danger) {
        background = pack_rgb_pixel(188U, 40U, 40U);
        border = pack_rgb_pixel(255U, 152U, 152U);
        text_color = pack_rgb_pixel(255U, 255U, 255U);
    } else if (button->is_active) {
        background = pack_rgb_pixel(38U, 108U, 196U);
        border = pack_rgb_pixel(120U, 188U, 255U);
        text_color = pack_rgb_pixel(255U, 255U, 255U);
    } else if (button->is_accent) {
        background = pack_rgb_pixel(38U, 126U, 72U);
        border = pack_rgb_pixel(96U, 204U, 134U);
        text_color = pack_rgb_pixel(255U, 255U, 255U);
    }
    if (is_hovered) {
        if (button->is_danger) {
            background = pack_rgb_pixel(216U, 52U, 52U);
        } else if (button->is_active) {
            background = pack_rgb_pixel(52U, 126U, 218U);
        } else if (button->is_accent) {
            background = pack_rgb_pixel(48U, 148U, 86U);
        } else {
            background = pack_rgb_pixel(72U, 78U, 90U);
        }
        border = pack_rgb_pixel(210U, 224U, 242U);
    }

    draw_filled_rectangle(canvas, button->left_x, button->top_y, button->width, button->height, background);
    draw_rectangle_outline(canvas, button->left_x, button->top_y, button->width, button->height, border, 0);

    int32_t text_x = button->left_x + (button->width - measure_text_2x_width(button->label)) / 2;
    if (button->has_swatch) {
        const int32_t swatch_size = button->height - 10;
        const int32_t swatch_x = button->left_x + 6;
        const int32_t swatch_y = button->top_y + 5;
        draw_filled_rectangle(
            canvas,
            swatch_x,
            swatch_y,
            swatch_size,
            swatch_size,
            pack_rgb_pixel(
                button->swatch_color.red,
                button->swatch_color.green,
                button->swatch_color.blue
            )
        );
        draw_rectangle_outline(
            canvas,
            swatch_x,
            swatch_y,
            swatch_size,
            swatch_size,
            pack_rgb_pixel(230U, 230U, 230U),
            0
        );
        text_x = swatch_x + swatch_size + 6;
    }

    const int32_t text_y = button->top_y + (button->height - FONT_GLYPH_HEIGHT) / 2;
    draw_bitmap_text_2x(canvas, text_x, text_y, button->label, text_color);
}

static void set_status_message(EditorState *state, const char *message) {
    const int written = snprintf(state->status_message, sizeof(state->status_message), "%s", message);
    if (written < 0) {
        state->status_message[0] = '\0';
    }
    state->needs_redraw = true;
}

static void clear_segment_selection(EditorState *state) {
    free(state->selection_mask);
    state->selection_mask = nullptr;
    state->selection_mask_length = 0U;
    state->selected_segment_count = 0U;
}

static void clear_hover_preview(EditorState *state) {
    free(state->hover_mask);
    state->hover_mask = nullptr;
    state->hover_mask_length = 0U;
    state->hover_seed_chain = SIZE_MAX;
    state->has_hover_cross_box = false;
}

static void clear_floating_fragment(EditorState *state) {
    free_stitch_design(&state->floating_fragment);
    state->has_floating_fragment = false;
    state->floating_is_grid_origin_aligned = false;
    state->floating_offset_x = 0;
    state->floating_offset_y = 0;
}

static void clear_live_preview(EditorState *state) {
    free(state->edit_preview_keep_mask);
    state->edit_preview_keep_mask = nullptr;
    state->edit_preview_mask_length = 0U;
    state->has_edit_preview_mask = false;
    state->preview_dirty = false;
}

static bool ensure_selection_mask_allocated(EditorState *state) {
    const size_t total_segments = count_design_segments(&state->design);
    if (total_segments == 0U) {
        clear_segment_selection(state);
        return false;
    }
    if (state->selection_mask != nullptr && state->selection_mask_length == total_segments) {
        return true;
    }
    clear_segment_selection(state);
    state->selection_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
    if (state->selection_mask == nullptr) {
        return false;
    }
    state->selection_mask_length = total_segments;
    return true;
}

static void recount_selected_segments(EditorState *state) {
    size_t count = 0U;
    for (size_t index = 0U; index < state->selection_mask_length; index += 1U) {
        if (state->selection_mask[index] != 0U) {
            count += 1U;
        }
    }
    state->selected_segment_count = count;
}

static void history_clear(EditHistory *history) {
    for (size_t index = 0U; index < history->state_count; index += 1U) {
        free_stitch_design(&history->states[index]);
    }
    history->state_count = 0U;
    history->current_index = 0U;
}

static bool history_push(EditHistory *history, const StitchDesign *design) {
    if (history->state_count > 0U && history->current_index + 1U < history->state_count) {
        for (size_t drop_index = history->current_index + 1U;
             drop_index < history->state_count;
             drop_index += 1U) {
            free_stitch_design(&history->states[drop_index]);
        }
        history->state_count = history->current_index + 1U;
    }

    if (history->state_count == MAX_HISTORY_STATES) {
        free_stitch_design(&history->states[0]);
        for (size_t shift_index = 1U; shift_index < MAX_HISTORY_STATES; shift_index += 1U) {
            history->states[shift_index - 1U] = history->states[shift_index];
        }
        init_stitch_design(&history->states[MAX_HISTORY_STATES - 1U]);
        history->state_count = MAX_HISTORY_STATES - 1U;
        if (history->current_index > 0U) {
            history->current_index -= 1U;
        }
    }

    if (!clone_stitch_design(design, &history->states[history->state_count])) {
        return false;
    }
    history->current_index = history->state_count;
    history->state_count += 1U;
    return true;
}

static void strip_history_push(EditorState *state) {
    if (state->strip_history.state_count == MAX_STRIP_HISTORY_STATES) {
        for (size_t index = 1U; index < MAX_STRIP_HISTORY_STATES; index += 1U) {
            state->strip_history.states[index - 1U] = state->strip_history.states[index];
        }
        state->strip_history.state_count = MAX_STRIP_HISTORY_STATES - 1U;
    }
    state->strip_history.states[state->strip_history.state_count] = state->strip_pattern;
    state->strip_history.state_count += 1U;
}

static void get_viewport_bounds(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t *left_out,
    int32_t *top_out,
    int32_t *width_out,
    int32_t *height_out
) {
    const int32_t viewport_left = LEFT_PANEL_WIDTH;
    const int32_t viewport_top = state->toolbar_height;
    int32_t viewport_width = window_width - viewport_left;
    int32_t viewport_height = window_height - viewport_top - BOTTOM_PANEL_HEIGHT;
    if (viewport_width < 64) {
        viewport_width = 64;
    }
    if (viewport_height < 64) {
        viewport_height = 64;
    }
    *left_out = viewport_left;
    *top_out = viewport_top;
    *width_out = viewport_width;
    *height_out = viewport_height;
}

static int32_t world_to_screen_x(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t world_x
) {
    int32_t viewport_left = 0;
    int32_t viewport_top = 0;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    get_viewport_bounds(
        state,
        window_width,
        window_height,
        &viewport_left,
        &viewport_top,
        &viewport_width,
        &viewport_height
    );
    const double center_x = (double)viewport_left + (double)viewport_width * 0.5;
    return (int32_t)llround(center_x + ((double)world_x - state->camera_center_x) * state->pixels_per_unit);
}

static int32_t world_to_screen_y(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t world_y
) {
    int32_t viewport_left = 0;
    int32_t viewport_top = 0;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    get_viewport_bounds(
        state,
        window_width,
        window_height,
        &viewport_left,
        &viewport_top,
        &viewport_width,
        &viewport_height
    );
    const double center_y = (double)viewport_top + (double)viewport_height * 0.5;
    return (int32_t)llround(center_y + ((double)world_y - state->camera_center_y) * state->pixels_per_unit);
}

static int32_t screen_to_world_x(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t screen_x
) {
    int32_t viewport_left = 0;
    int32_t viewport_top = 0;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    get_viewport_bounds(
        state,
        window_width,
        window_height,
        &viewport_left,
        &viewport_top,
        &viewport_width,
        &viewport_height
    );
    const double center_x = (double)viewport_left + (double)viewport_width * 0.5;
    return (int32_t)llround(state->camera_center_x + ((double)screen_x - center_x) / state->pixels_per_unit);
}

static int32_t screen_to_world_y(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t screen_y
) {
    int32_t viewport_left = 0;
    int32_t viewport_top = 0;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    get_viewport_bounds(
        state,
        window_width,
        window_height,
        &viewport_left,
        &viewport_top,
        &viewport_width,
        &viewport_height
    );
    const double center_y = (double)viewport_top + (double)viewport_height * 0.5;
    return (int32_t)llround(state->camera_center_y + ((double)screen_y - center_y) / state->pixels_per_unit);
}

static int32_t snap_coordinate_to_grid(int32_t value, int32_t origin, int32_t pitch) {
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        return value;
    }
    const int32_t cell_index = floor_divide_int32(value - origin + pitch / 2, pitch);
    return origin + cell_index * pitch;
}

static void fit_camera_to_design(EditorState *state, int32_t window_width, int32_t window_height) {
    int32_t viewport_left = 0;
    int32_t viewport_top = 0;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    get_viewport_bounds(
        state,
        window_width,
        window_height,
        &viewport_left,
        &viewport_top,
        &viewport_width,
        &viewport_height
    );

    if (!state->design.has_bounds) {
        state->camera_center_x = 0.0;
        state->camera_center_y = 0.0;
        state->pixels_per_unit = 0.5;
        state->needs_redraw = true;
        return;
    }

    double design_width = (double)state->design.maximum_x - (double)state->design.minimum_x;
    double design_height = (double)state->design.maximum_y - (double)state->design.minimum_y;
    if (design_width < 20.0) {
        design_width = 20.0;
    }
    if (design_height < 20.0) {
        design_height = 20.0;
    }

    const double scale_x = ((double)viewport_width * 0.86) / design_width;
    const double scale_y = ((double)viewport_height * 0.86) / design_height;
    double best_scale = scale_x;
    if (scale_y < best_scale) {
        best_scale = scale_y;
    }
    if (best_scale < 0.05) {
        best_scale = 0.05;
    }
    if (best_scale > 12.0) {
        best_scale = 12.0;
    }

    state->camera_center_x =
        ((double)state->design.minimum_x + (double)state->design.maximum_x) * 0.5;
    state->camera_center_y =
        ((double)state->design.minimum_y + (double)state->design.maximum_y) * 0.5;
    state->pixels_per_unit = best_scale;
    state->needs_redraw = true;
}

static bool get_active_hoop_dimensions(
    const EditorState *state,
    int32_t *width_units_out,
    int32_t *height_units_out
) {
    if (state->hoop_preset_index == 0U || state->hoop_preset_index >= HOOP_PRESET_COUNT) {
        return false;
    }
    const HoopPreset preset = janome_550e_hoops[state->hoop_preset_index];
    if (state->hoop_rotated) {
        *width_units_out = preset.height_units;
        *height_units_out = preset.width_units;
    } else {
        *width_units_out = preset.width_units;
        *height_units_out = preset.height_units;
    }
    return true;
}

static bool design_exceeds_selected_hoop(const EditorState *state) {
    if (!state->design.has_bounds) {
        return false;
    }
    int32_t hoop_width = 0;
    int32_t hoop_height = 0;
    if (!get_active_hoop_dimensions(state, &hoop_width, &hoop_height)) {
        return false;
    }
    const int32_t half_width = hoop_width / 2;
    const int32_t half_height = hoop_height / 2;
    return state->design.minimum_x < -half_width ||
           state->design.maximum_x > half_width ||
           state->design.minimum_y < -half_height ||
           state->design.maximum_y > half_height;
}

static bool tool_uses_box_preview(EditorTool tool) {
    return tool == TOOL_CROP_KEEP ||
           tool == TOOL_CUT_OUT ||
           tool == TOOL_SPLIT_VERTICAL ||
           tool == TOOL_SPLIT_HORIZONTAL;
}

static EditOperationKind tool_to_edit_operation(EditorTool tool) {
    if (tool == TOOL_CUT_OUT) {
        return EDIT_CUT_OUT;
    }
    if (tool == TOOL_SPLIT_VERTICAL) {
        return EDIT_SPLIT_VERTICAL;
    }
    if (tool == TOOL_SPLIT_HORIZONTAL) {
        return EDIT_SPLIT_HORIZONTAL;
    }
    return EDIT_CROP_KEEP;
}

static void refresh_live_preview(EditorState *state) {
    if (!state->preview_dirty) {
        return;
    }
    state->preview_dirty = false;

    if (!state->has_box_selection || !tool_uses_box_preview(state->active_tool)) {
        state->has_edit_preview_mask = false;
        return;
    }

    const size_t total_segments = count_design_segments(&state->design);
    if (total_segments == 0U) {
        state->has_edit_preview_mask = false;
        return;
    }

    if (state->edit_preview_keep_mask == nullptr || state->edit_preview_mask_length != total_segments) {
        free(state->edit_preview_keep_mask);
        state->edit_preview_keep_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
        state->edit_preview_mask_length = total_segments;
    }
    if (state->edit_preview_keep_mask == nullptr) {
        state->edit_preview_mask_length = 0U;
        state->has_edit_preview_mask = false;
        return;
    }

    const CutRectangle rectangle = {
        state->box_start_x,
        state->box_start_y,
        state->box_end_x,
        state->box_end_y
    };
    const EditOperationKind operation = tool_to_edit_operation(state->active_tool);
    state->has_edit_preview_mask = build_edit_segment_mask(
        &state->design,
        state->grid,
        operation,
        rectangle,
        state->split_coordinate,
        false,
        state->edit_preview_keep_mask,
        state->edit_preview_mask_length
    );
}

static const char *basename_of_path(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash != nullptr && last_slash[1] != '\0') {
        return last_slash + 1;
    }
    return path;
}

static bool directory_exists(const char *directory_path) {
    struct stat path_info = {};
    if (stat(directory_path, &path_info) != 0) {
        return false;
    }
    return S_ISDIR(path_info.st_mode);
}

static bool file_has_embroidery_extension(const char *file_name) {
    const char *last_dot = strrchr(file_name, '.');
    if (last_dot == nullptr || last_dot[1] == '\0') {
        return false;
    }
    const char *extension = last_dot + 1;
    return strings_equal_ignore_case(extension, "jef") ||
           strings_equal_ignore_case(extension, "jef+") ||
           strings_equal_ignore_case(extension, "pes") ||
           strings_equal_ignore_case(extension, "pec") ||
           strings_equal_ignore_case(extension, "dst");
}

static int compare_file_browser_entries(const void *left_pointer, const void *right_pointer) {
    const FileBrowserEntry *left = (const FileBrowserEntry *)left_pointer;
    const FileBrowserEntry *right = (const FileBrowserEntry *)right_pointer;
    if (left->is_directory != right->is_directory) {
        if (left->is_directory) {
            return -1;
        }
        return 1;
    }
    size_t index = 0U;
    while (left->name[index] != '\0' && right->name[index] != '\0') {
        const unsigned char left_char = ascii_lowercase((unsigned char)left->name[index]);
        const unsigned char right_char = ascii_lowercase((unsigned char)right->name[index]);
        if (left_char < right_char) {
            return -1;
        }
        if (left_char > right_char) {
            return 1;
        }
        index += 1U;
    }
    if (left->name[index] == '\0' && right->name[index] == '\0') {
        return 0;
    }
    if (left->name[index] == '\0') {
        return -1;
    }
    return 1;
}

static bool file_dialog_append_entry(
    FileDialogState *dialog,
    const char *entry_name,
    bool is_directory,
    uint64_t file_size_bytes
) {
    if (dialog->entry_count >= dialog->entry_capacity) {
        size_t next_capacity = dialog->entry_capacity * 2U;
        if (next_capacity < 128U) {
            next_capacity = 128U;
        }
        size_t allocation_bytes = 0U;
        if (ckd_mul(&allocation_bytes, next_capacity, sizeof(FileBrowserEntry))) {
            return false;
        }
        FileBrowserEntry *grown = (FileBrowserEntry *)realloc(dialog->entries, allocation_bytes);
        if (grown == nullptr) {
            return false;
        }
        dialog->entries = grown;
        dialog->entry_capacity = next_capacity;
    }

    FileBrowserEntry *target = &dialog->entries[dialog->entry_count];
    *target = (FileBrowserEntry){};
    snprintf(target->name, sizeof(target->name), "%.255s", entry_name);
    target->is_directory = is_directory;
    target->file_size_bytes = file_size_bytes;
    dialog->entry_count += 1U;
    return true;
}

static bool load_file_browser_directory(EditorState *state, const char *directory_path) {
    FileDialogState *dialog = &state->file_dialog;
    DIR *directory_stream = opendir(directory_path);
    if (directory_stream == nullptr) {
        snprintf(dialog->error_text, sizeof(dialog->error_text), "Cannot open directory");
        state->needs_redraw = true;
        return false;
    }

    dialog->entry_count = 0U;
    dialog->selected_index = SIZE_MAX;
    dialog->scroll_offset = 0;
    dialog->confirm_overwrite = false;
    dialog->error_text[0] = '\0';

    const size_t raw_length = strlen(directory_path);
    size_t clean_length = raw_length;
    while (clean_length > 1U && directory_path[clean_length - 1U] == '/') {
        clean_length -= 1U;
    }
    const int path_written = snprintf(
        dialog->current_directory,
        sizeof(dialog->current_directory),
        "%.*s",
        (int)clean_length,
        directory_path
    );
    if (path_written < 0 || (size_t)path_written >= sizeof(dialog->current_directory)) {
        closedir(directory_stream);
        return false;
    }

    struct dirent *entry = readdir(directory_stream);
    while (entry != nullptr) {
        if (entry->d_name[0] != '.') {
            char full_path[MAX_PATH_BUFFER] = {};
            int joined = 0;
            if (strcmp(dialog->current_directory, "/") == 0) {
                joined = snprintf(full_path, sizeof(full_path), "/%s", entry->d_name);
            } else {
                joined = snprintf(full_path, sizeof(full_path), "%s/%s", dialog->current_directory, entry->d_name);
            }
            if (joined > 0 && (size_t)joined < sizeof(full_path)) {
                struct stat entry_stat = {};
                if (stat(full_path, &entry_stat) == 0) {
                    const bool is_dir = S_ISDIR(entry_stat.st_mode);
                    if (is_dir || file_has_embroidery_extension(entry->d_name)) {
                        file_dialog_append_entry(
                            dialog,
                            entry->d_name,
                            is_dir,
                            (uint64_t)entry_stat.st_size
                        );
                    }
                }
            }
        }
        entry = readdir(directory_stream);
    }

    closedir(directory_stream);
    if (dialog->entry_count > 1U) {
        qsort(dialog->entries, dialog->entry_count, sizeof(FileBrowserEntry), compare_file_browser_entries);
    }
    state->needs_redraw = true;
    return true;
}

static void close_file_dialog(EditorState *state) {
    if (state->file_dialog.mode == FILE_DIALOG_SAVE_AS ||
        state->file_dialog.mode == FILE_DIALOG_STRIP_SAVE_AS) {
        SDL_StopTextInput();
    }
    state->file_dialog.mode = FILE_DIALOG_CLOSED;
    state->file_dialog.confirm_overwrite = false;
    state->file_dialog.error_text[0] = '\0';
    state->needs_redraw = true;
}

static void open_file_dialog(EditorState *state, FileDialogMode mode) {
    FileDialogState *dialog = &state->file_dialog;
    dialog->mode = mode;
    dialog->confirm_overwrite = false;
    dialog->error_text[0] = '\0';

    char start_directory[MAX_PATH_BUFFER] = {};
    if (dialog->current_directory[0] != '\0' && directory_exists(dialog->current_directory)) {
        snprintf(start_directory, sizeof(start_directory), "%s", dialog->current_directory);
    } else if (state->has_file_path) {
        const char *last_slash = strrchr(state->current_file_path, '/');
        if (last_slash != nullptr) {
            size_t dir_length = (size_t)(last_slash - state->current_file_path);
            if (dir_length == 0U) {
                dir_length = 1U;
            }
            snprintf(start_directory, sizeof(start_directory), "%.*s", (int)dir_length, state->current_file_path);
        }
    }

    if (start_directory[0] == '\0' || !directory_exists(start_directory)) {
        const char *home_dir = getenv("HOME");
        if (home_dir != nullptr) {
            char downloads_dir[MAX_PATH_BUFFER] = {};
            const int dl_written = snprintf(downloads_dir, sizeof(downloads_dir), "%s/Downloads", home_dir);
            if (dl_written > 0 && (size_t)dl_written < sizeof(downloads_dir) && directory_exists(downloads_dir)) {
                snprintf(start_directory, sizeof(start_directory), "%s", downloads_dir);
            } else {
                snprintf(start_directory, sizeof(start_directory), "%s", home_dir);
            }
        } else if (getcwd(start_directory, sizeof(start_directory)) == nullptr) {
            snprintf(start_directory, sizeof(start_directory), "/");
        }
    }

    if (mode == FILE_DIALOG_SAVE_AS || mode == FILE_DIALOG_STRIP_SAVE_AS) {
        const char *default_name = "design_edited.jef";
        if (mode == FILE_DIALOG_STRIP_SAVE_AS) {
            default_name = "strip_design.jef";
        } else if (state->has_file_path) {
            const char *base_name = basename_of_path(state->current_file_path);
            const char *last_dot = strrchr(base_name, '.');
            size_t stem_len = strlen(base_name);
            if (last_dot != nullptr) {
                stem_len = (size_t)(last_dot - base_name);
            }
            if (stem_len > 180U) {
                stem_len = 180U;
            }
            snprintf(dialog->filename_input, sizeof(dialog->filename_input), "%.*s_edited.jef", (int)stem_len, base_name);
            default_name = nullptr;
        }
        if (default_name != nullptr) {
            snprintf(dialog->filename_input, sizeof(dialog->filename_input), "%s", default_name);
        }
        SDL_StartTextInput();
    } else {
        dialog->filename_input[0] = '\0';
    }

    load_file_browser_directory(state, start_directory);
}

static bool load_design_into_editor(
    EditorState *state,
    const char *file_path,
    int32_t window_width,
    int32_t window_height
) {
    StitchDesign loaded = {};
    if (!load_embroidery_file(file_path, &loaded)) {
        set_status_message(state, "Failed to load embroidery file");
        return false;
    }

    clear_segment_selection(state);
    clear_hover_preview(state);
    clear_floating_fragment(state);
    clear_live_preview(state);
    state->has_box_selection = false;
    state->is_dragging_box = false;

    free_stitch_design(&state->design);
    state->design = loaded;
    history_clear(&state->history);
    history_push(&state->history, &state->design);

    state->grid = detect_cross_stitch_grid(&state->design);
    state->active_color_block_index = 0U;
    state->color_scroll_offset = 0;
    state->simulator_percent = 100.0;

    const int path_written = snprintf(
        state->current_file_path,
        sizeof(state->current_file_path),
        "%s",
        file_path
    );
    state->has_file_path = (path_written > 0 && (size_t)path_written < sizeof(state->current_file_path));

    state->hoop_rotated = false;
    state->hoop_preset_index = 5U;
    for (size_t preset_index = 1U; preset_index < HOOP_PRESET_COUNT; preset_index += 1U) {
        const int32_t half_width = janome_550e_hoops[preset_index].width_units / 2;
        const int32_t half_height = janome_550e_hoops[preset_index].height_units / 2;
        if (state->design.minimum_x >= -half_width && state->design.maximum_x <= half_width &&
            state->design.minimum_y >= -half_height && state->design.maximum_y <= half_height) {
            state->hoop_preset_index = preset_index;
            break;
        }
    }

    fit_camera_to_design(state, window_width, window_height);

    char message[256] = {};
    const int message_written = snprintf(
        message,
        sizeof(message),
        "Loaded %.80s | Grid %.1fmm %dx",
        basename_of_path(file_path),
        (double)state->grid.cross_pitch_units * 0.1,
        state->grid.cross_passes
    );
    if (message_written > 0 && (size_t)message_written < sizeof(message)) {
        set_status_message(state, message);
    }
    return true;
}

static bool load_dropped_file_as_merge_fragment(
    EditorState *state,
    const char *file_path,
    int32_t window_width,
    int32_t window_height
) {
    StitchDesign fragment = {};
    if (!load_embroidery_file(file_path, &fragment)) {
        set_status_message(state, "Failed to load file for merge");
        return false;
    }

    clear_floating_fragment(state);
    clear_segment_selection(state);
    state->floating_fragment = fragment;
    state->has_floating_fragment = true;
    state->floating_is_grid_origin_aligned = true;

    const CrossGridInfo fragment_grid = detect_cross_stitch_grid(&state->floating_fragment);
    state->floating_base_origin_x = fragment_grid.grid_origin_x;
    state->floating_base_origin_y = fragment_grid.grid_origin_y;
    state->floating_anchor_x = 0;
    state->floating_anchor_y = 0;

    const int32_t cursor_world_x =
        screen_to_world_x(state, window_width, window_height, state->mouse_screen_x);
    const int32_t cursor_world_y =
        screen_to_world_y(state, window_width, window_height, state->mouse_screen_y);
    const int32_t pitch = state->grid.cross_pitch_units;
    const int32_t phase_shift_x = state->grid.grid_origin_x - state->floating_base_origin_x;
    const int32_t phase_shift_y = state->grid.grid_origin_y - state->floating_base_origin_y;
    state->floating_offset_x = snap_coordinate_to_grid(cursor_world_x, phase_shift_x, pitch);
    state->floating_offset_y = snap_coordinate_to_grid(cursor_world_y, phase_shift_y, pitch);

    char message[256] = {};
    const int written = snprintf(
        message,
        sizeof(message),
        "Merge %.70s: move/arrows, Click/Enter place, Esc cancel",
        basename_of_path(file_path)
    );
    if (written > 0 && (size_t)written < sizeof(message)) {
        set_status_message(state, message);
    }
    return true;
}

static void add_toolbar_button(
    EditorState *state,
    int32_t *cursor_x,
    int32_t *cursor_y,
    int32_t max_row_width,
    uint8_t command_id,
    const char *label,
    bool is_active,
    bool is_accent,
    bool is_readout
) {
    if (state->toolbar_button_count >= MAX_TOOLBAR_BUTTONS) {
        return;
    }
    const int32_t button_width = measure_text_2x_width(label) + TOOLBAR_BUTTON_PAD;
    if (*cursor_x > TOOLBAR_PADDING && *cursor_x + button_width > max_row_width - TOOLBAR_PADDING) {
        *cursor_x = TOOLBAR_PADDING;
        *cursor_y += TOOLBAR_BUTTON_HEIGHT + TOOLBAR_ROW_GAP;
    }

    UiButton *button = &state->toolbar_buttons[state->toolbar_button_count];
    *button = (UiButton){};
    button->left_x = *cursor_x;
    button->top_y = *cursor_y;
    button->width = button_width;
    button->height = TOOLBAR_BUTTON_HEIGHT;
    button->command_id = command_id;
    button->is_active = is_active;
    button->is_accent = is_accent;
    button->is_readout = is_readout;
    snprintf(button->label, sizeof(button->label), "%.31s", label);
    state->toolbar_button_count += 1U;

    *cursor_x += button_width + TOOLBAR_BUTTON_GAP;
}

static void layout_toolbar_buttons(EditorState *state, int32_t window_width) {
    state->toolbar_button_count = 0U;
    int32_t cursor_x = TOOLBAR_PADDING;
    int32_t cursor_y = TOOLBAR_PADDING;

    /* Row 1 (fits in 1 row on 1920px; auto-wraps cleanly on 1280px and 960px). */
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_OPEN_FILE_DIALOG, "Open", false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_MERGE_FILE_DIALOG, "Merge", false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_SAVE, "Save", false, true, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_SAVE_AS_DIALOG, "Save As", false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_UNDO, "Undo", false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_REDO, "Redo", false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOOL_SELECT_ELEMENT, "Sel Elem", state->active_tool == TOOL_SELECT_ELEMENT, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOOL_SELECT_CROSS, "Sel X", state->active_tool == TOOL_SELECT_CROSS, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOOL_DRAW_CROSS, "Draw X", state->active_tool == TOOL_DRAW_CROSS, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_OPEN_STRIP_MODAL, "Strip", state->strip_modal_open, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOOL_CROP, "Crop", state->active_tool == TOOL_CROP_KEEP, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOOL_CUT, "Cut", state->active_tool == TOOL_CUT_OUT, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOOL_SPLIT_V, "Split V", state->active_tool == TOOL_SPLIT_VERTICAL, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOOL_SPLIT_H, "Split H", state->active_tool == TOOL_SPLIT_HORIZONTAL, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOOL_CLONE, "Clone", state->active_tool == TOOL_CLONE, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOOL_PICK, "Pick", state->active_tool == TOOL_PICK_COLOR, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_DELETE_SELECTION, "Del", false, false, false);

    const char *view_label = "View:Thread";
    if (state->view_mode == VIEW_MODE_BLOCKS) {
        view_label = "View:Blocks";
    } else if (state->view_mode == VIEW_MODE_LINE) {
        view_label = "View:Line";
    }
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_CYCLE_VIEW_MODE, view_label, state->view_mode != VIEW_MODE_LINE, false, false);
    add_toolbar_button(
        state,
        &cursor_x,
        &cursor_y,
        window_width,
        COMMAND_CYCLE_THREAD_THICKNESS,
        thread_thickness_labels[state->thread_thickness_index],
        state->view_mode == VIEW_MODE_THREAD,
        false,
        false
    );

    /* Row 2 (explicit 2-row split on wide >=1600px screens; natural compact wrap on narrower screens). */
    if (window_width >= 1600) {
        cursor_x = TOOLBAR_PADDING;
        cursor_y += TOOLBAR_BUTTON_HEIGHT + TOOLBAR_ROW_GAP;
    }

    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_ROTATE_90, "Rot 90", false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_FLIP_X, "Flip X", false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_FLIP_Y, "Flip Y", false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_CENTER, "Center", false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_MERGE_COLORS, "Merge Colors", false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_RECOLOR_SELECTION, "Recolor", state->selected_segment_count > 0U, false, false);
    if (state->drop_merge_mode) {
        add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOGGLE_DROP_MODE, "Drop:Merge", true, false, false);
    } else {
        add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOGGLE_DROP_MODE, "Drop:Open", false, false, false);
    }

    char hoop_label[32] = {};
    snprintf(hoop_label, sizeof(hoop_label), "Hoop:%s", janome_550e_hoops[state->hoop_preset_index].label);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_CYCLE_HOOP, hoop_label, state->hoop_preset_index > 0U, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_ROTATE_HOOP, "Rot Hoop", state->hoop_rotated, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOGGLE_GRID, "Grid", state->show_grid, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOGGLE_JUMPS, "Jumps", state->show_jumps, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOGGLE_LOCKS, "Locks", state->show_locks, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_PITCH_DECREASE, "-", false, false, false);

    char pitch_label[32] = {};
    snprintf(pitch_label, sizeof(pitch_label), "%.1fmm", (double)state->grid.cross_pitch_units * 0.1);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_NONE, pitch_label, false, false, true);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_PITCH_INCREASE, "+", false, false, false);

    char passes_label[32] = {};
    snprintf(passes_label, sizeof(passes_label), "Pass:%dx", state->grid.cross_passes);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_CYCLE_PASSES, passes_label, false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_FIT_VIEW, "Fit", false, false, false);

    state->toolbar_height = cursor_y + TOOLBAR_BUTTON_HEIGHT + TOOLBAR_PADDING;
}

static StartScreenLayout compute_start_screen_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    int32_t viewport_left = 0;
    int32_t viewport_top = 0;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    get_viewport_bounds(
        state,
        window_width,
        window_height,
        &viewport_left,
        &viewport_top,
        &viewport_width,
        &viewport_height
    );

    StartScreenLayout layout = {};
    layout.card_width = 720;
    if (layout.card_width > viewport_width - 32) {
        layout.card_width = viewport_width - 32;
    }
    layout.card_height = 260;
    layout.card_left = viewport_left + (viewport_width - layout.card_width) / 2;
    layout.card_top = viewport_top + (viewport_height - layout.card_height) / 2;

    const int32_t button_width = 230;
    const int32_t button_height = 54;
    const int32_t button_gap = 28;
    const int32_t pair_left = layout.card_left + (layout.card_width - (button_width * 2 + button_gap)) / 2;
    const int32_t button_top = layout.card_top + 104;

    layout.open_button.left_x = pair_left;
    layout.open_button.top_y = button_top;
    layout.open_button.width = button_width;
    layout.open_button.height = button_height;
    layout.open_button.is_active = true;
    snprintf(layout.open_button.label, sizeof(layout.open_button.label), "Open File");

    layout.strip_button.left_x = pair_left + button_width + button_gap;
    layout.strip_button.top_y = button_top;
    layout.strip_button.width = button_width;
    layout.strip_button.height = button_height;
    layout.strip_button.is_accent = true;
    snprintf(layout.strip_button.label, sizeof(layout.strip_button.label), "New Strip");

    return layout;
}

static PaletteModalLayout compute_palette_modal_layout(int32_t window_width, int32_t window_height) {
    PaletteModalLayout layout = {};
    layout.columns = 10;
    layout.rows = 8;
    layout.cell_size = 58;
    layout.cell_gap = 6;

    const int32_t grid_width = layout.columns * layout.cell_size + (layout.columns - 1) * layout.cell_gap;
    const int32_t grid_height = layout.rows * layout.cell_size + (layout.rows - 1) * layout.cell_gap;
    layout.modal_width = grid_width + 40;
    layout.modal_height = grid_height + 116;
    layout.modal_left = (window_width - layout.modal_width) / 2;
    layout.modal_top = (window_height - layout.modal_height) / 2;
    if (layout.modal_left < 10) {
        layout.modal_left = 10;
    }
    if (layout.modal_top < 10) {
        layout.modal_top = 10;
    }
    layout.grid_left = layout.modal_left + 20;
    layout.grid_top = layout.modal_top + 48;
    return layout;
}

static void add_modal_button(
    UiButton *buttons,
    size_t *button_count,
    size_t capacity,
    int32_t left_x,
    int32_t top_y,
    int32_t width,
    int32_t height,
    uint8_t command_id,
    const char *label,
    bool is_active,
    bool is_accent,
    bool is_danger,
    bool is_readout,
    bool has_swatch,
    RgbColor swatch_color
) {
    if (*button_count >= capacity) {
        return;
    }
    UiButton *button = &buttons[*button_count];
    *button = (UiButton){};
    button->left_x = left_x;
    button->top_y = top_y;
    button->width = width;
    button->height = height;
    button->command_id = command_id;
    button->is_active = is_active;
    button->is_accent = is_accent;
    button->is_danger = is_danger;
    button->is_readout = is_readout;
    button->has_swatch = has_swatch;
    button->swatch_color = swatch_color;
    snprintf(button->label, sizeof(button->label), "%.31s", label);
    *button_count += 1U;
}

static StripModalLayout compute_strip_modal_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    StripModalLayout layout = {};

    layout.modal_width = window_width - 40;
    if (layout.modal_width > 1320) {
        layout.modal_width = 1320;
    }
    if (layout.modal_width < 880) {
        layout.modal_width = window_width - 16;
    }
    layout.modal_height = window_height - 48;
    if (layout.modal_height > 820) {
        layout.modal_height = 820;
    }
    if (layout.modal_height < 520) {
        layout.modal_height = window_height - 16;
    }
    layout.modal_left = (window_width - layout.modal_width) / 2;
    layout.modal_top = (window_height - layout.modal_height) / 2;

    const RgbColor no_swatch = {};
    const int32_t row_one_y = layout.modal_top + 42;
    int32_t cursor_x = layout.modal_left + 14;
    char value_text[32] = {};

    /* Row 1: W, H, RepX, RepY controls (fits <= 1240px cleanly). */
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 64, 30, STRIP_CMD_WIDTH_DEC, "W -", false, false, false, false, false, no_swatch);
    cursor_x += 68;
    snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.width);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 48, 30, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += 52;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 32, 30, STRIP_CMD_WIDTH_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += 44;

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 64, 30, STRIP_CMD_HEIGHT_DEC, "H -", false, false, false, false, false, no_swatch);
    cursor_x += 68;
    snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.height);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 48, 30, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += 52;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 32, 30, STRIP_CMD_HEIGHT_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += 44;

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 108, 30, STRIP_CMD_REPEAT_X_DEC, "RepX -", false, false, false, false, false, no_swatch);
    cursor_x += 112;
    snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.repeat_x);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 48, 30, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += 52;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 32, 30, STRIP_CMD_REPEAT_X_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += 44;

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 108, 30, STRIP_CMD_REPEAT_Y_DEC, "RepY -", false, false, false, false, false, no_swatch);
    cursor_x += 112;
    snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.repeat_y);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 48, 30, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += 52;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 32, 30, STRIP_CMD_REPEAT_Y_INC, "+", false, false, false, false, false, no_swatch);

    /* Row 2: Pitch, Passes (1x..6x), Color Picker, Clear, MirX, MirY, Undo. */
    const int32_t row_two_y = row_one_y + 36;
    cursor_x = layout.modal_left + 14;

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 108, 30, STRIP_CMD_PITCH_DEC, "Size -", false, false, false, false, false, no_swatch);
    cursor_x += 112;
    snprintf(value_text, sizeof(value_text), "%.1fmm", (double)state->strip_pattern.pitch_units * 0.1);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 92, 30, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += 96;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 32, 30, STRIP_CMD_PITCH_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += 42;

    snprintf(value_text, sizeof(value_text), "Pass:%dx", state->strip_pattern.passes);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 124, 30, STRIP_CMD_CYCLE_PASSES, value_text, false, false, false, false, false, no_swatch);
    cursor_x += 130;

    uint8_t active_color = state->strip_active_color;
    if (active_color == 0U || active_color >= JANOME_PALETTE_COUNT) {
        active_color = 10U;
    }
    snprintf(value_text, sizeof(value_text), "Col#%02u", (unsigned int)active_color);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 134, 30, STRIP_CMD_PICK_COLOR, value_text, false, false, false, false, true, janome_palette[active_color]);
    cursor_x += 140;

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 92, 30, STRIP_CMD_CLEAR, "Clear", false, false, false, false, false, no_swatch);
    cursor_x += 98;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 78, 30, STRIP_CMD_MIRROR_X, "MirX", false, false, false, false, false, no_swatch);
    cursor_x += 84;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 78, 30, STRIP_CMD_MIRROR_Y, "MirY", false, false, false, false, false, no_swatch);
    cursor_x += 84;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 78, 30, STRIP_CMD_UNDO, "Undo", false, false, false, false, false, no_swatch);

    /* Footer buttons: Insert, Save As, Save JEF, Close. */
    const int32_t footer_y = layout.modal_top + layout.modal_height - 42;
    const int32_t close_x = layout.modal_left + layout.modal_width - 106;
    const int32_t save_jef_x = close_x - 146;
    const int32_t save_as_x = save_jef_x - 130;
    const int32_t insert_x = save_as_x - 114;

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, insert_x, footer_y, 108, 32, STRIP_CMD_INSERT, "Insert", false, true, false, false, false, no_swatch);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, save_as_x, footer_y, 124, 32, STRIP_CMD_SAVE_AS, "Save As", false, false, false, false, false, no_swatch);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, save_jef_x, footer_y, 140, 32, STRIP_CMD_SAVE_JEF, "Save JEF", false, false, false, false, false, no_swatch);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, close_x, footer_y, 92, 32, STRIP_CMD_CLOSE, "Close", false, false, false, false, false, no_swatch);

    const int32_t panes_top = row_two_y + 40;
    const int32_t panes_height = footer_y - panes_top - 12;
    const int32_t inner_width = layout.modal_width - 42;
    layout.editor_left = layout.modal_left + 14;
    layout.editor_top = panes_top;
    layout.editor_width = (inner_width * 58) / 100;
    layout.editor_height = panes_height;

    layout.preview_left = layout.editor_left + layout.editor_width + 14;
    layout.preview_top = panes_top;
    layout.preview_width = inner_width - layout.editor_width;
    layout.preview_height = panes_height;

    const int32_t cell_width = (layout.editor_width - 16) / state->strip_pattern.width;
    const int32_t cell_height = (layout.editor_height - 16) / state->strip_pattern.height;
    int32_t cell_size = cell_width;
    if (cell_height < cell_size) {
        cell_size = cell_height;
    }
    if (cell_size < 6) {
        cell_size = 6;
    }
    if (cell_size > 54) {
        cell_size = 54;
    }
    layout.editor_cell_size = cell_size;
    return layout;
}

static FileDialogLayout compute_file_dialog_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    FileDialogLayout layout = {};
    layout.modal_width = window_width - 48;
    if (layout.modal_width > 1120) {
        layout.modal_width = 1120;
    }
    if (layout.modal_width < 760) {
        layout.modal_width = window_width - 16;
    }
    layout.modal_height = window_height - 56;
    if (layout.modal_height > 680) {
        layout.modal_height = 680;
    }
    if (layout.modal_height < 420) {
        layout.modal_height = window_height - 16;
    }
    layout.modal_left = (window_width - layout.modal_width) / 2;
    layout.modal_top = (window_height - layout.modal_height) / 2;

    const RgbColor no_swatch = {};
    const int32_t nav_y = layout.modal_top + 42;
    int32_t nav_x = layout.modal_left + 16;

    add_modal_button(layout.buttons, &layout.button_count, MAX_DIALOG_BUTTONS, nav_x, nav_y, 56, 30, DIALOG_CMD_PARENT, "..", false, false, false, false, false, no_swatch);
    nav_x += 62;
    add_modal_button(layout.buttons, &layout.button_count, MAX_DIALOG_BUTTONS, nav_x, nav_y, 82, 30, DIALOG_CMD_HOME, "Home", false, false, false, false, false, no_swatch);
    nav_x += 88;
    add_modal_button(layout.buttons, &layout.button_count, MAX_DIALOG_BUTTONS, nav_x, nav_y, 162, 30, DIALOG_CMD_DOWNLOADS, "Downloads", false, false, false, false, false, no_swatch);

    const bool is_save_mode =
        state->file_dialog.mode == FILE_DIALOG_SAVE_AS ||
        state->file_dialog.mode == FILE_DIALOG_STRIP_SAVE_AS;

    const int32_t footer_y = layout.modal_top + layout.modal_height - 46;
    const int32_t cancel_x = layout.modal_left + layout.modal_width - 128;
    const char *confirm_label = "Open";
    if (state->file_dialog.mode == FILE_DIALOG_MERGE) {
        confirm_label = "Merge";
    } else if (is_save_mode) {
        if (state->file_dialog.confirm_overwrite) {
            confirm_label = "Overwrite";
        } else {
            confirm_label = "Save";
        }
    }
    const int32_t confirm_width = measure_text_2x_width(confirm_label) + 22;
    const int32_t confirm_x = cancel_x - confirm_width - 10;

    add_modal_button(
        layout.buttons,
        &layout.button_count,
        MAX_DIALOG_BUTTONS,
        confirm_x,
        footer_y,
        confirm_width,
        34,
        DIALOG_CMD_CONFIRM,
        confirm_label,
        !is_save_mode,
        is_save_mode && !state->file_dialog.confirm_overwrite,
        is_save_mode && state->file_dialog.confirm_overwrite,
        false,
        false,
        no_swatch
    );
    add_modal_button(
        layout.buttons,
        &layout.button_count,
        MAX_DIALOG_BUTTONS,
        cancel_x,
        footer_y,
        112,
        34,
        DIALOG_CMD_CANCEL,
        "Cancel",
        false,
        false,
        false,
        false,
        false,
        no_swatch
    );

    layout.list_left = layout.modal_left + 16;
    layout.list_top = nav_y + 68;
    layout.list_width = layout.modal_width - 32;
    if (is_save_mode) {
        layout.input_left = layout.modal_left + 96;
        layout.input_top = footer_y;
        layout.input_width = confirm_x - layout.input_left - 12;
        layout.input_height = 34;
        layout.list_height = footer_y - layout.list_top - 36;
    } else {
        layout.list_height = footer_y - layout.list_top - 14;
    }
    return layout;
}

static void draw_cross_grid_background(
    PixelCanvas *canvas,
    const EditorState *state,
    int32_t viewport_left,
    int32_t viewport_top,
    int32_t viewport_width,
    int32_t viewport_height
) {
    if (!state->show_grid) {
        return;
    }

    int32_t pitch = state->grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }

    int32_t visual_step = pitch;
    while ((double)visual_step * state->pixels_per_unit < 8.0 && visual_step < 5000) {
        if (visual_step == pitch) {
            visual_step = pitch * 5;
        } else {
            visual_step *= 2;
        }
    }

    const int32_t world_min_x = screen_to_world_x(state, canvas->width, canvas->height, viewport_left);
    const int32_t world_max_x =
        screen_to_world_x(state, canvas->width, canvas->height, viewport_left + viewport_width);
    const int32_t world_min_y = screen_to_world_y(state, canvas->width, canvas->height, viewport_top);
    const int32_t world_max_y =
        screen_to_world_y(state, canvas->width, canvas->height, viewport_top + viewport_height);

    const uint32_t minor_color = pack_rgb_pixel(226U, 229U, 234U);
    const uint32_t major_color = pack_rgb_pixel(204U, 210U, 220U);

    const int32_t start_column = floor_divide_int32(world_min_x - state->grid.grid_origin_x, visual_step);
    const int32_t end_column = floor_divide_int32(world_max_x - state->grid.grid_origin_x, visual_step) + 1;
    for (int32_t column = start_column; column <= end_column; column += 1) {
        const int32_t world_x = state->grid.grid_origin_x + column * visual_step;
        const int32_t screen_x = world_to_screen_x(state, canvas->width, canvas->height, world_x);
        uint32_t line_color = minor_color;
        if (column % 5 == 0) {
            line_color = major_color;
        }
        draw_line_clipped(canvas, screen_x, viewport_top, screen_x, viewport_top + viewport_height, line_color, 0);
    }

    const int32_t start_row = floor_divide_int32(world_min_y - state->grid.grid_origin_y, visual_step);
    const int32_t end_row = floor_divide_int32(world_max_y - state->grid.grid_origin_y, visual_step) + 1;
    for (int32_t row = start_row; row <= end_row; row += 1) {
        const int32_t world_y = state->grid.grid_origin_y + row * visual_step;
        const int32_t screen_y = world_to_screen_y(state, canvas->width, canvas->height, world_y);
        uint32_t line_color = minor_color;
        if (row % 5 == 0) {
            line_color = major_color;
        }
        draw_line_clipped(canvas, viewport_left, screen_y, viewport_left + viewport_width, screen_y, line_color, 0);
    }
}

static void draw_hoop_overlay(PixelCanvas *canvas, const EditorState *state) {
    int32_t hoop_width = 0;
    int32_t hoop_height = 0;
    if (!get_active_hoop_dimensions(state, &hoop_width, &hoop_height)) {
        return;
    }

    const int32_t left_x = world_to_screen_x(state, canvas->width, canvas->height, -hoop_width / 2);
    const int32_t right_x = world_to_screen_x(state, canvas->width, canvas->height, hoop_width / 2);
    const int32_t top_y = world_to_screen_y(state, canvas->width, canvas->height, -hoop_height / 2);
    const int32_t bottom_y = world_to_screen_y(state, canvas->width, canvas->height, hoop_height / 2);

    const bool exceeds = design_exceeds_selected_hoop(state);
    uint32_t hoop_color = pack_rgb_pixel(36U, 118U, 198U);
    if (exceeds) {
        hoop_color = pack_rgb_pixel(224U, 36U, 36U);
    }

    draw_rectangle_outline(canvas, left_x, top_y, right_x - left_x, bottom_y - top_y, hoop_color, 8);
    draw_rectangle_outline(canvas, left_x - 1, top_y - 1, right_x - left_x + 2, bottom_y - top_y + 2, hoop_color, 8);

    const int32_t origin_x = world_to_screen_x(state, canvas->width, canvas->height, 0);
    const int32_t origin_y = world_to_screen_y(state, canvas->width, canvas->height, 0);
    draw_line_clipped(canvas, origin_x - 10, origin_y, origin_x + 10, origin_y, hoop_color, 0);
    draw_line_clipped(canvas, origin_x, origin_y - 10, origin_x, origin_y + 10, hoop_color, 0);
}

static uint32_t make_dimmed_color(RgbColor color) {
    const uint8_t red = (uint8_t)((uint32_t)color.red / 5U + 190U);
    const uint8_t green = (uint8_t)((uint32_t)color.green / 5U + 190U);
    const uint8_t blue = (uint8_t)((uint32_t)color.blue / 5U + 190U);
    return pack_rgb_pixel(red, green, blue);
}

static uint32_t compute_rgb_luminance(RgbColor color) {
    return ((uint32_t)color.red * 299U + (uint32_t)color.green * 587U + (uint32_t)color.blue * 114U) / 1000U;
}

static void compute_thread_colors(
    RgbColor base_rgb,
    bool dimmed,
    uint32_t *core_color_out,
    uint32_t *outline_color_out
) {
    if (dimmed) {
        const uint8_t core_red = (uint8_t)((uint32_t)base_rgb.red / 5U + 190U);
        const uint8_t core_green = (uint8_t)((uint32_t)base_rgb.green / 5U + 190U);
        const uint8_t core_blue = (uint8_t)((uint32_t)base_rgb.blue / 5U + 190U);
        *core_color_out = pack_rgb_pixel(core_red, core_green, core_blue);
        *outline_color_out = pack_rgb_pixel(
            (uint8_t)((uint32_t)core_red * 86U / 100U),
            (uint8_t)((uint32_t)core_green * 86U / 100U),
            (uint8_t)((uint32_t)core_blue * 86U / 100U)
        );
        return;
    }

    const uint32_t luminance = compute_rgb_luminance(base_rgb);
    if (luminance < 45U) {
        /* Slightly lift very dark/black core so 1px black border separates crossing diagonals. */
        uint32_t lifted_red = (uint32_t)base_rgb.red + 26U;
        uint32_t lifted_green = (uint32_t)base_rgb.green + 26U;
        uint32_t lifted_blue = (uint32_t)base_rgb.blue + 30U;
        if (lifted_red > 255U) {
            lifted_red = 255U;
        }
        if (lifted_green > 255U) {
            lifted_green = 255U;
        }
        if (lifted_blue > 255U) {
            lifted_blue = 255U;
        }
        *core_color_out = pack_rgb_pixel((uint8_t)lifted_red, (uint8_t)lifted_green, (uint8_t)lifted_blue);
        *outline_color_out = pack_rgb_pixel(
            (uint8_t)((uint32_t)base_rgb.red / 3U),
            (uint8_t)((uint32_t)base_rgb.green / 3U),
            (uint8_t)((uint32_t)base_rgb.blue / 3U)
        );
        return;
    }

    *core_color_out = pack_rgb_pixel(base_rgb.red, base_rgb.green, base_rgb.blue);
    *outline_color_out = pack_rgb_pixel(
        (uint8_t)((uint32_t)base_rgb.red * 58U / 100U),
        (uint8_t)((uint32_t)base_rgb.green * 58U / 100U),
        (uint8_t)((uint32_t)base_rgb.blue * 58U / 100U)
    );
}

static int32_t compute_thread_stroke_px(const EditorState *state) {
    const double raw_width =
        thread_thickness_units[state->thread_thickness_index] * state->pixels_per_unit;
    if (raw_width < 1.15) {
        return 1;
    }
    if (raw_width < 2.4) {
        return 2;
    }
    int32_t stroke_px = (int32_t)llround(raw_width);
    if (stroke_px < 3) {
        stroke_px = 3;
    }
    if (stroke_px > MAX_THREAD_STROKE_PX) {
        stroke_px = MAX_THREAD_STROKE_PX;
    }
    return stroke_px;
}

static void draw_thread_capsule_clipped(
    PixelCanvas *canvas,
    int32_t start_x,
    int32_t start_y,
    int32_t end_x,
    int32_t end_y,
    int32_t stroke_px,
    uint32_t core_color,
    uint32_t outline_color
) {
    const float outer_radius = (float)stroke_px * 0.5F;
    const int32_t radius_pad = (stroke_px + 2) / 2;

    int32_t box_min_x = start_x;
    int32_t box_max_x = end_x;
    if (end_x < start_x) {
        box_min_x = end_x;
        box_max_x = start_x;
    }
    int32_t box_min_y = start_y;
    int32_t box_max_y = end_y;
    if (end_y < start_y) {
        box_min_y = end_y;
        box_max_y = start_y;
    }

    box_min_x -= radius_pad;
    box_max_x += radius_pad;
    box_min_y -= radius_pad;
    box_max_y += radius_pad;

    if (box_max_x < canvas->clip_min_x || box_min_x >= canvas->clip_max_x ||
        box_max_y < canvas->clip_min_y || box_min_y >= canvas->clip_max_y) {
        return;
    }

    if (box_min_x < canvas->clip_min_x) {
        box_min_x = canvas->clip_min_x;
    }
    if (box_max_x >= canvas->clip_max_x) {
        box_max_x = canvas->clip_max_x - 1;
    }
    if (box_min_y < canvas->clip_min_y) {
        box_min_y = canvas->clip_min_y;
    }
    if (box_max_y >= canvas->clip_max_y) {
        box_max_y = canvas->clip_max_y - 1;
    }

    float inner_radius = outer_radius - 1.0F;
    if (inner_radius < 0.5F) {
        inner_radius = 0.5F;
    }
    const float outer_radius_sq = outer_radius * outer_radius;
    const float inner_radius_sq = inner_radius * inner_radius;

    const float delta_x_float = (float)(end_x - start_x);
    const float delta_y_float = (float)(end_y - start_y);
    const float length_sq = delta_x_float * delta_x_float + delta_y_float * delta_y_float;
    const size_t stride = (size_t)canvas->width;

    if (length_sq < 0.25F) {
        for (int32_t pixel_y = box_min_y; pixel_y <= box_max_y; pixel_y += 1) {
            uint32_t *row_pixels = canvas->pixels + (size_t)pixel_y * stride;
            const float relative_y = (float)(pixel_y - start_y);
            const float relative_y_sq = relative_y * relative_y;
            float relative_x = (float)(box_min_x - start_x);
            for (int32_t pixel_x = box_min_x; pixel_x <= box_max_x; pixel_x += 1) {
                const float distance_sq = relative_x * relative_x + relative_y_sq;
                if (distance_sq <= inner_radius_sq) {
                    row_pixels[pixel_x] = core_color;
                } else if (distance_sq <= outer_radius_sq) {
                    row_pixels[pixel_x] = outline_color;
                }
                relative_x += 1.0F;
            }
        }
        return;
    }

    const float inverse_length_sq = 1.0F / length_sq;
    const float step_projection = delta_x_float * inverse_length_sq;
    const bool use_scanline_band = (end_y != start_y);
    float slope_x_per_y = 0.0F;
    float band_half_width = (float)(box_max_x - box_min_x + 1);
    if (use_scanline_band) {
        slope_x_per_y = delta_x_float / delta_y_float;
        band_half_width = outer_radius * sqrtf(length_sq) / fabsf(delta_y_float) + 1.5F;
    }

    float line_x = (float)start_x + (float)(box_min_y - start_y) * slope_x_per_y;
    for (int32_t pixel_y = box_min_y; pixel_y <= box_max_y; pixel_y += 1) {
        int32_t scan_min_x = box_min_x;
        int32_t scan_max_x = box_max_x;
        if (use_scanline_band) {
            const int32_t band_left = (int32_t)(line_x - band_half_width);
            const int32_t band_right = (int32_t)(line_x + band_half_width) + 1;
            if (band_left > scan_min_x) {
                scan_min_x = band_left;
            }
            if (band_right < scan_max_x) {
                scan_max_x = band_right;
            }
            line_x += slope_x_per_y;
        }
        if (scan_min_x > scan_max_x) {
            continue;
        }

        uint32_t *row_pixels = canvas->pixels + (size_t)pixel_y * stride;
        const float relative_y = (float)(pixel_y - start_y);
        float relative_x = (float)(scan_min_x - start_x);
        float raw_projection = (relative_x * delta_x_float + relative_y * delta_y_float) * inverse_length_sq;

        for (int32_t pixel_x = scan_min_x; pixel_x <= scan_max_x; pixel_x += 1) {
            float projection = raw_projection;
            if (projection < 0.0F) {
                projection = 0.0F;
            } else if (projection > 1.0F) {
                projection = 1.0F;
            }
            const float offset_x = relative_x - projection * delta_x_float;
            const float offset_y = relative_y - projection * delta_y_float;
            const float distance_sq = offset_x * offset_x + offset_y * offset_y;
            if (distance_sq <= inner_radius_sq) {
                row_pixels[pixel_x] = core_color;
            } else if (distance_sq <= outer_radius_sq) {
                row_pixels[pixel_x] = outline_color;
            }
            relative_x += 1.0F;
            raw_projection += step_projection;
        }
    }
}

static uint64_t hash_block_center(size_t color_block_index, int32_t center_x, int32_t center_y) {
    uint64_t state = 14695981039346656037ULL;
    state ^= (uint64_t)color_block_index;
    state *= 1099511628211ULL;
    state ^= (uint64_t)(uint32_t)center_x;
    state *= 1099511628211ULL;
    state ^= (uint64_t)(uint32_t)center_y;
    state *= 1099511628211ULL;
    return state;
}

static void reset_block_dedup_frame(EditorState *state) {
    if (state->block_dedup_table == nullptr) {
        return;
    }
    state->block_dedup_generation += 1U;
    if (state->block_dedup_generation == 0U) {
        memset(state->block_dedup_table, 0, BLOCK_DEDUP_CAPACITY * sizeof(BlockDedupSlot));
        state->block_dedup_generation = 1U;
    }
}

/* Returns true if this square should be drawn (new or upgraded visual state). */
static bool block_dedup_try_insert(
    EditorState *state,
    size_t color_block_index,
    int32_t center_x,
    int32_t center_y,
    uint8_t visual_flags
) {
    if (state->block_dedup_table == nullptr) {
        return true;
    }
    const size_t mask = BLOCK_DEDUP_CAPACITY - 1U;
    const uint32_t generation = state->block_dedup_generation;
    size_t slot_index = (size_t)hash_block_center(color_block_index, center_x, center_y) & mask;

    for (size_t probe = 0U; probe < 16U; probe += 1U) {
        BlockDedupSlot *slot = &state->block_dedup_table[slot_index];
        if (slot->generation != generation) {
            slot->color_block_index = color_block_index;
            slot->center_x = center_x;
            slot->center_y = center_y;
            slot->visual_flags = visual_flags;
            slot->generation = generation;
            return true;
        }
        if (slot->color_block_index == color_block_index &&
            slot->center_x == center_x &&
            slot->center_y == center_y) {
            if (visual_flags > slot->visual_flags) {
                slot->visual_flags = visual_flags;
                return true;
            }
            return false;
        }
        slot_index = (slot_index + 1U) & mask;
    }
    return true;
}

static bool block_dedup_contains_center(
    const EditorState *state,
    size_t color_block_index,
    int32_t center_x,
    int32_t center_y
) {
    if (state->block_dedup_table == nullptr) {
        return false;
    }
    const size_t mask = BLOCK_DEDUP_CAPACITY - 1U;
    const uint32_t generation = state->block_dedup_generation;
    size_t slot_index = (size_t)hash_block_center(color_block_index, center_x, center_y) & mask;

    for (size_t probe = 0U; probe < 16U; probe += 1U) {
        const BlockDedupSlot *slot = &state->block_dedup_table[slot_index];
        if (slot->generation != generation) {
            return false;
        }
        if (slot->color_block_index == color_block_index &&
            slot->center_x == center_x &&
            slot->center_y == center_y) {
            return true;
        }
        slot_index = (slot_index + 1U) & mask;
    }
    return false;
}

static void draw_lock_marker(PixelCanvas *canvas, int32_t center_x, int32_t center_y) {
    draw_filled_rectangle(canvas, center_x - 3, center_y - 3, 7, 7, pack_rgb_pixel(236U, 42U, 42U));
    draw_rectangle_outline(canvas, center_x - 4, center_y - 4, 9, 9, pack_rgb_pixel(255U, 255U, 255U), 0);
}

static void draw_needle_tip_marker(PixelCanvas *canvas, int32_t needle_tip_x, int32_t needle_tip_y) {
    const uint32_t needle_color = pack_rgb_pixel(230U, 24U, 24U);
    draw_filled_rectangle(canvas, needle_tip_x - 4, needle_tip_y - 4, 9, 9, needle_color);
    draw_rectangle_outline(canvas, needle_tip_x - 6, needle_tip_y - 6, 13, 13, pack_rgb_pixel(255U, 255U, 255U), 0);
}

static void draw_styled_segment_thread_or_line(
    PixelCanvas *canvas,
    int32_t start_x,
    int32_t start_y,
    int32_t end_x,
    int32_t end_y,
    bool thread_mode,
    int32_t stroke_px,
    uint8_t visual_flags,
    uint32_t core_color,
    uint32_t outline_color,
    uint32_t dimmed_core,
    uint32_t dimmed_outline
) {
    const uint32_t selected_core = pack_rgb_pixel(255U, 152U, 0U);
    const uint32_t selected_outline = pack_rgb_pixel(156U, 78U, 0U);
    const uint32_t hover_core = pack_rgb_pixel(0U, 198U, 255U);
    const uint32_t hover_outline = pack_rgb_pixel(0U, 104U, 152U);

    if (visual_flags == 3U) {
        if (thread_mode && stroke_px >= 3) {
            draw_thread_capsule_clipped(canvas, start_x, start_y, end_x, end_y, stroke_px, selected_core, selected_outline);
        } else {
            draw_thick_line_clipped(canvas, start_x, start_y, end_x, end_y, selected_core);
        }
        return;
    }
    if (visual_flags == 2U) {
        if (thread_mode && stroke_px >= 3) {
            draw_thread_capsule_clipped(canvas, start_x, start_y, end_x, end_y, stroke_px, hover_core, hover_outline);
        } else {
            draw_thick_line_clipped(canvas, start_x, start_y, end_x, end_y, hover_core);
        }
        return;
    }

    uint32_t active_core = core_color;
    uint32_t active_outline = outline_color;
    if (visual_flags == 0U) {
        active_core = dimmed_core;
        active_outline = dimmed_outline;
    }

    if (!thread_mode || stroke_px <= 1) {
        draw_line_clipped(canvas, start_x, start_y, end_x, end_y, active_core, 0);
    } else if (stroke_px == 2) {
        draw_thick_line_clipped(canvas, start_x, start_y, end_x, end_y, active_core);
    } else {
        draw_thread_capsule_clipped(canvas, start_x, start_y, end_x, end_y, stroke_px, active_core, active_outline);
    }
}

static void render_design_line_or_thread(
    PixelCanvas *canvas,
    const EditorState *state,
    const StitchDesign *design,
    int32_t world_offset_x,
    int32_t world_offset_y,
    const uint8_t *keep_preview_mask,
    size_t keep_preview_mask_length,
    bool draw_overlays,
    const uint8_t *selection_mask,
    size_t selection_mask_length,
    const uint8_t *hover_mask,
    size_t hover_mask_length,
    size_t maximum_stitches_to_draw
) {
    int32_t viewport_left = 0;
    int32_t viewport_top = 0;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    get_viewport_bounds(state, canvas->width, canvas->height, &viewport_left, &viewport_top, &viewport_width, &viewport_height);

    const double scale = state->pixels_per_unit;
    const double origin_screen_x =
        (double)viewport_left + (double)viewport_width * 0.5 - state->camera_center_x * scale;
    const double origin_screen_y =
        (double)viewport_top + (double)viewport_height * 0.5 - state->camera_center_y * scale;

    const bool thread_mode = (state->view_mode == VIEW_MODE_THREAD);
    int32_t stroke_px = 1;
    if (thread_mode) {
        stroke_px = compute_thread_stroke_px(state);
    }
    const int32_t cull_margin = stroke_px + 2;
    const uint32_t jump_color = pack_rgb_pixel(158U, 102U, 204U);

    size_t previous_block_index = SIZE_MAX;
    bool has_previous_end = false;
    int32_t previous_end_x = 0;
    int32_t previous_end_y = 0;
    size_t flat_segment_index = 0U;
    size_t drawn_stitch_count = 0U;
    bool has_needle_tip = false;
    int32_t needle_tip_x = 0;
    int32_t needle_tip_y = 0;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t chain_segments = chain.point_count - 1U;
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled) {
            flat_segment_index += chain_segments;
            continue;
        }
        if (drawn_stitch_count >= maximum_stitches_to_draw) {
            break;
        }

        const RgbColor base_rgb = design->blocks[chain.color_block_index].display_color;
        uint32_t core_color = pack_rgb_pixel(base_rgb.red, base_rgb.green, base_rgb.blue);
        uint32_t outline_color = core_color;
        uint32_t dimmed_core = make_dimmed_color(base_rgb);
        uint32_t dimmed_outline = dimmed_core;
        if (thread_mode) {
            compute_thread_colors(base_rgb, false, &core_color, &outline_color);
            compute_thread_colors(base_rgb, true, &dimmed_core, &dimmed_outline);
        }

        const StitchPoint *chain_points = &design->points[chain.point_offset];
        const int32_t first_screen_x =
            fast_round_to_int32(origin_screen_x + (double)(chain_points[0].position_x + world_offset_x) * scale);
        const int32_t first_screen_y =
            fast_round_to_int32(origin_screen_y + (double)(chain_points[0].position_y + world_offset_y) * scale);

        if (draw_overlays && state->show_jumps && has_previous_end &&
            previous_block_index == chain.color_block_index) {
            draw_line_clipped(canvas, previous_end_x, previous_end_y, first_screen_x, first_screen_y, jump_color, 4);
        }

        int32_t start_x = first_screen_x;
        int32_t start_y = first_screen_y;
        bool has_last_drawn = false;
        int32_t last_first_x = 0;
        int32_t last_first_y = 0;
        int32_t last_second_x = 0;
        int32_t last_second_y = 0;
        uint8_t last_visual_flags = 255U;

        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (drawn_stitch_count >= maximum_stitches_to_draw) {
                break;
            }
            const StitchPoint next_point = chain_points[local_index + 1U];
            const int32_t end_x =
                fast_round_to_int32(origin_screen_x + (double)(next_point.position_x + world_offset_x) * scale);
            const int32_t end_y =
                fast_round_to_int32(origin_screen_y + (double)(next_point.position_y + world_offset_y) * scale);

            const size_t current_flat = flat_segment_index + local_index;
            uint8_t visual_flags = 1U;
            if (selection_mask != nullptr && current_flat < selection_mask_length && selection_mask[current_flat] != 0U) {
                visual_flags = 3U;
            } else if (hover_mask != nullptr && current_flat < hover_mask_length && hover_mask[current_flat] != 0U) {
                visual_flags = 2U;
            } else if (keep_preview_mask != nullptr && current_flat < keep_preview_mask_length && keep_preview_mask[current_flat] == 0U) {
                visual_flags = 0U;
            }

            /* Skip redundant consecutive back-and-forth passes on identical endpoints (5x speedup). */
            const bool same_as_last =
                has_last_drawn &&
                visual_flags == last_visual_flags &&
                ((start_x == last_first_x && start_y == last_first_y && end_x == last_second_x && end_y == last_second_y) ||
                 (start_x == last_second_x && start_y == last_second_y && end_x == last_first_x && end_y == last_first_y));

            const bool inside_viewport =
                !((start_x < canvas->clip_min_x - cull_margin && end_x < canvas->clip_min_x - cull_margin) ||
                  (start_x >= canvas->clip_max_x + cull_margin && end_x >= canvas->clip_max_x + cull_margin) ||
                  (start_y < canvas->clip_min_y - cull_margin && end_y < canvas->clip_min_y - cull_margin) ||
                  (start_y >= canvas->clip_max_y + cull_margin && end_y >= canvas->clip_max_y + cull_margin));

            if (!same_as_last && inside_viewport) {
                draw_styled_segment_thread_or_line(
                    canvas,
                    start_x,
                    start_y,
                    end_x,
                    end_y,
                    thread_mode,
                    stroke_px,
                    visual_flags,
                    core_color,
                    outline_color,
                    dimmed_core,
                    dimmed_outline
                );
                has_last_drawn = true;
                last_first_x = start_x;
                last_first_y = start_y;
                last_second_x = end_x;
                last_second_y = end_y;
                last_visual_flags = visual_flags;
            }

            start_x = end_x;
            start_y = end_y;
            needle_tip_x = end_x;
            needle_tip_y = end_y;
            has_needle_tip = true;
            drawn_stitch_count += 1U;
        }

        if (draw_overlays && state->show_locks) {
            if (chain.start_was_cut) {
                draw_lock_marker(canvas, first_screen_x, first_screen_y);
            }
            if (chain.end_was_cut && drawn_stitch_count < maximum_stitches_to_draw) {
                draw_lock_marker(canvas, start_x, start_y);
            }
        }

        previous_block_index = chain.color_block_index;
        previous_end_x = start_x;
        previous_end_y = start_y;
        has_previous_end = true;
        flat_segment_index += chain_segments;
    }

    if (draw_overlays && maximum_stitches_to_draw < SIZE_MAX && has_needle_tip) {
        draw_needle_tip_marker(canvas, needle_tip_x, needle_tip_y);
    }
}

static void draw_cross_block_square(
    PixelCanvas *canvas,
    int32_t left_x,
    int32_t top_y,
    int32_t right_x,
    int32_t bottom_y,
    RgbColor base_rgb,
    uint8_t visual_flags,
    bool leave_gap,
    bool draw_light_border
) {
    int32_t cell_width = right_x - left_x;
    int32_t cell_height = bottom_y - top_y;
    if (cell_width < 1) {
        cell_width = 1;
    }
    if (cell_height < 1) {
        cell_height = 1;
    }

    int32_t fill_width = cell_width;
    int32_t fill_height = cell_height;
    if (leave_gap && cell_width > 2 && cell_height > 2) {
        fill_width = cell_width - 1;
        fill_height = cell_height - 1;
    }

    uint32_t fill_color = pack_rgb_pixel(base_rgb.red, base_rgb.green, base_rgb.blue);
    if (visual_flags == 0U) {
        fill_color = make_dimmed_color(base_rgb);
    }
    draw_filled_rectangle(canvas, left_x, top_y, fill_width, fill_height, fill_color);

    if (draw_light_border && visual_flags == 1U && compute_rgb_luminance(base_rgb) > 215U &&
        fill_width >= 4 && fill_height >= 4) {
        draw_rectangle_outline(canvas, left_x, top_y, fill_width, fill_height, pack_rgb_pixel(192U, 198U, 208U), 0);
    }

    if (visual_flags == 3U) {
        draw_blended_rectangle(canvas, left_x, top_y, fill_width, fill_height, 255U, 152U, 0U, 88U);
        draw_rectangle_outline(canvas, left_x, top_y, fill_width, fill_height, pack_rgb_pixel(255U, 140U, 0U), 0);
        if (fill_width >= 6 && fill_height >= 6) {
            draw_rectangle_outline(canvas, left_x + 1, top_y + 1, fill_width - 2, fill_height - 2, pack_rgb_pixel(255U, 210U, 80U), 0);
        }
    } else if (visual_flags == 2U) {
        draw_blended_rectangle(canvas, left_x, top_y, fill_width, fill_height, 0U, 198U, 255U, 74U);
        draw_rectangle_outline(canvas, left_x, top_y, fill_width, fill_height, pack_rgb_pixel(0U, 168U, 240U), 0);
    }
}

static void draw_blocks_non_diagonal_segment(
    PixelCanvas *canvas,
    const EditorState *state,
    size_t color_block_index,
    StitchPoint start_point,
    StitchPoint end_point,
    int32_t start_x,
    int32_t start_y,
    int32_t end_x,
    int32_t end_y,
    int32_t pitch,
    uint8_t visual_flags,
    uint32_t line_color,
    uint32_t dimmed_color,
    bool draw_thick_outline
) {
    if ((start_x < canvas->clip_min_x - 2 && end_x < canvas->clip_min_x - 2) ||
        (start_x >= canvas->clip_max_x + 2 && end_x >= canvas->clip_max_x + 2) ||
        (start_y < canvas->clip_min_y - 2 && end_y < canvas->clip_min_y - 2) ||
        (start_y >= canvas->clip_max_y + 2 && end_y >= canvas->clip_max_y + 2)) {
        return;
    }

    int64_t abs_dx = (int64_t)end_point.position_x - (int64_t)start_point.position_x;
    if (abs_dx < 0LL) {
        abs_dx = -abs_dx;
    }
    int64_t abs_dy = (int64_t)end_point.position_y - (int64_t)start_point.position_y;
    if (abs_dy < 0LL) {
        abs_dy = -abs_dy;
    }
    const bool is_micro_lock = (abs_dx <= 5LL && abs_dy <= 5LL);
    if (is_micro_lock && !state->show_locks) {
        return;
    }

    if (is_cross_center_connector(start_point, end_point, pitch)) {
        const bool touches_drawn_center =
            block_dedup_contains_center(state, color_block_index, start_point.position_x, start_point.position_y) ||
            block_dedup_contains_center(state, color_block_index, end_point.position_x, end_point.position_y);
        if (touches_drawn_center) {
            return;
        }
    }

    uint32_t draw_color = line_color;
    bool force_thick = draw_thick_outline;
    if (visual_flags == 3U) {
        draw_color = pack_rgb_pixel(255U, 152U, 0U);
        force_thick = true;
    } else if (visual_flags == 2U) {
        draw_color = pack_rgb_pixel(0U, 198U, 255U);
        force_thick = true;
    } else if (visual_flags == 0U) {
        draw_color = dimmed_color;
    }

    if (force_thick) {
        draw_thick_line_clipped(canvas, start_x, start_y, end_x, end_y, draw_color);
    } else {
        draw_line_clipped(canvas, start_x, start_y, end_x, end_y, draw_color, 0);
    }
}

static void render_design_blocks(
    PixelCanvas *canvas,
    EditorState *state,
    const StitchDesign *design,
    int32_t world_offset_x,
    int32_t world_offset_y,
    const uint8_t *keep_preview_mask,
    size_t keep_preview_mask_length,
    bool draw_overlays,
    const uint8_t *selection_mask,
    size_t selection_mask_length,
    const uint8_t *hover_mask,
    size_t hover_mask_length,
    size_t maximum_stitches_to_draw
) {
    int32_t viewport_left = 0;
    int32_t viewport_top = 0;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    get_viewport_bounds(state, canvas->width, canvas->height, &viewport_left, &viewport_top, &viewport_width, &viewport_height);

    const double scale = state->pixels_per_unit;
    const double origin_screen_x =
        (double)viewport_left + (double)viewport_width * 0.5 - state->camera_center_x * scale;
    const double origin_screen_y =
        (double)viewport_top + (double)viewport_height * 0.5 - state->camera_center_y * scale;

    int32_t pitch = state->grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }
    const double half_pitch_world = (double)pitch * 0.5;
    const double cell_size_px = (double)pitch * scale;
    const bool leave_gap = (cell_size_px >= 6.0);
    const bool draw_light_border = (cell_size_px >= 4.5);
    reset_block_dedup_frame(state);

    /* Pass 1: Deduplicated cross-stitch filled squares with viewport culling. */
    size_t flat_segment_index = 0U;
    size_t drawn_stitch_count = 0U;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t chain_segments = chain.point_count - 1U;
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled) {
            flat_segment_index += chain_segments;
            continue;
        }
        if (drawn_stitch_count >= maximum_stitches_to_draw) {
            break;
        }

        const RgbColor base_rgb = design->blocks[chain.color_block_index].display_color;
        const StitchPoint *chain_points = &design->points[chain.point_offset];

        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (drawn_stitch_count >= maximum_stitches_to_draw) {
                break;
            }
            const StitchPoint start_point = chain_points[local_index];
            const StitchPoint end_point = chain_points[local_index + 1U];
            drawn_stitch_count += 1U;

            if (!is_cross_diagonal_for_pitch(start_point, end_point, pitch)) {
                continue;
            }

            const int32_t center_x =
                (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
            const int32_t center_y =
                (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);

            const double world_center_x = (double)(center_x + world_offset_x);
            const double world_center_y = (double)(center_y + world_offset_y);
            const int32_t left_x = fast_round_to_int32(origin_screen_x + (world_center_x - half_pitch_world) * scale);
            const int32_t right_x = fast_round_to_int32(origin_screen_x + (world_center_x + half_pitch_world) * scale);
            const int32_t top_y = fast_round_to_int32(origin_screen_y + (world_center_y - half_pitch_world) * scale);
            const int32_t bottom_y = fast_round_to_int32(origin_screen_y + (world_center_y + half_pitch_world) * scale);

            if (right_x < canvas->clip_min_x || left_x >= canvas->clip_max_x ||
                bottom_y < canvas->clip_min_y || top_y >= canvas->clip_max_y) {
                continue;
            }

            const size_t current_flat = flat_segment_index + local_index;
            uint8_t visual_flags = 1U;
            if (selection_mask != nullptr && current_flat < selection_mask_length && selection_mask[current_flat] != 0U) {
                visual_flags = 3U;
            } else if (hover_mask != nullptr && current_flat < hover_mask_length && hover_mask[current_flat] != 0U) {
                visual_flags = 2U;
            } else if (keep_preview_mask != nullptr && current_flat < keep_preview_mask_length && keep_preview_mask[current_flat] == 0U) {
                visual_flags = 0U;
            }

            if (block_dedup_try_insert(state, chain.color_block_index, center_x, center_y, visual_flags)) {
                draw_cross_block_square(
                    canvas,
                    left_x,
                    top_y,
                    right_x,
                    bottom_y,
                    base_rgb,
                    visual_flags,
                    leave_gap,
                    draw_light_border
                );
            }
        }
        flat_segment_index += chain_segments;
    }

    /* Pass 2: Non-diagonal backstitch/contour lines on top, plus jumps, locks, and needle marker. */
    const uint32_t jump_color = pack_rgb_pixel(158U, 102U, 204U);
    const bool draw_thick_outline = (scale >= 1.0);

    size_t previous_block_index = SIZE_MAX;
    bool has_previous_end = false;
    int32_t previous_end_x = 0;
    int32_t previous_end_y = 0;
    bool has_needle_tip = false;
    int32_t needle_tip_x = 0;
    int32_t needle_tip_y = 0;
    flat_segment_index = 0U;
    drawn_stitch_count = 0U;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t chain_segments = chain.point_count - 1U;
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled) {
            flat_segment_index += chain_segments;
            continue;
        }
        if (drawn_stitch_count >= maximum_stitches_to_draw) {
            break;
        }

        const RgbColor base_rgb = design->blocks[chain.color_block_index].display_color;
        const uint32_t line_color = pack_rgb_pixel(
            (uint8_t)((uint32_t)base_rgb.red * 78U / 100U),
            (uint8_t)((uint32_t)base_rgb.green * 78U / 100U),
            (uint8_t)((uint32_t)base_rgb.blue * 78U / 100U)
        );
        const uint32_t dimmed_color = make_dimmed_color(base_rgb);

        const StitchPoint *chain_points = &design->points[chain.point_offset];
        const int32_t first_screen_x =
            fast_round_to_int32(origin_screen_x + (double)(chain_points[0].position_x + world_offset_x) * scale);
        const int32_t first_screen_y =
            fast_round_to_int32(origin_screen_y + (double)(chain_points[0].position_y + world_offset_y) * scale);

        if (draw_overlays && state->show_jumps && has_previous_end &&
            previous_block_index == chain.color_block_index) {
            draw_line_clipped(canvas, previous_end_x, previous_end_y, first_screen_x, first_screen_y, jump_color, 4);
        }

        int32_t start_x = first_screen_x;
        int32_t start_y = first_screen_y;
        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (drawn_stitch_count >= maximum_stitches_to_draw) {
                break;
            }
            const StitchPoint start_point = chain_points[local_index];
            const StitchPoint end_point = chain_points[local_index + 1U];
            const int32_t end_x =
                fast_round_to_int32(origin_screen_x + (double)(end_point.position_x + world_offset_x) * scale);
            const int32_t end_y =
                fast_round_to_int32(origin_screen_y + (double)(end_point.position_y + world_offset_y) * scale);
            drawn_stitch_count += 1U;
            needle_tip_x = end_x;
            needle_tip_y = end_y;
            has_needle_tip = true;

            if (!is_cross_diagonal_for_pitch(start_point, end_point, pitch)) {
                const size_t current_flat = flat_segment_index + local_index;
                uint8_t visual_flags = 1U;
                if (selection_mask != nullptr && current_flat < selection_mask_length && selection_mask[current_flat] != 0U) {
                    visual_flags = 3U;
                } else if (hover_mask != nullptr && current_flat < hover_mask_length && hover_mask[current_flat] != 0U) {
                    visual_flags = 2U;
                } else if (keep_preview_mask != nullptr && current_flat < keep_preview_mask_length && keep_preview_mask[current_flat] == 0U) {
                    visual_flags = 0U;
                }
                draw_blocks_non_diagonal_segment(
                    canvas,
                    state,
                    chain.color_block_index,
                    start_point,
                    end_point,
                    start_x,
                    start_y,
                    end_x,
                    end_y,
                    pitch,
                    visual_flags,
                    line_color,
                    dimmed_color,
                    draw_thick_outline
                );
            }

            start_x = end_x;
            start_y = end_y;
        }

        if (draw_overlays && state->show_locks) {
            if (chain.start_was_cut) {
                draw_lock_marker(canvas, first_screen_x, first_screen_y);
            }
            if (chain.end_was_cut && drawn_stitch_count < maximum_stitches_to_draw) {
                draw_lock_marker(canvas, start_x, start_y);
            }
        }

        previous_block_index = chain.color_block_index;
        previous_end_x = start_x;
        previous_end_y = start_y;
        has_previous_end = true;
        flat_segment_index += chain_segments;
    }

    if (draw_overlays && maximum_stitches_to_draw < SIZE_MAX && has_needle_tip) {
        draw_needle_tip_marker(canvas, needle_tip_x, needle_tip_y);
    }
}

static void render_design_chains(
    PixelCanvas *canvas,
    EditorState *state,
    const StitchDesign *design,
    int32_t world_offset_x,
    int32_t world_offset_y,
    const uint8_t *keep_preview_mask,
    size_t keep_preview_mask_length,
    bool draw_overlays,
    const uint8_t *selection_mask,
    size_t selection_mask_length,
    const uint8_t *hover_mask,
    size_t hover_mask_length,
    size_t maximum_stitches_to_draw
) {
    if (state->view_mode == VIEW_MODE_BLOCKS) {
        render_design_blocks(
            canvas,
            state,
            design,
            world_offset_x,
            world_offset_y,
            keep_preview_mask,
            keep_preview_mask_length,
            draw_overlays,
            selection_mask,
            selection_mask_length,
            hover_mask,
            hover_mask_length,
            maximum_stitches_to_draw
        );
        return;
    }
    render_design_line_or_thread(
        canvas,
        state,
        design,
        world_offset_x,
        world_offset_y,
        keep_preview_mask,
        keep_preview_mask_length,
        draw_overlays,
        selection_mask,
        selection_mask_length,
        hover_mask,
        hover_mask_length,
        maximum_stitches_to_draw
    );
}

static bool should_show_start_screen(const EditorState *state) {
    return state->design.chain_count == 0U &&
           state->design.block_count == 0U &&
           !state->has_floating_fragment &&
           !state->strip_modal_open &&
           state->palette_target == PALETTE_TARGET_NONE &&
           state->file_dialog.mode == FILE_DIALOG_CLOSED;
}

static void render_start_screen(PixelCanvas *canvas, const EditorState *state) {
    if (!should_show_start_screen(state)) {
        return;
    }
    const StartScreenLayout layout = compute_start_screen_layout(state, canvas->width, canvas->height);
    draw_filled_rectangle(
        canvas,
        layout.card_left,
        layout.card_top,
        layout.card_width,
        layout.card_height,
        pack_rgb_pixel(36U, 40U, 48U)
    );
    draw_rectangle_outline(
        canvas,
        layout.card_left,
        layout.card_top,
        layout.card_width,
        layout.card_height,
        pack_rgb_pixel(110U, 148U, 204U),
        0
    );

    const char *title = "embroidery-studio (Janome MC550E)";
    const int32_t title_x = layout.card_left + (layout.card_width - measure_text_2x_width(title)) / 2;
    draw_bitmap_text_2x(canvas, title_x, layout.card_top + 36, title, pack_rgb_pixel(242U, 246U, 252U));

    draw_ui_button(canvas, &layout.open_button, state->mouse_screen_x, state->mouse_screen_y);
    draw_ui_button(canvas, &layout.strip_button, state->mouse_screen_x, state->mouse_screen_y);

    const char *hint = "Or drop .jef .pes .pec .dst here";
    const int32_t hint_x = layout.card_left + (layout.card_width - measure_text_2x_width(hint)) / 2;
    draw_bitmap_text_2x(canvas, hint_x, layout.card_top + 196, hint, pack_rgb_pixel(174U, 188U, 206U));
}

static void render_left_color_panel(PixelCanvas *canvas, const EditorState *state) {
    const int32_t panel_top = state->toolbar_height;
    const int32_t panel_height = canvas->height - panel_top - BOTTOM_PANEL_HEIGHT;
    draw_filled_rectangle(canvas, 0, panel_top, LEFT_PANEL_WIDTH, panel_height, pack_rgb_pixel(34U, 37U, 44U));
    draw_line_clipped(
        canvas,
        LEFT_PANEL_WIDTH - 1,
        panel_top,
        LEFT_PANEL_WIDTH - 1,
        panel_top + panel_height,
        pack_rgb_pixel(72U, 78U, 90U),
        0
    );

    UiButton add_button = {};
    add_button.left_x = 10;
    add_button.top_y = panel_top + 10;
    add_button.width = LEFT_PANEL_WIDTH - 20;
    add_button.height = 34;
    add_button.is_accent = true;
    snprintf(add_button.label, sizeof(add_button.label), "+ Color");
    draw_ui_button(canvas, &add_button, state->mouse_screen_x, state->mouse_screen_y);

    const int32_t list_top = panel_top + 54;
    const int32_t list_bottom = panel_top + panel_height - 8;
    canvas->clip_min_x = 0;
    canvas->clip_min_y = list_top;
    canvas->clip_max_x = LEFT_PANEL_WIDTH - 1;
    canvas->clip_max_y = list_bottom;

    if (state->design.block_count == 0U) {
        draw_bitmap_text_2x(canvas, 24, list_top + 16, "No colors yet", pack_rgb_pixel(150U, 158U, 172U));
        reset_canvas_clip(canvas);
        return;
    }

    for (size_t block_index = 0U; block_index < state->design.block_count; block_index += 1U) {
        const int32_t row_y = list_top + (int32_t)block_index * COLOR_ROW_HEIGHT - state->color_scroll_offset;
        if (row_y + COLOR_ROW_HEIGHT < list_top || row_y >= list_bottom) {
            continue;
        }

        const ColorBlock block = state->design.blocks[block_index];
        const bool is_active = (block_index == state->active_color_block_index);
        uint32_t row_background = pack_rgb_pixel(42U, 46U, 54U);
        if (is_active) {
            row_background = pack_rgb_pixel(54U, 82U, 118U);
        }
        draw_filled_rectangle(canvas, 8, row_y + 2, LEFT_PANEL_WIDTH - 16, COLOR_ROW_HEIGHT - 4, row_background);
        if (is_active) {
            draw_rectangle_outline(canvas, 8, row_y + 2, LEFT_PANEL_WIDTH - 16, COLOR_ROW_HEIGHT - 4, pack_rgb_pixel(128U, 190U, 255U), 0);
        }

        const int32_t swatch_x = 14;
        const int32_t swatch_y = row_y + 8;
        draw_filled_rectangle(
            canvas,
            swatch_x,
            swatch_y,
            24,
            24,
            pack_rgb_pixel(block.display_color.red, block.display_color.green, block.display_color.blue)
        );
        draw_rectangle_outline(canvas, swatch_x, swatch_y, 24, 24, pack_rgb_pixel(232U, 236U, 242U), 0);

        char label[32] = {};
        const int label_written = snprintf(
            label,
            sizeof(label),
            "%02zu J%02u",
            block_index + 1U,
            (unsigned int)block.janome_color_index
        );
        if (label_written < 0 || (size_t)label_written >= sizeof(label)) {
            label[0] = '\0';
        }
        uint32_t label_color = pack_rgb_pixel(240U, 242U, 248U);
        if (!block.enabled) {
            label_color = pack_rgb_pixel(128U, 134U, 144U);
        }
        draw_bitmap_text_2x(canvas, 44, row_y + 12, label, label_color);

        static const char *row_symbols[5] = {"V", "^", "v", "C", "X"};
        constexpr int32_t ROW_BUTTON_WIDTH = 34;
        constexpr int32_t ROW_BUTTON_HEIGHT = 28;
        constexpr int32_t ROW_BUTTON_GAP = 4;
        const int32_t buttons_start_x = LEFT_PANEL_WIDTH - 12 - (5 * ROW_BUTTON_WIDTH + 4 * ROW_BUTTON_GAP);
        const int32_t button_y = row_y + 6;

        for (int32_t action_index = 0; action_index < 5; action_index += 1) {
            const int32_t button_x = buttons_start_x + action_index * (ROW_BUTTON_WIDTH + ROW_BUTTON_GAP);
            const bool hovered = point_is_in_box(
                state->mouse_screen_x,
                state->mouse_screen_y,
                button_x,
                button_y,
                ROW_BUTTON_WIDTH,
                ROW_BUTTON_HEIGHT
            );
            uint32_t button_background = pack_rgb_pixel(60U, 65U, 75U);
            uint32_t text_color = pack_rgb_pixel(234U, 238U, 244U);
            const char *symbol = row_symbols[action_index];
            if (action_index == 0) {
                if (block.enabled) {
                    button_background = pack_rgb_pixel(42U, 118U, 72U);
                } else {
                    button_background = pack_rgb_pixel(46U, 48U, 54U);
                    symbol = "-";
                    text_color = pack_rgb_pixel(140U, 144U, 152U);
                }
            } else if (action_index == 4) {
                button_background = pack_rgb_pixel(132U, 48U, 48U);
            }
            if (hovered) {
                button_background = pack_rgb_pixel(88U, 102U, 124U);
            }
            draw_filled_rectangle(canvas, button_x, button_y, ROW_BUTTON_WIDTH, ROW_BUTTON_HEIGHT, button_background);
            draw_rectangle_outline(canvas, button_x, button_y, ROW_BUTTON_WIDTH, ROW_BUTTON_HEIGHT, pack_rgb_pixel(110U, 118U, 132U), 0);
            draw_bitmap_text_2x(canvas, button_x + 9, button_y + 6, symbol, text_color);
        }
    }

    reset_canvas_clip(canvas);
}

static void render_bottom_panel(PixelCanvas *canvas, EditorState *state) {
    const int32_t panel_top = canvas->height - BOTTOM_PANEL_HEIGHT;
    draw_filled_rectangle(canvas, 0, panel_top, canvas->width, BOTTOM_PANEL_HEIGHT, pack_rgb_pixel(28U, 31U, 37U));
    draw_line_clipped(canvas, 0, panel_top, canvas->width, panel_top, pack_rgb_pixel(72U, 78U, 90U), 0);

    const DesignStats stats = compute_design_stats(&state->design);

    const int32_t slider_row_y = panel_top + 8;
    char simulator_label[32] = {};
    const int simulator_written =
        snprintf(simulator_label, sizeof(simulator_label), "Sim:%3d%%", (int)llround(state->simulator_percent));
    if (simulator_written < 0 || (size_t)simulator_written >= sizeof(simulator_label)) {
        simulator_label[0] = '\0';
    }
    draw_bitmap_text_2x(canvas, 12, slider_row_y + 4, simulator_label, pack_rgb_pixel(210U, 224U, 244U));

    const size_t shown_stitches =
        (size_t)llround((double)stats.stitch_count * (state->simulator_percent / 100.0));
    char count_label[48] = {};
    const int count_written = snprintf(count_label, sizeof(count_label), "%zu/%zu", shown_stitches, stats.stitch_count);
    if (count_written < 0 || (size_t)count_written >= sizeof(count_label)) {
        count_label[0] = '\0';
    }
    const int32_t count_width = measure_text_2x_width(count_label);
    const int32_t count_x = canvas->width - count_width - 14;
    draw_bitmap_text_2x(canvas, count_x, slider_row_y + 4, count_label, pack_rgb_pixel(196U, 206U, 220U));

    state->slider_left_x = 12 + measure_text_2x_width(simulator_label) + 14;
    state->slider_right_x = count_x - 16;
    state->slider_top_y = slider_row_y + 2;
    state->slider_bottom_y = slider_row_y + 24;

    if (state->slider_right_x > state->slider_left_x + 40) {
        const int32_t track_width = state->slider_right_x - state->slider_left_x;
        const int32_t track_y = slider_row_y + 8;
        draw_filled_rectangle(canvas, state->slider_left_x, track_y, track_width, 10, pack_rgb_pixel(56U, 62U, 74U));
        const int32_t fill_width = (int32_t)llround((double)track_width * (state->simulator_percent / 100.0));
        draw_filled_rectangle(canvas, state->slider_left_x, track_y, fill_width, 10, pack_rgb_pixel(56U, 146U, 232U));
        draw_rectangle_outline(canvas, state->slider_left_x, track_y, track_width, 10, pack_rgb_pixel(104U, 116U, 136U), 0);

        const int32_t knob_x = state->slider_left_x + fill_width - 7;
        draw_filled_rectangle(canvas, knob_x, slider_row_y + 2, 14, 22, pack_rgb_pixel(236U, 242U, 250U));
        draw_rectangle_outline(canvas, knob_x, slider_row_y + 2, 14, 22, pack_rgb_pixel(32U, 88U, 152U), 0);
    }

    const int32_t line_two_y = panel_top + 42;
    double width_mm = 0.0;
    double height_mm = 0.0;
    if (state->design.has_bounds) {
        width_mm = (double)((int64_t)state->design.maximum_x - (int64_t)state->design.minimum_x) * 0.1;
        height_mm = (double)((int64_t)state->design.maximum_y - (int64_t)state->design.minimum_y) * 0.1;
    }
    const double cursor_x_mm =
        (double)screen_to_world_x(state, canvas->width, canvas->height, state->mouse_screen_x) * 0.1;
    const double cursor_y_mm =
        (double)screen_to_world_y(state, canvas->width, canvas->height, state->mouse_screen_y) * 0.1;

    char metrics_line[256] = {};
    snprintf(
        metrics_line,
        sizeof(metrics_line),
        "%.1fx%.1fmm St:%zu C:%zu J:%zu %.1f,%.1f G:%.1fmm %dx",
        width_mm,
        height_mm,
        stats.stitch_count,
        stats.color_count,
        stats.jump_count,
        cursor_x_mm,
        cursor_y_mm,
        (double)state->grid.cross_pitch_units * 0.1,
        state->grid.cross_passes
    );
    const bool out_of_hoop = design_exceeds_selected_hoop(state);
    int32_t metrics_clip_max_x = canvas->width - 12;
    if (out_of_hoop) {
        const char *warning_text = "OUT OF HOOP!";
        const int32_t badge_width = measure_text_2x_width(warning_text) + 12;
        const int32_t badge_x = canvas->width - badge_width - 12;
        metrics_clip_max_x = badge_x - 8;
        draw_filled_rectangle(canvas, badge_x, line_two_y - 4, badge_width, 24, pack_rgb_pixel(210U, 28U, 28U));
        draw_rectangle_outline(canvas, badge_x, line_two_y - 4, badge_width, 24, pack_rgb_pixel(255U, 196U, 196U), 0);
        draw_bitmap_text_2x(canvas, badge_x + 6, line_two_y, warning_text, pack_rgb_pixel(255U, 255U, 255U));
    }

    canvas->clip_max_x = metrics_clip_max_x;
    draw_bitmap_text_2x(canvas, 12, line_two_y, metrics_line, pack_rgb_pixel(234U, 238U, 244U));
    canvas->clip_max_x = canvas->width - 12;

    const int32_t line_three_y = panel_top + 74;
    draw_bitmap_text_2x(canvas, 12, line_three_y, state->status_message, pack_rgb_pixel(154U, 214U, 255U));
    reset_canvas_clip(canvas);
}

static void render_palette_modal(PixelCanvas *canvas, EditorState *state) {
    if (state->palette_target == PALETTE_TARGET_NONE) {
        return;
    }

    draw_blended_rectangle(canvas, 0, 0, canvas->width, canvas->height, 10U, 12U, 16U, 165U);
    const PaletteModalLayout layout = compute_palette_modal_layout(canvas->width, canvas->height);
    draw_filled_rectangle(canvas, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height, pack_rgb_pixel(36U, 40U, 48U));
    draw_rectangle_outline(canvas, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height, pack_rgb_pixel(130U, 170U, 224U), 0);

    draw_bitmap_text_2x(canvas, layout.modal_left + 20, layout.modal_top + 14, "Janome 79-Color Palette (Esc)", pack_rgb_pixel(245U, 248U, 252U));

    state->hovered_palette_index = -1;
    for (int32_t palette_index = 0; palette_index < (int32_t)JANOME_PALETTE_COUNT; palette_index += 1) {
        const int32_t column = palette_index % layout.columns;
        const int32_t row = palette_index / layout.columns;
        const int32_t cell_x = layout.grid_left + column * (layout.cell_size + layout.cell_gap);
        const int32_t cell_y = layout.grid_top + row * (layout.cell_size + layout.cell_gap);
        const RgbColor color = janome_palette[palette_index];
        const bool hovered = point_is_in_box(
            state->mouse_screen_x,
            state->mouse_screen_y,
            cell_x,
            cell_y,
            layout.cell_size,
            layout.cell_size
        );
        if (hovered) {
            state->hovered_palette_index = palette_index;
        }

        draw_filled_rectangle(canvas, cell_x, cell_y, layout.cell_size, layout.cell_size, pack_rgb_pixel(color.red, color.green, color.blue));
        uint32_t border_color = pack_rgb_pixel(90U, 96U, 108U);
        if (hovered) {
            border_color = pack_rgb_pixel(255U, 255U, 255U);
            draw_rectangle_outline(canvas, cell_x - 2, cell_y - 2, layout.cell_size + 4, layout.cell_size + 4, border_color, 0);
        }
        draw_rectangle_outline(canvas, cell_x, cell_y, layout.cell_size, layout.cell_size, border_color, 0);

        char index_text[16] = {};
        const int index_written = snprintf(index_text, sizeof(index_text), "%02d", palette_index);
        if (index_written < 0 || (size_t)index_written >= sizeof(index_text)) {
            index_text[0] = '\0';
        }
        const uint32_t luminance =
            ((uint32_t)color.red * 299U + (uint32_t)color.green * 587U + (uint32_t)color.blue * 114U) / 1000U;
        uint32_t label_color = pack_rgb_pixel(255U, 255U, 255U);
        if (luminance > 140U) {
            label_color = pack_rgb_pixel(16U, 18U, 22U);
        }
        draw_bitmap_text_2x(
            canvas,
            cell_x + (layout.cell_size - measure_text_2x_width(index_text)) / 2,
            cell_y + (layout.cell_size - FONT_GLYPH_HEIGHT) / 2,
            index_text,
            label_color
        );
    }

    const int32_t footer_y = layout.modal_top + layout.modal_height - 46;
    if (state->hovered_palette_index >= 0 && state->hovered_palette_index < (int32_t)JANOME_PALETTE_COUNT) {
        const RgbColor color = janome_palette[state->hovered_palette_index];
        draw_filled_rectangle(canvas, layout.modal_left + 20, footer_y, 34, 30, pack_rgb_pixel(color.red, color.green, color.blue));
        draw_rectangle_outline(canvas, layout.modal_left + 20, footer_y, 34, 30, pack_rgb_pixel(235U, 240U, 248U), 0);
        char info_text[64] = {};
        snprintf(
            info_text,
            sizeof(info_text),
            "Janome #%02d  #%02X%02X%02X",
            state->hovered_palette_index,
            (unsigned int)color.red,
            (unsigned int)color.green,
            (unsigned int)color.blue
        );
        draw_bitmap_text_2x(canvas, layout.modal_left + 66, footer_y + 7, info_text, pack_rgb_pixel(240U, 246U, 255U));
    } else {
        draw_bitmap_text_2x(canvas, layout.modal_left + 20, footer_y + 7, "Hover: #ID & HEX | Click: pick", pack_rgb_pixel(180U, 190U, 204U));
    }
}

static void render_strip_modal(PixelCanvas *canvas, const EditorState *state) {
    if (!state->strip_modal_open) {
        return;
    }

    draw_blended_rectangle(canvas, 0, 0, canvas->width, canvas->height, 10U, 12U, 16U, 175U);
    const StripModalLayout layout = compute_strip_modal_layout(state, canvas->width, canvas->height);
    draw_filled_rectangle(canvas, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height, pack_rgb_pixel(34U, 38U, 46U));
    draw_rectangle_outline(canvas, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height, pack_rgb_pixel(124U, 176U, 232U), 0);

    draw_bitmap_text_2x(
        canvas,
        layout.modal_left + 14,
        layout.modal_top + 12,
        "Strip Designer (LMB draw, RMB erase)",
        pack_rgb_pixel(242U, 246U, 252U)
    );

    for (size_t button_index = 0U; button_index < layout.button_count; button_index += 1U) {
        draw_ui_button(canvas, &layout.buttons[button_index], state->mouse_screen_x, state->mouse_screen_y);
    }

    draw_filled_rectangle(canvas, layout.editor_left, layout.editor_top, layout.editor_width, layout.editor_height, pack_rgb_pixel(246U, 247U, 250U));
    draw_rectangle_outline(canvas, layout.editor_left, layout.editor_top, layout.editor_width, layout.editor_height, pack_rgb_pixel(92U, 100U, 114U), 0);

    const int32_t grid_pixel_width = state->strip_pattern.width * layout.editor_cell_size;
    const int32_t grid_pixel_height = state->strip_pattern.height * layout.editor_cell_size;
    const int32_t grid_origin_x = layout.editor_left + (layout.editor_width - grid_pixel_width) / 2;
    const int32_t grid_origin_y = layout.editor_top + (layout.editor_height - grid_pixel_height) / 2;

    for (int32_t row = 0; row < state->strip_pattern.height; row += 1) {
        for (int32_t column = 0; column < state->strip_pattern.width; column += 1) {
            const int32_t cell_x = grid_origin_x + column * layout.editor_cell_size;
            const int32_t cell_y = grid_origin_y + row * layout.editor_cell_size;
            const uint8_t color_index = state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column];
            if (color_index > 0U && color_index < JANOME_PALETTE_COUNT) {
                const RgbColor color = janome_palette[color_index];
                const uint32_t packed = pack_rgb_pixel(color.red, color.green, color.blue);
                draw_thick_line_clipped(canvas, cell_x + 2, cell_y + 2, cell_x + layout.editor_cell_size - 3, cell_y + layout.editor_cell_size - 3, packed);
                draw_thick_line_clipped(canvas, cell_x + layout.editor_cell_size - 3, cell_y + 2, cell_x + 2, cell_y + layout.editor_cell_size - 3, packed);
            }
            draw_rectangle_outline(canvas, cell_x, cell_y, layout.editor_cell_size, layout.editor_cell_size, pack_rgb_pixel(204U, 210U, 220U), 0);
        }
    }

    draw_filled_rectangle(canvas, layout.preview_left, layout.preview_top, layout.preview_width, layout.preview_height, pack_rgb_pixel(242U, 244U, 248U));
    draw_rectangle_outline(canvas, layout.preview_left, layout.preview_top, layout.preview_width, layout.preview_height, pack_rgb_pixel(92U, 100U, 114U), 0);

    const int32_t total_columns = state->strip_pattern.width * state->strip_pattern.repeat_x;
    const int32_t total_rows = state->strip_pattern.height * state->strip_pattern.repeat_y;
    const double scale_x = (double)(layout.preview_width - 20) / (double)total_columns;
    const double scale_y = (double)(layout.preview_height - 20) / (double)total_rows;
    double preview_cell = scale_x;
    if (scale_y < preview_cell) {
        preview_cell = scale_y;
    }
    if (preview_cell > 28.0) {
        preview_cell = 28.0;
    }

    const int32_t preview_total_width = (int32_t)llround((double)total_columns * preview_cell);
    const int32_t preview_total_height = (int32_t)llround((double)total_rows * preview_cell);
    const int32_t preview_start_x = layout.preview_left + (layout.preview_width - preview_total_width) / 2;
    const int32_t preview_start_y = layout.preview_top + (layout.preview_height - preview_total_height) / 2;

    for (int32_t row = 0; row < total_rows; row += 1) {
        const int32_t source_row = row % state->strip_pattern.height;
        const int32_t top_edge = preview_start_y + (int32_t)llround((double)row * preview_cell);
        const int32_t bottom_edge = preview_start_y + (int32_t)llround((double)(row + 1) * preview_cell);
        for (int32_t column = 0; column < total_columns; column += 1) {
            const int32_t source_column = column % state->strip_pattern.width;
            const uint8_t color_index = state->strip_pattern.cells[source_row * STRIP_MAX_GRID_DIM + source_column];
            if (color_index == 0U || color_index >= JANOME_PALETTE_COUNT) {
                continue;
            }
            const int32_t left_edge = preview_start_x + (int32_t)llround((double)column * preview_cell);
            const int32_t right_edge = preview_start_x + (int32_t)llround((double)(column + 1) * preview_cell);
            const RgbColor color = janome_palette[color_index];
            const uint32_t packed = pack_rgb_pixel(color.red, color.green, color.blue);
            if (preview_cell >= 5.0) {
                draw_line_clipped(canvas, left_edge, top_edge, right_edge - 1, bottom_edge - 1, packed, 0);
                draw_line_clipped(canvas, right_edge - 1, top_edge, left_edge, bottom_edge - 1, packed, 0);
            } else {
                int32_t fill_width = right_edge - left_edge;
                int32_t fill_height = bottom_edge - top_edge;
                if (fill_width < 1) {
                    fill_width = 1;
                }
                if (fill_height < 1) {
                    fill_height = 1;
                }
                draw_filled_rectangle(canvas, left_edge, top_edge, fill_width, fill_height, packed);
            }
        }
    }

    const double final_width_mm = (double)(total_columns * state->strip_pattern.pitch_units) * 0.1;
    const double final_height_mm = (double)(total_rows * state->strip_pattern.pitch_units) * 0.1;
    char summary[96] = {};
    snprintf(summary, sizeof(summary), "%.1fx%.1fmm (%dx%d)", final_width_mm, final_height_mm, total_columns, total_rows);
    draw_bitmap_text_2x(canvas, layout.modal_left + 14, layout.modal_top + layout.modal_height - 34, summary, pack_rgb_pixel(204U, 232U, 255U));
}

static void render_file_dialog(PixelCanvas *canvas, const EditorState *state) {
    if (state->file_dialog.mode == FILE_DIALOG_CLOSED) {
        return;
    }

    draw_blended_rectangle(canvas, 0, 0, canvas->width, canvas->height, 8U, 10U, 14U, 185U);
    const FileDialogLayout layout = compute_file_dialog_layout(state, canvas->width, canvas->height);
    draw_filled_rectangle(canvas, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height, pack_rgb_pixel(34U, 38U, 46U));
    draw_rectangle_outline(canvas, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height, pack_rgb_pixel(124U, 176U, 232U), 0);

    const char *title = "Open Embroidery File";
    if (state->file_dialog.mode == FILE_DIALOG_MERGE) {
        title = "Merge Embroidery File";
    } else if (state->file_dialog.mode == FILE_DIALOG_SAVE_AS) {
        title = "Save Design As (.jef)";
    } else if (state->file_dialog.mode == FILE_DIALOG_STRIP_SAVE_AS) {
        title = "Save Strip As (.jef)";
    }
    draw_bitmap_text_2x(canvas, layout.modal_left + 16, layout.modal_top + 12, title, pack_rgb_pixel(245U, 248U, 252U));

    for (size_t index = 0U; index < layout.button_count; index += 1U) {
        draw_ui_button(canvas, &layout.buttons[index], state->mouse_screen_x, state->mouse_screen_y);
    }

    /* Current directory path bar (left-truncated cleanly to fit width). */
    const int32_t path_y = layout.modal_top + 80;
    size_t max_path_chars = (size_t)((layout.modal_width - 36) / FONT_GLYPH_WIDTH);
    if (max_path_chars > 250U) {
        max_path_chars = 250U;
    }
    const char *dir_text = state->file_dialog.current_directory;
    const size_t dir_len = strlen(dir_text);
    char visible_path[256] = {};
    if (dir_len <= max_path_chars) {
        snprintf(visible_path, sizeof(visible_path), "%.250s", dir_text);
    } else if (max_path_chars > 4U) {
        const size_t tail_chars = max_path_chars - 3U;
        snprintf(visible_path, sizeof(visible_path), "...%.250s", dir_text + (dir_len - tail_chars));
    }
    draw_bitmap_text_2x(canvas, layout.modal_left + 16, path_y, visible_path, pack_rgb_pixel(186U, 214U, 244U));

    /* Scrollable directory/file list. */
    draw_filled_rectangle(canvas, layout.list_left, layout.list_top, layout.list_width, layout.list_height, pack_rgb_pixel(24U, 27U, 33U));
    draw_rectangle_outline(canvas, layout.list_left, layout.list_top, layout.list_width, layout.list_height, pack_rgb_pixel(86U, 94U, 108U), 0);

    canvas->clip_min_x = layout.list_left + 1;
    canvas->clip_min_y = layout.list_top + 1;
    canvas->clip_max_x = layout.list_left + layout.list_width - 1;
    canvas->clip_max_y = layout.list_top + layout.list_height - 1;

    if (state->file_dialog.entry_count == 0U) {
        draw_bitmap_text_2x(canvas, layout.list_left + 16, layout.list_top + 14, "(Empty or no embroidery files)", pack_rgb_pixel(146U, 154U, 168U));
    } else {
        const int32_t size_column_width = 136;
        const size_t max_name_chars =
            (size_t)((layout.list_width - size_column_width - 28) / FONT_GLYPH_WIDTH);

        for (size_t entry_index = 0U; entry_index < state->file_dialog.entry_count; entry_index += 1U) {
            const int32_t row_y = layout.list_top + (int32_t)entry_index * FILE_ROW_HEIGHT - state->file_dialog.scroll_offset;
            if (row_y + FILE_ROW_HEIGHT < layout.list_top || row_y >= layout.list_top + layout.list_height) {
                continue;
            }

            const FileBrowserEntry *entry = &state->file_dialog.entries[entry_index];
            const bool is_selected = (entry_index == state->file_dialog.selected_index);
            const bool is_hovered = point_is_in_box(
                state->mouse_screen_x,
                state->mouse_screen_y,
                layout.list_left,
                row_y,
                layout.list_width,
                FILE_ROW_HEIGHT
            );
            if (is_selected) {
                draw_filled_rectangle(canvas, layout.list_left + 2, row_y + 1, layout.list_width - 4, FILE_ROW_HEIGHT - 2, pack_rgb_pixel(42U, 102U, 180U));
            } else if (is_hovered) {
                draw_filled_rectangle(canvas, layout.list_left + 2, row_y + 1, layout.list_width - 4, FILE_ROW_HEIGHT - 2, pack_rgb_pixel(44U, 50U, 62U));
            }

            char display_name[264] = {};
            if (entry->is_directory) {
                snprintf(display_name, sizeof(display_name), "[D] %.255s", entry->name);
            } else {
                snprintf(display_name, sizeof(display_name), "%.255s", entry->name);
            }
            if (strlen(display_name) > max_name_chars && max_name_chars > 3U) {
                display_name[max_name_chars - 3U] = '.';
                display_name[max_name_chars - 2U] = '.';
                display_name[max_name_chars - 1U] = '.';
                display_name[max_name_chars] = '\0';
            }

            uint32_t name_color = pack_rgb_pixel(234U, 238U, 244U);
            if (entry->is_directory) {
                name_color = pack_rgb_pixel(255U, 224U, 128U);
            }
            draw_bitmap_text_2x(canvas, layout.list_left + 10, row_y + 7, display_name, name_color);

            char size_text[32] = {};
            if (entry->is_directory) {
                snprintf(size_text, sizeof(size_text), "DIR");
            } else if (entry->file_size_bytes >= 1048576ULL) {
                snprintf(size_text, sizeof(size_text), "%.1fM", (double)entry->file_size_bytes / 1048576.0);
            } else if (entry->file_size_bytes >= 1024ULL) {
                snprintf(size_text, sizeof(size_text), "%.1fK", (double)entry->file_size_bytes / 1024.0);
            } else {
                snprintf(size_text, sizeof(size_text), "%lluB", (unsigned long long)entry->file_size_bytes);
            }
            const int32_t size_x = layout.list_left + layout.list_width - measure_text_2x_width(size_text) - 10;
            draw_bitmap_text_2x(canvas, size_x, row_y + 7, size_text, pack_rgb_pixel(164U, 176U, 192U));
        }
    }

    reset_canvas_clip(canvas);

    const bool is_save_mode =
        state->file_dialog.mode == FILE_DIALOG_SAVE_AS ||
        state->file_dialog.mode == FILE_DIALOG_STRIP_SAVE_AS;

    if (is_save_mode) {
        if (state->file_dialog.confirm_overwrite) {
            draw_bitmap_text_2x(
                canvas,
                layout.modal_left + 16,
                layout.input_top - 26,
                "File exists! Click Overwrite or Enter",
                pack_rgb_pixel(255U, 120U, 120U)
            );
        } else if (state->file_dialog.error_text[0] != '\0') {
            draw_bitmap_text_2x(
                canvas,
                layout.modal_left + 16,
                layout.input_top - 26,
                state->file_dialog.error_text,
                pack_rgb_pixel(255U, 140U, 120U)
            );
        }

        draw_bitmap_text_2x(canvas, layout.modal_left + 16, layout.input_top + 9, "Name:", pack_rgb_pixel(224U, 232U, 244U));
        draw_filled_rectangle(canvas, layout.input_left, layout.input_top, layout.input_width, layout.input_height, pack_rgb_pixel(20U, 22U, 28U));
        draw_rectangle_outline(canvas, layout.input_left, layout.input_top, layout.input_width, layout.input_height, pack_rgb_pixel(118U, 174U, 236U), 0);

        char visible_input[260] = {};
        const size_t max_input_chars = (size_t)((layout.input_width - 16) / FONT_GLYPH_WIDTH);
        const size_t input_len = strlen(state->file_dialog.filename_input);
        if (input_len + 1U <= max_input_chars) {
            snprintf(visible_input, sizeof(visible_input), "%s_", state->file_dialog.filename_input);
        } else if (max_input_chars > 2U) {
            const size_t tail = max_input_chars - 1U;
            snprintf(visible_input, sizeof(visible_input), "%s_", state->file_dialog.filename_input + (input_len - tail));
        }
        draw_bitmap_text_2x(canvas, layout.input_left + 8, layout.input_top + 9, visible_input, pack_rgb_pixel(246U, 250U, 255U));
    } else if (state->file_dialog.error_text[0] != '\0') {
        draw_bitmap_text_2x(canvas, layout.modal_left + 16, layout.modal_top + layout.modal_height - 38, state->file_dialog.error_text, pack_rgb_pixel(255U, 130U, 130U));
    }
}

static void render_frame(PixelCanvas *canvas, EditorState *state) {
    layout_toolbar_buttons(state, canvas->width);
    refresh_live_preview(state);

    reset_canvas_clip(canvas);
    draw_filled_rectangle(canvas, 0, 0, canvas->width, canvas->height, pack_rgb_pixel(245U, 246U, 248U));

    draw_filled_rectangle(canvas, 0, 0, canvas->width, state->toolbar_height, pack_rgb_pixel(30U, 33U, 40U));
    draw_line_clipped(canvas, 0, state->toolbar_height - 1, canvas->width, state->toolbar_height - 1, pack_rgb_pixel(72U, 78U, 90U), 0);
    for (size_t button_index = 0U; button_index < state->toolbar_button_count; button_index += 1U) {
        draw_ui_button(canvas, &state->toolbar_buttons[button_index], state->mouse_screen_x, state->mouse_screen_y);
    }

    int32_t viewport_left = 0;
    int32_t viewport_top = 0;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    get_viewport_bounds(state, canvas->width, canvas->height, &viewport_left, &viewport_top, &viewport_width, &viewport_height);

    canvas->clip_min_x = viewport_left;
    canvas->clip_min_y = viewport_top;
    canvas->clip_max_x = viewport_left + viewport_width;
    canvas->clip_max_y = viewport_top + viewport_height;

    draw_cross_grid_background(canvas, state, viewport_left, viewport_top, viewport_width, viewport_height);
    draw_hoop_overlay(canvas, state);

    const DesignStats total_stats = compute_design_stats(&state->design);
    size_t max_visible_stitches = SIZE_MAX;
    if (state->simulator_percent < 99.95) {
        max_visible_stitches = (size_t)llround((double)total_stats.stitch_count * (state->simulator_percent / 100.0));
    }

    if (should_show_start_screen(state)) {
        render_start_screen(canvas, state);
    } else if (state->design.chain_count > 0U) {
        const uint8_t *preview_mask = nullptr;
        size_t preview_mask_length = 0U;
        if (state->has_edit_preview_mask && state->edit_preview_keep_mask != nullptr) {
            preview_mask = state->edit_preview_keep_mask;
            preview_mask_length = state->edit_preview_mask_length;
        }
        render_design_chains(
            canvas,
            state,
            &state->design,
            0,
            0,
            preview_mask,
            preview_mask_length,
            true,
            state->selection_mask,
            state->selection_mask_length,
            state->hover_mask,
            state->hover_mask_length,
            max_visible_stitches
        );
    }

    if (state->has_floating_fragment) {
        render_design_chains(
            canvas,
            state,
            &state->floating_fragment,
            state->floating_offset_x,
            state->floating_offset_y,
            nullptr,
            0U,
            false,
            nullptr,
            0U,
            nullptr,
            0U,
            SIZE_MAX
        );
        if (state->floating_fragment.has_bounds) {
            const int32_t box_left = world_to_screen_x(state, canvas->width, canvas->height, state->floating_fragment.minimum_x + state->floating_offset_x);
            const int32_t box_right = world_to_screen_x(state, canvas->width, canvas->height, state->floating_fragment.maximum_x + state->floating_offset_x);
            const int32_t box_top = world_to_screen_y(state, canvas->width, canvas->height, state->floating_fragment.minimum_y + state->floating_offset_y);
            const int32_t box_bottom = world_to_screen_y(state, canvas->width, canvas->height, state->floating_fragment.maximum_y + state->floating_offset_y);
            draw_rectangle_outline(canvas, box_left, box_top, box_right - box_left + 1, box_bottom - box_top + 1, pack_rgb_pixel(32U, 156U, 90U), 6);
        }
    }

    const bool no_modal_open =
        !state->strip_modal_open &&
        state->palette_target == PALETTE_TARGET_NONE &&
        state->file_dialog.mode == FILE_DIALOG_CLOSED;
    const bool mouse_in_viewport = point_is_in_box(
        state->mouse_screen_x,
        state->mouse_screen_y,
        viewport_left,
        viewport_top,
        viewport_width,
        viewport_height
    );
    if (no_modal_open && !state->has_floating_fragment && mouse_in_viewport) {
        if (state->active_tool == TOOL_SELECT_CROSS && state->has_hover_cross_box) {
            const int32_t left_edge = world_to_screen_x(state, canvas->width, canvas->height, state->hover_cross_box.minimum_x);
            const int32_t right_edge = world_to_screen_x(state, canvas->width, canvas->height, state->hover_cross_box.maximum_x);
            const int32_t top_edge = world_to_screen_y(state, canvas->width, canvas->height, state->hover_cross_box.minimum_y);
            const int32_t bottom_edge = world_to_screen_y(state, canvas->width, canvas->height, state->hover_cross_box.maximum_y);
            draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, 0U, 180U, 255U, 54U);
            draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, pack_rgb_pixel(0U, 140U, 232U), 0);
        } else if (state->active_tool == TOOL_DRAW_CROSS) {
            const int32_t world_x = screen_to_world_x(state, canvas->width, canvas->height, state->mouse_screen_x);
            const int32_t world_y = screen_to_world_y(state, canvas->width, canvas->height, state->mouse_screen_y);
            int32_t cell_column = 0;
            int32_t cell_row = 0;
            world_to_grid_cell(world_x, world_y, state->grid, &cell_column, &cell_row);
            const int32_t pitch = state->grid.cross_pitch_units;
            const int32_t cell_min_x = state->grid.grid_origin_x + cell_column * pitch;
            const int32_t cell_min_y = state->grid.grid_origin_y + cell_row * pitch;
            const int32_t left_edge = world_to_screen_x(state, canvas->width, canvas->height, cell_min_x);
            const int32_t right_edge = world_to_screen_x(state, canvas->width, canvas->height, cell_min_x + pitch);
            const int32_t top_edge = world_to_screen_y(state, canvas->width, canvas->height, cell_min_y);
            const int32_t bottom_edge = world_to_screen_y(state, canvas->width, canvas->height, cell_min_y + pitch);
            RgbColor preview_rgb = janome_palette[10];
            if (state->active_color_block_index < state->design.block_count) {
                preview_rgb = state->design.blocks[state->active_color_block_index].display_color;
            }
            const uint32_t cross_color = pack_rgb_pixel(preview_rgb.red, preview_rgb.green, preview_rgb.blue);
            if (state->view_mode == VIEW_MODE_BLOCKS) {
                draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, preview_rgb.red, preview_rgb.green, preview_rgb.blue, 148U);
                draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, cross_color, 0);
            } else if (state->view_mode == VIEW_MODE_THREAD) {
                draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, preview_rgb.red, preview_rgb.green, preview_rgb.blue, 55U);
                uint32_t preview_core = cross_color;
                uint32_t preview_outline = cross_color;
                compute_thread_colors(preview_rgb, false, &preview_core, &preview_outline);
                const int32_t preview_stroke = compute_thread_stroke_px(state);
                if (preview_stroke >= 3) {
                    draw_thread_capsule_clipped(canvas, left_edge, top_edge, right_edge, bottom_edge, preview_stroke, preview_core, preview_outline);
                    draw_thread_capsule_clipped(canvas, right_edge, top_edge, left_edge, bottom_edge, preview_stroke, preview_core, preview_outline);
                } else {
                    draw_thick_line_clipped(canvas, left_edge, top_edge, right_edge, bottom_edge, preview_core);
                    draw_thick_line_clipped(canvas, right_edge, top_edge, left_edge, bottom_edge, preview_core);
                }
                draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, cross_color, 0);
            } else {
                draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, preview_rgb.red, preview_rgb.green, preview_rgb.blue, 70U);
                draw_thick_line_clipped(canvas, left_edge, top_edge, right_edge, bottom_edge, cross_color);
                draw_thick_line_clipped(canvas, right_edge, top_edge, left_edge, bottom_edge, cross_color);
                draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, cross_color, 0);
            }
        }
    }

    if (state->has_box_selection) {
        if (state->active_tool == TOOL_SPLIT_VERTICAL) {
            const int32_t line_x = world_to_screen_x(state, canvas->width, canvas->height, state->split_coordinate);
            draw_line_clipped(canvas, line_x, viewport_top, line_x, viewport_top + viewport_height, pack_rgb_pixel(230U, 52U, 36U), 6);
            draw_line_clipped(canvas, line_x + 1, viewport_top, line_x + 1, viewport_top + viewport_height, pack_rgb_pixel(230U, 52U, 36U), 6);
        } else if (state->active_tool == TOOL_SPLIT_HORIZONTAL) {
            const int32_t line_y = world_to_screen_y(state, canvas->width, canvas->height, state->split_coordinate);
            draw_line_clipped(canvas, viewport_left, line_y, viewport_left + viewport_width, line_y, pack_rgb_pixel(230U, 52U, 36U), 6);
            draw_line_clipped(canvas, viewport_left, line_y + 1, viewport_left + viewport_width, line_y + 1, pack_rgb_pixel(230U, 52U, 36U), 6);
        } else {
            int32_t left_edge = world_to_screen_x(state, canvas->width, canvas->height, state->box_start_x);
            int32_t right_edge = world_to_screen_x(state, canvas->width, canvas->height, state->box_end_x);
            int32_t top_edge = world_to_screen_y(state, canvas->width, canvas->height, state->box_start_y);
            int32_t bottom_edge = world_to_screen_y(state, canvas->width, canvas->height, state->box_end_y);
            if (right_edge < left_edge) {
                const int32_t swap_x = left_edge;
                left_edge = right_edge;
                right_edge = swap_x;
            }
            if (bottom_edge < top_edge) {
                const int32_t swap_y = top_edge;
                top_edge = bottom_edge;
                bottom_edge = swap_y;
            }
            uint8_t tint_red = 40U;
            uint8_t tint_green = 152U;
            uint8_t tint_blue = 228U;
            if (state->active_tool == TOOL_CUT_OUT) {
                tint_red = 224U;
                tint_green = 52U;
                tint_blue = 52U;
            } else if (state->active_tool == TOOL_CROP_KEEP || state->active_tool == TOOL_CLONE) {
                tint_red = 42U;
                tint_green = 172U;
                tint_blue = 92U;
            }
            draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, tint_red, tint_green, tint_blue, 36U);
            draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, pack_rgb_pixel(tint_red, tint_green, tint_blue), 0);
        }
    }

    reset_canvas_clip(canvas);
    render_left_color_panel(canvas, state);
    render_bottom_panel(canvas, state);
    render_strip_modal(canvas, state);
    render_file_dialog(canvas, state);
    render_palette_modal(canvas, state);
}

static void save_current_design(EditorState *state) {
    const char *base_path = "untitled.jef";
    if (state->has_file_path) {
        base_path = state->current_file_path;
    }

    if (state->has_box_selection &&
        (state->active_tool == TOOL_SPLIT_VERTICAL || state->active_tool == TOOL_SPLIT_HORIZONTAL)) {
        const CutRectangle empty_box = {};
        const EditOperationKind operation = tool_to_edit_operation(state->active_tool);
        StitchDesign part_one = {};
        StitchDesign part_two = {};
        const bool first_built = apply_edit_operation(&state->design, state->grid, operation, empty_box, state->split_coordinate, false, &part_one);
        const bool second_built = apply_edit_operation(&state->design, state->grid, operation, empty_box, state->split_coordinate, true, &part_two);
        if (!first_built || !second_built || part_one.chain_count == 0U || part_two.chain_count == 0U) {
            free_stitch_design(&part_one);
            free_stitch_design(&part_two);
            set_status_message(state, "Split produces an empty half; adjust line");
            return;
        }
        char part_one_path[MAX_PATH_BUFFER] = {};
        char part_two_path[MAX_PATH_BUFFER] = {};
        if (!build_unique_suffixed_jef_path(base_path, "_part1", part_one_path, sizeof(part_one_path)) ||
            !build_unique_suffixed_jef_path(base_path, "_part2", part_two_path, sizeof(part_two_path))) {
            free_stitch_design(&part_one);
            free_stitch_design(&part_two);
            set_status_message(state, "Output path error");
            return;
        }
        const bool first_saved = write_jef_file(&part_one, part_one_path);
        const bool second_saved = write_jef_file(&part_two, part_two_path);
        free_stitch_design(&part_one);
        free_stitch_design(&part_two);
        if (first_saved && second_saved) {
            char message[256] = {};
            snprintf(message, sizeof(message), "Saved %.60s & %.60s", basename_of_path(part_one_path), basename_of_path(part_two_path));
            set_status_message(state, message);
        } else {
            set_status_message(state, "Failed to export split JEF parts");
        }
        return;
    }

    char output_path[MAX_PATH_BUFFER] = {};
    if (!build_unique_suffixed_jef_path(base_path, "_edited", output_path, sizeof(output_path))) {
        set_status_message(state, "Output path error");
        return;
    }
    if (!write_jef_file(&state->design, output_path)) {
        set_status_message(state, "Failed to save JEF (design may be empty)");
        return;
    }
    char message[256] = {};
    snprintf(message, sizeof(message), "Saved %.120s", basename_of_path(output_path));
    set_status_message(state, message);
}

static void delete_active_selection(EditorState *state) {
    if (state->selection_mask == nullptr || state->selected_segment_count == 0U) {
        set_status_message(state, "Nothing selected to delete");
        return;
    }
    StitchDesign result = {};
    if (!delete_masked_segments(&state->design, state->selection_mask, state->selection_mask_length, &result)) {
        set_status_message(state, "Failed to delete selection");
        return;
    }
    free_stitch_design(&state->design);
    state->design = result;
    history_push(&state->history, &state->design);
    if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
        state->active_color_block_index = state->design.block_count - 1U;
    }
    clear_segment_selection(state);
    clear_hover_preview(state);
    set_status_message(state, "Deleted selection with tie-off locks");
}

static void commit_floating_fragment(EditorState *state, bool keep_stamping) {
    if (!state->has_floating_fragment) {
        return;
    }
    StitchDesign merged = {};
    if (!insert_design_fragment(
            &state->design,
            &state->floating_fragment,
            state->floating_offset_x,
            state->floating_offset_y,
            &merged
        )) {
        set_status_message(state, "Failed to insert fragment");
        return;
    }
    free_stitch_design(&state->design);
    state->design = merged;
    history_push(&state->history, &state->design);
    clear_hover_preview(state);
    if (!keep_stamping) {
        clear_floating_fragment(state);
    }
    set_status_message(state, "Inserted fragment into design");
}

static void open_strip_modal(EditorState *state) {
    state->strip_pattern.pitch_units = state->grid.cross_pitch_units;
    state->strip_pattern.passes = state->grid.cross_passes;
    if (state->active_color_block_index < state->design.block_count) {
        const uint8_t block_janome =
            state->design.blocks[state->active_color_block_index].janome_color_index;
        if (block_janome > 0U && block_janome < JANOME_PALETTE_COUNT) {
            state->strip_active_color = block_janome;
        }
    }
    state->strip_modal_open = true;
    state->needs_redraw = true;
}

static void execute_toolbar_command(
    EditorState *state,
    uint8_t command_id,
    int32_t window_width,
    int32_t window_height
) {
    switch (command_id) {
        case COMMAND_TOOL_SELECT_ELEMENT:
            state->active_tool = TOOL_SELECT_ELEMENT;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_hover_preview(state);
            set_status_message(state, "Sel Elem: Click select, Shift+Click add, RMB/Del delete, Arrows move");
            break;
        case COMMAND_TOOL_SELECT_CROSS:
            state->active_tool = TOOL_SELECT_CROSS;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_hover_preview(state);
            set_status_message(state, "Sel X: Click/drag cross, Shift+Click add, RMB/Del delete, Arrows move");
            break;
        case COMMAND_TOOL_DRAW_CROSS:
            state->active_tool = TOOL_DRAW_CROSS;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_hover_preview(state);
            set_status_message(state, "Draw X: LMB draw cross on grid, RMB erase cross");
            break;
        case COMMAND_OPEN_STRIP_MODAL:
            open_strip_modal(state);
            break;
        case COMMAND_TOOL_CROP:
            state->active_tool = TOOL_CROP_KEEP;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_segment_selection(state);
            set_status_message(state, "Crop: drag rectangle (Shift=grid snap), Enter to apply");
            break;
        case COMMAND_TOOL_CUT:
            state->active_tool = TOOL_CUT_OUT;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_segment_selection(state);
            set_status_message(state, "Cut: drag box (Shift=grid snap), dimmed crosses delete on Enter");
            break;
        case COMMAND_TOOL_SPLIT_V:
            state->active_tool = TOOL_SPLIT_VERTICAL;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_segment_selection(state);
            set_status_message(state, "Split V: click/drag vertical line, Enter or Save exports 2 parts");
            break;
        case COMMAND_TOOL_SPLIT_H:
            state->active_tool = TOOL_SPLIT_HORIZONTAL;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_segment_selection(state);
            set_status_message(state, "Split H: click/drag horizontal line, Enter or Save exports 2 parts");
            break;
        case COMMAND_TOOL_CLONE:
            state->active_tool = TOOL_CLONE;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_segment_selection(state);
            set_status_message(state, "Clone: drag area to copy, move snapped to grid, Click/Enter stamp");
            break;
        case COMMAND_TOOL_PICK:
            state->active_tool = TOOL_PICK_COLOR;
            set_status_message(state, "Pick: click any stitch to select its color block");
            break;
        case COMMAND_OPEN_FILE_DIALOG:
            open_file_dialog(state, FILE_DIALOG_OPEN);
            break;
        case COMMAND_MERGE_FILE_DIALOG:
            open_file_dialog(state, FILE_DIALOG_MERGE);
            break;
        case COMMAND_SAVE_AS_DIALOG:
            if (state->design.chain_count == 0U) {
                set_status_message(state, "Nothing to save");
            } else {
                open_file_dialog(state, FILE_DIALOG_SAVE_AS);
            }
            break;
        case COMMAND_ROTATE_90:
            if (state->design.chain_count > 0U) {
                rotate_design_90_clockwise(&state->design);
                state->grid = detect_cross_stitch_grid(&state->design);
                history_push(&state->history, &state->design);
                clear_segment_selection(state);
                clear_hover_preview(state);
                set_status_message(state, "Rotated 90 degrees clockwise");
            }
            break;
        case COMMAND_FLIP_X:
            if (state->design.chain_count > 0U) {
                mirror_design_horizontal(&state->design);
                state->grid = detect_cross_stitch_grid(&state->design);
                history_push(&state->history, &state->design);
                clear_segment_selection(state);
                clear_hover_preview(state);
                set_status_message(state, "Flipped horizontally");
            }
            break;
        case COMMAND_FLIP_Y:
            if (state->design.chain_count > 0U) {
                mirror_design_vertical(&state->design);
                state->grid = detect_cross_stitch_grid(&state->design);
                history_push(&state->history, &state->design);
                clear_segment_selection(state);
                clear_hover_preview(state);
                set_status_message(state, "Flipped vertically");
            }
            break;
        case COMMAND_CENTER:
            if (state->design.chain_count > 0U) {
                center_stitch_design(&state->design);
                state->grid = detect_cross_stitch_grid(&state->design);
                history_push(&state->history, &state->design);
                fit_camera_to_design(state, window_width, window_height);
                set_status_message(state, "Centered design at (0, 0)");
            }
            break;
        case COMMAND_MERGE_COLORS:
            if (state->design.block_count > 1U) {
                const size_t before_blocks = state->design.block_count;
                if (merge_duplicate_color_blocks(&state->design)) {
                    history_push(&state->history, &state->design);
                    if (state->active_color_block_index >= state->design.block_count &&
                        state->design.block_count > 0U) {
                        state->active_color_block_index = state->design.block_count - 1U;
                    }
                    clear_segment_selection(state);
                    clear_hover_preview(state);
                    char message[128] = {};
                    snprintf(message, sizeof(message), "Merged duplicate colors: %zu -> %zu blocks", before_blocks, state->design.block_count);
                    set_status_message(state, message);
                }
            }
            break;
        case COMMAND_RECOLOR_SELECTION:
            if (state->selected_segment_count > 0U) {
                state->palette_target = PALETTE_TARGET_RECOLOR_SELECTION;
                state->needs_redraw = true;
            } else {
                set_status_message(state, "Select crosses or element first to recolor");
            }
            break;
        case COMMAND_DELETE_SELECTION:
            delete_active_selection(state);
            break;
        case COMMAND_UNDO:
            if (state->history.current_index > 0U) {
                state->history.current_index -= 1U;
                free_stitch_design(&state->design);
                clone_stitch_design(&state->history.states[state->history.current_index], &state->design);
                clear_segment_selection(state);
                clear_hover_preview(state);
                clear_live_preview(state);
                state->has_box_selection = false;
                if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
                    state->active_color_block_index = state->design.block_count - 1U;
                }
                set_status_message(state, "Undo");
            }
            break;
        case COMMAND_REDO:
            if (state->history.current_index + 1U < state->history.state_count) {
                state->history.current_index += 1U;
                free_stitch_design(&state->design);
                clone_stitch_design(&state->history.states[state->history.current_index], &state->design);
                clear_segment_selection(state);
                clear_hover_preview(state);
                clear_live_preview(state);
                state->has_box_selection = false;
                if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
                    state->active_color_block_index = state->design.block_count - 1U;
                }
                set_status_message(state, "Redo");
            }
            break;
        case COMMAND_SAVE:
            save_current_design(state);
            break;
        case COMMAND_TOGGLE_DROP_MODE:
            state->drop_merge_mode = !state->drop_merge_mode;
            if (state->drop_merge_mode) {
                set_status_message(state, "Drop mode: Merge (dropped files attach for docking)");
            } else {
                set_status_message(state, "Drop mode: Open (dropped files replace current design)");
            }
            break;
        case COMMAND_CYCLE_HOOP:
            state->hoop_preset_index = (state->hoop_preset_index + 1U) % HOOP_PRESET_COUNT;
            state->needs_redraw = true;
            break;
        case COMMAND_ROTATE_HOOP:
            state->hoop_rotated = !state->hoop_rotated;
            state->needs_redraw = true;
            break;
        case COMMAND_TOGGLE_GRID:
            state->show_grid = !state->show_grid;
            state->needs_redraw = true;
            break;
        case COMMAND_TOGGLE_JUMPS:
            state->show_jumps = !state->show_jumps;
            state->needs_redraw = true;
            break;
        case COMMAND_TOGGLE_LOCKS:
            state->show_locks = !state->show_locks;
            state->needs_redraw = true;
            break;
        case COMMAND_PITCH_DECREASE:
            if (state->grid.cross_pitch_units > MIN_CROSS_PITCH_UNITS) {
                state->grid.cross_pitch_units -= 1;
                state->preview_dirty = true;
                state->needs_redraw = true;
            }
            break;
        case COMMAND_PITCH_INCREASE:
            if (state->grid.cross_pitch_units < MAX_CROSS_PITCH_UNITS) {
                state->grid.cross_pitch_units += 1;
                state->preview_dirty = true;
                state->needs_redraw = true;
            }
            break;
        case COMMAND_CYCLE_PASSES:
            state->grid.cross_passes += 1;
            if (state->grid.cross_passes > MAX_CROSS_PASSES) {
                state->grid.cross_passes = MIN_CROSS_PASSES;
            }
            state->needs_redraw = true;
            break;
        case COMMAND_CYCLE_VIEW_MODE:
            if (state->view_mode == VIEW_MODE_THREAD) {
                state->view_mode = VIEW_MODE_BLOCKS;
                set_status_message(state, "View mode: Blocks (cross-stitch chart squares)");
            } else if (state->view_mode == VIEW_MODE_BLOCKS) {
                state->view_mode = VIEW_MODE_LINE;
                set_status_message(state, "View mode: Line (1px technical stitch lines)");
            } else {
                state->view_mode = VIEW_MODE_THREAD;
                set_status_message(state, "View mode: Thread (thick realistic cross stitches)");
            }
            break;
        case COMMAND_CYCLE_THREAD_THICKNESS:
            state->thread_thickness_index =
                (state->thread_thickness_index + 1U) % THREAD_THICKNESS_PRESET_COUNT;
            {
                char message[96] = {};
                snprintf(
                    message,
                    sizeof(message),
                    "Thread thickness: %.2f mm",
                    thread_thickness_units[state->thread_thickness_index] * 0.1
                );
                set_status_message(state, message);
            }
            break;
        case COMMAND_FIT_VIEW:
            fit_camera_to_design(state, window_width, window_height);
            break;
        default:
            break;
    }
}

static void execute_file_dialog_confirm(
    EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    FileDialogState *dialog = &state->file_dialog;
    const bool is_save_mode =
        dialog->mode == FILE_DIALOG_SAVE_AS ||
        dialog->mode == FILE_DIALOG_STRIP_SAVE_AS;

    if (!is_save_mode) {
        if (dialog->selected_index >= dialog->entry_count) {
            snprintf(dialog->error_text, sizeof(dialog->error_text), "Select a file first");
            state->needs_redraw = true;
            return;
        }
        const FileBrowserEntry *entry = &dialog->entries[dialog->selected_index];
        char full_path[MAX_PATH_BUFFER] = {};
        int joined = 0;
        if (strcmp(dialog->current_directory, "/") == 0) {
            joined = snprintf(full_path, sizeof(full_path), "/%s", entry->name);
        } else {
            joined = snprintf(full_path, sizeof(full_path), "%s/%s", dialog->current_directory, entry->name);
        }
        if (joined <= 0 || (size_t)joined >= sizeof(full_path)) {
            return;
        }
        if (entry->is_directory) {
            load_file_browser_directory(state, full_path);
            return;
        }
        const FileDialogMode active_mode = dialog->mode;
        close_file_dialog(state);
        if (active_mode == FILE_DIALOG_OPEN) {
            load_design_into_editor(state, full_path, window_width, window_height);
        } else if (active_mode == FILE_DIALOG_MERGE) {
            load_dropped_file_as_merge_fragment(state, full_path, window_width, window_height);
        }
        return;
    }

    if (dialog->filename_input[0] == '\0') {
        snprintf(dialog->error_text, sizeof(dialog->error_text), "Enter a filename");
        state->needs_redraw = true;
        return;
    }

    /* Auto-append .jef extension if missing. */
    char clean_name[300] = {};
    const size_t name_len = strlen(dialog->filename_input);
    bool has_jef_suffix = false;
    if (name_len >= 4U && strings_equal_ignore_case(dialog->filename_input + name_len - 4U, ".jef")) {
        has_jef_suffix = true;
    }
    if (has_jef_suffix) {
        snprintf(clean_name, sizeof(clean_name), "%s", dialog->filename_input);
    } else {
        snprintf(clean_name, sizeof(clean_name), "%s.jef", dialog->filename_input);
    }

    char target_path[MAX_PATH_BUFFER] = {};
    int joined = 0;
    if (strcmp(dialog->current_directory, "/") == 0) {
        joined = snprintf(target_path, sizeof(target_path), "/%s", clean_name);
    } else {
        joined = snprintf(target_path, sizeof(target_path), "%s/%s", dialog->current_directory, clean_name);
    }
    if (joined <= 0 || (size_t)joined >= sizeof(target_path)) {
        snprintf(dialog->error_text, sizeof(dialog->error_text), "Path too long");
        state->needs_redraw = true;
        return;
    }

    if (file_already_exists(target_path)) {
        if (!dialog->confirm_overwrite || strcmp(dialog->pending_overwrite_path, target_path) != 0) {
            dialog->confirm_overwrite = true;
            snprintf(dialog->pending_overwrite_path, sizeof(dialog->pending_overwrite_path), "%s", target_path);
            state->needs_redraw = true;
            return;
        }
    }

    bool saved = false;
    if (dialog->mode == FILE_DIALOG_STRIP_SAVE_AS) {
        StitchDesign built_strip = {};
        if (!build_strip_design(&state->strip_pattern, &built_strip)) {
            snprintf(dialog->error_text, sizeof(dialog->error_text), "Strip is empty");
            state->needs_redraw = true;
            return;
        }
        saved = write_jef_file(&built_strip, target_path);
        free_stitch_design(&built_strip);
    } else {
        saved = write_jef_file(&state->design, target_path);
    }

    if (!saved) {
        snprintf(dialog->error_text, sizeof(dialog->error_text), "Failed to write JEF file");
        state->needs_redraw = true;
        return;
    }

    char message[256] = {};
    snprintf(message, sizeof(message), "Saved %.120s", basename_of_path(target_path));
    close_file_dialog(state);
    set_status_message(state, message);
}

static void handle_file_dialog_command(
    EditorState *state,
    uint8_t command_id,
    int32_t window_width,
    int32_t window_height
) {
    FileDialogState *dialog = &state->file_dialog;
    if (command_id == DIALOG_CMD_CANCEL) {
        close_file_dialog(state);
        return;
    }
    if (command_id == DIALOG_CMD_CONFIRM) {
        execute_file_dialog_confirm(state, window_width, window_height);
        return;
    }
    if (command_id == DIALOG_CMD_PARENT) {
        char parent_path[MAX_PATH_BUFFER] = {};
        const char *last_slash = strrchr(dialog->current_directory, '/');
        if (last_slash == nullptr || last_slash == dialog->current_directory) {
            snprintf(parent_path, sizeof(parent_path), "/");
        } else {
            const size_t parent_len = (size_t)(last_slash - dialog->current_directory);
            snprintf(parent_path, sizeof(parent_path), "%.*s", (int)parent_len, dialog->current_directory);
        }
        load_file_browser_directory(state, parent_path);
        return;
    }
    if (command_id == DIALOG_CMD_HOME) {
        const char *home_dir = getenv("HOME");
        if (home_dir != nullptr) {
            load_file_browser_directory(state, home_dir);
        }
        return;
    }
    if (command_id == DIALOG_CMD_DOWNLOADS) {
        const char *home_dir = getenv("HOME");
        if (home_dir != nullptr) {
            char downloads_dir[MAX_PATH_BUFFER] = {};
            const int dl_written = snprintf(downloads_dir, sizeof(downloads_dir), "%s/Downloads", home_dir);
            if (dl_written > 0 && (size_t)dl_written < sizeof(downloads_dir) && directory_exists(downloads_dir)) {
                load_file_browser_directory(state, downloads_dir);
            } else {
                load_file_browser_directory(state, home_dir);
            }
        }
    }
}

static void handle_file_dialog_click(
    EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_width,
    int32_t window_height
) {
    const FileDialogLayout layout = compute_file_dialog_layout(state, window_width, window_height);
    for (size_t index = 0U; index < layout.button_count; index += 1U) {
        const UiButton *button = &layout.buttons[index];
        if (point_is_in_box(mouse_x, mouse_y, button->left_x, button->top_y, button->width, button->height)) {
            handle_file_dialog_command(state, button->command_id, window_width, window_height);
            return;
        }
    }

    if (!point_is_in_box(mouse_x, mouse_y, layout.list_left, layout.list_top, layout.list_width, layout.list_height)) {
        return;
    }

    FileDialogState *dialog = &state->file_dialog;
    const int32_t row_signed = (mouse_y - layout.list_top + dialog->scroll_offset) / FILE_ROW_HEIGHT;
    if (row_signed < 0 || (size_t)row_signed >= dialog->entry_count) {
        return;
    }

    const size_t clicked_index = (size_t)row_signed;
    const uint32_t now_ticks = SDL_GetTicks();
    const bool is_double_click =
        clicked_index == dialog->last_click_index &&
        (now_ticks - dialog->last_click_ticks) <= DOUBLE_CLICK_MS;

    dialog->selected_index = clicked_index;
    dialog->last_click_index = clicked_index;
    dialog->last_click_ticks = now_ticks;
    dialog->confirm_overwrite = false;
    dialog->error_text[0] = '\0';

    const FileBrowserEntry *entry = &dialog->entries[clicked_index];
    const bool is_save_mode =
        dialog->mode == FILE_DIALOG_SAVE_AS ||
        dialog->mode == FILE_DIALOG_STRIP_SAVE_AS;

    if (!entry->is_directory && is_save_mode) {
        snprintf(dialog->filename_input, sizeof(dialog->filename_input), "%.255s", entry->name);
    }
    state->needs_redraw = true;

    if (is_double_click) {
        if (entry->is_directory) {
            char next_dir[MAX_PATH_BUFFER] = {};
            int joined = 0;
            if (strcmp(dialog->current_directory, "/") == 0) {
                joined = snprintf(next_dir, sizeof(next_dir), "/%s", entry->name);
            } else {
                joined = snprintf(next_dir, sizeof(next_dir), "%s/%s", dialog->current_directory, entry->name);
            }
            if (joined > 0 && (size_t)joined < sizeof(next_dir)) {
                load_file_browser_directory(state, next_dir);
            }
        } else {
            execute_file_dialog_confirm(state, window_width, window_height);
        }
    }
}

static void apply_palette_selection(EditorState *state, uint8_t chosen_janome_index) {
    const PaletteModalTarget target = state->palette_target;
    state->palette_target = PALETTE_TARGET_NONE;
    if (chosen_janome_index >= JANOME_PALETTE_COUNT) {
        return;
    }

    uint8_t valid_janome = chosen_janome_index;
    if (valid_janome == 0U) {
        valid_janome = 1U;
    }

    if (target == PALETTE_TARGET_STRIP_COLOR) {
        state->strip_active_color = valid_janome;
        state->needs_redraw = true;
        return;
    }

    if (target == PALETTE_TARGET_ADD_BLOCK) {
        size_t new_block_index = 0U;
        if (design_append_block(&state->design, janome_palette[valid_janome], valid_janome, true, &new_block_index)) {
            state->active_color_block_index = new_block_index;
            history_push(&state->history, &state->design);
            char message[128] = {};
            snprintf(message, sizeof(message), "Added Color Block #%zu (Janome #%02u)", new_block_index + 1U, (unsigned int)valid_janome);
            set_status_message(state, message);
        }
        return;
    }

    if (target == PALETTE_TARGET_RECOLOR_BLOCK &&
        state->palette_target_block_index < state->design.block_count) {
        state->design.blocks[state->palette_target_block_index].janome_color_index = valid_janome;
        state->design.blocks[state->palette_target_block_index].display_color = janome_palette[valid_janome];
        history_push(&state->history, &state->design);
        char message[128] = {};
        snprintf(message, sizeof(message), "Changed Block #%zu to Janome #%02u", state->palette_target_block_index + 1U, (unsigned int)valid_janome);
        set_status_message(state, message);
        return;
    }

    if (target == PALETTE_TARGET_RECOLOR_SELECTION &&
        state->selection_mask != nullptr &&
        state->selected_segment_count > 0U) {
        StitchDesign recolored = {};
        if (recolor_masked_segments(&state->design, state->selection_mask, state->selection_mask_length, valid_janome, &recolored)) {
            free_stitch_design(&state->design);
            state->design = recolored;
            history_push(&state->history, &state->design);
            clear_segment_selection(state);
            clear_hover_preview(state);
            char message[128] = {};
            snprintf(message, sizeof(message), "Recolored selection to Janome #%02u", (unsigned int)valid_janome);
            set_status_message(state, message);
        }
    }
}

static bool handle_palette_modal_click(
    EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_width,
    int32_t window_height
) {
    const PaletteModalLayout layout = compute_palette_modal_layout(window_width, window_height);
    if (!point_is_in_box(mouse_x, mouse_y, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height)) {
        state->palette_target = PALETTE_TARGET_NONE;
        state->needs_redraw = true;
        return true;
    }

    for (int32_t palette_index = 0; palette_index < (int32_t)JANOME_PALETTE_COUNT; palette_index += 1) {
        const int32_t column = palette_index % layout.columns;
        const int32_t row = palette_index / layout.columns;
        const int32_t cell_x = layout.grid_left + column * (layout.cell_size + layout.cell_gap);
        const int32_t cell_y = layout.grid_top + row * (layout.cell_size + layout.cell_gap);
        if (point_is_in_box(mouse_x, mouse_y, cell_x, cell_y, layout.cell_size, layout.cell_size)) {
            apply_palette_selection(state, (uint8_t)palette_index);
            return true;
        }
    }
    return true;
}

static void paint_strip_cell_at_mouse(
    EditorState *state,
    const StripModalLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y,
    bool erase_mode
) {
    const int32_t grid_pixel_width = state->strip_pattern.width * layout->editor_cell_size;
    const int32_t grid_pixel_height = state->strip_pattern.height * layout->editor_cell_size;
    const int32_t grid_origin_x = layout->editor_left + (layout->editor_width - grid_pixel_width) / 2;
    const int32_t grid_origin_y = layout->editor_top + (layout->editor_height - grid_pixel_height) / 2;

    if (!point_is_in_box(mouse_x, mouse_y, grid_origin_x, grid_origin_y, grid_pixel_width, grid_pixel_height)) {
        return;
    }
    const int32_t column = (mouse_x - grid_origin_x) / layout->editor_cell_size;
    const int32_t row = (mouse_y - grid_origin_y) / layout->editor_cell_size;
    if (column < 0 || column >= state->strip_pattern.width ||
        row < 0 || row >= state->strip_pattern.height) {
        return;
    }

    uint8_t target_color = state->strip_active_color;
    if (target_color == 0U || target_color >= JANOME_PALETTE_COUNT) {
        target_color = 10U;
    }
    if (erase_mode) {
        target_color = 0U;
    }

    uint8_t *cell = &state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column];
    if (*cell == target_color) {
        return;
    }
    if (!state->strip_stroke_pushed_undo) {
        strip_history_push(state);
        state->strip_stroke_pushed_undo = true;
    }
    *cell = target_color;
    state->needs_redraw = true;
}

static void handle_strip_command(
    EditorState *state,
    uint8_t command_id,
    int32_t window_width,
    int32_t window_height
) {
    switch (command_id) {
        case STRIP_CMD_WIDTH_DEC:
            if (state->strip_pattern.width > 1) {
                strip_history_push(state);
                state->strip_pattern.width -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_WIDTH_INC:
            if (state->strip_pattern.width < STRIP_MAX_GRID_DIM) {
                strip_history_push(state);
                state->strip_pattern.width += 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_HEIGHT_DEC:
            if (state->strip_pattern.height > 1) {
                strip_history_push(state);
                state->strip_pattern.height -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_HEIGHT_INC:
            if (state->strip_pattern.height < STRIP_MAX_GRID_DIM) {
                strip_history_push(state);
                state->strip_pattern.height += 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_REPEAT_X_DEC:
            if (state->strip_pattern.repeat_x > 1) {
                strip_history_push(state);
                state->strip_pattern.repeat_x -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_REPEAT_X_INC:
            if (state->strip_pattern.repeat_x < STRIP_MAX_REPEAT) {
                strip_history_push(state);
                state->strip_pattern.repeat_x += 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_REPEAT_Y_DEC:
            if (state->strip_pattern.repeat_y > 1) {
                strip_history_push(state);
                state->strip_pattern.repeat_y -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_REPEAT_Y_INC:
            if (state->strip_pattern.repeat_y < STRIP_MAX_REPEAT) {
                strip_history_push(state);
                state->strip_pattern.repeat_y += 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_PITCH_DEC:
            if (state->strip_pattern.pitch_units > MIN_CROSS_PITCH_UNITS) {
                state->strip_pattern.pitch_units -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_PITCH_INC:
            if (state->strip_pattern.pitch_units < MAX_CROSS_PITCH_UNITS) {
                state->strip_pattern.pitch_units += 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_CYCLE_PASSES:
            state->strip_pattern.passes += 1;
            if (state->strip_pattern.passes > MAX_CROSS_PASSES) {
                state->strip_pattern.passes = MIN_CROSS_PASSES;
            }
            state->needs_redraw = true;
            break;
        case STRIP_CMD_PICK_COLOR:
            state->palette_target = PALETTE_TARGET_STRIP_COLOR;
            state->needs_redraw = true;
            break;
        case STRIP_CMD_CLEAR:
            strip_history_push(state);
            memset(state->strip_pattern.cells, 0, sizeof(state->strip_pattern.cells));
            state->needs_redraw = true;
            break;
        case STRIP_CMD_MIRROR_X:
            strip_history_push(state);
            for (int32_t row = 0; row < state->strip_pattern.height; row += 1) {
                for (int32_t column = 0; column < state->strip_pattern.width / 2; column += 1) {
                    const int32_t mirror_column = state->strip_pattern.width - 1 - column;
                    const uint8_t saved = state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column];
                    state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column] =
                        state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + mirror_column];
                    state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + mirror_column] = saved;
                }
            }
            state->needs_redraw = true;
            break;
        case STRIP_CMD_MIRROR_Y:
            strip_history_push(state);
            for (int32_t row = 0; row < state->strip_pattern.height / 2; row += 1) {
                const int32_t mirror_row = state->strip_pattern.height - 1 - row;
                for (int32_t column = 0; column < state->strip_pattern.width; column += 1) {
                    const uint8_t saved = state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column];
                    state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column] =
                        state->strip_pattern.cells[mirror_row * STRIP_MAX_GRID_DIM + column];
                    state->strip_pattern.cells[mirror_row * STRIP_MAX_GRID_DIM + column] = saved;
                }
            }
            state->needs_redraw = true;
            break;
        case STRIP_CMD_UNDO:
            if (state->strip_history.state_count > 0U) {
                state->strip_history.state_count -= 1U;
                state->strip_pattern = state->strip_history.states[state->strip_history.state_count];
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_INSERT: {
            StitchDesign built_strip = {};
            if (!build_strip_design(&state->strip_pattern, &built_strip)) {
                set_status_message(state, "Strip is empty: paint crosses first");
                return;
            }
            clear_floating_fragment(state);
            clear_segment_selection(state);
            state->floating_fragment = built_strip;
            state->has_floating_fragment = true;
            state->floating_is_grid_origin_aligned = true;
            state->floating_base_origin_x = 0;
            state->floating_base_origin_y = 0;
            state->floating_anchor_x = 0;
            state->floating_anchor_y = 0;
            state->strip_modal_open = false;

            const int32_t cursor_world_x = screen_to_world_x(state, window_width, window_height, state->mouse_screen_x);
            const int32_t cursor_world_y = screen_to_world_y(state, window_width, window_height, state->mouse_screen_y);
            state->floating_offset_x = snap_coordinate_to_grid(cursor_world_x, state->grid.grid_origin_x, state->grid.cross_pitch_units);
            state->floating_offset_y = snap_coordinate_to_grid(cursor_world_y, state->grid.grid_origin_y, state->grid.cross_pitch_units);
            set_status_message(state, "Place Strip: move/arrows, Click/Enter insert, Esc cancel");
            break;
        }
        case STRIP_CMD_SAVE_AS:
            open_file_dialog(state, FILE_DIALOG_STRIP_SAVE_AS);
            break;
        case STRIP_CMD_SAVE_JEF: {
            StitchDesign built_strip = {};
            if (!build_strip_design(&state->strip_pattern, &built_strip)) {
                set_status_message(state, "Strip is empty: paint crosses first");
                return;
            }
            const char *base_path = "strip_design.jef";
            const char *suffix = "_strip";
            if (state->has_file_path) {
                base_path = state->current_file_path;
            } else {
                suffix = "";
            }
            char output_path[MAX_PATH_BUFFER] = {};
            if (build_unique_suffixed_jef_path(base_path, suffix, output_path, sizeof(output_path)) &&
                write_jef_file(&built_strip, output_path)) {
                char message[256] = {};
                snprintf(message, sizeof(message), "Exported strip to %.110s", basename_of_path(output_path));
                set_status_message(state, message);
            } else {
                set_status_message(state, "Failed to export strip JEF");
            }
            free_stitch_design(&built_strip);
            break;
        }
        case STRIP_CMD_CLOSE:
            state->strip_modal_open = false;
            state->needs_redraw = true;
            break;
        default:
            break;
    }
}

static bool handle_left_panel_click(EditorState *state, int32_t mouse_x, int32_t mouse_y, int32_t window_height) {
    const int32_t panel_top = state->toolbar_height;
    const int32_t panel_bottom = window_height - BOTTOM_PANEL_HEIGHT;
    if (mouse_x >= LEFT_PANEL_WIDTH || mouse_y < panel_top || mouse_y >= panel_bottom) {
        return false;
    }

    if (point_is_in_box(mouse_x, mouse_y, 10, panel_top + 10, LEFT_PANEL_WIDTH - 20, 34)) {
        state->palette_target = PALETTE_TARGET_ADD_BLOCK;
        state->needs_redraw = true;
        return true;
    }

    const int32_t list_top = panel_top + 54;
    if (mouse_y < list_top) {
        return true;
    }
    const int32_t row_index_signed = (mouse_y - list_top + state->color_scroll_offset) / COLOR_ROW_HEIGHT;
    if (row_index_signed < 0 || (size_t)row_index_signed >= state->design.block_count) {
        return true;
    }
    const size_t block_index = (size_t)row_index_signed;
    const int32_t row_y = list_top + row_index_signed * COLOR_ROW_HEIGHT - state->color_scroll_offset;

    constexpr int32_t ROW_BUTTON_WIDTH = 34;
    constexpr int32_t ROW_BUTTON_HEIGHT = 28;
    constexpr int32_t ROW_BUTTON_GAP = 4;
    const int32_t buttons_start_x = LEFT_PANEL_WIDTH - 12 - (5 * ROW_BUTTON_WIDTH + 4 * ROW_BUTTON_GAP);
    const int32_t button_y = row_y + 6;

    for (int32_t action_index = 0; action_index < 5; action_index += 1) {
        const int32_t button_x = buttons_start_x + action_index * (ROW_BUTTON_WIDTH + ROW_BUTTON_GAP);
        if (!point_is_in_box(mouse_x, mouse_y, button_x, button_y, ROW_BUTTON_WIDTH, ROW_BUTTON_HEIGHT)) {
            continue;
        }
        if (action_index == 0) {
            state->design.blocks[block_index].enabled = !state->design.blocks[block_index].enabled;
            recompute_design_bounds(&state->design);
            history_push(&state->history, &state->design);
            state->preview_dirty = true;
            clear_hover_preview(state);
            state->needs_redraw = true;
        } else if (action_index == 1) {
            if (move_color_block_order(&state->design, block_index, -1)) {
                state->active_color_block_index = block_index - 1U;
                history_push(&state->history, &state->design);
                clear_segment_selection(state);
                clear_hover_preview(state);
                set_status_message(state, "Moved color block earlier in stitch order");
            }
        } else if (action_index == 2) {
            if (move_color_block_order(&state->design, block_index, 1)) {
                state->active_color_block_index = block_index + 1U;
                history_push(&state->history, &state->design);
                clear_segment_selection(state);
                clear_hover_preview(state);
                set_status_message(state, "Moved color block later in stitch order");
            }
        } else if (action_index == 3) {
            state->palette_target = PALETTE_TARGET_RECOLOR_BLOCK;
            state->palette_target_block_index = block_index;
            state->needs_redraw = true;
        } else if (action_index == 4) {
            if (remove_color_block_at(&state->design, block_index)) {
                if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
                    state->active_color_block_index = state->design.block_count - 1U;
                }
                history_push(&state->history, &state->design);
                clear_segment_selection(state);
                clear_hover_preview(state);
                state->preview_dirty = true;
                set_status_message(state, "Deleted color block");
            }
        }
        return true;
    }

    state->active_color_block_index = block_index;
    const ColorBlock block = state->design.blocks[block_index];
    char message[128] = {};
    snprintf(
        message,
        sizeof(message),
        "Active Color: Block #%zu (Janome #%02u #%02X%02X%02X)",
        block_index + 1U,
        (unsigned int)block.janome_color_index,
        (unsigned int)block.display_color.red,
        (unsigned int)block.display_color.green,
        (unsigned int)block.display_color.blue
    );
    set_status_message(state, message);
    return true;
}

static void update_slider_from_mouse(EditorState *state, int32_t mouse_x) {
    if (state->slider_right_x <= state->slider_left_x) {
        return;
    }
    double ratio =
        (double)(mouse_x - state->slider_left_x) / (double)(state->slider_right_x - state->slider_left_x);
    if (ratio < 0.0) {
        ratio = 0.0;
    }
    if (ratio > 1.0) {
        ratio = 1.0;
    }
    state->simulator_percent = ratio * 100.0;
    state->needs_redraw = true;
}

static void apply_draw_x_stroke_at(
    EditorState *state,
    int32_t world_x,
    int32_t world_y,
    bool erase_mode
) {
    int32_t cell_column = 0;
    int32_t cell_row = 0;
    world_to_grid_cell(world_x, world_y, state->grid, &cell_column, &cell_row);
    if (state->draw_stroke_pushed_undo &&
        cell_column == state->last_drawn_column &&
        cell_row == state->last_drawn_row) {
        return;
    }

    if (erase_mode) {
        const size_t total_segments = count_design_segments(&state->design);
        if (total_segments == 0U) {
            return;
        }
        uint8_t *erase_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
        if (erase_mask == nullptr) {
            return;
        }
        const size_t matched = select_cross_at_point_in_mask(
            &state->design,
            state->grid,
            world_x,
            world_y,
            erase_mask,
            total_segments,
            false,
            nullptr
        );
        if (matched > 0U) {
            StitchDesign trimmed = {};
            if (delete_masked_segments(&state->design, erase_mask, total_segments, &trimmed)) {
                free_stitch_design(&state->design);
                state->design = trimmed;
                state->draw_stroke_pushed_undo = true;
                state->last_drawn_column = cell_column;
                state->last_drawn_row = cell_row;
                clear_segment_selection(state);
                clear_hover_preview(state);
                state->needs_redraw = true;
            }
        }
        free(erase_mask);
        return;
    }

    if (state->design.block_count == 0U) {
        size_t new_block = 0U;
        if (!design_append_block(&state->design, janome_palette[10], 10U, true, &new_block)) {
            return;
        }
        state->active_color_block_index = new_block;
    }
    if (state->active_color_block_index >= state->design.block_count) {
        state->active_color_block_index = state->design.block_count - 1U;
    }
    state->design.blocks[state->active_color_block_index].enabled = true;

    if (draw_single_cross_into_design(
            &state->design,
            state->grid,
            cell_column,
            cell_row,
            state->active_color_block_index
        )) {
        state->draw_stroke_pushed_undo = true;
        state->last_drawn_column = cell_column;
        state->last_drawn_row = cell_row;
        clear_segment_selection(state);
        clear_hover_preview(state);
        state->needs_redraw = true;
    }
}

static void update_canvas_hover(
    EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_width,
    int32_t window_height
) {
    int32_t viewport_left = 0;
    int32_t viewport_top = 0;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    get_viewport_bounds(state, window_width, window_height, &viewport_left, &viewport_top, &viewport_width, &viewport_height);

    if (!point_is_in_box(mouse_x, mouse_y, viewport_left, viewport_top, viewport_width, viewport_height) ||
        state->design.chain_count == 0U ||
        state->has_floating_fragment) {
        if (state->hover_seed_chain != SIZE_MAX || state->has_hover_cross_box) {
            clear_hover_preview(state);
            state->needs_redraw = true;
        }
        return;
    }

    const int32_t world_x = screen_to_world_x(state, window_width, window_height, mouse_x);
    const int32_t world_y = screen_to_world_y(state, window_width, window_height, mouse_y);

    if (state->active_tool == TOOL_SELECT_CROSS) {
        bool snapped = false;
        const CutRectangle box = resolve_cross_box_at_point(&state->design, state->grid, world_x, world_y, &snapped);
        if (!state->has_hover_cross_box ||
            box.minimum_x != state->hover_cross_box.minimum_x ||
            box.minimum_y != state->hover_cross_box.minimum_y) {
            state->hover_cross_box = box;
            state->has_hover_cross_box = true;
            state->needs_redraw = true;
        }
        return;
    }

    if (state->active_tool == TOOL_SELECT_ELEMENT) {
        int32_t search_radius = state->grid.cross_pitch_units;
        const int32_t pixel_radius = (int32_t)llround(12.0 / state->pixels_per_unit);
        if (pixel_radius > search_radius) {
            search_radius = pixel_radius;
        }
        size_t hovered_chain = SIZE_MAX;
        if (!find_nearest_chain_at_point(&state->design, world_x, world_y, search_radius, &hovered_chain)) {
            if (state->hover_seed_chain != SIZE_MAX) {
                clear_hover_preview(state);
                state->needs_redraw = true;
            }
            return;
        }
        if (hovered_chain == state->hover_seed_chain) {
            return;
        }

        const size_t total_segments = count_design_segments(&state->design);
        if (state->hover_mask == nullptr || state->hover_mask_length != total_segments) {
            free(state->hover_mask);
            state->hover_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
            state->hover_mask_length = total_segments;
        } else {
            memset(state->hover_mask, 0, total_segments);
        }
        if (state->hover_mask != nullptr) {
            select_connected_element_in_mask(
                &state->design,
                state->grid,
                hovered_chain,
                state->hover_mask,
                state->hover_mask_length,
                false
            );
            state->hover_seed_chain = hovered_chain;
            state->needs_redraw = true;
        }
    }
}

static void nudge_active_selection_or_fragment(
    EditorState *state,
    int32_t step_columns,
    int32_t step_rows,
    bool shift_held
) {
    int32_t pitch = state->grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }
    const int32_t delta_x = step_columns * pitch;
    const int32_t delta_y = step_rows * pitch;

    if (state->has_floating_fragment) {
        state->floating_offset_x += delta_x;
        state->floating_offset_y += delta_y;
        state->needs_redraw = true;
        return;
    }

    if (state->selection_mask != nullptr && state->selected_segment_count > 0U) {
        StitchDesign shifted = {};
        uint8_t *updated_mask = nullptr;
        size_t updated_mask_length = 0U;
        if (shift_masked_segments(
                &state->design,
                state->selection_mask,
                state->selection_mask_length,
                delta_x,
                delta_y,
                &shifted,
                &updated_mask,
                &updated_mask_length
            )) {
            free_stitch_design(&state->design);
            state->design = shifted;
            history_push(&state->history, &state->design);
            free(state->selection_mask);
            state->selection_mask = updated_mask;
            state->selection_mask_length = updated_mask_length;
            recount_selected_segments(state);
            clear_hover_preview(state);
            set_status_message(state, "Shifted selection by 1 grid pitch");
        }
        return;
    }

    if (shift_held && state->design.chain_count > 0U) {
        shift_design_center(&state->design, delta_x, delta_y);
        history_push(&state->history, &state->design);
        set_status_message(state, "Shifted design center");
    }
}

static void apply_box_edit_or_split(EditorState *state) {
    if (!state->has_box_selection) {
        return;
    }
    if (state->active_tool == TOOL_SPLIT_VERTICAL || state->active_tool == TOOL_SPLIT_HORIZONTAL) {
        save_current_design(state);
        return;
    }
    if (state->active_tool != TOOL_CROP_KEEP && state->active_tool != TOOL_CUT_OUT) {
        return;
    }

    const CutRectangle rectangle = {
        state->box_start_x,
        state->box_start_y,
        state->box_end_x,
        state->box_end_y
    };
    const EditOperationKind operation = tool_to_edit_operation(state->active_tool);
    StitchDesign edited = {};
    if (!apply_edit_operation(&state->design, state->grid, operation, rectangle, 0, false, &edited) ||
        edited.chain_count == 0U) {
        free_stitch_design(&edited);
        set_status_message(state, "Edit leaves zero stitches; not applied");
        return;
    }
    free_stitch_design(&state->design);
    state->design = edited;
    clear_live_preview(state);
    state->has_box_selection = false;
    history_push(&state->history, &state->design);
    if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
        state->active_color_block_index = state->design.block_count - 1U;
    }
    set_status_message(state, "Applied edit with atomic crosses and tie-off locks");
}

static void handle_mouse_button_down(
    EditorState *state,
    const SDL_MouseButtonEvent *button_event,
    int32_t window_width,
    int32_t window_height
) {
    const int32_t mouse_x = button_event->x;
    const int32_t mouse_y = button_event->y;
    const SDL_Keymod key_mod = SDL_GetModState();
    const bool shift_held = (key_mod & KMOD_SHIFT) != 0;

    if (state->palette_target != PALETTE_TARGET_NONE) {
        if (button_event->button == SDL_BUTTON_LEFT) {
            handle_palette_modal_click(state, mouse_x, mouse_y, window_width, window_height);
        }
        return;
    }

    if (state->file_dialog.mode != FILE_DIALOG_CLOSED) {
        if (button_event->button == SDL_BUTTON_LEFT) {
            handle_file_dialog_click(state, mouse_x, mouse_y, window_width, window_height);
        }
        return;
    }

    if (state->strip_modal_open) {
        const StripModalLayout layout = compute_strip_modal_layout(state, window_width, window_height);
        if (button_event->button == SDL_BUTTON_LEFT) {
            for (size_t index = 0U; index < layout.button_count; index += 1U) {
                const UiButton *button = &layout.buttons[index];
                if (!button->is_readout &&
                    point_is_in_box(mouse_x, mouse_y, button->left_x, button->top_y, button->width, button->height)) {
                    handle_strip_command(state, button->command_id, window_width, window_height);
                    return;
                }
            }
            state->strip_is_painting = true;
            state->strip_stroke_pushed_undo = false;
            paint_strip_cell_at_mouse(state, &layout, mouse_x, mouse_y, false);
        } else if (button_event->button == SDL_BUTTON_RIGHT) {
            state->strip_is_erasing = true;
            state->strip_stroke_pushed_undo = false;
            paint_strip_cell_at_mouse(state, &layout, mouse_x, mouse_y, true);
        }
        return;
    }

    if (button_event->button == SDL_BUTTON_LEFT && mouse_y < state->toolbar_height) {
        for (size_t index = 0U; index < state->toolbar_button_count; index += 1U) {
            const UiButton *button = &state->toolbar_buttons[index];
            if (!button->is_readout &&
                point_is_in_box(mouse_x, mouse_y, button->left_x, button->top_y, button->width, button->height)) {
                execute_toolbar_command(state, button->command_id, window_width, window_height);
                return;
            }
        }
        return;
    }

    if (button_event->button == SDL_BUTTON_LEFT &&
        mouse_y >= state->slider_top_y - 4 &&
        mouse_y <= state->slider_bottom_y + 4 &&
        mouse_x >= state->slider_left_x - 8 &&
        mouse_x <= state->slider_right_x + 8) {
        state->is_dragging_slider = true;
        update_slider_from_mouse(state, mouse_x);
        return;
    }

    if (button_event->button == SDL_BUTTON_LEFT &&
        handle_left_panel_click(state, mouse_x, mouse_y, window_height)) {
        return;
    }

    if (button_event->button == SDL_BUTTON_LEFT && should_show_start_screen(state)) {
        const StartScreenLayout start_layout = compute_start_screen_layout(state, window_width, window_height);
        if (point_is_in_box(
                mouse_x,
                mouse_y,
                start_layout.open_button.left_x,
                start_layout.open_button.top_y,
                start_layout.open_button.width,
                start_layout.open_button.height
            )) {
            open_file_dialog(state, FILE_DIALOG_OPEN);
            return;
        }
        if (point_is_in_box(
                mouse_x,
                mouse_y,
                start_layout.strip_button.left_x,
                start_layout.strip_button.top_y,
                start_layout.strip_button.width,
                start_layout.strip_button.height
            )) {
            open_strip_modal(state);
            return;
        }
    }

    int32_t viewport_left = 0;
    int32_t viewport_top = 0;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    get_viewport_bounds(state, window_width, window_height, &viewport_left, &viewport_top, &viewport_width, &viewport_height);
    if (!point_is_in_box(mouse_x, mouse_y, viewport_left, viewport_top, viewport_width, viewport_height)) {
        return;
    }

    if (button_event->button == SDL_BUTTON_MIDDLE ||
        (button_event->button == SDL_BUTTON_LEFT && state->space_is_held)) {
        state->is_panning = true;
        state->pan_last_x = mouse_x;
        state->pan_last_y = mouse_y;
        return;
    }

    if (state->has_floating_fragment) {
        if (button_event->button == SDL_BUTTON_LEFT) {
            commit_floating_fragment(state, shift_held);
        } else if (button_event->button == SDL_BUTTON_RIGHT) {
            clear_floating_fragment(state);
            set_status_message(state, "Cancelled fragment placement");
        }
        return;
    }

    const int32_t world_x = screen_to_world_x(state, window_width, window_height, mouse_x);
    const int32_t world_y = screen_to_world_y(state, window_width, window_height, mouse_y);

    if (state->active_tool == TOOL_DRAW_CROSS) {
        if (button_event->button == SDL_BUTTON_LEFT || button_event->button == SDL_BUTTON_RIGHT) {
            state->is_drawing_crosses = true;
            state->drawing_erases = (button_event->button == SDL_BUTTON_RIGHT);
            state->draw_stroke_pushed_undo = false;
            apply_draw_x_stroke_at(state, world_x, world_y, state->drawing_erases);
        }
        return;
    }

    if (state->active_tool == TOOL_PICK_COLOR && button_event->button == SDL_BUTTON_LEFT) {
        size_t picked_chain = 0U;
        if (find_nearest_chain_at_point(&state->design, world_x, world_y, state->grid.cross_pitch_units * 2, &picked_chain)) {
            const size_t block_index = state->design.chains[picked_chain].color_block_index;
            if (block_index < state->design.block_count) {
                state->active_color_block_index = block_index;
                const ColorBlock block = state->design.blocks[block_index];
                char message[128] = {};
                snprintf(
                    message,
                    sizeof(message),
                    "Picked Block #%zu (Janome #%02u #%02X%02X%02X)",
                    block_index + 1U,
                    (unsigned int)block.janome_color_index,
                    (unsigned int)block.display_color.red,
                    (unsigned int)block.display_color.green,
                    (unsigned int)block.display_color.blue
                );
                set_status_message(state, message);
            }
        }
        return;
    }

    if (state->active_tool == TOOL_SELECT_ELEMENT) {
        if (button_event->button == SDL_BUTTON_RIGHT) {
            if (state->selected_segment_count > 0U) {
                delete_active_selection(state);
            } else if (state->hover_seed_chain != SIZE_MAX && ensure_selection_mask_allocated(state)) {
                memset(state->selection_mask, 0, state->selection_mask_length);
                select_connected_element_in_mask(&state->design, state->grid, state->hover_seed_chain, state->selection_mask, state->selection_mask_length, false);
                recount_selected_segments(state);
                delete_active_selection(state);
            }
            return;
        }
        if (button_event->button == SDL_BUTTON_LEFT) {
            size_t picked_chain = 0U;
            int32_t search_radius = state->grid.cross_pitch_units;
            const int32_t pixel_radius = (int32_t)llround(12.0 / state->pixels_per_unit);
            if (pixel_radius > search_radius) {
                search_radius = pixel_radius;
            }
            if (!find_nearest_chain_at_point(&state->design, world_x, world_y, search_radius, &picked_chain)) {
                if (!shift_held) {
                    clear_segment_selection(state);
                    set_status_message(state, "Selection cleared");
                }
                return;
            }
            if (!ensure_selection_mask_allocated(state)) {
                return;
            }
            if (!shift_held) {
                memset(state->selection_mask, 0, state->selection_mask_length);
            }
            select_connected_element_in_mask(&state->design, state->grid, picked_chain, state->selection_mask, state->selection_mask_length, shift_held);
            recount_selected_segments(state);
            char message[128] = {};
            snprintf(message, sizeof(message), "Sel Elem: %zu segments | Del, Recolor, Arrows", state->selected_segment_count);
            set_status_message(state, message);
        }
        return;
    }

    if (state->active_tool == TOOL_SELECT_CROSS) {
        if (button_event->button == SDL_BUTTON_RIGHT) {
            if (state->selected_segment_count > 0U) {
                delete_active_selection(state);
            } else if (ensure_selection_mask_allocated(state)) {
                memset(state->selection_mask, 0, state->selection_mask_length);
                select_cross_at_point_in_mask(&state->design, state->grid, world_x, world_y, state->selection_mask, state->selection_mask_length, false, nullptr);
                recount_selected_segments(state);
                if (state->selected_segment_count > 0U) {
                    delete_active_selection(state);
                }
            }
            return;
        }
        if (button_event->button == SDL_BUTTON_LEFT) {
            state->is_dragging_box = true;
            state->has_box_selection = true;
            state->box_start_x = world_x;
            state->box_start_y = world_y;
            state->box_end_x = world_x;
            state->box_end_y = world_y;
            state->needs_redraw = true;
        }
        return;
    }

    if (button_event->button == SDL_BUTTON_LEFT) {
        int32_t anchor_x = world_x;
        int32_t anchor_y = world_y;
        if (shift_held) {
            anchor_x = snap_coordinate_to_grid(world_x, state->grid.grid_origin_x, state->grid.cross_pitch_units);
            anchor_y = snap_coordinate_to_grid(world_y, state->grid.grid_origin_y, state->grid.cross_pitch_units);
        }
        state->is_dragging_box = true;
        state->has_box_selection = true;
        state->box_start_x = anchor_x;
        state->box_start_y = anchor_y;
        state->box_end_x = anchor_x;
        state->box_end_y = anchor_y;
        if (state->active_tool == TOOL_SPLIT_VERTICAL) {
            state->split_coordinate = anchor_x;
        } else if (state->active_tool == TOOL_SPLIT_HORIZONTAL) {
            state->split_coordinate = anchor_y;
        }
        state->preview_dirty = true;
        state->needs_redraw = true;
    }
}

static void handle_mouse_button_up(
    EditorState *state,
    const SDL_MouseButtonEvent *button_event,
    int32_t window_width,
    int32_t window_height
) {
    const SDL_Keymod key_mod = SDL_GetModState();
    const bool shift_held = (key_mod & KMOD_SHIFT) != 0;

    if (state->strip_modal_open) {
        state->strip_is_painting = false;
        state->strip_is_erasing = false;
        state->strip_stroke_pushed_undo = false;
        return;
    }
    if (state->is_dragging_slider && button_event->button == SDL_BUTTON_LEFT) {
        state->is_dragging_slider = false;
        return;
    }
    if (state->is_panning) {
        state->is_panning = false;
        return;
    }
    if (state->is_drawing_crosses) {
        state->is_drawing_crosses = false;
        if (state->draw_stroke_pushed_undo) {
            history_push(&state->history, &state->design);
        }
        state->draw_stroke_pushed_undo = false;
        return;
    }
    if (!state->is_dragging_box || button_event->button != SDL_BUTTON_LEFT) {
        return;
    }
    state->is_dragging_box = false;

    const int32_t world_x = screen_to_world_x(state, window_width, window_height, button_event->x);
    const int32_t world_y = screen_to_world_y(state, window_width, window_height, button_event->y);

    if (state->active_tool == TOOL_SELECT_CROSS) {
        state->has_box_selection = false;
        int32_t drag_delta_x = state->box_end_x - state->box_start_x;
        if (drag_delta_x < 0) {
            drag_delta_x = -drag_delta_x;
        }
        int32_t drag_delta_y = state->box_end_y - state->box_start_y;
        if (drag_delta_y < 0) {
            drag_delta_y = -drag_delta_y;
        }
        if (!ensure_selection_mask_allocated(state)) {
            return;
        }
        if (!shift_held) {
            memset(state->selection_mask, 0, state->selection_mask_length);
        }
        if (drag_delta_x < 4 && drag_delta_y < 4) {
            select_cross_at_point_in_mask(&state->design, state->grid, world_x, world_y, state->selection_mask, state->selection_mask_length, shift_held, nullptr);
        } else {
            const CutRectangle selection_box = {state->box_start_x, state->box_start_y, state->box_end_x, state->box_end_y};
            select_crosses_in_rectangle_mask(&state->design, state->grid, selection_box, state->selection_mask, state->selection_mask_length);
        }
        recount_selected_segments(state);
        char message[128] = {};
        snprintf(message, sizeof(message), "Sel X: %zu segments | Del, Recolor, Arrows", state->selected_segment_count);
        set_status_message(state, message);
        return;
    }

    if (state->active_tool == TOOL_CLONE) {
        state->has_box_selection = false;
        const CutRectangle clone_box = {state->box_start_x, state->box_start_y, state->box_end_x, state->box_end_y};
        StitchDesign cloned_patch = {};
        if (apply_edit_operation(&state->design, state->grid, EDIT_CROP_KEEP, clone_box, 0, false, &cloned_patch) &&
            cloned_patch.chain_count > 0U) {
            clear_floating_fragment(state);
            state->floating_fragment = cloned_patch;
            state->has_floating_fragment = true;
            state->floating_is_grid_origin_aligned = false;
            state->floating_anchor_x = (cloned_patch.minimum_x + cloned_patch.maximum_x) / 2;
            state->floating_anchor_y = (cloned_patch.minimum_y + cloned_patch.maximum_y) / 2;
            state->floating_offset_x = 0;
            state->floating_offset_y = 0;
            set_status_message(state, "Clone ready: move/arrows, Click/Enter stamp, Shift+Click multi");
        } else {
            free_stitch_design(&cloned_patch);
            set_status_message(state, "Clone box contained no crosses");
        }
        return;
    }

    state->preview_dirty = true;
    state->needs_redraw = true;
}

static void handle_mouse_motion(
    EditorState *state,
    const SDL_MouseMotionEvent *motion_event,
    int32_t window_width,
    int32_t window_height
) {
    state->mouse_screen_x = motion_event->x;
    state->mouse_screen_y = motion_event->y;
    state->needs_redraw = true;

    if (state->palette_target != PALETTE_TARGET_NONE || state->file_dialog.mode != FILE_DIALOG_CLOSED) {
        return;
    }

    if (state->strip_modal_open) {
        if (state->strip_is_painting || state->strip_is_erasing) {
            const StripModalLayout layout = compute_strip_modal_layout(state, window_width, window_height);
            paint_strip_cell_at_mouse(state, &layout, motion_event->x, motion_event->y, state->strip_is_erasing);
        }
        return;
    }

    if (state->is_dragging_slider) {
        update_slider_from_mouse(state, motion_event->x);
        return;
    }

    if (state->is_panning) {
        const double delta_x = (double)(motion_event->x - state->pan_last_x);
        const double delta_y = (double)(motion_event->y - state->pan_last_y);
        state->camera_center_x -= delta_x / state->pixels_per_unit;
        state->camera_center_y -= delta_y / state->pixels_per_unit;
        state->pan_last_x = motion_event->x;
        state->pan_last_y = motion_event->y;
        return;
    }

    const int32_t world_x = screen_to_world_x(state, window_width, window_height, motion_event->x);
    const int32_t world_y = screen_to_world_y(state, window_width, window_height, motion_event->y);

    if (state->has_floating_fragment) {
        const int32_t pitch = state->grid.cross_pitch_units;
        if (state->floating_is_grid_origin_aligned) {
            const int32_t phase_x = state->grid.grid_origin_x - state->floating_base_origin_x;
            const int32_t phase_y = state->grid.grid_origin_y - state->floating_base_origin_y;
            state->floating_offset_x = snap_coordinate_to_grid(world_x, phase_x, pitch);
            state->floating_offset_y = snap_coordinate_to_grid(world_y, phase_y, pitch);
        } else {
            state->floating_offset_x = snap_coordinate_to_grid(world_x - state->floating_anchor_x, 0, pitch);
            state->floating_offset_y = snap_coordinate_to_grid(world_y - state->floating_anchor_y, 0, pitch);
        }
        return;
    }

    if (state->is_drawing_crosses) {
        apply_draw_x_stroke_at(state, world_x, world_y, state->drawing_erases);
        return;
    }

    if (state->is_dragging_box) {
        const bool shift_held = (SDL_GetModState() & KMOD_SHIFT) != 0;
        if (state->active_tool == TOOL_SELECT_CROSS || !shift_held) {
            state->box_end_x = world_x;
            state->box_end_y = world_y;
        } else {
            state->box_end_x = snap_coordinate_to_grid(world_x, state->grid.grid_origin_x, state->grid.cross_pitch_units);
            state->box_end_y = snap_coordinate_to_grid(world_y, state->grid.grid_origin_y, state->grid.cross_pitch_units);
        }
        if (state->active_tool == TOOL_SPLIT_VERTICAL) {
            state->split_coordinate = state->box_end_x;
        } else if (state->active_tool == TOOL_SPLIT_HORIZONTAL) {
            state->split_coordinate = state->box_end_y;
        }
        if (tool_uses_box_preview(state->active_tool)) {
            state->preview_dirty = true;
        }
        return;
    }

    update_canvas_hover(state, motion_event->x, motion_event->y, window_width, window_height);
}

static void handle_mouse_wheel(
    EditorState *state,
    const SDL_MouseWheelEvent *wheel_event,
    int32_t window_width,
    int32_t window_height
) {
    if (state->palette_target != PALETTE_TARGET_NONE) {
        return;
    }

    if (state->file_dialog.mode != FILE_DIALOG_CLOSED) {
        const FileDialogLayout layout = compute_file_dialog_layout(state, window_width, window_height);
        state->file_dialog.scroll_offset -= wheel_event->y * FILE_ROW_HEIGHT * 2;
        const int32_t content_height = (int32_t)state->file_dialog.entry_count * FILE_ROW_HEIGHT;
        int32_t max_scroll = content_height - layout.list_height;
        if (max_scroll < 0) {
            max_scroll = 0;
        }
        if (state->file_dialog.scroll_offset < 0) {
            state->file_dialog.scroll_offset = 0;
        }
        if (state->file_dialog.scroll_offset > max_scroll) {
            state->file_dialog.scroll_offset = max_scroll;
        }
        state->needs_redraw = true;
        return;
    }

    if (state->strip_modal_open) {
        return;
    }

    if (state->mouse_screen_x < LEFT_PANEL_WIDTH &&
        state->mouse_screen_y >= state->toolbar_height &&
        state->mouse_screen_y < window_height - BOTTOM_PANEL_HEIGHT) {
        state->color_scroll_offset -= wheel_event->y * COLOR_ROW_HEIGHT;
        const int32_t list_height = window_height - state->toolbar_height - BOTTOM_PANEL_HEIGHT - 62;
        const int32_t content_height = (int32_t)state->design.block_count * COLOR_ROW_HEIGHT;
        int32_t maximum_scroll = content_height - list_height;
        if (maximum_scroll < 0) {
            maximum_scroll = 0;
        }
        if (state->color_scroll_offset < 0) {
            state->color_scroll_offset = 0;
        }
        if (state->color_scroll_offset > maximum_scroll) {
            state->color_scroll_offset = maximum_scroll;
        }
        state->needs_redraw = true;
        return;
    }

    const int32_t before_world_x = screen_to_world_x(state, window_width, window_height, state->mouse_screen_x);
    const int32_t before_world_y = screen_to_world_y(state, window_width, window_height, state->mouse_screen_y);

    if (wheel_event->y > 0) {
        state->pixels_per_unit *= 1.18;
    } else if (wheel_event->y < 0) {
        state->pixels_per_unit /= 1.18;
    }
    if (state->pixels_per_unit < 0.04) {
        state->pixels_per_unit = 0.04;
    }
    if (state->pixels_per_unit > 28.0) {
        state->pixels_per_unit = 28.0;
    }

    const int32_t after_world_x = screen_to_world_x(state, window_width, window_height, state->mouse_screen_x);
    const int32_t after_world_y = screen_to_world_y(state, window_width, window_height, state->mouse_screen_y);
    state->camera_center_x += (double)(before_world_x - after_world_x);
    state->camera_center_y += (double)(before_world_y - after_world_y);
    state->needs_redraw = true;
}

static void handle_text_input(EditorState *state, const SDL_TextInputEvent *text_event) {
    if (state->file_dialog.mode != FILE_DIALOG_SAVE_AS &&
        state->file_dialog.mode != FILE_DIALOG_STRIP_SAVE_AS) {
        return;
    }
    FileDialogState *dialog = &state->file_dialog;
    size_t current_len = strlen(dialog->filename_input);
    for (size_t index = 0U; text_event->text[index] != '\0'; index += 1U) {
        const unsigned char byte = (unsigned char)text_event->text[index];
        if (byte < 32U || byte == '/' || byte > 126U) {
            continue;
        }
        if (current_len + 1U < sizeof(dialog->filename_input)) {
            dialog->filename_input[current_len] = (char)byte;
            current_len += 1U;
            dialog->filename_input[current_len] = '\0';
        }
    }
    dialog->confirm_overwrite = false;
    dialog->error_text[0] = '\0';
    state->needs_redraw = true;
}

static void handle_key_down(
    EditorState *state,
    const SDL_KeyboardEvent *key_event,
    int32_t window_width,
    int32_t window_height
) {
    const SDL_Keycode key_code = key_event->keysym.sym;
    const bool ctrl_held = (key_event->keysym.mod & KMOD_CTRL) != 0;
    const bool shift_held = (key_event->keysym.mod & KMOD_SHIFT) != 0;

    if (key_code == SDLK_SPACE && state->file_dialog.mode == FILE_DIALOG_CLOSED) {
        state->space_is_held = true;
        return;
    }

    if (state->palette_target != PALETTE_TARGET_NONE) {
        if (key_code == SDLK_ESCAPE) {
            state->palette_target = PALETTE_TARGET_NONE;
            state->needs_redraw = true;
        }
        return;
    }

    if (state->file_dialog.mode != FILE_DIALOG_CLOSED) {
        if (key_code == SDLK_ESCAPE) {
            close_file_dialog(state);
        } else if (key_code == SDLK_RETURN || key_code == SDLK_KP_ENTER) {
            execute_file_dialog_confirm(state, window_width, window_height);
        } else if (key_code == SDLK_BACKSPACE &&
                   (state->file_dialog.mode == FILE_DIALOG_SAVE_AS ||
                    state->file_dialog.mode == FILE_DIALOG_STRIP_SAVE_AS)) {
            const size_t length = strlen(state->file_dialog.filename_input);
            if (length > 0U) {
                state->file_dialog.filename_input[length - 1U] = '\0';
                state->file_dialog.confirm_overwrite = false;
                state->file_dialog.error_text[0] = '\0';
                state->needs_redraw = true;
            }
        }
        return;
    }

    if (key_code == SDLK_ESCAPE) {
        if (state->strip_modal_open) {
            state->strip_modal_open = false;
            state->needs_redraw = true;
            return;
        }
        if (state->has_floating_fragment) {
            clear_floating_fragment(state);
            set_status_message(state, "Cancelled placement");
            return;
        }
        state->has_box_selection = false;
        state->is_dragging_box = false;
        clear_live_preview(state);
        clear_segment_selection(state);
        clear_hover_preview(state);
        set_status_message(state, "Cleared selection");
        return;
    }

    if (state->strip_modal_open) {
        if (ctrl_held && key_code == SDLK_z) {
            handle_strip_command(state, STRIP_CMD_UNDO, window_width, window_height);
        }
        return;
    }

    if (ctrl_held && key_code == SDLK_q) {
        state->should_quit = true;
        return;
    }
    if (ctrl_held && key_code == SDLK_o) {
        open_file_dialog(state, FILE_DIALOG_OPEN);
        return;
    }
    if (ctrl_held && key_code == SDLK_s) {
        if (shift_held) {
            execute_toolbar_command(state, COMMAND_SAVE_AS_DIALOG, window_width, window_height);
        } else {
            save_current_design(state);
        }
        return;
    }
    if (ctrl_held && key_code == SDLK_z) {
        if (shift_held) {
            execute_toolbar_command(state, COMMAND_REDO, window_width, window_height);
        } else {
            execute_toolbar_command(state, COMMAND_UNDO, window_width, window_height);
        }
        return;
    }
    if (ctrl_held && key_code == SDLK_y) {
        execute_toolbar_command(state, COMMAND_REDO, window_width, window_height);
        return;
    }

    switch (key_code) {
        case SDLK_1:
        case SDLK_e:
            execute_toolbar_command(state, COMMAND_TOOL_SELECT_ELEMENT, window_width, window_height);
            break;
        case SDLK_2:
        case SDLK_x:
            execute_toolbar_command(state, COMMAND_TOOL_SELECT_CROSS, window_width, window_height);
            break;
        case SDLK_3:
        case SDLK_d:
            execute_toolbar_command(state, COMMAND_TOOL_DRAW_CROSS, window_width, window_height);
            break;
        case SDLK_m:
            execute_toolbar_command(state, COMMAND_OPEN_STRIP_MODAL, window_width, window_height);
            break;
        case SDLK_4:
            execute_toolbar_command(state, COMMAND_TOOL_CROP, window_width, window_height);
            break;
        case SDLK_5:
            execute_toolbar_command(state, COMMAND_TOOL_CUT, window_width, window_height);
            break;
        case SDLK_6:
            execute_toolbar_command(state, COMMAND_TOOL_SPLIT_V, window_width, window_height);
            break;
        case SDLK_7:
            execute_toolbar_command(state, COMMAND_TOOL_SPLIT_H, window_width, window_height);
            break;
        case SDLK_8:
            execute_toolbar_command(state, COMMAND_TOOL_CLONE, window_width, window_height);
            break;
        case SDLK_9:
        case SDLK_i:
            execute_toolbar_command(state, COMMAND_TOOL_PICK, window_width, window_height);
            break;
        case SDLK_r:
            execute_toolbar_command(state, COMMAND_ROTATE_90, window_width, window_height);
            break;
        case SDLK_h:
            execute_toolbar_command(state, COMMAND_FLIP_X, window_width, window_height);
            break;
        case SDLK_v:
            execute_toolbar_command(state, COMMAND_FLIP_Y, window_width, window_height);
            break;
        case SDLK_c:
            execute_toolbar_command(state, COMMAND_CENTER, window_width, window_height);
            break;
        case SDLK_g:
            execute_toolbar_command(state, COMMAND_TOGGLE_GRID, window_width, window_height);
            break;
        case SDLK_j:
            execute_toolbar_command(state, COMMAND_TOGGLE_JUMPS, window_width, window_height);
            break;
        case SDLK_l:
            execute_toolbar_command(state, COMMAND_TOGGLE_LOCKS, window_width, window_height);
            break;
        case SDLK_o:
            execute_toolbar_command(state, COMMAND_CYCLE_HOOP, window_width, window_height);
            break;
        case SDLK_t:
            execute_toolbar_command(state, COMMAND_CYCLE_VIEW_MODE, window_width, window_height);
            break;
        case SDLK_w:
            execute_toolbar_command(state, COMMAND_CYCLE_THREAD_THICKNESS, window_width, window_height);
            break;
        case SDLK_f:
        case SDLK_0:
            execute_toolbar_command(state, COMMAND_FIT_VIEW, window_width, window_height);
            break;
        case SDLK_DELETE:
        case SDLK_BACKSPACE:
            delete_active_selection(state);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (state->has_floating_fragment) {
                commit_floating_fragment(state, shift_held);
            } else {
                apply_box_edit_or_split(state);
            }
            break;
        case SDLK_LEFT:
            nudge_active_selection_or_fragment(state, -1, 0, shift_held);
            break;
        case SDLK_RIGHT:
            nudge_active_selection_or_fragment(state, 1, 0, shift_held);
            break;
        case SDLK_UP:
            nudge_active_selection_or_fragment(state, 0, -1, shift_held);
            break;
        case SDLK_DOWN:
            nudge_active_selection_or_fragment(state, 0, 1, shift_held);
            break;
        default:
            break;
    }
}

static bool resize_pixel_canvas(
    PixelCanvas *canvas,
    SDL_Renderer *renderer,
    SDL_Texture **texture_slot,
    int32_t new_width,
    int32_t new_height
) {
    if (new_width < 64) {
        new_width = 64;
    }
    if (new_height < 64) {
        new_height = 64;
    }
    if (canvas->width == new_width && canvas->height == new_height &&
        canvas->pixels != nullptr && *texture_slot != nullptr) {
        return true;
    }

    size_t pixel_count = 0U;
    size_t pixel_bytes = 0U;
    if (ckd_mul(&pixel_count, (size_t)new_width, (size_t)new_height) ||
        ckd_mul(&pixel_bytes, pixel_count, sizeof(uint32_t))) {
        return false;
    }

    uint32_t *next_pixels = (uint32_t *)malloc(pixel_bytes);
    if (next_pixels == nullptr) {
        return false;
    }
    SDL_Texture *next_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        new_width,
        new_height
    );
    if (next_texture == nullptr) {
        free(next_pixels);
        return false;
    }

    if (*texture_slot != nullptr) {
        SDL_DestroyTexture(*texture_slot);
    }
    free(canvas->pixels);

    *texture_slot = next_texture;
    canvas->pixels = next_pixels;
    canvas->width = new_width;
    canvas->height = new_height;
    reset_canvas_clip(canvas);
    return true;
}

static void init_default_editor_state(EditorState *state) {
    *state = (EditorState){};
    state->grid.cross_pitch_units = DEFAULT_CROSS_PITCH_UNITS;
    state->grid.cross_passes = 3;
    state->grid.raw_detected_passes = 3;

    state->strip_pattern.width = 5;
    state->strip_pattern.height = 5;
    state->strip_pattern.repeat_x = 1;
    state->strip_pattern.repeat_y = 6;
    state->strip_pattern.pitch_units = DEFAULT_CROSS_PITCH_UNITS;
    state->strip_pattern.passes = 3;
    state->strip_active_color = 10U;

    state->active_tool = TOOL_SELECT_ELEMENT;
    state->view_mode = VIEW_MODE_THREAD;
    state->thread_thickness_index = 1U;
    state->block_dedup_table = (BlockDedupSlot *)calloc(BLOCK_DEDUP_CAPACITY, sizeof(BlockDedupSlot));
    state->block_dedup_generation = 0U;
    state->show_grid = true;
    state->show_jumps = false;
    state->show_locks = false;
    state->hoop_preset_index = 4U;
    state->simulator_percent = 100.0;
    state->pixels_per_unit = 0.5;
    state->hover_seed_chain = SIZE_MAX;
    state->hovered_palette_index = -1;
    state->file_dialog.selected_index = SIZE_MAX;
    state->file_dialog.last_click_index = SIZE_MAX;
    state->needs_redraw = true;
    set_status_message(state, "Ready | Open File, New Strip, or Drag & Drop");
}

static void free_editor_state(EditorState *state) {
    free(state->block_dedup_table);
    state->block_dedup_table = nullptr;
    free(state->file_dialog.entries);
    state->file_dialog.entries = nullptr;
    clear_segment_selection(state);
    clear_hover_preview(state);
    clear_floating_fragment(state);
    clear_live_preview(state);
    history_clear(&state->history);
    free_stitch_design(&state->design);
}

static void print_usage(FILE *stream) {
    fprintf(stream, "Usage: embroidery-studio [input.{jef,jef+,pes,pec,dst}]\n");
}

int main(int argument_count, char **argument_values) {
    int exit_code = EXIT_FAILURE;
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *framebuffer_texture = nullptr;
    bool sdl_initialized = false;
    PixelCanvas canvas = {};
    EditorState state = {};

    init_default_editor_state(&state);

    const char *initial_input_path = nullptr;
    if (argument_count == 2) {
        if (strcmp(argument_values[1], "-h") == 0 || strcmp(argument_values[1], "--help") == 0) {
            print_usage(stdout);
            exit_code = EXIT_SUCCESS;
            goto cleanup;
        }
        if (argument_values[1][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argument_values[1]);
            print_usage(stderr);
            goto cleanup;
        }
        initial_input_path = argument_values[1];
    } else if (argument_count > 2) {
        fprintf(stderr, "Too many arguments\n");
        print_usage(stderr);
        goto cleanup;
    }

    if (getenv("SDL_VIDEODRIVER") == nullptr) {
        SDL_SetHint(SDL_HINT_VIDEODRIVER, "wayland,x11");
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        goto cleanup;
    }
    sdl_initialized = true;
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    window = SDL_CreateWindow(
        "embroidery-studio (Janome MC550E)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_INITIAL_WIDTH,
        WINDOW_INITIAL_HEIGHT,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );
    if (window == nullptr) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == nullptr) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    int window_width = WINDOW_INITIAL_WIDTH;
    int window_height = WINDOW_INITIAL_HEIGHT;
    SDL_GetWindowSize(window, &window_width, &window_height);
    if (!resize_pixel_canvas(&canvas, renderer, &framebuffer_texture, window_width, window_height)) {
        fprintf(stderr, "Failed to allocate initial framebuffer\n");
        goto cleanup;
    }
    layout_toolbar_buttons(&state, canvas.width);

    if (initial_input_path != nullptr) {
        if (!load_design_into_editor(&state, initial_input_path, canvas.width, canvas.height)) {
            goto cleanup;
        }
    }

    while (!state.should_quit) {
        SDL_Event event = {};
        if (SDL_WaitEventTimeout(&event, 16) != 0) {
            do {
                if (event.type == SDL_QUIT) {
                    state.should_quit = true;
                } else if (event.type == SDL_WINDOWEVENT &&
                           (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                            event.window.event == SDL_WINDOWEVENT_EXPOSED)) {
                    SDL_GetWindowSize(window, &window_width, &window_height);
                    if (!resize_pixel_canvas(&canvas, renderer, &framebuffer_texture, window_width, window_height)) {
                        state.should_quit = true;
                        break;
                    }
                    state.needs_redraw = true;
                } else if (event.type == SDL_DROPFILE && event.drop.file != nullptr) {
                    if (state.file_dialog.mode != FILE_DIALOG_CLOSED) {
                        close_file_dialog(&state);
                    }
                    if (state.design.chain_count == 0U || !state.drop_merge_mode) {
                        load_design_into_editor(&state, event.drop.file, canvas.width, canvas.height);
                    } else {
                        load_dropped_file_as_merge_fragment(&state, event.drop.file, canvas.width, canvas.height);
                    }
                    SDL_free(event.drop.file);
                } else if (event.type == SDL_TEXTINPUT) {
                    handle_text_input(&state, &event.text);
                } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    handle_mouse_button_down(&state, &event.button, canvas.width, canvas.height);
                } else if (event.type == SDL_MOUSEBUTTONUP) {
                    handle_mouse_button_up(&state, &event.button, canvas.width, canvas.height);
                } else if (event.type == SDL_MOUSEMOTION) {
                    handle_mouse_motion(&state, &event.motion, canvas.width, canvas.height);
                } else if (event.type == SDL_MOUSEWHEEL) {
                    handle_mouse_wheel(&state, &event.wheel, canvas.width, canvas.height);
                } else if (event.type == SDL_KEYDOWN) {
                    handle_key_down(&state, &event.key, canvas.width, canvas.height);
                } else if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_SPACE) {
                    state.space_is_held = false;
                }
            } while (SDL_PollEvent(&event) != 0);
        }

        if (state.needs_redraw) {
            render_frame(&canvas, &state);
            SDL_UpdateTexture(framebuffer_texture, nullptr, canvas.pixels, canvas.width * (int)sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, framebuffer_texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            state.needs_redraw = false;
        }
    }

    exit_code = EXIT_SUCCESS;

cleanup:
    if (state.file_dialog.mode == FILE_DIALOG_SAVE_AS ||
        state.file_dialog.mode == FILE_DIALOG_STRIP_SAVE_AS) {
        SDL_StopTextInput();
    }
    free_editor_state(&state);
    free(canvas.pixels);
    if (framebuffer_texture != nullptr) {
        SDL_DestroyTexture(framebuffer_texture);
    }
    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
    }
    if (sdl_initialized) {
        SDL_Quit();
    }
    return exit_code;
}
