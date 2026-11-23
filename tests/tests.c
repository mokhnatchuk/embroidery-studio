#define _POSIX_C_SOURCE 200809L

#include "core.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
            delete_masked_segments(&original, segment_mask, total_segments, &edited),
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
            "Sel Elem recolor to Janome index 12"
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
            delete_masked_segments(&original, segment_mask, total_segments, &edited),
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
    if (!test_optional_real_directory()) {
        return EXIT_FAILURE;
    }
    printf("ALL EMBROIDERY STUDIO C23 TESTS PASSED\n");
    return EXIT_SUCCESS;
}
