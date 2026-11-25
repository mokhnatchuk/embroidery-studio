#define _POSIX_C_SOURCE 200809L

#include "formats.h"
#include "model.h"
#include "ops.h"
#include "render.h"
#include "ui.h"

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sync_sdl_text_input(bool *sdl_text_input_enabled, bool desired_active) {
    if (*sdl_text_input_enabled == desired_active) {
        return;
    }
    if (desired_active) {
        SDL_StartTextInput();
    } else {
        SDL_StopTextInput();
    }
    *sdl_text_input_enabled = desired_active;
}

static bool load_design_into_editor(
    EditorState *state,
    const char *file_path,
    int32_t window_width,
    int32_t window_height
) {
    StitchDesign loaded = {};
    if (!load_embroidery_file(file_path, &loaded)) {
        set_status_message(state, "Failed to load embroidery file");
        return false;
    }

    clear_segment_selection(state);
    clear_hover_preview(state);
    clear_floating_fragment(state);
    clear_live_preview(state);
    state->has_box_selection = false;
    state->is_dragging_box = false;

    free_stitch_design(&state->design);
    state->design = loaded;
    history_clear(&state->history);
    (void)history_push(&state->history, &state->design);

    state->grid = detect_cross_stitch_grid(&state->design);
    state->active_color_block_index = 0U;
    state->color_scroll_offset = 0;
    state->simulator_percent = 100.0;

    const int path_written = snprintf(
        state->current_file_path,
        sizeof(state->current_file_path),
        "%s",
        file_path
    );
    state->has_file_path = (path_written > 0 && (size_t)path_written < sizeof(state->current_file_path));

    state->hoop_rotated = false;
    state->hoop_preset_index = 5U;
    for (size_t preset_index = 1U; preset_index < HOOP_PRESET_COUNT; preset_index += 1U) {
        const int32_t half_width = janome_550e_hoops[preset_index].width_units / 2;
        const int32_t half_height = janome_550e_hoops[preset_index].height_units / 2;
        if (state->design.minimum_x >= -half_width && state->design.maximum_x <= half_width &&
            state->design.minimum_y >= -half_height && state->design.maximum_y <= half_height) {
            state->hoop_preset_index = preset_index;
            break;
        }
    }

    fit_camera_to_design(state, window_width, window_height);

    char message[256] = {};
    const int message_written = snprintf(
        message,
        sizeof(message),
        "Loaded %.80s | Grid %.1fmm %dx",
        basename_of_path(file_path),
        (double)state->grid.cross_pitch_units * 0.1,
        state->grid.cross_passes
    );
    if (message_written > 0 && (size_t)message_written < sizeof(message)) {
        set_status_message(state, message);
    }
    return true;
}

static bool load_dropped_file_as_merge_fragment(
    EditorState *state,
    const char *file_path,
    int32_t window_width,
    int32_t window_height
) {
    StitchDesign fragment = {};
    if (!load_embroidery_file(file_path, &fragment)) {
        set_status_message(state, "Failed to load file for merge");
        return false;
    }

    clear_floating_fragment(state);
    clear_segment_selection(state);
    state->floating_fragment = fragment;
    state->has_floating_fragment = true;
    state->floating_is_grid_origin_aligned = true;

    const CrossGridInfo fragment_grid = detect_cross_stitch_grid(&state->floating_fragment);
    state->floating_base_origin_x = fragment_grid.grid_origin_x;
    state->floating_base_origin_y = fragment_grid.grid_origin_y;
    state->floating_anchor_x = 0;
    state->floating_anchor_y = 0;

    const int32_t cursor_world_x =
        editor_screen_to_world_x(state, window_width, window_height, state->mouse_screen_x);
    const int32_t cursor_world_y =
        editor_screen_to_world_y(state, window_width, window_height, state->mouse_screen_y);
    const int32_t pitch = state->grid.cross_pitch_units;
    const int32_t phase_shift_x = state->grid.grid_origin_x - state->floating_base_origin_x;
    const int32_t phase_shift_y = state->grid.grid_origin_y - state->floating_base_origin_y;
    state->floating_offset_x = snap_coordinate_to_grid(cursor_world_x, phase_shift_x, pitch);
    state->floating_offset_y = snap_coordinate_to_grid(cursor_world_y, phase_shift_y, pitch);

    char message[256] = {};
    const int written = snprintf(
        message,
        sizeof(message),
        "Merge %.70s: move/arrows, Click/Enter place, Esc cancel",
        basename_of_path(file_path)
    );
    if (written > 0 && (size_t)written < sizeof(message)) {
        set_status_message(state, message);
    }
    return true;
}

static void save_current_design(EditorState *state) {
    const char *base_path = "untitled.jef";
    if (state->has_file_path) {
        base_path = state->current_file_path;
    }

    if (state->has_box_selection &&
        (state->active_tool == TOOL_SPLIT_VERTICAL || state->active_tool == TOOL_SPLIT_HORIZONTAL)) {
        const CutRectangle empty_box = {};
        const EditOperationKind operation = tool_to_edit_operation(state->active_tool);
        StitchDesign part_one = {};
        StitchDesign part_two = {};
        const bool first_built = apply_edit_operation(&state->design, state->grid, operation, empty_box, state->split_coordinate, false, &part_one);
        const bool second_built = apply_edit_operation(&state->design, state->grid, operation, empty_box, state->split_coordinate, true, &part_two);
        if (!first_built || !second_built || part_one.chain_count == 0U || part_two.chain_count == 0U) {
            free_stitch_design(&part_one);
            free_stitch_design(&part_two);
            set_status_message(state, "Split produces an empty half; adjust line");
            return;
        }
        char part_one_path[MAX_PATH_BUFFER] = {};
        char part_two_path[MAX_PATH_BUFFER] = {};
        if (!build_unique_suffixed_jef_path(base_path, "_part1", part_one_path, sizeof(part_one_path)) ||
            !build_unique_suffixed_jef_path(base_path, "_part2", part_two_path, sizeof(part_two_path))) {
            free_stitch_design(&part_one);
            free_stitch_design(&part_two);
            set_status_message(state, "Output path error");
            return;
        }
        const bool first_saved = write_jef_file(&part_one, part_one_path);
        const bool second_saved = write_jef_file(&part_two, part_two_path);
        free_stitch_design(&part_one);
        free_stitch_design(&part_two);
        if (first_saved && second_saved) {
            char message[256] = {};
            (void)snprintf(message, sizeof(message), "Saved %.60s & %.60s", basename_of_path(part_one_path), basename_of_path(part_two_path));
            set_status_message(state, message);
        } else {
            set_status_message(state, "Failed to export split JEF parts");
        }
        return;
    }

    char output_path[MAX_PATH_BUFFER] = {};
    if (!build_unique_suffixed_jef_path(base_path, "_edited", output_path, sizeof(output_path))) {
        set_status_message(state, "Output path error");
        return;
    }
    if (!write_jef_file(&state->design, output_path)) {
        set_status_message(state, "Failed to save JEF (design may be empty)");
        return;
    }
    char message[256] = {};
    (void)snprintf(message, sizeof(message), "Saved %.120s", basename_of_path(output_path));
    set_status_message(state, message);
}

static void delete_active_selection(EditorState *state) {
    if (state->selection_mask == nullptr || state->selected_segment_count == 0U) {
        set_status_message(state, "Nothing selected to delete");
        return;
    }
    StitchDesign result = {};
    if (!delete_masked_segments(
            &state->design,
            state->grid.cross_pitch_units,
            state->selection_mask,
            state->selection_mask_length,
            &result
        )) {
        set_status_message(state, "Failed to delete selection");
        return;
    }
    free_stitch_design(&state->design);
    state->design = result;
    (void)history_push(&state->history, &state->design);
    if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
        state->active_color_block_index = state->design.block_count - 1U;
    }
    clear_segment_selection(state);
    clear_hover_preview(state);
    set_status_message(state, "Deleted selection with tie-off locks");
}

static void commit_floating_fragment(EditorState *state, bool keep_stamping) {
    if (!state->has_floating_fragment) {
        return;
    }
    StitchDesign merged = {};
    if (!insert_design_fragment(
            &state->design,
            &state->floating_fragment,
            state->floating_offset_x,
            state->floating_offset_y,
            &merged
        )) {
        set_status_message(state, "Failed to insert fragment");
        return;
    }
    free_stitch_design(&state->design);
    state->design = merged;
    (void)history_push(&state->history, &state->design);
    clear_hover_preview(state);
    if (!keep_stamping) {
        clear_floating_fragment(state);
    }
    set_status_message(state, "Inserted fragment into design");
}

static void open_strip_modal(EditorState *state) {
    state->strip_pattern.pitch_units = state->grid.cross_pitch_units;
    state->strip_pattern.passes = state->grid.cross_passes;
    if (state->active_color_block_index < state->design.block_count) {
        const uint8_t block_janome =
            state->design.blocks[state->active_color_block_index].janome_color_index;
        if (block_janome > 0U && block_janome < JANOME_PALETTE_COUNT) {
            state->strip_active_color = block_janome;
        }
    }
    state->strip_modal_open = true;
    state->needs_redraw = true;
}

