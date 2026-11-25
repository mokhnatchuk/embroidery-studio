#define _POSIX_C_SOURCE 200809L

#include "ui.h"

#include <dirent.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

constexpr int32_t LEFT_ROW_BUTTON_WIDTH = 34;
constexpr int32_t LEFT_ROW_BUTTON_HEIGHT = 28;
constexpr int32_t LEFT_ROW_BUTTON_GAP = 4;

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

bool point_is_in_box(
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

void draw_bitmap_text_2x(
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
                if ((row_bits & (1U << (uint32_t)column)) != 0U) {
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

int32_t measure_text_2x_width(const char *text) {
    return (int32_t)strlen(text) * FONT_GLYPH_WIDTH;
}

void draw_ui_button(
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

void set_status_message(EditorState *state, const char *message) {
    const int written = snprintf(state->status_message, sizeof(state->status_message), "%s", message);
    if (written < 0) {
        state->status_message[0] = '\0';
    }
    state->needs_redraw = true;
}

void clear_segment_selection(EditorState *state) {
    free(state->selection_mask);
    state->selection_mask = nullptr;
    state->selection_mask_length = 0U;
    state->selected_segment_count = 0U;
}

void clear_hover_preview(EditorState *state) {
    free(state->hover_mask);
    state->hover_mask = nullptr;
    state->hover_mask_length = 0U;
    state->hover_seed_chain = SIZE_MAX;
    state->has_hover_cross_box = false;
}

void clear_floating_fragment(EditorState *state) {
    free_stitch_design(&state->floating_fragment);
    state->has_floating_fragment = false;
    state->floating_is_grid_origin_aligned = false;
    state->floating_offset_x = 0;
    state->floating_offset_y = 0;
}

void clear_live_preview(EditorState *state) {
    free(state->edit_preview_keep_mask);
    state->edit_preview_keep_mask = nullptr;
    state->edit_preview_mask_length = 0U;
    state->has_edit_preview_mask = false;
    state->preview_dirty = false;
}

bool ensure_selection_mask_allocated(EditorState *state) {
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

void recount_selected_segments(EditorState *state) {
    size_t count = 0U;
    for (size_t segment_index = 0U; segment_index < state->selection_mask_length; segment_index += 1U) {
        if (state->selection_mask[segment_index] != 0U) {
            count += 1U;
        }
    }
    state->selected_segment_count = count;
}

void strip_history_push(EditorState *state) {
    if (state->strip_history.state_count == MAX_STRIP_HISTORY_STATES) {
        for (size_t shift_index = 1U; shift_index < MAX_STRIP_HISTORY_STATES; shift_index += 1U) {
            state->strip_history.states[shift_index - 1U] = state->strip_history.states[shift_index];
        }
        state->strip_history.state_count = MAX_STRIP_HISTORY_STATES - 1U;
    }
    state->strip_history.states[state->strip_history.state_count] = state->strip_pattern;
    state->strip_history.state_count += 1U;
}

RenderViewport get_editor_viewport(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    RenderViewport viewport = {};
    viewport.left = LEFT_PANEL_WIDTH;
    viewport.top = state->toolbar_height;
    viewport.width = window_width - viewport.left;
    viewport.height = window_height - viewport.top - BOTTOM_PANEL_HEIGHT;
    if (viewport.width < 64) {
        viewport.width = 64;
    }
    if (viewport.height < 64) {
        viewport.height = 64;
    }
    return viewport;
}

int32_t editor_world_to_screen_x(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t world_x
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    return viewport_world_to_screen_x(viewport, state->camera, world_x);
}

int32_t editor_world_to_screen_y(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t world_y
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    return viewport_world_to_screen_y(viewport, state->camera, world_y);
}

int32_t editor_screen_to_world_x(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t screen_x
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    return viewport_screen_to_world_x(viewport, state->camera, screen_x);
}

int32_t editor_screen_to_world_y(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height,
    int32_t screen_y
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    return viewport_screen_to_world_y(viewport, state->camera, screen_y);
}

int32_t snap_coordinate_to_grid(int32_t value, int32_t origin, int32_t pitch) {
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        return value;
    }
    const int32_t cell_index = floor_divide_int32(value - origin + pitch / 2, pitch);
    return origin + cell_index * pitch;
}

void fit_camera_to_design(EditorState *state, int32_t window_width, int32_t window_height) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);

    if (!state->design.has_bounds) {
        state->camera.center_x = 0.0;
        state->camera.center_y = 0.0;
        state->camera.pixels_per_unit = 0.5;
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

    const double scale_x = ((double)viewport.width * 0.86) / design_width;
    const double scale_y = ((double)viewport.height * 0.86) / design_height;
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

    state->camera.center_x =
        ((double)state->design.minimum_x + (double)state->design.maximum_x) * 0.5;
    state->camera.center_y =
        ((double)state->design.minimum_y + (double)state->design.maximum_y) * 0.5;
    state->camera.pixels_per_unit = best_scale;
    state->needs_redraw = true;
}

bool tool_uses_box_preview(EditorTool tool) {
    return tool == TOOL_CROP_KEEP ||
           tool == TOOL_CUT_OUT ||
           tool == TOOL_SPLIT_VERTICAL ||
           tool == TOOL_SPLIT_HORIZONTAL;
}

EditOperationKind tool_to_edit_operation(EditorTool tool) {
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

void refresh_live_preview(EditorState *state) {
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

bool should_show_start_screen(const EditorState *state) {
    return state->design.chain_count == 0U &&
           state->design.block_count == 0U &&
           !state->has_floating_fragment &&
           !state->strip_modal_open &&
           state->palette_target == PALETTE_TARGET_NONE &&
           state->file_dialog.mode == FILE_DIALOG_CLOSED;
}

void update_slider_from_mouse(EditorState *state, int32_t mouse_x) {
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

const char *basename_of_path(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash != nullptr && last_slash[1] != '\0') {
        return last_slash + 1;
    }
    return path;
}

bool directory_exists(const char *directory_path) {
    struct stat path_info = {};
    if (stat(directory_path, &path_info) != 0) {
        return false;
    }
    return S_ISDIR(path_info.st_mode);
}

bool file_has_embroidery_extension(const char *file_name) {
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
    size_t character_index = 0U;
    while (left->name[character_index] != '\0' && right->name[character_index] != '\0') {
        const unsigned char left_char = ascii_lowercase((unsigned char)left->name[character_index]);
        const unsigned char right_char = ascii_lowercase((unsigned char)right->name[character_index]);
        if (left_char < right_char) {
            return -1;
        }
        if (left_char > right_char) {
            return 1;
        }
        character_index += 1U;
    }
    if (left->name[character_index] == '\0' && right->name[character_index] == '\0') {
        return 0;
    }
    if (left->name[character_index] == '\0') {
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
        size_t next_capacity = 0U;
        if (dialog->entry_capacity == 0U) {
            next_capacity = 128U;
        } else if (ckd_mul(&next_capacity, dialog->entry_capacity, (size_t)2U)) {
            return false;
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
    const int name_written = snprintf(target->name, sizeof(target->name), "%.255s", entry_name);
    if (name_written < 0) {
        target->name[0] = '\0';
    }
    target->is_directory = is_directory;
    target->file_size_bytes = file_size_bytes;
    dialog->entry_count += 1U;
    return true;
}

bool load_file_browser_directory(EditorState *state, const char *directory_path) {
    bool succeeded = false;
    FileDialogState *dialog = &state->file_dialog;
    DIR *directory_stream = opendir(directory_path);
    if (directory_stream == nullptr) {
        const int error_written =
            snprintf(dialog->error_text, sizeof(dialog->error_text), "Cannot open directory");
        if (error_written < 0) {
            dialog->error_text[0] = '\0';
        }
        state->needs_redraw = true;
        goto cleanup;
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
    if (clean_length > (size_t)INT_MAX) {
        goto cleanup;
    }
    const int path_written = snprintf(
        dialog->current_directory,
        sizeof(dialog->current_directory),
        "%.*s",
        (int)clean_length,
        directory_path
    );
    if (path_written < 0 || (size_t)path_written >= sizeof(dialog->current_directory)) {
        goto cleanup;
    }

    for (struct dirent *entry = readdir(directory_stream);
         entry != nullptr;
         entry = readdir(directory_stream)) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char full_path[MAX_PATH_BUFFER] = {};
        int joined = 0;
        if (strcmp(dialog->current_directory, "/") == 0) {
            joined = snprintf(full_path, sizeof(full_path), "/%s", entry->d_name);
        } else {
            joined = snprintf(full_path, sizeof(full_path), "%s/%s", dialog->current_directory, entry->d_name);
        }
        if (joined <= 0 || (size_t)joined >= sizeof(full_path)) {
            continue;
        }
        struct stat entry_stat = {};
        if (stat(full_path, &entry_stat) != 0) {
            continue;
        }
        const bool is_dir = S_ISDIR(entry_stat.st_mode);
        if (!is_dir && !file_has_embroidery_extension(entry->d_name)) {
            continue;
        }
        uint64_t byte_size = 0ULL;
        if (entry_stat.st_size > 0) {
            byte_size = (uint64_t)entry_stat.st_size;
        }
        (void)file_dialog_append_entry(dialog, entry->d_name, is_dir, byte_size);
    }

    if (dialog->entry_count > 1U) {
        qsort(dialog->entries, dialog->entry_count, sizeof(FileBrowserEntry), compare_file_browser_entries);
    }
    state->needs_redraw = true;
    succeeded = true;

cleanup:
    if (directory_stream != nullptr) {
        closedir(directory_stream);
    }
    return succeeded;
}

void close_file_dialog_state(EditorState *state) {
    if (state->file_dialog.mode == FILE_DIALOG_SAVE_AS ||
        state->file_dialog.mode == FILE_DIALOG_STRIP_SAVE_AS) {
        state->text_input_active = false;
    }
    state->file_dialog.mode = FILE_DIALOG_CLOSED;
    state->file_dialog.confirm_overwrite = false;
    state->file_dialog.error_text[0] = '\0';
    state->needs_redraw = true;
}

void open_file_dialog_state(EditorState *state, FileDialogMode mode) {
    FileDialogState *dialog = &state->file_dialog;
    dialog->mode = mode;
    dialog->confirm_overwrite = false;
    dialog->error_text[0] = '\0';

    char start_directory[MAX_PATH_BUFFER] = {};
    if (dialog->current_directory[0] != '\0' && directory_exists(dialog->current_directory)) {
        const int written = snprintf(start_directory, sizeof(start_directory), "%s", dialog->current_directory);
        if (written < 0) {
            start_directory[0] = '\0';
        }
    } else if (state->has_file_path) {
        const char *last_slash = strrchr(state->current_file_path, '/');
        if (last_slash != nullptr) {
            size_t dir_length = (size_t)(last_slash - state->current_file_path);
            if (dir_length == 0U) {
                dir_length = 1U;
            }
            if (dir_length <= (size_t)INT_MAX) {
                const int written = snprintf(
                    start_directory,
                    sizeof(start_directory),
                    "%.*s",
                    (int)dir_length,
                    state->current_file_path
                );
                if (written < 0) {
                    start_directory[0] = '\0';
                }
            }
        }
    }

    if (start_directory[0] == '\0' || !directory_exists(start_directory)) {
        const char *home_dir = getenv("HOME");
        if (home_dir != nullptr) {
            char downloads_dir[MAX_PATH_BUFFER] = {};
            const int dl_written = snprintf(downloads_dir, sizeof(downloads_dir), "%s/Downloads", home_dir);
            if (dl_written > 0 && (size_t)dl_written < sizeof(downloads_dir) && directory_exists(downloads_dir)) {
                (void)snprintf(start_directory, sizeof(start_directory), "%s", downloads_dir);
            } else {
                (void)snprintf(start_directory, sizeof(start_directory), "%s", home_dir);
            }
        } else if (getcwd(start_directory, sizeof(start_directory)) == nullptr) {
            (void)snprintf(start_directory, sizeof(start_directory), "/");
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
            (void)snprintf(
                dialog->filename_input,
                sizeof(dialog->filename_input),
                "%.*s_edited.jef",
                (int)stem_len,
                base_name
            );
            default_name = nullptr;
        }
        if (default_name != nullptr) {
            (void)snprintf(dialog->filename_input, sizeof(dialog->filename_input), "%s", default_name);
        }
        state->text_input_active = true;
    } else {
        dialog->filename_input[0] = '\0';
        state->text_input_active = false;
    }

    (void)load_file_browser_directory(state, start_directory);
}

void file_dialog_navigate_parent(EditorState *state) {
    FileDialogState *dialog = &state->file_dialog;
    char parent_path[MAX_PATH_BUFFER] = {};
    const char *last_slash = strrchr(dialog->current_directory, '/');
    if (last_slash == nullptr || last_slash == dialog->current_directory) {
        (void)snprintf(parent_path, sizeof(parent_path), "/");
    } else {
        const size_t parent_len = (size_t)(last_slash - dialog->current_directory);
        if (parent_len <= (size_t)INT_MAX) {
            (void)snprintf(parent_path, sizeof(parent_path), "%.*s", (int)parent_len, dialog->current_directory);
        }
    }
    if (parent_path[0] != '\0') {
        (void)load_file_browser_directory(state, parent_path);
    }
}

void file_dialog_navigate_home(EditorState *state) {
    const char *home_dir = getenv("HOME");
    if (home_dir != nullptr) {
        (void)load_file_browser_directory(state, home_dir);
    }
}

void file_dialog_navigate_downloads(EditorState *state) {
    const char *home_dir = getenv("HOME");
    if (home_dir == nullptr) {
        return;
    }
    char downloads_dir[MAX_PATH_BUFFER] = {};
    const int dl_written = snprintf(downloads_dir, sizeof(downloads_dir), "%s/Downloads", home_dir);
    if (dl_written > 0 && (size_t)dl_written < sizeof(downloads_dir) && directory_exists(downloads_dir)) {
        (void)load_file_browser_directory(state, downloads_dir);
    } else {
        (void)load_file_browser_directory(state, home_dir);
    }
}

void file_dialog_append_text_input(EditorState *state, const char *utf8_text) {
    if (state->file_dialog.mode != FILE_DIALOG_SAVE_AS &&
        state->file_dialog.mode != FILE_DIALOG_STRIP_SAVE_AS) {
        return;
    }
    FileDialogState *dialog = &state->file_dialog;
    size_t current_len = strlen(dialog->filename_input);
    for (size_t char_index = 0U; utf8_text[char_index] != '\0'; char_index += 1U) {
        const unsigned char byte = (unsigned char)utf8_text[char_index];
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

void file_dialog_backspace_input(EditorState *state) {
    if (state->file_dialog.mode != FILE_DIALOG_SAVE_AS &&
        state->file_dialog.mode != FILE_DIALOG_STRIP_SAVE_AS) {
        return;
    }
    const size_t length = strlen(state->file_dialog.filename_input);
    if (length > 0U) {
        state->file_dialog.filename_input[length - 1U] = '\0';
        state->file_dialog.confirm_overwrite = false;
        state->file_dialog.error_text[0] = '\0';
        state->needs_redraw = true;
    }
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
    (void)snprintf(button->label, sizeof(button->label), "%.31s", label);
    state->toolbar_button_count += 1U;

    *cursor_x += button_width + TOOLBAR_BUTTON_GAP;
}

void layout_toolbar_buttons(EditorState *state, int32_t window_width) {
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
    size_t thickness_index = state->thread_thickness_index;
    if (thickness_index >= THREAD_THICKNESS_PRESET_COUNT) {
        thickness_index = 1U;
    }
    add_toolbar_button(
        state,
        &cursor_x,
        &cursor_y,
        window_width,
        COMMAND_CYCLE_THREAD_THICKNESS,
        thread_thickness_labels[thickness_index],
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
    (void)snprintf(hoop_label, sizeof(hoop_label), "Hoop:%s", janome_550e_hoops[state->hoop_preset_index].label);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_CYCLE_HOOP, hoop_label, state->hoop_preset_index > 0U, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_ROTATE_HOOP, "Rot Hoop", state->hoop_rotated, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOGGLE_GRID, "Grid", state->show_grid, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOGGLE_JUMPS, "Jumps", state->show_jumps, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_TOGGLE_LOCKS, "Locks", state->show_locks, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_PITCH_DECREASE, "-", false, false, false);

    char pitch_label[32] = {};
    (void)snprintf(pitch_label, sizeof(pitch_label), "%.1fmm", (double)state->grid.cross_pitch_units * 0.1);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_NONE, pitch_label, false, false, true);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_PITCH_INCREASE, "+", false, false, false);

    char passes_label[32] = {};
    (void)snprintf(passes_label, sizeof(passes_label), "Pass:%dx", state->grid.cross_passes);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_CYCLE_PASSES, passes_label, false, false, false);
    add_toolbar_button(state, &cursor_x, &cursor_y, window_width, COMMAND_FIT_VIEW, "Fit", false, false, false);

    state->toolbar_height = cursor_y + TOOLBAR_BUTTON_HEIGHT + TOOLBAR_PADDING;
}

StartScreenLayout compute_start_screen_layout(
    const EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);

    StartScreenLayout layout = {};
    layout.card_width = 720;
    if (layout.card_width > viewport.width - 32) {
        layout.card_width = viewport.width - 32;
    }
    layout.card_height = 260;
    layout.card_left = viewport.left + (viewport.width - layout.card_width) / 2;
    layout.card_top = viewport.top + (viewport.height - layout.card_height) / 2;

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
    (void)snprintf(layout.open_button.label, sizeof(layout.open_button.label), "Open File");

    layout.strip_button.left_x = pair_left + button_width + button_gap;
    layout.strip_button.top_y = button_top;
    layout.strip_button.width = button_width;
    layout.strip_button.height = button_height;
    layout.strip_button.is_accent = true;
    (void)snprintf(layout.strip_button.label, sizeof(layout.strip_button.label), "New Strip");

    return layout;
}

PaletteModalLayout compute_palette_modal_layout(int32_t window_width, int32_t window_height) {
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
    (void)snprintf(button->label, sizeof(button->label), "%.31s", label);
    *button_count += 1U;
}

StripModalLayout compute_strip_modal_layout(
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

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 64, 30, STRIP_CMD_WIDTH_DEC, "W -", false, false, false, false, false, no_swatch);
    cursor_x += 68;
    (void)snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.width);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 48, 30, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += 52;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 32, 30, STRIP_CMD_WIDTH_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += 44;

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 64, 30, STRIP_CMD_HEIGHT_DEC, "H -", false, false, false, false, false, no_swatch);
    cursor_x += 68;
    (void)snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.height);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 48, 30, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += 52;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 32, 30, STRIP_CMD_HEIGHT_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += 44;

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 108, 30, STRIP_CMD_REPEAT_X_DEC, "RepX -", false, false, false, false, false, no_swatch);
    cursor_x += 112;
    (void)snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.repeat_x);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 48, 30, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += 52;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 32, 30, STRIP_CMD_REPEAT_X_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += 44;

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 108, 30, STRIP_CMD_REPEAT_Y_DEC, "RepY -", false, false, false, false, false, no_swatch);
    cursor_x += 112;
    (void)snprintf(value_text, sizeof(value_text), "%d", state->strip_pattern.repeat_y);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 48, 30, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += 52;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_one_y, 32, 30, STRIP_CMD_REPEAT_Y_INC, "+", false, false, false, false, false, no_swatch);

    const int32_t row_two_y = row_one_y + 36;
    cursor_x = layout.modal_left + 14;

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 108, 30, STRIP_CMD_PITCH_DEC, "Size -", false, false, false, false, false, no_swatch);
    cursor_x += 112;
    (void)snprintf(value_text, sizeof(value_text), "%.1fmm", (double)state->strip_pattern.pitch_units * 0.1);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 92, 30, STRIP_CMD_NONE, value_text, false, false, false, true, false, no_swatch);
    cursor_x += 96;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 32, 30, STRIP_CMD_PITCH_INC, "+", false, false, false, false, false, no_swatch);
    cursor_x += 42;

    (void)snprintf(value_text, sizeof(value_text), "Pass:%dx", state->strip_pattern.passes);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 124, 30, STRIP_CMD_CYCLE_PASSES, value_text, false, false, false, false, false, no_swatch);
    cursor_x += 130;

    uint8_t active_color = state->strip_active_color;
    if (active_color == 0U || active_color >= JANOME_PALETTE_COUNT) {
        active_color = 10U;
    }
    (void)snprintf(value_text, sizeof(value_text), "Col#%02u", (unsigned int)active_color);
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 134, 30, STRIP_CMD_PICK_COLOR, value_text, false, false, false, false, true, janome_palette[active_color]);
    cursor_x += 140;

    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 92, 30, STRIP_CMD_CLEAR, "Clear", false, false, false, false, false, no_swatch);
    cursor_x += 98;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 78, 30, STRIP_CMD_MIRROR_X, "MirX", false, false, false, false, false, no_swatch);
    cursor_x += 84;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 78, 30, STRIP_CMD_MIRROR_Y, "MirY", false, false, false, false, false, no_swatch);
    cursor_x += 84;
    add_modal_button(layout.buttons, &layout.button_count, MAX_STRIP_BUTTONS, cursor_x, row_two_y, 78, 30, STRIP_CMD_UNDO, "Undo", false, false, false, false, false, no_swatch);

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

