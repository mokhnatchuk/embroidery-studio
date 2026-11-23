#ifndef EMBROIDERY_CORE_H
#define EMBROIDERY_CORE_H

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdckdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

/* Bound coordinates to +/-10 km in 0.1 mm stitch units to prevent overflow. */
constexpr int64_t MAX_COORDINATE_MAGNITUDE = 100000000LL;

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
    ColorBlock *blocks;
    size_t block_count;
    size_t block_capacity;
    StitchChain *chains;
    size_t chain_count;
    size_t chain_capacity;
    StitchPoint *points;
    size_t point_count;
    size_t point_capacity;
    int32_t minimum_x;
    int32_t maximum_x;
    int32_t minimum_y;
    int32_t maximum_y;
    bool has_bounds;
    bool has_manual_center_shift;
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
    size_t stitch_count;
    size_t chain_count;
    size_t color_count;
    size_t jump_count;
    size_t lock_count;
} DesignStats;

typedef struct {
    uint8_t *bytes;
    size_t byte_count;
    size_t byte_capacity;
} ByteBuffer;

typedef struct {
    StitchDesign *design;
    int64_t current_x;
    int64_t current_y;
    size_t active_block_index;
    size_t active_chain_point_offset;
    size_t active_chain_point_count;
    bool chain_is_open;
} DesignBuilder;

typedef struct {
    int32_t first_x;
    int32_t first_y;
    int32_t second_x;
    int32_t second_y;
    uint32_t hit_count;
    bool is_occupied;
} DiagonalEdgeSlot;

typedef struct {
    int32_t position_x;
    int32_t position_y;
    size_t local_chain_index;
    int32_t next_node_index;
} SpatialVertexNode;

typedef struct {
    uint8_t color_index;
    int64_t sum_center_x;
    int64_t sum_center_y;
    int32_t canonical_center_x;
    int32_t canonical_center_y;
    uint32_t diagonal_count;
    bool keep_cluster;
} CrossCluster;

typedef struct {
    size_t cluster_index;
    int32_t next_node_index;
} CrossClusterNode;

/* Standard Janome JEF thread palette for indices 0..78. */
static const RgbColor janome_palette[JANOME_PALETTE_COUNT] = {
    {0x00U, 0x00U, 0x00U}, {0x00U, 0x00U, 0x00U}, {0xFFU, 0xFFU, 0xFFU},
    {0xFFU, 0xFFU, 0x17U}, {0xFFU, 0x66U, 0x00U}, {0x2FU, 0x59U, 0x33U},
    {0x23U, 0x73U, 0x36U}, {0x65U, 0xC2U, 0xC8U}, {0xABU, 0x5AU, 0x96U},
    {0xF6U, 0x69U, 0xA0U}, {0xFFU, 0x00U, 0x00U}, {0xB1U, 0x70U, 0x4EU},
    {0x0BU, 0x2FU, 0x84U}, {0xE4U, 0xC3U, 0x5DU}, {0x48U, 0x1AU, 0x05U},
    {0xACU, 0x9CU, 0xC7U}, {0xFCU, 0xF2U, 0x94U}, {0xF9U, 0x99U, 0xB7U},
    {0xFAU, 0xB3U, 0x81U}, {0xC9U, 0xA4U, 0x80U}, {0x97U, 0x05U, 0x33U},
    {0xA0U, 0xB8U, 0xCCU}, {0x7FU, 0xC2U, 0x1CU}, {0xE5U, 0xE5U, 0xE5U},
    {0x88U, 0x9BU, 0x9BU}, {0x98U, 0xD6U, 0xBDU}, {0xB2U, 0xE1U, 0xE3U},
    {0x36U, 0x8BU, 0xA0U}, {0x4FU, 0x83U, 0xABU}, {0x38U, 0x6AU, 0x91U},
    {0x07U, 0x16U, 0x50U}, {0xF9U, 0x99U, 0xA2U}, {0xF9U, 0x67U, 0x6BU},
    {0xE3U, 0x31U, 0x1FU}, {0xE2U, 0xA1U, 0x88U}, {0xB5U, 0x94U, 0x74U},
    {0xE4U, 0xCFU, 0x99U}, {0xFFU, 0xCBU, 0x00U}, {0xE1U, 0xADU, 0xD4U},
    {0xC3U, 0x00U, 0x7EU}, {0x80U, 0x00U, 0x4BU}, {0x54U, 0x05U, 0x71U},
    {0xB1U, 0x05U, 0x25U}, {0xCAU, 0xE0U, 0xC0U}, {0x89U, 0x98U, 0x56U},
    {0x5CU, 0x94U, 0x1AU}, {0x00U, 0x31U, 0x14U}, {0x5DU, 0xAEU, 0x94U},
    {0x4CU, 0xBFU, 0x8FU}, {0x00U, 0x77U, 0x72U}, {0x59U, 0x5BU, 0x61U},
    {0xFFU, 0xFFU, 0xF2U}, {0xB1U, 0x58U, 0x18U}, {0xCBU, 0x8AU, 0x07U},
    {0x98U, 0x6CU, 0x80U}, {0x98U, 0x69U, 0x2DU}, {0x4DU, 0x34U, 0x19U},
    {0x4CU, 0x33U, 0x0BU}, {0x33U, 0x20U, 0x0AU}, {0x52U, 0x3AU, 0x97U},
    {0x0DU, 0x21U, 0x7EU}, {0x1EU, 0x77U, 0xACU}, {0xB2U, 0xDDU, 0x53U},
    {0xF3U, 0x36U, 0x89U}, {0xDEU, 0x64U, 0x9EU}, {0x98U, 0x41U, 0x61U},
    {0x4CU, 0x56U, 0x12U}, {0x4CU, 0x88U, 0x1FU}, {0xE4U, 0xDEU, 0x79U},
    {0xCBU, 0x8AU, 0x1AU}, {0xCBU, 0xA2U, 0x1CU}, {0xFFU, 0x98U, 0x05U},
    {0xFCU, 0xB2U, 0x57U}, {0xFFU, 0xE5U, 0x05U}, {0xF0U, 0x33U, 0x1FU},
    {0x1AU, 0x84U, 0x2DU}, {0x38U, 0x6CU, 0xAEU}, {0xE3U, 0xC4U, 0xB4U},
    {0xE3U, 0xACU, 0x81U}
};

/* Standard Brother PEC/PES thread palette for indices 0..64. */
static const RgbColor brother_palette[BROTHER_PALETTE_COUNT] = {
    {0x00U, 0x00U, 0x00U}, {0x0EU, 0x1FU, 0x7CU}, {0x0AU, 0x55U, 0xA3U},
    {0x00U, 0x87U, 0x77U}, {0x4BU, 0x6BU, 0xAFU}, {0xEDU, 0x17U, 0x1FU},
    {0xD1U, 0x5CU, 0x00U}, {0x91U, 0x36U, 0x97U}, {0xE4U, 0x9AU, 0xCBU},
    {0x91U, 0x5FU, 0xACU}, {0x9EU, 0xD6U, 0x7DU}, {0xE8U, 0xA9U, 0x00U},
    {0xFEU, 0xBAU, 0x35U}, {0xFFU, 0xFFU, 0x00U}, {0x70U, 0xBCU, 0x1FU},
    {0xBAU, 0x98U, 0x00U}, {0xA8U, 0xA8U, 0xA8U}, {0x7DU, 0x6FU, 0x00U},
    {0xFFU, 0xFFU, 0xB3U}, {0x4FU, 0x55U, 0x56U}, {0x00U, 0x00U, 0x00U},
    {0x0BU, 0x3DU, 0x91U}, {0x77U, 0x01U, 0x76U}, {0x29U, 0x31U, 0x33U},
    {0x2AU, 0x13U, 0x01U}, {0xF6U, 0x4AU, 0x8AU}, {0xB2U, 0x76U, 0x24U},
    {0xFCU, 0xBBU, 0xC5U}, {0xFEU, 0x37U, 0x0FU}, {0xF0U, 0xF0U, 0xF0U},
    {0x6AU, 0x1CU, 0x8AU}, {0xA8U, 0xDDU, 0xC4U}, {0x25U, 0x84U, 0xBBU},
    {0xFEU, 0xB3U, 0x43U}, {0xFFU, 0xF3U, 0x6BU}, {0xD0U, 0xA6U, 0x60U},
    {0xD1U, 0x54U, 0x00U}, {0x66U, 0xBAU, 0x49U}, {0x13U, 0x4AU, 0x46U},
    {0x87U, 0x87U, 0x87U}, {0xD8U, 0xCCU, 0xC6U}, {0x43U, 0x56U, 0x07U},
    {0xFDU, 0xD9U, 0xDEU}, {0xF9U, 0x93U, 0xBCU}, {0x00U, 0x38U, 0x22U},
    {0xB2U, 0xAFU, 0xD4U}, {0x68U, 0x6AU, 0xB0U}, {0xEFU, 0xE3U, 0xB9U},
    {0xF7U, 0x38U, 0x66U}, {0xB5U, 0x4BU, 0x64U}, {0x13U, 0x2BU, 0x1AU},
    {0xC7U, 0x01U, 0x56U}, {0xFEU, 0x9EU, 0x32U}, {0xA8U, 0xDEU, 0xEBU},
    {0x00U, 0x67U, 0x3EU}, {0x4EU, 0x29U, 0x90U}, {0x2FU, 0x7EU, 0x20U},
    {0xFFU, 0xCCU, 0xCCU}, {0xFFU, 0xD9U, 0x11U}, {0x09U, 0x5BU, 0xA6U},
    {0xF0U, 0xF9U, 0x70U}, {0xE3U, 0xF3U, 0x5BU}, {0xFFU, 0x99U, 0x00U},
    {0xFFU, 0xF0U, 0x8DU}, {0xFFU, 0xC8U, 0xC8U}
};

/* High-contrast default thread palette for Tajima DST files without color tables. */
static const RgbColor dst_default_palette[DST_DEFAULT_PALETTE_COUNT] = {
    {0x1EU, 0x1EU, 0x1EU}, {0xC8U, 0x28U, 0x28U}, {0x1EU, 0x5AU, 0xAAU},
    {0xF0U, 0xB4U, 0x1EU}, {0x28U, 0x8CU, 0x46U}, {0x82U, 0x3CU, 0x96U},
    {0xE6U, 0x78U, 0x1EU}, {0x5AU, 0x5AU, 0x5AU}
};

static uint32_t read_uint32_little_endian(const uint8_t *bytes, size_t offset) {
    const uint32_t byte_zero = (uint32_t)bytes[offset];
    const uint32_t byte_one = (uint32_t)bytes[offset + 1U] << 8;
    const uint32_t byte_two = (uint32_t)bytes[offset + 2U] << 16;
    const uint32_t byte_three = (uint32_t)bytes[offset + 3U] << 24;
    return byte_zero | byte_one | byte_two | byte_three;
}

static void write_uint32_little_endian(uint8_t *destination, size_t offset, uint32_t value) {
    destination[offset] = (uint8_t)(value & 0xFFU);
    destination[offset + 1U] = (uint8_t)((value >> 8) & 0xFFU);
    destination[offset + 2U] = (uint8_t)((value >> 16) & 0xFFU);
    destination[offset + 3U] = (uint8_t)((value >> 24) & 0xFFU);
}

static void write_int32_little_endian(uint8_t *destination, size_t offset, int32_t value) {
    write_uint32_little_endian(destination, offset, (uint32_t)value);
}

static unsigned char ascii_lowercase(unsigned char character) {
    if (character >= 'A' && character <= 'Z') {
        return (unsigned char)(character - 'A' + 'a');
    }
    return character;
}

static bool strings_equal_ignore_case(const char *left_text, const char *right_text) {
    size_t character_index = 0U;
    while (left_text[character_index] != '\0' && right_text[character_index] != '\0') {
        const unsigned char left_character =
            ascii_lowercase((unsigned char)left_text[character_index]);
        const unsigned char right_character =
            ascii_lowercase((unsigned char)right_text[character_index]);
        if (left_character != right_character) {
            return false;
        }
        character_index += 1U;
    }
    return left_text[character_index] == '\0' && right_text[character_index] == '\0';
}

static int32_t floor_divide_int32(int32_t dividend, int32_t divisor) {
    int32_t quotient = dividend / divisor;
    const int32_t remainder = dividend % divisor;
    if (remainder != 0 && ((dividend < 0) != (divisor < 0))) {
        quotient -= 1;
    }
    return quotient;
}

static int32_t positive_modulo_int32(int32_t value, int32_t modulus) {
    int32_t remainder = value % modulus;
    if (remainder < 0) {
        remainder += modulus;
    }
    return remainder;
}

static uint8_t find_nearest_janome_color(RgbColor target_color) {
    uint8_t best_index = 1U;
    int64_t best_distance = INT64_MAX;
    for (size_t palette_index = 1U; palette_index < JANOME_PALETTE_COUNT; palette_index += 1U) {
        const RgbColor candidate = janome_palette[palette_index];
        const int64_t delta_red = (int64_t)target_color.red - (int64_t)candidate.red;
        const int64_t delta_green = (int64_t)target_color.green - (int64_t)candidate.green;
        const int64_t delta_blue = (int64_t)target_color.blue - (int64_t)candidate.blue;
        const int64_t weighted_distance =
            2LL * delta_red * delta_red +
            4LL * delta_green * delta_green +
            3LL * delta_blue * delta_blue;
        if (weighted_distance < best_distance) {
            best_distance = weighted_distance;
            best_index = (uint8_t)palette_index;
        }
    }
    return best_index;
}

static void init_stitch_design(StitchDesign *design) {
    memset(design, 0, sizeof(*design));
}

static void free_stitch_design(StitchDesign *design) {
    free(design->blocks);
    free(design->chains);
    free(design->points);
    memset(design, 0, sizeof(*design));
}

static bool design_reserve_points(StitchDesign *design, size_t required_count) {
    if (required_count <= design->point_capacity) {
        return true;
    }
    if (required_count > MAX_DESIGN_POINTS) {
        fprintf(stderr, "Point count exceeds safety limit\n");
        return false;
    }

    size_t next_capacity = design->point_capacity;
    if (next_capacity == 0U) {
        next_capacity = 4096U;
    }
    while (next_capacity < required_count) {
        if (next_capacity >= MAX_DESIGN_POINTS / 2U) {
            next_capacity = MAX_DESIGN_POINTS;
            break;
        }
        next_capacity *= 2U;
    }

    size_t allocation_bytes = 0U;
    if (ckd_mul(&allocation_bytes, next_capacity, sizeof(StitchPoint))) {
        return false;
    }

    StitchPoint *grown_points = (StitchPoint *)realloc(design->points, allocation_bytes);
    if (grown_points == nullptr) {
        fprintf(stderr, "Failed to allocate stitch point buffer\n");
        return false;
    }
    design->points = grown_points;
    design->point_capacity = next_capacity;
    return true;
}

static bool design_reserve_chains(StitchDesign *design, size_t required_count) {
    if (required_count <= design->chain_capacity) {
        return true;
    }
    if (required_count > MAX_DESIGN_CHAINS) {
        fprintf(stderr, "Chain count exceeds safety limit\n");
        return false;
    }

    size_t next_capacity = design->chain_capacity;
    if (next_capacity == 0U) {
        next_capacity = 256U;
    }
    while (next_capacity < required_count) {
        if (next_capacity >= MAX_DESIGN_CHAINS / 2U) {
            next_capacity = MAX_DESIGN_CHAINS;
            break;
        }
        next_capacity *= 2U;
    }

    size_t allocation_bytes = 0U;
    if (ckd_mul(&allocation_bytes, next_capacity, sizeof(StitchChain))) {
        return false;
    }

    StitchChain *grown_chains = (StitchChain *)realloc(design->chains, allocation_bytes);
    if (grown_chains == nullptr) {
        fprintf(stderr, "Failed to allocate stitch chain buffer\n");
        return false;
    }
    design->chains = grown_chains;
    design->chain_capacity = next_capacity;
    return true;
}

static bool design_reserve_blocks(StitchDesign *design, size_t required_count) {
    if (required_count <= design->block_capacity) {
        return true;
    }
    if (required_count > MAX_COLOR_BLOCKS) {
        fprintf(stderr, "Color block count exceeds safety limit\n");
        return false;
    }

    size_t next_capacity = design->block_capacity;
    if (next_capacity == 0U) {
        next_capacity = 16U;
    }
    while (next_capacity < required_count) {
        if (next_capacity >= MAX_COLOR_BLOCKS / 2U) {
            next_capacity = MAX_COLOR_BLOCKS;
            break;
        }
        next_capacity *= 2U;
    }

    size_t allocation_bytes = 0U;
    if (ckd_mul(&allocation_bytes, next_capacity, sizeof(ColorBlock))) {
        return false;
    }

    ColorBlock *grown_blocks = (ColorBlock *)realloc(design->blocks, allocation_bytes);
    if (grown_blocks == nullptr) {
        fprintf(stderr, "Failed to allocate color block buffer\n");
        return false;
    }
    design->blocks = grown_blocks;
    design->block_capacity = next_capacity;
    return true;
}

static bool design_append_point(StitchDesign *design, int32_t position_x, int32_t position_y) {
    size_t required_count = 0U;
    if (ckd_add(&required_count, design->point_count, (size_t)1U) ||
        !design_reserve_points(design, required_count)) {
        return false;
    }
    design->points[design->point_count].position_x = position_x;
    design->points[design->point_count].position_y = position_y;
    design->point_count += 1U;
    return true;
}

static bool design_append_chain(
    StitchDesign *design,
    size_t point_offset,
    size_t point_count,
    size_t color_block_index,
    bool start_was_cut,
    bool end_was_cut
) {
    size_t required_count = 0U;
    if (ckd_add(&required_count, design->chain_count, (size_t)1U) ||
        !design_reserve_chains(design, required_count)) {
        return false;
    }
    StitchChain *target_chain = &design->chains[design->chain_count];
    target_chain->point_offset = point_offset;
    target_chain->point_count = point_count;
    target_chain->color_block_index = color_block_index;
    target_chain->start_was_cut = start_was_cut;
    target_chain->end_was_cut = end_was_cut;
    design->chain_count += 1U;
    return true;
}

