#ifndef EMBROIDERY_MODEL_H
#define EMBROIDERY_MODEL_H

#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>

constexpr size_t MAX_INPUT_FILE_SIZE = 67108864U;
constexpr size_t MAX_DESIGN_POINTS = 16777216U;
constexpr size_t MAX_DESIGN_CHAINS = 4194304U;
constexpr size_t MAX_COLOR_BLOCKS = 4096U;
constexpr size_t JEF_HEADER_SIZE = 116U;
constexpr size_t PEC_HEADER_SIZE = 532U;
constexpr size_t DST_HEADER_SIZE = 512U;
constexpr size_t JANOME_PALETTE_COUNT = 79U;
constexpr size_t BROTHER_PALETTE_COUNT = 65U;
constexpr size_t DST_DEFAULT_PALETTE_COUNT = 8U;
constexpr int32_t LOCK_STITCH_UNITS = 4;
constexpr int32_t JEF_MAX_STEP = 127;
constexpr int32_t MIN_CROSS_PITCH_UNITS = 10;
constexpr int32_t MAX_CROSS_PITCH_UNITS = 50;
constexpr int32_t DEFAULT_CROSS_PITCH_UNITS = 18;
constexpr int32_t MIN_CROSS_PASSES = 1;
constexpr int32_t MAX_CROSS_PASSES = 6;
constexpr int32_t STRIP_MAX_GRID_DIM = 64;
constexpr int32_t STRIP_MAX_REPEAT = 64;
constexpr size_t STRIP_MAX_EXPANDED_CELLS = 262144U;
constexpr size_t MAX_PATH_BUFFER = 4096U;
constexpr int32_t SPATIAL_GRID_MAX_DIM = 256;
constexpr size_t SCRATCH_ARENA_ALIGN = 16U;

/* Bound coordinates to +/-10 km in 0.1 mm stitch units to prevent overflow. */
constexpr int64_t MAX_COORDINATE_MAGNITUDE = 100000000LL;

static_assert(JANOME_PALETTE_COUNT == 79U, "Janome palette must contain 79 entries");
static_assert(BROTHER_PALETTE_COUNT == 65U, "Brother palette must contain 65 entries");
static_assert(DST_DEFAULT_PALETTE_COUNT == 8U, "DST default palette must contain 8 entries");
static_assert(MIN_CROSS_PITCH_UNITS > 0 && MAX_CROSS_PITCH_UNITS >= MIN_CROSS_PITCH_UNITS);
static_assert(MIN_CROSS_PASSES >= 1 && MAX_CROSS_PASSES >= MIN_CROSS_PASSES);
static_assert((SCRATCH_ARENA_ALIGN & (SCRATCH_ARENA_ALIGN - 1U)) == 0U);

typedef enum : uint8_t {
    FORMAT_UNKNOWN = 0U,
    FORMAT_JEF = 1U,
    FORMAT_PES = 2U,
    FORMAT_PEC = 3U,
    FORMAT_DST = 4U
} EmbroideryFormat;

typedef enum : uint8_t {
    EDIT_CROP_KEEP = 0U,
    EDIT_CUT_OUT = 1U,
    EDIT_SPLIT_VERTICAL = 2U,
    EDIT_SPLIT_HORIZONTAL = 3U
} EditOperationKind;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RgbColor;

typedef struct {
    int32_t position_x;
    int32_t position_y;
} StitchPoint;

typedef struct {
    size_t ref_count;
    size_t capacity;
    StitchPoint *points;
} SharedPointBuffer;

typedef struct {
    size_t point_offset;
    size_t point_count;
    size_t color_block_index;
    bool start_was_cut;
    bool end_was_cut;
} StitchChain;

typedef struct {
    RgbColor display_color;
    uint8_t janome_color_index;
    bool enabled;
} ColorBlock;

typedef struct {
    size_t stitch_count;
    size_t chain_count;
    size_t color_count;
    size_t jump_count;
    size_t lock_count;
} DesignStats;

typedef struct {
    ColorBlock *blocks;
    size_t block_count;
    size_t block_capacity;
    StitchChain *chains;
    size_t chain_count;
    size_t chain_capacity;
    SharedPointBuffer *point_buffer;
    StitchPoint *points;
    size_t point_count;
    size_t point_capacity;
    int32_t minimum_x;
    int32_t maximum_x;
    int32_t minimum_y;
    int32_t maximum_y;
    bool has_bounds;
    bool has_manual_center_shift;
    DesignStats cached_stats;
    bool stats_valid;
    uint64_t geometry_revision;
} StitchDesign;

typedef struct {
    int32_t cross_pitch_units;
    int32_t grid_origin_x;
    int32_t grid_origin_y;
    int32_t cross_passes;
    int32_t raw_detected_passes;
    size_t diagonal_stitch_count;
    bool detected_from_design;
} CrossGridInfo;