static void execute_toolbar_command(
    EditorState *state,
    uint8_t command_id,
    int32_t window_width,
    int32_t window_height
) {
    switch (command_id) {
        case COMMAND_TOOL_SELECT_ELEMENT:
            state->active_tool = TOOL_SELECT_ELEMENT;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_hover_preview(state);
            set_status_message(state, "Sel Elem: Click select, Shift+Click add, RMB/Del delete, Arrows move");
            break;
        case COMMAND_TOOL_SELECT_CROSS:
            state->active_tool = TOOL_SELECT_CROSS;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_hover_preview(state);
            set_status_message(state, "Sel X: Click/drag cross, Shift+Click add, RMB/Del delete, Arrows move");
            break;
        case COMMAND_TOOL_DRAW_CROSS:
            state->active_tool = TOOL_DRAW_CROSS;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_hover_preview(state);
            set_status_message(state, "Draw X: LMB draw cross on grid, RMB erase cross");
            break;
        case COMMAND_OPEN_STRIP_MODAL:
            open_strip_modal(state);
            break;
        case COMMAND_TOOL_CROP:
            state->active_tool = TOOL_CROP_KEEP;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_segment_selection(state);
            set_status_message(state, "Crop: drag rectangle (Shift=grid snap), Enter to apply");
            break;
        case COMMAND_TOOL_CUT:
            state->active_tool = TOOL_CUT_OUT;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_segment_selection(state);
            set_status_message(state, "Cut: drag box (Shift=grid snap), dimmed crosses delete on Enter");
            break;
        case COMMAND_TOOL_SPLIT_V:
            state->active_tool = TOOL_SPLIT_VERTICAL;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_segment_selection(state);
            set_status_message(state, "Split V: click/drag vertical line, Enter or Save exports 2 parts");
            break;
        case COMMAND_TOOL_SPLIT_H:
            state->active_tool = TOOL_SPLIT_HORIZONTAL;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_segment_selection(state);
            set_status_message(state, "Split H: click/drag horizontal line, Enter or Save exports 2 parts");
            break;
        case COMMAND_TOOL_CLONE:
            state->active_tool = TOOL_CLONE;
            state->has_box_selection = false;
            clear_live_preview(state);
            clear_segment_selection(state);
            set_status_message(state, "Clone: drag area to copy, move snapped to grid, Click/Enter stamp");
            break;
        case COMMAND_TOOL_PICK:
            state->active_tool = TOOL_PICK_COLOR;
            set_status_message(state, "Pick: click any stitch to select its color block");
            break;
        case COMMAND_OPEN_FILE_DIALOG:
            open_file_dialog_state(state, FILE_DIALOG_OPEN);
            break;
        case COMMAND_MERGE_FILE_DIALOG:
            open_file_dialog_state(state, FILE_DIALOG_MERGE);
            break;
        case COMMAND_SAVE_AS_DIALOG:
            if (state->design.chain_count == 0U) {
                set_status_message(state, "Nothing to save");
            } else {
                open_file_dialog_state(state, FILE_DIALOG_SAVE_AS);
            }
            break;
        case COMMAND_ROTATE_90:
            if (state->design.chain_count > 0U) {
                rotate_design_90_clockwise(&state->design);
                state->grid = detect_cross_stitch_grid(&state->design);
                (void)history_push(&state->history, &state->design);
                clear_segment_selection(state);
                clear_hover_preview(state);
                set_status_message(state, "Rotated 90 degrees clockwise");
            }
            break;
        case COMMAND_FLIP_X:
            if (state->design.chain_count > 0U) {
                mirror_design_horizontal(&state->design);
                state->grid = detect_cross_stitch_grid(&state->design);
                (void)history_push(&state->history, &state->design);
                clear_segment_selection(state);
                clear_hover_preview(state);
                set_status_message(state, "Flipped horizontally");
            }
            break;
        case COMMAND_FLIP_Y:
            if (state->design.chain_count > 0U) {
                mirror_design_vertical(&state->design);
                state->grid = detect_cross_stitch_grid(&state->design);
                (void)history_push(&state->history, &state->design);
                clear_segment_selection(state);
                clear_hover_preview(state);
                set_status_message(state, "Flipped vertically");
            }
            break;
        case COMMAND_CENTER:
            if (state->design.chain_count > 0U) {
                center_stitch_design(&state->design);
                state->grid = detect_cross_stitch_grid(&state->design);
                (void)history_push(&state->history, &state->design);
                fit_camera_to_design(state, window_width, window_height);
                set_status_message(state, "Centered design at (0, 0)");
            }
            break;
        case COMMAND_MERGE_COLORS:
            if (state->design.block_count > 1U) {
                const size_t before_blocks = state->design.block_count;
                if (merge_duplicate_color_blocks(&state->design)) {
                    (void)history_push(&state->history, &state->design);
                    if (state->active_color_block_index >= state->design.block_count &&
                        state->design.block_count > 0U) {
                        state->active_color_block_index = state->design.block_count - 1U;
                    }
                    clear_segment_selection(state);
                    clear_hover_preview(state);
                    char message[128] = {};
                    (void)snprintf(message, sizeof(message), "Merged duplicate colors: %zu -> %zu blocks", before_blocks, state->design.block_count);
                    set_status_message(state, message);
                }
            }
            break;
        case COMMAND_RECOLOR_SELECTION:
            if (state->selected_segment_count > 0U) {
                state->palette_target = PALETTE_TARGET_RECOLOR_SELECTION;
                state->needs_redraw = true;
            } else {
                set_status_message(state, "Select crosses or element first to recolor");
            }
            break;
        case COMMAND_DELETE_SELECTION:
            delete_active_selection(state);
            break;
        case COMMAND_UNDO:
            if (history_undo(&state->history, &state->design)) {
                clear_segment_selection(state);
                clear_hover_preview(state);
                clear_live_preview(state);
                state->has_box_selection = false;
                if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
                    state->active_color_block_index = state->design.block_count - 1U;
                }
                set_status_message(state, "Undo");
            } else if (history_can_undo(&state->history)) {
                set_status_message(state, "Undo failed: out of memory");
            }
            break;
        case COMMAND_REDO:
            if (history_redo(&state->history, &state->design)) {
                clear_segment_selection(state);
                clear_hover_preview(state);
                clear_live_preview(state);
                state->has_box_selection = false;
                if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
                    state->active_color_block_index = state->design.block_count - 1U;
                }
                set_status_message(state, "Redo");
            } else if (history_can_redo(&state->history)) {
                set_status_message(state, "Redo failed: out of memory");
            }
            break;
        case COMMAND_SAVE:
            save_current_design(state);
            break;
        case COMMAND_TOGGLE_DROP_MODE:
            state->drop_merge_mode = !state->drop_merge_mode;
            if (state->drop_merge_mode) {
                set_status_message(state, "Drop mode: Merge (dropped files attach for docking)");
            } else {
                set_status_message(state, "Drop mode: Open (dropped files replace current design)");
            }
            break;
        case COMMAND_CYCLE_HOOP:
            state->hoop_preset_index = (state->hoop_preset_index + 1U) % HOOP_PRESET_COUNT;
            state->needs_redraw = true;
            break;
        case COMMAND_ROTATE_HOOP:
            state->hoop_rotated = !state->hoop_rotated;
            state->needs_redraw = true;
            break;
        case COMMAND_TOGGLE_GRID:
            state->show_grid = !state->show_grid;
            state->needs_redraw = true;
            break;
        case COMMAND_TOGGLE_JUMPS:
            state->show_jumps = !state->show_jumps;
            state->needs_redraw = true;
            break;
        case COMMAND_TOGGLE_LOCKS:
            state->show_locks = !state->show_locks;
            state->needs_redraw = true;
            break;
        case COMMAND_PITCH_DECREASE:
            if (state->grid.cross_pitch_units > MIN_CROSS_PITCH_UNITS) {
                state->grid.cross_pitch_units -= 1;
                state->preview_dirty = true;
                state->needs_redraw = true;
            }
            break;
        case COMMAND_PITCH_INCREASE:
            if (state->grid.cross_pitch_units < MAX_CROSS_PITCH_UNITS) {
                state->grid.cross_pitch_units += 1;
                state->preview_dirty = true;
                state->needs_redraw = true;
            }
            break;
        case COMMAND_CYCLE_PASSES:
            state->grid.cross_passes += 1;
            if (state->grid.cross_passes > MAX_CROSS_PASSES) {
                state->grid.cross_passes = MIN_CROSS_PASSES;
            }
            state->needs_redraw = true;
            break;
        case COMMAND_CYCLE_VIEW_MODE:
            if (state->view_mode == VIEW_MODE_THREAD) {
                state->view_mode = VIEW_MODE_BLOCKS;
                set_status_message(state, "View mode: Blocks (cross-stitch chart squares)");
            } else if (state->view_mode == VIEW_MODE_BLOCKS) {
                state->view_mode = VIEW_MODE_LINE;
                set_status_message(state, "View mode: Line (1px technical stitch lines)");
            } else {
                state->view_mode = VIEW_MODE_THREAD;
                set_status_message(state, "View mode: Thread (thick realistic cross stitches)");
            }
            break;
        case COMMAND_CYCLE_THREAD_THICKNESS:
            state->thread_thickness_index =
                (state->thread_thickness_index + 1U) % THREAD_THICKNESS_PRESET_COUNT;
            {
                char message[96] = {};
                (void)snprintf(
                    message,
                    sizeof(message),
                    "Thread thickness: %.2f mm",
                    thread_thickness_units[state->thread_thickness_index] * 0.1
                );
                set_status_message(state, message);
            }
            break;
        case COMMAND_FIT_VIEW:
            fit_camera_to_design(state, window_width, window_height);
            break;
        default:
            break;
    }
}