FileDialogLayout compute_file_dialog_layout(
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

uint8_t hit_test_toolbar(const EditorState *state, int32_t mouse_x, int32_t mouse_y) {
    if (mouse_y >= state->toolbar_height) {
        return COMMAND_NONE;
    }
    for (size_t button_index = 0U; button_index < state->toolbar_button_count; button_index += 1U) {
        const UiButton *button = &state->toolbar_buttons[button_index];
        if (!button->is_readout &&
            point_is_in_box(mouse_x, mouse_y, button->left_x, button->top_y, button->width, button->height)) {
            return button->command_id;
        }
    }
    return COMMAND_NONE;
}

bool hit_test_slider(const EditorState *state, int32_t mouse_x, int32_t mouse_y) {
    return mouse_y >= state->slider_top_y - 4 &&
           mouse_y <= state->slider_bottom_y + 4 &&
           mouse_x >= state->slider_left_x - 8 &&
           mouse_x <= state->slider_right_x + 8;
}

LeftPanelHitKind hit_test_left_panel(
    const EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_height,
    size_t *block_index_out
) {
    if (block_index_out != nullptr) {
        *block_index_out = SIZE_MAX;
    }
    const int32_t panel_top = state->toolbar_height;
    const int32_t panel_bottom = window_height - BOTTOM_PANEL_HEIGHT;
    if (mouse_x >= LEFT_PANEL_WIDTH || mouse_y < panel_top || mouse_y >= panel_bottom) {
        return LEFT_PANEL_HIT_NONE;
    }
    if (point_is_in_box(mouse_x, mouse_y, 10, panel_top + 10, LEFT_PANEL_WIDTH - 20, 34)) {
        return LEFT_PANEL_HIT_ADD_COLOR;
    }

    const int32_t list_top = panel_top + 54;
    if (mouse_y < list_top) {
        return LEFT_PANEL_HIT_PANEL_EMPTY;
    }
    const int32_t row_index_signed = (mouse_y - list_top + state->color_scroll_offset) / COLOR_ROW_HEIGHT;
    if (row_index_signed < 0 || (size_t)row_index_signed >= state->design.block_count) {
        return LEFT_PANEL_HIT_PANEL_EMPTY;
    }
    const size_t block_index = (size_t)row_index_signed;
    if (block_index_out != nullptr) {
        *block_index_out = block_index;
    }

    const int32_t row_y = list_top + row_index_signed * COLOR_ROW_HEIGHT - state->color_scroll_offset;
    const int32_t buttons_start_x =
        LEFT_PANEL_WIDTH - 12 - (5 * LEFT_ROW_BUTTON_WIDTH + 4 * LEFT_ROW_BUTTON_GAP);
    const int32_t button_y = row_y + 6;
    static const LeftPanelHitKind action_kinds[5] = {
        LEFT_PANEL_HIT_TOGGLE_VISIBILITY,
        LEFT_PANEL_HIT_MOVE_UP,
        LEFT_PANEL_HIT_MOVE_DOWN,
        LEFT_PANEL_HIT_RECOLOR_BLOCK,
        LEFT_PANEL_HIT_DELETE_BLOCK
    };
    for (int32_t action_index = 0; action_index < 5; action_index += 1) {
        const int32_t button_x =
            buttons_start_x + action_index * (LEFT_ROW_BUTTON_WIDTH + LEFT_ROW_BUTTON_GAP);
        if (point_is_in_box(
                mouse_x,
                mouse_y,
                button_x,
                button_y,
                LEFT_ROW_BUTTON_WIDTH,
                LEFT_ROW_BUTTON_HEIGHT
            )) {
            return action_kinds[action_index];
        }
    }
    return LEFT_PANEL_HIT_SELECT_BLOCK;
}

bool hit_test_palette_modal(
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_width,
    int32_t window_height,
    int32_t *palette_index_out
) {
    *palette_index_out = -1;
    const PaletteModalLayout layout = compute_palette_modal_layout(window_width, window_height);
    if (!point_is_in_box(mouse_x, mouse_y, layout.modal_left, layout.modal_top, layout.modal_width, layout.modal_height)) {
        return false;
    }
    for (int32_t palette_index = 0; palette_index < (int32_t)JANOME_PALETTE_COUNT; palette_index += 1) {
        const int32_t column = palette_index % layout.columns;
        const int32_t row = palette_index / layout.columns;
        const int32_t cell_x = layout.grid_left + column * (layout.cell_size + layout.cell_gap);
        const int32_t cell_y = layout.grid_top + row * (layout.cell_size + layout.cell_gap);
        if (point_is_in_box(mouse_x, mouse_y, cell_x, cell_y, layout.cell_size, layout.cell_size)) {
            *palette_index_out = palette_index;
            return true;
        }
    }
    return true;
}

uint8_t hit_test_strip_modal_button(
    const StripModalLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y
) {
    for (size_t button_index = 0U; button_index < layout->button_count; button_index += 1U) {
        const UiButton *button = &layout->buttons[button_index];
        if (!button->is_readout &&
            point_is_in_box(mouse_x, mouse_y, button->left_x, button->top_y, button->width, button->height)) {
            return button->command_id;
        }
    }
    return STRIP_CMD_NONE;
}

bool hit_test_strip_editor_cell(
    const StripPattern *pattern,
    const StripModalLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t *column_out,
    int32_t *row_out
) {
    const int32_t grid_pixel_width = pattern->width * layout->editor_cell_size;
    const int32_t grid_pixel_height = pattern->height * layout->editor_cell_size;
    const int32_t grid_origin_x = layout->editor_left + (layout->editor_width - grid_pixel_width) / 2;
    const int32_t grid_origin_y = layout->editor_top + (layout->editor_height - grid_pixel_height) / 2;

    if (!point_is_in_box(mouse_x, mouse_y, grid_origin_x, grid_origin_y, grid_pixel_width, grid_pixel_height)) {
        return false;
    }
    const int32_t column = (mouse_x - grid_origin_x) / layout->editor_cell_size;
    const int32_t row = (mouse_y - grid_origin_y) / layout->editor_cell_size;
    if (column < 0 || column >= pattern->width || row < 0 || row >= pattern->height) {
        return false;
    }
    *column_out = column;
    *row_out = row;
    return true;
}

uint8_t hit_test_file_dialog_button(
    const FileDialogLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y
) {
    for (size_t button_index = 0U; button_index < layout->button_count; button_index += 1U) {
        const UiButton *button = &layout->buttons[button_index];
        if (point_is_in_box(mouse_x, mouse_y, button->left_x, button->top_y, button->width, button->height)) {
            return button->command_id;
        }
    }
    return DIALOG_CMD_NONE;
}

bool hit_test_file_dialog_entry(
    const EditorState *state,
    const FileDialogLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y,
    size_t *entry_index_out
) {
    if (!point_is_in_box(mouse_x, mouse_y, layout->list_left, layout->list_top, layout->list_width, layout->list_height)) {
        return false;
    }
    const int32_t row_signed =
        (mouse_y - layout->list_top + state->file_dialog.scroll_offset) / FILE_ROW_HEIGHT;
    if (row_signed < 0 || (size_t)row_signed >= state->file_dialog.entry_count) {
        return false;
    }
    *entry_index_out = (size_t)row_signed;
    return true;
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
    (void)snprintf(add_button.label, sizeof(add_button.label), "+ Color");
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

    static const char *const row_symbols[5] = {"V", "^", "v", "C", "X"};
    const int32_t buttons_start_x =
        LEFT_PANEL_WIDTH - 12 - (5 * LEFT_ROW_BUTTON_WIDTH + 4 * LEFT_ROW_BUTTON_GAP);

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

        const int32_t button_y = row_y + 6;
        for (int32_t action_index = 0; action_index < 5; action_index += 1) {
            const int32_t button_x =
                buttons_start_x + action_index * (LEFT_ROW_BUTTON_WIDTH + LEFT_ROW_BUTTON_GAP);
            const bool hovered = point_is_in_box(
                state->mouse_screen_x,
                state->mouse_screen_y,
                button_x,
                button_y,
                LEFT_ROW_BUTTON_WIDTH,
                LEFT_ROW_BUTTON_HEIGHT
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
            draw_filled_rectangle(canvas, button_x, button_y, LEFT_ROW_BUTTON_WIDTH, LEFT_ROW_BUTTON_HEIGHT, button_background);
            draw_rectangle_outline(canvas, button_x, button_y, LEFT_ROW_BUTTON_WIDTH, LEFT_ROW_BUTTON_HEIGHT, pack_rgb_pixel(110U, 118U, 132U), 0);
            draw_bitmap_text_2x(canvas, button_x + 9, button_y + 6, symbol, text_color);
        }
    }

    reset_canvas_clip(canvas);
}

static void render_bottom_panel(PixelCanvas *canvas, EditorState *state, DesignStats stats) {
    const int32_t panel_top = canvas->height - BOTTOM_PANEL_HEIGHT;
    draw_filled_rectangle(canvas, 0, panel_top, canvas->width, BOTTOM_PANEL_HEIGHT, pack_rgb_pixel(28U, 31U, 37U));
    draw_line_clipped(canvas, 0, panel_top, canvas->width, panel_top, pack_rgb_pixel(72U, 78U, 90U), 0);

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
        (double)editor_screen_to_world_x(state, canvas->width, canvas->height, state->mouse_screen_x) * 0.1;
    const double cursor_y_mm =
        (double)editor_screen_to_world_y(state, canvas->width, canvas->height, state->mouse_screen_y) * 0.1;

    char metrics_line[256] = {};
    (void)snprintf(
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
    const bool out_of_hoop =
        design_exceeds_hoop(&state->design, state->hoop_preset_index, state->hoop_rotated);
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
        const uint32_t luminance = compute_rgb_luminance(color);
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
        (void)snprintf(
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
    (void)snprintf(summary, sizeof(summary), "%.1fx%.1fmm (%dx%d)", final_width_mm, final_height_mm, total_columns, total_rows);
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

    for (size_t button_index = 0U; button_index < layout.button_count; button_index += 1U) {
        draw_ui_button(canvas, &layout.buttons[button_index], state->mouse_screen_x, state->mouse_screen_y);
    }

    const int32_t path_y = layout.modal_top + 80;
    size_t max_path_chars = (size_t)((layout.modal_width - 36) / FONT_GLYPH_WIDTH);
    if (max_path_chars > 250U) {
        max_path_chars = 250U;
    }
    const char *dir_text = state->file_dialog.current_directory;
    const size_t dir_len = strlen(dir_text);
    char visible_path[256] = {};
    if (dir_len <= max_path_chars) {
        (void)snprintf(visible_path, sizeof(visible_path), "%.250s", dir_text);
    } else if (max_path_chars > 4U) {
        const size_t tail_chars = max_path_chars - 3U;
        (void)snprintf(visible_path, sizeof(visible_path), "...%.250s", dir_text + (dir_len - tail_chars));
    }
    draw_bitmap_text_2x(canvas, layout.modal_left + 16, path_y, visible_path, pack_rgb_pixel(186U, 214U, 244U));

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
                (void)snprintf(display_name, sizeof(display_name), "[D] %.255s", entry->name);
            } else {
                (void)snprintf(display_name, sizeof(display_name), "%.255s", entry->name);
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
                (void)snprintf(size_text, sizeof(size_text), "DIR");
            } else if (entry->file_size_bytes >= 1048576ULL) {
                (void)snprintf(size_text, sizeof(size_text), "%.1fM", (double)entry->file_size_bytes / 1048576.0);
            } else if (entry->file_size_bytes >= 1024ULL) {
                (void)snprintf(size_text, sizeof(size_text), "%.1fK", (double)entry->file_size_bytes / 1024.0);
            } else {
                (void)snprintf(size_text, sizeof(size_text), "%lluB", (unsigned long long)entry->file_size_bytes);
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
            (void)snprintf(visible_input, sizeof(visible_input), "%s_", state->file_dialog.filename_input);
        } else if (max_input_chars > 2U) {
            const size_t tail = max_input_chars - 1U;
            (void)snprintf(visible_input, sizeof(visible_input), "%s_", state->file_dialog.filename_input + (input_len - tail));
        }
        draw_bitmap_text_2x(canvas, layout.input_left + 8, layout.input_top + 9, visible_input, pack_rgb_pixel(246U, 250U, 255U));
    } else if (state->file_dialog.error_text[0] != '\0') {
        draw_bitmap_text_2x(canvas, layout.modal_left + 16, layout.modal_top + layout.modal_height - 38, state->file_dialog.error_text, pack_rgb_pixel(255U, 130U, 130U));
    }
}

static void render_draw_x_cursor_preview(
    PixelCanvas *canvas,
    const EditorState *state,
    RenderViewport viewport
) {
    const int32_t world_x = viewport_screen_to_world_x(viewport, state->camera, state->mouse_screen_x);
    const int32_t world_y = viewport_screen_to_world_y(viewport, state->camera, state->mouse_screen_y);
    int32_t cell_column = 0;
    int32_t cell_row = 0;
    world_to_grid_cell(world_x, world_y, state->grid, &cell_column, &cell_row);
    const int32_t pitch = state->grid.cross_pitch_units;
    const int32_t cell_min_x = state->grid.grid_origin_x + cell_column * pitch;
    const int32_t cell_min_y = state->grid.grid_origin_y + cell_row * pitch;
    const int32_t left_edge = viewport_world_to_screen_x(viewport, state->camera, cell_min_x);
    const int32_t right_edge = viewport_world_to_screen_x(viewport, state->camera, cell_min_x + pitch);
    const int32_t top_edge = viewport_world_to_screen_y(viewport, state->camera, cell_min_y);
    const int32_t bottom_edge = viewport_world_to_screen_y(viewport, state->camera, cell_min_y + pitch);

    RgbColor preview_rgb = janome_palette[10];
    if (state->active_color_block_index < state->design.block_count) {
        preview_rgb = state->design.blocks[state->active_color_block_index].display_color;
    }
    const uint32_t cross_color = pack_rgb_pixel(preview_rgb.red, preview_rgb.green, preview_rgb.blue);

    if (state->view_mode == VIEW_MODE_BLOCKS) {
        draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, preview_rgb.red, preview_rgb.green, preview_rgb.blue, 148U);
        draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, cross_color, 0);
        return;
    }

    if (state->view_mode == VIEW_MODE_THREAD) {
        draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, preview_rgb.red, preview_rgb.green, preview_rgb.blue, 55U);
        uint32_t preview_core = cross_color;
        uint32_t preview_outline = cross_color;
        compute_thread_colors(preview_rgb, false, &preview_core, &preview_outline);
        const int32_t preview_stroke =
            compute_thread_stroke_px(state->thread_thickness_index, state->camera.pixels_per_unit);
        if (preview_stroke >= 3) {
            draw_thread_capsule_clipped(canvas, left_edge, top_edge, right_edge, bottom_edge, preview_stroke, preview_core, preview_outline);
            draw_thread_capsule_clipped(canvas, right_edge, top_edge, left_edge, bottom_edge, preview_stroke, preview_core, preview_outline);
        } else {
            draw_thick_line_clipped(canvas, left_edge, top_edge, right_edge, bottom_edge, preview_core);
            draw_thick_line_clipped(canvas, right_edge, top_edge, left_edge, bottom_edge, preview_core);
        }
        draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, cross_color, 0);
        return;
    }

    draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, preview_rgb.red, preview_rgb.green, preview_rgb.blue, 70U);
    draw_thick_line_clipped(canvas, left_edge, top_edge, right_edge, bottom_edge, cross_color);
    draw_thick_line_clipped(canvas, right_edge, top_edge, left_edge, bottom_edge, cross_color);
    draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, cross_color, 0);
}

