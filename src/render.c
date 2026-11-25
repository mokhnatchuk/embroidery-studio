#include "render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

const HoopPreset standard_hoop_presets[HOOP_PRESET_COUNT] = {
    {"Off", 0, 0},
    {"140x140", 1400, 1400},
    {"140x200", 1400, 2000},
    {"200x200", 2000, 2000},
    {"200x280", 2000, 2800},
    {"200x360", 2000, 3600}
};

const double thread_thickness_units[THREAD_THICKNESS_PRESET_COUNT] = {
    3.5, 4.5, 6.0
};

const char *const thread_thickness_labels[THREAD_THICKNESS_PRESET_COUNT] = {
    "Thk:0.35", "Thk:0.45", "Thk:0.60"
};

bool init_pixel_canvas(PixelCanvas *canvas, int32_t width, int32_t height) {
    *canvas = (PixelCanvas){};
    if (width <= 0 || height <= 0) {
        return false;
    }
    size_t pixel_count = 0U;
    size_t pixel_bytes = 0U;
    if (ckd_mul(&pixel_count, (size_t)width, (size_t)height) ||
        ckd_mul(&pixel_bytes, pixel_count, sizeof(uint32_t))) {
        return false;
    }
    uint32_t *pixels = (uint32_t *)calloc(pixel_count, sizeof(uint32_t));
    if (pixels == nullptr) {
        return false;
    }
    canvas->pixels = pixels;
    canvas->width = width;
    canvas->height = height;
    reset_canvas_clip(canvas);
    return true;
}

void free_pixel_canvas(PixelCanvas *canvas) {
    free(canvas->pixels);
    *canvas = (PixelCanvas){};
}

bool init_block_dedup_table(BlockDedupTable *table) {
    *table = (BlockDedupTable){};
    size_t table_bytes = 0U;
    if (ckd_mul(&table_bytes, BLOCK_DEDUP_CAPACITY, sizeof(BlockDedupSlot))) {
        return false;
    }
    BlockDedupSlot *slots = (BlockDedupSlot *)calloc(BLOCK_DEDUP_CAPACITY, sizeof(BlockDedupSlot));
    if (slots == nullptr) {
        return false;
    }
    table->slots = slots;
    table->generation = 0U;
    return true;
}

void free_block_dedup_table(BlockDedupTable *table) {
    free(table->slots);
    *table = (BlockDedupTable){};
}

uint32_t pack_rgb_pixel(uint8_t red, uint8_t green, uint8_t blue) {
    return (0xFFU << 24U) | ((uint32_t)red << 16U) | ((uint32_t)green << 8U) | (uint32_t)blue;
}

uint32_t compute_rgb_luminance(RgbColor color) {
    return ((uint32_t)color.red * 299U + (uint32_t)color.green * 587U + (uint32_t)color.blue * 114U) / 1000U;
}

static uint32_t make_dimmed_color(RgbColor color) {
    const uint8_t red = (uint8_t)((uint32_t)color.red / 5U + 190U);
    const uint8_t green = (uint8_t)((uint32_t)color.green / 5U + 190U);
    const uint8_t blue = (uint8_t)((uint32_t)color.blue / 5U + 190U);
    return pack_rgb_pixel(red, green, blue);
}