static void execute_file_dialog_confirm(
    EditorState *state,
    int32_t window_width,
    int32_t window_height
) {
    FileDialogState *dialog = &state->file_dialog;
    const bool is_save_mode =
        dialog->mode == FILE_DIALOG_SAVE_AS ||
        dialog->mode == FILE_DIALOG_STRIP_SAVE_AS;

    if (!is_save_mode) {
        if (dialog->selected_index >= dialog->entry_count) {
            (void)snprintf(dialog->error_text, sizeof(dialog->error_text), "Select a file first");
            state->needs_redraw = true;
            return;
        }
        const FileBrowserEntry *entry = &dialog->entries[dialog->selected_index];
        char full_path[MAX_PATH_BUFFER] = {};
        int joined = 0;
        if (strcmp(dialog->current_directory, "/") == 0) {
            joined = snprintf(full_path, sizeof(full_path), "/%s", entry->name);
        } else {
            joined = snprintf(full_path, sizeof(full_path), "%s/%s", dialog->current_directory, entry->name);
        }
        if (joined <= 0 || (size_t)joined >= sizeof(full_path)) {
            return;
        }
        if (entry->is_directory) {
            (void)load_file_browser_directory(state, full_path);
            return;
        }
        const FileDialogMode active_mode = dialog->mode;
        close_file_dialog_state(state);
        if (active_mode == FILE_DIALOG_OPEN) {
            (void)load_design_into_editor(state, full_path, window_width, window_height);
        } else if (active_mode == FILE_DIALOG_MERGE) {
            (void)load_dropped_file_as_merge_fragment(state, full_path, window_width, window_height);
        }
        return;
    }

    if (dialog->filename_input[0] == '\0') {
        (void)snprintf(dialog->error_text, sizeof(dialog->error_text), "Enter a filename");
        state->needs_redraw = true;
        return;
    }

    /* Auto-append .jef extension if missing. */
    char clean_name[300] = {};
    const size_t name_len = strlen(dialog->filename_input);
    bool has_jef_suffix = false;
    if (name_len >= 4U && strings_equal_ignore_case(dialog->filename_input + name_len - 4U, ".jef")) {
        has_jef_suffix = true;
    }
    if (has_jef_suffix) {
        (void)snprintf(clean_name, sizeof(clean_name), "%s", dialog->filename_input);
    } else {
        (void)snprintf(clean_name, sizeof(clean_name), "%s.jef", dialog->filename_input);
    }

    char target_path[MAX_PATH_BUFFER] = {};
    int joined = 0;
    if (strcmp(dialog->current_directory, "/") == 0) {
        joined = snprintf(target_path, sizeof(target_path), "/%s", clean_name);
    } else {
        joined = snprintf(target_path, sizeof(target_path), "%s/%s", dialog->current_directory, clean_name);
    }
    if (joined <= 0 || (size_t)joined >= sizeof(target_path)) {
        (void)snprintf(dialog->error_text, sizeof(dialog->error_text), "Path too long");
        state->needs_redraw = true;
        return;
    }

    if (file_already_exists(target_path)) {
        if (!dialog->confirm_overwrite || strcmp(dialog->pending_overwrite_path, target_path) != 0) {
            dialog->confirm_overwrite = true;
            (void)snprintf(dialog->pending_overwrite_path, sizeof(dialog->pending_overwrite_path), "%s", target_path);
            state->needs_redraw = true;
            return;
        }
    }

    bool saved = false;
    if (dialog->mode == FILE_DIALOG_STRIP_SAVE_AS) {
        StitchDesign built_strip = {};
        if (!build_strip_design(&state->strip_pattern, &built_strip)) {
            (void)snprintf(dialog->error_text, sizeof(dialog->error_text), "Strip is empty");
            state->needs_redraw = true;
            return;
        }
        saved = write_jef_file(&built_strip, target_path);
        free_stitch_design(&built_strip);
    } else {
        saved = write_jef_file(&state->design, target_path);
    }

    if (!saved) {
        (void)snprintf(dialog->error_text, sizeof(dialog->error_text), "Failed to write JEF file");
        state->needs_redraw = true;
        return;
    }

    char message[256] = {};
    (void)snprintf(message, sizeof(message), "Saved %.120s", basename_of_path(target_path));
    close_file_dialog_state(state);
    set_status_message(state, message);
}

static void handle_file_dialog_command(
    EditorState *state,
    uint8_t command_id,
    int32_t window_width,
    int32_t window_height
) {
    if (command_id == DIALOG_CMD_CANCEL) {
        close_file_dialog_state(state);
        return;
    }
    if (command_id == DIALOG_CMD_CONFIRM) {
        execute_file_dialog_confirm(state, window_width, window_height);
        return;
    }
    if (command_id == DIALOG_CMD_PARENT) {
        file_dialog_navigate_parent(state);
        return;
    }
    if (command_id == DIALOG_CMD_HOME) {
        file_dialog_navigate_home(state);
        return;
    }
    if (command_id == DIALOG_CMD_DOWNLOADS) {
        file_dialog_navigate_downloads(state);
    }
}

static void handle_file_dialog_click(
    EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_width,
    int32_t window_height,
    uint32_t now_ticks
) {
    const FileDialogLayout layout = compute_file_dialog_layout(state, window_width, window_height);
    const uint8_t button_command = hit_test_file_dialog_button(&layout, mouse_x, mouse_y);
    if (button_command != DIALOG_CMD_NONE) {
        handle_file_dialog_command(state, button_command, window_width, window_height);
        return;
    }

    size_t clicked_index = SIZE_MAX;
    if (!hit_test_file_dialog_entry(state, &layout, mouse_x, mouse_y, &clicked_index)) {
        return;
    }

    FileDialogState *dialog = &state->file_dialog;
    const bool is_double_click =
        clicked_index == dialog->last_click_index &&
        (now_ticks - dialog->last_click_ticks) <= DOUBLE_CLICK_MS;

    dialog->selected_index = clicked_index;
    dialog->last_click_index = clicked_index;
    dialog->last_click_ticks = now_ticks;
    dialog->confirm_overwrite = false;
    dialog->error_text[0] = '\0';

    const FileBrowserEntry *entry = &dialog->entries[clicked_index];
    const bool is_save_mode =
        dialog->mode == FILE_DIALOG_SAVE_AS ||
        dialog->mode == FILE_DIALOG_STRIP_SAVE_AS;

    if (!entry->is_directory && is_save_mode) {
        (void)snprintf(dialog->filename_input, sizeof(dialog->filename_input), "%.255s", entry->name);
    }
    state->needs_redraw = true;

    if (is_double_click) {
        if (entry->is_directory) {
            char next_dir[MAX_PATH_BUFFER] = {};
            int joined = 0;
            if (strcmp(dialog->current_directory, "/") == 0) {
                joined = snprintf(next_dir, sizeof(next_dir), "/%s", entry->name);
            } else {
                joined = snprintf(next_dir, sizeof(next_dir), "%s/%s", dialog->current_directory, entry->name);
            }
            if (joined > 0 && (size_t)joined < sizeof(next_dir)) {
                (void)load_file_browser_directory(state, next_dir);
            }
        } else {
            execute_file_dialog_confirm(state, window_width, window_height);
        }
    }
}

static void apply_palette_selection(EditorState *state, uint8_t chosen_janome_index) {
    const PaletteModalTarget target = state->palette_target;
    state->palette_target = PALETTE_TARGET_NONE;
    if (chosen_janome_index >= JANOME_PALETTE_COUNT) {
        return;
    }

    uint8_t valid_janome = chosen_janome_index;
    if (valid_janome == 0U) {
        valid_janome = 1U;
    }

    if (target == PALETTE_TARGET_STRIP_COLOR) {
        state->strip_active_color = valid_janome;
        state->needs_redraw = true;
        return;
    }

    if (target == PALETTE_TARGET_ADD_BLOCK) {
        size_t new_block_index = 0U;
        if (design_append_block(&state->design, janome_palette[valid_janome], valid_janome, true, &new_block_index)) {
            state->active_color_block_index = new_block_index;
            (void)history_push(&state->history, &state->design);
            char message[128] = {};
            (void)snprintf(message, sizeof(message), "Added Color Block #%zu (Janome #%02u)", new_block_index + 1U, (unsigned int)valid_janome);
            set_status_message(state, message);
        }
        return;
    }

    if (target == PALETTE_TARGET_RECOLOR_BLOCK &&
        state->palette_target_block_index < state->design.block_count) {
        state->design.blocks[state->palette_target_block_index].janome_color_index = valid_janome;
        state->design.blocks[state->palette_target_block_index].display_color = janome_palette[valid_janome];
        (void)history_push(&state->history, &state->design);
        char message[128] = {};
        (void)snprintf(message, sizeof(message), "Changed Block #%zu to Janome #%02u", state->palette_target_block_index + 1U, (unsigned int)valid_janome);
        set_status_message(state, message);
        return;
    }

    if (target == PALETTE_TARGET_RECOLOR_SELECTION &&
        state->selection_mask != nullptr &&
        state->selected_segment_count > 0U) {
        StitchDesign recolored = {};
        if (recolor_masked_segments(&state->design, state->selection_mask, state->selection_mask_length, valid_janome, &recolored)) {
            free_stitch_design(&state->design);
            state->design = recolored;
            (void)history_push(&state->history, &state->design);
            clear_segment_selection(state);
            clear_hover_preview(state);
            char message[128] = {};
            (void)snprintf(message, sizeof(message), "Recolored selection to Janome #%02u", (unsigned int)valid_janome);
            set_status_message(state, message);
        }
    }
}

static void handle_palette_modal_click(
    EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_width,
    int32_t window_height
) {
    int32_t chosen_index = -1;
    if (!hit_test_palette_modal(mouse_x, mouse_y, window_width, window_height, &chosen_index)) {
        state->palette_target = PALETTE_TARGET_NONE;
        state->needs_redraw = true;
        return;
    }
    if (chosen_index >= 0 && chosen_index < (int32_t)JANOME_PALETTE_COUNT) {
        apply_palette_selection(state, (uint8_t)chosen_index);
    }
}

static void paint_strip_cell_at_mouse(
    EditorState *state,
    const StripModalLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y,
    bool erase_mode
) {
    int32_t column = 0;
    int32_t row = 0;
    if (!hit_test_strip_editor_cell(&state->strip_pattern, layout, mouse_x, mouse_y, &column, &row)) {
        return;
    }

    uint8_t target_color = state->strip_active_color;
    if (target_color == 0U || target_color >= JANOME_PALETTE_COUNT) {
        target_color = 10U;
    }
    if (erase_mode) {
        target_color = 0U;
    }

    uint8_t *cell = &state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column];
    if (*cell == target_color) {
        return;
    }
    if (!state->strip_stroke_pushed_undo) {
        strip_history_push(state);
        state->strip_stroke_pushed_undo = true;
    }
    *cell = target_color;
    state->needs_redraw = true;
}