static void render_box_selection_overlay(
    PixelCanvas *canvas,
    const EditorState *state,
    RenderViewport viewport
) {
    if (!state->has_box_selection) {
        return;
    }
    if (state->active_tool == TOOL_SPLIT_VERTICAL) {
        const int32_t line_x = viewport_world_to_screen_x(viewport, state->camera, state->split_coordinate);
        draw_line_clipped(canvas, line_x, viewport.top, line_x, viewport.top + viewport.height, pack_rgb_pixel(230U, 52U, 36U), 6);
        draw_line_clipped(canvas, line_x + 1, viewport.top, line_x + 1, viewport.top + viewport.height, pack_rgb_pixel(230U, 52U, 36U), 6);
        return;
    }
    if (state->active_tool == TOOL_SPLIT_HORIZONTAL) {
        const int32_t line_y = viewport_world_to_screen_y(viewport, state->camera, state->split_coordinate);
        draw_line_clipped(canvas, viewport.left, line_y, viewport.left + viewport.width, line_y, pack_rgb_pixel(230U, 52U, 36U), 6);
        draw_line_clipped(canvas, viewport.left, line_y + 1, viewport.left + viewport.width, line_y + 1, pack_rgb_pixel(230U, 52U, 36U), 6);
        return;
    }

    int32_t left_edge = viewport_world_to_screen_x(viewport, state->camera, state->box_start_x);
    int32_t right_edge = viewport_world_to_screen_x(viewport, state->camera, state->box_end_x);
    int32_t top_edge = viewport_world_to_screen_y(viewport, state->camera, state->box_start_y);
    int32_t bottom_edge = viewport_world_to_screen_y(viewport, state->camera, state->box_end_y);
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

void render_editor_frame(PixelCanvas *canvas, EditorState *state) {
    layout_toolbar_buttons(state, canvas->width);
    refresh_live_preview(state);

    reset_canvas_clip(canvas);
    draw_filled_rectangle(canvas, 0, 0, canvas->width, canvas->height, pack_rgb_pixel(245U, 246U, 248U));

    draw_filled_rectangle(canvas, 0, 0, canvas->width, state->toolbar_height, pack_rgb_pixel(30U, 33U, 40U));
    draw_line_clipped(canvas, 0, state->toolbar_height - 1, canvas->width, state->toolbar_height - 1, pack_rgb_pixel(72U, 78U, 90U), 0);
    for (size_t button_index = 0U; button_index < state->toolbar_button_count; button_index += 1U) {
        draw_ui_button(canvas, &state->toolbar_buttons[button_index], state->mouse_screen_x, state->mouse_screen_y);
    }

    const RenderViewport viewport = get_editor_viewport(state, canvas->width, canvas->height);
    canvas->clip_min_x = viewport.left;
    canvas->clip_min_y = viewport.top;
    canvas->clip_max_x = viewport.left + viewport.width;
    canvas->clip_max_y = viewport.top + viewport.height;

    draw_cross_grid_background(canvas, viewport, state->camera, state->grid, state->show_grid);
    draw_hoop_overlay(canvas, viewport, state->camera, &state->design, state->hoop_preset_index, state->hoop_rotated);

    const DesignStats total_stats = compute_design_stats(&state->design);
    size_t max_visible_stitches = SIZE_MAX;
    if (state->simulator_percent < 99.95) {
        max_visible_stitches = (size_t)llround((double)total_stats.stitch_count * (state->simulator_percent / 100.0));
    }

    if (should_show_start_screen(state)) {
        render_start_screen(canvas, state);
    } else if (state->design.chain_count > 0U) {
        DesignRenderParams main_params = {
            .viewport = viewport,
            .camera = state->camera,
            .grid = state->grid,
            .view_mode = state->view_mode,
            .thread_thickness_index = state->thread_thickness_index,
            .show_jumps = state->show_jumps,
            .show_locks = state->show_locks,
            .draw_overlays = true,
            .world_offset_x = 0,
            .world_offset_y = 0,
            .keep_preview_mask = nullptr,
            .keep_preview_mask_length = 0U,
            .selection_mask = state->selection_mask,
            .selection_mask_length = state->selection_mask_length,
            .hover_mask = state->hover_mask,
            .hover_mask_length = state->hover_mask_length,
            .maximum_stitches_to_draw = max_visible_stitches,
            .block_dedup = &state->block_dedup
        };
        if (state->has_edit_preview_mask && state->edit_preview_keep_mask != nullptr) {
            main_params.keep_preview_mask = state->edit_preview_keep_mask;
            main_params.keep_preview_mask_length = state->edit_preview_mask_length;
        }
        render_design_chains(canvas, &state->design, &main_params);
    }

    if (state->has_floating_fragment) {
        const DesignRenderParams fragment_params = {
            .viewport = viewport,
            .camera = state->camera,
            .grid = state->grid,
            .view_mode = state->view_mode,
            .thread_thickness_index = state->thread_thickness_index,
            .show_jumps = false,
            .show_locks = false,
            .draw_overlays = false,
            .world_offset_x = state->floating_offset_x,
            .world_offset_y = state->floating_offset_y,
            .keep_preview_mask = nullptr,
            .keep_preview_mask_length = 0U,
            .selection_mask = nullptr,
            .selection_mask_length = 0U,
            .hover_mask = nullptr,
            .hover_mask_length = 0U,
            .maximum_stitches_to_draw = SIZE_MAX,
            .block_dedup = &state->block_dedup
        };
        render_design_chains(canvas, &state->floating_fragment, &fragment_params);
        if (state->floating_fragment.has_bounds) {
            const int32_t box_left = viewport_world_to_screen_x(viewport, state->camera, state->floating_fragment.minimum_x + state->floating_offset_x);
            const int32_t box_right = viewport_world_to_screen_x(viewport, state->camera, state->floating_fragment.maximum_x + state->floating_offset_x);
            const int32_t box_top = viewport_world_to_screen_y(viewport, state->camera, state->floating_fragment.minimum_y + state->floating_offset_y);
            const int32_t box_bottom = viewport_world_to_screen_y(viewport, state->camera, state->floating_fragment.maximum_y + state->floating_offset_y);
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
        viewport.left,
        viewport.top,
        viewport.width,
        viewport.height
    );
    if (no_modal_open && !state->has_floating_fragment && mouse_in_viewport) {
        if (state->active_tool == TOOL_SELECT_CROSS && state->has_hover_cross_box) {
            const int32_t left_edge = viewport_world_to_screen_x(viewport, state->camera, state->hover_cross_box.minimum_x);
            const int32_t right_edge = viewport_world_to_screen_x(viewport, state->camera, state->hover_cross_box.maximum_x);
            const int32_t top_edge = viewport_world_to_screen_y(viewport, state->camera, state->hover_cross_box.minimum_y);
            const int32_t bottom_edge = viewport_world_to_screen_y(viewport, state->camera, state->hover_cross_box.maximum_y);
            draw_blended_rectangle(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, 0U, 180U, 255U, 54U);
            draw_rectangle_outline(canvas, left_edge, top_edge, right_edge - left_edge + 1, bottom_edge - top_edge + 1, pack_rgb_pixel(0U, 140U, 232U), 0);
        } else if (state->active_tool == TOOL_DRAW_CROSS) {
            render_draw_x_cursor_preview(canvas, state, viewport);
        }
    }

    render_box_selection_overlay(canvas, state, viewport);

    reset_canvas_clip(canvas);
    render_left_color_panel(canvas, state);
    render_bottom_panel(canvas, state, total_stats);
    render_strip_modal(canvas, state);
    render_file_dialog(canvas, state);
    render_palette_modal(canvas, state);
}

void init_default_editor_state(EditorState *state) {
    *state = (EditorState){};
    history_init(&state->history);
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
    (void)init_block_dedup_table(&state->block_dedup);
    state->show_grid = true;
    state->show_jumps = false;
    state->show_locks = false;
    state->hoop_preset_index = 4U;
    state->simulator_percent = 100.0;
    state->camera.pixels_per_unit = 0.5;
    state->hover_seed_chain = SIZE_MAX;
    state->hovered_palette_index = -1;
    state->file_dialog.selected_index = SIZE_MAX;
    state->file_dialog.last_click_index = SIZE_MAX;
    state->needs_redraw = true;
    set_status_message(state, "Ready | Open File, New Strip, or Drag & Drop");
}

void free_editor_state(EditorState *state) {
    free_block_dedup_table(&state->block_dedup);
    free(state->file_dialog.entries);
    state->file_dialog.entries = nullptr;
    state->file_dialog.entry_count = 0U;
    state->file_dialog.entry_capacity = 0U;
    clear_segment_selection(state);
    clear_hover_preview(state);
    clear_floating_fragment(state);
    clear_live_preview(state);
    history_clear(&state->history);
    free_stitch_design(&state->design);
}