static bool design_append_block(
    StitchDesign *design,
    RgbColor display_color,
    uint8_t janome_color_index,
    bool enabled,
    size_t *output_block_index
) {
    size_t required_count = 0U;
    if (ckd_add(&required_count, design->block_count, (size_t)1U) ||
        !design_reserve_blocks(design, required_count)) {
        return false;
    }
    const size_t new_index = design->block_count;
    design->blocks[new_index].display_color = display_color;
    design->blocks[new_index].janome_color_index = janome_color_index;
    design->blocks[new_index].enabled = enabled;
    design->block_count += 1U;
    if (output_block_index != nullptr) {
        *output_block_index = new_index;
    }
    return true;
}

static void expand_bounds_from_chain(
    const StitchDesign *design,
    StitchChain chain,
    bool *has_bounds,
    int64_t *minimum_x,
    int64_t *maximum_x,
    int64_t *minimum_y,
    int64_t *maximum_y
) {
    for (size_t local_index = 0U; local_index < chain.point_count; local_index += 1U) {
        const StitchPoint point = design->points[chain.point_offset + local_index];
        const int64_t point_x = (int64_t)point.position_x;
        const int64_t point_y = (int64_t)point.position_y;
        if (!(*has_bounds)) {
            *minimum_x = point_x;
            *maximum_x = point_x;
            *minimum_y = point_y;
            *maximum_y = point_y;
            *has_bounds = true;
            continue;
        }
        if (point_x < *minimum_x) {
            *minimum_x = point_x;
        }
        if (point_x > *maximum_x) {
            *maximum_x = point_x;
        }
        if (point_y < *minimum_y) {
            *minimum_y = point_y;
        }
        if (point_y > *maximum_y) {
            *maximum_y = point_y;
        }
    }
}

static void recompute_design_bounds(StitchDesign *design) {
    bool has_bounds = false;
    int64_t minimum_x = 0LL;
    int64_t maximum_x = 0LL;
    int64_t minimum_y = 0LL;
    int64_t maximum_y = 0LL;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count) {
            continue;
        }
        if (!design->blocks[chain.color_block_index].enabled || chain.point_count < 2U) {
            continue;
        }
        expand_bounds_from_chain(
            design,
            chain,
            &has_bounds,
            &minimum_x,
            &maximum_x,
            &minimum_y,
            &maximum_y
        );
    }

    design->has_bounds = has_bounds;
    design->minimum_x = (int32_t)minimum_x;
    design->maximum_x = (int32_t)maximum_x;
    design->minimum_y = (int32_t)minimum_y;
    design->maximum_y = (int32_t)maximum_y;
}

static void center_stitch_design(StitchDesign *design) {
    recompute_design_bounds(design);
    if (!design->has_bounds) {
        return;
    }

    const int64_t center_x = ((int64_t)design->minimum_x + (int64_t)design->maximum_x) / 2LL;
    const int64_t center_y = ((int64_t)design->minimum_y + (int64_t)design->maximum_y) / 2LL;
    for (size_t point_index = 0U; point_index < design->point_count; point_index += 1U) {
        design->points[point_index].position_x =
            (int32_t)((int64_t)design->points[point_index].position_x - center_x);
        design->points[point_index].position_y =
            (int32_t)((int64_t)design->points[point_index].position_y - center_y);
    }
    design->has_manual_center_shift = false;
    recompute_design_bounds(design);
}

static bool clone_stitch_design(const StitchDesign *source, StitchDesign *destination) {
    init_stitch_design(destination);

    if (source->block_count > 0U) {
        if (!design_reserve_blocks(destination, source->block_count)) {
            goto cleanup;
        }
        memcpy(destination->blocks, source->blocks, source->block_count * sizeof(ColorBlock));
        destination->block_count = source->block_count;
    }
    if (source->chain_count > 0U) {
        if (!design_reserve_chains(destination, source->chain_count)) {
            goto cleanup;
        }
        memcpy(destination->chains, source->chains, source->chain_count * sizeof(StitchChain));
        destination->chain_count = source->chain_count;
    }
    if (source->point_count > 0U) {
        if (!design_reserve_points(destination, source->point_count)) {
            goto cleanup;
        }
        memcpy(destination->points, source->points, source->point_count * sizeof(StitchPoint));
        destination->point_count = source->point_count;
    }

    destination->minimum_x = source->minimum_x;
    destination->maximum_x = source->maximum_x;
    destination->minimum_y = source->minimum_y;
    destination->maximum_y = source->maximum_y;
    destination->has_bounds = source->has_bounds;
    destination->has_manual_center_shift = source->has_manual_center_shift;
    return true;

cleanup:
    free_stitch_design(destination);
    return false;
}

/* Reorder chains contiguously by color_block_index in O(blocks + chains) time. */
static bool sort_chains_by_color_block(StitchDesign *design) {
    bool succeeded = false;
    size_t *block_counts = nullptr;
    size_t *block_write_offsets = nullptr;
    StitchChain *sorted_chains = nullptr;

    if (design->block_count == 0U || design->chain_count < 2U) {
        return true;
    }

    block_counts = (size_t *)calloc(design->block_count, sizeof(size_t));
    block_write_offsets = (size_t *)calloc(design->block_count, sizeof(size_t));
    sorted_chains = (StitchChain *)malloc(design->chain_count * sizeof(StitchChain));
    if (block_counts == nullptr || block_write_offsets == nullptr || sorted_chains == nullptr) {
        fprintf(stderr, "Failed to allocate chain sort buffers\n");
        goto cleanup;
    }

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const size_t block_index = design->chains[chain_index].color_block_index;
        if (block_index < design->block_count) {
            block_counts[block_index] += 1U;
        }
    }

    size_t running_offset = 0U;
    for (size_t block_index = 0U; block_index < design->block_count; block_index += 1U) {
        block_write_offsets[block_index] = running_offset;
        running_offset += block_counts[block_index];
    }

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index < design->block_count) {
            const size_t destination_index = block_write_offsets[chain.color_block_index];
            sorted_chains[destination_index] = chain;
            block_write_offsets[chain.color_block_index] = destination_index + 1U;
        }
    }

    memcpy(design->chains, sorted_chains, running_offset * sizeof(StitchChain));
    design->chain_count = running_offset;
    succeeded = true;

cleanup:
    free(sorted_chains);
    free(block_write_offsets);
    free(block_counts);
    return succeeded;
}

static bool compact_non_empty_blocks(StitchDesign *design) {
    if (design->block_count == 0U) {
        return true;
    }

    size_t *index_remap = (size_t *)malloc(design->block_count * sizeof(size_t));
    if (index_remap == nullptr) {
        fprintf(stderr, "Failed to allocate color remap table\n");
        return false;
    }

    for (size_t block_index = 0U; block_index < design->block_count; block_index += 1U) {
        index_remap[block_index] = SIZE_MAX;
    }

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index < design->block_count && chain.point_count >= 2U) {
            index_remap[chain.color_block_index] = 1U;
        }
    }

    size_t compacted_count = 0U;
    for (size_t block_index = 0U; block_index < design->block_count; block_index += 1U) {
        if (index_remap[block_index] != SIZE_MAX) {
            index_remap[block_index] = compacted_count;
            design->blocks[compacted_count] = design->blocks[block_index];
            compacted_count += 1U;
        }
    }
    design->block_count = compacted_count;

    size_t kept_chains = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count >= 2U && index_remap[chain.color_block_index] != SIZE_MAX) {
            design->chains[kept_chains] = chain;
            design->chains[kept_chains].color_block_index = index_remap[chain.color_block_index];
            kept_chains += 1U;
        }
    }
    design->chain_count = kept_chains;

    free(index_remap);
    return sort_chains_by_color_block(design);
}

static DesignStats compute_design_stats(const StitchDesign *design) {
    DesignStats stats = {};

    size_t previous_block_index = SIZE_MAX;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count) {
            continue;
        }
        if (!design->blocks[chain.color_block_index].enabled || chain.point_count < 2U) {
            continue;
        }

        if (chain.color_block_index != previous_block_index) {
            stats.color_count += 1U;
            previous_block_index = chain.color_block_index;
        } else {
            stats.jump_count += 1U;
        }

        stats.chain_count += 1U;
        stats.stitch_count += chain.point_count - 1U;
        if (chain.start_was_cut) {
            stats.lock_count += 1U;
        }
        if (chain.end_was_cut) {
            stats.lock_count += 1U;
        }
    }

    return stats;
}

static bool builder_finish_chain(DesignBuilder *builder) {
    if (!builder->chain_is_open) {
        return true;
    }
    builder->chain_is_open = false;

    if (builder->active_chain_point_count < 2U) {
        builder->design->point_count = builder->active_chain_point_offset;
        builder->active_chain_point_count = 0U;
        return true;
    }

    const bool appended = design_append_chain(
        builder->design,
        builder->active_chain_point_offset,
        builder->active_chain_point_count,
        builder->active_block_index,
        false,
        false
    );
    builder->active_chain_point_count = 0U;
    return appended;
}

static bool builder_record_move(DesignBuilder *builder, int64_t delta_x, int64_t delta_y) {
    if (!builder_finish_chain(builder)) {
        return false;
    }
    const int64_t next_x = builder->current_x + delta_x;
    const int64_t next_y = builder->current_y + delta_y;
    if (next_x < -MAX_COORDINATE_MAGNITUDE || next_x > MAX_COORDINATE_MAGNITUDE ||
        next_y < -MAX_COORDINATE_MAGNITUDE || next_y > MAX_COORDINATE_MAGNITUDE) {
        fprintf(stderr, "Stitch coordinates out of valid range\n");
        return false;
    }
    builder->current_x = next_x;
    builder->current_y = next_y;
    return true;
}

static bool builder_start_color_block(
    DesignBuilder *builder,
    RgbColor display_color,
    uint8_t janome_color_index
) {
    if (!builder_finish_chain(builder)) {
        return false;
    }
    return design_append_block(
        builder->design,
        display_color,
        janome_color_index,
        true,
        &builder->active_block_index
    );
}

static bool builder_record_stitch(DesignBuilder *builder, int64_t delta_x, int64_t delta_y) {
    const int64_t next_x = builder->current_x + delta_x;
    const int64_t next_y = builder->current_y + delta_y;
    if (next_x < -MAX_COORDINATE_MAGNITUDE || next_x > MAX_COORDINATE_MAGNITUDE ||
        next_y < -MAX_COORDINATE_MAGNITUDE || next_y > MAX_COORDINATE_MAGNITUDE) {
        fprintf(stderr, "Stitch coordinates out of valid range\n");
        return false;
    }

    builder->current_x = next_x;
    builder->current_y = next_y;

    if (!builder->chain_is_open) {
        builder->active_chain_point_offset = builder->design->point_count;
        builder->active_chain_point_count = 0U;
        builder->chain_is_open = true;
        if (!design_append_point(builder->design, (int32_t)next_x, (int32_t)next_y)) {
            return false;
        }
        builder->active_chain_point_count = 1U;
        return true;
    }

    const StitchPoint last_point = builder->design->points[builder->design->point_count - 1U];
    if (last_point.position_x == (int32_t)next_x && last_point.position_y == (int32_t)next_y) {
        return true;
    }

    if (!design_append_point(builder->design, (int32_t)next_x, (int32_t)next_y)) {
        return false;
    }
    builder->active_chain_point_count += 1U;
    return true;
}

static EmbroideryFormat detect_embroidery_format(
    const char *input_path,
    const uint8_t *file_buffer,
    size_t file_size
) {
    const char *last_slash = strrchr(input_path, '/');
    const char *file_name = input_path;
    if (last_slash != nullptr) {
        file_name = last_slash + 1;
    }

    const char *last_dot = strrchr(file_name, '.');
    if (last_dot != nullptr && last_dot[1] != '\0') {
        const char *extension = last_dot + 1;
        if (strings_equal_ignore_case(extension, "jef") ||
            strings_equal_ignore_case(extension, "jef+")) {
            return FORMAT_JEF;
        }
        if (strings_equal_ignore_case(extension, "pes")) {
            return FORMAT_PES;
        }
        if (strings_equal_ignore_case(extension, "pec")) {
            return FORMAT_PEC;
        }
        if (strings_equal_ignore_case(extension, "dst")) {
            return FORMAT_DST;
        }
    }

    if (file_size >= 4U && memcmp(file_buffer, "#PES", 4U) == 0) {
        return FORMAT_PES;
    }
    if (file_size >= 4U && memcmp(file_buffer, "#PEC", 4U) == 0) {
        return FORMAT_PEC;
    }
    return FORMAT_UNKNOWN;
}

static uint8_t resolve_janome_slot_index(
    const uint8_t *file_buffer,
    uint32_t color_count,
    uint32_t color_slot
) {
    if (color_count == 0U) {
        return 1U;
    }
    const uint32_t wrapped_slot = color_slot % color_count;
    const uint32_t raw_palette_word =
        read_uint32_little_endian(file_buffer, JEF_HEADER_SIZE + (size_t)wrapped_slot * 4U);
    int64_t signed_index = (int64_t)(int32_t)raw_palette_word;
    if (signed_index < 0) {
        signed_index = -signed_index;
    }
    const uint8_t palette_index = (uint8_t)(signed_index % (int64_t)JANOME_PALETTE_COUNT);
    if (palette_index == 0U) {
        return 1U;
    }
    return palette_index;
}

static RgbColor resolve_brother_slot_color(
    const uint8_t *file_buffer,
    size_t pec_offset,
    uint32_t color_count,
    uint32_t color_slot
) {
    if (color_count == 0U) {
        return brother_palette[20];
    }
    const uint32_t wrapped_slot = color_slot % color_count;
    const uint8_t raw_palette_index = file_buffer[pec_offset + 49U + (size_t)wrapped_slot];
    return brother_palette[(size_t)raw_palette_index % BROTHER_PALETTE_COUNT];
}

static bool parse_jef_design(
    const uint8_t *file_buffer,
    size_t file_size,
    StitchDesign *design
) {
    if (file_size < JEF_HEADER_SIZE) {
        fprintf(stderr, "Invalid JEF file size\n");
        return false;
    }

    const uint32_t stitch_offset = read_uint32_little_endian(file_buffer, 0U);
    uint32_t color_count = read_uint32_little_endian(file_buffer, 24U);
    if (stitch_offset < JEF_HEADER_SIZE || (size_t)stitch_offset >= file_size) {
        fprintf(stderr, "Invalid JEF stitch offset\n");
        return false;
    }

    const size_t available_color_entries = ((size_t)stitch_offset - JEF_HEADER_SIZE) / 4U;
    if ((size_t)color_count > available_color_entries) {
        color_count = (uint32_t)available_color_entries;
    }

    DesignBuilder builder = {};
    builder.design = design;

    uint32_t current_color_slot = 0U;
    uint8_t janome_index = resolve_janome_slot_index(file_buffer, color_count, current_color_slot);
    if (!builder_start_color_block(&builder, janome_palette[janome_index], janome_index)) {
        return false;
    }

    size_t byte_offset = (size_t)stitch_offset;
    while (byte_offset + 1U < file_size) {
        const uint8_t first_byte = file_buffer[byte_offset];
        const uint8_t second_byte = file_buffer[byte_offset + 1U];
        byte_offset += 2U;

        if (first_byte != 0x80U) {
            const int64_t delta_x = (int64_t)(int8_t)first_byte;
            const int64_t delta_y = -(int64_t)(int8_t)second_byte;
            if (!builder_record_stitch(&builder, delta_x, delta_y)) {
                return false;
            }
            continue;
        }

        if (second_byte == 0x10U || byte_offset + 1U >= file_size) {
            break;
        }

        const int64_t move_x = (int64_t)(int8_t)file_buffer[byte_offset];
        const int64_t move_y = -(int64_t)(int8_t)file_buffer[byte_offset + 1U];
        byte_offset += 2U;

        if (second_byte == 0x01U) {
            current_color_slot += 1U;
            janome_index = resolve_janome_slot_index(file_buffer, color_count, current_color_slot);
            if (!builder_record_move(&builder, move_x, move_y)) {
                return false;
            }
            if (!builder_start_color_block(&builder, janome_palette[janome_index], janome_index)) {
                return false;
            }
        } else if (second_byte == 0x02U || second_byte == 0x04U) {
            if (!builder_record_move(&builder, move_x, move_y)) {
                return false;
            }
        } else if (second_byte == 0x80U) {
            if (!builder_finish_chain(&builder)) {
                return false;
            }
        } else {
            break;
        }
    }

    if (!builder_finish_chain(&builder)) {
        return false;
    }
    return compact_non_empty_blocks(design);
}

static bool decode_pec_axis(
    const uint8_t *file_buffer,
    size_t file_size,
    size_t *byte_offset,
    int64_t *decoded_delta,
    bool *is_jump_or_trim
) {
    if (*byte_offset >= file_size) {
        return false;
    }

    const uint8_t lead_byte = file_buffer[*byte_offset];
    if ((lead_byte & 0x80U) != 0U) {
        if (*byte_offset + 1U >= file_size) {
            return false;
        }
        if ((lead_byte & 0x30U) != 0U) {
            *is_jump_or_trim = true;
        }
        const uint32_t high_nibble = (uint32_t)(lead_byte & 0x0FU) << 8;
        const uint32_t low_byte = (uint32_t)file_buffer[*byte_offset + 1U];
        const uint32_t combined_value = high_nibble | low_byte;
        if (combined_value >= 2048U) {
            *decoded_delta = (int64_t)combined_value - 4096LL;
        } else {
            *decoded_delta = (int64_t)combined_value;
        }
        *byte_offset += 2U;
        return true;
    }

    if (lead_byte >= 64U) {
        *decoded_delta = (int64_t)lead_byte - 128LL;
    } else {
        *decoded_delta = (int64_t)lead_byte;
    }
    *byte_offset += 1U;
    return true;
}