typedef struct {
    int32_t width;
    int32_t height;
    int32_t repeat_x;
    int32_t repeat_y;
    int32_t pitch_units;
    int32_t passes;
    uint8_t cells[STRIP_MAX_GRID_DIM * STRIP_MAX_GRID_DIM];
} StripPattern;

typedef struct {
    int32_t minimum_x;
    int32_t minimum_y;
    int32_t maximum_x;
    int32_t maximum_y;
} CutRectangle;

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t used;
} ScratchArena;

typedef struct {
    uint32_t chain_index;
    uint32_t local_index;
    uint32_t flat_index;
} SpatialSegmentEntry;

typedef struct {
    int32_t origin_x;
    int32_t origin_y;
    int32_t cell_size;
    int32_t grid_cols;
    int32_t grid_rows;
    int32_t max_cell_segment_half_span;
    size_t cell_count;
    size_t entry_count;
    size_t overflow_count;
    size_t total_segments;
    size_t chain_count;
    uint32_t *cell_offsets;
    SpatialSegmentEntry *entries;
    SpatialSegmentEntry *overflow_entries;
    size_t *chain_segment_offsets;
    int32_t component_pitch_units;
    size_t component_count;
    size_t *chain_component_id;
    size_t *component_chain_offsets;
    size_t *component_chains;
    uint64_t built_revision;
    bool valid;
} DesignSpatialIndex;

extern const RgbColor janome_palette[JANOME_PALETTE_COUNT];
extern const RgbColor brother_palette[BROTHER_PALETTE_COUNT];
extern const RgbColor dst_default_palette[DST_DEFAULT_PALETTE_COUNT];

uint32_t read_uint32_little_endian(const uint8_t *bytes, size_t offset);
void write_uint32_little_endian(uint8_t *destination, size_t offset, uint32_t value);
void write_int32_little_endian(uint8_t *destination, size_t offset, int32_t value);
unsigned char ascii_lowercase(unsigned char character);
bool strings_equal_ignore_case(const char *left_text, const char *right_text);
int32_t floor_divide_int32(int32_t dividend, int32_t divisor);
int32_t positive_modulo_int32(int32_t value, int32_t modulus);
uint8_t find_nearest_janome_color(RgbColor target_color);

void scratch_arena_init(ScratchArena *arena);
void scratch_arena_free(ScratchArena *arena);
void scratch_arena_reset(ScratchArena *arena);
bool scratch_arena_prepare(ScratchArena *arena, size_t required_bytes);
void *scratch_arena_push(ScratchArena *arena, size_t element_count, size_t element_size);
bool scratch_arena_add_slice_bytes(size_t *total_bytes, size_t element_count, size_t element_size);

void init_stitch_design(StitchDesign *design);
void free_stitch_design(StitchDesign *design);
bool design_ensure_unique_points(StitchDesign *design);
bool design_reserve_points(StitchDesign *design, size_t required_count);
bool design_reserve_chains(StitchDesign *design, size_t required_count);
bool design_reserve_blocks(StitchDesign *design, size_t required_count);
bool design_append_point(StitchDesign *design, int32_t position_x, int32_t position_y);
bool design_append_chain(
    StitchDesign *design,
    size_t point_offset,
    size_t point_count,
    size_t color_block_index,
    bool start_was_cut,
    bool end_was_cut
);
bool design_append_block(
    StitchDesign *design,
    RgbColor display_color,
    uint8_t janome_color_index,
    bool enabled,
    size_t *output_block_index
);
void design_mark_mutated(StitchDesign *design);
size_t design_metadata_bytes(const StitchDesign *design);
size_t design_point_buffer_bytes(const StitchDesign *design);

void expand_bounds_from_chain(
    const StitchDesign *design,
    StitchChain chain,
    bool *has_bounds,
    int64_t *minimum_x,
    int64_t *maximum_x,
    int64_t *minimum_y,
    int64_t *maximum_y
);
void recompute_design_bounds(StitchDesign *design);
void center_stitch_design(StitchDesign *design);
bool clone_stitch_design(const StitchDesign *source, StitchDesign *destination);
bool sort_chains_by_color_block(StitchDesign *design);
bool compact_design_points(StitchDesign *design);
bool compact_non_empty_blocks(StitchDesign *design);
DesignStats compute_design_stats(const StitchDesign *design);
size_t count_design_segments(const StitchDesign *design);
bool point_is_inside_rectangle(StitchPoint point, CutRectangle rectangle);
bool point_is_strictly_inside_rectangle(
    int32_t point_x,
    int32_t point_y,
    CutRectangle rectangle
);

void spatial_index_init(DesignSpatialIndex *index);
void spatial_index_free(DesignSpatialIndex *index);
void spatial_index_invalidate(DesignSpatialIndex *index);
bool spatial_index_build(
    DesignSpatialIndex *index,
    const StitchDesign *design,
    int32_t cross_pitch_units,
    ScratchArena *scratch
);

#endif
