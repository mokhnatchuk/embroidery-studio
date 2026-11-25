#define _POSIX_C_SOURCE 200809L

#include "formats.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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

static void log_format_error(const char *message) {
#ifndef EMBROIDERY_QUIET_ERRORS
    (void)fprintf(stderr, "%s\n", message);
#else
    (void)message;
#endif
}

static void log_format_errno_error(const char *prefix, int error_number) {
#ifndef EMBROIDERY_QUIET_ERRORS
    (void)fprintf(stderr, "%s: %s\n", prefix, strerror(error_number));
#else
    (void)prefix;
    (void)error_number;
#endif
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
    int64_t next_x = 0LL;
    int64_t next_y = 0LL;
    if (ckd_add(&next_x, builder->current_x, delta_x) ||
        ckd_add(&next_y, builder->current_y, delta_y) ||
        next_x < -MAX_COORDINATE_MAGNITUDE || next_x > MAX_COORDINATE_MAGNITUDE ||
        next_y < -MAX_COORDINATE_MAGNITUDE || next_y > MAX_COORDINATE_MAGNITUDE) {
        log_format_error("Stitch coordinates out of valid range");
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
    int64_t next_x = 0LL;
    int64_t next_y = 0LL;
    if (ckd_add(&next_x, builder->current_x, delta_x) ||
        ckd_add(&next_y, builder->current_y, delta_y) ||
        next_x < -MAX_COORDINATE_MAGNITUDE || next_x > MAX_COORDINATE_MAGNITUDE ||
        next_y < -MAX_COORDINATE_MAGNITUDE || next_y > MAX_COORDINATE_MAGNITUDE) {
        log_format_error("Stitch coordinates out of valid range");
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

EmbroideryFormat detect_embroidery_format(
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
    if (signed_index < 0LL) {
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

bool parse_jef_design(
    const uint8_t *file_buffer,
    size_t file_size,
    StitchDesign *design
) {
    if (file_size < JEF_HEADER_SIZE) {
        log_format_error("Invalid JEF file size");
        return false;
    }

    const uint32_t stitch_offset = read_uint32_little_endian(file_buffer, 0U);
    uint32_t color_count = read_uint32_little_endian(file_buffer, 24U);
    if (stitch_offset < JEF_HEADER_SIZE || (size_t)stitch_offset >= file_size) {
        log_format_error("Invalid JEF stitch offset");
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
        const uint32_t high_nibble = (uint32_t)(lead_byte & 0x0FU) << 8U;
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
        log_format_error("PEC section is truncated or missing stitch data");
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

        int64_t delta_x = 0LL;
        int64_t delta_y = 0LL;
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

bool parse_pes_design(
    const uint8_t *file_buffer,
    size_t file_size,
    StitchDesign *design
) {
    if (file_size < 12U || memcmp(file_buffer, "#PES", 4U) != 0) {
        log_format_error("Invalid PES header signature");
        return false;
    }

    const uint32_t pec_offset = read_uint32_little_endian(file_buffer, 8U);
    if ((size_t)pec_offset >= file_size || file_size - (size_t)pec_offset <= PEC_HEADER_SIZE) {
        log_format_error("Invalid PEC offset in PES file");
        return false;
    }

    return parse_pec_section_design(file_buffer, file_size, (size_t)pec_offset, design);
}

bool parse_pec_design(
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
        log_format_error("Invalid PEC header signature");
        return false;
    }

    return parse_pec_section_design(file_buffer, file_size, pec_offset, design);
}

bool parse_dst_design(
    const uint8_t *file_buffer,
    size_t file_size,
    StitchDesign *design
) {
    if (file_size < DST_HEADER_SIZE + 3U) {
        log_format_error("Invalid DST file size");
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

        int64_t delta_x = 0LL;
        int64_t delta_y = 0LL;

        if ((first_byte & 0x01U) != 0U) {
            delta_x += 1LL;
        }
        if ((first_byte & 0x02U) != 0U) {
            delta_x -= 1LL;
        }
        if ((first_byte & 0x04U) != 0U) {
            delta_x += 9LL;
        }
        if ((first_byte & 0x08U) != 0U) {
            delta_x -= 9LL;
        }
        if ((first_byte & 0x80U) != 0U) {
            delta_y -= 1LL;
        }
        if ((first_byte & 0x40U) != 0U) {
            delta_y += 1LL;
        }
        if ((first_byte & 0x20U) != 0U) {
            delta_y -= 9LL;
        }
        if ((first_byte & 0x10U) != 0U) {
            delta_y += 9LL;
        }

        if ((second_byte & 0x01U) != 0U) {
            delta_x += 3LL;
        }
        if ((second_byte & 0x02U) != 0U) {
            delta_x -= 3LL;
        }
        if ((second_byte & 0x04U) != 0U) {
            delta_x += 27LL;
        }
        if ((second_byte & 0x08U) != 0U) {
            delta_x -= 27LL;
        }
        if ((second_byte & 0x80U) != 0U) {
            delta_y -= 3LL;
        }
        if ((second_byte & 0x40U) != 0U) {
            delta_y += 3LL;
        }
        if ((second_byte & 0x20U) != 0U) {
            delta_y -= 27LL;
        }
        if ((second_byte & 0x10U) != 0U) {
            delta_y += 27LL;
        }

        if ((third_byte & 0x04U) != 0U) {
            delta_x += 81LL;
        }
        if ((third_byte & 0x08U) != 0U) {
            delta_x -= 81LL;
        }
        if ((third_byte & 0x20U) != 0U) {
            delta_y -= 81LL;
        }
        if ((third_byte & 0x10U) != 0U) {
            delta_y += 81LL;
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

bool load_embroidery_file(const char *input_path, StitchDesign *design) {
    bool succeeded = false;
    FILE *input_file = nullptr;
    uint8_t *file_buffer = nullptr;

    init_stitch_design(design);

    input_file = fopen(input_path, "rb");
    if (input_file == nullptr) {
        log_format_errno_error("Failed to open input file", errno);
        goto cleanup;
    }

    const int file_descriptor = fileno(input_file);
    struct stat file_stat = {};
    if (file_descriptor < 0 || fstat(file_descriptor, &file_stat) != 0) {
        log_format_error("Failed to inspect input file metadata");
        goto cleanup;
    }
    if (!S_ISREG(file_stat.st_mode)) {
        log_format_error("Input path is not a regular file");
        goto cleanup;
    }
    if (file_stat.st_size <= 0 ||
        (uint64_t)file_stat.st_size > (uint64_t)MAX_INPUT_FILE_SIZE) {
        log_format_error("Invalid input file size");
        goto cleanup;
    }

    const size_t file_size = (size_t)file_stat.st_size;
    file_buffer = (uint8_t *)malloc(file_size);
    if (file_buffer == nullptr) {
        log_format_error("Failed to allocate input buffer");
        goto cleanup;
    }

    const size_t bytes_read = fread(file_buffer, 1U, file_size, input_file);
    if (bytes_read != file_size) {
        log_format_error("Failed to read input file");
        goto cleanup;
    }
    if (fclose(input_file) != 0) {
        input_file = nullptr;
        log_format_error("Failed to close input file");
        goto cleanup;
    }
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
        log_format_error("Unsupported embroidery format");
        goto cleanup;
    }

    if (!parse_succeeded) {
        goto cleanup;
    }

    center_stitch_design(design);
    if (!design->has_bounds || design->chain_count == 0U) {
        log_format_error("No drawable stitches found");
        goto cleanup;
    }

    succeeded = true;

cleanup:
    if (input_file != nullptr) {
        (void)fclose(input_file);
    }
    free(file_buffer);
    if (!succeeded) {
        free_stitch_design(design);
    }
    return succeeded;
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
            log_format_error("Failed to grow JEF export buffer");
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

    const int64_t step_count = (dominant_delta + JEF_MAX_STEP - 1LL) / JEF_MAX_STEP;
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

/* Select standard JEF hoop code from design extents in 0.1 mm. */
static uint32_t select_jef_hoop_code(
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
    /* Extended JEF hoop code for large hoops (200x280 mm and 200x360 mm). */
    return 38U;
}

/* Group drawable chains contiguously by color block without mutating the source design. */
static bool build_grouped_export_chain_order(
    const StitchDesign *design,
    size_t *ordered_indices,
    size_t ordered_capacity,
    size_t *ordered_count_out
) {
    bool succeeded = false;
    size_t *block_counts = nullptr;
    size_t *block_offsets = nullptr;

    *ordered_count_out = 0U;

    size_t count_bytes = 0U;
    if (ordered_indices == nullptr || ordered_capacity < design->chain_count ||
        ckd_mul(&count_bytes, design->block_count, sizeof(size_t))) {
        goto cleanup;
    }

    block_counts = (size_t *)calloc(design->block_count, sizeof(size_t));
    block_offsets = (size_t *)calloc(design->block_count, sizeof(size_t));
    if (block_counts == nullptr || block_offsets == nullptr) {
        log_format_error("Failed to allocate export chain ordering buffers");
        goto cleanup;
    }

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index < design->block_count &&
            design->blocks[chain.color_block_index].enabled &&
            chain.point_count >= 2U) {
            block_counts[chain.color_block_index] += 1U;
        }
    }

    size_t running_offset = 0U;
    for (size_t block_index = 0U; block_index < design->block_count; block_index += 1U) {
        block_offsets[block_index] = running_offset;
        if (ckd_add(&running_offset, running_offset, block_counts[block_index])) {
            goto cleanup;
        }
    }
    if (running_offset > ordered_capacity) {
        goto cleanup;
    }

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index < design->block_count &&
            design->blocks[chain.color_block_index].enabled &&
            chain.point_count >= 2U) {
            const size_t write_slot = block_offsets[chain.color_block_index];
            if (write_slot >= ordered_capacity) {
                goto cleanup;
            }
            ordered_indices[write_slot] = chain_index;
            block_offsets[chain.color_block_index] = write_slot + 1U;
        }
    }

    *ordered_count_out = running_offset;
    succeeded = true;

cleanup:
    free(block_offsets);
    free(block_counts);
    return succeeded;
}

bool write_jef_file(const StitchDesign *design, const char *output_path) {
    bool succeeded = false;
    FILE *output_file = nullptr;
    bool temp_file_created = false;
    char temp_path[MAX_PATH_BUFFER] = {};
    uint8_t *header_buffer = nullptr;
    size_t *active_blocks = nullptr;
    size_t *ordered_chains = nullptr;
    size_t ordered_chain_count = 0U;
    ByteBuffer stitch_stream = {};

    if (design->block_count == 0U || design->chain_count == 0U) {
        log_format_error("Cannot export empty embroidery design");
        goto cleanup;
    }

    size_t ordered_chain_bytes = 0U;
    if (ckd_mul(&ordered_chain_bytes, design->chain_count, sizeof(size_t))) {
        goto cleanup;
    }
    ordered_chains = (size_t *)calloc(design->chain_count, sizeof(size_t));
    if (ordered_chains == nullptr) {
        log_format_error("Failed to allocate export chain ordering buffer");
        goto cleanup;
    }

    if (!build_grouped_export_chain_order(
            design,
            ordered_chains,
            design->chain_count,
            &ordered_chain_count
        )) {
        goto cleanup;
    }
    if (ordered_chain_count == 0U || ordered_chain_count > design->chain_count) {
        log_format_error("No drawable stitches to export");
        goto cleanup;
    }

    size_t active_block_bytes = 0U;
    if (ckd_mul(&active_block_bytes, design->block_count, sizeof(size_t))) {
        goto cleanup;
    }
    active_blocks = (size_t *)malloc(active_block_bytes);
    if (active_blocks == nullptr) {
        log_format_error("Failed to allocate active block list");
        goto cleanup;
    }

    size_t exported_block_count = 0U;
    bool has_export_bounds = false;
    int64_t screen_min_x = 0LL;
    int64_t screen_max_x = 0LL;
    int64_t screen_min_y = 0LL;
    int64_t screen_max_y = 0LL;
    size_t previous_seen_block = SIZE_MAX;

    for (size_t order_slot = 0U;
         order_slot < ordered_chain_count && order_slot < design->chain_count;
         order_slot += 1U) {
        const size_t chain_index = ordered_chains[order_slot];
        if (chain_index >= design->chain_count) {
            goto cleanup;
        }
        const StitchChain chain = design->chains[chain_index];
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
            if (exported_block_count >= design->block_count ||
                exported_block_count >= MAX_COLOR_BLOCKS) {
                log_format_error("Exported color block count exceeds limit");
                goto cleanup;
            }
            active_blocks[exported_block_count] = chain.color_block_index;
            exported_block_count += 1U;
            previous_seen_block = chain.color_block_index;
        }
    }

    if (!has_export_bounds || exported_block_count == 0U ||
        exported_block_count > MAX_COLOR_BLOCKS) {
        log_format_error("No drawable stitches to export");
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
        select_jef_hoop_code(extent_left, extent_top, extent_right, extent_bottom);

    int64_t needle_x = 0LL;
    int64_t needle_y = 0LL;
    size_t emitted_block_index = SIZE_MAX;

    for (size_t order_slot = 0U;
         order_slot < ordered_chain_count && order_slot < design->chain_count;
         order_slot += 1U) {
        const size_t chain_index = ordered_chains[order_slot];
        if (chain_index >= design->chain_count) {
            goto cleanup;
        }
        const StitchChain chain = design->chains[chain_index];
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
        log_format_error("Exported JEF stitch stream exceeds format limit");
        goto cleanup;
    }

    size_t color_table_bytes = 0U;
    size_t header_size = 0U;
    if (ckd_mul(&color_table_bytes, exported_block_count, (size_t)8U) ||
        ckd_add(&header_size, JEF_HEADER_SIZE, color_table_bytes)) {
        log_format_error("JEF header size overflow");
        goto cleanup;
    }
    header_buffer = (uint8_t *)calloc(1U, header_size);
    if (header_buffer == nullptr) {
        log_format_error("Failed to allocate JEF header buffer");
        goto cleanup;
    }

    write_uint32_little_endian(header_buffer, 0U, (uint32_t)header_size);
    write_uint32_little_endian(header_buffer, 4U, 10U);

    char date_string[16] = "20260101000000";
    const time_t current_time = time(nullptr);
    struct tm local_time_buffer = {};
    if (localtime_r(&current_time, &local_time_buffer) != nullptr) {
        const int written_characters = snprintf(
            date_string,
            sizeof(date_string),
            "%04d%02d%02d%02d%02d%02d",
            local_time_buffer.tm_year + 1900,
            local_time_buffer.tm_mon + 1,
            local_time_buffer.tm_mday,
            local_time_buffer.tm_hour,
            local_time_buffer.tm_min,
            local_time_buffer.tm_sec
        );
        if (written_characters < 0 || (size_t)written_characters >= sizeof(date_string)) {
            memcpy(date_string, "20260101000000", 15U);
        }
    }
    for (size_t date_index = 0U; date_index < 14U; date_index += 1U) {
        header_buffer[8U + date_index] = (uint8_t)date_string[date_index];
    }

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

    /* Atomic write via temp file in the same directory to avoid EXDEV on USB drives. */
    const int temp_written = snprintf(
        temp_path,
        sizeof(temp_path),
        "%s.tmp.%ld",
        output_path,
        (long)getpid()
    );
    if (temp_written <= 0 || (size_t)temp_written >= sizeof(temp_path)) {
        log_format_error("Temporary output path exceeds maximum length");
        goto cleanup;
    }

    output_file = fopen(temp_path, "wb");
    if (output_file == nullptr) {
        log_format_errno_error("Failed to open temporary JEF output file", errno);
        goto cleanup;
    }
    temp_file_created = true;

    if (fwrite(header_buffer, 1U, header_size, output_file) != header_size) {
        log_format_error("Failed to write JEF header");
        goto cleanup;
    }
    if (fwrite(stitch_stream.bytes, 1U, stitch_stream.byte_count, output_file) !=
        stitch_stream.byte_count) {
        log_format_error("Failed to write JEF stitch data");
        goto cleanup;
    }
    if (fflush(output_file) != 0) {
        log_format_errno_error("Failed to flush JEF output file", errno);
        goto cleanup;
    }
    const int output_descriptor = fileno(output_file);
    if (output_descriptor < 0 || fsync(output_descriptor) != 0) {
        log_format_errno_error("Failed to sync JEF output file", errno);
        goto cleanup;
    }
    if (fclose(output_file) != 0) {
        output_file = nullptr;
        log_format_errno_error("Failed to close JEF output file", errno);
        goto cleanup;
    }
    output_file = nullptr;

    if (rename(temp_path, output_path) != 0) {
        log_format_errno_error("Failed to finalize JEF output file", errno);
        goto cleanup;
    }
    temp_file_created = false;
    succeeded = true;

cleanup:
    if (output_file != nullptr) {
        (void)fclose(output_file);
    }
    if (temp_file_created) {
        (void)remove(temp_path);
    }
    free(header_buffer);
    free(active_blocks);
    free(ordered_chains);
    free(stitch_stream.bytes);
    return succeeded;
}

bool build_suffixed_jef_path(
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
    if (stem_length > (size_t)INT_MAX) {
        log_format_error("Base path exceeds maximum length");
        return false;
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
        log_format_error("Output path exceeds maximum length");
        return false;
    }
    return true;
}

bool file_already_exists(const char *file_path) {
    struct stat path_info = {};
    return stat(file_path, &path_info) == 0;
}

bool build_unique_suffixed_jef_path(
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
        char numbered_suffix[64] = {};
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