static bool parse_pec_section_design(
    const uint8_t *file_buffer,
    size_t file_size,
    size_t pec_offset,
    StitchDesign *design
) {
    if (pec_offset >= file_size || file_size - pec_offset <= PEC_HEADER_SIZE) {
        fprintf(stderr, "PEC section is truncated or missing stitch data\n");
        return false;
    }

    const uint32_t color_count = (uint32_t)file_buffer[pec_offset + 48U] + 1U;
    DesignBuilder builder = {};
    builder.design = design;

    uint32_t current_color_slot = 0U;
    RgbColor active_color =
        resolve_brother_slot_color(file_buffer, pec_offset, color_count, current_color_slot);
    if (!builder_start_color_block(
            &builder,
            active_color,
            find_nearest_janome_color(active_color)
        )) {
        return false;
    }

    size_t byte_offset = pec_offset + PEC_HEADER_SIZE;
    while (byte_offset < file_size) {
        const uint8_t lead_byte = file_buffer[byte_offset];
        if (lead_byte == 0xFFU) {
            break;
        }
        if (lead_byte == 0xFEU) {
            if (byte_offset + 2U >= file_size || file_buffer[byte_offset + 1U] != 0xB0U) {
                break;
            }
            byte_offset += 3U;
            current_color_slot += 1U;
            active_color = resolve_brother_slot_color(
                file_buffer,
                pec_offset,
                color_count,
                current_color_slot
            );
            if (!builder_start_color_block(
                    &builder,
                    active_color,
                    find_nearest_janome_color(active_color)
                )) {
                return false;
            }
            continue;
        }

        int64_t delta_x = 0;
        int64_t delta_y = 0;
        bool is_jump_or_trim = false;
        if (!decode_pec_axis(file_buffer, file_size, &byte_offset, &delta_x, &is_jump_or_trim)) {
            break;
        }
        if (!decode_pec_axis(file_buffer, file_size, &byte_offset, &delta_y, &is_jump_or_trim)) {
            break;
        }

        if (is_jump_or_trim) {
            if (!builder_record_move(&builder, delta_x, delta_y)) {
                return false;
            }
        } else {
            if (!builder_record_stitch(&builder, delta_x, delta_y)) {
                return false;
            }
        }
    }

    if (!builder_finish_chain(&builder)) {
        return false;
    }
    return compact_non_empty_blocks(design);
}

static bool parse_pes_design(
    const uint8_t *file_buffer,
    size_t file_size,
    StitchDesign *design
) {
    if (file_size < 12U || memcmp(file_buffer, "#PES", 4U) != 0) {
        fprintf(stderr, "Invalid PES header signature\n");
        return false;
    }

    const uint32_t pec_offset = read_uint32_little_endian(file_buffer, 8U);
    if ((size_t)pec_offset >= file_size || file_size - (size_t)pec_offset <= PEC_HEADER_SIZE) {
        fprintf(stderr, "Invalid PEC offset in PES file\n");
        return false;
    }

    return parse_pec_section_design(file_buffer, file_size, (size_t)pec_offset, design);
}

static bool parse_pec_design(
    const uint8_t *file_buffer,
    size_t file_size,
    StitchDesign *design
) {
    size_t pec_offset = 0U;
    if (file_size >= 8U && memcmp(file_buffer, "#PEC", 4U) == 0) {
        pec_offset = 8U;
    } else if (file_size >= 3U && memcmp(file_buffer, "LA:", 3U) == 0) {
        pec_offset = 0U;
    } else {
        fprintf(stderr, "Invalid PEC header signature\n");
        return false;
    }

    return parse_pec_section_design(file_buffer, file_size, pec_offset, design);
}

static bool parse_dst_design(
    const uint8_t *file_buffer,
    size_t file_size,
    StitchDesign *design
) {
    if (file_size < DST_HEADER_SIZE + 3U) {
        fprintf(stderr, "Invalid DST file size\n");
        return false;
    }

    DesignBuilder builder = {};
    builder.design = design;

    uint32_t current_color_slot = 0U;
    RgbColor active_color = dst_default_palette[0];
    if (!builder_start_color_block(
            &builder,
            active_color,
            find_nearest_janome_color(active_color)
        )) {
        return false;
    }

    size_t byte_offset = DST_HEADER_SIZE;
    while (byte_offset + 2U < file_size) {
        const uint8_t first_byte = file_buffer[byte_offset];
        const uint8_t second_byte = file_buffer[byte_offset + 1U];
        const uint8_t third_byte = file_buffer[byte_offset + 2U];
        byte_offset += 3U;

        if ((third_byte & 0xF3U) == 0xF3U) {
            break;
        }

        int64_t delta_x = 0;
        int64_t delta_y = 0;

        if ((first_byte & 0x01U) != 0U) {
            delta_x += 1;
        }
        if ((first_byte & 0x02U) != 0U) {
            delta_x -= 1;
        }
        if ((first_byte & 0x04U) != 0U) {
            delta_x += 9;
        }
        if ((first_byte & 0x08U) != 0U) {
            delta_x -= 9;
        }
        if ((first_byte & 0x80U) != 0U) {
            delta_y -= 1;
        }
        if ((first_byte & 0x40U) != 0U) {
            delta_y += 1;
        }
        if ((first_byte & 0x20U) != 0U) {
            delta_y -= 9;
        }
        if ((first_byte & 0x10U) != 0U) {
            delta_y += 9;
        }

        if ((second_byte & 0x01U) != 0U) {
            delta_x += 3;
        }
        if ((second_byte & 0x02U) != 0U) {
            delta_x -= 3;
        }
        if ((second_byte & 0x04U) != 0U) {
            delta_x += 27;
        }
        if ((second_byte & 0x08U) != 0U) {
            delta_x -= 27;
        }
        if ((second_byte & 0x80U) != 0U) {
            delta_y -= 3;
        }
        if ((second_byte & 0x40U) != 0U) {
            delta_y += 3;
        }
        if ((second_byte & 0x20U) != 0U) {
            delta_y -= 27;
        }
        if ((second_byte & 0x10U) != 0U) {
            delta_y += 27;
        }

        if ((third_byte & 0x04U) != 0U) {
            delta_x += 81;
        }
        if ((third_byte & 0x08U) != 0U) {
            delta_x -= 81;
        }
        if ((third_byte & 0x20U) != 0U) {
            delta_y -= 81;
        }
        if ((third_byte & 0x10U) != 0U) {
            delta_y += 81;
        }

        if ((third_byte & 0xC3U) == 0xC3U) {
            current_color_slot += 1U;
            active_color = dst_default_palette[current_color_slot % DST_DEFAULT_PALETTE_COUNT];
            if (!builder_record_move(&builder, delta_x, delta_y)) {
                return false;
            }
            if (!builder_start_color_block(
                    &builder,
                    active_color,
                    find_nearest_janome_color(active_color)
                )) {
                return false;
            }
        } else if ((third_byte & 0x83U) == 0x83U) {
            if (!builder_record_move(&builder, delta_x, delta_y)) {
                return false;
            }
        } else {
            if (!builder_record_stitch(&builder, delta_x, delta_y)) {
                return false;
            }
        }
    }

    if (!builder_finish_chain(&builder)) {
        return false;
    }
    return compact_non_empty_blocks(design);
}

static bool load_embroidery_file(const char *input_path, StitchDesign *design) {
    bool succeeded = false;
    FILE *input_file = nullptr;
    uint8_t *file_buffer = nullptr;

    init_stitch_design(design);

    input_file = fopen(input_path, "rb");
    if (input_file == nullptr) {
        fprintf(stderr, "Failed to open input file: %s\n", strerror(errno));
        goto cleanup;
    }

    if (fseek(input_file, 0L, SEEK_END) != 0) {
        fprintf(stderr, "Failed to seek input file\n");
        goto cleanup;
    }
    const long reported_length = ftell(input_file);
    if (reported_length <= 0L || (uint64_t)reported_length > (uint64_t)MAX_INPUT_FILE_SIZE) {
        fprintf(stderr, "Invalid input file size\n");
        goto cleanup;
    }
    if (fseek(input_file, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "Failed to rewind input file\n");
        goto cleanup;
    }

    const size_t file_size = (size_t)reported_length;
    file_buffer = (uint8_t *)malloc(file_size);
    if (file_buffer == nullptr) {
        fprintf(stderr, "Failed to allocate input buffer\n");
        goto cleanup;
    }

    const size_t bytes_read = fread(file_buffer, 1U, file_size, input_file);
    if (bytes_read != file_size) {
        fprintf(stderr, "Failed to read input file\n");
        goto cleanup;
    }
    fclose(input_file);
    input_file = nullptr;

    const EmbroideryFormat format = detect_embroidery_format(input_path, file_buffer, file_size);
    bool parse_succeeded = false;
    if (format == FORMAT_JEF) {
        parse_succeeded = parse_jef_design(file_buffer, file_size, design);
    } else if (format == FORMAT_PES) {
        parse_succeeded = parse_pes_design(file_buffer, file_size, design);
    } else if (format == FORMAT_PEC) {
        parse_succeeded = parse_pec_design(file_buffer, file_size, design);
    } else if (format == FORMAT_DST) {
        parse_succeeded = parse_dst_design(file_buffer, file_size, design);
    } else {
        fprintf(stderr, "Unsupported embroidery format\n");
        goto cleanup;
    }

    if (!parse_succeeded) {
        goto cleanup;
    }

    center_stitch_design(design);
    if (!design->has_bounds || design->chain_count == 0U) {
        fprintf(stderr, "No drawable stitches found\n");
        goto cleanup;
    }

    succeeded = true;

cleanup:
    if (input_file != nullptr) {
        fclose(input_file);
    }
    free(file_buffer);
    if (!succeeded) {
        free_stitch_design(design);
    }
    return succeeded;
}

static uint64_t hash_four_int32(int32_t value_a, int32_t value_b, int32_t value_c, int32_t value_d) {
    uint64_t state = 14695981039346656037ULL;
    const uint32_t words[4] = {
        (uint32_t)value_a,
        (uint32_t)value_b,
        (uint32_t)value_c,
        (uint32_t)value_d
    };
    for (size_t word_index = 0U; word_index < 4U; word_index += 1U) {
        state ^= (uint64_t)words[word_index];
        state *= 1099511628211ULL;
    }
    return state;
}

static void insert_diagonal_edge_hit(
    DiagonalEdgeSlot *table,
    size_t table_mask,
    size_t *occupied_slots,
    size_t maximum_occupied_slots,
    StitchPoint start_point,
    StitchPoint end_point
) {
    int32_t first_x = start_point.position_x;
    int32_t first_y = start_point.position_y;
    int32_t second_x = end_point.position_x;
    int32_t second_y = end_point.position_y;
    if (second_x < first_x || (second_x == first_x && second_y < first_y)) {
        first_x = end_point.position_x;
        first_y = end_point.position_y;
        second_x = start_point.position_x;
        second_y = start_point.position_y;
    }

    size_t slot_index = (size_t)hash_four_int32(first_x, first_y, second_x, second_y) & table_mask;
    for (size_t probe_step = 0U; probe_step <= table_mask; probe_step += 1U) {
        DiagonalEdgeSlot *slot = &table[slot_index];
        if (!slot->is_occupied) {
            if (*occupied_slots >= maximum_occupied_slots) {
                return;
            }
            slot->first_x = first_x;
            slot->first_y = first_y;
            slot->second_x = second_x;
            slot->second_y = second_y;
            slot->hit_count = 1U;
            slot->is_occupied = true;
            *occupied_slots += 1U;
            return;
        }
        if (slot->first_x == first_x && slot->first_y == first_y &&
            slot->second_x == second_x && slot->second_y == second_y) {
            if (slot->hit_count < UINT32_MAX) {
                slot->hit_count += 1U;
            }
            return;
        }
        slot_index = (slot_index + 1U) & table_mask;
    }
}

static bool measure_diagonal_pitch(
    StitchPoint start_point,
    StitchPoint end_point,
    int32_t *pitch_out
) {
    int64_t delta_x = (int64_t)end_point.position_x - (int64_t)start_point.position_x;
    if (delta_x < 0LL) {
        delta_x = -delta_x;
    }
    int64_t delta_y = (int64_t)end_point.position_y - (int64_t)start_point.position_y;
    if (delta_y < 0LL) {
        delta_y = -delta_y;
    }
    int64_t axis_difference = delta_x - delta_y;
    if (axis_difference < 0LL) {
        axis_difference = -axis_difference;
    }
    if (axis_difference > 2LL ||
        delta_x < MIN_CROSS_PITCH_UNITS || delta_x > MAX_CROSS_PITCH_UNITS ||
        delta_y < MIN_CROSS_PITCH_UNITS || delta_y > MAX_CROSS_PITCH_UNITS) {
        return false;
    }
    *pitch_out = (int32_t)((delta_x + delta_y) / 2LL);
    return true;
}

static CrossGridInfo detect_cross_stitch_grid(const StitchDesign *design) {
    CrossGridInfo grid = {};
    grid.cross_pitch_units = DEFAULT_CROSS_PITCH_UNITS;
    grid.cross_passes = 3;
    grid.raw_detected_passes = 3;

    DiagonalEdgeSlot *edge_table = nullptr;
    size_t pitch_histogram[MAX_CROSS_PITCH_UNITS + 1] = {};

    size_t diagonal_count = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        for (size_t local_index = 0U; local_index + 1U < chain.point_count; local_index += 1U) {
            int32_t pitch_value = 0;
            if (measure_diagonal_pitch(
                    design->points[chain.point_offset + local_index],
                    design->points[chain.point_offset + local_index + 1U],
                    &pitch_value
                )) {
                pitch_histogram[pitch_value] += 1U;
                diagonal_count += 1U;
            }
        }
    }

    grid.diagonal_stitch_count = diagonal_count;
    int32_t best_pitch = DEFAULT_CROSS_PITCH_UNITS;
    size_t best_pitch_hits = 0U;
    for (int32_t candidate = MIN_CROSS_PITCH_UNITS; candidate <= MAX_CROSS_PITCH_UNITS; candidate += 1) {
        if (pitch_histogram[candidate] > best_pitch_hits) {
            best_pitch_hits = pitch_histogram[candidate];
            best_pitch = candidate;
        }
    }

    if (best_pitch_hits < 8U) {
        goto cleanup;
    }

    grid.cross_pitch_units = best_pitch;
    grid.detected_from_design = true;

    size_t origin_x_histogram[MAX_CROSS_PITCH_UNITS + 1] = {};
    size_t origin_y_histogram[MAX_CROSS_PITCH_UNITS + 1] = {};

    size_t table_capacity = 1024U;
    const size_t target_capacity = best_pitch_hits * 2U + 64U;
    while (table_capacity < target_capacity && table_capacity < 1048576U) {
        table_capacity *= 2U;
    }
    edge_table = (DiagonalEdgeSlot *)calloc(table_capacity, sizeof(DiagonalEdgeSlot));
    const size_t table_mask = table_capacity - 1U;
    const size_t maximum_occupied_slots = table_capacity / 2U;
    size_t occupied_slots = 0U;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        for (size_t local_index = 0U; local_index + 1U < chain.point_count; local_index += 1U) {
            const StitchPoint start_point = design->points[chain.point_offset + local_index];
            const StitchPoint end_point = design->points[chain.point_offset + local_index + 1U];
            int32_t pitch_value = 0;
            if (!measure_diagonal_pitch(start_point, end_point, &pitch_value)) {
                continue;
            }
            int32_t pitch_error = pitch_value - best_pitch;
            if (pitch_error < 0) {
                pitch_error = -pitch_error;
            }
            if (pitch_error > 1) {
                continue;
            }
            origin_x_histogram[positive_modulo_int32(start_point.position_x, best_pitch)] += 1U;
            origin_y_histogram[positive_modulo_int32(start_point.position_y, best_pitch)] += 1U;
            origin_x_histogram[positive_modulo_int32(end_point.position_x, best_pitch)] += 1U;
            origin_y_histogram[positive_modulo_int32(end_point.position_y, best_pitch)] += 1U;
            if (edge_table != nullptr) {
                insert_diagonal_edge_hit(
                    edge_table,
                    table_mask,
                    &occupied_slots,
                    maximum_occupied_slots,
                    start_point,
                    end_point
                );
            }
        }
    }

    size_t best_origin_x_hits = 0U;
    size_t best_origin_y_hits = 0U;
    for (int32_t phase = 0; phase < best_pitch; phase += 1) {
        if (origin_x_histogram[phase] > best_origin_x_hits) {
            best_origin_x_hits = origin_x_histogram[phase];
            grid.grid_origin_x = phase;
        }
        if (origin_y_histogram[phase] > best_origin_y_hits) {
            best_origin_y_hits = origin_y_histogram[phase];
            grid.grid_origin_y = phase;
        }
    }

    if (edge_table != nullptr) {
        size_t pass_histogram[16] = {};
        for (size_t slot_index = 0U; slot_index < table_capacity; slot_index += 1U) {
            if (!edge_table[slot_index].is_occupied) {
                continue;
            }
            uint32_t count = edge_table[slot_index].hit_count;
            if (count > 15U) {
                count = 15U;
            }
            pass_histogram[count] += 1U;
        }

        int32_t best_passes = 3;
        size_t best_pass_hits = 0U;
        for (int32_t pass_value = 1; pass_value <= 15; pass_value += 1) {
            if (pass_histogram[pass_value] > best_pass_hits) {
                best_pass_hits = pass_histogram[pass_value];
                best_passes = pass_value;
            }
        }
        grid.raw_detected_passes = best_passes;
        if (best_passes < MIN_CROSS_PASSES) {
            grid.cross_passes = MIN_CROSS_PASSES;
        } else if (best_passes > MAX_CROSS_PASSES) {
            grid.cross_passes = MAX_CROSS_PASSES;
        } else {
            grid.cross_passes = best_passes;
        }
    }

cleanup:
    free(edge_table);
    return grid;
}

static size_t count_design_segments(const StitchDesign *design) {
    size_t total_segments = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        if (design->chains[chain_index].point_count >= 2U) {
            total_segments += design->chains[chain_index].point_count - 1U;
        }
    }
    return total_segments;
}

static bool point_is_inside_rectangle(StitchPoint point, CutRectangle rectangle) {
    return point.position_x >= rectangle.minimum_x &&
           point.position_x <= rectangle.maximum_x &&
           point.position_y >= rectangle.minimum_y &&
           point.position_y <= rectangle.maximum_y;
}

static bool point_is_strictly_inside_rectangle(
    int32_t point_x,
    int32_t point_y,
    CutRectangle rectangle
) {
    return point_x > rectangle.minimum_x &&
           point_x < rectangle.maximum_x &&
           point_y > rectangle.minimum_y &&
           point_y < rectangle.maximum_y;
}

