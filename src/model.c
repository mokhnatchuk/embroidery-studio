#include "model.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void log_model_error(const char *message) {
#ifndef EMBROIDERY_QUIET_ERRORS
    (void)fprintf(stderr, "%s\n", message);
#else
    (void)message;
#endif
}

/* Standard Janome JEF thread palette for indices 0..78. */
const RgbColor janome_palette[JANOME_PALETTE_COUNT] = {
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
const RgbColor brother_palette[BROTHER_PALETTE_COUNT] = {
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
const RgbColor dst_default_palette[DST_DEFAULT_PALETTE_COUNT] = {
    {0x1EU, 0x1EU, 0x1EU}, {0xC8U, 0x28U, 0x28U}, {0x1EU, 0x5AU, 0xAAU},
    {0xF0U, 0xB4U, 0x1EU}, {0x28U, 0x8CU, 0x46U}, {0x82U, 0x3CU, 0x96U},
    {0xE6U, 0x78U, 0x1EU}, {0x5AU, 0x5AU, 0x5AU}
};

uint32_t read_uint32_little_endian(const uint8_t *bytes, size_t offset) {
    const uint32_t byte_zero = (uint32_t)bytes[offset];
    const uint32_t byte_one = (uint32_t)bytes[offset + 1U] << 8U;
    const uint32_t byte_two = (uint32_t)bytes[offset + 2U] << 16U;
    const uint32_t byte_three = (uint32_t)bytes[offset + 3U] << 24U;
    return byte_zero | byte_one | byte_two | byte_three;
}

void write_uint32_little_endian(uint8_t *destination, size_t offset, uint32_t value) {
    destination[offset] = (uint8_t)(value & 0xFFU);
    destination[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
    destination[offset + 2U] = (uint8_t)((value >> 16U) & 0xFFU);
    destination[offset + 3U] = (uint8_t)((value >> 24U) & 0xFFU);
}

void write_int32_little_endian(uint8_t *destination, size_t offset, int32_t value) {
    write_uint32_little_endian(destination, offset, (uint32_t)value);
}

unsigned char ascii_lowercase(unsigned char character) {
    if (character >= 'A' && character <= 'Z') {
        return (unsigned char)(character - 'A' + 'a');
    }
    return character;
}

bool strings_equal_ignore_case(const char *left_text, const char *right_text) {
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

int32_t floor_divide_int32(int32_t dividend, int32_t divisor) {
    int32_t quotient = dividend / divisor;
    const int32_t remainder = dividend % divisor;
    if (remainder != 0 && ((dividend < 0) != (divisor < 0))) {
        quotient -= 1;
    }
    return quotient;
}

int32_t positive_modulo_int32(int32_t value, int32_t modulus) {
    int32_t remainder = value % modulus;
    if (remainder < 0) {
        remainder += modulus;
    }
    return remainder;
}

uint8_t find_nearest_janome_color(RgbColor target_color) {
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

void init_stitch_design(StitchDesign *design) {
    *design = (StitchDesign){};
}

void free_stitch_design(StitchDesign *design) {
    free(design->blocks);
    free(design->chains);
    free(design->points);
    *design = (StitchDesign){};
}

bool design_reserve_points(StitchDesign *design, size_t required_count) {
    if (required_count <= design->point_capacity) {
        return true;
    }
    if (required_count > MAX_DESIGN_POINTS) {
        log_model_error("Point count exceeds safety limit");
        return false;
    }

    size_t next_capacity = design->point_capacity;
    if (next_capacity == 0U) {
        next_capacity = 4096U;
    }
    while (next_capacity < required_count) {
        size_t doubled_capacity = 0U;
        if (ckd_mul(&doubled_capacity, next_capacity, (size_t)2U) ||
            doubled_capacity > MAX_DESIGN_POINTS) {
            next_capacity = MAX_DESIGN_POINTS;
            break;
        }
        next_capacity = doubled_capacity;
    }

    size_t allocation_bytes = 0U;
    if (ckd_mul(&allocation_bytes, next_capacity, sizeof(StitchPoint))) {
        return false;
    }

    StitchPoint *grown_points = (StitchPoint *)realloc(design->points, allocation_bytes);
    if (grown_points == nullptr) {
        log_model_error("Failed to allocate stitch point buffer");
        return false;
    }
    design->points = grown_points;
    design->point_capacity = next_capacity;
    return true;
}

bool design_reserve_chains(StitchDesign *design, size_t required_count) {
    if (required_count <= design->chain_capacity) {
        return true;
    }
    if (required_count > MAX_DESIGN_CHAINS) {
        log_model_error("Chain count exceeds safety limit");
        return false;
    }

    size_t next_capacity = design->chain_capacity;
    if (next_capacity == 0U) {
        next_capacity = 256U;
    }
    while (next_capacity < required_count) {
        size_t doubled_capacity = 0U;
        if (ckd_mul(&doubled_capacity, next_capacity, (size_t)2U) ||
            doubled_capacity > MAX_DESIGN_CHAINS) {
            next_capacity = MAX_DESIGN_CHAINS;
            break;
        }
        next_capacity = doubled_capacity;
    }

    size_t allocation_bytes = 0U;
    if (ckd_mul(&allocation_bytes, next_capacity, sizeof(StitchChain))) {
        return false;
    }

    StitchChain *grown_chains = (StitchChain *)realloc(design->chains, allocation_bytes);
    if (grown_chains == nullptr) {
        log_model_error("Failed to allocate stitch chain buffer");
        return false;
    }
    design->chains = grown_chains;
    design->chain_capacity = next_capacity;
    return true;
}

bool design_reserve_blocks(StitchDesign *design, size_t required_count) {
    if (required_count <= design->block_capacity) {
        return true;
    }
    if (required_count > MAX_COLOR_BLOCKS) {
        log_model_error("Color block count exceeds safety limit");
        return false;
    }

    size_t next_capacity = design->block_capacity;
    if (next_capacity == 0U) {
        next_capacity = 16U;
    }
    while (next_capacity < required_count) {
        size_t doubled_capacity = 0U;
        if (ckd_mul(&doubled_capacity, next_capacity, (size_t)2U) ||
            doubled_capacity > MAX_COLOR_BLOCKS) {
            next_capacity = MAX_COLOR_BLOCKS;
            break;
        }
        next_capacity = doubled_capacity;
    }

    size_t allocation_bytes = 0U;
    if (ckd_mul(&allocation_bytes, next_capacity, sizeof(ColorBlock))) {
        return false;
    }

    ColorBlock *grown_blocks = (ColorBlock *)realloc(design->blocks, allocation_bytes);
    if (grown_blocks == nullptr) {
        log_model_error("Failed to allocate color block buffer");
        return false;
    }
    design->blocks = grown_blocks;
    design->block_capacity = next_capacity;
    return true;
}

bool design_append_point(StitchDesign *design, int32_t position_x, int32_t position_y) {
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

bool design_append_chain(
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

bool design_append_block(
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

void expand_bounds_from_chain(
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

void recompute_design_bounds(StitchDesign *design) {
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

void center_stitch_design(StitchDesign *design) {
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

bool clone_stitch_design(const StitchDesign *source, StitchDesign *destination) {
    init_stitch_design(destination);

    if (source->block_count > 0U) {
        size_t block_bytes = 0U;
        if (ckd_mul(&block_bytes, source->block_count, sizeof(ColorBlock)) ||
            !design_reserve_blocks(destination, source->block_count)) {
            goto cleanup;
        }
        memcpy(destination->blocks, source->blocks, block_bytes);
        destination->block_count = source->block_count;
    }
    if (source->chain_count > 0U) {
        size_t chain_bytes = 0U;
        if (ckd_mul(&chain_bytes, source->chain_count, sizeof(StitchChain)) ||
            !design_reserve_chains(destination, source->chain_count)) {
            goto cleanup;
        }
        memcpy(destination->chains, source->chains, chain_bytes);
        destination->chain_count = source->chain_count;
    }
    if (source->point_count > 0U) {
        size_t point_bytes = 0U;
        if (ckd_mul(&point_bytes, source->point_count, sizeof(StitchPoint)) ||
            !design_reserve_points(destination, source->point_count)) {
            goto cleanup;
        }
        memcpy(destination->points, source->points, point_bytes);
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
bool sort_chains_by_color_block(StitchDesign *design) {
    bool succeeded = false;
    size_t *block_counts = nullptr;
    size_t *block_write_offsets = nullptr;
    StitchChain *sorted_chains = nullptr;

    if (design->block_count == 0U || design->chain_count < 2U) {
        return true;
    }

    size_t count_bytes = 0U;
    size_t chain_bytes = 0U;
    if (ckd_mul(&count_bytes, design->block_count, sizeof(size_t)) ||
        ckd_mul(&chain_bytes, design->chain_count, sizeof(StitchChain))) {
        goto cleanup;
    }

    block_counts = (size_t *)calloc(design->block_count, sizeof(size_t));
    block_write_offsets = (size_t *)calloc(design->block_count, sizeof(size_t));
    sorted_chains = (StitchChain *)malloc(chain_bytes);
    if (block_counts == nullptr || block_write_offsets == nullptr || sorted_chains == nullptr) {
        log_model_error("Failed to allocate chain sort buffers");
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

    if (running_offset > 0U) {
        size_t copy_bytes = 0U;
        if (ckd_mul(&copy_bytes, running_offset, sizeof(StitchChain))) {
            goto cleanup;
        }
        memcpy(design->chains, sorted_chains, copy_bytes);
    }
    design->chain_count = running_offset;
    succeeded = true;

cleanup:
    free(sorted_chains);
    free(block_write_offsets);
    free(block_counts);
    return succeeded;
}

/* Strip orphaned stitch points left behind after chain or block removal. */
bool compact_design_points(StitchDesign *design) {
    bool succeeded = false;
    StitchPoint *compacted_points = nullptr;

    size_t total_live_points = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U ||
            chain.point_offset > design->point_count ||
            chain.point_count > design->point_count - chain.point_offset) {
            continue;
        }
        if (ckd_add(&total_live_points, total_live_points, chain.point_count)) {
            goto cleanup;
        }
    }

    if (total_live_points == 0U) {
        design->point_count = 0U;
        succeeded = true;
        goto cleanup;
    }

    if (total_live_points == design->point_count) {
        succeeded = true;
        goto cleanup;
    }

    size_t allocation_bytes = 0U;
    if (ckd_mul(&allocation_bytes, total_live_points, sizeof(StitchPoint))) {
        goto cleanup;
    }

    compacted_points = (StitchPoint *)malloc(allocation_bytes);
    if (compacted_points == nullptr) {
        log_model_error("Failed to allocate compacted point buffer");
        goto cleanup;
    }

    size_t write_offset = 0U;
    size_t kept_chains = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U ||
            chain.point_offset > design->point_count ||
            chain.point_count > design->point_count - chain.point_offset) {
            continue;
        }
        size_t copy_bytes = 0U;
        if (ckd_mul(&copy_bytes, chain.point_count, sizeof(StitchPoint))) {
            goto cleanup;
        }
        memcpy(compacted_points + write_offset, design->points + chain.point_offset, copy_bytes);
        chain.point_offset = write_offset;
        write_offset += chain.point_count;
        design->chains[kept_chains] = chain;
        kept_chains += 1U;
    }

    free(design->points);
    design->points = compacted_points;
    compacted_points = nullptr;
    design->point_count = write_offset;
    design->point_capacity = total_live_points;
    design->chain_count = kept_chains;
    succeeded = true;

cleanup:
    free(compacted_points);
    return succeeded;
}

bool compact_non_empty_blocks(StitchDesign *design) {
    bool succeeded = false;
    size_t *index_remap = nullptr;

    if (design->block_count == 0U) {
        design->chain_count = 0U;
        design->point_count = 0U;
        return true;
    }

    const size_t original_block_count = design->block_count;
    size_t remap_bytes = 0U;
    if (ckd_mul(&remap_bytes, original_block_count, sizeof(size_t))) {
        goto cleanup;
    }

    index_remap = (size_t *)malloc(remap_bytes);
    if (index_remap == nullptr) {
        log_model_error("Failed to allocate color remap table");
        goto cleanup;
    }

    for (size_t block_index = 0U; block_index < original_block_count; block_index += 1U) {
        index_remap[block_index] = SIZE_MAX;
    }

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index < original_block_count && chain.point_count >= 2U) {
            index_remap[chain.color_block_index] = 1U;
        }
    }

    size_t compacted_count = 0U;
    for (size_t block_index = 0U; block_index < original_block_count; block_index += 1U) {
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
        if (chain.point_count >= 2U &&
            chain.color_block_index < original_block_count &&
            index_remap[chain.color_block_index] != SIZE_MAX) {
            design->chains[kept_chains] = chain;
            design->chains[kept_chains].color_block_index = index_remap[chain.color_block_index];
            kept_chains += 1U;
        }
    }
    design->chain_count = kept_chains;

    if (!sort_chains_by_color_block(design)) {
        goto cleanup;
    }
    if (!compact_design_points(design)) {
        goto cleanup;
    }
    succeeded = true;

cleanup:
    free(index_remap);
    return succeeded;
}

DesignStats compute_design_stats(const StitchDesign *design) {
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

size_t count_design_segments(const StitchDesign *design) {
    size_t total_segments = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        if (design->chains[chain_index].point_count >= 2U) {
            total_segments += design->chains[chain_index].point_count - 1U;
        }
    }
    return total_segments;
}

bool point_is_inside_rectangle(StitchPoint point, CutRectangle rectangle) {
    return point.position_x >= rectangle.minimum_x &&
           point.position_x <= rectangle.maximum_x &&
           point.position_y >= rectangle.minimum_y &&
           point.position_y <= rectangle.maximum_y;
}

bool point_is_strictly_inside_rectangle(
    int32_t point_x,
    int32_t point_y,
    CutRectangle rectangle
) {
    return point_x > rectangle.minimum_x &&
           point_x < rectangle.maximum_x &&
           point_y > rectangle.minimum_y &&
           point_y < rectangle.maximum_y;
}
