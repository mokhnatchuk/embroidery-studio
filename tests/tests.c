#define _POSIX_C_SOURCE 200809L

#include "formats.h"
#include "model.h"
#include "ops.h"
#include "render.h"
#include "ui.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool assert_condition(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "TEST FAILED: %s\n", message);
        return false;
    }
    return true;
}

static bool write_binary_file(const char *file_path, const uint8_t *data, size_t data_size) {
    bool succeeded = false;
    FILE *output_file = fopen(file_path, "wb");
    if (output_file == nullptr) {
        fprintf(stderr, "TEST FAILED: cannot open %s for writing\n", file_path);
        goto cleanup;
    }
    if (data_size > 0U && fwrite(data, 1U, data_size, output_file) != data_size) {
        fprintf(stderr, "TEST FAILED: short write on %s\n", file_path);
        goto cleanup;
    }
    if (fclose(output_file) != 0) {
        output_file = nullptr;
        fprintf(stderr, "TEST FAILED: failed to close %s\n", file_path);
        goto cleanup;
    }
    output_file = nullptr;
    succeeded = true;

cleanup:
    if (output_file != nullptr) {
        fclose(output_file);
    }
    return succeeded;
}

static bool read_binary_file(const char *file_path, uint8_t **buffer_out, size_t *size_out) {
    bool succeeded = false;
    FILE *input_file = fopen(file_path, "rb");
    uint8_t *buffer = nullptr;

    *buffer_out = nullptr;
    *size_out = 0U;

    if (input_file == nullptr) {
        goto cleanup;
    }
    if (fseek(input_file, 0L, SEEK_END) != 0) {
        goto cleanup;
    }
    const long length_long = ftell(input_file);
    if (length_long <= 0L) {
        goto cleanup;
    }
    if (fseek(input_file, 0L, SEEK_SET) != 0) {
        goto cleanup;
    }

    const size_t file_size = (size_t)length_long;
    buffer = (uint8_t *)malloc(file_size);
    if (buffer == nullptr) {
        goto cleanup;
    }
    if (fread(buffer, 1U, file_size, input_file) != file_size) {
        goto cleanup;
    }

    *buffer_out = buffer;
    *size_out = file_size;
    buffer = nullptr;
    succeeded = true;

cleanup:
    free(buffer);
    if (input_file != nullptr) {
        fclose(input_file);
    }
    return succeeded;
}

static bool reload_and_verify_jef(
    const char *file_path,
    size_t minimum_stitches,
    size_t minimum_colors
) {
    StitchDesign reloaded = {};
    if (!load_embroidery_file(file_path, &reloaded)) {
        fprintf(stderr, "TEST FAILED: could not reload exported JEF %s\n", file_path);
        return false;
    }
    const DesignStats stats = compute_design_stats(&reloaded);
    const bool valid =
        reloaded.has_bounds &&
        stats.stitch_count >= minimum_stitches &&
        stats.color_count >= minimum_colors;
    if (!valid) {
        fprintf(
            stderr,
            "TEST FAILED: reloaded JEF %s invalid (stitches=%zu colors=%zu)\n",
            file_path,
            stats.stitch_count,
            stats.color_count
        );
    }
    free_stitch_design(&reloaded);
    return valid;
}

static size_t count_segments_with_midpoint_in_box(
    const StitchDesign *design,
    CutRectangle query_box,
    bool strict_bounds
) {
    size_t count = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled ||
            chain.point_count < 2U) {
            continue;
        }
        const StitchPoint *points = &design->points[chain.point_offset];
        for (size_t local_index = 0U; local_index + 1U < chain.point_count; local_index += 1U) {
            const int32_t midpoint_x =
                (int32_t)(((int64_t)points[local_index].position_x +
                           (int64_t)points[local_index + 1U].position_x) / 2LL);
            const int32_t midpoint_y =
                (int32_t)(((int64_t)points[local_index].position_y +
                           (int64_t)points[local_index + 1U].position_y) / 2LL);
            if (strict_bounds) {
                if (point_is_strictly_inside_rectangle(midpoint_x, midpoint_y, query_box)) {
                    count += 1U;
                }
            } else {
                const StitchPoint midpoint = {midpoint_x, midpoint_y};
                if (point_is_inside_rectangle(midpoint, query_box)) {
                    count += 1U;
                }
            }
        }
    }
    return count;
}

static void encode_dst_triplet(
    int32_t delta_x,
    int32_t delta_y,
    uint8_t control_bits,
    uint8_t triplet_out[3]
) {
    /* DST stores +Y upward, while our model uses +Y downward. */
    int32_t remaining_x = delta_x;
    int32_t remaining_y = -delta_y;

    uint8_t byte_zero = 0U;
    uint8_t byte_one = 0U;
    uint8_t byte_two = (uint8_t)(control_bits | 0x03U);

    if (remaining_x >= 41) {
        byte_two |= 0x04U;
        remaining_x -= 81;
    } else if (remaining_x <= -41) {
        byte_two |= 0x08U;
        remaining_x += 81;
    }
    if (remaining_x >= 14) {
        byte_one |= 0x04U;
        remaining_x -= 27;
    } else if (remaining_x <= -14) {
        byte_one |= 0x08U;
        remaining_x += 27;
    }
    if (remaining_x >= 5) {
        byte_zero |= 0x04U;
        remaining_x -= 9;
    } else if (remaining_x <= -5) {
        byte_zero |= 0x08U;
        remaining_x += 9;
    }
    if (remaining_x >= 2) {
        byte_one |= 0x01U;
        remaining_x -= 3;
    } else if (remaining_x <= -2) {
        byte_one |= 0x02U;
        remaining_x += 3;
    }
    if (remaining_x >= 1) {
        byte_zero |= 0x01U;
    } else if (remaining_x <= -1) {
        byte_zero |= 0x02U;
    }

    if (remaining_y >= 41) {
        byte_two |= 0x20U;
        remaining_y -= 81;
    } else if (remaining_y <= -41) {
        byte_two |= 0x10U;
        remaining_y += 81;
    }
    if (remaining_y >= 14) {
        byte_one |= 0x20U;
        remaining_y -= 27;
    } else if (remaining_y <= -14) {
        byte_one |= 0x10U;
        remaining_y += 27;
    }
    if (remaining_y >= 5) {
        byte_zero |= 0x20U;
        remaining_y -= 9;
    } else if (remaining_y <= -5) {
        byte_zero |= 0x10U;
        remaining_y += 9;
    }
    if (remaining_y >= 2) {
        byte_one |= 0x80U;
        remaining_y -= 3;
    } else if (remaining_y <= -2) {
        byte_one |= 0x40U;
        remaining_y += 3;
    }
    if (remaining_y >= 1) {
        byte_zero |= 0x80U;
    } else if (remaining_y <= -1) {
        byte_zero |= 0x40U;
    }

    triplet_out[0] = byte_zero;
    triplet_out[1] = byte_one;
    triplet_out[2] = byte_two;
}

static size_t build_synthetic_pec_payload(uint8_t *buffer, size_t capacity) {
    if (capacity < PEC_HEADER_SIZE + 64U) {
        return 0U;
    }
    memset(buffer, 0, PEC_HEADER_SIZE);
    memcpy(buffer, "LA:SYNTHETIC    ", 16U);
    buffer[48] = 1U; /* 2 colors */
    buffer[49] = 5U; /* Brother red */
    buffer[50] = 20U; /* Brother black */

    size_t cursor = PEC_HEADER_SIZE;
    static const uint8_t stitch_stream[] = {
        /* First color block: 1-byte deltas forming a cross */
        0x00U, 0x00U,
        18U, 18U,
        0U, (uint8_t)(128U - 18U),
        (uint8_t)(128U - 18U), 18U,
        /* 2-byte jump (+36, 0) using jump flag 0x10 */
        0x90U, 0x24U, 0x80U, 0x00U,
        /* Color change */
        0xFEU, 0xB0U, 0x01U,
        /* Second color block: 1-byte and 2-byte signed stitches */
        0x00U, 0x00U,
        18U, 18U,
        0U, (uint8_t)(128U - 18U),
        0x8FU, 0xEEU, 0x80U, 0x12U,
        /* End of PEC */
        0xFFU
    };
    memcpy(buffer + cursor, stitch_stream, sizeof(stitch_stream));
    cursor += sizeof(stitch_stream);
    return cursor;
}

static bool write_synthetic_pes_and_pec(const char *pes_path, const char *pec_path) {
    uint8_t pec_payload[PEC_HEADER_SIZE + 64U] = {};
    const size_t pec_length = build_synthetic_pec_payload(pec_payload, sizeof(pec_payload));
    if (pec_length == 0U) {
        return false;
    }

    uint8_t pec_file[8U + sizeof(pec_payload)] = {};
    memcpy(pec_file, "#PEC0001", 8U);
    memcpy(pec_file + 8U, pec_payload, pec_length);
    if (!write_binary_file(pec_path, pec_file, 8U + pec_length)) {
        return false;
    }

    uint8_t pes_file[16U + sizeof(pec_payload)] = {};
    memcpy(pes_file, "#PES0060", 8U);
    pes_file[8] = 16U;
    pes_file[9] = 0U;
    pes_file[10] = 0U;
    pes_file[11] = 0U;
    memcpy(pes_file + 16U, pec_payload, pec_length);
    return write_binary_file(pes_path, pes_file, 16U + pec_length);
}

static bool write_synthetic_dst(const char *dst_path) {
    uint8_t dst_file[DST_HEADER_SIZE + 36U] = {};
    memset(dst_file, ' ', DST_HEADER_SIZE);
    memcpy(dst_file, "LA:SYNTHETIC_DST", 16U);
    dst_file[DST_HEADER_SIZE - 1U] = 0x1AU;

    size_t cursor = DST_HEADER_SIZE;
    const int32_t deltas[][3] = {
        {0, 0, 0x00},
        {18, 18, 0x00},
        {0, -18, 0x00},
        {-18, 18, 0x00},
        {36, 0, 0x80},
        {0, 0, 0xC0},
        {0, 0, 0x00},
        {18, 18, 0x00},
        {0, -18, 0x00},
        {-18, 18, 0x00}
    };
    const size_t step_count = sizeof(deltas) / sizeof(deltas[0]);
    for (size_t step_index = 0U; step_index < step_count; step_index += 1U) {
        encode_dst_triplet(
            deltas[step_index][0],
            deltas[step_index][1],
            (uint8_t)deltas[step_index][2],
            dst_file + cursor
        );
        cursor += 3U;
    }
    dst_file[cursor] = 0x00U;
    dst_file[cursor + 1U] = 0x00U;
    dst_file[cursor + 2U] = 0xF3U;
    cursor += 3U;

    return write_binary_file(dst_path, dst_file, cursor);
}