static bool point_kept_by_edit(
    EditOperationKind operation,
    int32_t point_x,
    int32_t point_y,
    CutRectangle rectangle,
    int32_t split_coordinate,
    bool keep_second_half
) {
    if (operation == EDIT_CROP_KEEP) {
        return point_is_strictly_inside_rectangle(point_x, point_y, rectangle);
    }
    if (operation == EDIT_CUT_OUT) {
        return !point_is_strictly_inside_rectangle(point_x, point_y, rectangle);
    }
    if (operation == EDIT_SPLIT_VERTICAL) {
        if (keep_second_half) {
            return point_x >= split_coordinate;
        }
        return point_x < split_coordinate;
    }
    if (keep_second_half) {
        return point_y >= split_coordinate;
    }
    return point_y < split_coordinate;
}

static bool is_cross_diagonal_for_pitch(
    StitchPoint start_point,
    StitchPoint end_point,
    int32_t pitch_units
) {
    if (pitch_units < MIN_CROSS_PITCH_UNITS) {
        pitch_units = DEFAULT_CROSS_PITCH_UNITS;
    }
    int64_t delta_x = (int64_t)end_point.position_x - (int64_t)start_point.position_x;
    if (delta_x < 0LL) {
        delta_x = -delta_x;
    }
    int64_t delta_y = (int64_t)end_point.position_y - (int64_t)start_point.position_y;
    if (delta_y < 0LL) {
        delta_y = -delta_y;
    }
    int64_t axis_difference = delta_x - delta_y;
    if (axis_difference < 0LL) {
        axis_difference = -axis_difference;
    }
    if (axis_difference > 3LL) {
        return false;
    }

    const double segment_length = sqrt((double)(delta_x * delta_x + delta_y * delta_y));
    const double expected_diagonal = (double)pitch_units * 1.4142135623730951;
    return segment_length >= expected_diagonal * 0.80 &&
           segment_length <= expected_diagonal * 1.20;
}

static bool is_cross_center_connector(
    StitchPoint start_point,
    StitchPoint end_point,
    int32_t pitch_units
) {
    if (pitch_units < MIN_CROSS_PITCH_UNITS) {
        pitch_units = DEFAULT_CROSS_PITCH_UNITS;
    }
    int64_t delta_x = (int64_t)end_point.position_x - (int64_t)start_point.position_x;
    if (delta_x < 0LL) {
        delta_x = -delta_x;
    }
    int64_t delta_y = (int64_t)end_point.position_y - (int64_t)start_point.position_y;
    if (delta_y < 0LL) {
        delta_y = -delta_y;
    }
    const double half_pitch = (double)pitch_units * 0.5;
    return fabs((double)delta_x - half_pitch) <= 3.0 &&
           fabs((double)delta_y - half_pitch) <= 3.0;
}

static bool run_is_cut_artifact(
    const StitchPoint *chain_points,
    size_t run_start_segment,
    size_t run_segment_count,
    int32_t pitch_units
) {
    if (run_segment_count == 1U) {
        return !is_cross_diagonal_for_pitch(
            chain_points[run_start_segment],
            chain_points[run_start_segment + 1U],
            pitch_units
        );
    }
    if (run_segment_count <= 2U) {
        for (size_t offset = 0U; offset < run_segment_count; offset += 1U) {
            const StitchPoint first = chain_points[run_start_segment + offset];
            const StitchPoint second = chain_points[run_start_segment + offset + 1U];
            int64_t delta_x = (int64_t)second.position_x - (int64_t)first.position_x;
            if (delta_x < 0LL) {
                delta_x = -delta_x;
            }
            int64_t delta_y = (int64_t)second.position_y - (int64_t)first.position_y;
            if (delta_y < 0LL) {
                delta_y = -delta_y;
            }
            if (delta_x > 5LL || delta_y > 5LL) {
                return false;
            }
        }
        return true;
    }
    return false;
}

static uint64_t hash_color_and_cell(uint8_t color_index, int32_t cell_x, int32_t cell_y) {
    uint64_t state = 14695981039346656037ULL;
    state ^= (uint64_t)color_index;
    state *= 1099511628211ULL;
    state ^= (uint64_t)(uint32_t)cell_x;
    state *= 1099511628211ULL;
    state ^= (uint64_t)(uint32_t)cell_y;
    state *= 1099511628211ULL;
    return state;
}

static size_t lookup_cross_cluster(
    uint8_t color_index,
    int32_t target_x,
    int32_t target_y,
    size_t bucket_mask,
    const int32_t *bucket_heads,
    const CrossClusterNode *nodes,
    const CrossCluster *clusters
) {
    constexpr int32_t CLUSTER_TOLERANCE_UNITS = 3;
    const int32_t base_cell_x = floor_divide_int32(target_x, CLUSTER_TOLERANCE_UNITS);
    const int32_t base_cell_y = floor_divide_int32(target_y, CLUSTER_TOLERANCE_UNITS);

    for (int32_t neighbor_offset = 0; neighbor_offset < 9; neighbor_offset += 1) {
        const int32_t offset_x = (neighbor_offset % 3) - 1;
        const int32_t offset_y = (neighbor_offset / 3) - 1;
        const size_t bucket_index =
            (size_t)hash_color_and_cell(color_index, base_cell_x + offset_x, base_cell_y + offset_y) &
            bucket_mask;

        int32_t node_index = bucket_heads[bucket_index];
        while (node_index >= 0) {
            const CrossClusterNode node = nodes[node_index];
            node_index = node.next_node_index;
            const CrossCluster cluster = clusters[node.cluster_index];
            if (cluster.color_index != color_index) {
                continue;
            }
            int32_t diff_x = target_x - cluster.canonical_center_x;
            if (diff_x < 0) {
                diff_x = -diff_x;
            }
            int32_t diff_y = target_y - cluster.canonical_center_y;
            if (diff_y < 0) {
                diff_y = -diff_y;
            }
            if (diff_x <= CLUSTER_TOLERANCE_UNITS && diff_y <= CLUSTER_TOLERANCE_UNITS) {
                return node.cluster_index;
            }
        }
    }
    return SIZE_MAX;
}

static bool evaluate_segment_keep_with_clusters(
    StitchPoint start_point,
    StitchPoint end_point,
    uint8_t color_index,
    int32_t pitch,
    EditOperationKind operation,
    CutRectangle rectangle,
    int32_t split_coordinate,
    bool keep_second_half,
    size_t cluster_count,
    size_t bucket_mask,
    const int32_t *bucket_heads,
    const CrossClusterNode *cluster_nodes,
    const CrossCluster *clusters
) {
    const int32_t midpoint_x =
        (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
    const int32_t midpoint_y =
        (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);

    if (cluster_count > 0U && is_cross_diagonal_for_pitch(start_point, end_point, pitch)) {
        const size_t cluster_index = lookup_cross_cluster(
            color_index,
            midpoint_x,
            midpoint_y,
            bucket_mask,
            bucket_heads,
            cluster_nodes,
            clusters
        );
        if (cluster_index != SIZE_MAX) {
            return clusters[cluster_index].keep_cluster;
        }
    }

    if (cluster_count > 0U && is_cross_center_connector(start_point, end_point, pitch)) {
        size_t cluster_index = lookup_cross_cluster(
            color_index,
            start_point.position_x,
            start_point.position_y,
            bucket_mask,
            bucket_heads,
            cluster_nodes,
            clusters
        );
        if (cluster_index == SIZE_MAX) {
            cluster_index = lookup_cross_cluster(
                color_index,
                end_point.position_x,
                end_point.position_y,
                bucket_mask,
                bucket_heads,
                cluster_nodes,
                clusters
            );
        }
        if (cluster_index != SIZE_MAX) {
            return clusters[cluster_index].keep_cluster;
        }
    }

    return point_kept_by_edit(
        operation,
        midpoint_x,
        midpoint_y,
        rectangle,
        split_coordinate,
        keep_second_half
    );
}

static void insert_chain_diagonals_into_clusters(
    const StitchPoint *chain_points,
    size_t point_count,
    uint8_t color_index,
    int32_t pitch,
    size_t bucket_mask,
    int32_t *bucket_heads,
    CrossClusterNode *cluster_nodes,
    CrossCluster *clusters,
    size_t *cluster_count
) {
    constexpr int32_t CLUSTER_TOLERANCE_UNITS = 3;
    for (size_t local_index = 0U; local_index + 1U < point_count; local_index += 1U) {
        const StitchPoint start_point = chain_points[local_index];
        const StitchPoint end_point = chain_points[local_index + 1U];
        if (!is_cross_diagonal_for_pitch(start_point, end_point, pitch)) {
            continue;
        }
        const int32_t midpoint_x =
            (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
        const int32_t midpoint_y =
            (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);

        const size_t existing_index = lookup_cross_cluster(
            color_index,
            midpoint_x,
            midpoint_y,
            bucket_mask,
            bucket_heads,
            cluster_nodes,
            clusters
        );
        if (existing_index != SIZE_MAX) {
            clusters[existing_index].sum_center_x += (int64_t)midpoint_x;
            clusters[existing_index].sum_center_y += (int64_t)midpoint_y;
            clusters[existing_index].diagonal_count += 1U;
            clusters[existing_index].canonical_center_x =
                (int32_t)(clusters[existing_index].sum_center_x /
                          (int64_t)clusters[existing_index].diagonal_count);
            clusters[existing_index].canonical_center_y =
                (int32_t)(clusters[existing_index].sum_center_y /
                          (int64_t)clusters[existing_index].diagonal_count);
            continue;
        }

        const size_t new_cluster_index = *cluster_count;
        clusters[new_cluster_index] = (CrossCluster){
            .color_index = color_index,
            .sum_center_x = (int64_t)midpoint_x,
            .sum_center_y = (int64_t)midpoint_y,
            .canonical_center_x = midpoint_x,
            .canonical_center_y = midpoint_y,
            .diagonal_count = 1U,
            .keep_cluster = false
        };
        const int32_t cell_x = floor_divide_int32(midpoint_x, CLUSTER_TOLERANCE_UNITS);
        const int32_t cell_y = floor_divide_int32(midpoint_y, CLUSTER_TOLERANCE_UNITS);
        const size_t bucket_index =
            (size_t)hash_color_and_cell(color_index, cell_x, cell_y) & bucket_mask;
        cluster_nodes[new_cluster_index].cluster_index = new_cluster_index;
        cluster_nodes[new_cluster_index].next_node_index = bucket_heads[bucket_index];
        bucket_heads[bucket_index] = (int32_t)new_cluster_index;
        *cluster_count += 1U;
    }
}

/* Unified keep-mask builder used identically by both live preview and edit commit. */
static bool build_edit_segment_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    EditOperationKind operation,
    CutRectangle rectangle,
    int32_t split_coordinate,
    bool keep_second_half,
    uint8_t *keep_mask,
    size_t mask_length
) {
    bool succeeded = false;
    CrossCluster *clusters = nullptr;
    CrossClusterNode *cluster_nodes = nullptr;
    int32_t *bucket_heads = nullptr;

    if (mask_length == 0U || keep_mask == nullptr) {
        return true;
    }
    memset(keep_mask, 0, mask_length);

    if (rectangle.minimum_x > rectangle.maximum_x) {
        const int32_t swap_x = rectangle.minimum_x;
        rectangle.minimum_x = rectangle.maximum_x;
        rectangle.maximum_x = swap_x;
    }
    if (rectangle.minimum_y > rectangle.maximum_y) {
        const int32_t swap_y = rectangle.minimum_y;
        rectangle.minimum_y = rectangle.maximum_y;
        rectangle.maximum_y = swap_y;
    }

    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }

    size_t diagonal_count = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled ||
            chain.point_count < 2U) {
            continue;
        }
        const StitchPoint *chain_points = &design->points[chain.point_offset];
        for (size_t local_index = 0U; local_index + 1U < chain.point_count; local_index += 1U) {
            if (is_cross_diagonal_for_pitch(chain_points[local_index], chain_points[local_index + 1U], pitch)) {
                diagonal_count += 1U;
            }
        }
    }

    size_t cluster_count = 0U;
    size_t bucket_mask = 0U;

    if (diagonal_count > 0U) {
        size_t bucket_count = 1024U;
        while (bucket_count < diagonal_count * 2U && bucket_count < 1048576U) {
            bucket_count *= 2U;
        }
        bucket_mask = bucket_count - 1U;

        size_t cluster_bytes = 0U;
        size_t node_bytes = 0U;
        size_t bucket_bytes = 0U;
        if (ckd_mul(&cluster_bytes, diagonal_count, sizeof(CrossCluster)) ||
            ckd_mul(&node_bytes, diagonal_count, sizeof(CrossClusterNode)) ||
            ckd_mul(&bucket_bytes, bucket_count, sizeof(int32_t))) {
            goto cleanup;
        }

        clusters = (CrossCluster *)malloc(cluster_bytes);
        cluster_nodes = (CrossClusterNode *)malloc(node_bytes);
        bucket_heads = (int32_t *)malloc(bucket_bytes);
        if (clusters == nullptr || cluster_nodes == nullptr || bucket_heads == nullptr) {
            fprintf(stderr, "Failed to allocate cross cluster buffers\n");
            goto cleanup;
        }
        for (size_t bucket_index = 0U; bucket_index < bucket_count; bucket_index += 1U) {
            bucket_heads[bucket_index] = -1;
        }

        for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
            const StitchChain chain = design->chains[chain_index];
            if (chain.color_block_index >= design->block_count ||
                !design->blocks[chain.color_block_index].enabled ||
                chain.point_count < 2U) {
                continue;
            }
            insert_chain_diagonals_into_clusters(
                &design->points[chain.point_offset],
                chain.point_count,
                design->blocks[chain.color_block_index].janome_color_index,
                pitch,
                bucket_mask,
                bucket_heads,
                cluster_nodes,
                clusters,
                &cluster_count
            );
        }

        for (size_t cluster_index = 0U; cluster_index < cluster_count; cluster_index += 1U) {
            clusters[cluster_index].keep_cluster = point_kept_by_edit(
                operation,
                clusters[cluster_index].canonical_center_x,
                clusters[cluster_index].canonical_center_y,
                rectangle,
                split_coordinate,
                keep_second_half
            );
        }
    }

    size_t flat_segment_index = 0U;
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

        const uint8_t color_index = design->blocks[chain.color_block_index].janome_color_index;
        const StitchPoint *chain_points = &design->points[chain.point_offset];

        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (flat_segment_index + local_index >= mask_length) {
                break;
            }
            if (evaluate_segment_keep_with_clusters(
                    chain_points[local_index],
                    chain_points[local_index + 1U],
                    color_index,
                    pitch,
                    operation,
                    rectangle,
                    split_coordinate,
                    keep_second_half,
                    cluster_count,
                    bucket_mask,
                    bucket_heads,
                    cluster_nodes,
                    clusters
                )) {
                keep_mask[flat_segment_index + local_index] = 1U;
            }
        }

        /* Remove only isolated cut artifacts without touching whole crosses or external stitches. */
        size_t local_segment = 0U;
        while (local_segment < chain_segments) {
            while (local_segment < chain_segments &&
                   flat_segment_index + local_segment < mask_length &&
                   keep_mask[flat_segment_index + local_segment] == 0U) {
                local_segment += 1U;
            }
            if (local_segment >= chain_segments) {
                break;
            }
            const size_t run_start = local_segment;
            while (local_segment < chain_segments &&
                   flat_segment_index + local_segment < mask_length &&
                   keep_mask[flat_segment_index + local_segment] != 0U) {
                local_segment += 1U;
            }
            const size_t run_length = local_segment - run_start;
            const bool run_was_cut = (run_start > 0U || local_segment < chain_segments);
            if (run_was_cut && run_is_cut_artifact(chain_points, run_start, run_length, pitch)) {
                memset(keep_mask + flat_segment_index + run_start, 0, run_length);
            }
        }

        flat_segment_index += chain_segments;
    }

    succeeded = true;

cleanup:
    free(bucket_heads);
    free(cluster_nodes);
    free(clusters);
    return succeeded;
}

static void compute_lock_step(
    StitchPoint anchor_point,
    StitchPoint neighbor_point,
    int32_t *step_x_out,
    int32_t *step_y_out
) {
    const double delta_x =
        (double)((int64_t)neighbor_point.position_x - (int64_t)anchor_point.position_x);
    const double delta_y =
        (double)((int64_t)neighbor_point.position_y - (int64_t)anchor_point.position_y);
    const double segment_length = sqrt(delta_x * delta_x + delta_y * delta_y);

    if (segment_length < 0.001) {
        *step_x_out = LOCK_STITCH_UNITS;
        *step_y_out = 0;
        return;
    }

    const double scale = (double)LOCK_STITCH_UNITS / segment_length;
    int32_t step_x = (int32_t)llround(delta_x * scale);
    int32_t step_y = (int32_t)llround(delta_y * scale);
    if (step_x == 0 && step_y == 0) {
        if (fabs(delta_x) >= fabs(delta_y)) {
            if (delta_x < 0.0) {
                step_x = -LOCK_STITCH_UNITS;
            } else {
                step_x = LOCK_STITCH_UNITS;
            }
        } else {
            if (delta_y < 0.0) {
                step_y = -LOCK_STITCH_UNITS;
            } else {
                step_y = LOCK_STITCH_UNITS;
            }
        }
    }
    *step_x_out = step_x;
    *step_y_out = step_y;
}