static void handle_strip_command(
    EditorState *state,
    uint8_t command_id,
    int32_t window_width,
    int32_t window_height
) {
    switch (command_id) {
        case STRIP_CMD_WIDTH_DEC:
            if (state->strip_pattern.width > 1) {
                strip_history_push(state);
                state->strip_pattern.width -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_WIDTH_INC:
            if (state->strip_pattern.width < STRIP_MAX_GRID_DIM) {
                strip_history_push(state);
                state->strip_pattern.width += 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_HEIGHT_DEC:
            if (state->strip_pattern.height > 1) {
                strip_history_push(state);
                state->strip_pattern.height -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_HEIGHT_INC:
            if (state->strip_pattern.height < STRIP_MAX_GRID_DIM) {
                strip_history_push(state);
                state->strip_pattern.height += 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_REPEAT_X_DEC:
            if (state->strip_pattern.repeat_x > 1) {
                strip_history_push(state);
                state->strip_pattern.repeat_x -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_REPEAT_X_INC:
            if (state->strip_pattern.repeat_x < STRIP_MAX_REPEAT) {
                strip_history_push(state);
                state->strip_pattern.repeat_x += 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_REPEAT_Y_DEC:
            if (state->strip_pattern.repeat_y > 1) {
                strip_history_push(state);
                state->strip_pattern.repeat_y -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_REPEAT_Y_INC:
            if (state->strip_pattern.repeat_y < STRIP_MAX_REPEAT) {
                strip_history_push(state);
                state->strip_pattern.repeat_y += 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_PITCH_DEC:
            if (state->strip_pattern.pitch_units > MIN_CROSS_PITCH_UNITS) {
                state->strip_pattern.pitch_units -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_PITCH_INC:
            if (state->strip_pattern.pitch_units < MAX_CROSS_PITCH_UNITS) {
                state->strip_pattern.pitch_units += 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_CYCLE_PASSES:
            state->strip_pattern.passes += 1;
            if (state->strip_pattern.passes > MAX_CROSS_PASSES) {
                state->strip_pattern.passes = MIN_CROSS_PASSES;
            }
            state->needs_redraw = true;
            break;
        case STRIP_CMD_PICK_COLOR:
            state->palette_target = PALETTE_TARGET_STRIP_COLOR;
            state->needs_redraw = true;
            break;
        case STRIP_CMD_CLEAR:
            strip_history_push(state);
            memset(state->strip_pattern.cells, 0, sizeof(state->strip_pattern.cells));
            state->needs_redraw = true;
            break;
        case STRIP_CMD_MIRROR_X:
            strip_history_push(state);
            for (int32_t row = 0; row < state->strip_pattern.height; row += 1) {
                for (int32_t column = 0; column < state->strip_pattern.width / 2; column += 1) {
                    const int32_t mirror_column = state->strip_pattern.width - 1 - column;
                    const uint8_t saved = state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column];
                    state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column] =
                        state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + mirror_column];
                    state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + mirror_column] = saved;
                }
            }
            state->needs_redraw = true;
            break;
        case STRIP_CMD_MIRROR_Y:
            strip_history_push(state);
            for (int32_t row = 0; row < state->strip_pattern.height / 2; row += 1) {
                const int32_t mirror_row = state->strip_pattern.height - 1 - row;
                for (int32_t column = 0; column < state->strip_pattern.width; column += 1) {
                    const uint8_t saved = state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column];
                    state->strip_pattern.cells[row * STRIP_MAX_GRID_DIM + column] =
                        state->strip_pattern.cells[mirror_row * STRIP_MAX_GRID_DIM + column];
                    state->strip_pattern.cells[mirror_row * STRIP_MAX_GRID_DIM + column] = saved;
                }
            }
            state->needs_redraw = true;
            break;
        case STRIP_CMD_UNDO:
            if (state->strip_history.state_count > 0U) {
                state->strip_history.state_count -= 1U;
                state->strip_pattern = state->strip_history.states[state->strip_history.state_count];
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_INSERT: {
            StitchDesign built_strip = {};
            if (!build_strip_design(&state->strip_pattern, &built_strip)) {
                set_status_message(state, "Strip is empty: paint crosses first");
                return;
            }
            clear_floating_fragment(state);
            clear_segment_selection(state);
            state->floating_fragment = built_strip;
            state->has_floating_fragment = true;
            state->floating_is_grid_origin_aligned = true;
            state->floating_base_origin_x = 0;
            state->floating_base_origin_y = 0;
            state->floating_anchor_x = 0;
            state->floating_anchor_y = 0;
            state->strip_modal_open = false;

            const int32_t cursor_world_x = editor_screen_to_world_x(state, window_width, window_height, state->mouse_screen_x);
            const int32_t cursor_world_y = editor_screen_to_world_y(state, window_width, window_height, state->mouse_screen_y);
            state->floating_offset_x = snap_coordinate_to_grid(cursor_world_x, state->grid.grid_origin_x, state->grid.cross_pitch_units);
            state->floating_offset_y = snap_coordinate_to_grid(cursor_world_y, state->grid.grid_origin_y, state->grid.cross_pitch_units);
            set_status_message(state, "Place Strip: move/arrows, Click/Enter insert, Esc cancel");
            break;
        }
        case STRIP_CMD_SAVE_AS:
            open_file_dialog_state(state, FILE_DIALOG_STRIP_SAVE_AS);
            break;
        case STRIP_CMD_SAVE_JEF: {
            StitchDesign built_strip = {};
            if (!build_strip_design(&state->strip_pattern, &built_strip)) {
                set_status_message(state, "Strip is empty: paint crosses first");
                return;
            }
            const char *base_path = "strip_design.jef";
            const char *suffix = "_strip";
            if (state->has_file_path) {
                base_path = state->current_file_path;
            } else {
                suffix = "";
            }
            char output_path[MAX_PATH_BUFFER] = {};
            if (build_unique_suffixed_jef_path(base_path, suffix, output_path, sizeof(output_path)) &&
                write_jef_file(&built_strip, output_path)) {
                char message[256] = {};
                (void)snprintf(message, sizeof(message), "Exported strip to %.110s", basename_of_path(output_path));
                set_status_message(state, message);
            } else {
                set_status_message(state, "Failed to export strip JEF");
            }
            free_stitch_design(&built_strip);
            break;
        }
        case STRIP_CMD_CLOSE:
            state->strip_modal_open = false;
            state->needs_redraw = true;
            break;
        default:
            break;
    }
}

static bool handle_left_panel_click(
    EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_height
) {
    size_t block_index = SIZE_MAX;
    const LeftPanelHitKind hit =
        hit_test_left_panel(state, mouse_x, mouse_y, window_height, &block_index);
    if (hit == LEFT_PANEL_HIT_NONE) {
        return false;
    }
    if (hit == LEFT_PANEL_HIT_PANEL_EMPTY) {
        return true;
    }
    if (hit == LEFT_PANEL_HIT_ADD_COLOR) {
        state->palette_target = PALETTE_TARGET_ADD_BLOCK;
        state->needs_redraw = true;
        return true;
    }
    if (block_index >= state->design.block_count) {
        return true;
    }

    if (hit == LEFT_PANEL_HIT_TOGGLE_VISIBILITY) {
        state->design.blocks[block_index].enabled = !state->design.blocks[block_index].enabled;
        recompute_design_bounds(&state->design);
        (void)history_push(&state->history, &state->design);
        state->preview_dirty = true;
        clear_hover_preview(state);
        state->needs_redraw = true;
        return true;
    }
    if (hit == LEFT_PANEL_HIT_MOVE_UP) {
        if (move_color_block_order(&state->design, block_index, -1)) {
            state->active_color_block_index = block_index - 1U;
            (void)history_push(&state->history, &state->design);
            clear_segment_selection(state);
            clear_hover_preview(state);
            set_status_message(state, "Moved color block earlier in stitch order");
        }
        return true;
    }
    if (hit == LEFT_PANEL_HIT_MOVE_DOWN) {
        if (move_color_block_order(&state->design, block_index, 1)) {
            state->active_color_block_index = block_index + 1U;
            (void)history_push(&state->history, &state->design);
            clear_segment_selection(state);
            clear_hover_preview(state);
            set_status_message(state, "Moved color block later in stitch order");
        }
        return true;
    }
    if (hit == LEFT_PANEL_HIT_RECOLOR_BLOCK) {
        state->palette_target = PALETTE_TARGET_RECOLOR_BLOCK;
        state->palette_target_block_index = block_index;
        state->needs_redraw = true;
        return true;
    }
    if (hit == LEFT_PANEL_HIT_DELETE_BLOCK) {
        if (remove_color_block_at(&state->design, block_index)) {
            if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
                state->active_color_block_index = state->design.block_count - 1U;
            }
            (void)history_push(&state->history, &state->design);
            clear_segment_selection(state);
            clear_hover_preview(state);
            state->preview_dirty = true;
            set_status_message(state, "Deleted color block");
        }
        return true;
    }

    state->active_color_block_index = block_index;
    const ColorBlock block = state->design.blocks[block_index];
    char message[128] = {};
    (void)snprintf(
        message,
        sizeof(message),
        "Active Color: Block #%zu (Janome #%02u #%02X%02X%02X)",
        block_index + 1U,
        (unsigned int)block.janome_color_index,
        (unsigned int)block.display_color.red,
        (unsigned int)block.display_color.green,
        (unsigned int)block.display_color.blue
    );
    set_status_message(state, message);
    return true;
}

static void apply_draw_x_stroke_at(
    EditorState *state,
    int32_t world_x,
    int32_t world_y,
    bool erase_mode
) {
    int32_t cell_column = 0;
    int32_t cell_row = 0;
    world_to_grid_cell(world_x, world_y, state->grid, &cell_column, &cell_row);
    if (state->draw_stroke_pushed_undo &&
        cell_column == state->last_drawn_column &&
        cell_row == state->last_drawn_row) {
        return;
    }

    if (erase_mode) {
        const size_t total_segments = count_design_segments(&state->design);
        if (total_segments == 0U) {
            return;
        }
        uint8_t *erase_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
        if (erase_mask == nullptr) {
            return;
        }
        const size_t matched = select_cross_at_point_in_mask(
            &state->design,
            state->grid,
            world_x,
            world_y,
            erase_mask,
            total_segments,
            false,
            nullptr
        );
        if (matched > 0U) {
            StitchDesign trimmed = {};
            if (delete_masked_segments(
                    &state->design,
                    state->grid.cross_pitch_units,
                    erase_mask,
                    total_segments,
                    &trimmed
                )) {
                free_stitch_design(&state->design);
                state->design = trimmed;
                state->draw_stroke_pushed_undo = true;
                state->last_drawn_column = cell_column;
                state->last_drawn_row = cell_row;
                clear_segment_selection(state);
                clear_hover_preview(state);
                state->needs_redraw = true;
            }
        }
        free(erase_mask);
        return;
    }

    if (state->design.block_count == 0U) {
        size_t new_block = 0U;
        if (!design_append_block(&state->design, janome_palette[10], 10U, true, &new_block)) {
            return;
        }
        state->active_color_block_index = new_block;
    }
    if (state->active_color_block_index >= state->design.block_count) {
        state->active_color_block_index = state->design.block_count - 1U;
    }
    state->design.blocks[state->active_color_block_index].enabled = true;

    if (draw_single_cross_into_design(
            &state->design,
            state->grid,
            cell_column,
            cell_row,
            state->active_color_block_index
        )) {
        state->draw_stroke_pushed_undo = true;
        state->last_drawn_column = cell_column;
        state->last_drawn_row = cell_row;
        clear_segment_selection(state);
        clear_hover_preview(state);
        state->needs_redraw = true;
    }
}

static void update_canvas_hover(
    EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t window_width,
    int32_t window_height
) {
    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    if (!point_is_in_box(mouse_x, mouse_y, viewport.left, viewport.top, viewport.width, viewport.height) ||
        state->design.chain_count == 0U ||
        state->has_floating_fragment) {
        if (state->hover_seed_chain != SIZE_MAX || state->has_hover_cross_box) {
            clear_hover_preview(state);
            state->needs_redraw = true;
        }
        return;
    }

    const int32_t world_x = viewport_screen_to_world_x(viewport, state->camera, mouse_x);
    const int32_t world_y = viewport_screen_to_world_y(viewport, state->camera, mouse_y);

    if (state->active_tool == TOOL_SELECT_CROSS) {
        bool snapped = false;
        const CutRectangle box = resolve_cross_box_at_point(&state->design, state->grid, world_x, world_y, &snapped);
        if (!state->has_hover_cross_box ||
            box.minimum_x != state->hover_cross_box.minimum_x ||
            box.minimum_y != state->hover_cross_box.minimum_y) {
            state->hover_cross_box = box;
            state->has_hover_cross_box = true;
            state->needs_redraw = true;
        }
        return;
    }

    if (state->active_tool == TOOL_SELECT_ELEMENT) {
        int32_t search_radius = state->grid.cross_pitch_units;
        const int32_t pixel_radius = (int32_t)llround(12.0 / state->camera.pixels_per_unit);
        if (pixel_radius > search_radius) {
            search_radius = pixel_radius;
        }
        size_t hovered_chain = SIZE_MAX;
        if (!find_nearest_chain_at_point(&state->design, world_x, world_y, search_radius, &hovered_chain)) {
            if (state->hover_seed_chain != SIZE_MAX) {
                clear_hover_preview(state);
                state->needs_redraw = true;
            }
            return;
        }
        if (hovered_chain == state->hover_seed_chain) {
            return;
        }

        const size_t total_segments = count_design_segments(&state->design);
        if (state->hover_mask == nullptr || state->hover_mask_length != total_segments) {
            free(state->hover_mask);
            state->hover_mask = (uint8_t *)calloc(total_segments, sizeof(uint8_t));
            state->hover_mask_length = total_segments;
        } else {
            memset(state->hover_mask, 0, total_segments);
        }
        if (state->hover_mask != nullptr) {
            (void)select_connected_element_in_mask(
                &state->design,
                state->grid,
                hovered_chain,
                state->hover_mask,
                state->hover_mask_length,
                false
            );
            state->hover_seed_chain = hovered_chain;
            state->needs_redraw = true;
        }
    }
}

static void nudge_active_selection_or_fragment(
    EditorState *state,
    int32_t step_columns,
    int32_t step_rows,
    bool shift_held
) {
    int32_t pitch = state->grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }
    const int32_t delta_x = step_columns * pitch;
    const int32_t delta_y = step_rows * pitch;

    if (state->has_floating_fragment) {
        state->floating_offset_x += delta_x;
        state->floating_offset_y += delta_y;
        state->needs_redraw = true;
        return;
    }

    if (state->selection_mask != nullptr && state->selected_segment_count > 0U) {
        StitchDesign shifted = {};
        uint8_t *updated_mask = nullptr;
        size_t updated_mask_length = 0U;
        if (shift_masked_segments(
                &state->design,
                state->selection_mask,
                state->selection_mask_length,
                delta_x,
                delta_y,
                &shifted,
                &updated_mask,
                &updated_mask_length
            )) {
            free_stitch_design(&state->design);
            state->design = shifted;
            (void)history_push(&state->history, &state->design);
            free(state->selection_mask);
            state->selection_mask = updated_mask;
            state->selection_mask_length = updated_mask_length;
            recount_selected_segments(state);
            clear_hover_preview(state);
            set_status_message(state, "Shifted selection by 1 grid pitch");
        }
        return;
    }

    if (shift_held && state->design.chain_count > 0U) {
        shift_design_center(&state->design, delta_x, delta_y);
        (void)history_push(&state->history, &state->design);
        set_status_message(state, "Shifted design center");
    }
}

static void apply_box_edit_or_split(EditorState *state) {
    if (!state->has_box_selection) {
        return;
    }
    if (state->active_tool == TOOL_SPLIT_VERTICAL || state->active_tool == TOOL_SPLIT_HORIZONTAL) {
        save_current_design(state);
        return;
    }
    if (state->active_tool != TOOL_CROP_KEEP && state->active_tool != TOOL_CUT_OUT) {
        return;
    }

    const CutRectangle rectangle = {
        state->box_start_x,
        state->box_start_y,
        state->box_end_x,
        state->box_end_y
    };
    const EditOperationKind operation = tool_to_edit_operation(state->active_tool);
    StitchDesign edited = {};
    if (!apply_edit_operation(&state->design, state->grid, operation, rectangle, 0, false, &edited) ||
        edited.chain_count == 0U) {
        free_stitch_design(&edited);
        set_status_message(state, "Edit leaves zero stitches; not applied");
        return;
    }
    free_stitch_design(&state->design);
    state->design = edited;
    clear_live_preview(state);
    state->has_box_selection = false;
    (void)history_push(&state->history, &state->design);
    if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
        state->active_color_block_index = state->design.block_count - 1U;
    }
    set_status_message(state, "Applied edit with atomic crosses and tie-off locks");
}

static void handle_canvas_tool_mouse_down(
    EditorState *state,
    uint8_t mouse_button,
    int32_t world_x,
    int32_t world_y,
    bool shift_held
) {
    if (state->active_tool == TOOL_DRAW_CROSS) {
        if (mouse_button == SDL_BUTTON_LEFT || mouse_button == SDL_BUTTON_RIGHT) {
            state->is_drawing_crosses = true;
            state->drawing_erases = (mouse_button == SDL_BUTTON_RIGHT);
            state->draw_stroke_pushed_undo = false;
            apply_draw_x_stroke_at(state, world_x, world_y, state->drawing_erases);
        }
        return;
    }

    if (state->active_tool == TOOL_PICK_COLOR && mouse_button == SDL_BUTTON_LEFT) {
        size_t picked_chain = 0U;
        if (find_nearest_chain_at_point(&state->design, world_x, world_y, state->grid.cross_pitch_units * 2, &picked_chain)) {
            const size_t block_index = state->design.chains[picked_chain].color_block_index;
            if (block_index < state->design.block_count) {
                state->active_color_block_index = block_index;
                const ColorBlock block = state->design.blocks[block_index];
                char message[128] = {};
                (void)snprintf(
                    message,
                    sizeof(message),
                    "Picked Block #%zu (Janome #%02u #%02X%02X%02X)",
                    block_index + 1U,
                    (unsigned int)block.janome_color_index,
                    (unsigned int)block.display_color.red,
                    (unsigned int)block.display_color.green,
                    (unsigned int)block.display_color.blue
                );
                set_status_message(state, message);
            }
        }
        return;
    }

    if (state->active_tool == TOOL_SELECT_ELEMENT) {
        if (mouse_button == SDL_BUTTON_RIGHT) {
            if (state->selected_segment_count > 0U) {
                delete_active_selection(state);
            } else if (state->hover_seed_chain != SIZE_MAX && ensure_selection_mask_allocated(state)) {
                memset(state->selection_mask, 0, state->selection_mask_length);
                (void)select_connected_element_in_mask(&state->design, state->grid, state->hover_seed_chain, state->selection_mask, state->selection_mask_length, false);
                recount_selected_segments(state);
                delete_active_selection(state);
            }
            return;
        }
        if (mouse_button == SDL_BUTTON_LEFT) {
            size_t picked_chain = 0U;
            int32_t search_radius = state->grid.cross_pitch_units;
            const int32_t pixel_radius = (int32_t)llround(12.0 / state->camera.pixels_per_unit);
            if (pixel_radius > search_radius) {
                search_radius = pixel_radius;
            }
            if (!find_nearest_chain_at_point(&state->design, world_x, world_y, search_radius, &picked_chain)) {
                if (!shift_held) {
                    clear_segment_selection(state);
                    set_status_message(state, "Selection cleared");
                }
                return;
            }
            if (!ensure_selection_mask_allocated(state)) {
                return;
            }
            if (!shift_held) {
                memset(state->selection_mask, 0, state->selection_mask_length);
            }
            (void)select_connected_element_in_mask(&state->design, state->grid, picked_chain, state->selection_mask, state->selection_mask_length, shift_held);
            recount_selected_segments(state);
            char message[128] = {};
            (void)snprintf(message, sizeof(message), "Sel Elem: %zu segments | Del, Recolor, Arrows", state->selected_segment_count);
            set_status_message(state, message);
        }
        return;
    }

    if (state->active_tool == TOOL_SELECT_CROSS) {
        if (mouse_button == SDL_BUTTON_RIGHT) {
            if (state->selected_segment_count > 0U) {
                delete_active_selection(state);
            } else if (ensure_selection_mask_allocated(state)) {
                memset(state->selection_mask, 0, state->selection_mask_length);
                (void)select_cross_at_point_in_mask(&state->design, state->grid, world_x, world_y, state->selection_mask, state->selection_mask_length, false, nullptr);
                recount_selected_segments(state);
                if (state->selected_segment_count > 0U) {
                    delete_active_selection(state);
                }
            }
            return;
        }
        if (mouse_button == SDL_BUTTON_LEFT) {
            state->is_dragging_box = true;
            state->has_box_selection = true;
            state->box_start_x = world_x;
            state->box_start_y = world_y;
            state->box_end_x = world_x;
            state->box_end_y = world_y;
            state->needs_redraw = true;
        }
        return;
    }

    if (mouse_button == SDL_BUTTON_LEFT) {
        int32_t anchor_x = world_x;
        int32_t anchor_y = world_y;
        if (shift_held) {
            anchor_x = snap_coordinate_to_grid(world_x, state->grid.grid_origin_x, state->grid.cross_pitch_units);
            anchor_y = snap_coordinate_to_grid(world_y, state->grid.grid_origin_y, state->grid.cross_pitch_units);
        }
        state->is_dragging_box = true;
        state->has_box_selection = true;
        state->box_start_x = anchor_x;
        state->box_start_y = anchor_y;
        state->box_end_x = anchor_x;
        state->box_end_y = anchor_y;
        if (state->active_tool == TOOL_SPLIT_VERTICAL) {
            state->split_coordinate = anchor_x;
        } else if (state->active_tool == TOOL_SPLIT_HORIZONTAL) {
            state->split_coordinate = anchor_y;
        }
        state->preview_dirty = true;
        state->needs_redraw = true;
    }
}

static void handle_mouse_button_down(
    EditorState *state,
    const SDL_MouseButtonEvent *button_event,
    int32_t window_width,
    int32_t window_height
) {
    const int32_t mouse_x = button_event->x;
    const int32_t mouse_y = button_event->y;
    const SDL_Keymod key_mod = SDL_GetModState();
    const bool shift_held = ((unsigned int)key_mod & (unsigned int)KMOD_SHIFT) != 0U;

    if (state->palette_target != PALETTE_TARGET_NONE) {
        if (button_event->button == SDL_BUTTON_LEFT) {
            handle_palette_modal_click(state, mouse_x, mouse_y, window_width, window_height);
        }
        return;
    }

    if (state->file_dialog.mode != FILE_DIALOG_CLOSED) {
        if (button_event->button == SDL_BUTTON_LEFT) {
            handle_file_dialog_click(state, mouse_x, mouse_y, window_width, window_height, SDL_GetTicks());
        }
        return;
    }

    if (state->strip_modal_open) {
        const StripModalLayout layout = compute_strip_modal_layout(state, window_width, window_height);
        if (button_event->button == SDL_BUTTON_LEFT) {
            const uint8_t strip_command = hit_test_strip_modal_button(&layout, mouse_x, mouse_y);
            if (strip_command != STRIP_CMD_NONE) {
                handle_strip_command(state, strip_command, window_width, window_height);
                return;
            }
            state->strip_is_painting = true;
            state->strip_stroke_pushed_undo = false;
            paint_strip_cell_at_mouse(state, &layout, mouse_x, mouse_y, false);
        } else if (button_event->button == SDL_BUTTON_RIGHT) {
            state->strip_is_erasing = true;
            state->strip_stroke_pushed_undo = false;
            paint_strip_cell_at_mouse(state, &layout, mouse_x, mouse_y, true);
        }
        return;
    }

    if (button_event->button == SDL_BUTTON_LEFT && mouse_y < state->toolbar_height) {
        const uint8_t toolbar_command = hit_test_toolbar(state, mouse_x, mouse_y);
        if (toolbar_command != COMMAND_NONE) {
            execute_toolbar_command(state, toolbar_command, window_width, window_height);
        }
        return;
    }

    if (button_event->button == SDL_BUTTON_LEFT && hit_test_slider(state, mouse_x, mouse_y)) {
        state->is_dragging_slider = true;
        update_slider_from_mouse(state, mouse_x);
        return;
    }

    if (button_event->button == SDL_BUTTON_LEFT &&
        handle_left_panel_click(state, mouse_x, mouse_y, window_height)) {
        return;
    }

    if (button_event->button == SDL_BUTTON_LEFT && should_show_start_screen(state)) {
        const StartScreenLayout start_layout = compute_start_screen_layout(state, window_width, window_height);
        if (point_is_in_box(
                mouse_x,
                mouse_y,
                start_layout.open_button.left_x,
                start_layout.open_button.top_y,
                start_layout.open_button.width,
                start_layout.open_button.height
            )) {
            open_file_dialog_state(state, FILE_DIALOG_OPEN);
            return;
        }
        if (point_is_in_box(
                mouse_x,
                mouse_y,
                start_layout.strip_button.left_x,
                start_layout.strip_button.top_y,
                start_layout.strip_button.width,
                start_layout.strip_button.height
            )) {
            open_strip_modal(state);
            return;
        }
    }

    const RenderViewport viewport = get_editor_viewport(state, window_width, window_height);
    if (!point_is_in_box(mouse_x, mouse_y, viewport.left, viewport.top, viewport.width, viewport.height)) {
        return;
    }

    if (button_event->button == SDL_BUTTON_MIDDLE ||
        (button_event->button == SDL_BUTTON_LEFT && state->space_is_held)) {
        state->is_panning = true;
        state->pan_last_x = mouse_x;
        state->pan_last_y = mouse_y;
        return;
    }

    if (state->has_floating_fragment) {
        if (button_event->button == SDL_BUTTON_LEFT) {
            commit_floating_fragment(state, shift_held);
        } else if (button_event->button == SDL_BUTTON_RIGHT) {
            clear_floating_fragment(state);
            set_status_message(state, "Cancelled fragment placement");
        }
        return;
    }

    const int32_t world_x = viewport_screen_to_world_x(viewport, state->camera, mouse_x);
    const int32_t world_y = viewport_screen_to_world_y(viewport, state->camera, mouse_y);
    handle_canvas_tool_mouse_down(state, button_event->button, world_x, world_y, shift_held);
}

static void handle_mouse_button_up(
    EditorState *state,
    const SDL_MouseButtonEvent *button_event,
    int32_t window_width,
    int32_t window_height
) {
    const SDL_Keymod key_mod = SDL_GetModState();
    const bool shift_held = ((unsigned int)key_mod & (unsigned int)KMOD_SHIFT) != 0U;

    if (state->strip_modal_open) {
        state->strip_is_painting = false;
        state->strip_is_erasing = false;
        state->strip_stroke_pushed_undo = false;
        return;
    }
    if (state->is_dragging_slider && button_event->button == SDL_BUTTON_LEFT) {
        state->is_dragging_slider = false;
        return;
    }
    if (state->is_panning) {
        state->is_panning = false;
        return;
    }
    if (state->is_drawing_crosses) {
        state->is_drawing_crosses = false;
        if (state->draw_stroke_pushed_undo) {
            (void)history_push(&state->history, &state->design);
        }
        state->draw_stroke_pushed_undo = false;
        return;
    }
    if (!state->is_dragging_box || button_event->button != SDL_BUTTON_LEFT) {
        return;
    }
    state->is_dragging_box = false;

    const int32_t world_x = editor_screen_to_world_x(state, window_width, window_height, button_event->x);
    const int32_t world_y = editor_screen_to_world_y(state, window_width, window_height, button_event->y);

    if (state->active_tool == TOOL_SELECT_CROSS) {
        state->has_box_selection = false;
        int32_t drag_delta_x = state->box_end_x - state->box_start_x;
        if (drag_delta_x < 0) {
            drag_delta_x = -drag_delta_x;
        }
        int32_t drag_delta_y = state->box_end_y - state->box_start_y;
        if (drag_delta_y < 0) {
            drag_delta_y = -drag_delta_y;
        }
        if (!ensure_selection_mask_allocated(state)) {
            return;
        }
        if (!shift_held) {
            memset(state->selection_mask, 0, state->selection_mask_length);
        }
        if (drag_delta_x < 4 && drag_delta_y < 4) {
            (void)select_cross_at_point_in_mask(&state->design, state->grid, world_x, world_y, state->selection_mask, state->selection_mask_length, shift_held, nullptr);
        } else {
            const CutRectangle selection_box = {state->box_start_x, state->box_start_y, state->box_end_x, state->box_end_y};
            (void)select_crosses_in_rectangle_mask(&state->design, state->grid, selection_box, state->selection_mask, state->selection_mask_length);
        }
        recount_selected_segments(state);
        char message[128] = {};
        (void)snprintf(message, sizeof(message), "Sel X: %zu segments | Del, Recolor, Arrows", state->selected_segment_count);
        set_status_message(state, message);
        return;
    }

    if (state->active_tool == TOOL_CLONE) {
        state->has_box_selection = false;
        const CutRectangle clone_box = {state->box_start_x, state->box_start_y, state->box_end_x, state->box_end_y};
        StitchDesign cloned_patch = {};
        if (apply_edit_operation(&state->design, state->grid, EDIT_CROP_KEEP, clone_box, 0, false, &cloned_patch) &&
            cloned_patch.chain_count > 0U) {
            clear_floating_fragment(state);
            state->floating_fragment = cloned_patch;
            state->has_floating_fragment = true;
            state->floating_is_grid_origin_aligned = false;
            state->floating_anchor_x = (cloned_patch.minimum_x + cloned_patch.maximum_x) / 2;
            state->floating_anchor_y = (cloned_patch.minimum_y + cloned_patch.maximum_y) / 2;
            state->floating_offset_x = 0;
            state->floating_offset_y = 0;
            set_status_message(state, "Clone ready: move/arrows, Click/Enter stamp, Shift+Click multi");
        } else {
            free_stitch_design(&cloned_patch);
            set_status_message(state, "Clone box contained no crosses");
        }
        return;
    }

    state->preview_dirty = true;
    state->needs_redraw = true;
}

static void handle_mouse_motion(
    EditorState *state,
    const SDL_MouseMotionEvent *motion_event,
    int32_t window_width,
    int32_t window_height
) {
    state->mouse_screen_x = motion_event->x;
    state->mouse_screen_y = motion_event->y;
    state->needs_redraw = true;

    if (state->palette_target != PALETTE_TARGET_NONE || state->file_dialog.mode != FILE_DIALOG_CLOSED) {
        return;
    }

    if (state->strip_modal_open) {
        if (state->strip_is_painting || state->strip_is_erasing) {
            const StripModalLayout layout = compute_strip_modal_layout(state, window_width, window_height);
            paint_strip_cell_at_mouse(state, &layout, motion_event->x, motion_event->y, state->strip_is_erasing);
        }
        return;
    }

    if (state->is_dragging_slider) {
        update_slider_from_mouse(state, motion_event->x);
        return;
    }

    if (state->is_panning) {
        const double delta_x = (double)(motion_event->x - state->pan_last_x);
        const double delta_y = (double)(motion_event->y - state->pan_last_y);
        state->camera.center_x -= delta_x / state->camera.pixels_per_unit;
        state->camera.center_y -= delta_y / state->camera.pixels_per_unit;
        state->pan_last_x = motion_event->x;
        state->pan_last_y = motion_event->y;
        return;
    }

    const int32_t world_x = editor_screen_to_world_x(state, window_width, window_height, motion_event->x);
    const int32_t world_y = editor_screen_to_world_y(state, window_width, window_height, motion_event->y);

    if (state->has_floating_fragment) {
        const int32_t pitch = state->grid.cross_pitch_units;
        if (state->floating_is_grid_origin_aligned) {
            const int32_t phase_x = state->grid.grid_origin_x - state->floating_base_origin_x;
            const int32_t phase_y = state->grid.grid_origin_y - state->floating_base_origin_y;
            state->floating_offset_x = snap_coordinate_to_grid(world_x, phase_x, pitch);
            state->floating_offset_y = snap_coordinate_to_grid(world_y, phase_y, pitch);
        } else {
            state->floating_offset_x = snap_coordinate_to_grid(world_x - state->floating_anchor_x, 0, pitch);
            state->floating_offset_y = snap_coordinate_to_grid(world_y - state->floating_anchor_y, 0, pitch);
        }
        return;
    }

    if (state->is_drawing_crosses) {
        apply_draw_x_stroke_at(state, world_x, world_y, state->drawing_erases);
        return;
    }

    if (state->is_dragging_box) {
        const bool shift_held = ((unsigned int)SDL_GetModState() & (unsigned int)KMOD_SHIFT) != 0U;
        if (state->active_tool == TOOL_SELECT_CROSS || !shift_held) {
            state->box_end_x = world_x;
            state->box_end_y = world_y;
        } else {
            state->box_end_x = snap_coordinate_to_grid(world_x, state->grid.grid_origin_x, state->grid.cross_pitch_units);
            state->box_end_y = snap_coordinate_to_grid(world_y, state->grid.grid_origin_y, state->grid.cross_pitch_units);
        }
        if (state->active_tool == TOOL_SPLIT_VERTICAL) {
            state->split_coordinate = state->box_end_x;
        } else if (state->active_tool == TOOL_SPLIT_HORIZONTAL) {
            state->split_coordinate = state->box_end_y;
        }
        if (tool_uses_box_preview(state->active_tool)) {
            state->preview_dirty = true;
        }
        return;
    }

    update_canvas_hover(state, motion_event->x, motion_event->y, window_width, window_height);
}

static void handle_mouse_wheel(
    EditorState *state,
    const SDL_MouseWheelEvent *wheel_event,
    int32_t window_width,
    int32_t window_height
) {
    if (state->palette_target != PALETTE_TARGET_NONE) {
        return;
    }

    if (state->file_dialog.mode != FILE_DIALOG_CLOSED) {
        const FileDialogLayout layout = compute_file_dialog_layout(state, window_width, window_height);
        state->file_dialog.scroll_offset -= wheel_event->y * FILE_ROW_HEIGHT * 2;
        const int32_t content_height = (int32_t)state->file_dialog.entry_count * FILE_ROW_HEIGHT;
        int32_t max_scroll = content_height - layout.list_height;
        if (max_scroll < 0) {
            max_scroll = 0;
        }
        if (state->file_dialog.scroll_offset < 0) {
            state->file_dialog.scroll_offset = 0;
        }
        if (state->file_dialog.scroll_offset > max_scroll) {
            state->file_dialog.scroll_offset = max_scroll;
        }
        state->needs_redraw = true;
        return;
    }

    if (state->strip_modal_open) {
        return;
    }

    if (state->mouse_screen_x < LEFT_PANEL_WIDTH &&
        state->mouse_screen_y >= state->toolbar_height &&
        state->mouse_screen_y < window_height - BOTTOM_PANEL_HEIGHT) {
        state->color_scroll_offset -= wheel_event->y * COLOR_ROW_HEIGHT;
        const int32_t list_height = window_height - state->toolbar_height - BOTTOM_PANEL_HEIGHT - 62;
        const int32_t content_height = (int32_t)state->design.block_count * COLOR_ROW_HEIGHT;
        int32_t maximum_scroll = content_height - list_height;
        if (maximum_scroll < 0) {
            maximum_scroll = 0;
        }
        if (state->color_scroll_offset < 0) {
            state->color_scroll_offset = 0;
        }
        if (state->color_scroll_offset > maximum_scroll) {
            state->color_scroll_offset = maximum_scroll;
        }
        state->needs_redraw = true;
        return;
    }

    const int32_t before_world_x = editor_screen_to_world_x(state, window_width, window_height, state->mouse_screen_x);
    const int32_t before_world_y = editor_screen_to_world_y(state, window_width, window_height, state->mouse_screen_y);

    if (wheel_event->y > 0) {
        state->camera.pixels_per_unit *= 1.18;
    } else if (wheel_event->y < 0) {
        state->camera.pixels_per_unit /= 1.18;
    }
    if (state->camera.pixels_per_unit < 0.04) {
        state->camera.pixels_per_unit = 0.04;
    }
    if (state->camera.pixels_per_unit > 28.0) {
        state->camera.pixels_per_unit = 28.0;
    }

    const int32_t after_world_x = editor_screen_to_world_x(state, window_width, window_height, state->mouse_screen_x);
    const int32_t after_world_y = editor_screen_to_world_y(state, window_width, window_height, state->mouse_screen_y);
    state->camera.center_x += (double)(before_world_x - after_world_x);
    state->camera.center_y += (double)(before_world_y - after_world_y);
    state->needs_redraw = true;
}

static void handle_key_down(
    EditorState *state,
    const SDL_KeyboardEvent *key_event,
    int32_t window_width,
    int32_t window_height
) {
    const SDL_Keycode key_code = key_event->keysym.sym;
    const bool ctrl_held = ((unsigned int)key_event->keysym.mod & (unsigned int)KMOD_CTRL) != 0U;
    const bool shift_held = ((unsigned int)key_event->keysym.mod & (unsigned int)KMOD_SHIFT) != 0U;

    if (key_code == SDLK_SPACE && state->file_dialog.mode == FILE_DIALOG_CLOSED) {
        state->space_is_held = true;
        return;
    }

    if (state->palette_target != PALETTE_TARGET_NONE) {
        if (key_code == SDLK_ESCAPE) {
            state->palette_target = PALETTE_TARGET_NONE;
            state->needs_redraw = true;
        }
        return;
    }

    if (state->file_dialog.mode != FILE_DIALOG_CLOSED) {
        if (key_code == SDLK_ESCAPE) {
            close_file_dialog_state(state);
        } else if (key_code == SDLK_RETURN || key_code == SDLK_KP_ENTER) {
            execute_file_dialog_confirm(state, window_width, window_height);
        } else if (key_code == SDLK_BACKSPACE) {
            file_dialog_backspace_input(state);
        }
        return;
    }

    if (key_code == SDLK_ESCAPE) {
        if (state->strip_modal_open) {
            state->strip_modal_open = false;
            state->needs_redraw = true;
            return;
        }
        if (state->has_floating_fragment) {
            clear_floating_fragment(state);
            set_status_message(state, "Cancelled placement");
            return;
        }
        state->has_box_selection = false;
        state->is_dragging_box = false;
        clear_live_preview(state);
        clear_segment_selection(state);
        clear_hover_preview(state);
        set_status_message(state, "Cleared selection");
        return;
    }

    if (state->strip_modal_open) {
        if (ctrl_held && key_code == SDLK_z) {
            handle_strip_command(state, STRIP_CMD_UNDO, window_width, window_height);
        }
        return;
    }

    if (ctrl_held && key_code == SDLK_q) {
        state->should_quit = true;
        return;
    }
    if (ctrl_held && key_code == SDLK_o) {
        open_file_dialog_state(state, FILE_DIALOG_OPEN);
        return;
    }
    if (ctrl_held && key_code == SDLK_s) {
        if (shift_held) {
            execute_toolbar_command(state, COMMAND_SAVE_AS_DIALOG, window_width, window_height);
        } else {
            save_current_design(state);
        }
        return;
    }
    if (ctrl_held && key_code == SDLK_z) {
        if (shift_held) {
            execute_toolbar_command(state, COMMAND_REDO, window_width, window_height);
        } else {
            execute_toolbar_command(state, COMMAND_UNDO, window_width, window_height);
        }
        return;
    }
    if (ctrl_held && key_code == SDLK_y) {
        execute_toolbar_command(state, COMMAND_REDO, window_width, window_height);
        return;
    }

    switch (key_code) {
        case SDLK_1:
        case SDLK_e:
            execute_toolbar_command(state, COMMAND_TOOL_SELECT_ELEMENT, window_width, window_height);
            break;
        case SDLK_2:
        case SDLK_x:
            execute_toolbar_command(state, COMMAND_TOOL_SELECT_CROSS, window_width, window_height);
            break;
        case SDLK_3:
        case SDLK_d:
            execute_toolbar_command(state, COMMAND_TOOL_DRAW_CROSS, window_width, window_height);
            break;
        case SDLK_m:
            execute_toolbar_command(state, COMMAND_OPEN_STRIP_MODAL, window_width, window_height);
            break;
        case SDLK_4:
            execute_toolbar_command(state, COMMAND_TOOL_CROP, window_width, window_height);
            break;
        case SDLK_5:
            execute_toolbar_command(state, COMMAND_TOOL_CUT, window_width, window_height);
            break;
        case SDLK_6:
            execute_toolbar_command(state, COMMAND_TOOL_SPLIT_V, window_width, window_height);
            break;
        case SDLK_7:
            execute_toolbar_command(state, COMMAND_TOOL_SPLIT_H, window_width, window_height);
            break;
        case SDLK_8:
            execute_toolbar_command(state, COMMAND_TOOL_CLONE, window_width, window_height);
            break;
        case SDLK_9:
        case SDLK_i:
            execute_toolbar_command(state, COMMAND_TOOL_PICK, window_width, window_height);
            break;
        case SDLK_r:
            execute_toolbar_command(state, COMMAND_ROTATE_90, window_width, window_height);
            break;
        case SDLK_h:
            execute_toolbar_command(state, COMMAND_FLIP_X, window_width, window_height);
            break;
        case SDLK_v:
            execute_toolbar_command(state, COMMAND_FLIP_Y, window_width, window_height);
            break;
        case SDLK_c:
            execute_toolbar_command(state, COMMAND_CENTER, window_width, window_height);
            break;
        case SDLK_g:
            execute_toolbar_command(state, COMMAND_TOGGLE_GRID, window_width, window_height);
            break;
        case SDLK_j:
            execute_toolbar_command(state, COMMAND_TOGGLE_JUMPS, window_width, window_height);
            break;
        case SDLK_l:
            execute_toolbar_command(state, COMMAND_TOGGLE_LOCKS, window_width, window_height);
            break;
        case SDLK_o:
            execute_toolbar_command(state, COMMAND_CYCLE_HOOP, window_width, window_height);
            break;
        case SDLK_t:
            execute_toolbar_command(state, COMMAND_CYCLE_VIEW_MODE, window_width, window_height);
            break;
        case SDLK_w:
            execute_toolbar_command(state, COMMAND_CYCLE_THREAD_THICKNESS, window_width, window_height);
            break;
        case SDLK_f:
        case SDLK_0:
            execute_toolbar_command(state, COMMAND_FIT_VIEW, window_width, window_height);
            break;
        case SDLK_DELETE:
        case SDLK_BACKSPACE:
            delete_active_selection(state);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (state->has_floating_fragment) {
                commit_floating_fragment(state, shift_held);
            } else {
                apply_box_edit_or_split(state);
            }
            break;
        case SDLK_LEFT:
            nudge_active_selection_or_fragment(state, -1, 0, shift_held);
            break;
        case SDLK_RIGHT:
            nudge_active_selection_or_fragment(state, 1, 0, shift_held);
            break;
        case SDLK_UP:
            nudge_active_selection_or_fragment(state, 0, -1, shift_held);
            break;
        case SDLK_DOWN:
            nudge_active_selection_or_fragment(state, 0, 1, shift_held);
            break;
        default:
            break;
    }
}

static bool resize_pixel_canvas(
    PixelCanvas *canvas,
    SDL_Renderer *renderer,
    SDL_Texture **texture_slot,
    int32_t new_width,
    int32_t new_height
) {
    if (new_width < 64) {
        new_width = 64;
    }
    if (new_height < 64) {
        new_height = 64;
    }
    if (canvas->width == new_width && canvas->height == new_height &&
        canvas->pixels != nullptr && *texture_slot != nullptr) {
        return true;
    }

    size_t pixel_count = 0U;
    size_t pixel_bytes = 0U;
    if (ckd_mul(&pixel_count, (size_t)new_width, (size_t)new_height) ||
        ckd_mul(&pixel_bytes, pixel_count, sizeof(uint32_t))) {
        return false;
    }

    uint32_t *next_pixels = (uint32_t *)malloc(pixel_bytes);
    if (next_pixels == nullptr) {
        return false;
    }
    SDL_Texture *next_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        new_width,
        new_height
    );
    if (next_texture == nullptr) {
        free(next_pixels);
        return false;
    }

    if (*texture_slot != nullptr) {
        SDL_DestroyTexture(*texture_slot);
    }
    free(canvas->pixels);

    *texture_slot = next_texture;
    canvas->pixels = next_pixels;
    canvas->width = new_width;
    canvas->height = new_height;
    reset_canvas_clip(canvas);
    return true;
}

static void print_usage(FILE *stream) {
    (void)fprintf(stream, "Usage: embroidery-studio [input.{jef,jef+,pes,pec,dst}]\n");
}

int main(int argument_count, char **argument_values) {
    int exit_code = EXIT_FAILURE;
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *framebuffer_texture = nullptr;
    bool sdl_initialized = false;
    bool sdl_text_input_enabled = false;
    PixelCanvas canvas = {};
    EditorState state = {};

    init_default_editor_state(&state);

    const char *initial_input_path = nullptr;
    if (argument_count == 2) {
        if (strcmp(argument_values[1], "-h") == 0 || strcmp(argument_values[1], "--help") == 0) {
            print_usage(stdout);
            exit_code = EXIT_SUCCESS;
            goto cleanup;
        }
        if (argument_values[1][0] == '-') {
            (void)fprintf(stderr, "Unknown option: %s\n", argument_values[1]);
            print_usage(stderr);
            goto cleanup;
        }
        initial_input_path = argument_values[1];
    } else if (argument_count > 2) {
        (void)fprintf(stderr, "Too many arguments\n");
        print_usage(stderr);
        goto cleanup;
    }

    if (getenv("SDL_VIDEODRIVER") == nullptr) {
        (void)SDL_SetHint(SDL_HINT_VIDEODRIVER, "wayland,x11");
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        (void)fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        goto cleanup;
    }
    sdl_initialized = true;
    (void)SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    const int centered_position = (int)SDL_WINDOWPOS_CENTERED_MASK;
    const uint32_t window_flags = (uint32_t)SDL_WINDOW_RESIZABLE | (uint32_t)SDL_WINDOW_SHOWN;
    window = SDL_CreateWindow(
        "embroidery-studio (Janome MC550E)",
        centered_position,
        centered_position,
        WINDOW_INITIAL_WIDTH,
        WINDOW_INITIAL_HEIGHT,
        window_flags
    );
    if (window == nullptr) {
        (void)fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    const uint32_t renderer_flags =
        (uint32_t)SDL_RENDERER_ACCELERATED | (uint32_t)SDL_RENDERER_PRESENTVSYNC;
    renderer = SDL_CreateRenderer(window, -1, renderer_flags);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, (uint32_t)SDL_RENDERER_SOFTWARE);
    }
    if (renderer == nullptr) {
        (void)fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    int window_width = WINDOW_INITIAL_WIDTH;
    int window_height = WINDOW_INITIAL_HEIGHT;
    SDL_GetWindowSize(window, &window_width, &window_height);
    if (!resize_pixel_canvas(&canvas, renderer, &framebuffer_texture, window_width, window_height)) {
        (void)fprintf(stderr, "Failed to allocate initial framebuffer\n");
        goto cleanup;
    }
    layout_toolbar_buttons(&state, canvas.width);

    if (initial_input_path != nullptr) {
        if (!load_design_into_editor(&state, initial_input_path, canvas.width, canvas.height)) {
            goto cleanup;
        }
    }

    while (!state.should_quit) {
        SDL_Event event = {};
        if (SDL_WaitEventTimeout(&event, 16) != 0) {
            do {
                if (event.type == SDL_QUIT) {
                    state.should_quit = true;
                } else if (event.type == SDL_WINDOWEVENT &&
                           (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                            event.window.event == SDL_WINDOWEVENT_EXPOSED)) {
                    SDL_GetWindowSize(window, &window_width, &window_height);
                    if (!resize_pixel_canvas(&canvas, renderer, &framebuffer_texture, window_width, window_height)) {
                        state.should_quit = true;
                        break;
                    }
                    state.needs_redraw = true;
                } else if (event.type == SDL_DROPFILE && event.drop.file != nullptr) {
                    if (state.file_dialog.mode != FILE_DIALOG_CLOSED) {
                        close_file_dialog_state(&state);
                    }
                    if (state.design.chain_count == 0U || !state.drop_merge_mode) {
                        (void)load_design_into_editor(&state, event.drop.file, canvas.width, canvas.height);
                    } else {
                        (void)load_dropped_file_as_merge_fragment(&state, event.drop.file, canvas.width, canvas.height);
                    }
                    SDL_free(event.drop.file);
                } else if (event.type == SDL_TEXTINPUT) {
                    file_dialog_append_text_input(&state, event.text.text);
                } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    handle_mouse_button_down(&state, &event.button, canvas.width, canvas.height);
                } else if (event.type == SDL_MOUSEBUTTONUP) {
                    handle_mouse_button_up(&state, &event.button, canvas.width, canvas.height);
                } else if (event.type == SDL_MOUSEMOTION) {
                    handle_mouse_motion(&state, &event.motion, canvas.width, canvas.height);
                } else if (event.type == SDL_MOUSEWHEEL) {
                    handle_mouse_wheel(&state, &event.wheel, canvas.width, canvas.height);
                } else if (event.type == SDL_KEYDOWN) {
                    handle_key_down(&state, &event.key, canvas.width, canvas.height);
                } else if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_SPACE) {
                    state.space_is_held = false;
                }
            } while (SDL_PollEvent(&event) != 0);
        }

        sync_sdl_text_input(&sdl_text_input_enabled, state.text_input_active);

        if (state.needs_redraw) {
            render_editor_frame(&canvas, &state);
            (void)SDL_UpdateTexture(framebuffer_texture, nullptr, canvas.pixels, canvas.width * (int)sizeof(uint32_t));
            (void)SDL_RenderClear(renderer);
            (void)SDL_RenderCopy(renderer, framebuffer_texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            state.needs_redraw = false;
        }
    }

    exit_code = EXIT_SUCCESS;

cleanup:
    sync_sdl_text_input(&sdl_text_input_enabled, false);
    free_editor_state(&state);
    free_pixel_canvas(&canvas);
    if (framebuffer_texture != nullptr) {
        SDL_DestroyTexture(framebuffer_texture);
    }
    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
    }
    if (sdl_initialized) {
        SDL_Quit();
    }
    return exit_code;
}
