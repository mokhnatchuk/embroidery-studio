#define _POSIX_C_SOURCE 200809L

#include "formats.h"
#include "model.h"
#include "ops.h"
#include "render.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Link-time allocation counter (--wrap=malloc/calloc/realloc) for zero-alloc verification. */
void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *pointer, size_t size);

static bool g_count_allocations = false;
static size_t g_allocation_count = 0U;

void *__wrap_malloc(size_t size) {
    if (g_count_allocations) {
        g_allocation_count += 1U;
    }
    return __real_malloc(size);
}

void *__wrap_calloc(size_t count, size_t size) {
    if (g_count_allocations) {
        g_allocation_count += 1U;
    }
    return __real_calloc(count, size);
}

void *__wrap_realloc(void *pointer, size_t size) {
    if (g_count_allocations) {
        g_allocation_count += 1U;
    }
    return __real_realloc(pointer, size);
}

static double monotonic_milliseconds(void) {
    struct timespec now = {};
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec * 1000.0 + (double)now.tv_nsec / 1000000.0;
}

static bool build_large_synthetic_design(StitchDesign *design) {
    init_stitch_design(design);
    constexpr int32_t PITCH = 18;
    constexpr int32_t COLS = 160;
    constexpr int32_t ROWS = 160;
    constexpr int32_t PASSES = 3;
    constexpr size_t COLOR_COUNT = 8U;
    static const uint8_t palette_ids[8] = {10U, 12U, 6U, 37U, 8U, 4U, 27U, 1U};

    for (size_t color_slot = 0U; color_slot < COLOR_COUNT; color_slot += 1U) {
        const uint8_t janome = palette_ids[color_slot];
        if (!design_append_block(design, janome_palette[janome], janome, true, nullptr)) {
            return false;
        }
    }

    /* Group 4x4 motifs into chains so the design has ~230,000 stitches and ~1,600 chains. */
    for (int32_t block_row = 0; block_row < ROWS; block_row += 4) {
        for (int32_t block_col = 0; block_col < COLS; block_col += 4) {
            const size_t color_block = (size_t)(((block_row / 4) + (block_col / 4)) % (int32_t)COLOR_COUNT);
            const size_t point_offset = design->point_count;
            for (int32_t local_row = 0; local_row < 4; local_row += 1) {
                for (int32_t local_col = 0; local_col < 4; local_col += 1) {
                    const int32_t cell_x = (block_col + local_col - COLS / 2) * PITCH;
                    const int32_t cell_y = (block_row + local_row - ROWS / 2) * PITCH;
                    const int32_t left = cell_x;
                    const int32_t right = cell_x + PITCH;
                    const int32_t top = cell_y;
                    const int32_t bottom = cell_y + PITCH;
                    const int32_t center_x = cell_x + PITCH / 2;
                    const int32_t center_y = cell_y + PITCH / 2;

                    if (!design_append_point(design, left, bottom)) {
                        return false;
                    }
                    for (int32_t pass = 0; pass < PASSES; pass += 1) {
                        if ((pass % 2) == 0) {
                            if (!design_append_point(design, right, top)) {
                                return false;
                            }
                        } else if (!design_append_point(design, left, bottom)) {
                            return false;
                        }
                    }
                    if (!design_append_point(design, center_x, center_y) ||
                        !design_append_point(design, right, bottom)) {
                        return false;
                    }
                    for (int32_t pass = 0; pass < PASSES; pass += 1) {
                        if ((pass % 2) == 0) {
                            if (!design_append_point(design, left, top)) {
                                return false;
                            }
                        } else if (!design_append_point(design, right, bottom)) {
                            return false;
                        }
                    }
                }
            }
            const size_t chain_points = design->point_count - point_offset;
            if (!design_append_chain(design, point_offset, chain_points, color_block, false, false)) {
                return false;
            }
        }
    }

    if (!sort_chains_by_color_block(design)) {
        return false;
    }
    recompute_design_bounds(design);
    return true;
}