static bool append_locked_subchain(
    StitchDesign *destination,
    const StitchPoint *source_points,
    size_t run_start_index,
    size_t run_point_count,
    size_t color_block_index,
    bool cut_at_start,
    bool cut_at_end
) {
    const size_t output_point_offset = destination->point_count;
    size_t extra_lock_points = 0U;
    if (cut_at_start) {
        extra_lock_points += 3U;
    }
    if (cut_at_end) {
        extra_lock_points += 3U;
    }
    if (!design_reserve_points(
            destination,
            destination->point_count + run_point_count + extra_lock_points
        )) {
        return false;
    }

    if (cut_at_start) {
        const StitchPoint anchor_point = source_points[run_start_index];
        StitchPoint neighbor_point = source_points[run_start_index + 1U];
        for (size_t offset_index = 1U; offset_index < run_point_count; offset_index += 1U) {
            const StitchPoint candidate = source_points[run_start_index + offset_index];
            if (candidate.position_x != anchor_point.position_x ||
                candidate.position_y != anchor_point.position_y) {
                neighbor_point = candidate;
                break;
            }
        }

        int32_t lock_step_x = 0;
        int32_t lock_step_y = 0;
        compute_lock_step(anchor_point, neighbor_point, &lock_step_x, &lock_step_y);
        const int32_t forward_x = anchor_point.position_x + lock_step_x;
        const int32_t forward_y = anchor_point.position_y + lock_step_y;

        if (!design_append_point(destination, forward_x, forward_y) ||
            !design_append_point(destination, anchor_point.position_x, anchor_point.position_y) ||
            !design_append_point(destination, forward_x, forward_y)) {
            return false;
        }
    }

    for (size_t local_index = 0U; local_index < run_point_count; local_index += 1U) {
        const StitchPoint current_point = source_points[run_start_index + local_index];
        if (!design_append_point(destination, current_point.position_x, current_point.position_y)) {
            return false;
        }
    }

    if (cut_at_end) {
        const StitchPoint anchor_point = source_points[run_start_index + run_point_count - 1U];
        StitchPoint neighbor_point = source_points[run_start_index + run_point_count - 2U];
        for (size_t reverse_step = 2U; reverse_step <= run_point_count; reverse_step += 1U) {
            const StitchPoint candidate =
                source_points[run_start_index + run_point_count - reverse_step];
            if (candidate.position_x != anchor_point.position_x ||
                candidate.position_y != anchor_point.position_y) {
                neighbor_point = candidate;
                break;
            }
        }

        int32_t lock_step_x = 0;
        int32_t lock_step_y = 0;
        compute_lock_step(anchor_point, neighbor_point, &lock_step_x, &lock_step_y);
        const int32_t inward_x = anchor_point.position_x + lock_step_x;
        const int32_t inward_y = anchor_point.position_y + lock_step_y;

        if (!design_append_point(destination, inward_x, inward_y) ||
            !design_append_point(destination, anchor_point.position_x, anchor_point.position_y) ||
            !design_append_point(destination, inward_x, inward_y)) {
            return false;
        }
    }

    const size_t output_point_count = destination->point_count - output_point_offset;
    return design_append_chain(
        destination,
        output_point_offset,
        output_point_count,
        color_block_index,
        cut_at_start,
        cut_at_end
    );
}

static bool reorder_cutout_chains_by_side(StitchDesign *design, CutRectangle cutout_rectangle) {
    if (design->chain_count < 2U) {
        return true;
    }

    size_t left_or_right_chains = 0U;
    size_t top_or_bottom_chains = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        bool has_chain_bounds = false;
        int64_t chain_min_x = 0LL;
        int64_t chain_max_x = 0LL;
        int64_t chain_min_y = 0LL;
        int64_t chain_max_y = 0LL;
        expand_bounds_from_chain(
            design,
            chain,
            &has_chain_bounds,
            &chain_min_x,
            &chain_max_x,
            &chain_min_y,
            &chain_max_y
        );
        if (chain_max_x <= (int64_t)cutout_rectangle.minimum_x ||
            chain_min_x >= (int64_t)cutout_rectangle.maximum_x) {
            left_or_right_chains += 1U;
        }
        if (chain_max_y <= (int64_t)cutout_rectangle.minimum_y ||
            chain_min_y >= (int64_t)cutout_rectangle.maximum_y) {
            top_or_bottom_chains += 1U;
        }
    }

    const int64_t rectangle_width =
        (int64_t)cutout_rectangle.maximum_x - (int64_t)cutout_rectangle.minimum_x;
    const int64_t rectangle_height =
        (int64_t)cutout_rectangle.maximum_y - (int64_t)cutout_rectangle.minimum_y;
    bool split_along_x = (rectangle_height >= rectangle_width);
    if (left_or_right_chains > 0U && top_or_bottom_chains == 0U) {
        split_along_x = true;
    } else if (top_or_bottom_chains > 0U && left_or_right_chains == 0U) {
        split_along_x = false;
    }

    const int64_t rectangle_center_x =
        ((int64_t)cutout_rectangle.minimum_x + (int64_t)cutout_rectangle.maximum_x) / 2LL;
    const int64_t rectangle_center_y =
        ((int64_t)cutout_rectangle.minimum_y + (int64_t)cutout_rectangle.maximum_y) / 2LL;

    uint8_t *chain_sides = (uint8_t *)malloc(design->chain_count * sizeof(uint8_t));
    StitchChain *reordered_chains =
        (StitchChain *)malloc(design->chain_count * sizeof(StitchChain));
    if (chain_sides == nullptr || reordered_chains == nullptr) {
        free(chain_sides);
        free(reordered_chains);
        fprintf(stderr, "Failed to allocate cutout reorder buffers\n");
        return false;
    }

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        int64_t sum_x = 0LL;
        int64_t sum_y = 0LL;
        for (size_t local_index = 0U; local_index < chain.point_count; local_index += 1U) {
            const StitchPoint point = design->points[chain.point_offset + local_index];
            sum_x += (int64_t)point.position_x;
            sum_y += (int64_t)point.position_y;
        }
        const int64_t average_x = sum_x / (int64_t)chain.point_count;
        const int64_t average_y = sum_y / (int64_t)chain.point_count;

        chain_sides[chain_index] = 0U;
        if (split_along_x) {
            if (average_x >= rectangle_center_x) {
                chain_sides[chain_index] = 1U;
            }
        } else if (average_y >= rectangle_center_y) {
            chain_sides[chain_index] = 1U;
        }
    }

    size_t write_index = 0U;
    size_t range_start = 0U;
    while (range_start < design->chain_count) {
        const size_t block_index = design->chains[range_start].color_block_index;
        size_t range_end = range_start + 1U;
        while (range_end < design->chain_count &&
               design->chains[range_end].color_block_index == block_index) {
            range_end += 1U;
        }

        for (size_t chain_index = range_start; chain_index < range_end; chain_index += 1U) {
            if (chain_sides[chain_index] == 0U) {
                reordered_chains[write_index] = design->chains[chain_index];
                write_index += 1U;
            }
        }
        for (size_t chain_index = range_start; chain_index < range_end; chain_index += 1U) {
            if (chain_sides[chain_index] != 0U) {
                reordered_chains[write_index] = design->chains[chain_index];
                write_index += 1U;
            }
        }

        range_start = range_end;
    }

    memcpy(design->chains, reordered_chains, design->chain_count * sizeof(StitchChain));
    free(chain_sides);
    free(reordered_chains);
    return true;
}

static bool apply_edit_operation(
    const StitchDesign *source,
    CrossGridInfo grid,
    EditOperationKind operation,
    CutRectangle rectangle,
    int32_t split_coordinate,
    bool keep_second_half,
    StitchDesign *result
) {
    bool succeeded = false;
    uint8_t *keep_mask = nullptr;
    init_stitch_design(result);

    if (rectangle.minimum_x > rectangle.maximum_x) {
        const int32_t swap_x = rectangle.minimum_x;
        rectangle.minimum_x = rectangle.maximum_x;
        rectangle.maximum_x = swap_x;
    }
    if (rectangle.minimum_y > rectangle.maximum_y) {
        const int32_t swap_y = rectangle.minimum_y;
        rectangle.minimum_y = rectangle.maximum_y;
        rectangle.maximum_y = swap_y;
    }

    const size_t total_design_segments = count_design_segments(source);
    if (total_design_segments == 0U) {
        succeeded = true;
        goto cleanup;
    }

    keep_mask = (uint8_t *)calloc(total_design_segments, sizeof(uint8_t));
    if (keep_mask == nullptr) {
        fprintf(stderr, "Failed to allocate edit keep mask\n");
        goto cleanup;
    }

    if (!build_edit_segment_mask(
            source,
            grid,
            operation,
            rectangle,
            split_coordinate,
            keep_second_half,
            keep_mask,
            total_design_segments
        )) {
        goto cleanup;
    }

    if (source->block_count > 0U) {
        if (!design_reserve_blocks(result, source->block_count)) {
            goto cleanup;
        }
        memcpy(result->blocks, source->blocks, source->block_count * sizeof(ColorBlock));
        result->block_count = source->block_count;
    }
    size_t reserve_points = 0U;
    size_t reserve_chains = 0U;
    if (ckd_add(&reserve_points, source->point_count, (size_t)1024U) ||
        ckd_add(&reserve_chains, source->chain_count, (size_t)128U) ||
        !design_reserve_points(result, reserve_points) ||
        !design_reserve_chains(result, reserve_chains)) {
        goto cleanup;
    }

    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < source->chain_count; chain_index += 1U) {
        const StitchChain chain = source->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t total_segments = chain.point_count - 1U;
        if (chain.color_block_index >= source->block_count ||
            !source->blocks[chain.color_block_index].enabled) {
            flat_segment_index += total_segments;
            continue;
        }

        const StitchPoint *chain_points = &source->points[chain.point_offset];
        size_t segment_index = 0U;

        while (segment_index < total_segments) {
            while (segment_index < total_segments &&
                   keep_mask[flat_segment_index + segment_index] == 0U) {
                segment_index += 1U;
            }
            if (segment_index >= total_segments) {
                break;
            }

            const size_t run_start_segment = segment_index;
            while (segment_index < total_segments &&
                   keep_mask[flat_segment_index + segment_index] != 0U) {
                segment_index += 1U;
            }

            const size_t run_segment_count = segment_index - run_start_segment;
            const bool cut_at_start = (run_start_segment > 0U);
            const bool cut_at_end = (segment_index < total_segments);

            if (!append_locked_subchain(
                    result,
                    chain_points,
                    run_start_segment,
                    run_segment_count + 1U,
                    chain.color_block_index,
                    cut_at_start,
                    cut_at_end
                )) {
                goto cleanup;
            }
        }
        flat_segment_index += total_segments;
    }

    if (!compact_non_empty_blocks(result)) {
        goto cleanup;
    }
    if (operation == EDIT_CUT_OUT && !reorder_cutout_chains_by_side(result, rectangle)) {
        goto cleanup;
    }

    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    free(keep_mask);
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

static void world_to_grid_cell(
    int32_t world_x,
    int32_t world_y,
    CrossGridInfo grid,
    int32_t *cell_column_out,
    int32_t *cell_row_out
) {
    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }
    *cell_column_out = floor_divide_int32(world_x - grid.grid_origin_x, pitch);
    *cell_row_out = floor_divide_int32(world_y - grid.grid_origin_y, pitch);
}

static CutRectangle resolve_cross_box_at_point(
    const StitchDesign *design,
    CrossGridInfo grid,
    int32_t world_x,
    int32_t world_y,
    bool *snapped_to_stitch_out
) {
    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }

    const int64_t maximum_search_radius = ((int64_t)pitch * 65LL) / 100LL + 2LL;
    const int64_t maximum_radius_squared = maximum_search_radius * maximum_search_radius;
    int64_t best_distance_squared = maximum_radius_squared + 1LL;
    int32_t best_center_x = 0;
    int32_t best_center_y = 0;
    bool snapped = false;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled ||
            chain.point_count < 2U) {
            continue;
        }
        for (size_t local_index = 0U; local_index + 1U < chain.point_count; local_index += 1U) {
            const StitchPoint start_point = design->points[chain.point_offset + local_index];
            const StitchPoint end_point = design->points[chain.point_offset + local_index + 1U];
            int32_t diagonal_pitch = 0;
            if (!measure_diagonal_pitch(start_point, end_point, &diagonal_pitch)) {
                continue;
            }
            int32_t pitch_error = diagonal_pitch - pitch;
            if (pitch_error < 0) {
                pitch_error = -pitch_error;
            }
            if (pitch_error > 2) {
                continue;
            }
            const int32_t midpoint_x =
                (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
            const int32_t midpoint_y =
                (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);
            const int64_t delta_x = (int64_t)midpoint_x - (int64_t)world_x;
            const int64_t delta_y = (int64_t)midpoint_y - (int64_t)world_y;
            const int64_t distance_squared = delta_x * delta_x + delta_y * delta_y;
            if (distance_squared < best_distance_squared) {
                best_distance_squared = distance_squared;
                best_center_x = midpoint_x;
                best_center_y = midpoint_y;
                snapped = true;
            }
        }
    }

    if (snapped_to_stitch_out != nullptr) {
        *snapped_to_stitch_out = snapped;
    }

    CutRectangle box;
    if (snapped) {
        const int32_t half_pitch = pitch / 2;
        box.minimum_x = best_center_x - half_pitch;
        box.maximum_x = box.minimum_x + pitch;
        box.minimum_y = best_center_y - half_pitch;
        box.maximum_y = box.minimum_y + pitch;
        return box;
    }

    int32_t cell_column = 0;
    int32_t cell_row = 0;
    world_to_grid_cell(world_x, world_y, grid, &cell_column, &cell_row);
    box.minimum_x = grid.grid_origin_x + cell_column * pitch;
    box.maximum_x = box.minimum_x + pitch;
    box.minimum_y = grid.grid_origin_y + cell_row * pitch;
    box.maximum_y = box.minimum_y + pitch;
    return box;
}

static bool segment_belongs_to_cross_box(
    StitchPoint start_point,
    StitchPoint end_point,
    CutRectangle cross_box
) {
    if (start_point.position_x < cross_box.minimum_x - 2 ||
        start_point.position_x > cross_box.maximum_x + 2 ||
        end_point.position_x < cross_box.minimum_x - 2 ||
        end_point.position_x > cross_box.maximum_x + 2 ||
        start_point.position_y < cross_box.minimum_y - 2 ||
        start_point.position_y > cross_box.maximum_y + 2 ||
        end_point.position_y < cross_box.minimum_y - 2 ||
        end_point.position_y > cross_box.maximum_y + 2) {
        return false;
    }

    const int32_t midpoint_x =
        (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
    const int32_t midpoint_y =
        (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);
    return midpoint_x > cross_box.minimum_x &&
           midpoint_x < cross_box.maximum_x &&
           midpoint_y > cross_box.minimum_y &&
           midpoint_y < cross_box.maximum_y;
}

static size_t select_cross_box_in_mask(
    const StitchDesign *design,
    CutRectangle cross_box,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode
) {
    size_t matched_count = 0U;
    size_t already_selected_count = 0U;
    size_t flat_segment_index = 0U;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const bool block_enabled =
            chain.color_block_index < design->block_count &&
            design->blocks[chain.color_block_index].enabled;
        const size_t chain_segments = chain.point_count - 1U;
        if (!block_enabled) {
            flat_segment_index += chain_segments;
            continue;
        }

        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (flat_segment_index < mask_length &&
                segment_belongs_to_cross_box(
                    design->points[chain.point_offset + local_index],
                    design->points[chain.point_offset + local_index + 1U],
                    cross_box
                )) {
                matched_count += 1U;
                if (segment_mask[flat_segment_index] != 0U) {
                    already_selected_count += 1U;
                }
            }
            flat_segment_index += 1U;
        }
    }

    if (matched_count == 0U) {
        return 0U;
    }

    uint8_t target_value = 1U;
    if (toggle_mode && already_selected_count == matched_count) {
        target_value = 0U;
    }

    flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const bool block_enabled =
            chain.color_block_index < design->block_count &&
            design->blocks[chain.color_block_index].enabled;
        const size_t chain_segments = chain.point_count - 1U;
        if (!block_enabled) {
            flat_segment_index += chain_segments;
            continue;
        }

        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (flat_segment_index < mask_length &&
                segment_belongs_to_cross_box(
                    design->points[chain.point_offset + local_index],
                    design->points[chain.point_offset + local_index + 1U],
                    cross_box
                )) {
                segment_mask[flat_segment_index] = target_value;
            }
            flat_segment_index += 1U;
        }
    }

    return matched_count;
}

static size_t select_cross_at_point_in_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    int32_t world_x,
    int32_t world_y,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode,
    CutRectangle *resolved_box_out
) {
    bool snapped = false;
    const CutRectangle cross_box = resolve_cross_box_at_point(design, grid, world_x, world_y, &snapped);
    if (resolved_box_out != nullptr) {
        *resolved_box_out = cross_box;
    }
    return select_cross_box_in_mask(design, cross_box, segment_mask, mask_length, toggle_mode);
}

static size_t select_crosses_in_rectangle_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    CutRectangle selection_rectangle,
    uint8_t *segment_mask,
    size_t mask_length
) {
    if (selection_rectangle.minimum_x > selection_rectangle.maximum_x) {
        const int32_t swap_x = selection_rectangle.minimum_x;
        selection_rectangle.minimum_x = selection_rectangle.maximum_x;
        selection_rectangle.maximum_x = swap_x;
    }
    if (selection_rectangle.minimum_y > selection_rectangle.maximum_y) {
        const int32_t swap_y = selection_rectangle.minimum_y;
        selection_rectangle.minimum_y = selection_rectangle.maximum_y;
        selection_rectangle.maximum_y = swap_y;
    }

    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }

    size_t selected_count = 0U;
    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const bool block_enabled =
            chain.color_block_index < design->block_count &&
            design->blocks[chain.color_block_index].enabled;
        const size_t chain_segments = chain.point_count - 1U;
        if (!block_enabled) {
            flat_segment_index += chain_segments;
            continue;
        }

        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            const StitchPoint start_point = design->points[chain.point_offset + local_index];
            const StitchPoint end_point = design->points[chain.point_offset + local_index + 1U];
            int64_t span_x = (int64_t)end_point.position_x - (int64_t)start_point.position_x;
            if (span_x < 0LL) {
                span_x = -span_x;
            }
            int64_t span_y = (int64_t)end_point.position_y - (int64_t)start_point.position_y;
            if (span_y < 0LL) {
                span_y = -span_y;
            }
            const StitchPoint midpoint = {
                (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL),
                (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL)
            };
            if (flat_segment_index < mask_length &&
                span_x <= (int64_t)pitch + 3LL &&
                span_y <= (int64_t)pitch + 3LL &&
                point_is_inside_rectangle(midpoint, selection_rectangle)) {
                segment_mask[flat_segment_index] = 1U;
                selected_count += 1U;
            }
            flat_segment_index += 1U;
        }
    }
    return selected_count;
}

