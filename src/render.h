#ifndef EMBROIDERY_RENDER_H
#define EMBROIDERY_RENDER_H

#include "model.h"
#include "ops.h"

constexpr size_t HOOP_PRESET_COUNT = 6U;
constexpr size_t THREAD_THICKNESS_PRESET_COUNT = 3U;
constexpr int32_t MAX_THREAD_STROKE_PX = 24;
constexpr size_t BLOCK_DEDUP_CAPACITY = 65536U;

static_assert(
    (BLOCK_DEDUP_CAPACITY & (BLOCK_DEDUP_CAPACITY - 1U)) == 0U,
    "Block dedup capacity must be a power of two"
);

typedef enum : uint8_t {
    VIEW_MODE_THREAD = 0U,
    VIEW_MODE_BLOCKS = 1U,
    VIEW_MODE_LINE = 2U
} StitchViewMode;

typedef struct {
    const char *label;
    int32_t width_units;
    int32_t height_units;
} HoopPreset;

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
    int32_t left;
    int32_t top;
    int32_t width;
    int32_t height;
} RenderViewport;

typedef struct {
    double center_x;
    double center_y;
    double pixels_per_unit;
} CameraView;

typedef struct {
    size_t color_block_index;
    int32_t center_x;
    int32_t center_y;
    uint8_t visual_flags;
    uint32_t generation;
} BlockDedupSlot;

typedef struct {
    BlockDedupSlot *slots;
    uint32_t generation;
} BlockDedupTable;

typedef struct {
    RenderViewport viewport;
    CameraView camera;
    CrossGridInfo grid;
    StitchViewMode view_mode;
    size_t thread_thickness_index;
    bool show_jumps;
    bool show_locks;
    bool draw_overlays;
    int32_t world_offset_x;
    int32_t world_offset_y;
    const uint8_t *keep_preview_mask;
    size_t keep_preview_mask_length;
    const uint8_t *selection_mask;
    size_t selection_mask_length;
    const uint8_t *hover_mask;
    size_t hover_mask_length;
    size_t maximum_stitches_to_draw;
    BlockDedupTable *block_dedup;
} DesignRenderParams;

extern const HoopPreset janome_550e_hoops[HOOP_PRESET_COUNT];
extern const double thread_thickness_units[THREAD_THICKNESS_PRESET_COUNT];
extern const char *const thread_thickness_labels[THREAD_THICKNESS_PRESET_COUNT];

bool init_pixel_canvas(PixelCanvas *canvas, int32_t width, int32_t height);
void free_pixel_canvas(PixelCanvas *canvas);
bool init_block_dedup_table(BlockDedupTable *table);
void free_block_dedup_table(BlockDedupTable *table);

uint32_t pack_rgb_pixel(uint8_t red, uint8_t green, uint8_t blue);
uint32_t compute_rgb_luminance(RgbColor color);
void compute_thread_colors(
    RgbColor base_rgb,
    bool dimmed,
    uint32_t *core_color_out,
    uint32_t *outline_color_out
);
int32_t compute_thread_stroke_px(size_t thread_thickness_index, double pixels_per_unit);

void reset_canvas_clip(PixelCanvas *canvas);
void put_pixel_clipped(
    PixelCanvas *canvas,
    int32_t pixel_x,
    int32_t pixel_y,
    uint32_t packed_color
);
void blend_pixel_clipped(
    PixelCanvas *canvas,
    int32_t pixel_x,
    int32_t pixel_y,
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    uint8_t alpha
);
void draw_filled_rectangle(
    PixelCanvas *canvas,
    int32_t rectangle_x,
    int32_t rectangle_y,
    int32_t rectangle_width,
    int32_t rectangle_height,
    uint32_t packed_color
);
void draw_blended_rectangle(
    PixelCanvas *canvas,
    int32_t rectangle_x,
    int32_t rectangle_y,
    int32_t rectangle_width,
    int32_t rectangle_height,
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    uint8_t alpha
);
void draw_line_clipped(
    PixelCanvas *canvas,
    int32_t start_x,
    int32_t start_y,
    int32_t end_x,
    int32_t end_y,
    uint32_t packed_color,
    int32_t dash_period
);
void draw_thick_line_clipped(
    PixelCanvas *canvas,
    int32_t start_x,
    int32_t start_y,
    int32_t end_x,
    int32_t end_y,
    uint32_t packed_color
);
void draw_rectangle_outline(
    PixelCanvas *canvas,
    int32_t rectangle_x,
    int32_t rectangle_y,
    int32_t rectangle_width,
    int32_t rectangle_height,
    uint32_t packed_color,
    int32_t dash_period
);
void draw_thread_capsule_clipped(
    PixelCanvas *canvas,
    int32_t start_x,
    int32_t start_y,
    int32_t end_x,
    int32_t end_y,
    int32_t stroke_px,
    uint32_t core_color,
    uint32_t outline_color
);

int32_t viewport_world_to_screen_x(RenderViewport viewport, CameraView camera, int32_t world_x);
int32_t viewport_world_to_screen_y(RenderViewport viewport, CameraView camera, int32_t world_y);
int32_t viewport_screen_to_world_x(RenderViewport viewport, CameraView camera, int32_t screen_x);
int32_t viewport_screen_to_world_y(RenderViewport viewport, CameraView camera, int32_t screen_y);

bool get_hoop_dimensions(
    size_t hoop_preset_index,
    bool hoop_rotated,
    int32_t *width_units_out,
    int32_t *height_units_out
);
bool design_exceeds_hoop(
    const StitchDesign *design,
    size_t hoop_preset_index,
    bool hoop_rotated
);

void draw_cross_grid_background(
    PixelCanvas *canvas,
    RenderViewport viewport,
    CameraView camera,
    CrossGridInfo grid,
    bool show_grid
);
void draw_hoop_overlay(
    PixelCanvas *canvas,
    RenderViewport viewport,
    CameraView camera,
    const StitchDesign *design,
    size_t hoop_preset_index,
    bool hoop_rotated
);
void render_design_chains(
    PixelCanvas *canvas,
    const StitchDesign *design,
    const DesignRenderParams *params
);

#endif