static bool build_synthetic_cross_grid_design(StitchDesign *design_out) {
    /* 10 columns x 14 rows of 5x crosses at 1.8 mm pitch, 2 alternating colors. */
    StripPattern pattern = {};
    pattern.width = 10;
    pattern.height = 14;
    pattern.repeat_x = 1;
    pattern.repeat_y = 1;
    pattern.pitch_units = 18;
    pattern.passes = 5;

    for (int32_t row = 0; row < pattern.height; row += 1) {
        for (int32_t column = 0; column < pattern.width; column += 1) {
            uint8_t color_index = 10U;
            if ((column + row) % 3 == 0) {
                color_index = 1U;
            } else if ((column + row) % 3 == 1) {
                color_index = 4U;
            }
            pattern.cells[row * STRIP_MAX_GRID_DIM + column] = color_index;
        }
    }
    return build_strip_design(&pattern, design_out);
}

static bool test_invalid_and_corrupt_inputs(void) {
    StitchDesign scratch = {};

    const uint8_t truncated_jef[16] = {0x74U, 0x00U, 0x00U, 0x00U, 0x0AU, 0x00U, 0x00U, 0x00U};
    if (!assert_condition(
            !parse_jef_design(truncated_jef, sizeof(truncated_jef), &scratch),
            "truncated JEF must fail safely"
        )) {
        free_stitch_design(&scratch);
        return false;
    }
    free_stitch_design(&scratch);

    uint8_t bad_offset_jef[JEF_HEADER_SIZE + 8U] = {};
    bad_offset_jef[0] = 0xFFU;
    bad_offset_jef[1] = 0xFFU;
    if (!assert_condition(
            !parse_jef_design(bad_offset_jef, sizeof(bad_offset_jef), &scratch),
            "out-of-bounds JEF stitch offset must fail safely"
        )) {
        free_stitch_design(&scratch);
        return false;
    }
    free_stitch_design(&scratch);

    const uint8_t truncated_pes[12] = {'#', 'P', 'E', 'S', '0', '0', '6', '0', 0xFFU, 0xFFU, 0x00U, 0x00U};
    if (!assert_condition(
            !parse_pes_design(truncated_pes, sizeof(truncated_pes), &scratch),
            "truncated PES must fail safely"
        )) {
        free_stitch_design(&scratch);
        return false;
    }
    free_stitch_design(&scratch);

    const uint8_t invalid_pec[32] = {'B', 'A', 'D', '!'};
    if (!assert_condition(
            !parse_pec_design(invalid_pec, sizeof(invalid_pec), &scratch),
            "invalid PEC header must fail safely"
        )) {
        free_stitch_design(&scratch);
        return false;
    }
    free_stitch_design(&scratch);

    const uint8_t truncated_dst[128] = {};
    if (!assert_condition(
            !parse_dst_design(truncated_dst, sizeof(truncated_dst), &scratch),
            "truncated DST must fail safely"
        )) {
        free_stitch_design(&scratch);
        return false;
    }
    free_stitch_design(&scratch);

    printf("[PASS] corrupt and truncated inputs rejected cleanly\n");
    return true;
}

static bool test_synthetic_format_parsers(void) {
    bool passed = false;
    const char *jef_path = "/tmp/embroidery_studio_test_synth.jef";
    const char *jef_plus_path = "/tmp/embroidery_studio_test_synth.jef+";
    const char *pes_path = "/tmp/embroidery_studio_test_synth.pes";
    const char *pec_path = "/tmp/embroidery_studio_test_synth.pec";
    const char *dst_path = "/tmp/embroidery_studio_test_synth.dst";

    StitchDesign base_design = {};
    StitchDesign loaded = {};
    uint8_t *jef_bytes = nullptr;
    uint8_t *jef_plus_bytes = nullptr;
    size_t jef_size = 0U;

    if (!assert_condition(build_synthetic_cross_grid_design(&base_design), "build synthetic base design") ||
        !assert_condition(write_jef_file(&base_design, jef_path), "write synthetic JEF")) {
        goto cleanup;
    }

    if (!assert_condition(read_binary_file(jef_path, &jef_bytes, &jef_size), "read synthetic JEF bytes")) {
        goto cleanup;
    }
    const size_t metadata_bytes = 64U;
    jef_plus_bytes = (uint8_t *)malloc(jef_size + metadata_bytes);
    if (!assert_condition(jef_plus_bytes != nullptr, "allocate JEF+ buffer")) {
        goto cleanup;
    }
    memcpy(jef_plus_bytes, jef_bytes, jef_size);
    memset(jef_plus_bytes + jef_size, 0x5AU, metadata_bytes);
    if (!assert_condition(
            write_binary_file(jef_plus_path, jef_plus_bytes, jef_size + metadata_bytes),
            "write synthetic JEF+"
        )) {
        goto cleanup;
    }

    if (!assert_condition(write_synthetic_pes_and_pec(pes_path, pec_path), "write synthetic PES and PEC") ||
        !assert_condition(write_synthetic_dst(dst_path), "write synthetic DST")) {
        goto cleanup;
    }

    const char *format_paths[5] = {jef_path, jef_plus_path, pes_path, pec_path, dst_path};
    const size_t min_colors[5] = {3U, 3U, 2U, 2U, 2U};
    const size_t min_stitches[5] = {500U, 500U, 6U, 6U, 6U};

    for (size_t format_index = 0U; format_index < 5U; format_index += 1U) {
        free_stitch_design(&loaded);
        if (!assert_condition(
                load_embroidery_file(format_paths[format_index], &loaded),
                "load synthetic embroidery format"
            )) {
            goto cleanup;
        }
        const DesignStats stats = compute_design_stats(&loaded);
        if (!assert_condition(
                loaded.has_bounds &&
                stats.stitch_count >= min_stitches[format_index] &&
                stats.color_count >= min_colors[format_index],
                "synthetic format stitch and color counts valid"
            )) {
            goto cleanup;
        }
    }

    printf("[PASS] standalone synthetic parsers (.jef, .jef+, .pes, .pec, .dst)\n");
    passed = true;

cleanup:
    free(jef_plus_bytes);
    free(jef_bytes);
    free_stitch_design(&loaded);
    free_stitch_design(&base_design);
    remove(jef_path);
    remove(jef_plus_path);
    remove(pes_path);
    remove(pec_path);
    remove(dst_path);
    return passed;
}

/* Regression test: 1-cross-wide x 10-row-high Cut must leave neighbors intact. */
static bool test_narrow_cut_regression(const StitchDesign *original, CrossGridInfo grid) {
    bool passed = false;
    const char *cut_path = "/tmp/embroidery_studio_test_narrow_cut.jef";
    const char *crop_path = "/tmp/embroidery_studio_test_narrow_crop.jef";
    uint8_t *keep_mask = nullptr;
    StitchDesign cut_result = {};
    StitchDesign crop_result = {};

    const int32_t pitch = grid.cross_pitch_units;
    const int32_t target_left = original->minimum_x + 4 * pitch;
    const int32_t target_top = original->minimum_y + 2 * pitch;
    const CutRectangle narrow_box = {target_left, target_top, target_left + pitch, target_top + 10 * pitch};
    const CutRectangle left_neighbor_box = {target_left - pitch, target_top, target_left, target_top + 10 * pitch};
    const CutRectangle right_neighbor_box = {target_left + pitch, target_top, target_left + 2 * pitch, target_top + 10 * pitch};
    const CutRectangle above_box = {original->minimum_x, original->minimum_y, original->maximum_x, target_top};
    const CutRectangle below_box = {original->minimum_x, target_top + 10 * pitch, original->maximum_x, original->maximum_y};

    const size_t total_segments = count_design_segments(original);
    keep_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
    if (!assert_condition(keep_mask != nullptr, "allocate regression keep mask")) {
        goto cleanup;
    }

    if (!assert_condition(
            build_edit_segment_mask(
                original,
                grid,
                EDIT_CUT_OUT,
                narrow_box,
                0,
                false,
                keep_mask,
                total_segments
            ),
            "build_edit_segment_mask for narrow 1x10 Cut"
        )) {
        goto cleanup;
    }

    size_t removed_inside = 0U;
    size_t removed_left = 0U;
    size_t removed_right = 0U;
    size_t removed_below = 0U;
    size_t removed_above = 0U;
    size_t flat_segment = 0U;

    for (size_t chain_index = 0U; chain_index < original->chain_count; chain_index += 1U) {
        const StitchChain chain = original->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t chain_segments = chain.point_count - 1U;
        if (chain.color_block_index >= original->block_count ||
            !original->blocks[chain.color_block_index].enabled) {
            flat_segment += chain_segments;
            continue;
        }
        const StitchPoint *points = &original->points[chain.point_offset];
        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (keep_mask[flat_segment + local_index] == 0U) {
                const int32_t midpoint_x =
                    (int32_t)(((int64_t)points[local_index].position_x +
                               (int64_t)points[local_index + 1U].position_x) / 2LL);
                const int32_t midpoint_y =
                    (int32_t)(((int64_t)points[local_index].position_y +
                               (int64_t)points[local_index + 1U].position_y) / 2LL);
                if (point_is_strictly_inside_rectangle(midpoint_x, midpoint_y, narrow_box)) {
                    removed_inside += 1U;
                } else if (point_is_strictly_inside_rectangle(midpoint_x, midpoint_y, left_neighbor_box)) {
                    removed_left += 1U;
                } else if (point_is_strictly_inside_rectangle(midpoint_x, midpoint_y, right_neighbor_box)) {
                    removed_right += 1U;
                } else if (midpoint_y > narrow_box.maximum_y) {
                    removed_below += 1U;
                } else if (midpoint_y < narrow_box.minimum_y) {
                    removed_above += 1U;
                }
            }
        }
        flat_segment += chain_segments;
    }

    if (!assert_condition(
            removed_inside >= 100U &&
            removed_left == 0U &&
            removed_right == 0U &&
            removed_below == 0U &&
            removed_above == 0U,
            "narrow 1x10 Cut removes ONLY crosses inside box with 0 collateral damage"
        )) {
        goto cleanup;
    }

    const size_t before_left = count_segments_with_midpoint_in_box(original, left_neighbor_box, true);
    const size_t before_right = count_segments_with_midpoint_in_box(original, right_neighbor_box, true);
    const size_t before_below = count_segments_with_midpoint_in_box(original, below_box, true);
    const size_t before_above = count_segments_with_midpoint_in_box(original, above_box, true);

    if (!assert_condition(
            apply_edit_operation(original, grid, EDIT_CUT_OUT, narrow_box, 0, false, &cut_result),
            "apply_edit_operation narrow 1x10 Cut"
        )) {
        goto cleanup;
    }

    const size_t after_inside = count_segments_with_midpoint_in_box(&cut_result, narrow_box, true);
    const size_t after_left = count_segments_with_midpoint_in_box(&cut_result, left_neighbor_box, true);
    const size_t after_right = count_segments_with_midpoint_in_box(&cut_result, right_neighbor_box, true);
    const size_t after_below = count_segments_with_midpoint_in_box(&cut_result, below_box, true);
    const size_t after_above = count_segments_with_midpoint_in_box(&cut_result, above_box, true);

    if (!assert_condition(
            after_inside == 0U &&
            after_left >= before_left &&
            after_right >= before_right &&
            after_below >= before_below &&
            after_above >= before_above,
            "committed 1x10 Cut keeps 100% of left, right, above, and below stitches intact"
        )) {
        goto cleanup;
    }

    if (!assert_condition(write_jef_file(&cut_result, cut_path), "write narrow cut JEF") ||
        !reload_and_verify_jef(cut_path, 500U, 3U)) {
        goto cleanup;
    }

    if (!assert_condition(
            apply_edit_operation(original, grid, EDIT_CROP_KEEP, narrow_box, 0, false, &crop_result),
            "apply_edit_operation narrow 1x10 Crop Keep"
        )) {
        goto cleanup;
    }
    const int32_t crop_width = crop_result.maximum_x - crop_result.minimum_x;
    const int32_t crop_height = crop_result.maximum_y - crop_result.minimum_y;
    if (!assert_condition(
            crop_width <= pitch + 4 && crop_height <= 10 * pitch + 4 &&
            compute_design_stats(&crop_result).stitch_count >= 100U,
            "narrow 1x10 Crop Keep isolates exact 1-column x 10-row strip"
        )) {
        goto cleanup;
    }
    if (!assert_condition(write_jef_file(&crop_result, crop_path), "write narrow crop JEF") ||
        !reload_and_verify_jef(crop_path, 100U, 1U)) {
        goto cleanup;
    }

    printf("[PASS] 1x10 narrow Cut & Crop regression test (removed=%zu, collateral=0)\n", removed_inside);
    passed = true;