static int64_t point_to_segment_distance_squared(
    int32_t query_x,
    int32_t query_y,
    StitchPoint start_point,
    StitchPoint end_point
) {
    const double delta_x = (double)end_point.position_x - (double)start_point.position_x;
    const double delta_y = (double)end_point.position_y - (double)start_point.position_y;
    const double length_squared = delta_x * delta_x + delta_y * delta_y;
    double projection = 0.0;
    if (length_squared > 0.0001) {
        projection =
            (((double)query_x - (double)start_point.position_x) * delta_x +
             ((double)query_y - (double)start_point.position_y) * delta_y) /
            length_squared;
        if (projection < 0.0) {
            projection = 0.0;
        }
        if (projection > 1.0) {
            projection = 1.0;
        }
    }
    const double closest_x = (double)start_point.position_x + projection * delta_x;
    const double closest_y = (double)start_point.position_y + projection * delta_y;
    const double difference_x = (double)query_x - closest_x;
    const double difference_y = (double)query_y - closest_y;
    return (int64_t)llround(difference_x * difference_x + difference_y * difference_y);
}

static bool find_nearest_chain_at_point(
    const StitchDesign *design,
    int32_t world_x,
    int32_t world_y,
    int32_t maximum_distance_units,
    size_t *out_chain_index
) {
    const int64_t maximum_distance_squared =
        (int64_t)maximum_distance_units * (int64_t)maximum_distance_units;
    int64_t best_distance_squared = maximum_distance_squared + 1LL;
    bool found = false;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled ||
            chain.point_count < 2U) {
            continue;
        }
        for (size_t local_index = 0U; local_index + 1U < chain.point_count; local_index += 1U) {
            const int64_t distance_squared = point_to_segment_distance_squared(
                world_x,
                world_y,
                design->points[chain.point_offset + local_index],
                design->points[chain.point_offset + local_index + 1U]
            );
            if (distance_squared < best_distance_squared) {
                best_distance_squared = distance_squared;
                *out_chain_index = chain_index;
                found = true;
            }
        }
    }
    return found;
}

static uint64_t hash_two_int32(int32_t cell_x, int32_t cell_y) {
    uint64_t state = 14695981039346656037ULL;
    state ^= (uint64_t)(uint32_t)cell_x;
    state *= 1099511628211ULL;
    state ^= (uint64_t)(uint32_t)cell_y;
    state *= 1099511628211ULL;
    return state;
}

static void enqueue_spatial_neighbors_for_point(
    StitchPoint point,
    int32_t threshold,
    int64_t threshold_squared,
    size_t bucket_mask,
    const int32_t *bucket_heads,
    const SpatialVertexNode *vertex_nodes,
    uint8_t *visited_flags,
    size_t *bfs_queue,
    size_t *queue_tail
) {
    const int32_t base_cell_x = floor_divide_int32(point.position_x, threshold);
    const int32_t base_cell_y = floor_divide_int32(point.position_y, threshold);

    for (int32_t neighbor_offset = 0; neighbor_offset < 9; neighbor_offset += 1) {
        const int32_t offset_x = (neighbor_offset % 3) - 1;
        const int32_t offset_y = (neighbor_offset / 3) - 1;
        const size_t bucket_index =
            (size_t)hash_two_int32(base_cell_x + offset_x, base_cell_y + offset_y) & bucket_mask;

        int32_t node_index = bucket_heads[bucket_index];
        while (node_index >= 0) {
            const SpatialVertexNode node = vertex_nodes[node_index];
            node_index = node.next_node_index;
            if (visited_flags[node.local_chain_index] != 0U) {
                continue;
            }
            const int64_t difference_x = (int64_t)node.position_x - (int64_t)point.position_x;
            const int64_t difference_y = (int64_t)node.position_y - (int64_t)point.position_y;
            if (difference_x * difference_x + difference_y * difference_y <= threshold_squared) {
                visited_flags[node.local_chain_index] = 1U;
                bfs_queue[*queue_tail] = node.local_chain_index;
                *queue_tail += 1U;
            }
        }
    }
}

/* Iterative spatial-hash BFS finding connected same-color chains within pitch * 1.25. */
static size_t select_connected_element_in_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    size_t seed_chain_index,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode
) {
    size_t selected_segments = 0U;
    size_t *same_color_chains = nullptr;
    size_t *chain_segment_offsets = nullptr;
    uint8_t *visited_flags = nullptr;
    size_t *bfs_queue = nullptr;
    int32_t *bucket_heads = nullptr;
    SpatialVertexNode *vertex_nodes = nullptr;

    if (seed_chain_index >= design->chain_count) {
        return 0U;
    }
    const size_t seed_block_index = design->chains[seed_chain_index].color_block_index;
    if (seed_block_index >= design->block_count || !design->blocks[seed_block_index].enabled) {
        return 0U;
    }
    const uint8_t target_janome_index = design->blocks[seed_block_index].janome_color_index;

    int32_t threshold = (int32_t)llround((double)grid.cross_pitch_units * 1.25);
    if (threshold < 12) {
        threshold = 12;
    }
    const int64_t threshold_squared = (int64_t)threshold * (int64_t)threshold;

    chain_segment_offsets = (size_t *)malloc(design->chain_count * sizeof(size_t));
    same_color_chains = (size_t *)malloc(design->chain_count * sizeof(size_t));
    if (chain_segment_offsets == nullptr || same_color_chains == nullptr) {
        goto cleanup;
    }

    size_t running_segment = 0U;
    size_t candidate_chain_count = 0U;
    size_t seed_local_index = SIZE_MAX;
    size_t total_candidate_vertices = 0U;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        chain_segment_offsets[chain_index] = running_segment;
        if (chain.point_count >= 2U) {
            running_segment += chain.point_count - 1U;
        } else {
            continue;
        }
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled) {
            continue;
        }
        if (design->blocks[chain.color_block_index].janome_color_index == target_janome_index) {
            if (chain_index == seed_chain_index) {
                seed_local_index = candidate_chain_count;
            }
            same_color_chains[candidate_chain_count] = chain_index;
            candidate_chain_count += 1U;
            total_candidate_vertices += chain.point_count;
        }
    }

    if (seed_local_index == SIZE_MAX || candidate_chain_count == 0U || total_candidate_vertices == 0U) {
        goto cleanup;
    }

    size_t bucket_count = 1024U;
    while (bucket_count < total_candidate_vertices * 2U && bucket_count < 1048576U) {
        bucket_count *= 2U;
    }
    const size_t bucket_mask = bucket_count - 1U;

    visited_flags = (uint8_t *)calloc(candidate_chain_count, sizeof(uint8_t));
    bfs_queue = (size_t *)malloc(candidate_chain_count * sizeof(size_t));
    bucket_heads = (int32_t *)malloc(bucket_count * sizeof(int32_t));
    vertex_nodes = (SpatialVertexNode *)malloc(total_candidate_vertices * sizeof(SpatialVertexNode));
    if (visited_flags == nullptr || bfs_queue == nullptr || bucket_heads == nullptr || vertex_nodes == nullptr) {
        goto cleanup;
    }

    for (size_t bucket_index = 0U; bucket_index < bucket_count; bucket_index += 1U) {
        bucket_heads[bucket_index] = -1;
    }

    size_t node_write_index = 0U;
    for (size_t local_chain = 0U; local_chain < candidate_chain_count; local_chain += 1U) {
        const StitchChain chain = design->chains[same_color_chains[local_chain]];
        for (size_t local_point_index = 0U; local_point_index < chain.point_count; local_point_index += 1U) {
            const StitchPoint point = design->points[chain.point_offset + local_point_index];
            const int32_t cell_x = floor_divide_int32(point.position_x, threshold);
            const int32_t cell_y = floor_divide_int32(point.position_y, threshold);
            const size_t bucket_index = (size_t)hash_two_int32(cell_x, cell_y) & bucket_mask;

            vertex_nodes[node_write_index].position_x = point.position_x;
            vertex_nodes[node_write_index].position_y = point.position_y;
            vertex_nodes[node_write_index].local_chain_index = local_chain;
            vertex_nodes[node_write_index].next_node_index = bucket_heads[bucket_index];
            bucket_heads[bucket_index] = (int32_t)node_write_index;
            node_write_index += 1U;
        }
    }

    size_t queue_head = 0U;
    size_t queue_tail = 0U;
    visited_flags[seed_local_index] = 1U;
    bfs_queue[queue_tail] = seed_local_index;
    queue_tail += 1U;

    while (queue_head < queue_tail) {
        const size_t current_local = bfs_queue[queue_head];
        queue_head += 1U;
        const StitchChain current_chain = design->chains[same_color_chains[current_local]];

        for (size_t local_point_index = 0U; local_point_index < current_chain.point_count; local_point_index += 1U) {
            enqueue_spatial_neighbors_for_point(
                design->points[current_chain.point_offset + local_point_index],
                threshold,
                threshold_squared,
                bucket_mask,
                bucket_heads,
                vertex_nodes,
                visited_flags,
                bfs_queue,
                &queue_tail
            );
        }
    }

    uint8_t target_mask_value = 1U;
    if (toggle_mode) {
        const size_t seed_segment_offset = chain_segment_offsets[seed_chain_index];
        if (seed_segment_offset < mask_length && segment_mask[seed_segment_offset] != 0U) {
            target_mask_value = 0U;
        }
    }

    for (size_t queue_index = 0U; queue_index < queue_tail; queue_index += 1U) {
        const size_t chain_index = same_color_chains[bfs_queue[queue_index]];
        const size_t segment_offset = chain_segment_offsets[chain_index];
        const size_t segment_count = design->chains[chain_index].point_count - 1U;
        for (size_t local_segment_index = 0U; local_segment_index < segment_count; local_segment_index += 1U) {
            if (segment_offset + local_segment_index < mask_length) {
                segment_mask[segment_offset + local_segment_index] = target_mask_value;
                selected_segments += 1U;
            }
        }
    }

cleanup:
    free(vertex_nodes);
    free(bucket_heads);
    free(bfs_queue);
    free(visited_flags);
    free(same_color_chains);
    free(chain_segment_offsets);
    return selected_segments;
}

static bool delete_masked_segments(
    const StitchDesign *source,
    const uint8_t *segment_mask,
    size_t mask_length,
    StitchDesign *result
) {
    bool succeeded = false;
    init_stitch_design(result);

    if (source->block_count > 0U) {
        if (!design_reserve_blocks(result, source->block_count)) {
            goto cleanup;
        }
        memcpy(result->blocks, source->blocks, source->block_count * sizeof(ColorBlock));
        result->block_count = source->block_count;
    }
    if (!design_reserve_points(result, source->point_count + 512U) ||
        !design_reserve_chains(result, source->chain_count + 64U)) {
        goto cleanup;
    }

    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < source->chain_count; chain_index += 1U) {
        const StitchChain chain = source->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t total_segments = chain.point_count - 1U;
        const StitchPoint *chain_points = &source->points[chain.point_offset];
        size_t local_segment = 0U;

        while (local_segment < total_segments) {
            while (local_segment < total_segments &&
                   flat_segment_index + local_segment < mask_length &&
                   segment_mask[flat_segment_index + local_segment] != 0U) {
                local_segment += 1U;
            }
            if (local_segment >= total_segments) {
                break;
            }

            const size_t run_start = local_segment;
            while (local_segment < total_segments &&
                   (flat_segment_index + local_segment >= mask_length ||
                    segment_mask[flat_segment_index + local_segment] == 0U)) {
                local_segment += 1U;
            }

            const size_t run_length = local_segment - run_start;
            const bool cut_at_start = (run_start > 0U);
            const bool cut_at_end = (local_segment < total_segments);
            if ((cut_at_start || cut_at_end) &&
                run_is_cut_artifact(chain_points, run_start, run_length, DEFAULT_CROSS_PITCH_UNITS)) {
                continue;
            }

            if (!append_locked_subchain(
                    result,
                    chain_points,
                    run_start,
                    run_length + 1U,
                    chain.color_block_index,
                    cut_at_start,
                    cut_at_end
                )) {
                goto cleanup;
            }
        }

        flat_segment_index += total_segments;
    }

    if (!compact_non_empty_blocks(result)) {
        goto cleanup;
    }
    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

static bool recolor_masked_segments(
    const StitchDesign *source,
    const uint8_t *segment_mask,
    size_t mask_length,
    uint8_t new_janome_index,
    StitchDesign *result
) {
    bool succeeded = false;
    init_stitch_design(result);

    if (new_janome_index == 0U || new_janome_index >= JANOME_PALETTE_COUNT) {
        new_janome_index = 1U;
    }

    if (source->block_count > 0U) {
        if (!design_reserve_blocks(result, source->block_count + 1U)) {
            goto cleanup;
        }
        memcpy(result->blocks, source->blocks, source->block_count * sizeof(ColorBlock));
        result->block_count = source->block_count;
    }

    size_t target_block_index = SIZE_MAX;
    for (size_t block_index = 0U; block_index < result->block_count; block_index += 1U) {
        if (result->blocks[block_index].enabled &&
            result->blocks[block_index].janome_color_index == new_janome_index) {
            target_block_index = block_index;
            break;
        }
    }
    if (target_block_index == SIZE_MAX) {
        if (!design_append_block(
                result,
                janome_palette[new_janome_index],
                new_janome_index,
                true,
                &target_block_index
            )) {
            goto cleanup;
        }
    }

    if (!design_reserve_points(result, source->point_count + 1024U) ||
        !design_reserve_chains(result, source->chain_count + 128U)) {
        goto cleanup;
    }

    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < source->chain_count; chain_index += 1U) {
        const StitchChain chain = source->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t total_segments = chain.point_count - 1U;
        const StitchPoint *chain_points = &source->points[chain.point_offset];
        size_t local_segment = 0U;

        while (local_segment < total_segments) {
            bool is_selected = false;
            if (flat_segment_index + local_segment < mask_length &&
                segment_mask[flat_segment_index + local_segment] != 0U) {
                is_selected = true;
            }
            const size_t run_start = local_segment;
            while (local_segment < total_segments) {
                bool next_selected = false;
                if (flat_segment_index + local_segment < mask_length &&
                    segment_mask[flat_segment_index + local_segment] != 0U) {
                    next_selected = true;
                }
                if (next_selected != is_selected) {
                    break;
                }
                local_segment += 1U;
            }

            const size_t run_length = local_segment - run_start;
            const bool cut_at_start = (run_start > 0U);
            const bool cut_at_end = (local_segment < total_segments);
            size_t destination_block = chain.color_block_index;
            if (is_selected) {
                destination_block = target_block_index;
            }

            if (!append_locked_subchain(
                    result,
                    chain_points,
                    run_start,
                    run_length + 1U,
                    destination_block,
                    cut_at_start,
                    cut_at_end
                )) {
                goto cleanup;
            }
        }
        flat_segment_index += total_segments;
    }

    if (!compact_non_empty_blocks(result)) {
        goto cleanup;
    }
    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

static bool shift_masked_segments(
    const StitchDesign *source,
    const uint8_t *segment_mask,
    size_t mask_length,
    int32_t delta_x,
    int32_t delta_y,
    StitchDesign *result,
    uint8_t **updated_mask_out,
    size_t *updated_mask_length_out
) {
    bool succeeded = false;
    uint8_t *chain_is_selected = nullptr;
    size_t chain_flag_capacity = 0U;
    uint8_t *next_mask = nullptr;
    init_stitch_design(result);

    if (source->block_count > 0U) {
        if (!design_reserve_blocks(result, source->block_count)) {
            goto cleanup;
        }
        memcpy(result->blocks, source->blocks, source->block_count * sizeof(ColorBlock));
        result->block_count = source->block_count;
    }
    if (!design_reserve_points(result, source->point_count + 512U) ||
        !design_reserve_chains(result, source->chain_count + 64U)) {
        goto cleanup;
    }

    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < source->chain_count; chain_index += 1U) {
        const StitchChain chain = source->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t total_segments = chain.point_count - 1U;
        const StitchPoint *chain_points = &source->points[chain.point_offset];
        size_t local_segment = 0U;

        while (local_segment < total_segments) {
            bool is_selected = false;
            if (flat_segment_index + local_segment < mask_length &&
                segment_mask[flat_segment_index + local_segment] != 0U) {
                is_selected = true;
            }
            const size_t run_start = local_segment;
            while (local_segment < total_segments) {
                bool next_selected = false;
                if (flat_segment_index + local_segment < mask_length &&
                    segment_mask[flat_segment_index + local_segment] != 0U) {
                    next_selected = true;
                }
                if (next_selected != is_selected) {
                    break;
                }
                local_segment += 1U;
            }

            const size_t run_length = local_segment - run_start;
            const bool cut_at_start = (run_start > 0U);
            const bool cut_at_end = (local_segment < total_segments);
            const size_t before_points = result->point_count;

            if (!append_locked_subchain(
                    result,
                    chain_points,
                    run_start,
                    run_length + 1U,
                    chain.color_block_index,
                    cut_at_start,
                    cut_at_end
                )) {
                goto cleanup;
            }

            if (result->chain_count > chain_flag_capacity) {
                size_t next_capacity = chain_flag_capacity * 2U;
                if (next_capacity < 64U) {
                    next_capacity = 64U;
                }
                while (next_capacity < result->chain_count) {
                    next_capacity *= 2U;
                }
                uint8_t *grown_flags = (uint8_t *)realloc(chain_is_selected, next_capacity);
                if (grown_flags == nullptr) {
                    goto cleanup;
                }
                chain_is_selected = grown_flags;
                chain_flag_capacity = next_capacity;
            }
            if (is_selected) {
                chain_is_selected[result->chain_count - 1U] = 1U;
            } else {
                chain_is_selected[result->chain_count - 1U] = 0U;
            }

            if (is_selected) {
                for (size_t point_index = before_points; point_index < result->point_count; point_index += 1U) {
                    const int64_t next_x =
                        (int64_t)result->points[point_index].position_x + (int64_t)delta_x;
                    const int64_t next_y =
                        (int64_t)result->points[point_index].position_y + (int64_t)delta_y;
                    if (next_x < -MAX_COORDINATE_MAGNITUDE || next_x > MAX_COORDINATE_MAGNITUDE ||
                        next_y < -MAX_COORDINATE_MAGNITUDE || next_y > MAX_COORDINATE_MAGNITUDE) {
                        goto cleanup;
                    }
                    result->points[point_index].position_x = (int32_t)next_x;
                    result->points[point_index].position_y = (int32_t)next_y;
                }
            }
        }
        flat_segment_index += total_segments;
    }

    if (updated_mask_out != nullptr && updated_mask_length_out != nullptr) {
        const size_t result_segments = count_design_segments(result);
        if (result_segments > 0U) {
            next_mask = (uint8_t *)calloc(result_segments, sizeof(uint8_t));
            if (next_mask == nullptr) {
                goto cleanup;
            }
            size_t write_segment = 0U;
            for (size_t chain_index = 0U; chain_index < result->chain_count; chain_index += 1U) {
                const size_t chain_segments = result->chains[chain_index].point_count - 1U;
                if (chain_is_selected[chain_index] != 0U) {
                    memset(next_mask + write_segment, 1, chain_segments);
                }
                write_segment += chain_segments;
            }
        }
        *updated_mask_out = next_mask;
        *updated_mask_length_out = result_segments;
        next_mask = nullptr;
    }

    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    free(next_mask);
    free(chain_is_selected);
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