void compute_thread_colors(
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

int32_t compute_thread_stroke_px(size_t thread_thickness_index, double pixels_per_unit) {
    size_t safe_index = thread_thickness_index;
    if (safe_index >= THREAD_THICKNESS_PRESET_COUNT) {
        safe_index = 1U;
    }
    const double raw_width = thread_thickness_units[safe_index] * pixels_per_unit;
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

void reset_canvas_clip(PixelCanvas *canvas) {
    canvas->clip_min_x = 0;
    canvas->clip_min_y = 0;
    canvas->clip_max_x = canvas->width;
    canvas->clip_max_y = canvas->height;
}

void put_pixel_clipped(
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

void blend_pixel_clipped(
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
    const uint32_t background_red = (existing >> 16U) & 0xFFU;
    const uint32_t background_green = (existing >> 8U) & 0xFFU;
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

void draw_filled_rectangle(
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

void draw_line_clipped(
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

void draw_thick_line_clipped(
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

void draw_rectangle_outline(
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

void draw_thread_capsule_clipped(
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

int32_t viewport_world_to_screen_x(RenderViewport viewport, CameraView camera, int32_t world_x) {
    const double center_x = (double)viewport.left + (double)viewport.width * 0.5;
    return (int32_t)llround(center_x + ((double)world_x - camera.center_x) * camera.pixels_per_unit);
}

int32_t viewport_world_to_screen_y(RenderViewport viewport, CameraView camera, int32_t world_y) {
    const double center_y = (double)viewport.top + (double)viewport.height * 0.5;
    return (int32_t)llround(center_y + ((double)world_y - camera.center_y) * camera.pixels_per_unit);
}

int32_t viewport_screen_to_world_x(RenderViewport viewport, CameraView camera, int32_t screen_x) {
    double safe_scale = camera.pixels_per_unit;
    if (safe_scale <= 0.0001) {
        safe_scale = 0.5;
    }
    const double center_x = (double)viewport.left + (double)viewport.width * 0.5;
    return (int32_t)llround(camera.center_x + ((double)screen_x - center_x) / safe_scale);
}

int32_t viewport_screen_to_world_y(RenderViewport viewport, CameraView camera, int32_t screen_y) {
    double safe_scale = camera.pixels_per_unit;
    if (safe_scale <= 0.0001) {
        safe_scale = 0.5;
    }
    const double center_y = (double)viewport.top + (double)viewport.height * 0.5;
    return (int32_t)llround(camera.center_y + ((double)screen_y - center_y) / safe_scale);
}

bool get_hoop_dimensions(
    size_t hoop_preset_index,
    bool hoop_rotated,
    int32_t *width_units_out,
    int32_t *height_units_out
) {
    if (hoop_preset_index == 0U || hoop_preset_index >= HOOP_PRESET_COUNT) {
        return false;
    }
    const HoopPreset preset = standard_hoop_presets[hoop_preset_index];
    if (hoop_rotated) {
        *width_units_out = preset.height_units;
        *height_units_out = preset.width_units;
    } else {
        *width_units_out = preset.width_units;
        *height_units_out = preset.height_units;
    }
    return true;
}

bool design_exceeds_hoop(
    const StitchDesign *design,
    size_t hoop_preset_index,
    bool hoop_rotated
) {
    if (!design->has_bounds) {
        return false;
    }
    int32_t hoop_width = 0;
    int32_t hoop_height = 0;
    if (!get_hoop_dimensions(hoop_preset_index, hoop_rotated, &hoop_width, &hoop_height)) {
        return false;
    }
    const int32_t half_width = hoop_width / 2;
    const int32_t half_height = hoop_height / 2;
    return design->minimum_x < -half_width ||
           design->maximum_x > half_width ||
           design->minimum_y < -half_height ||
           design->maximum_y > half_height;
}

void draw_cross_grid_background(
    PixelCanvas *canvas,
    RenderViewport viewport,
    CameraView camera,
    CrossGridInfo grid,
    bool show_grid
) {
    if (!show_grid || camera.pixels_per_unit <= 0.0 || viewport.width <= 0 || viewport.height <= 0) {
        return;
    }

    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }

    int32_t visual_step = pitch;
    while ((double)visual_step * camera.pixels_per_unit < 8.0 && visual_step < 5000) {
        if (visual_step == pitch) {
            visual_step = pitch * 5;
        } else {
            visual_step *= 2;
        }
    }

    const int32_t world_min_x = viewport_screen_to_world_x(viewport, camera, viewport.left);
    const int32_t world_max_x = viewport_screen_to_world_x(viewport, camera, viewport.left + viewport.width);
    const int32_t world_min_y = viewport_screen_to_world_y(viewport, camera, viewport.top);
    const int32_t world_max_y = viewport_screen_to_world_y(viewport, camera, viewport.top + viewport.height);

    const uint32_t minor_color = pack_rgb_pixel(226U, 229U, 234U);
    const uint32_t major_color = pack_rgb_pixel(204U, 210U, 220U);

    const int32_t start_column = floor_divide_int32(world_min_x - grid.grid_origin_x, visual_step);
    const int32_t end_column = floor_divide_int32(world_max_x - grid.grid_origin_x, visual_step) + 1;
    for (int32_t column = start_column; column <= end_column; column += 1) {
        const int32_t world_x = grid.grid_origin_x + column * visual_step;
        const int32_t screen_x = viewport_world_to_screen_x(viewport, camera, world_x);
        uint32_t line_color = minor_color;
        if (column % 5 == 0) {
            line_color = major_color;
        }
        draw_line_clipped(canvas, screen_x, viewport.top, screen_x, viewport.top + viewport.height, line_color, 0);
    }

    const int32_t start_row = floor_divide_int32(world_min_y - grid.grid_origin_y, visual_step);
    const int32_t end_row = floor_divide_int32(world_max_y - grid.grid_origin_y, visual_step) + 1;
    for (int32_t row = start_row; row <= end_row; row += 1) {
        const int32_t world_y = grid.grid_origin_y + row * visual_step;
        const int32_t screen_y = viewport_world_to_screen_y(viewport, camera, world_y);
        uint32_t line_color = minor_color;
        if (row % 5 == 0) {
            line_color = major_color;
        }
        draw_line_clipped(canvas, viewport.left, screen_y, viewport.left + viewport.width, screen_y, line_color, 0);
    }
}

void draw_hoop_overlay(
    PixelCanvas *canvas,
    RenderViewport viewport,
    CameraView camera,
    const StitchDesign *design,
    size_t hoop_preset_index,
    bool hoop_rotated
) {
    int32_t hoop_width = 0;
    int32_t hoop_height = 0;
    if (!get_hoop_dimensions(hoop_preset_index, hoop_rotated, &hoop_width, &hoop_height)) {
        return;
    }

    const int32_t left_x = viewport_world_to_screen_x(viewport, camera, -hoop_width / 2);
    const int32_t right_x = viewport_world_to_screen_x(viewport, camera, hoop_width / 2);
    const int32_t top_y = viewport_world_to_screen_y(viewport, camera, -hoop_height / 2);
    const int32_t bottom_y = viewport_world_to_screen_y(viewport, camera, hoop_height / 2);

    const bool exceeds = design_exceeds_hoop(design, hoop_preset_index, hoop_rotated);
    uint32_t hoop_color = pack_rgb_pixel(36U, 118U, 198U);
    if (exceeds) {
        hoop_color = pack_rgb_pixel(224U, 36U, 36U);
    }

    draw_rectangle_outline(canvas, left_x, top_y, right_x - left_x, bottom_y - top_y, hoop_color, 8);
    draw_rectangle_outline(canvas, left_x - 1, top_y - 1, right_x - left_x + 2, bottom_y - top_y + 2, hoop_color, 8);

    const int32_t origin_x = viewport_world_to_screen_x(viewport, camera, 0);
    const int32_t origin_y = viewport_world_to_screen_y(viewport, camera, 0);
    draw_line_clipped(canvas, origin_x - 10, origin_y, origin_x + 10, origin_y, hoop_color, 0);
    draw_line_clipped(canvas, origin_x, origin_y - 10, origin_x, origin_y + 10, hoop_color, 0);
}

static uint64_t hash_block_center(size_t color_block_index, int32_t center_x, int32_t center_y) {
    uint64_t hash_value = 14695981039346656037ULL;
    hash_value ^= (uint64_t)color_block_index;
    hash_value *= 1099511628211ULL;
    hash_value ^= (uint64_t)(uint32_t)center_x;
    hash_value *= 1099511628211ULL;
    hash_value ^= (uint64_t)(uint32_t)center_y;
    hash_value *= 1099511628211ULL;
    return hash_value;
}

static void reset_block_dedup_frame(BlockDedupTable *table) {
    if (table == nullptr || table->slots == nullptr) {
        return;
    }
    table->generation += 1U;
    if (table->generation == 0U) {
        memset(table->slots, 0, BLOCK_DEDUP_CAPACITY * sizeof(BlockDedupSlot));
        table->generation = 1U;
    }
}

/* Returns true if this square should be drawn (new or upgraded visual state). */
static bool block_dedup_try_insert(
    BlockDedupTable *table,
    size_t color_block_index,
    int32_t center_x,
    int32_t center_y,
    uint8_t visual_flags
) {
    if (table == nullptr || table->slots == nullptr) {
        return true;
    }
    const size_t mask = BLOCK_DEDUP_CAPACITY - 1U;
    const uint32_t generation = table->generation;
    size_t slot_index = (size_t)hash_block_center(color_block_index, center_x, center_y) & mask;

    for (size_t probe = 0U; probe < 16U; probe += 1U) {
        BlockDedupSlot *slot = &table->slots[slot_index];
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
    const BlockDedupTable *table,
    size_t color_block_index,
    int32_t center_x,
    int32_t center_y
) {
    if (table == nullptr || table->slots == nullptr) {
        return false;
    }
    const size_t mask = BLOCK_DEDUP_CAPACITY - 1U;
    const uint32_t generation = table->generation;
    size_t slot_index = (size_t)hash_block_center(color_block_index, center_x, center_y) & mask;

    for (size_t probe = 0U; probe < 16U; probe += 1U) {
        const BlockDedupSlot *slot = &table->slots[slot_index];
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
    const StitchDesign *design,
    const DesignRenderParams *params
) {
    const double scale = params->camera.pixels_per_unit;
    const double origin_screen_x =
        (double)params->viewport.left + (double)params->viewport.width * 0.5 -
        params->camera.center_x * scale;
    const double origin_screen_y =
        (double)params->viewport.top + (double)params->viewport.height * 0.5 -
        params->camera.center_y * scale;

    const bool thread_mode = (params->view_mode == VIEW_MODE_THREAD);
    int32_t stroke_px = 1;
    if (thread_mode) {
        stroke_px = compute_thread_stroke_px(params->thread_thickness_index, scale);
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
        if (drawn_stitch_count >= params->maximum_stitches_to_draw) {
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
            fast_round_to_int32(origin_screen_x + (double)(chain_points[0].position_x + params->world_offset_x) * scale);
        const int32_t first_screen_y =
            fast_round_to_int32(origin_screen_y + (double)(chain_points[0].position_y + params->world_offset_y) * scale);

        if (params->draw_overlays && params->show_jumps && has_previous_end &&
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
            if (drawn_stitch_count >= params->maximum_stitches_to_draw) {
                break;
            }
            const StitchPoint next_point = chain_points[local_index + 1U];
            const int32_t end_x =
                fast_round_to_int32(origin_screen_x + (double)(next_point.position_x + params->world_offset_x) * scale);
            const int32_t end_y =
                fast_round_to_int32(origin_screen_y + (double)(next_point.position_y + params->world_offset_y) * scale);

            const size_t current_flat = flat_segment_index + local_index;
            uint8_t visual_flags = 1U;
            if (params->selection_mask != nullptr &&
                current_flat < params->selection_mask_length &&
                params->selection_mask[current_flat] != 0U) {
                visual_flags = 3U;
            } else if (params->hover_mask != nullptr &&
                       current_flat < params->hover_mask_length &&
                       params->hover_mask[current_flat] != 0U) {
                visual_flags = 2U;
            } else if (params->keep_preview_mask != nullptr &&
                       current_flat < params->keep_preview_mask_length &&
                       params->keep_preview_mask[current_flat] == 0U) {
                visual_flags = 0U;
            }

            /* Skip redundant consecutive back-and-forth passes on identical endpoints. */
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

        if (params->draw_overlays && params->show_locks) {
            if (chain.start_was_cut) {
                draw_lock_marker(canvas, first_screen_x, first_screen_y);
            }
            if (chain.end_was_cut && drawn_stitch_count < params->maximum_stitches_to_draw) {
                draw_lock_marker(canvas, start_x, start_y);
            }
        }

        previous_block_index = chain.color_block_index;
        previous_end_x = start_x;
        previous_end_y = start_y;
        has_previous_end = true;
        flat_segment_index += chain_segments;
    }

    if (params->draw_overlays && params->maximum_stitches_to_draw < SIZE_MAX && has_needle_tip) {
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
    const DesignRenderParams *params,
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
    if (is_micro_lock && !params->show_locks) {
        return;
    }

    if (is_cross_center_connector(start_point, end_point, pitch)) {
        const bool touches_drawn_center =
            block_dedup_contains_center(params->block_dedup, color_block_index, start_point.position_x, start_point.position_y) ||
            block_dedup_contains_center(params->block_dedup, color_block_index, end_point.position_x, end_point.position_y);
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
    const StitchDesign *design,
    const DesignRenderParams *params
) {
    const double scale = params->camera.pixels_per_unit;
    const double origin_screen_x =
        (double)params->viewport.left + (double)params->viewport.width * 0.5 -
        params->camera.center_x * scale;
    const double origin_screen_y =
        (double)params->viewport.top + (double)params->viewport.height * 0.5 -
        params->camera.center_y * scale;

    int32_t pitch = params->grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }
    const double half_pitch_world = (double)pitch * 0.5;
    const double cell_size_px = (double)pitch * scale;
    const bool leave_gap = (cell_size_px >= 6.0);
    const bool draw_light_border = (cell_size_px >= 4.5);
    reset_block_dedup_frame(params->block_dedup);

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
        if (drawn_stitch_count >= params->maximum_stitches_to_draw) {
            break;
        }

        const RgbColor base_rgb = design->blocks[chain.color_block_index].display_color;
        const StitchPoint *chain_points = &design->points[chain.point_offset];

        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (drawn_stitch_count >= params->maximum_stitches_to_draw) {
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

            const double world_center_x = (double)(center_x + params->world_offset_x);
            const double world_center_y = (double)(center_y + params->world_offset_y);
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
            if (params->selection_mask != nullptr &&
                current_flat < params->selection_mask_length &&
                params->selection_mask[current_flat] != 0U) {
                visual_flags = 3U;
            } else if (params->hover_mask != nullptr &&
                       current_flat < params->hover_mask_length &&
                       params->hover_mask[current_flat] != 0U) {
                visual_flags = 2U;
            } else if (params->keep_preview_mask != nullptr &&
                       current_flat < params->keep_preview_mask_length &&
                       params->keep_preview_mask[current_flat] == 0U) {
                visual_flags = 0U;
            }

            if (block_dedup_try_insert(params->block_dedup, chain.color_block_index, center_x, center_y, visual_flags)) {
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
        if (drawn_stitch_count >= params->maximum_stitches_to_draw) {
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
            fast_round_to_int32(origin_screen_x + (double)(chain_points[0].position_x + params->world_offset_x) * scale);
        const int32_t first_screen_y =
            fast_round_to_int32(origin_screen_y + (double)(chain_points[0].position_y + params->world_offset_y) * scale);

        if (params->draw_overlays && params->show_jumps && has_previous_end &&
            previous_block_index == chain.color_block_index) {
            draw_line_clipped(canvas, previous_end_x, previous_end_y, first_screen_x, first_screen_y, jump_color, 4);
        }

        const bool track_needle_tip = (params->maximum_stitches_to_draw < SIZE_MAX);
        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (drawn_stitch_count >= params->maximum_stitches_to_draw) {
                break;
            }
            const StitchPoint start_point = chain_points[local_index];
            const StitchPoint end_point = chain_points[local_index + 1U];
            drawn_stitch_count += 1U;

            if (is_cross_diagonal_for_pitch(start_point, end_point, pitch)) {
                if (track_needle_tip && drawn_stitch_count == params->maximum_stitches_to_draw) {
                    needle_tip_x = fast_round_to_int32(
                        origin_screen_x + (double)(end_point.position_x + params->world_offset_x) * scale
                    );
                    needle_tip_y = fast_round_to_int32(
                        origin_screen_y + (double)(end_point.position_y + params->world_offset_y) * scale
                    );
                    has_needle_tip = true;
                }
                continue;
            }

            const int32_t start_x = fast_round_to_int32(
                origin_screen_x + (double)(start_point.position_x + params->world_offset_x) * scale
            );
            const int32_t start_y = fast_round_to_int32(
                origin_screen_y + (double)(start_point.position_y + params->world_offset_y) * scale
            );
            const int32_t end_x = fast_round_to_int32(
                origin_screen_x + (double)(end_point.position_x + params->world_offset_x) * scale
            );
            const int32_t end_y = fast_round_to_int32(
                origin_screen_y + (double)(end_point.position_y + params->world_offset_y) * scale
            );
            if (track_needle_tip) {
                needle_tip_x = end_x;
                needle_tip_y = end_y;
                has_needle_tip = true;
            }

            const size_t current_flat = flat_segment_index + local_index;
            uint8_t visual_flags = 1U;
            if (params->selection_mask != nullptr &&
                current_flat < params->selection_mask_length &&
                params->selection_mask[current_flat] != 0U) {
                visual_flags = 3U;
            } else if (params->hover_mask != nullptr &&
                       current_flat < params->hover_mask_length &&
                       params->hover_mask[current_flat] != 0U) {
                visual_flags = 2U;
            } else if (params->keep_preview_mask != nullptr &&
                       current_flat < params->keep_preview_mask_length &&
                       params->keep_preview_mask[current_flat] == 0U) {
                visual_flags = 0U;
            }
            draw_blocks_non_diagonal_segment(
                canvas,
                params,
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

        const StitchPoint tail_point = chain_points[chain_segments];
        const int32_t last_screen_x = fast_round_to_int32(
            origin_screen_x + (double)(tail_point.position_x + params->world_offset_x) * scale
        );
        const int32_t last_screen_y = fast_round_to_int32(
            origin_screen_y + (double)(tail_point.position_y + params->world_offset_y) * scale
        );

        if (params->draw_overlays && params->show_locks) {
            if (chain.start_was_cut) {
                draw_lock_marker(canvas, first_screen_x, first_screen_y);
            }
            if (chain.end_was_cut && drawn_stitch_count < params->maximum_stitches_to_draw) {
                draw_lock_marker(canvas, last_screen_x, last_screen_y);
            }
        }

        previous_block_index = chain.color_block_index;
        previous_end_x = last_screen_x;
        previous_end_y = last_screen_y;
        has_previous_end = true;
        flat_segment_index += chain_segments;
    }

    if (params->draw_overlays && params->maximum_stitches_to_draw < SIZE_MAX && has_needle_tip) {
        draw_needle_tip_marker(canvas, needle_tip_x, needle_tip_y);
    }
}

void render_design_chains(
    PixelCanvas *canvas,
    const StitchDesign *design,
    const DesignRenderParams *params
) {
    if (canvas == nullptr || canvas->pixels == nullptr || design == nullptr || params == nullptr) {
        return;
    }
    if (params->view_mode == VIEW_MODE_BLOCKS) {
        render_design_blocks(canvas, design, params);
        return;
    }
    render_design_line_or_thread(canvas, design, params);
}

void init_stitch_layer_cache(StitchLayerCache *cache) {
    *cache = (StitchLayerCache){};
}

void free_stitch_layer_cache(StitchLayerCache *cache) {
    free_pixel_canvas(&cache->buffer);
    *cache = (StitchLayerCache){};
}

void invalidate_stitch_layer_cache(StitchLayerCache *cache) {
    cache->valid = false;
}

bool ensure_stitch_layer_cache(
    StitchLayerCache *cache,
    const StitchDesign *design,
    const DesignRenderParams *params,
    uint64_t design_revision,
    uint64_t view_revision,
    uint64_t mask_revision,
    bool show_grid,
    size_t hoop_preset_index,
    bool hoop_rotated,
    bool *rebuilt_out
) {
    if (rebuilt_out != nullptr) {
        *rebuilt_out = false;
    }
    if (cache == nullptr || design == nullptr || params == nullptr ||
        params->viewport.width <= 0 || params->viewport.height <= 0) {
        return false;
    }

    if (cache->valid &&
        cache->viewport_width == params->viewport.width &&
        cache->viewport_height == params->viewport.height &&
        cache->design_revision == design_revision &&
        cache->view_revision == view_revision &&
        cache->mask_revision == mask_revision) {
        return true;
    }

    if (cache->buffer.pixels == nullptr ||
        cache->buffer.width != params->viewport.width ||
        cache->buffer.height != params->viewport.height) {
        PixelCanvas resized = {};
        if (!init_pixel_canvas(&resized, params->viewport.width, params->viewport.height)) {
            return false;
        }
        free_pixel_canvas(&cache->buffer);
        cache->buffer = resized;
    }

    reset_canvas_clip(&cache->buffer);
    draw_filled_rectangle(
        &cache->buffer,
        0,
        0,
        cache->buffer.width,
        cache->buffer.height,
        pack_rgb_pixel(245U, 246U, 248U)
    );

    const RenderViewport local_viewport = {
        .left = 0,
        .top = 0,
        .width = cache->buffer.width,
        .height = cache->buffer.height
    };
    draw_cross_grid_background(&cache->buffer, local_viewport, params->camera, params->grid, show_grid);
    draw_hoop_overlay(&cache->buffer, local_viewport, params->camera, design, hoop_preset_index, hoop_rotated);

    if (design->chain_count > 0U) {
        DesignRenderParams cached_params = *params;
        cached_params.viewport = local_viewport;
        cached_params.hover_mask = nullptr;
        cached_params.hover_mask_length = 0U;
        render_design_chains(&cache->buffer, design, &cached_params);
    }

    cache->viewport_width = params->viewport.width;
    cache->viewport_height = params->viewport.height;
    cache->design_revision = design_revision;
    cache->view_revision = view_revision;
    cache->mask_revision = mask_revision;
    cache->valid = true;
    if (rebuilt_out != nullptr) {
        *rebuilt_out = true;
    }
    return true;
}

void blit_stitch_layer_cache_rect(
    PixelCanvas *destination,
    const StitchLayerCache *cache,
    RenderViewport viewport,
    int32_t screen_min_x,
    int32_t screen_min_y,
    int32_t screen_max_x,
    int32_t screen_max_y
) {
    if (destination == nullptr || destination->pixels == nullptr ||
        cache == nullptr || !cache->valid || cache->buffer.pixels == nullptr) {
        return;
    }

    int32_t min_x = screen_min_x;
    int32_t min_y = screen_min_y;
    int32_t max_x = screen_max_x;
    int32_t max_y = screen_max_y;

    if (min_x < viewport.left) {
        min_x = viewport.left;
    }
    if (min_y < viewport.top) {
        min_y = viewport.top;
    }
    int32_t vp_right = viewport.left + viewport.width;
    if (viewport.left + cache->buffer.width < vp_right) {
        vp_right = viewport.left + cache->buffer.width;
    }
    if (destination->width < vp_right) {
        vp_right = destination->width;
    }
    int32_t vp_bottom = viewport.top + viewport.height;
    if (viewport.top + cache->buffer.height < vp_bottom) {
        vp_bottom = viewport.top + cache->buffer.height;
    }
    if (destination->height < vp_bottom) {
        vp_bottom = destination->height;
    }
    if (max_x > vp_right) {
        max_x = vp_right;
    }
    if (max_y > vp_bottom) {
        max_y = vp_bottom;
    }
    if (min_x < 0) {
        min_x = 0;
    }
    if (min_y < 0) {
        min_y = 0;
    }
    if (min_x >= max_x || min_y >= max_y) {
        return;
    }

    const int32_t copy_width = max_x - min_x;
    const int32_t src_offset_x = min_x - viewport.left;
    const size_t row_bytes = (size_t)copy_width * sizeof(uint32_t);
    const size_t destination_stride = (size_t)destination->width;
    const size_t source_stride = (size_t)cache->buffer.width;

    for (int32_t screen_y = min_y; screen_y < max_y; screen_y += 1) {
        const int32_t src_y = screen_y - viewport.top;
        uint32_t *destination_row =
            destination->pixels + (size_t)screen_y * destination_stride + (size_t)min_x;
        const uint32_t *source_row =
            cache->buffer.pixels + (size_t)src_y * source_stride + (size_t)src_offset_x;
        memcpy(destination_row, source_row, row_bytes);
    }
}

void blit_stitch_layer_cache(
    PixelCanvas *destination,
    const StitchLayerCache *cache,
    RenderViewport viewport
) {
    blit_stitch_layer_cache_rect(
        destination,
        cache,
        viewport,
        viewport.left,
        viewport.top,
        viewport.left + viewport.width,
        viewport.top + viewport.height
    );
}

static void expand_screen_rect_bounds(
    bool *has_bounds,
    int32_t *min_x,
    int32_t *min_y,
    int32_t *max_x,
    int32_t *max_y,
    int32_t rect_min_x,
    int32_t rect_min_y,
    int32_t rect_max_x,
    int32_t rect_max_y
) {
    if (!(*has_bounds)) {
        *min_x = rect_min_x;
        *min_y = rect_min_y;
        *max_x = rect_max_x;
        *max_y = rect_max_y;
        *has_bounds = true;
        return;
    }
    if (rect_min_x < *min_x) {
        *min_x = rect_min_x;
    }
    if (rect_min_y < *min_y) {
        *min_y = rect_min_y;
    }
    if (rect_max_x > *max_x) {
        *max_x = rect_max_x;
    }
    if (rect_max_y > *max_y) {
        *max_y = rect_max_y;
    }
}

void render_chain_subset_overlay(
    PixelCanvas *canvas,
    const StitchDesign *design,
    const DesignRenderParams *params,
    const size_t *chain_indices,
    size_t chain_count,
    uint8_t visual_flags,
    int32_t *bounds_min_x_out,
    int32_t *bounds_min_y_out,
    int32_t *bounds_max_x_out,
    int32_t *bounds_max_y_out
) {
    if (bounds_min_x_out != nullptr && bounds_min_y_out != nullptr &&
        bounds_max_x_out != nullptr && bounds_max_y_out != nullptr) {
        *bounds_min_x_out = 0;
        *bounds_min_y_out = 0;
        *bounds_max_x_out = 0;
        *bounds_max_y_out = 0;
    }
    if (canvas == nullptr || canvas->pixels == nullptr || design == nullptr ||
        params == nullptr || chain_indices == nullptr || chain_count == 0U) {
        return;
    }

    const double scale = params->camera.pixels_per_unit;
    const double origin_screen_x =
        (double)params->viewport.left + (double)params->viewport.width * 0.5 -
        params->camera.center_x * scale;
    const double origin_screen_y =
        (double)params->viewport.top + (double)params->viewport.height * 0.5 -
        params->camera.center_y * scale;
    bool has_drawn_bounds = false;
    int32_t drawn_min_x = 0;
    int32_t drawn_min_y = 0;
    int32_t drawn_max_x = 0;
    int32_t drawn_max_y = 0;

    if (params->view_mode == VIEW_MODE_BLOCKS) {
        int32_t pitch = params->grid.cross_pitch_units;
        if (pitch < MIN_CROSS_PITCH_UNITS) {
            pitch = DEFAULT_CROSS_PITCH_UNITS;
        }
        const double half_pitch_world = (double)pitch * 0.5;
        const double cell_size_px = (double)pitch * scale;
        const bool leave_gap = (cell_size_px >= 6.0);
        const bool draw_light_border = (cell_size_px >= 4.5);
        const bool draw_thick_outline = (scale >= 1.0);
        reset_block_dedup_frame(params->block_dedup);

        for (size_t subset_index = 0U; subset_index < chain_count; subset_index += 1U) {
            const size_t chain_index = chain_indices[subset_index];
            if (chain_index >= design->chain_count) {
                continue;
            }
            const StitchChain chain = design->chains[chain_index];
            if (chain.point_count < 2U || chain.color_block_index >= design->block_count ||
                !design->blocks[chain.color_block_index].enabled) {
                continue;
            }
            const RgbColor base_rgb = design->blocks[chain.color_block_index].display_color;
            const uint32_t line_color = pack_rgb_pixel(base_rgb.red, base_rgb.green, base_rgb.blue);
            const uint32_t dimmed_color = make_dimmed_color(base_rgb);
            const StitchPoint *chain_points = &design->points[chain.point_offset];
            const size_t chain_segments = chain.point_count - 1U;

            for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
                const StitchPoint start_point = chain_points[local_index];
                const StitchPoint end_point = chain_points[local_index + 1U];
                if (is_cross_diagonal_for_pitch(start_point, end_point, pitch)) {
                    const int32_t center_x =
                        (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
                    const int32_t center_y =
                        (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);
                    const double world_center_x = (double)(center_x + params->world_offset_x);
                    const double world_center_y = (double)(center_y + params->world_offset_y);
                    const int32_t left_x = fast_round_to_int32(origin_screen_x + (world_center_x - half_pitch_world) * scale);
                    const int32_t right_x = fast_round_to_int32(origin_screen_x + (world_center_x + half_pitch_world) * scale);
                    const int32_t top_y = fast_round_to_int32(origin_screen_y + (world_center_y - half_pitch_world) * scale);
                    const int32_t bottom_y = fast_round_to_int32(origin_screen_y + (world_center_y + half_pitch_world) * scale);
                    if (right_x < canvas->clip_min_x || left_x >= canvas->clip_max_x ||
                        bottom_y < canvas->clip_min_y || top_y >= canvas->clip_max_y) {
                        continue;
                    }
                    if (block_dedup_try_insert(params->block_dedup, chain.color_block_index, center_x, center_y, visual_flags)) {
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
                        expand_screen_rect_bounds(
                            &has_drawn_bounds,
                            &drawn_min_x,
                            &drawn_min_y,
                            &drawn_max_x,
                            &drawn_max_y,
                            left_x - 1,
                            top_y - 1,
                            right_x + 2,
                            bottom_y + 2
                        );
                    }
                } else {
                    const int32_t start_x = fast_round_to_int32(
                        origin_screen_x + (double)(start_point.position_x + params->world_offset_x) * scale
                    );
                    const int32_t start_y = fast_round_to_int32(
                        origin_screen_y + (double)(start_point.position_y + params->world_offset_y) * scale
                    );
                    const int32_t end_x = fast_round_to_int32(
                        origin_screen_x + (double)(end_point.position_x + params->world_offset_x) * scale
                    );
                    const int32_t end_y = fast_round_to_int32(
                        origin_screen_y + (double)(end_point.position_y + params->world_offset_y) * scale
                    );
                    draw_blocks_non_diagonal_segment(
                        canvas,
                        params,
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
                    int32_t seg_min_x = start_x;
                    int32_t seg_max_x = end_x;
                    if (end_x < start_x) {
                        seg_min_x = end_x;
                        seg_max_x = start_x;
                    }
                    int32_t seg_min_y = start_y;
                    int32_t seg_max_y = end_y;
                    if (end_y < start_y) {
                        seg_min_y = end_y;
                        seg_max_y = start_y;
                    }
                    expand_screen_rect_bounds(
                        &has_drawn_bounds,
                        &drawn_min_x,
                        &drawn_min_y,
                        &drawn_max_x,
                        &drawn_max_y,
                        seg_min_x - 2,
                        seg_min_y - 2,
                        seg_max_x + 3,
                        seg_max_y + 3
                    );
                }
            }
        }
        if (has_drawn_bounds && bounds_min_x_out != nullptr && bounds_min_y_out != nullptr &&
            bounds_max_x_out != nullptr && bounds_max_y_out != nullptr) {
            *bounds_min_x_out = drawn_min_x;
            *bounds_min_y_out = drawn_min_y;
            *bounds_max_x_out = drawn_max_x;
            *bounds_max_y_out = drawn_max_y;
        }
        return;
    }

    const bool thread_mode = (params->view_mode == VIEW_MODE_THREAD);
    int32_t stroke_px = 1;
    if (thread_mode) {
        stroke_px = compute_thread_stroke_px(params->thread_thickness_index, scale);
    }
    const int32_t cull_margin = stroke_px + 2;

    for (size_t subset_index = 0U; subset_index < chain_count; subset_index += 1U) {
        const size_t chain_index = chain_indices[subset_index];
        if (chain_index >= design->chain_count) {
            continue;
        }
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U || chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled) {
            continue;
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
        int32_t start_x = fast_round_to_int32(
            origin_screen_x + (double)(chain_points[0].position_x + params->world_offset_x) * scale
        );
        int32_t start_y = fast_round_to_int32(
            origin_screen_y + (double)(chain_points[0].position_y + params->world_offset_y) * scale
        );
        const size_t chain_segments = chain.point_count - 1U;
        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            const StitchPoint next_point = chain_points[local_index + 1U];
            const int32_t end_x = fast_round_to_int32(
                origin_screen_x + (double)(next_point.position_x + params->world_offset_x) * scale
            );
            const int32_t end_y = fast_round_to_int32(
                origin_screen_y + (double)(next_point.position_y + params->world_offset_y) * scale
            );
            const bool inside_viewport =
                !((start_x < canvas->clip_min_x - cull_margin && end_x < canvas->clip_min_x - cull_margin) ||
                  (start_x >= canvas->clip_max_x + cull_margin && end_x >= canvas->clip_max_x + cull_margin) ||
                  (start_y < canvas->clip_min_y - cull_margin && end_y < canvas->clip_min_y - cull_margin) ||
                  (start_y >= canvas->clip_max_y + cull_margin && end_y >= canvas->clip_max_y + cull_margin));
            if (inside_viewport) {
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
                int32_t seg_min_x = start_x;
                int32_t seg_max_x = end_x;
                if (end_x < start_x) {
                    seg_min_x = end_x;
                    seg_max_x = start_x;
                }
                int32_t seg_min_y = start_y;
                int32_t seg_max_y = end_y;
                if (end_y < start_y) {
                    seg_min_y = end_y;
                    seg_max_y = start_y;
                }
                expand_screen_rect_bounds(
                    &has_drawn_bounds,
                    &drawn_min_x,
                    &drawn_min_y,
                    &drawn_max_x,
                    &drawn_max_y,
                    seg_min_x - cull_margin,
                    seg_min_y - cull_margin,
                    seg_max_x + cull_margin + 1,
                    seg_max_y + cull_margin + 1
                );
            }
            start_x = end_x;
            start_y = end_y;
        }
    }

    if (has_drawn_bounds && bounds_min_x_out != nullptr && bounds_min_y_out != nullptr &&
        bounds_max_x_out != nullptr && bounds_max_y_out != nullptr) {
        *bounds_min_x_out = drawn_min_x;
        *bounds_min_y_out = drawn_min_y;
        *bounds_max_x_out = drawn_max_x;
        *bounds_max_y_out = drawn_max_y;
    }
}