cleanup:
    free_stitch_design(&crop_result);
    free_stitch_design(&cut_result);
    free(keep_mask);
    remove(cut_path);
    remove(crop_path);
    return passed;
}

static bool test_core_editing_workflows(void) {
    bool passed = false;
    const char *base_path = "/tmp/embroidery_studio_test_workflow_base.jef";
    const char *work_path = "/tmp/embroidery_studio_test_workflow_out.jef";
    const char *split_one_path = "/tmp/embroidery_studio_test_split1.jef";
    const char *split_two_path = "/tmp/embroidery_studio_test_split2.jef";

    StitchDesign built = {};
    StitchDesign original = {};
    StitchDesign edited = {};
    StitchDesign part_one = {};
    StitchDesign part_two = {};
    StitchDesign strip_design = {};
    StitchDesign combined = {};
    uint8_t *segment_mask = nullptr;
    uint8_t *shifted_mask = nullptr;
    uint8_t *second_shifted_mask = nullptr;

    if (!assert_condition(build_synthetic_cross_grid_design(&built), "build synthetic workflow grid") ||
        !assert_condition(write_jef_file(&built, base_path), "export synthetic workflow JEF") ||
        !assert_condition(load_embroidery_file(base_path, &original), "reload synthetic workflow JEF")) {
        goto cleanup;
    }

    const DesignStats original_stats = compute_design_stats(&original);
    const CrossGridInfo grid = detect_cross_stitch_grid(&original);
    if (!assert_condition(
            grid.detected_from_design && grid.cross_pitch_units == 18 && grid.cross_passes == 5,
            "auto-detect pitch=18 passes=5x on synthetic grid"
        )) {
        goto cleanup;
    }

    if (!test_narrow_cut_regression(&original, grid)) {
        goto cleanup;
    }

    /* 1. Sel X: select one cross cell and delete with locks. */
    const size_t total_segments = count_design_segments(&original);
    segment_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
    if (!assert_condition(segment_mask != nullptr, "allocate segment mask")) {
        goto cleanup;
    }

    const int32_t sample_x = original.minimum_x + 3 * grid.cross_pitch_units + grid.cross_pitch_units / 2;
    const int32_t sample_y = original.minimum_y + 5 * grid.cross_pitch_units + grid.cross_pitch_units / 2;
    CutRectangle resolved_cross_box = {};
    const size_t cell_hits = select_cross_at_point_in_mask(
        &original,
        grid,
        sample_x,
        sample_y,
        segment_mask,
        total_segments,
        false,
        &resolved_cross_box
    );
    if (!assert_condition(cell_hits >= 2U, "Sel X matches segments in cell")) {
        goto cleanup;
    }
    if (!assert_condition(
            delete_masked_segments(&original, grid.cross_pitch_units, segment_mask, total_segments, &edited),
            "Sel X delete masked cell"
        )) {
        goto cleanup;
    }
    const DesignStats after_sel_x = compute_design_stats(&edited);
    if (!assert_condition(
            after_sel_x.stitch_count < original_stats.stitch_count && after_sel_x.lock_count >= 1U,
            "Sel X delete reduces stitches and inserts tie-off locks"
        )) {
        goto cleanup;
    }
    if (!assert_condition(write_jef_file(&edited, work_path), "write sel_x JEF") ||
        !reload_and_verify_jef(work_path, 500U, 2U)) {
        goto cleanup;
    }
    free_stitch_design(&edited);
    printf("[PASS] Sel X single-cross selection & locked deletion\n");

    /* 2. Sel Elem: spatial-hash BFS, recolor, chained arrow shift, delete. */
    memset(segment_mask, 0, total_segments);
    size_t seed_chain = 0U;
    if (!assert_condition(
            find_nearest_chain_at_point(&original, sample_x, sample_y, grid.cross_pitch_units, &seed_chain),
            "find_nearest_chain_at_point finds seed chain"
        )) {
        goto cleanup;
    }
    const size_t element_segments = select_connected_element_in_mask(
        &original,
        grid,
        seed_chain,
        segment_mask,
        total_segments,
        false
    );
    if (!assert_condition(element_segments >= cell_hits, "Sel Elem selects connected element")) {
        goto cleanup;
    }
    if (!assert_condition(
            recolor_masked_segments(&original, segment_mask, total_segments, 12U, &edited),
            "Sel Elem recolor to JEF palette index 12"
        )) {
        goto cleanup;
    }
    if (!assert_condition(write_jef_file(&edited, work_path), "write recolor JEF") ||
        !reload_and_verify_jef(work_path, 500U, 2U)) {
        goto cleanup;
    }
    free_stitch_design(&edited);

    size_t shifted_mask_length = 0U;
    if (!assert_condition(
            shift_masked_segments(
                &original,
                segment_mask,
                total_segments,
                grid.cross_pitch_units,
                0,
                &edited,
                &shifted_mask,
                &shifted_mask_length
            ),
            "Sel Elem first arrow shift"
        )) {
        goto cleanup;
    }
    size_t second_mask_length = 0U;
    if (!assert_condition(
            shift_masked_segments(
                &edited,
                shifted_mask,
                shifted_mask_length,
                0,
                grid.cross_pitch_units,
                &combined,
                &second_shifted_mask,
                &second_mask_length
            ),
            "Sel Elem second arrow shift with updated mask"
        )) {
        goto cleanup;
    }
    if (!assert_condition(write_jef_file(&combined, work_path), "write shift JEF") ||
        !reload_and_verify_jef(work_path, 500U, 2U)) {
        goto cleanup;
    }
    free_stitch_design(&edited);
    free_stitch_design(&combined);

    if (!assert_condition(
            delete_masked_segments(&original, grid.cross_pitch_units, segment_mask, total_segments, &edited),
            "Sel Elem delete element"
        )) {
        goto cleanup;
    }
    if (!assert_condition(
            compute_design_stats(&edited).stitch_count < original_stats.stitch_count,
            "Sel Elem delete reduces stitches"
        )) {
        goto cleanup;
    }
    free_stitch_design(&edited);
    printf("[PASS] Sel Elem BFS, recolor, chained arrow shift, and delete\n");

    /* 3. Strip builder with 1x, 5x, and 6x passes + insert + Merge Colors. */
    constexpr int32_t TEST_PASS_MODES[3] = {1, 5, 6};
    for (size_t mode_index = 0U; mode_index < 3U; mode_index += 1U) {
        StripPattern pattern = {};
        pattern.width = 3;
        pattern.height = 5;
        pattern.repeat_x = 1;
        pattern.repeat_y = 6;
        pattern.pitch_units = grid.cross_pitch_units;
        pattern.passes = TEST_PASS_MODES[mode_index];
        pattern.cells[0 * STRIP_MAX_GRID_DIM + 1] = 10U;
        pattern.cells[1 * STRIP_MAX_GRID_DIM + 0] = 10U;
        pattern.cells[1 * STRIP_MAX_GRID_DIM + 2] = 10U;
        pattern.cells[2 * STRIP_MAX_GRID_DIM + 0] = 10U;
        pattern.cells[2 * STRIP_MAX_GRID_DIM + 1] = 1U;
        pattern.cells[2 * STRIP_MAX_GRID_DIM + 2] = 10U;
        pattern.cells[3 * STRIP_MAX_GRID_DIM + 0] = 10U;
        pattern.cells[3 * STRIP_MAX_GRID_DIM + 2] = 10U;
        pattern.cells[4 * STRIP_MAX_GRID_DIM + 1] = 10U;

        free_stitch_design(&strip_design);
        if (!assert_condition(build_strip_design(&pattern, &strip_design), "build strip design")) {
            goto cleanup;
        }
        const CrossGridInfo strip_grid = detect_cross_stitch_grid(&strip_design);
        if (!assert_condition(
                strip_grid.cross_pitch_units == 18 &&
                strip_grid.cross_passes == TEST_PASS_MODES[mode_index],
                "strip pass count round-trip 1x/5x/6x"
            )) {
            goto cleanup;
        }
    }

    if (!assert_condition(
            insert_design_fragment(&original, &strip_design, 12 * grid.cross_pitch_units, 0, &combined),
            "insert strip fragment"
        )) {
        goto cleanup;
    }
    const size_t blocks_before_merge = combined.block_count;
    const size_t stitches_before_merge = compute_design_stats(&combined).stitch_count;
    if (!assert_condition(merge_duplicate_color_blocks(&combined), "merge duplicate color blocks")) {
        goto cleanup;
    }
    if (!assert_condition(
            combined.block_count <= blocks_before_merge &&
            compute_design_stats(&combined).stitch_count == stitches_before_merge,
            "Merge Colors preserves exact stitch count"
        )) {
        goto cleanup;
    }
    if (!assert_condition(write_jef_file(&combined, work_path), "write merged JEF") ||
        !reload_and_verify_jef(work_path, original_stats.stitch_count, 3U)) {
        goto cleanup;
    }
    free_stitch_design(&combined);
    printf("[PASS] Strip generator (1x/5x/6x passes), fragment insert, and Merge Colors\n");

    /* 4. Crop Keep, Cut Out, Split V/H, Clone, Transforms, Color Order/Delete. */
    const CutRectangle crop_box = {
        original.minimum_x + 2 * grid.cross_pitch_units,
        original.minimum_y + 2 * grid.cross_pitch_units,
        original.maximum_x - 2 * grid.cross_pitch_units,
        original.maximum_y - 2 * grid.cross_pitch_units
    };
    if (!assert_condition(
            apply_edit_operation(&original, grid, EDIT_CROP_KEEP, crop_box, 0, false, &edited),
            "Crop Keep"
        ) ||
        !assert_condition(
            insert_design_fragment(&original, &edited, 12 * grid.cross_pitch_units, 0, &combined),
            "Clone fragment insert"
        )) {
        goto cleanup;
    }
    free_stitch_design(&combined);
    free_stitch_design(&edited);

    const CutRectangle empty_box = {};
    const int32_t split_x = (original.minimum_x + original.maximum_x) / 2;
    const int32_t split_y = (original.minimum_y + original.maximum_y) / 2;

    if (!assert_condition(
            apply_edit_operation(&original, grid, EDIT_SPLIT_VERTICAL, empty_box, split_x, false, &part_one) &&
            apply_edit_operation(&original, grid, EDIT_SPLIT_VERTICAL, empty_box, split_x, true, &part_two),
            "Split Vertical"
        ) ||
        !assert_condition(
            write_jef_file(&part_one, split_one_path) &&
            write_jef_file(&part_two, split_two_path),
            "write Split V JEFs"
        ) ||
        !reload_and_verify_jef(split_one_path, 100U, 1U) ||
        !reload_and_verify_jef(split_two_path, 100U, 1U)) {
        goto cleanup;
    }
    free_stitch_design(&part_one);
    free_stitch_design(&part_two);

    if (!assert_condition(
            apply_edit_operation(&original, grid, EDIT_SPLIT_HORIZONTAL, empty_box, split_y, false, &part_one) &&
            apply_edit_operation(&original, grid, EDIT_SPLIT_HORIZONTAL, empty_box, split_y, true, &part_two),
            "Split Horizontal"
        ) ||
        !assert_condition(
            write_jef_file(&part_one, split_one_path) &&
            write_jef_file(&part_two, split_two_path),
            "write Split H JEFs"
        ) ||
        !reload_and_verify_jef(split_one_path, 100U, 1U) ||
        !reload_and_verify_jef(split_two_path, 100U, 1U)) {
        goto cleanup;
    }
    free_stitch_design(&part_one);
    free_stitch_design(&part_two);

    if (!assert_condition(clone_stitch_design(&original, &edited), "clone for transforms")) {
        goto cleanup;
    }
    rotate_design_90_clockwise(&edited);
    mirror_design_horizontal(&edited);
    mirror_design_vertical(&edited);
    if (!assert_condition(move_color_block_order(&edited, 0U, 1), "move color block down") ||
        !assert_condition(move_color_block_order(&edited, 1U, -1), "move color block up") ||
        !assert_condition(remove_color_block_at(&edited, edited.block_count - 1U), "remove last block")) {
        goto cleanup;
    }
    shift_design_center(&edited, 20, -20);
    center_stitch_design(&edited);
    if (!assert_condition(write_jef_file(&edited, work_path), "write transform JEF") ||
        !reload_and_verify_jef(work_path, 200U, 1U)) {
        goto cleanup;
    }
    free_stitch_design(&edited);
    printf("[PASS] Crop, Clone, Split V/H, Rotate/Flip, Color Order/Delete\n");

    passed = true;

cleanup:
    free(second_shifted_mask);
    free(shifted_mask);
    free(segment_mask);
    free_stitch_design(&combined);
    free_stitch_design(&strip_design);
    free_stitch_design(&part_two);
    free_stitch_design(&part_one);
    free_stitch_design(&edited);
    free_stitch_design(&original);
    free_stitch_design(&built);
    remove(base_path);
    remove(work_path);
    remove(split_one_path);
    remove(split_two_path);
    return passed;
}