static bool move_color_block_order(StitchDesign *design, size_t block_index, int32_t direction) {
    if (design->block_count < 2U || block_index >= design->block_count) {
        return false;
    }
    if (direction < 0 && block_index == 0U) {
        return false;
    }
    if (direction > 0 && block_index + 1U >= design->block_count) {
        return false;
    }

    size_t swap_index = block_index + 1U;
    if (direction < 0) {
        swap_index = block_index - 1U;
    }

    const ColorBlock saved_block = design->blocks[block_index];
    design->blocks[block_index] = design->blocks[swap_index];
    design->blocks[swap_index] = saved_block;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        if (design->chains[chain_index].color_block_index == block_index) {
            design->chains[chain_index].color_block_index = swap_index;
        } else if (design->chains[chain_index].color_block_index == swap_index) {
            design->chains[chain_index].color_block_index = block_index;
        }
    }

    return sort_chains_by_color_block(design);
}

static bool remove_color_block_at(StitchDesign *design, size_t target_block_index) {
    if (target_block_index >= design->block_count) {
        return false;
    }

    size_t write_chain_index = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        if (design->chains[chain_index].color_block_index != target_block_index) {
            StitchChain moved = design->chains[chain_index];
            if (moved.color_block_index > target_block_index) {
                moved.color_block_index -= 1U;
            }
            design->chains[write_chain_index] = moved;
            write_chain_index += 1U;
        }
    }
    design->chain_count = write_chain_index;

    for (size_t block_index = target_block_index + 1U;
         block_index < design->block_count;
         block_index += 1U) {
        design->blocks[block_index - 1U] = design->blocks[block_index];
    }
    design->block_count -= 1U;
    recompute_design_bounds(design);
    return true;
}

static bool merge_duplicate_color_blocks(StitchDesign *design) {
    if (design->block_count < 2U) {
        return true;
    }

    size_t first_block_for_janome[JANOME_PALETTE_COUNT];
    for (size_t palette_index = 0U; palette_index < JANOME_PALETTE_COUNT; palette_index += 1U) {
        first_block_for_janome[palette_index] = SIZE_MAX;
    }

    size_t *block_remap = (size_t *)malloc(design->block_count * sizeof(size_t));
    if (block_remap == nullptr) {
        return false;
    }

    size_t merged_block_count = 0U;
    for (size_t block_index = 0U; block_index < design->block_count; block_index += 1U) {
        uint8_t janome_index = design->blocks[block_index].janome_color_index;
        if (janome_index >= JANOME_PALETTE_COUNT) {
            janome_index = 1U;
        }
        if (first_block_for_janome[janome_index] == SIZE_MAX) {
            first_block_for_janome[janome_index] = merged_block_count;
            design->blocks[merged_block_count] = design->blocks[block_index];
            block_remap[block_index] = merged_block_count;
            merged_block_count += 1U;
        } else {
            const size_t target = first_block_for_janome[janome_index];
            if (design->blocks[block_index].enabled) {
                design->blocks[target].enabled = true;
            }
            block_remap[block_index] = target;
        }
    }
    design->block_count = merged_block_count;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const size_t old_block = design->chains[chain_index].color_block_index;
        design->chains[chain_index].color_block_index = block_remap[old_block];
    }

    free(block_remap);
    if (!sort_chains_by_color_block(design)) {
        return false;
    }
    recompute_design_bounds(design);
    return true;
}

static bool append_cross_cell_stitches(
    StitchDesign *design,
    int32_t cell_x,
    int32_t cell_y,
    int32_t pitch_units,
    int32_t passes
) {
    if (passes < MIN_CROSS_PASSES) {
        passes = MIN_CROSS_PASSES;
    }
    if (passes > MAX_CROSS_PASSES) {
        passes = MAX_CROSS_PASSES;
    }

    const int32_t left_x = cell_x;
    const int32_t right_x = cell_x + pitch_units;
    const int32_t top_y = cell_y;
    const int32_t bottom_y = cell_y + pitch_units;
    const int32_t center_x = cell_x + pitch_units / 2;
    const int32_t center_y = cell_y + pitch_units / 2;

    const StitchPoint corners[4] = {
        {left_x, bottom_y},
        {right_x, top_y},
        {right_x, bottom_y},
        {left_x, top_y}
    };

    size_t start_corner_index = 0U;
    if (design->point_count > 0U) {
        const StitchPoint previous_point = design->points[design->point_count - 1U];
        int64_t best_distance_squared = INT64_MAX;
        for (size_t corner_index = 0U; corner_index < 4U; corner_index += 1U) {
            const int64_t delta_x =
                (int64_t)corners[corner_index].position_x - (int64_t)previous_point.position_x;
            const int64_t delta_y =
                (int64_t)corners[corner_index].position_y - (int64_t)previous_point.position_y;
            const int64_t distance_squared = delta_x * delta_x + delta_y * delta_y;
            if (distance_squared < best_distance_squared) {
                best_distance_squared = distance_squared;
                start_corner_index = corner_index;
            }
        }
    }

    StitchPoint first_start = corners[0];
    StitchPoint first_end = corners[1];
    StitchPoint second_start = corners[2];
    StitchPoint second_end = corners[3];
    if (start_corner_index == 1U) {
        first_start = corners[1];
        first_end = corners[0];
        second_start = corners[3];
        second_end = corners[2];
    } else if (start_corner_index == 2U) {
        first_start = corners[2];
        first_end = corners[3];
        second_start = corners[0];
        second_end = corners[1];
    } else if (start_corner_index == 3U) {
        first_start = corners[3];
        first_end = corners[2];
        second_start = corners[1];
        second_end = corners[0];
    }

    if (!design_append_point(design, first_start.position_x, first_start.position_y)) {
        return false;
    }
    for (int32_t pass_index = 0; pass_index < passes; pass_index += 1) {
        if ((pass_index % 2) == 0) {
            if (!design_append_point(design, first_end.position_x, first_end.position_y)) {
                return false;
            }
        } else {
            if (!design_append_point(design, first_start.position_x, first_start.position_y)) {
                return false;
            }
        }
    }

    /* Route to second diagonal through cell center so the travel stays hidden under the cross. */
    if (!design_append_point(design, center_x, center_y) ||
        !design_append_point(design, second_start.position_x, second_start.position_y)) {
        return false;
    }
    for (int32_t pass_index = 0; pass_index < passes; pass_index += 1) {
        if ((pass_index % 2) == 0) {
            if (!design_append_point(design, second_end.position_x, second_end.position_y)) {
                return false;
            }
        } else {
            if (!design_append_point(design, second_start.position_x, second_start.position_y)) {
                return false;
            }
        }
    }
    return true;
}

static bool draw_single_cross_into_design(
    StitchDesign *design,
    CrossGridInfo grid,
    int32_t cell_column,
    int32_t cell_row,
    size_t color_block_index
) {
    if (color_block_index >= design->block_count) {
        return false;
    }

    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }
    const int32_t cell_x = grid.grid_origin_x + cell_column * pitch;
    const int32_t cell_y = grid.grid_origin_y + cell_row * pitch;

    StitchDesign scratch;
    init_stitch_design(&scratch);
    if (!append_cross_cell_stitches(&scratch, cell_x, cell_y, pitch, grid.cross_passes)) {
        free_stitch_design(&scratch);
        return false;
    }

    const bool appended_successfully = append_locked_subchain(
        design,
        scratch.points,
        0U,
        scratch.point_count,
        color_block_index,
        true,
        true
    );
    free_stitch_design(&scratch);
    if (!appended_successfully) {
        return false;
    }
    if (!sort_chains_by_color_block(design)) {
        return false;
    }
    recompute_design_bounds(design);
    return true;
}

static bool trace_strip_color_chain(
    const uint8_t *expanded_cells,
    uint8_t *visited_cells,
    size_t total_columns,
    size_t total_rows,
    size_t start_column,
    size_t start_row,
    uint8_t active_color,
    int32_t pitch,
    int32_t passes,
    StitchDesign *temporary_chain_points
) {
    /* Prioritize horizontal row continuity before vertical/diagonal steps. */
    static const int32_t neighbor_delta_column[8] = {1, -1, 0, 0, 1, -1, 1, -1};
    static const int32_t neighbor_delta_row[8] = {0, 0, 1, -1, 1, 1, -1, -1};

    temporary_chain_points->point_count = 0U;
    size_t current_column = start_column;
    size_t current_row = start_row;

    while (true) {
        visited_cells[current_row * total_columns + current_column] = 1U;
        if (!append_cross_cell_stitches(
                temporary_chain_points,
                (int32_t)current_column * pitch,
                (int32_t)current_row * pitch,
                pitch,
                passes
            )) {
            return false;
        }

        bool found_next = false;
        for (size_t direction_index = 0U; direction_index < 8U; direction_index += 1U) {
            const int64_t next_column =
                (int64_t)current_column + (int64_t)neighbor_delta_column[direction_index];
            const int64_t next_row =
                (int64_t)current_row + (int64_t)neighbor_delta_row[direction_index];
            if (next_column < 0LL || (size_t)next_column >= total_columns ||
                next_row < 0LL || (size_t)next_row >= total_rows) {
                continue;
            }
            const size_t next_flat = (size_t)next_row * total_columns + (size_t)next_column;
            if (expanded_cells[next_flat] == active_color && visited_cells[next_flat] == 0U) {
                current_column = (size_t)next_column;
                current_row = (size_t)next_row;
                found_next = true;
                break;
            }
        }
        if (!found_next) {
            break;
        }
    }
    return true;
}

static bool build_strip_design(const StripPattern *pattern, StitchDesign *result) {
    bool succeeded = false;
    uint8_t *expanded_cells = nullptr;
    uint8_t *visited_cells = nullptr;
    StitchDesign temporary_chain_points;
    init_stitch_design(&temporary_chain_points);
    init_stitch_design(result);

    if (pattern->width < 1 || pattern->width > STRIP_MAX_GRID_DIM ||
        pattern->height < 1 || pattern->height > STRIP_MAX_GRID_DIM ||
        pattern->repeat_x < 1 || pattern->repeat_x > STRIP_MAX_REPEAT ||
        pattern->repeat_y < 1 || pattern->repeat_y > STRIP_MAX_REPEAT) {
        fprintf(stderr, "Invalid strip pattern dimensions\n");
        goto cleanup;
    }

    size_t total_columns = 0U;
    size_t total_rows = 0U;
    size_t total_cells = 0U;
    if (ckd_mul(&total_columns, (size_t)pattern->width, (size_t)pattern->repeat_x) ||
        ckd_mul(&total_rows, (size_t)pattern->height, (size_t)pattern->repeat_y) ||
        ckd_mul(&total_cells, total_columns, total_rows) ||
        total_cells > STRIP_MAX_EXPANDED_CELLS) {
        fprintf(stderr, "Expanded strip exceeds cell limit\n");
        goto cleanup;
    }

    expanded_cells = (uint8_t *)calloc(total_cells, sizeof(uint8_t));
    visited_cells = (uint8_t *)calloc(total_cells, sizeof(uint8_t));
    if (expanded_cells == nullptr || visited_cells == nullptr) {
        fprintf(stderr, "Failed to allocate strip expansion buffers\n");
        goto cleanup;
    }

    uint8_t used_colors[JANOME_PALETTE_COUNT] = {};
    size_t used_color_count = 0U;
    bool color_seen[JANOME_PALETTE_COUNT] = {};

    for (size_t row = 0U; row < total_rows; row += 1U) {
        const size_t source_row = row % (size_t)pattern->height;
        for (size_t column = 0U; column < total_columns; column += 1U) {
            const size_t source_column = column % (size_t)pattern->width;
            const uint8_t color_index =
                pattern->cells[source_row * STRIP_MAX_GRID_DIM + source_column];
            if (color_index > 0U && color_index < JANOME_PALETTE_COUNT) {
                expanded_cells[row * total_columns + column] = color_index;
                if (!color_seen[color_index]) {
                    color_seen[color_index] = true;
                    used_colors[used_color_count] = color_index;
                    used_color_count += 1U;
                }
            }
        }
    }

    if (used_color_count == 0U) {
        fprintf(stderr, "Strip pattern contains no crosses\n");
        goto cleanup;
    }

    int32_t pitch = pattern->pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = MIN_CROSS_PITCH_UNITS;
    }
    if (pitch > MAX_CROSS_PITCH_UNITS) {
        pitch = MAX_CROSS_PITCH_UNITS;
    }

    for (size_t color_order = 0U; color_order < used_color_count; color_order += 1U) {
        const uint8_t active_color = used_colors[color_order];
        size_t block_index = 0U;
        if (!design_append_block(
                result,
                janome_palette[active_color],
                active_color,
                true,
                &block_index
            )) {
            goto cleanup;
        }

        for (size_t start_cell = 0U; start_cell < total_cells; start_cell += 1U) {
            if (expanded_cells[start_cell] != active_color || visited_cells[start_cell] != 0U) {
                continue;
            }
            const size_t start_row = start_cell / total_columns;
            const size_t start_column = start_cell % total_columns;
            if (!trace_strip_color_chain(
                    expanded_cells,
                    visited_cells,
                    total_columns,
                    total_rows,
                    start_column,
                    start_row,
                    active_color,
                    pitch,
                    pattern->passes,
                    &temporary_chain_points
                )) {
                goto cleanup;
            }
            if (!append_locked_subchain(
                    result,
                    temporary_chain_points.points,
                    0U,
                    temporary_chain_points.point_count,
                    block_index,
                    true,
                    true
                )) {
                goto cleanup;
            }
        }
    }

    /* Center by whole pitch cells so all cell corners stay exact multiples of pitch for grid snapping. */
    const int32_t center_shift_x = -(int32_t)(total_columns / 2U) * pitch;
    const int32_t center_shift_y = -(int32_t)(total_rows / 2U) * pitch;
    for (size_t point_index = 0U; point_index < result->point_count; point_index += 1U) {
        result->points[point_index].position_x += center_shift_x;
        result->points[point_index].position_y += center_shift_y;
    }
    result->has_manual_center_shift = false;
    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    free_stitch_design(&temporary_chain_points);
    free(visited_cells);
    free(expanded_cells);
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