int main(int argument_count, char **argument_values) {
    int exit_code = EXIT_FAILURE;
    EditorState state = {};
    PixelCanvas canvas = {};
    init_default_editor_state(&state);

    if (argument_count > 1 && argument_values[1][0] != '\0') {
        if (!load_embroidery_file(argument_values[1], &state.design)) {
            (void)fprintf(stderr, "Failed to load benchmark file: %s\n", argument_values[1]);
            goto cleanup;
        }
    } else if (!build_large_synthetic_design(&state.design)) {
        (void)fprintf(stderr, "Failed to build synthetic 200k+ design\n");
        goto cleanup;
    }

    state.grid = detect_cross_stitch_grid(&state.design);
    editor_mark_design_changed(&state, false);
    const DesignStats stats = compute_design_stats(&state.design);

    if (!init_pixel_canvas(&canvas, 1920, 1080)) {
        (void)fprintf(stderr, "Failed to init 1920x1080 benchmark canvas\n");
        goto cleanup;
    }
    fit_camera_to_design(&state, canvas.width, canvas.height);

    const double index_start = monotonic_milliseconds();
    editor_ensure_spatial_index(&state);
    const double index_ms = monotonic_milliseconds() - index_start;

    const double cold_start = monotonic_milliseconds();
    render_editor_frame(&canvas, &state);
    const double cold_ms = monotonic_milliseconds() - cold_start;

    const RenderViewport viewport = get_editor_viewport(&state, canvas.width, canvas.height);
    constexpr int32_t ITERATIONS = 100;

    /* 1. Benchmark Sel Elem hover + interactive frame (strict zero-alloc check). */
    state.active_tool = TOOL_SELECT_ELEMENT;
    g_allocation_count = 0U;
    g_count_allocations = true;
    const double elem_hover_start = monotonic_milliseconds();
    for (int32_t step = 0; step < ITERATIONS; step += 1) {
        const int32_t mouse_x = viewport.left + viewport.width / 4 + (step * 7) % (viewport.width / 2);
        const int32_t mouse_y = viewport.top + viewport.height / 4 + (step * 5) % (viewport.height / 2);
        state.mouse_screen_x = mouse_x;
        state.mouse_screen_y = mouse_y;
        editor_update_canvas_hover(&state, mouse_x, mouse_y, canvas.width, canvas.height);
    }
    const double elem_hover_avg_ms = (monotonic_milliseconds() - elem_hover_start) / (double)ITERATIONS;
    const size_t elem_hover_allocs = g_allocation_count;

    g_allocation_count = 0U;
    const double elem_frame_start = monotonic_milliseconds();
    for (int32_t step = 0; step < ITERATIONS; step += 1) {
        const int32_t mouse_x = viewport.left + viewport.width / 4 + (step * 7) % (viewport.width / 2);
        const int32_t mouse_y = viewport.top + viewport.height / 4 + (step * 5) % (viewport.height / 2);
        state.mouse_screen_x = mouse_x;
        state.mouse_screen_y = mouse_y;
        editor_update_canvas_hover(&state, mouse_x, mouse_y, canvas.width, canvas.height);
        render_editor_frame(&canvas, &state);
    }
    const double elem_frame_avg_ms = (monotonic_milliseconds() - elem_frame_start) / (double)ITERATIONS;
    const size_t elem_frame_allocs = g_allocation_count;

    /* 2. Benchmark Sel X hover + interactive frame (strict zero-alloc check). */
    state.active_tool = TOOL_SELECT_CROSS;
    clear_hover_preview(&state);
    g_allocation_count = 0U;
    const double cross_hover_start = monotonic_milliseconds();
    for (int32_t step = 0; step < ITERATIONS; step += 1) {
        const int32_t mouse_x = viewport.left + viewport.width / 4 + (step * 9) % (viewport.width / 2);
        const int32_t mouse_y = viewport.top + viewport.height / 4 + (step * 6) % (viewport.height / 2);
        state.mouse_screen_x = mouse_x;
        state.mouse_screen_y = mouse_y;
        editor_update_canvas_hover(&state, mouse_x, mouse_y, canvas.width, canvas.height);
    }
    const double cross_hover_avg_ms = (monotonic_milliseconds() - cross_hover_start) / (double)ITERATIONS;
    const size_t cross_hover_allocs = g_allocation_count;

    g_allocation_count = 0U;
    const double cross_frame_start = monotonic_milliseconds();
    for (int32_t step = 0; step < ITERATIONS; step += 1) {
        const int32_t mouse_x = viewport.left + viewport.width / 4 + (step * 9) % (viewport.width / 2);
        const int32_t mouse_y = viewport.top + viewport.height / 4 + (step * 6) % (viewport.height / 2);
        state.mouse_screen_x = mouse_x;
        state.mouse_screen_y = mouse_y;
        editor_update_canvas_hover(&state, mouse_x, mouse_y, canvas.width, canvas.height);
        render_editor_frame(&canvas, &state);
    }
    const double cross_frame_avg_ms = (monotonic_milliseconds() - cross_frame_start) / (double)ITERATIONS;
    const size_t cross_frame_allocs = g_allocation_count;

    /* 3. Benchmark cached Cut preview mask (warm cache once, then measure zero-alloc drag). */
    g_count_allocations = false;
    state.active_tool = TOOL_CUT_OUT;
    state.has_box_selection = true;
    state.is_dragging_box = false;
    state.box_start_x = -200;
    state.box_start_y = -200;
    state.box_end_x = 200;
    state.box_end_y = 200;
    state.preview_dirty = true;
    refresh_live_preview(&state);

    g_allocation_count = 0U;
    g_count_allocations = true;
    const double cut_mask_start = monotonic_milliseconds();
    for (int32_t step = 0; step < 30; step += 1) {
        state.box_end_x = 180 + step * 8;
        state.box_end_y = 180 + step * 8;
        state.preview_dirty = true;
        refresh_live_preview(&state);
    }
    const double cut_mask_avg_ms = (monotonic_milliseconds() - cut_mask_start) / 30.0;
    const size_t cut_mask_allocs = g_allocation_count;
    g_count_allocations = false;

    printf(
        "Benchmark (%zu stitches, %zu chains, 1920x1080):\n"
        "  SpatialIndex build : %.3f ms\n"
        "  Cold bake frame    : %.3f ms\n"
        "  Sel Elem hover     : %.4f ms (allocs=%zu)\n"
        "  Sel Elem frame     : %.4f ms (allocs=%zu)\n"
        "  Sel X hover        : %.4f ms (allocs=%zu)\n"
        "  Sel X frame        : %.4f ms (allocs=%zu)\n"
        "  Cached Cut mask    : %.4f ms (allocs=%zu)\n",
        stats.stitch_count,
        stats.chain_count,
        index_ms,
        cold_ms,
        elem_hover_avg_ms,
        elem_hover_allocs,
        elem_frame_avg_ms,
        elem_frame_allocs,
        cross_hover_avg_ms,
        cross_hover_allocs,
        cross_frame_avg_ms,
        cross_frame_allocs,
        cut_mask_avg_ms,
        cut_mask_allocs
    );

    if (stats.stitch_count < 200000U ||
        elem_hover_allocs != 0U ||
        elem_frame_allocs != 0U ||
        cross_hover_allocs != 0U ||
        cross_frame_allocs != 0U ||
        cut_mask_allocs != 0U ||
        elem_hover_avg_ms >= 1.0 ||
        cross_hover_avg_ms >= 1.0 ||
        elem_frame_avg_ms >= 1.0 ||
        cross_frame_avg_ms >= 1.0) {
        (void)fprintf(stderr, "BENCHMARK CRITERIA FAILED\n");
        goto cleanup;
    }

    printf("[PASS] Stage 2 benchmark meets < 1.0 ms interactive/hover and 0 allocs on 200k+ stitches\n");
    exit_code = EXIT_SUCCESS;

cleanup:
    free_pixel_canvas(&canvas);
    free_editor_state(&state);
    return exit_code;
}