static bool test_draw_x_and_unique_paths(void) {
    bool passed = false;
    const char *base_stem_path = "/tmp/embroidery_studio_test_draw.jef";
    char first_path[256] = {};
    char second_path[256] = {};
    StitchDesign drawn = {};

    size_t red_block = 0U;
    if (!assert_condition(
            design_append_block(&drawn, janome_palette[10], 10U, true, &red_block),
            "append red color block"
        )) {
        goto cleanup;
    }

    CrossGridInfo grid = {};
    grid.cross_pitch_units = 18;
    grid.cross_passes = 5;

    for (int32_t column = 0; column < 4; column += 1) {
        if (!assert_condition(
                draw_single_cross_into_design(&drawn, grid, column, 0, red_block),
                "draw single 5x cross"
            )) {
            goto cleanup;
        }
    }

    const int32_t width_units = drawn.maximum_x - drawn.minimum_x;
    const int32_t height_units = drawn.maximum_y - drawn.minimum_y;
    if (!assert_condition(width_units == 72 && height_units == 18, "Draw X 4 crosses = 7.2 x 1.8 mm")) {
        goto cleanup;
    }

    remove("/tmp/embroidery_studio_test_draw_draw_x.jef");
    remove("/tmp/embroidery_studio_test_draw_draw_x_2.jef");
    if (!assert_condition(
            build_unique_suffixed_jef_path(base_stem_path, "_draw_x", first_path, sizeof(first_path)),
            "build first unique path"
        ) ||
        !assert_condition(write_jef_file(&drawn, first_path), "write first Draw X JEF") ||
        !assert_condition(
            build_unique_suffixed_jef_path(base_stem_path, "_draw_x", second_path, sizeof(second_path)),
            "build second unique path"
        ) ||
        !assert_condition(strcmp(first_path, second_path) != 0, "unique path avoids overwriting") ||
        !reload_and_verify_jef(first_path, 20U, 1U)) {
        goto cleanup;
    }

    const size_t total_segments = count_design_segments(&drawn);
    uint8_t *mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
    if (!assert_condition(mask != nullptr, "allocate rect mask")) {
        goto cleanup;
    }
    const CutRectangle box = {-5, -5, 40, 25};
    const size_t box_selected = select_crosses_in_rectangle_mask(&drawn, grid, box, mask, total_segments);
    free(mask);
    if (!assert_condition(box_selected > 0U, "select_crosses_in_rectangle_mask selects crosses")) {
        goto cleanup;
    }

    printf("[PASS] Draw X (5x), unique JEF path generation, and rectangle cross selection\n");
    passed = true;

cleanup:
    if (first_path[0] != '\0') {
        remove(first_path);
    }
    free_stitch_design(&drawn);
    return passed;
}