static bool insert_design_fragment(
    const StitchDesign *base_design,
    const StitchDesign *fragment,
    int32_t offset_x,
    int32_t offset_y,
    StitchDesign *result
) {
    bool succeeded = false;
    if (!clone_stitch_design(base_design, result)) {
        return false;
    }

    if (fragment->block_count == 0U || fragment->chain_count == 0U) {
        return true;
    }

    const size_t base_block_count = result->block_count;
    for (size_t block_index = 0U; block_index < fragment->block_count; block_index += 1U) {
        const ColorBlock block = fragment->blocks[block_index];
        if (!design_append_block(
                result,
                block.display_color,
                block.janome_color_index,
                block.enabled,
                nullptr
            )) {
            goto cleanup;
        }
    }

    for (size_t chain_index = 0U; chain_index < fragment->chain_count; chain_index += 1U) {
        const StitchChain chain = fragment->chains[chain_index];
        if (chain.color_block_index >= fragment->block_count ||
            !fragment->blocks[chain.color_block_index].enabled ||
            chain.point_count < 2U) {
            continue;
        }
        const size_t destination_point_offset = result->point_count;
        for (size_t local_index = 0U; local_index < chain.point_count; local_index += 1U) {
            const StitchPoint source_point = fragment->points[chain.point_offset + local_index];
            const int64_t placed_x = (int64_t)source_point.position_x + (int64_t)offset_x;
            const int64_t placed_y = (int64_t)source_point.position_y + (int64_t)offset_y;
            if (placed_x < -MAX_COORDINATE_MAGNITUDE || placed_x > MAX_COORDINATE_MAGNITUDE ||
                placed_y < -MAX_COORDINATE_MAGNITUDE || placed_y > MAX_COORDINATE_MAGNITUDE) {
                goto cleanup;
            }
            if (!design_append_point(result, (int32_t)placed_x, (int32_t)placed_y)) {
                goto cleanup;
            }
        }
        if (!design_append_chain(
                result,
                destination_point_offset,
                chain.point_count,
                base_block_count + chain.color_block_index,
                chain.start_was_cut,
                chain.end_was_cut
            )) {
            goto cleanup;
        }
    }

    if (!compact_non_empty_blocks(result)) {
        goto cleanup;
    }
    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

static void rotate_design_90_clockwise(StitchDesign *design) {
    for (size_t point_index = 0U; point_index < design->point_count; point_index += 1U) {
        const int32_t old_x = design->points[point_index].position_x;
        const int32_t old_y = design->points[point_index].position_y;
        design->points[point_index].position_x = -old_y;
        design->points[point_index].position_y = old_x;
    }
    recompute_design_bounds(design);
}

static void mirror_design_horizontal(StitchDesign *design) {
    for (size_t point_index = 0U; point_index < design->point_count; point_index += 1U) {
        design->points[point_index].position_x = -design->points[point_index].position_x;
    }
    recompute_design_bounds(design);
}

static void mirror_design_vertical(StitchDesign *design) {
    for (size_t point_index = 0U; point_index < design->point_count; point_index += 1U) {
        design->points[point_index].position_y = -design->points[point_index].position_y;
    }
    recompute_design_bounds(design);
}

static void shift_design_center(StitchDesign *design, int32_t delta_x, int32_t delta_y) {
    for (size_t point_index = 0U; point_index < design->point_count; point_index += 1U) {
        const int64_t next_x = (int64_t)design->points[point_index].position_x + (int64_t)delta_x;
        const int64_t next_y = (int64_t)design->points[point_index].position_y + (int64_t)delta_y;
        if (next_x < -MAX_COORDINATE_MAGNITUDE || next_x > MAX_COORDINATE_MAGNITUDE ||
            next_y < -MAX_COORDINATE_MAGNITUDE || next_y > MAX_COORDINATE_MAGNITUDE) {
            return;
        }
    }
    for (size_t point_index = 0U; point_index < design->point_count; point_index += 1U) {
        design->points[point_index].position_x += delta_x;
        design->points[point_index].position_y += delta_y;
    }
    design->has_manual_center_shift = true;
    recompute_design_bounds(design);
}

static bool byte_buffer_append(ByteBuffer *buffer, const uint8_t *bytes, size_t count) {
    size_t required_size = 0U;
    if (ckd_add(&required_size, buffer->byte_count, count)) {
        return false;
    }
    if (required_size > buffer->byte_capacity) {
        size_t next_capacity = buffer->byte_capacity;
        if (next_capacity == 0U) {
            next_capacity = 8192U;
        }
        while (next_capacity < required_size) {
            size_t doubled_capacity = 0U;
            if (ckd_mul(&doubled_capacity, next_capacity, (size_t)2U)) {
                next_capacity = required_size;
                break;
            }
            next_capacity = doubled_capacity;
        }
        uint8_t *grown_bytes = (uint8_t *)realloc(buffer->bytes, next_capacity);
        if (grown_bytes == nullptr) {
            fprintf(stderr, "Failed to grow JEF export buffer\n");
            return false;
        }
        buffer->bytes = grown_bytes;
        buffer->byte_capacity = next_capacity;
    }

    memcpy(buffer->bytes + buffer->byte_count, bytes, count);
    buffer->byte_count = required_size;
    return true;
}

static int32_t clamp_jef_step(int64_t delta) {
    if (delta > JEF_MAX_STEP) {
        return JEF_MAX_STEP;
    }
    if (delta < -JEF_MAX_STEP) {
        return -JEF_MAX_STEP;
    }
    return (int32_t)delta;
}

static bool emit_jef_jump_sequence(
    ByteBuffer *stream,
    int64_t *needle_x,
    int64_t *needle_y,
    int64_t target_x,
    int64_t target_y
) {
    int64_t remaining_x = target_x - *needle_x;
    int64_t remaining_y = target_y - *needle_y;

    if (remaining_x == 0LL && remaining_y == 0LL) {
        const uint8_t zero_jump[4] = {0x80U, 0x02U, 0x00U, 0x00U};
        return byte_buffer_append(stream, zero_jump, sizeof(zero_jump));
    }

    while (remaining_x != 0LL || remaining_y != 0LL) {
        const int32_t step_x = clamp_jef_step(remaining_x);
        const int32_t step_y = clamp_jef_step(remaining_y);
        const uint8_t jump_command[4] = {
            0x80U,
            0x02U,
            (uint8_t)(int8_t)step_x,
            (uint8_t)(int8_t)step_y
        };
        if (!byte_buffer_append(stream, jump_command, sizeof(jump_command))) {
            return false;
        }
        remaining_x -= (int64_t)step_x;
        remaining_y -= (int64_t)step_y;
        *needle_x += (int64_t)step_x;
        *needle_y += (int64_t)step_y;
    }
    return true;
}

static bool emit_jef_stitch_move(
    ByteBuffer *stream,
    int64_t *needle_x,
    int64_t *needle_y,
    int64_t target_x,
    int64_t target_y
) {
    const int64_t origin_x = *needle_x;
    const int64_t origin_y = *needle_y;
    const int64_t total_delta_x = target_x - origin_x;
    const int64_t total_delta_y = target_y - origin_y;
    if (total_delta_x == 0LL && total_delta_y == 0LL) {
        return true;
    }

    int64_t magnitude_x = total_delta_x;
    if (magnitude_x < 0LL) {
        magnitude_x = -magnitude_x;
    }
    int64_t magnitude_y = total_delta_y;
    if (magnitude_y < 0LL) {
        magnitude_y = -magnitude_y;
    }
    int64_t dominant_delta = magnitude_x;
    if (magnitude_y > dominant_delta) {
        dominant_delta = magnitude_y;
    }

    const int64_t step_count = (dominant_delta + JEF_MAX_STEP - 1) / JEF_MAX_STEP;
    for (int64_t step_index = 1LL; step_index <= step_count; step_index += 1LL) {
        const int64_t next_x = origin_x + (total_delta_x * step_index) / step_count;
        const int64_t next_y = origin_y + (total_delta_y * step_index) / step_count;
        const int32_t step_x = clamp_jef_step(next_x - *needle_x);
        const int32_t step_y = clamp_jef_step(next_y - *needle_y);
        const uint8_t stitch_bytes[2] = {
            (uint8_t)(int8_t)step_x,
            (uint8_t)(int8_t)step_y
        };
        if (!byte_buffer_append(stream, stitch_bytes, sizeof(stitch_bytes))) {
            return false;
        }
        *needle_x += (int64_t)step_x;
        *needle_y += (int64_t)step_y;
    }
    return true;
}

static bool emit_jef_chain(
    const StitchDesign *design,
    StitchChain chain,
    int64_t shift_x,
    int64_t shift_y,
    ByteBuffer *stitch_stream,
    int64_t *needle_x,
    int64_t *needle_y
) {
    const StitchPoint first_point = design->points[chain.point_offset];
    const int64_t start_jef_x = (int64_t)first_point.position_x + shift_x;
    const int64_t start_jef_y = -((int64_t)first_point.position_y + shift_y);

    if (!emit_jef_jump_sequence(stitch_stream, needle_x, needle_y, start_jef_x, start_jef_y)) {
        return false;
    }
    const uint8_t anchor_stitch[2] = {0x00U, 0x00U};
    if (!byte_buffer_append(stitch_stream, anchor_stitch, sizeof(anchor_stitch))) {
        return false;
    }

    for (size_t local_index = 1U; local_index < chain.point_count; local_index += 1U) {
        const StitchPoint point = design->points[chain.point_offset + local_index];
        const int64_t target_jef_x = (int64_t)point.position_x + shift_x;
        const int64_t target_jef_y = -((int64_t)point.position_y + shift_y);
        if (!emit_jef_stitch_move(stitch_stream, needle_x, needle_y, target_jef_x, target_jef_y)) {
            return false;
        }
    }
    return true;
}

static void write_jef_hoop_margin_group(
    uint8_t *header,
    size_t offset,
    int32_t half_width,
    int32_t half_height,
    int32_t extent_left,
    int32_t extent_top,
    int32_t extent_right,
    int32_t extent_bottom
) {
    if (extent_left <= half_width && extent_right <= half_width &&
        extent_top <= half_height && extent_bottom <= half_height) {
        write_int32_little_endian(header, offset, half_width - extent_left);
        write_int32_little_endian(header, offset + 4U, half_height - extent_top);
        write_int32_little_endian(header, offset + 8U, half_width - extent_right);
        write_int32_little_endian(header, offset + 12U, half_height - extent_bottom);
    } else {
        write_int32_little_endian(header, offset, -1);
        write_int32_little_endian(header, offset + 4U, -1);
        write_int32_little_endian(header, offset + 8U, -1);
        write_int32_little_endian(header, offset + 12U, -1);
    }
}

/* Select Janome Memory Craft 550E hoop code from design extents in 0.1 mm. */
static uint32_t select_janome_550e_hoop_code(
    int32_t extent_left,
    int32_t extent_top,
    int32_t extent_right,
    int32_t extent_bottom
) {
    if (extent_left <= 250 && extent_right <= 250 && extent_top <= 250 && extent_bottom <= 250) {
        return 1U;
    }
    if (extent_left <= 550 && extent_right <= 550 && extent_top <= 550 && extent_bottom <= 550) {
        return 0U;
    }
    if (extent_left <= 700 && extent_right <= 700 && extent_top <= 700 && extent_bottom <= 700) {
        return 3U;
    }
    if (extent_left <= 700 && extent_right <= 700 && extent_top <= 1000 && extent_bottom <= 1000) {
        return 2U;
    }
    if (extent_left <= 1000 && extent_right <= 1000 && extent_top <= 1000 && extent_bottom <= 1000) {
        return 4U;
    }
    /* Extended Janome MC500E/550E hoop code for RE28b (200x280) and RE36b (200x360). */
    return 38U;
}

static bool write_jef_file(const StitchDesign *design, const char *output_path) {
    bool succeeded = false;
    FILE *output_file = nullptr;
    uint8_t *header_buffer = nullptr;
    size_t *active_blocks = nullptr;
    ByteBuffer stitch_stream = {};

    if (design->block_count == 0U || design->chain_count == 0U) {
        fprintf(stderr, "Cannot export empty embroidery design\n");
        goto cleanup;
    }

    size_t active_block_bytes = 0U;
    if (ckd_mul(&active_block_bytes, design->block_count, sizeof(size_t))) {
        goto cleanup;
    }
    active_blocks = (size_t *)malloc(active_block_bytes);
    if (active_blocks == nullptr) {
        fprintf(stderr, "Failed to allocate active block list\n");
        goto cleanup;
    }

    size_t exported_block_count = 0U;
    bool has_export_bounds = false;
    int64_t screen_min_x = 0LL;
    int64_t screen_max_x = 0LL;
    int64_t screen_min_y = 0LL;
    int64_t screen_max_y = 0LL;
    size_t previous_seen_block = SIZE_MAX;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count) {
            continue;
        }
        if (!design->blocks[chain.color_block_index].enabled || chain.point_count < 2U) {
            continue;
        }
        expand_bounds_from_chain(
            design,
            chain,
            &has_export_bounds,
            &screen_min_x,
            &screen_max_x,
            &screen_min_y,
            &screen_max_y
        );
        if (chain.color_block_index != previous_seen_block) {
            active_blocks[exported_block_count] = chain.color_block_index;
            exported_block_count += 1U;
            previous_seen_block = chain.color_block_index;
        }
    }

    if (!has_export_bounds || exported_block_count == 0U ||
        exported_block_count > MAX_COLOR_BLOCKS) {
        fprintf(stderr, "No drawable stitches to export\n");
        goto cleanup;
    }

    int64_t shift_x = 0LL;
    int64_t shift_y = 0LL;
    if (!design->has_manual_center_shift) {
        shift_x = -((screen_min_x + screen_max_x) / 2LL);
        shift_y = -((screen_min_y + screen_max_y) / 2LL);
    }

    const int64_t jef_min_x = screen_min_x + shift_x;
    const int64_t jef_max_x = screen_max_x + shift_x;
    const int64_t jef_min_y = -(screen_max_y + shift_y);
    const int64_t jef_max_y = -(screen_min_y + shift_y);

    int32_t extent_left = 0;
    if (jef_min_x < 0LL) {
        extent_left = (int32_t)(-jef_min_x);
    }
    int32_t extent_right = 0;
    if (jef_max_x > 0LL) {
        extent_right = (int32_t)jef_max_x;
    }
    int32_t extent_bottom = 0;
    if (jef_min_y < 0LL) {
        extent_bottom = (int32_t)(-jef_min_y);
    }
    int32_t extent_top = 0;
    if (jef_max_y > 0LL) {
        extent_top = (int32_t)jef_max_y;
    }

    const uint32_t hoop_code =
        select_janome_550e_hoop_code(extent_left, extent_top, extent_right, extent_bottom);

    int64_t needle_x = 0LL;
    int64_t needle_y = 0LL;
    size_t emitted_block_index = SIZE_MAX;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count) {
            continue;
        }
        if (!design->blocks[chain.color_block_index].enabled || chain.point_count < 2U) {
            continue;
        }
        if (emitted_block_index != SIZE_MAX && chain.color_block_index != emitted_block_index) {
            const uint8_t color_change_command[4] = {0x80U, 0x01U, 0x00U, 0x00U};
            if (!byte_buffer_append(
                    &stitch_stream,
                    color_change_command,
                    sizeof(color_change_command)
                )) {
                goto cleanup;
            }
        }
        emitted_block_index = chain.color_block_index;

        if (!emit_jef_chain(
                design,
                chain,
                shift_x,
                shift_y,
                &stitch_stream,
                &needle_x,
                &needle_y
            )) {
            goto cleanup;
        }
    }

    const uint8_t end_command[2] = {0x80U, 0x10U};
    if (!byte_buffer_append(&stitch_stream, end_command, sizeof(end_command))) {
        goto cleanup;
    }
    if (stitch_stream.byte_count / 2U > (size_t)UINT32_MAX) {
        fprintf(stderr, "Exported JEF stitch stream exceeds format limit\n");
        goto cleanup;
    }

    size_t color_table_bytes = 0U;
    size_t header_size = 0U;
    if (ckd_mul(&color_table_bytes, exported_block_count, (size_t)8U) ||
        ckd_add(&header_size, JEF_HEADER_SIZE, color_table_bytes)) {
        fprintf(stderr, "JEF header size overflow\n");
        goto cleanup;
    }
    header_buffer = (uint8_t *)calloc(1U, header_size);
    if (header_buffer == nullptr) {
        fprintf(stderr, "Failed to allocate JEF header buffer\n");
        goto cleanup;
    }

    write_uint32_little_endian(header_buffer, 0U, (uint32_t)header_size);
    write_uint32_little_endian(header_buffer, 4U, 10U);

    char date_string[16] = "20260101000000";
    const time_t current_time = time(nullptr);
    const struct tm *local_time = localtime(&current_time);
    if (local_time != nullptr) {
        const int written_characters = snprintf(
            date_string,
            sizeof(date_string),
            "%04d%02d%02d%02d%02d%02d",
            local_time->tm_year + 1900,
            local_time->tm_mon + 1,
            local_time->tm_mday,
            local_time->tm_hour,
            local_time->tm_min,
            local_time->tm_sec
        );
        if (written_characters < 0 || (size_t)written_characters >= sizeof(date_string)) {
            memcpy(date_string, "20260101000000", 15U);
        }
    }
    memcpy(header_buffer + 8U, date_string, 14U);

    write_uint32_little_endian(header_buffer, 24U, (uint32_t)exported_block_count);
    write_uint32_little_endian(header_buffer, 28U, (uint32_t)(stitch_stream.byte_count / 2U));
    write_uint32_little_endian(header_buffer, 32U, hoop_code);

    write_int32_little_endian(header_buffer, 36U, extent_left);
    write_int32_little_endian(header_buffer, 40U, extent_top);
    write_int32_little_endian(header_buffer, 44U, extent_right);
    write_int32_little_endian(header_buffer, 48U, extent_bottom);

    write_jef_hoop_margin_group(
        header_buffer, 52U, 550, 550, extent_left, extent_top, extent_right, extent_bottom
    );
    write_jef_hoop_margin_group(
        header_buffer, 68U, 250, 250, extent_left, extent_top, extent_right, extent_bottom
    );
    write_jef_hoop_margin_group(
        header_buffer, 84U, 700, 1000, extent_left, extent_top, extent_right, extent_bottom
    );
    write_jef_hoop_margin_group(
        header_buffer, 100U, 1000, 1800, extent_left, extent_top, extent_right, extent_bottom
    );

    for (size_t order_index = 0U; order_index < exported_block_count; order_index += 1U) {
        const size_t block_index = active_blocks[order_index];
        uint32_t janome_index = (uint32_t)design->blocks[block_index].janome_color_index;
        if (janome_index == 0U || janome_index >= JANOME_PALETTE_COUNT) {
            janome_index = 1U;
        }
        write_uint32_little_endian(
            header_buffer,
            JEF_HEADER_SIZE + order_index * 4U,
            janome_index
        );
        write_uint32_little_endian(
            header_buffer,
            JEF_HEADER_SIZE + (exported_block_count + order_index) * 4U,
            13U
        );
    }

    output_file = fopen(output_path, "wb");
    if (output_file == nullptr) {
        fprintf(stderr, "Failed to open JEF output file: %s\n", strerror(errno));
        goto cleanup;
    }

    if (fwrite(header_buffer, 1U, header_size, output_file) != header_size) {
        fprintf(stderr, "Failed to write JEF header\n");
        goto cleanup;
    }
    if (fwrite(stitch_stream.bytes, 1U, stitch_stream.byte_count, output_file) !=
        stitch_stream.byte_count) {
        fprintf(stderr, "Failed to write JEF stitch data\n");
        goto cleanup;
    }

    if (fclose(output_file) != 0) {
        output_file = nullptr;
        fprintf(stderr, "Failed to close JEF output file\n");
        goto cleanup;
    }
    output_file = nullptr;
    succeeded = true;

cleanup:
    if (output_file != nullptr) {
        fclose(output_file);
    }
    free(header_buffer);
    free(active_blocks);
    free(stitch_stream.bytes);
    return succeeded;
}

static bool build_suffixed_jef_path(
    const char *base_path,
    const char *suffix,
    char *output_path,
    size_t output_capacity
) {
    const char *last_slash = strrchr(base_path, '/');
    const char *last_dot = strrchr(base_path, '.');
    size_t stem_length = strlen(base_path);
    if (last_dot != nullptr && (last_slash == nullptr || last_dot > last_slash)) {
        stem_length = (size_t)(last_dot - base_path);
    }

    const int written = snprintf(
        output_path,
        output_capacity,
        "%.*s%s.jef",
        (int)stem_length,
        base_path,
        suffix
    );
    if (written < 0 || (size_t)written >= output_capacity) {
        fprintf(stderr, "Output path exceeds maximum length\n");
        return false;
    }
    return true;
}

static bool file_already_exists(const char *file_path) {
    FILE *probe_file = fopen(file_path, "rb");
    if (probe_file != nullptr) {
        fclose(probe_file);
        return true;
    }
    return false;
}

static bool build_unique_suffixed_jef_path(
    const char *base_path,
    const char *suffix,
    char *output_path,
    size_t output_capacity
) {
    if (!build_suffixed_jef_path(base_path, suffix, output_path, output_capacity)) {
        return false;
    }
    if (!file_already_exists(output_path)) {
        return true;
    }

    for (unsigned int sequence_number = 2U; sequence_number <= 999U; sequence_number += 1U) {
        char numbered_suffix[64];
        const int suffix_written = snprintf(
            numbered_suffix,
            sizeof(numbered_suffix),
            "%s_%u",
            suffix,
            sequence_number
        );
        if (suffix_written < 0 || (size_t)suffix_written >= sizeof(numbered_suffix)) {
            return false;
        }
        if (!build_suffixed_jef_path(base_path, numbered_suffix, output_path, output_capacity)) {
            return false;
        }
        if (!file_already_exists(output_path)) {
            return true;
        }
    }
    return false;
}

#endif