static bool has_supported_extension(const char *file_name) {
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

static bool test_optional_real_directory(void) {
    const char *test_dir = getenv("EMB_TEST_DIR");
    if (test_dir == nullptr || test_dir[0] == '\0') {
        return true;
    }

    DIR *directory = opendir(test_dir);
    if (directory == nullptr) {
        fprintf(stderr, "TEST FAILED: cannot open EMB_TEST_DIR=%s\n", test_dir);
        return false;
    }

    size_t checked_files = 0U;
    bool all_ok = true;
    for (struct dirent *entry = readdir(directory); entry != nullptr; entry = readdir(directory)) {
        if (entry->d_name[0] == '.' || !has_supported_extension(entry->d_name)) {
            continue;
        }
        char full_path[1024] = {};
        const int written = snprintf(full_path, sizeof(full_path), "%s/%s", test_dir, entry->d_name);
        if (written <= 0 || (size_t)written >= sizeof(full_path)) {
            continue;
        }
        struct stat file_info = {};
        if (stat(full_path, &file_info) != 0 || !S_ISREG(file_info.st_mode)) {
            continue;
        }
        StitchDesign design = {};
        if (!load_embroidery_file(full_path, &design)) {
            fprintf(stderr, "TEST FAILED: EMB_TEST_DIR failed to load %s\n", full_path);
            all_ok = false;
            free_stitch_design(&design);
            break;
        }
        const CrossGridInfo grid = detect_cross_stitch_grid(&design);
        const DesignStats stats = compute_design_stats(&design);
        printf(
            "[INFO] EMB_TEST_DIR %s: stitches=%zu colors=%zu pitch=%.1fmm %dx\n",
            entry->d_name,
            stats.stitch_count,
            stats.color_count,
            (double)grid.cross_pitch_units * 0.1,
            grid.cross_passes
        );
        free_stitch_design(&design);
        checked_files += 1U;
    }

    closedir(directory);
    if (all_ok) {
        printf("[PASS] EMB_TEST_DIR checked %zu real embroidery file(s)\n", checked_files);
    }
    return all_ok;
}

static bool test_stage1_security_and_correctness_regressions(void) {
    bool passed = false;
    const char *unsorted_jef_path = "/tmp/embroidery_studio_test_unsorted.jef";
    char temp_probe_path[256] = {};
    StitchDesign unsorted = {};
    StitchDesign reloaded = {};
    StitchDesign corrupt_blocks = {};
    StitchDesign duplicate_blocks = {};
    StitchDesign pitch28_source = {};
    StitchDesign pitch28_deleted = {};
    StitchDesign zero_chain_design = {};
    StitchDesign history_work = {};
    StitchDesign dir_test = {};
    EditHistory history = {};

    /* 1. write_jef_file with unsorted/interleaved color blocks + atomic overwrite. */
    size_t red_block = 0U;
    size_t blue_block = 0U;
    if (!assert_condition(
            design_append_block(&unsorted, janome_palette[10], 10U, true, &red_block) &&
            design_append_block(&unsorted, janome_palette[12], 12U, true, &blue_block),
            "create 2 color blocks for interleaved JEF export"
        )) {
        goto cleanup;
    }

    const size_t chain_blocks[3] = {red_block, blue_block, red_block};
    for (size_t chain_slot = 0U; chain_slot < 3U; chain_slot += 1U) {
        const size_t offset = unsorted.point_count;
        const int32_t base_x = (int32_t)chain_slot * 36;
        if (!design_append_point(&unsorted, base_x, 0) ||
            !design_append_point(&unsorted, base_x + 18, 18) ||
            !design_append_point(&unsorted, base_x + 18, 0) ||
            !design_append_point(&unsorted, base_x, 18) ||
            !design_append_chain(&unsorted, offset, 4U, chain_blocks[chain_slot], false, false)) {
            goto cleanup;
        }
    }
    recompute_design_bounds(&unsorted);

    if (!assert_condition(
            write_jef_file(&unsorted, unsorted_jef_path) &&
            write_jef_file(&unsorted, unsorted_jef_path),
            "write_jef_file handles interleaved chains and atomic overwrite safely"
        )) {
        goto cleanup;
    }
    (void)snprintf(temp_probe_path, sizeof(temp_probe_path), "%s.tmp.%ld", unsorted_jef_path, (long)getpid());
    if (!assert_condition(!file_already_exists(temp_probe_path), "atomic save leaves no temporary file behind")) {
        goto cleanup;
    }
    if (!assert_condition(load_embroidery_file(unsorted_jef_path, &reloaded), "reload interleaved JEF")) {
        goto cleanup;
    }
    const DesignStats reloaded_stats = compute_design_stats(&reloaded);
    if (!assert_condition(
            reloaded_stats.color_count == 2U && reloaded_stats.stitch_count >= 9U,
            "interleaved JEF grouped into 2 color blocks with stitches intact"
        )) {
        goto cleanup;
    }

    /* 2. build_edit_segment_mask with short mask_length < total_segments (must terminate). */
    const CrossGridInfo default_grid = {
        .cross_pitch_units = 18,
        .grid_origin_x = 0,
        .grid_origin_y = 0,
        .cross_passes = 3,
        .raw_detected_passes = 3,
        .diagonal_stitch_count = 0U,
        .detected_from_design = true
    };
    const CutRectangle cut_box = {-10, -10, 20, 20};
    uint8_t tiny_mask[3] = {};
    if (!assert_condition(
            build_edit_segment_mask(&unsorted, default_grid, EDIT_CUT_OUT, cut_box, 0, false, tiny_mask, sizeof(tiny_mask)) &&
            build_edit_segment_mask(&unsorted, default_grid, EDIT_CUT_OUT, cut_box, 0, false, tiny_mask, 1U) &&
            build_edit_segment_mask(&unsorted, default_grid, EDIT_CUT_OUT, cut_box, 0, false, nullptr, 0U),
            "build_edit_segment_mask terminates cleanly on short or empty mask"
        )) {
        goto cleanup;
    }

    /* 3. compact_non_empty_blocks & merge_duplicate_color_blocks OOB index guards + point compaction. */
    if (!assert_condition(clone_stitch_design(&unsorted, &corrupt_blocks), "clone design for OOB block guard")) {
        goto cleanup;
    }
    const size_t points_before_compact = corrupt_blocks.point_count;
    corrupt_blocks.chains[2].color_block_index = 999U;
    if (!assert_condition(
            compact_non_empty_blocks(&corrupt_blocks) &&
            corrupt_blocks.chain_count == 2U &&
            corrupt_blocks.point_count < points_before_compact,
            "compact_non_empty_blocks guards OOB block index and compacts orphaned points"
        )) {
        goto cleanup;
    }

    const size_t points_before_remove = corrupt_blocks.point_count;
    if (!assert_condition(
            remove_color_block_at(&corrupt_blocks, 0U) &&
            corrupt_blocks.block_count == 1U &&
            corrupt_blocks.point_count < points_before_remove,
            "remove_color_block_at compacts orphaned points"
        )) {
        goto cleanup;
    }

    size_t dup_block_zero = 0U;
    size_t dup_block_one = 0U;
    size_t dup_block_two = 0U;
    if (!design_append_block(&duplicate_blocks, janome_palette[10], 10U, true, &dup_block_zero) ||
        !design_append_block(&duplicate_blocks, janome_palette[12], 12U, true, &dup_block_one) ||
        !design_append_block(&duplicate_blocks, janome_palette[10], 10U, true, &dup_block_two)) {
        goto cleanup;
    }
    for (size_t item_index = 0U; item_index < 3U; item_index += 1U) {
        const size_t offset = duplicate_blocks.point_count;
        const size_t block_target = item_index;
        if (!design_append_point(&duplicate_blocks, (int32_t)item_index * 20, 0) ||
            !design_append_point(&duplicate_blocks, (int32_t)item_index * 20 + 18, 18) ||
            !design_append_chain(&duplicate_blocks, offset, 2U, block_target, false, false)) {
            goto cleanup;
        }
    }
    /* Append a chain with an out-of-range color_block_index to verify guard. */
    const size_t bad_offset = duplicate_blocks.point_count;
    if (!design_append_point(&duplicate_blocks, 100, 100) ||
        !design_append_point(&duplicate_blocks, 118, 118) ||
        !design_append_chain(&duplicate_blocks, bad_offset, 2U, 777U, false, false)) {
        goto cleanup;
    }
    const size_t dup_points_before = duplicate_blocks.point_count;
    if (!assert_condition(
            merge_duplicate_color_blocks(&duplicate_blocks) &&
            duplicate_blocks.block_count == 2U &&
            duplicate_blocks.chain_count == 3U &&
            duplicate_blocks.point_count < dup_points_before,
            "merge_duplicate_color_blocks guards OOB block index and compacts points"
        )) {
        goto cleanup;
    }

    /* 4. append_locked_subchain underflow guard & reorder_cutout_chains_by_side zero-point guard. */
    const StitchPoint single_point[1] = {{10, 20}};
    if (!assert_condition(
            !append_locked_subchain(&zero_chain_design, single_point, 0U, 1U, 0U, true, true) &&
            !append_locked_subchain(&zero_chain_design, single_point, 0U, 0U, 0U, true, true),
            "append_locked_subchain rejects run_point_count < 2 without underflow"
        )) {
        goto cleanup;
    }
    if (!design_append_block(&zero_chain_design, janome_palette[10], 10U, true, nullptr) ||
        !design_append_point(&zero_chain_design, -40, 0) ||
        !design_append_point(&zero_chain_design, -22, 18) ||
        !design_append_point(&zero_chain_design, 40, 0) ||
        !design_append_point(&zero_chain_design, 58, 18) ||
        !design_append_chain(&zero_chain_design, 0U, 0U, 0U, false, false) ||
        !design_append_chain(&zero_chain_design, 0U, 2U, 0U, false, false) ||
        !design_append_chain(&zero_chain_design, 2U, 2U, 0U, false, false)) {
        goto cleanup;
    }
    const CutRectangle center_cut = {-10, -10, 10, 10};
    if (!assert_condition(
            reorder_cutout_chains_by_side(&zero_chain_design, center_cut),
            "reorder_cutout_chains_by_side handles zero-point chain without division by zero"
        )) {
        goto cleanup;
    }

    /* 5. Transactional EditHistory undo/redo round-trip. */
    history_init(&history);
    if (!assert_condition(clone_stitch_design(&unsorted, &history_work), "clone initial history state") ||
        !assert_condition(history_push(&history, &history_work), "push history state 0")) {
        goto cleanup;
    }
    const size_t initial_stitches = compute_design_stats(&history_work).stitch_count;
    if (!assert_condition(remove_color_block_at(&history_work, 1U), "mutate history state 1") ||
        !assert_condition(history_push(&history, &history_work), "push history state 1")) {
        goto cleanup;
    }
    const size_t mutated_stitches = compute_design_stats(&history_work).stitch_count;
    if (!assert_condition(mutated_stitches < initial_stitches, "mutated state has fewer stitches") ||
        !assert_condition(history_undo(&history, &history_work), "transactional undo succeeds") ||
        !assert_condition(compute_design_stats(&history_work).stitch_count == initial_stitches, "undo restores exact stitches") ||
        !assert_condition(!history_undo(&history, &history_work), "undo at index 0 safely returns false") ||
        !assert_condition(compute_design_stats(&history_work).stitch_count == initial_stitches, "failed undo preserves design") ||
        !assert_condition(history_redo(&history, &history_work), "transactional redo succeeds") ||
        !assert_condition(compute_design_stats(&history_work).stitch_count == mutated_stitches, "redo restores mutated stitches")) {
        goto cleanup;
    }

    /* 6. delete_masked_segments with real pitch=28 preserves valid 2.8 mm diagonal stitch. */
    size_t pitch28_block = 0U;
    if (!design_append_block(&pitch28_source, janome_palette[10], 10U, true, &pitch28_block) ||
        !design_append_point(&pitch28_source, 0, 0) ||
        !design_append_point(&pitch28_source, 28, 28) ||
        !design_append_point(&pitch28_source, 28, 0) ||
        !design_append_point(&pitch28_source, 0, 28) ||
        !design_append_chain(&pitch28_source, 0U, 4U, pitch28_block, false, false)) {
        goto cleanup;
    }
    const uint8_t pitch28_mask[3] = {0U, 1U, 1U};
    if (!assert_condition(
            delete_masked_segments(&pitch28_source, 28, pitch28_mask, 3U, &pitch28_deleted) &&
            pitch28_deleted.chain_count == 1U &&
            compute_design_stats(&pitch28_deleted).stitch_count >= 1U,
            "delete_masked_segments with pitch=28 preserves single 2.8mm cross diagonal"
        )) {
        goto cleanup;
    }

    /* 7. is_cross_diagonal_for_pitch integer math matches sqrt() reference across pitches 10..50. */
    for (int32_t pitch = MIN_CROSS_PITCH_UNITS; pitch <= MAX_CROSS_PITCH_UNITS; pitch += 1) {
        for (int32_t delta_x = 0; delta_x <= pitch + 15; delta_x += 1) {
            for (int32_t delta_y = delta_x - 4; delta_y <= delta_x + 4; delta_y += 1) {
                if (delta_y < 0) {
                    continue;
                }
                const StitchPoint start_pt = {0, 0};
                const StitchPoint end_pt = {delta_x, delta_y};
                const bool integer_result = is_cross_diagonal_for_pitch(start_pt, end_pt, pitch);

                int32_t axis_diff = delta_x - delta_y;
                if (axis_diff < 0) {
                    axis_diff = -axis_diff;
                }
                bool float_result = false;
                if (axis_diff <= 3) {
                    const double seg_len = sqrt((double)delta_x * (double)delta_x + (double)delta_y * (double)delta_y);
                    const double exp_diag = (double)pitch * sqrt(2.0);
                    float_result = (seg_len >= exp_diag * 0.80 - 1e-9 && seg_len <= exp_diag * 1.20 + 1e-9);
                }
                if (!assert_condition(
                        integer_result == float_result,
                        "is_cross_diagonal_for_pitch integer math matches sqrt reference"
                    )) {
                    goto cleanup;
                }
            }
        }
    }
    const StitchPoint huge_start = {-50000000, -50000000};
    const StitchPoint huge_end = {50000000, 50000000};
    if (!assert_condition(
            !is_cross_diagonal_for_pitch(huge_start, huge_end, 18),
            "is_cross_diagonal_for_pitch rejects huge coordinates without overflow"
        )) {
        goto cleanup;
    }

    /* 8. load_embroidery_file rejects directories via fstat + S_ISREG. */
    if (!assert_condition(
            !load_embroidery_file("/tmp", &dir_test),
            "load_embroidery_file rejects non-regular directory path"
        )) {
        goto cleanup;
    }

    printf("[PASS] Stage 1 security & correctness regressions (8/8 checks)\n");
    passed = true;

cleanup:
    history_clear(&history);
    free_stitch_design(&dir_test);
    free_stitch_design(&history_work);
    free_stitch_design(&zero_chain_design);
    free_stitch_design(&pitch28_deleted);
    free_stitch_design(&pitch28_source);
    free_stitch_design(&duplicate_blocks);
    free_stitch_design(&corrupt_blocks);
    free_stitch_design(&reloaded);
    free_stitch_design(&unsorted);
    remove(unsorted_jef_path);
    return passed;
}

static size_t count_non_background_pixels(const PixelCanvas *canvas, uint32_t background_pixel) {
    size_t drawn_pixels = 0U;
    const size_t total_pixels = (size_t)canvas->width * (size_t)canvas->height;
    for (size_t pixel_index = 0U; pixel_index < total_pixels; pixel_index += 1U) {
        if (canvas->pixels[pixel_index] != background_pixel) {
            drawn_pixels += 1U;
        }
    }
    return drawn_pixels;
}

static bool test_headless_render_modes(void) {
    bool passed = false;
    StitchDesign design = {};
    PixelCanvas canvas = {};
    PixelCanvas ui_canvas = {};
    BlockDedupTable dedup = {};
    EditorState ui_state = {};
    uint8_t *keep_mask = nullptr;
    uint8_t *selection_mask = nullptr;
    uint8_t *hover_mask = nullptr;

    if (!assert_condition(build_synthetic_cross_grid_design(&design), "build design for headless render")) {
        goto cleanup;
    }

    /* Add a non-diagonal contour backstitch chain to test Blocks Pass 2 rendering. */
    const size_t contour_offset = design.point_count;
    if (!design_append_point(&design, design.minimum_x, design.minimum_y) ||
        !design_append_point(&design, design.maximum_x, design.minimum_y) ||
        !design_append_point(&design, design.maximum_x, design.maximum_y) ||
        !design_append_chain(&design, contour_offset, 3U, 0U, true, true)) {
        goto cleanup;
    }
    recompute_design_bounds(&design);

    const CrossGridInfo grid = detect_cross_stitch_grid(&design);
    const size_t total_segments = count_design_segments(&design);
    keep_mask = (uint8_t *)malloc(total_segments);
    selection_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
    hover_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
    if (!assert_condition(
            keep_mask != nullptr && selection_mask != nullptr && hover_mask != nullptr,
            "allocate headless render masks"
        )) {
        goto cleanup;
    }
    for (size_t segment_index = 0U; segment_index < total_segments; segment_index += 1U) {
        if ((segment_index % 4U) == 0U) {
            keep_mask[segment_index] = 0U;
        } else {
            keep_mask[segment_index] = 1U;
        }
        if (segment_index < 20U) {
            selection_mask[segment_index] = 1U;
        } else if (segment_index < 40U) {
            hover_mask[segment_index] = 1U;
        }
    }

    if (!assert_condition(init_pixel_canvas(&canvas, 320, 240), "init 320x240 headless PixelCanvas") ||
        !assert_condition(init_block_dedup_table(&dedup), "init headless BlockDedupTable")) {
        goto cleanup;
    }

    const uint32_t background = pack_rgb_pixel(245U, 246U, 248U);
    const RenderViewport viewport = {0, 0, canvas.width, canvas.height};
    const CameraView camera = {
        .center_x = ((double)design.minimum_x + (double)design.maximum_x) * 0.5,
        .center_y = ((double)design.minimum_y + (double)design.maximum_y) * 0.5,
        .pixels_per_unit = 0.85
    };

    const StitchViewMode modes[3] = {VIEW_MODE_THREAD, VIEW_MODE_BLOCKS, VIEW_MODE_LINE};
    size_t drawn_counts[3] = {};

    for (size_t mode_index = 0U; mode_index < 3U; mode_index += 1U) {
        reset_canvas_clip(&canvas);
        draw_filled_rectangle(&canvas, 0, 0, canvas.width, canvas.height, background);

        DesignRenderParams params = {
            .viewport = viewport,
            .camera = camera,
            .grid = grid,
            .view_mode = modes[mode_index],
            .thread_thickness_index = 2U,
            .show_jumps = true,
            .show_locks = true,
            .draw_overlays = true,
            .world_offset_x = 0,
            .world_offset_y = 0,
            .keep_preview_mask = keep_mask,
            .keep_preview_mask_length = total_segments,
            .selection_mask = selection_mask,
            .selection_mask_length = total_segments,
            .hover_mask = hover_mask,
            .hover_mask_length = total_segments,
            .maximum_stitches_to_draw = total_segments / 2U,
            .block_dedup = &dedup
        };
        render_design_chains(&canvas, &design, &params);
        drawn_counts[mode_index] = count_non_background_pixels(&canvas, background);
    }

    if (!assert_condition(
            drawn_counts[0] > 500U && drawn_counts[1] > 500U && drawn_counts[2] > 500U,
            "headless Thread, Blocks, and Line modes all draw stitch pixels without SDL"
        )) {
        goto cleanup;
    }
    if (!assert_condition(
            drawn_counts[0] > drawn_counts[2] && drawn_counts[1] > drawn_counts[2],
            "Thread and Blocks cover more pixels than 1px Line mode"
        )) {
        goto cleanup;
    }

    /* Exercise full headless UI frame rendering (toolbar, panels, grid, hoop, stitches) without SDL. */
    if (!assert_condition(init_pixel_canvas(&ui_canvas, 1280, 720), "init 1280x720 headless UI canvas")) {
        goto cleanup;
    }
    init_default_editor_state(&ui_state);
    if (!assert_condition(clone_stitch_design(&design, &ui_state.design), "clone design into headless EditorState")) {
        goto cleanup;
    }
    ui_state.grid = grid;
    fit_camera_to_design(&ui_state, ui_canvas.width, ui_canvas.height);
    render_editor_frame(&ui_canvas, &ui_state);
    const EditorLayout ui_layout = compute_editor_layout(&ui_state, ui_canvas.width, ui_canvas.height);
    if (!assert_condition(
            ui_layout.top_button_count >= 10U &&
            ui_layout.rail_button_count == 15U &&
            hit_test_editor_buttons(&ui_layout, ui_layout.top_buttons[0].left_x + 4, ui_layout.top_buttons[0].top_y + 4) == COMMAND_OPEN_FILE_DIALOG,
            "headless render_editor_frame and unified hit_test_editor_buttons succeed without SDL"
        )) {
        goto cleanup;
    }

    printf(
        "[PASS] Headless render (no SDL): Thread=%zu px, Blocks=%zu px, Line=%zu px + UI frame\n",
        drawn_counts[0],
        drawn_counts[1],
        drawn_counts[2]
    );
    passed = true;

cleanup:
    free_editor_state(&ui_state);
    free_pixel_canvas(&ui_canvas);
    free_block_dedup_table(&dedup);
    free_pixel_canvas(&canvas);
    free(hover_mask);
    free(selection_mask);
    free(keep_mask);
    free_stitch_design(&design);
    return passed;
}

static bool test_stage2_caches_and_cow_history(void) {
    bool passed = false;
    StitchDesign design = {};
    StitchDesign snapshot_copy = {};
    EditHistory history = {};
    ScratchArena scratch = {};
    DesignSpatialIndex spatial = {};
    CrossClusterCache cross_cache = {};
    uint8_t *mask_plain = nullptr;
    uint8_t *mask_cached = nullptr;
    uint8_t *sel_plain = nullptr;
    uint8_t *sel_indexed = nullptr;
    EditorState editor = {};
    PixelCanvas canvas = {};

    history_init(&history);
    scratch_arena_init(&scratch);
    spatial_index_init(&spatial);
    cross_cluster_cache_init(&cross_cache);
    init_default_editor_state(&editor);

    if (!build_synthetic_cross_grid_design(&design)) {
        goto cleanup;
    }
    const CrossGridInfo grid = detect_cross_stitch_grid(&design);
    const size_t total_segments = count_design_segments(&design);
    if (!assert_condition(total_segments > 500U, "sample grid has enough segments for Stage 2 checks")) {
        goto cleanup;
    }

    /* 1. SharedPointBuffer CoW + EditHistory deduplicated memory accounting. */
    const SharedPointBuffer *initial_buffer = design.point_buffer;
    if (!assert_condition(initial_buffer != nullptr && initial_buffer->ref_count == 1U, "fresh design owns unique SharedPointBuffer")) {
        goto cleanup;
    }
    for (size_t step = 0U; step < 10U; step += 1U) {
        if (!assert_condition(history_push(&history, &design), "history_push with shared points")) {
            goto cleanup;
        }
    }
    if (!assert_condition(
            design.point_buffer == initial_buffer && initial_buffer->ref_count == 11U,
            "10 history snapshots share one SharedPointBuffer with ref_count=11"
        )) {
        goto cleanup;
    }
    const size_t single_point_bytes = design_point_buffer_bytes(&design);
    const size_t expected_history_bytes =
        single_point_bytes + 10U * design_metadata_bytes(&history.states[0]);
    const size_t estimated_bytes = history_estimated_bytes(&history);
    if (!assert_condition(
            estimated_bytes == expected_history_bytes,
            "history_estimated_bytes counts shared SharedPointBuffer only once"
        )) {
        goto cleanup;
    }

    /* Geometry mutation must detach CoW buffer without altering history snapshots. */
    const StitchPoint saved_first_point = history.states[0].points[0];
    rotate_design_90_clockwise(&design);
    if (!assert_condition(
            design.point_buffer != initial_buffer &&
            initial_buffer->ref_count == 10U &&
            history.states[0].points[0].position_x == saved_first_point.position_x &&
            history.states[0].points[0].position_y == saved_first_point.position_y,
            "rotate_design_90_clockwise detaches CoW buffer and preserves history snapshots"
        )) {
        goto cleanup;
    }
    if (!assert_condition(history_undo(&history, &design), "undo restores original unrotated design")) {
        goto cleanup;
    }

    /* 2. SpatialIndex exact equivalence with unindexed ops. */
    if (!assert_condition(
            spatial_index_build(&spatial, &design, grid.cross_pitch_units, &scratch),
            "spatial_index_build succeeds"
        )) {
        goto cleanup;
    }
    const int32_t probe_x = design.minimum_x + 5 * 18 + 9;
    const int32_t probe_y = design.minimum_y + 4 * 18 + 9;
    bool snapped_plain = false;
    bool snapped_indexed = false;
    const CutRectangle box_plain = resolve_cross_box_at_point(&design, grid, probe_x, probe_y, &snapped_plain);
    const CutRectangle box_indexed =
        resolve_cross_box_at_point_indexed(&design, &spatial, grid, probe_x, probe_y, &snapped_indexed);
    if (!assert_condition(
            snapped_plain == snapped_indexed &&
            box_plain.minimum_x == box_indexed.minimum_x &&
            box_plain.maximum_x == box_indexed.maximum_x &&
            box_plain.minimum_y == box_indexed.minimum_y &&
            box_plain.maximum_y == box_indexed.maximum_y,
            "resolve_cross_box_at_point_indexed matches unindexed exactly"
        )) {
        goto cleanup;
    }

    size_t chain_plain = SIZE_MAX;
    size_t chain_indexed = SIZE_MAX;
    const bool found_plain = find_nearest_chain_at_point(&design, probe_x, probe_y, 24, &chain_plain);
    const bool found_indexed = find_nearest_chain_at_point_indexed(&design, &spatial, probe_x, probe_y, 24, &chain_indexed);
    if (!assert_condition(found_plain && found_indexed && chain_plain == chain_indexed, "find_nearest_chain_at_point_indexed matches unindexed")) {
        goto cleanup;
    }

    sel_plain = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
    sel_indexed = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
    if (sel_plain == nullptr || sel_indexed == nullptr) {
        goto cleanup;
    }
    const size_t elem_plain = select_connected_element_in_mask(&design, grid, chain_plain, sel_plain, total_segments, false);
    const size_t elem_indexed = select_connected_element_in_mask_indexed(&design, &spatial, chain_indexed, sel_indexed, total_segments, false);
    if (!assert_condition(
            elem_plain > 0U && elem_plain == elem_indexed && memcmp(sel_plain, sel_indexed, total_segments) == 0,
            "select_connected_element_in_mask_indexed matches unindexed BFS bit-for-bit"
        )) {
        goto cleanup;
    }

    memset(sel_plain, 0, total_segments);
    memset(sel_indexed, 0, total_segments);
    const size_t cross_plain = select_cross_box_in_mask(&design, box_plain, sel_plain, total_segments, false);
    const size_t cross_indexed = select_cross_box_in_mask_indexed(&design, &spatial, box_indexed, sel_indexed, total_segments, false);
    if (!assert_condition(
            cross_plain > 0U && cross_plain == cross_indexed && memcmp(sel_plain, sel_indexed, total_segments) == 0,
            "select_cross_box_in_mask_indexed matches unindexed bit-for-bit"
        )) {
        goto cleanup;
    }

    /* 3. CrossClusterCache exact bit-for-bit match across Cut/Crop/Split. */
    mask_plain = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
    mask_cached = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
    if (mask_plain == nullptr || mask_cached == nullptr) {
        goto cleanup;
    }
    const CutRectangle edit_box = {
        design.minimum_x + 3 * 18,
        design.minimum_y + 2 * 18,
        design.minimum_x + 7 * 18,
        design.minimum_y + 8 * 18
    };
    const EditOperationKind ops[4] = {
        EDIT_CUT_OUT, EDIT_CROP_KEEP, EDIT_SPLIT_VERTICAL, EDIT_SPLIT_HORIZONTAL
    };
    for (size_t op_index = 0U; op_index < 4U; op_index += 1U) {
        if (!assert_condition(
                build_edit_segment_mask(&design, grid, ops[op_index], edit_box, probe_x, false, mask_plain, total_segments) &&
                build_edit_segment_mask_cached(&cross_cache, &design, grid, ops[op_index], edit_box, probe_x, false, mask_cached, total_segments, &scratch) &&
                memcmp(mask_plain, mask_cached, total_segments) == 0,
                "build_edit_segment_mask_cached matches uncached bit-for-bit"
            )) {
            goto cleanup;
        }
    }

    /* 4. Two-layer StitchLayerCache + headless hover update. */
    if (!assert_condition(init_pixel_canvas(&canvas, 1280, 720), "init canvas for Stage 2 render cache test")) {
        goto cleanup;
    }
    if (!assert_condition(clone_stitch_design(&design, &editor.design), "clone design into editor")) {
        goto cleanup;
    }
    editor.grid = grid;
    editor_mark_design_changed(&editor, false);
    fit_camera_to_design(&editor, canvas.width, canvas.height);
    render_editor_frame(&canvas, &editor);
    if (!assert_condition(editor.stitch_cache.valid, "first frame populates StitchLayerCache")) {
        goto cleanup;
    }
    const int32_t hover_screen_x = editor_world_to_screen_x(&editor, canvas.width, canvas.height, probe_x);
    const int32_t hover_screen_y = editor_world_to_screen_y(&editor, canvas.width, canvas.height, probe_y);
    editor_update_canvas_hover(&editor, hover_screen_x, hover_screen_y, canvas.width, canvas.height);
    if (!assert_condition(
            editor.hover_component_id < editor.spatial_index.component_count,
            "Sel Elem hover resolves precomputed component ID without hover_mask allocation"
        )) {
        goto cleanup;
    }
    editor.mouse_screen_x = hover_screen_x;
    editor.mouse_screen_y = hover_screen_y;
    render_editor_frame(&canvas, &editor);

    /* Move hover elsewhere and verify dirty-rect incremental frame matches a fresh full canvas pixel-for-pixel. */
    editor.active_tool = TOOL_SELECT_CROSS;
    clear_hover_preview(&editor);
    editor_update_canvas_hover(&editor, hover_screen_x + 24, hover_screen_y + 12, canvas.width, canvas.height);
    editor.mouse_screen_x = hover_screen_x + 24;
    editor.mouse_screen_y = hover_screen_y + 12;
    render_editor_frame(&canvas, &editor);

    PixelCanvas reference_canvas = {};
    if (!assert_condition(init_pixel_canvas(&reference_canvas, canvas.width, canvas.height), "init reference canvas")) {
        goto cleanup;
    }
    render_editor_frame(&reference_canvas, &editor);
    const size_t frame_bytes = (size_t)canvas.width * (size_t)canvas.height * sizeof(uint32_t);
    const bool pixel_exact = (memcmp(canvas.pixels, reference_canvas.pixels, frame_bytes) == 0);
    free_pixel_canvas(&reference_canvas);
    if (!assert_condition(pixel_exact, "dirty-rect incremental hover frame matches fresh full render pixel-for-pixel")) {
        goto cleanup;
    }

    printf("[PASS] Stage 2 caches, CoW SharedPointBuffer, SpatialIndex, and StitchLayerCache\n");
    passed = true;

cleanup:
    free_pixel_canvas(&canvas);
    free_editor_state(&editor);
    free(sel_indexed);
    free(sel_plain);
    free(mask_cached);
    free(mask_plain);
    cross_cluster_cache_free(&cross_cache);
    spatial_index_free(&spatial);
    scratch_arena_free(&scratch);
    history_clear(&history);
    free_stitch_design(&snapshot_copy);
    free_stitch_design(&design);
    return passed;
}

static bool buttons_overlap(const UiButton *first, const UiButton *second) {
    return first->left_x < second->left_x + second->width &&
           first->left_x + first->width > second->left_x &&
           first->top_y < second->top_y + second->height &&
           first->top_y + first->height > second->top_y;
}

static bool verify_button_group_bounds_and_disjoint(
    const UiFont *font,
    const UiButton *buttons,
    size_t count,
    UiRect container,
    const char *label
) {
    for (size_t first_index = 0U; first_index < count; first_index += 1U) {
        const UiButton *btn = &buttons[first_index];
        if (btn->width <= 0 || btn->height <= 0 ||
            btn->left_x < container.x ||
            btn->top_y < container.y ||
            btn->left_x + btn->width > container.x + container.width ||
            btn->top_y + btn->height > container.y + container.height) {
            fprintf(stderr, "Layout bounds violation in %s (button %zu '%s')\n", label, first_index, btn->label);
            return false;
        }
        const int32_t label_width = measure_ui_text_width(font, btn->label);
        if (label_width > btn->width - 4) {
            fprintf(
                stderr,
                "Label overflow in %s (button %zu '%s': text=%d px > width=%d px)\n",
                label,
                first_index,
                btn->label,
                label_width,
                btn->width
            );
            return false;
        }
        for (size_t second_index = first_index + 1U; second_index < count; second_index += 1U) {
            if (buttons_overlap(btn, &buttons[second_index])) {
                fprintf(
                    stderr,
                    "Layout overlap in %s ('%s' vs '%s')\n",
                    label,
                    btn->label,
                    buttons[second_index].label
                );
                return false;
            }
        }
    }
    return true;
}

static size_t count_antialiased_pixels(
    const PixelCanvas *canvas,
    uint32_t background_pixel,
    uint32_t foreground_pixel
) {
    size_t aa_count = 0U;
    const size_t total_pixels = (size_t)canvas->width * (size_t)canvas->height;
    for (size_t pixel_index = 0U; pixel_index < total_pixels; pixel_index += 1U) {
        const uint32_t pixel = canvas->pixels[pixel_index];
        if (pixel != background_pixel && pixel != foreground_pixel) {
            aa_count += 1U;
        }
    }
    return aa_count;
}

static bool test_stage3_headless_layouts_and_cyrillic(void) {
    bool passed = false;
    EditorState state = {};
    PixelCanvas canvas = {};
    UiFont fallback_font = {};
    init_default_editor_state(&state);

    if (!build_synthetic_cross_grid_design(&state.design)) {
        goto cleanup;
    }
    state.grid = detect_cross_stitch_grid(&state.design);
    editor_mark_design_changed(&state, true);
    (void)snprintf(state.current_file_path, sizeof(state.current_file_path), "/tmp/Вишиванка_Ґанок_Їжачок.jef");
    state.has_file_path = true;
    set_status_message(&state, "Готово: Вишивка Єднає Її Ґанок!");

    const int32_t widths[3] = {1920, 1280, 960};
    const int32_t heights[3] = {1080, 720, 1080};

    for (size_t res_index = 0U; res_index < 3U; res_index += 1U) {
        const int32_t width = widths[res_index];
        const int32_t height = heights[res_index];
        free_pixel_canvas(&canvas);
        if (!assert_condition(init_pixel_canvas(&canvas, width, height), "init layout test canvas")) {
            goto cleanup;
        }

        for (size_t collapse_step = 0U; collapse_step < 2U; collapse_step += 1U) {
            state.right_panel_collapsed = (collapse_step == 1U);
            const EditorLayout layout = compute_editor_layout(&state, width, height);

            if (!assert_condition(
                    layout.top_bar.height == TOP_BAR_HEIGHT &&
                    layout.left_rail.width == LEFT_RAIL_WIDTH &&
                    layout.bottom_bar.height == BOTTOM_BAR_HEIGHT,
                    "panel heights and widths match compact spec"
                )) {
                goto cleanup;
            }
            if (!assert_condition(
                    layout.viewport.width >= 600 && layout.viewport.height >= 600,
                    "viewport retains generous area across 1920x1080, 1280x720, 960x1080"
                )) {
                goto cleanup;
            }
            if (state.right_panel_collapsed) {
                if (!assert_condition(
                        layout.right_panel.width == 0 && layout.viewport.width == width - LEFT_RAIL_WIDTH,
                        "collapsed right panel gives full remaining width to viewport"
                    )) {
                    goto cleanup;
                }
            } else {
                if (!assert_condition(
                        layout.right_panel.width == RIGHT_PANEL_WIDTH &&
                        layout.prop_button_count >= 12U &&
                        verify_button_group_bounds_and_disjoint(&state.ui_font, layout.prop_buttons, layout.prop_button_count, layout.right_panel, "right_panel"),
                        "right panel prop buttons fit inside panel with zero overlap or text overflow"
                    )) {
                    goto cleanup;
                }
            }
            if (!assert_condition(
                    layout.top_button_count >= 11U &&
                    verify_button_group_bounds_and_disjoint(&state.ui_font, layout.top_buttons, layout.top_button_count, layout.top_bar, "top_bar"),
                    "single-row top bar buttons fit with zero overflow and zero overlap"
                )) {
                goto cleanup;
            }
            if (!assert_condition(
                    layout.rail_button_count == 15U &&
                    verify_button_group_bounds_and_disjoint(&state.ui_font, layout.rail_buttons, layout.rail_button_count, layout.left_rail, "left_rail"),
                    "left tool rail buttons (9 tools + 4 transforms + 2 actions) fit cleanly"
                )) {
                goto cleanup;
            }
            if (!assert_condition(
                    layout.slider_track.width >= 80 &&
                    layout.slider_track.y >= layout.bottom_bar.y &&
                    layout.slider_track.y + layout.slider_track.height <= layout.bottom_bar.y + layout.bottom_bar.height,
                    "bottom simulator slider fits inside bottom bar"
                )) {
                goto cleanup;
            }

            /* Exercise full render including Cut box dimensions badge and Ukrainian Cyrillic status. */
            state.active_tool = TOOL_CUT_OUT;
            state.has_box_selection = true;
            state.is_dragging_box = true;
            state.box_start_x = state.design.minimum_x;
            state.box_start_y = state.design.minimum_y;
            state.box_end_x = state.design.minimum_x + 18 * 6;
            state.box_end_y = state.design.minimum_y + 18 * 4;
            fit_camera_to_design(&state, width, height);
            render_editor_frame(&canvas, &state);
            state.is_dragging_box = false;
            state.has_box_selection = false;
        }
    }

    /* Verify antialiased FreeType Latin + Ukrainian Cyrillic measurement and rendering. */
    free_pixel_canvas(&canvas);
    if (!assert_condition(init_pixel_canvas(&canvas, 480, 64), "init Cyrillic text canvas")) {
        goto cleanup;
    }
    const char *ukrainian_sample = "Вишивка Є Ї Ґ є і ї ґ";
    const int32_t measured_width = measure_ui_text_width(&state.ui_font, ukrainian_sample);
    if (!assert_condition(
            measured_width >= 100 && measured_width <= 420,
            "UTF-8 Cyrillic string measures to reasonable proportional width"
        )) {
        goto cleanup;
    }
    const uint32_t bg = pack_rgb_pixel(20U, 22U, 26U);
    const uint32_t fg = pack_rgb_pixel(255U, 255U, 255U);
    draw_filled_rectangle(&canvas, 0, 0, canvas.width, canvas.height, bg);
    draw_ui_text_vcentered(&canvas, &state.ui_font, 10, 0, canvas.height, ukrainian_sample, fg);
    const size_t lit_pixels = count_non_background_pixels(&canvas, bg);
    const size_t aa_pixels = count_antialiased_pixels(&canvas, bg, fg);
    if (!assert_condition(lit_pixels > 400U, "Ukrainian Cyrillic glyphs render visible pixels")) {
        goto cleanup;
    }
    if (state.ui_font.using_freetype) {
        if (!assert_condition(
                aa_pixels > 200U,
                "FreeType UI font produces smooth antialiased edge pixels"
            )) {
            goto cleanup;
        }
    }

    /* Verify HiDPI 2.0x scaling rebakes native 32px glyphs and keeps layout clean. */
    const int32_t base_cap_height = state.ui_font.cap_height;
    state.right_panel_collapsed = false;
    editor_set_ui_scale(&state, 2.0);
    if (!assert_condition(
            state.ui_font.pixel_size == UI_FONT_BASE_PIXEL_SIZE * 2 &&
            state.ui_font.cap_height > base_cap_height,
            "HiDPI 2x scales native font rasterization and cap height"
        )) {
        goto cleanup;
    }
    const EditorLayout hidpi_layout = compute_editor_layout(&state, 3840, 2160);
    if (!assert_condition(
            hidpi_layout.top_bar.height == TOP_BAR_HEIGHT * 2 &&
            hidpi_layout.left_rail.width == LEFT_RAIL_WIDTH * 2 &&
            hidpi_layout.right_panel.width == RIGHT_PANEL_WIDTH * 2 &&
            hidpi_layout.bottom_bar.height == BOTTOM_BAR_HEIGHT * 2 &&
            verify_button_group_bounds_and_disjoint(&state.ui_font, hidpi_layout.top_buttons, hidpi_layout.top_button_count, hidpi_layout.top_bar, "hidpi_top_bar") &&
            verify_button_group_bounds_and_disjoint(&state.ui_font, hidpi_layout.rail_buttons, hidpi_layout.rail_button_count, hidpi_layout.left_rail, "hidpi_left_rail") &&
            verify_button_group_bounds_and_disjoint(&state.ui_font, hidpi_layout.prop_buttons, hidpi_layout.prop_button_count, hidpi_layout.right_panel, "hidpi_right_panel"),
            "HiDPI 2x layout scales panels and fits all labels with zero overflow"
        )) {
        goto cleanup;
    }

    /* Verify built-in 8x8 bitmap fallback when no system font path is available. */
    const bool loaded_missing =
        init_ui_font_with_path(&fallback_font, "/nonexistent/embroidery_missing_font.ttf", UI_FONT_BASE_PIXEL_SIZE);
    if (!assert_condition(
            !loaded_missing && !fallback_font.using_freetype && fallback_font.alpha_pixels == nullptr,
            "missing font path cleanly activates built-in 8x8 fallback without errors"
        )) {
        goto cleanup;
    }
    if (!assert_condition(
            measure_ui_text_width(&fallback_font, ukrainian_sample) == 21 * 14,
            "fallback 8x8 font measures UTF-8 Ukrainian text by codepoint count"
        )) {
        goto cleanup;
    }
    draw_filled_rectangle(&canvas, 0, 0, canvas.width, canvas.height, bg);
    draw_ui_text(&canvas, &fallback_font, 10, 16, ukrainian_sample, fg);
    const size_t fallback_pixels = count_non_background_pixels(&canvas, bg);
    if (!assert_condition(fallback_pixels > 500U, "fallback 8x8 font renders visible Ukrainian glyphs")) {
        goto cleanup;
    }

    printf(
        "[PASS] Stage 3 layouts (1920x1080, 1280x720, 960x1080, HiDPI 2x) + UI font (ft=%d, lit=%zu px, aa=%zu px, fallback=%zu px)\n",
        (int)state.ui_font.using_freetype,
        lit_pixels,
        aa_pixels,
        fallback_pixels
    );
    passed = true;

cleanup:
    free_ui_font(&fallback_font);
    free_pixel_canvas(&canvas);
    free_editor_state(&state);
    return passed;
}

static bool is_valid_sdl_dialog_filter_char(char character) {
    const bool is_lower = (character >= 'a' && character <= 'z');
    const bool is_upper = (character >= 'A' && character <= 'Z');
    const bool is_digit = (character >= '0' && character <= '9');
    const bool is_punct = (character == '_' || character == '.' || character == '-');
    return is_lower || is_upper || is_digit || is_punct;
}

/* Matches SDL3 SDL_DialogFileFilter validation: single "*" or ";"-delimited [a-zA-Z0-9_.-]+ tokens. */
static bool is_valid_sdl_dialog_filter_pattern(const char *pattern) {
    if (pattern == nullptr || pattern[0] == '\0') {
        return false;
    }
    if (strcmp(pattern, "*") == 0) {
        return true;
    }

    size_t token_length = 0U;
    for (size_t char_index = 0U; pattern[char_index] != '\0'; char_index += 1U) {
        const char character = pattern[char_index];
        if (character == ';') {
            if (token_length == 0U) {
                return false;
            }
            token_length = 0U;
            continue;
        }
        if (!is_valid_sdl_dialog_filter_char(character)) {
            return false;
        }
        token_length += 1U;
    }
    return token_length > 0U;
}

static bool test_sdl_dialog_file_filters(void) {
    for (size_t filter_index = 0U; filter_index < OPEN_DIALOG_FILTER_COUNT; filter_index += 1U) {
        const DialogFilterSpec spec = open_dialog_filters[filter_index];
        if (!assert_condition(
                spec.name != nullptr && spec.name[0] != '\0' &&
                is_valid_sdl_dialog_filter_pattern(spec.pattern),
                "open dialog filter obeys SDL3 SDL_DialogFileFilter pattern rules"
            )) {
            return false;
        }
    }
    for (size_t filter_index = 0U; filter_index < SAVE_DIALOG_FILTER_COUNT; filter_index += 1U) {
        const DialogFilterSpec spec = save_dialog_filters[filter_index];
        if (!assert_condition(
                spec.name != nullptr && spec.name[0] != '\0' &&
                is_valid_sdl_dialog_filter_pattern(spec.pattern),
                "save dialog filter obeys SDL3 SDL_DialogFileFilter pattern rules"
            )) {
            return false;
        }
    }

    if (!assert_condition(
            strcmp(open_dialog_filters[0].name, "Embroidery files") == 0 &&
            strcmp(open_dialog_filters[0].pattern, "jef;pes;pec;dst") == 0 &&
            strcmp(open_dialog_filters[1].name, "All files") == 0 &&
            strcmp(open_dialog_filters[1].pattern, "*") == 0,
            "open dialog filters match expected Embroidery files + All files specs"
        )) {
        return false;
    }
    if (!assert_condition(
            strcmp(save_dialog_filters[0].name, "JEF embroidery") == 0 &&
            strcmp(save_dialog_filters[0].pattern, "jef") == 0 &&
            strcmp(save_dialog_filters[1].name, "All files") == 0 &&
            strcmp(save_dialog_filters[1].pattern, "*") == 0,
            "save dialog filters match expected JEF embroidery + All files specs"
        )) {
        return false;
    }

    static const char *const invalid_patterns[9] = {
        nullptr,
        "",
        "jef;jef+;pes;pec;dst",
        ";jef",
        "jef;",
        "jef;;pes",
        "*.jef",
        "*;jef",
        "jef pes"
    };
    for (size_t case_index = 0U; case_index < 9U; case_index += 1U) {
        if (!assert_condition(
                !is_valid_sdl_dialog_filter_pattern(invalid_patterns[case_index]),
                "SDL3 dialog filter validator rejects invalid pattern"
            )) {
            return false;
        }
    }

    printf("[PASS] SDL3 dialog file filter pattern validation\n");
    return true;
}

int main(void) {
    if (!test_invalid_and_corrupt_inputs()) {
        return EXIT_FAILURE;
    }
    if (!test_synthetic_format_parsers()) {
        return EXIT_FAILURE;
    }
    if (!test_core_editing_workflows()) {
        return EXIT_FAILURE;
    }
    if (!test_draw_x_and_unique_paths()) {
        return EXIT_FAILURE;
    }
    if (!test_stage1_security_and_correctness_regressions()) {
        return EXIT_FAILURE;
    }
    if (!test_headless_render_modes()) {
        return EXIT_FAILURE;
    }
    if (!test_stage2_caches_and_cow_history()) {
        return EXIT_FAILURE;
    }
    if (!test_stage3_headless_layouts_and_cyrillic()) {
        return EXIT_FAILURE;
    }
    if (!test_sdl_dialog_file_filters()) {
        return EXIT_FAILURE;
    }
    if (!test_optional_real_directory()) {
        return EXIT_FAILURE;
    }
    printf("ALL EMBROIDERY STUDIO C23 TESTS PASSED\n");
    return EXIT_SUCCESS;
}
