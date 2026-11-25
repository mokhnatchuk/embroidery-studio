#define _POSIX_C_SOURCE 200809L

#include "formats.h"
#include "model.h"
#include "ops.h"
#include "render.h"
#include "ui.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr int32_t MAX_CANVAS_DIMENSION = 8192;

typedef enum : uint8_t {
    DIALOG_ACTION_NONE = 0U,
    DIALOG_ACTION_OPEN = 1U,
    DIALOG_ACTION_MERGE = 2U,
    DIALOG_ACTION_SAVE_AS = 3U,
    DIALOG_ACTION_STRIP_SAVE_AS = 4U
} NativeDialogAction;

typedef enum : uint8_t {
    DISCARD_NONE = 0U,
    DISCARD_QUIT = 1U,
    DISCARD_OPEN = 2U
} PendingDiscardKind;

typedef struct {
    NativeDialogAction action;
    uint32_t user_event_type;
} FileDialogRequest;

typedef struct {
    NativeDialogAction action;
    bool failed;
    bool cancelled;
    char path[MAX_PATH_BUFFER];
    char error_message[128];
} FileDialogResult;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    PixelCanvas canvas;
    SDL_DialogFileFilter open_file_filters[OPEN_DIALOG_FILTER_COUNT];
    SDL_DialogFileFilter save_file_filters[SAVE_DIALOG_FILTER_COUNT];
    uint32_t dialog_event_type;
    PendingDiscardKind pending_discard;
    bool last_title_dirty;
    char last_title_path[MAX_PATH_BUFFER];
} AppHost;

static void init_host_dialog_filters(AppHost *host) {
    for (size_t filter_index = 0U; filter_index < OPEN_DIALOG_FILTER_COUNT; filter_index += 1U) {
        host->open_file_filters[filter_index].name = open_dialog_filters[filter_index].name;
        host->open_file_filters[filter_index].pattern = open_dialog_filters[filter_index].pattern;
    }
    for (size_t filter_index = 0U; filter_index < SAVE_DIALOG_FILTER_COUNT; filter_index += 1U) {
        host->save_file_filters[filter_index].name = save_dialog_filters[filter_index].name;
        host->save_file_filters[filter_index].pattern = save_dialog_filters[filter_index].pattern;
    }
}

/* Background-safe SDL3 callback: copies chosen path and posts user event to main loop. */
static void SDLCALL native_file_dialog_callback(
    void *userdata,
    const char *const *filelist,
    int filter_index
) {
    (void)filter_index;
    FileDialogRequest *request = (FileDialogRequest *)userdata;
    if (request == nullptr) {
        return;
    }

    FileDialogResult *result = (FileDialogResult *)calloc(1U, sizeof(FileDialogResult));
    if (result != nullptr) {
        result->action = request->action;
        if (filelist == nullptr) {
            result->failed = true;
            const char *sdl_error = SDL_GetError();
            if (sdl_error != nullptr && sdl_error[0] != '\0') {
                (void)snprintf(result->error_message, sizeof(result->error_message), "%.120s", sdl_error);
            } else {
                (void)snprintf(result->error_message, sizeof(result->error_message), "System dialog unavailable");
            }
        } else if (filelist[0] == nullptr || filelist[0][0] == '\0') {
            result->cancelled = true;
        } else {
            (void)snprintf(result->path, sizeof(result->path), "%s", filelist[0]);
        }

        SDL_Event user_event = {};
        user_event.type = request->user_event_type;
        user_event.user.data1 = result;
        if (!SDL_PushEvent(&user_event)) {
            free(result);
        }
    }
    free(request);
}

static void build_default_dialog_location(
    const EditorState *state,
    bool for_save,
    const char *save_suffix,
    char *output_path,
    size_t output_capacity
) {
    output_path[0] = '\0';
    if (for_save) {
        const char *base_path = "untitled.jef";
        if (state->has_file_path) {
            base_path = state->current_file_path;
        }
        if (build_unique_suffixed_jef_path(base_path, save_suffix, output_path, output_capacity)) {
            return;
        }
    }
    if (state->has_file_path) {
        const char *last_slash = strrchr(state->current_file_path, '/');
        if (last_slash != nullptr) {
            const size_t dir_len = (size_t)(last_slash - state->current_file_path) + 1U;
            if (dir_len < output_capacity) {
                memcpy(output_path, state->current_file_path, dir_len);
                output_path[dir_len] = '\0';
                return;
            }
        }
    }
    const char *home_dir = getenv("HOME");
    if (home_dir != nullptr && home_dir[0] != '\0') {
        (void)snprintf(output_path, output_capacity, "%s/", home_dir);
    }
}

static void launch_native_file_dialog(AppHost *host, EditorState *state, NativeDialogAction action) {
    if (host->dialog_event_type == 0U) {
        set_status_message(state, "System file dialog unavailable; use Drag & Drop or CLI");
        return;
    }

    FileDialogRequest *request = (FileDialogRequest *)calloc(1U, sizeof(FileDialogRequest));
    if (request == nullptr) {
        set_status_message(state, "Out of memory opening file dialog");
        return;
    }
    request->action = action;
    request->user_event_type = host->dialog_event_type;

    char default_location[MAX_PATH_BUFFER] = {};
    const char *location_arg = nullptr;
    if (action == DIALOG_ACTION_OPEN || action == DIALOG_ACTION_MERGE) {
        build_default_dialog_location(state, false, "", default_location, sizeof(default_location));
        if (default_location[0] != '\0') {
            location_arg = default_location;
        }
        SDL_ShowOpenFileDialog(
            native_file_dialog_callback,
            request,
            host->window,
            host->open_file_filters,
            (int)OPEN_DIALOG_FILTER_COUNT,
            location_arg,
            false
        );
        set_status_message(state, "Waiting for system file dialog...");
        return;
    }

    const char *suffix = "_edited";
    if (action == DIALOG_ACTION_STRIP_SAVE_AS) {
        suffix = "_strip";
    }
    build_default_dialog_location(state, true, suffix, default_location, sizeof(default_location));
    if (default_location[0] != '\0') {
        location_arg = default_location;
    }
    SDL_ShowSaveFileDialog(
        native_file_dialog_callback,
        request,
        host->window,
        host->save_file_filters,
        (int)SAVE_DIALOG_FILTER_COUNT,
        location_arg
    );
    set_status_message(state, "Waiting for system save dialog...");
}

static bool ensure_jef_extension(const char *input_path, char *output_path, size_t output_capacity) {
    const size_t input_len = strlen(input_path);
    const bool has_jef =
        input_len >= 4U && strings_equal_ignore_case(input_path + (input_len - 4U), ".jef");
    int written = 0;
    if (has_jef) {
        written = snprintf(output_path, output_capacity, "%s", input_path);
    } else {
        written = snprintf(output_path, output_capacity, "%s.jef", input_path);
    }
    return written > 0 && (size_t)written < output_capacity;
}

static void sync_window_title(AppHost *host, const EditorState *state) {
    const char *current_path = "";
    if (state->has_file_path) {
        current_path = state->current_file_path;
    }
    if (host->last_title_dirty == state->is_dirty &&
        strcmp(host->last_title_path, current_path) == 0) {
        return;
    }

    char title[256] = {};
    if (state->has_file_path) {
        if (state->is_dirty) {
            (void)snprintf(title, sizeof(title), "Embroidery Studio - *%.180s", basename_of_path(current_path));
        } else {
            (void)snprintf(title, sizeof(title), "Embroidery Studio - %.180s", basename_of_path(current_path));
        }
    } else if (state->is_dirty) {
        (void)snprintf(title, sizeof(title), "Embroidery Studio - *untitled.jef");
    } else {
        (void)snprintf(title, sizeof(title), "Embroidery Studio");
    }
    (void)SDL_SetWindowTitle(host->window, title);
    host->last_title_dirty = state->is_dirty;
    (void)snprintf(host->last_title_path, sizeof(host->last_title_path), "%s", current_path);
}

static void clear_pending_discard(AppHost *host) {
    host->pending_discard = DISCARD_NONE;
}

static bool load_design_into_editor(
    AppHost *host,
    EditorState *state,
    const char *file_path,
    int32_t canvas_width,
    int32_t canvas_height
) {
    StitchDesign loaded = {};
    if (!load_embroidery_file(file_path, &loaded)) {
        set_status_message(state, "Failed to load embroidery file");
        return false;
    }

    clear_pending_discard(host);
    clear_segment_selection(state);
    clear_hover_preview(state);
    clear_floating_fragment(state);
    clear_live_preview(state);
    state->has_box_selection = false;
    state->is_dragging_box = false;

    free_stitch_design(&state->design);
    state->design = loaded;
    state->is_dirty = false;
    editor_mark_design_changed(state, false);
    history_clear(&state->history);
    (void)history_push(&state->history, &state->design);

    state->grid = detect_cross_stitch_grid(&state->design);
    editor_ensure_spatial_index(state);
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
        const int32_t half_width = standard_hoop_presets[preset_index].width_units / 2;
        const int32_t half_height = standard_hoop_presets[preset_index].height_units / 2;
        if (state->design.minimum_x >= -half_width && state->design.maximum_x <= half_width &&
            state->design.minimum_y >= -half_height && state->design.maximum_y <= half_height) {
            state->hoop_preset_index = preset_index;
            break;
        }
    }

    fit_camera_to_design(state, canvas_width, canvas_height);

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
    int32_t canvas_width,
    int32_t canvas_height
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
        editor_screen_to_world_x(state, canvas_width, canvas_height, state->mouse_screen_x);
    const int32_t cursor_world_y =
        editor_screen_to_world_y(state, canvas_width, canvas_height, state->mouse_screen_y);
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

static void save_current_design(AppHost *host, EditorState *state) {
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
            state->is_dirty = false;
            clear_pending_discard(host);
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
    state->is_dirty = false;
    clear_pending_discard(host);
    char message[256] = {};
    (void)snprintf(message, sizeof(message), "Saved %.120s", basename_of_path(output_path));
    set_status_message(state, message);
}

static void handle_native_dialog_result(
    AppHost *host,
    EditorState *state,
    FileDialogResult *result,
    int32_t canvas_width,
    int32_t canvas_height
) {
    if (result == nullptr) {
        return;
    }
    if (result->failed) {
        char message[256] = {};
        (void)snprintf(
            message,
            sizeof(message),
            "Dialog error (use Drag&Drop/CLI): %.120s",
            result->error_message
        );
        set_status_message(state, message);
        free(result);
        return;
    }
    if (result->cancelled) {
        set_status_message(state, "File dialog canceled");
        free(result);
        return;
    }

    if (result->action == DIALOG_ACTION_OPEN) {
        (void)load_design_into_editor(host, state, result->path, canvas_width, canvas_height);
    } else if (result->action == DIALOG_ACTION_MERGE) {
        (void)load_dropped_file_as_merge_fragment(state, result->path, canvas_width, canvas_height);
    } else if (result->action == DIALOG_ACTION_SAVE_AS) {
        char final_path[MAX_PATH_BUFFER] = {};
        if (!ensure_jef_extension(result->path, final_path, sizeof(final_path))) {
            set_status_message(state, "Output path too long");
        } else if (!write_jef_file(&state->design, final_path)) {
            set_status_message(state, "Failed to write JEF file");
        } else {
            (void)snprintf(state->current_file_path, sizeof(state->current_file_path), "%s", final_path);
            state->has_file_path = true;
            state->is_dirty = false;
            clear_pending_discard(host);
            char message[256] = {};
            (void)snprintf(message, sizeof(message), "Saved %.120s", basename_of_path(final_path));
            set_status_message(state, message);
        }
    } else if (result->action == DIALOG_ACTION_STRIP_SAVE_AS) {
        char final_path[MAX_PATH_BUFFER] = {};
        StitchDesign strip_design = {};
        if (!ensure_jef_extension(result->path, final_path, sizeof(final_path))) {
            set_status_message(state, "Output path too long");
        } else if (!build_strip_design(&state->strip_pattern, &strip_design) || strip_design.chain_count == 0U) {
            set_status_message(state, "Strip is empty; paint crosses first");
        } else if (!write_jef_file(&strip_design, final_path)) {
            set_status_message(state, "Failed to save strip JEF");
        } else {
            char message[256] = {};
            (void)snprintf(message, sizeof(message), "Saved strip %.120s", basename_of_path(final_path));
            set_status_message(state, message);
        }
        free_stitch_design(&strip_design);
    }
    free(result);
}

static void request_open_with_dirty_guard(AppHost *host, EditorState *state) {
    if (state->is_dirty && host->pending_discard != DISCARD_OPEN) {
        host->pending_discard = DISCARD_OPEN;
        set_status_message(state, "Unsaved changes! Press Ctrl+S to save, or click Open again");
        return;
    }
    clear_pending_discard(host);
    launch_native_file_dialog(host, state, DIALOG_ACTION_OPEN);
}

static void request_quit_with_dirty_guard(AppHost *host, EditorState *state) {
    if (state->is_dirty && host->pending_discard != DISCARD_QUIT) {
        host->pending_discard = DISCARD_QUIT;
        set_status_message(state, "Unsaved changes! Press Ctrl+S to save, or quit again to discard");
        return;
    }
    state->should_quit = true;
}

static void delete_active_selection(AppHost *host, EditorState *state) {
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
    clear_pending_discard(host);
    editor_mark_design_changed(state, true);
    (void)history_push(&state->history, &state->design);
    if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
        state->active_color_block_index = state->design.block_count - 1U;
    }
    clear_segment_selection(state);
    clear_hover_preview(state);
    set_status_message(state, "Deleted selection with tie-off locks");
}

static void commit_floating_fragment(AppHost *host, EditorState *state, bool keep_stamping) {
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
    clear_pending_discard(host);
    editor_mark_design_changed(state, true);
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
    AppHost *host,
    EditorState *state,
    uint8_t command_id,
    int32_t canvas_width,
    int32_t canvas_height
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
            request_open_with_dirty_guard(host, state);
            break;
        case COMMAND_MERGE_FILE_DIALOG:
            launch_native_file_dialog(host, state, DIALOG_ACTION_MERGE);
            break;
        case COMMAND_SAVE_AS_DIALOG:
            if (state->design.chain_count == 0U) {
                set_status_message(state, "Nothing to save");
            } else {
                launch_native_file_dialog(host, state, DIALOG_ACTION_SAVE_AS);
            }
            break;
        case COMMAND_TOGGLE_RIGHT_PANEL:
            state->right_panel_collapsed = !state->right_panel_collapsed;
            invalidate_stitch_layer_cache(&state->stitch_cache);
            state->needs_redraw = true;
            break;
        case COMMAND_ADD_COLOR_BLOCK:
            state->palette_target = PALETTE_TARGET_ADD_BLOCK;
            state->needs_redraw = true;
            break;
        case COMMAND_ROTATE_90:
            if (state->design.chain_count > 0U) {
                rotate_design_90_clockwise(&state->design);
                state->grid = detect_cross_stitch_grid(&state->design);
                clear_pending_discard(host);
                editor_mark_design_changed(state, true);
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
                clear_pending_discard(host);
                editor_mark_design_changed(state, true);
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
                clear_pending_discard(host);
                editor_mark_design_changed(state, true);
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
                clear_pending_discard(host);
                editor_mark_design_changed(state, true);
                (void)history_push(&state->history, &state->design);
                fit_camera_to_design(state, canvas_width, canvas_height);
                set_status_message(state, "Centered design at (0, 0)");
            }
            break;
        case COMMAND_MERGE_COLORS:
            if (state->design.block_count > 1U) {
                const size_t before_blocks = state->design.block_count;
                if (merge_duplicate_color_blocks(&state->design)) {
                    clear_pending_discard(host);
                    editor_mark_design_changed(state, true);
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
            delete_active_selection(host, state);
            break;
        case COMMAND_UNDO:
            if (history_undo(&state->history, &state->design)) {
                clear_pending_discard(host);
                editor_mark_design_changed(state, true);
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
                clear_pending_discard(host);
                editor_mark_design_changed(state, true);
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
            save_current_design(host, state);
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
            if (state->hoop_preset_index == 0U) {
                set_status_message(state, "Hoop overlay: Off");
            } else {
                char message[64] = {};
                (void)snprintf(
                    message,
                    sizeof(message),
                    "Hoop overlay: %s mm",
                    standard_hoop_presets[state->hoop_preset_index].label
                );
                set_status_message(state, message);
            }
            break;
        case COMMAND_ROTATE_HOOP:
            state->hoop_rotated = !state->hoop_rotated;
            {
                int32_t hoop_width = 0;
                int32_t hoop_height = 0;
                if (get_hoop_dimensions(state->hoop_preset_index, state->hoop_rotated, &hoop_width, &hoop_height)) {
                    char message[64] = {};
                    (void)snprintf(
                        message,
                        sizeof(message),
                        "Hoop overlay: %dx%d mm",
                        hoop_width / 10,
                        hoop_height / 10
                    );
                    set_status_message(state, message);
                } else {
                    state->needs_redraw = true;
                }
            }
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
            fit_camera_to_design(state, canvas_width, canvas_height);
            break;
        default:
            break;
    }
}

static void apply_palette_selection(AppHost *host, EditorState *state, uint8_t chosen_janome_index) {
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
            clear_pending_discard(host);
            editor_mark_design_changed(state, true);
            (void)history_push(&state->history, &state->design);
            char message[128] = {};
            (void)snprintf(message, sizeof(message), "Added Color Block #%zu (JEF #%02u)", new_block_index + 1U, (unsigned int)valid_janome);
            set_status_message(state, message);
        }
        return;
    }

    if (target == PALETTE_TARGET_RECOLOR_BLOCK &&
        state->palette_target_block_index < state->design.block_count) {
        state->design.blocks[state->palette_target_block_index].janome_color_index = valid_janome;
        state->design.blocks[state->palette_target_block_index].display_color = janome_palette[valid_janome];
        clear_pending_discard(host);
        editor_mark_design_changed(state, true);
        (void)history_push(&state->history, &state->design);
        char message[128] = {};
        (void)snprintf(message, sizeof(message), "Changed Block #%zu to JEF #%02u", state->palette_target_block_index + 1U, (unsigned int)valid_janome);
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
            clear_pending_discard(host);
            editor_mark_design_changed(state, true);
            (void)history_push(&state->history, &state->design);
            clear_segment_selection(state);
            clear_hover_preview(state);
            char message[128] = {};
            (void)snprintf(message, sizeof(message), "Recolored selection to JEF #%02u", (unsigned int)valid_janome);
            set_status_message(state, message);
        }
    }
}

static void handle_palette_modal_click(
    AppHost *host,
    EditorState *state,
    int32_t mouse_x,
    int32_t mouse_y,
    int32_t canvas_width,
    int32_t canvas_height
) {
    int32_t palette_index = -1;
    if (!hit_test_palette_modal(state, mouse_x, mouse_y, canvas_width, canvas_height, &palette_index)) {
        state->palette_target = PALETTE_TARGET_NONE;
        state->needs_redraw = true;
        return;
    }
    if (palette_index >= 0) {
        apply_palette_selection(host, state, (uint8_t)palette_index);
    }
}

static void paint_strip_cell_at_mouse(
    EditorState *state,
    const StripModalLayout *layout,
    int32_t mouse_x,
    int32_t mouse_y,
    bool erase_cell
) {
    int32_t column = 0;
    int32_t row = 0;
    if (!hit_test_strip_editor_cell(&state->strip_pattern, layout, mouse_x, mouse_y, &column, &row)) {
        return;
    }

    uint8_t target_value = state->strip_active_color;
    if (target_value == 0U || target_value >= JANOME_PALETTE_COUNT) {
        target_value = 10U;
    }
    if (erase_cell) {
        target_value = 0U;
    }

    const size_t cell_offset = (size_t)row * (size_t)STRIP_MAX_GRID_DIM + (size_t)column;
    if (state->strip_pattern.cells[cell_offset] != target_value) {
        if (!state->strip_stroke_pushed_undo) {
            strip_history_push(state);
            state->strip_stroke_pushed_undo = true;
        }
        state->strip_pattern.cells[cell_offset] = target_value;
        state->needs_redraw = true;
    }
}

static void handle_strip_command(
    AppHost *host,
    EditorState *state,
    uint8_t command_id,
    int32_t canvas_width,
    int32_t canvas_height
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
                state->strip_pattern.repeat_x -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_REPEAT_X_INC:
            if (state->strip_pattern.repeat_x < STRIP_MAX_REPEAT) {
                state->strip_pattern.repeat_x += 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_REPEAT_Y_DEC:
            if (state->strip_pattern.repeat_y > 1) {
                state->strip_pattern.repeat_y -= 1;
                state->needs_redraw = true;
            }
            break;
        case STRIP_CMD_REPEAT_Y_INC:
            if (state->strip_pattern.repeat_y < STRIP_MAX_REPEAT) {
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
                    const int32_t opposite = state->strip_pattern.width - 1 - column;
                    const size_t left_offset = (size_t)row * (size_t)STRIP_MAX_GRID_DIM + (size_t)column;
                    const size_t right_offset = (size_t)row * (size_t)STRIP_MAX_GRID_DIM + (size_t)opposite;
                    const uint8_t swap_cell = state->strip_pattern.cells[left_offset];
                    state->strip_pattern.cells[left_offset] = state->strip_pattern.cells[right_offset];
                    state->strip_pattern.cells[right_offset] = swap_cell;
                }
            }
            state->needs_redraw = true;
            break;
        case STRIP_CMD_MIRROR_Y:
            strip_history_push(state);
            for (int32_t row = 0; row < state->strip_pattern.height / 2; row += 1) {
                const int32_t opposite_row = state->strip_pattern.height - 1 - row;
                for (int32_t column = 0; column < state->strip_pattern.width; column += 1) {
                    const size_t top_offset = (size_t)row * (size_t)STRIP_MAX_GRID_DIM + (size_t)column;
                    const size_t bottom_offset = (size_t)opposite_row * (size_t)STRIP_MAX_GRID_DIM + (size_t)column;
                    const uint8_t swap_cell = state->strip_pattern.cells[top_offset];
                    state->strip_pattern.cells[top_offset] = state->strip_pattern.cells[bottom_offset];
                    state->strip_pattern.cells[bottom_offset] = swap_cell;
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
            StitchDesign generated = {};
            if (!build_strip_design(&state->strip_pattern, &generated) || generated.chain_count == 0U) {
                free_stitch_design(&generated);
                set_status_message(state, "Strip is empty; paint at least one cross");
                break;
            }
            state->strip_modal_open = false;
            if (state->design.chain_count == 0U) {
                free_stitch_design(&state->design);
                state->design = generated;
                state->grid = detect_cross_stitch_grid(&state->design);
                state->grid.cross_pitch_units = state->strip_pattern.pitch_units;
                state->grid.cross_passes = state->strip_pattern.passes;
                clear_pending_discard(host);
                editor_mark_design_changed(state, true);
                history_clear(&state->history);
                (void)history_push(&state->history, &state->design);
                fit_camera_to_design(state, canvas_width, canvas_height);
                set_status_message(state, "Created new cross-stitch strip design");
            } else {
                clear_floating_fragment(state);
                state->floating_fragment = generated;
                state->has_floating_fragment = true;
                state->floating_is_grid_origin_aligned = false;
                state->floating_anchor_x = (generated.minimum_x + generated.maximum_x) / 2;
                state->floating_anchor_y = (generated.minimum_y + generated.maximum_y) / 2;
                state->floating_offset_x = 0;
                state->floating_offset_y = 0;
                set_status_message(state, "Strip ready: move/arrows to dock, Click/Enter place, Shift+Click stamp");
            }
            break;
        }
        case STRIP_CMD_SAVE_JEF: {
            StitchDesign generated = {};
            if (!build_strip_design(&state->strip_pattern, &generated) || generated.chain_count == 0U) {
                free_stitch_design(&generated);
                set_status_message(state, "Strip is empty; paint crosses before saving");
                break;
            }
            const char *base_path = "strip.jef";
            if (state->has_file_path) {
                base_path = state->current_file_path;
            }
            char output_path[MAX_PATH_BUFFER] = {};
            if (build_unique_suffixed_jef_path(base_path, "_strip", output_path, sizeof(output_path)) &&
                write_jef_file(&generated, output_path)) {
                char message[256] = {};
                (void)snprintf(message, sizeof(message), "Saved strip %.120s", basename_of_path(output_path));
                set_status_message(state, message);
            } else {
                set_status_message(state, "Failed to save strip JEF");
            }
            free_stitch_design(&generated);
            break;
        }
        case STRIP_CMD_SAVE_AS:
            launch_native_file_dialog(host, state, DIALOG_ACTION_STRIP_SAVE_AS);
            break;
        case STRIP_CMD_CLOSE:
            state->strip_modal_open = false;
            state->needs_redraw = true;
            break;
        default:
            break;
    }
}

static void toggle_solo_color_block(AppHost *host, EditorState *state, size_t block_index) {
    if (block_index >= state->design.block_count) {
        return;
    }
    bool any_other_enabled = false;
    for (size_t index = 0U; index < state->design.block_count; index += 1U) {
        if (index != block_index && state->design.blocks[index].enabled) {
            any_other_enabled = true;
            break;
        }
    }

    if (any_other_enabled || !state->design.blocks[block_index].enabled) {
        for (size_t index = 0U; index < state->design.block_count; index += 1U) {
            state->design.blocks[index].enabled = (index == block_index);
        }
        char message[128] = {};
        (void)snprintf(message, sizeof(message), "Solo Color Block #%zu (RMB again to restore all)", block_index + 1U);
        set_status_message(state, message);
    } else {
        for (size_t index = 0U; index < state->design.block_count; index += 1U) {
            state->design.blocks[index].enabled = true;
        }
        set_status_message(state, "Restored visibility of all color blocks");
    }

    recompute_design_bounds(&state->design);
    clear_pending_discard(host);
    editor_mark_design_changed(state, true);
    (void)history_push(&state->history, &state->design);
    state->preview_dirty = true;
    clear_hover_preview(state);
}

static bool handle_color_list_click(
    AppHost *host,
    EditorState *state,
    const EditorLayout *layout,
    uint8_t mouse_button,
    int32_t mouse_x,
    int32_t mouse_y
) {
    size_t block_index = SIZE_MAX;
    const ColorListHitKind hit =
        hit_test_color_list(state, layout, mouse_x, mouse_y, &block_index);
    if (hit == COLOR_LIST_HIT_NONE) {
        return false;
    }
    if (hit == COLOR_LIST_HIT_PANEL_EMPTY || block_index >= state->design.block_count) {
        return true;
    }

    /* Right-click on V button or row toggles Solo visibility for that color block. */
    if (mouse_button == SDL_BUTTON_RIGHT) {
        toggle_solo_color_block(host, state, block_index);
        return true;
    }

    if (hit == COLOR_LIST_HIT_TOGGLE_VISIBILITY) {
        state->design.blocks[block_index].enabled = !state->design.blocks[block_index].enabled;
        recompute_design_bounds(&state->design);
        clear_pending_discard(host);
        editor_mark_design_changed(state, true);
        (void)history_push(&state->history, &state->design);
        state->preview_dirty = true;
        clear_hover_preview(state);
        return true;
    }
    if (hit == COLOR_LIST_HIT_MOVE_UP) {
        if (move_color_block_order(&state->design, block_index, -1)) {
            state->active_color_block_index = block_index - 1U;
            clear_pending_discard(host);
            editor_mark_design_changed(state, true);
            (void)history_push(&state->history, &state->design);
            clear_segment_selection(state);
            clear_hover_preview(state);
            set_status_message(state, "Moved color block earlier in stitch order");
        }
        return true;
    }
    if (hit == COLOR_LIST_HIT_MOVE_DOWN) {
        if (move_color_block_order(&state->design, block_index, 1)) {
            state->active_color_block_index = block_index + 1U;
            clear_pending_discard(host);
            editor_mark_design_changed(state, true);
            (void)history_push(&state->history, &state->design);
            clear_segment_selection(state);
            clear_hover_preview(state);
            set_status_message(state, "Moved color block later in stitch order");
        }
        return true;
    }
    if (hit == COLOR_LIST_HIT_RECOLOR_BLOCK) {
        state->palette_target = PALETTE_TARGET_RECOLOR_BLOCK;
        state->palette_target_block_index = block_index;
        state->needs_redraw = true;
        return true;
    }
    if (hit == COLOR_LIST_HIT_DELETE_BLOCK) {
        if (remove_color_block_at(&state->design, block_index)) {
            if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
                state->active_color_block_index = state->design.block_count - 1U;
            }
            clear_pending_discard(host);
            editor_mark_design_changed(state, true);
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
        "Active Color: Block #%zu (JEF #%02u #%02X%02X%02X | RMB=Solo)",
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
    AppHost *host,
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
        editor_ensure_spatial_index(state);
        size_t mask_bytes = 0U;
        if (!scratch_arena_add_slice_bytes(&mask_bytes, total_segments, sizeof(uint8_t)) ||
            !scratch_arena_prepare(&state->scratch, mask_bytes)) {
            return;
        }
        uint8_t *erase_mask = (uint8_t *)scratch_arena_push(&state->scratch, total_segments, sizeof(uint8_t));
        if (erase_mask == nullptr) {
            return;
        }
        memset(erase_mask, 0, total_segments);
        bool snapped = false;
        const CutRectangle cross_box = resolve_cross_box_at_point_indexed(
            &state->design,
            &state->spatial_index,
            state->grid,
            world_x,
            world_y,
            &snapped
        );
        const size_t matched = select_cross_box_in_mask_indexed(
            &state->design,
            &state->spatial_index,
            cross_box,
            erase_mask,
            total_segments,
            false
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
                clear_pending_discard(host);
                editor_mark_design_changed(state, true);
                state->draw_stroke_pushed_undo = true;
                state->last_drawn_column = cell_column;
                state->last_drawn_row = cell_row;
                clear_segment_selection(state);
                clear_hover_preview(state);
            }
        }
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
        clear_pending_discard(host);
        editor_mark_design_changed(state, true);
        state->draw_stroke_pushed_undo = true;
        state->last_drawn_column = cell_column;
        state->last_drawn_row = cell_row;
        clear_segment_selection(state);
        clear_hover_preview(state);
    }
}

static void nudge_active_selection_or_fragment(
    AppHost *host,
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
            clear_pending_discard(host);
            editor_mark_design_changed(state, true);
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
        clear_pending_discard(host);
        editor_mark_design_changed(state, true);
        (void)history_push(&state->history, &state->design);
        set_status_message(state, "Shifted design center");
    }
}

static void apply_box_edit_or_split(AppHost *host, EditorState *state) {
    if (!state->has_box_selection) {
        return;
    }
    if (state->active_tool == TOOL_SPLIT_VERTICAL || state->active_tool == TOOL_SPLIT_HORIZONTAL) {
        save_current_design(host, state);
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
    clear_pending_discard(host);
    editor_mark_design_changed(state, true);
    clear_live_preview(state);
    state->has_box_selection = false;
    (void)history_push(&state->history, &state->design);
    if (state->active_color_block_index >= state->design.block_count && state->design.block_count > 0U) {
        state->active_color_block_index = state->design.block_count - 1U;
    }
    set_status_message(state, "Applied edit with atomic crosses and tie-off locks");
}

static void handle_canvas_tool_mouse_down(
    AppHost *host,
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
            apply_draw_x_stroke_at(host, state, world_x, world_y, state->drawing_erases);
        }
        return;
    }

    if (state->active_tool == TOOL_PICK_COLOR && mouse_button == SDL_BUTTON_LEFT) {
        editor_ensure_spatial_index(state);
        size_t picked_chain = 0U;
        if (find_nearest_chain_at_point_indexed(
                &state->design,
                &state->spatial_index,
                world_x,
                world_y,
                state->grid.cross_pitch_units * 2,
                &picked_chain
            )) {
            const size_t block_index = state->design.chains[picked_chain].color_block_index;
            if (block_index < state->design.block_count) {
                state->active_color_block_index = block_index;
                const ColorBlock block = state->design.blocks[block_index];
                char message[128] = {};
                (void)snprintf(
                    message,
                    sizeof(message),
                    "Picked Block #%zu (JEF #%02u #%02X%02X%02X)",
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
        editor_ensure_spatial_index(state);
        if (mouse_button == SDL_BUTTON_RIGHT) {
            if (state->selected_segment_count > 0U) {
                delete_active_selection(host, state);
            } else if (state->hover_seed_chain != SIZE_MAX && ensure_selection_mask_allocated(state)) {
                memset(state->selection_mask, 0, state->selection_mask_length);
                (void)select_connected_element_in_mask_indexed(
                    &state->design,
                    &state->spatial_index,
                    state->hover_seed_chain,
                    state->selection_mask,
                    state->selection_mask_length,
                    false
                );
                recount_selected_segments(state);
                delete_active_selection(host, state);
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
            if (!find_nearest_chain_at_point_indexed(
                    &state->design,
                    &state->spatial_index,
                    world_x,
                    world_y,
                    search_radius,
                    &picked_chain
                )) {
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
            (void)select_connected_element_in_mask_indexed(
                &state->design,
                &state->spatial_index,
                picked_chain,
                state->selection_mask,
                state->selection_mask_length,
                shift_held
            );
            recount_selected_segments(state);
            char message[128] = {};
            (void)snprintf(message, sizeof(message), "Sel Elem: %zu segments | Del, Recolor, Arrows", state->selected_segment_count);
            set_status_message(state, message);
        }
        return;
    }

    if (state->active_tool == TOOL_SELECT_CROSS) {
        editor_ensure_spatial_index(state);
        if (mouse_button == SDL_BUTTON_RIGHT) {
            if (state->selected_segment_count > 0U) {
                delete_active_selection(host, state);
            } else if (ensure_selection_mask_allocated(state)) {
                memset(state->selection_mask, 0, state->selection_mask_length);
                bool snapped = false;
                const CutRectangle cross_box = resolve_cross_box_at_point_indexed(
                    &state->design,
                    &state->spatial_index,
                    state->grid,
                    world_x,
                    world_y,
                    &snapped
                );
                (void)select_cross_box_in_mask_indexed(
                    &state->design,
                    &state->spatial_index,
                    cross_box,
                    state->selection_mask,
                    state->selection_mask_length,
                    false
                );
                recount_selected_segments(state);
                if (state->selected_segment_count > 0U) {
                    delete_active_selection(host, state);
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

static void convert_window_to_canvas_point(
    const AppHost *host,
    float window_x,
    float window_y,
    int32_t *canvas_x_out,
    int32_t *canvas_y_out
) {
    float render_x = window_x;
    float render_y = window_y;
    if (host->renderer != nullptr) {
        (void)SDL_RenderCoordinatesFromWindow(host->renderer, window_x, window_y, &render_x, &render_y);
    }
    *canvas_x_out = (int32_t)llround((double)render_x);
    *canvas_y_out = (int32_t)llround((double)render_y);
}

static void handle_mouse_button_down(
    AppHost *host,
    EditorState *state,
    const SDL_MouseButtonEvent *button_event,
    int32_t canvas_width,
    int32_t canvas_height
) {
    int32_t mouse_x = 0;
    int32_t mouse_y = 0;
    convert_window_to_canvas_point(host, button_event->x, button_event->y, &mouse_x, &mouse_y);
    state->mouse_screen_x = mouse_x;
    state->mouse_screen_y = mouse_y;

    const unsigned int key_mod = (unsigned int)SDL_GetModState();
    const bool shift_held = (key_mod & (unsigned int)SDL_KMOD_SHIFT) != 0U;

    if (state->palette_target != PALETTE_TARGET_NONE) {
        if (button_event->button == SDL_BUTTON_LEFT) {
            handle_palette_modal_click(host, state, mouse_x, mouse_y, canvas_width, canvas_height);
        } else if (button_event->button == SDL_BUTTON_RIGHT) {
            state->palette_target = PALETTE_TARGET_NONE;
            state->needs_redraw = true;
        }
        return;
    }

    if (state->strip_modal_open) {
        const StripModalLayout layout = compute_strip_modal_layout(state, canvas_width, canvas_height);
        if (button_event->button == SDL_BUTTON_LEFT) {
            const uint8_t command_id = hit_test_strip_modal_button(&layout, mouse_x, mouse_y);
            if (command_id != STRIP_CMD_NONE) {
                handle_strip_command(host, state, command_id, canvas_width, canvas_height);
                return;
            }
            state->strip_is_painting = true;
            state->strip_is_erasing = false;
            state->strip_stroke_pushed_undo = false;
            paint_strip_cell_at_mouse(state, &layout, mouse_x, mouse_y, false);
        } else if (button_event->button == SDL_BUTTON_RIGHT) {
            state->strip_is_painting = false;
            state->strip_is_erasing = true;
            state->strip_stroke_pushed_undo = false;
            paint_strip_cell_at_mouse(state, &layout, mouse_x, mouse_y, true);
        }
        return;
    }

    const EditorLayout layout = compute_editor_layout(state, canvas_width, canvas_height);
    if (button_event->button == SDL_BUTTON_LEFT) {
        const uint8_t command_id = hit_test_editor_buttons(&layout, mouse_x, mouse_y);
        if (command_id != COMMAND_NONE) {
            execute_toolbar_command(host, state, command_id, canvas_width, canvas_height);
            return;
        }
        if (hit_test_slider_layout(&layout, mouse_x, mouse_y)) {
            state->is_dragging_slider = true;
            update_slider_from_layout(state, &layout, mouse_x);
            return;
        }
    }

    if (handle_color_list_click(host, state, &layout, button_event->button, mouse_x, mouse_y)) {
        return;
    }

    if (!rect_contains_point(layout.viewport, mouse_x, mouse_y)) {
        return;
    }

    if (should_show_start_screen(state) && button_event->button == SDL_BUTTON_LEFT) {
        const StartScreenLayout start_layout = compute_start_screen_layout(state, canvas_width, canvas_height);
        if (point_is_in_box(
                mouse_x,
                mouse_y,
                start_layout.open_button.left_x,
                start_layout.open_button.top_y,
                start_layout.open_button.width,
                start_layout.open_button.height
            )) {
            request_open_with_dirty_guard(host, state);
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

    if (button_event->button == SDL_BUTTON_MIDDLE ||
        (button_event->button == SDL_BUTTON_LEFT && state->space_is_held)) {
        state->is_panning = true;
        state->pan_last_x = mouse_x;
        state->pan_last_y = mouse_y;
        return;
    }

    if (state->has_floating_fragment) {
        if (button_event->button == SDL_BUTTON_LEFT) {
            commit_floating_fragment(host, state, shift_held);
        } else if (button_event->button == SDL_BUTTON_RIGHT) {
            clear_floating_fragment(state);
            set_status_message(state, "Canceled floating fragment");
        }
        return;
    }

    const RenderViewport viewport = {
        .left = layout.viewport.x,
        .top = layout.viewport.y,
        .width = layout.viewport.width,
        .height = layout.viewport.height
    };
    const int32_t world_x = viewport_screen_to_world_x(viewport, state->camera, mouse_x);
    const int32_t world_y = viewport_screen_to_world_y(viewport, state->camera, mouse_y);
    handle_canvas_tool_mouse_down(host, state, button_event->button, world_x, world_y, shift_held);
}

static void handle_mouse_button_up(
    const AppHost *host,
    EditorState *state,
    const SDL_MouseButtonEvent *button_event,
    int32_t canvas_width,
    int32_t canvas_height
) {
    int32_t mouse_x = 0;
    int32_t mouse_y = 0;
    convert_window_to_canvas_point(host, button_event->x, button_event->y, &mouse_x, &mouse_y);

    const unsigned int key_mod = (unsigned int)SDL_GetModState();
    const bool shift_held = (key_mod & (unsigned int)SDL_KMOD_SHIFT) != 0U;

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

    const int32_t world_x = editor_screen_to_world_x(state, canvas_width, canvas_height, mouse_x);
    const int32_t world_y = editor_screen_to_world_y(state, canvas_width, canvas_height, mouse_y);

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
            editor_ensure_spatial_index(state);
            bool snapped = false;
            const CutRectangle cross_box = resolve_cross_box_at_point_indexed(
                &state->design,
                &state->spatial_index,
                state->grid,
                world_x,
                world_y,
                &snapped
            );
            (void)select_cross_box_in_mask_indexed(
                &state->design,
                &state->spatial_index,
                cross_box,
                state->selection_mask,
                state->selection_mask_length,
                shift_held
            );
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
    AppHost *host,
    EditorState *state,
    const SDL_MouseMotionEvent *motion_event,
    int32_t canvas_width,
    int32_t canvas_height
) {
    int32_t mouse_x = 0;
    int32_t mouse_y = 0;
    convert_window_to_canvas_point(host, motion_event->x, motion_event->y, &mouse_x, &mouse_y);
    state->mouse_screen_x = mouse_x;
    state->mouse_screen_y = mouse_y;

    if (state->palette_target != PALETTE_TARGET_NONE) {
        state->needs_redraw = true;
        return;
    }

    if (state->strip_modal_open) {
        state->needs_redraw = true;
        if (state->strip_is_painting || state->strip_is_erasing) {
            const StripModalLayout layout = compute_strip_modal_layout(state, canvas_width, canvas_height);
            paint_strip_cell_at_mouse(state, &layout, mouse_x, mouse_y, state->strip_is_erasing);
        }
        return;
    }

    if (state->is_dragging_slider) {
        const EditorLayout layout = compute_editor_layout(state, canvas_width, canvas_height);
        update_slider_from_layout(state, &layout, mouse_x);
        return;
    }

    if (state->is_panning) {
        const double delta_x = (double)(mouse_x - state->pan_last_x);
        const double delta_y = (double)(mouse_y - state->pan_last_y);
        state->camera.center_x -= delta_x / state->camera.pixels_per_unit;
        state->camera.center_y -= delta_y / state->camera.pixels_per_unit;
        state->pan_last_x = mouse_x;
        state->pan_last_y = mouse_y;
        state->needs_redraw = true;
        return;
    }

    const int32_t world_x = editor_screen_to_world_x(state, canvas_width, canvas_height, mouse_x);
    const int32_t world_y = editor_screen_to_world_y(state, canvas_width, canvas_height, mouse_y);

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
        state->needs_redraw = true;
        return;
    }

    if (state->is_drawing_crosses) {
        apply_draw_x_stroke_at(host, state, world_x, world_y, state->drawing_erases);
        return;
    }

    if (state->is_dragging_box) {
        const bool shift_held = ((unsigned int)SDL_GetModState() & (unsigned int)SDL_KMOD_SHIFT) != 0U;
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
        state->needs_redraw = true;
        return;
    }

    const RenderViewport viewport = get_editor_viewport(state, canvas_width, canvas_height);
    if (!point_is_in_box(mouse_x, mouse_y, viewport.left, viewport.top, viewport.width, viewport.height)) {
        state->needs_redraw = true;
    }
    editor_update_canvas_hover(state, mouse_x, mouse_y, canvas_width, canvas_height);
}

static void handle_mouse_wheel(
    EditorState *state,
    const SDL_MouseWheelEvent *wheel_event,
    int32_t canvas_width,
    int32_t canvas_height
) {
    if (state->palette_target != PALETTE_TARGET_NONE || state->strip_modal_open) {
        return;
    }

    const int32_t scroll_steps = (int32_t)llround((double)wheel_event->y);
    if (scroll_steps == 0) {
        return;
    }

    const EditorLayout layout = compute_editor_layout(state, canvas_width, canvas_height);
    if (layout.right_panel.width > 0 &&
        rect_contains_point(layout.right_panel, state->mouse_screen_x, state->mouse_screen_y)) {
        const int32_t row_height = get_editor_color_row_height(state);
        state->color_scroll_offset -= scroll_steps * row_height;
        const int32_t content_height = (int32_t)state->design.block_count * row_height;
        int32_t maximum_scroll = content_height - layout.colors_list_rect.height;
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

    const int32_t before_world_x = editor_screen_to_world_x(state, canvas_width, canvas_height, state->mouse_screen_x);
    const int32_t before_world_y = editor_screen_to_world_y(state, canvas_width, canvas_height, state->mouse_screen_y);

    if (scroll_steps > 0) {
        state->camera.pixels_per_unit *= 1.18;
    } else {
        state->camera.pixels_per_unit /= 1.18;
    }
    if (state->camera.pixels_per_unit < 0.04) {
        state->camera.pixels_per_unit = 0.04;
    }
    if (state->camera.pixels_per_unit > 28.0) {
        state->camera.pixels_per_unit = 28.0;
    }

    const int32_t after_world_x = editor_screen_to_world_x(state, canvas_width, canvas_height, state->mouse_screen_x);
    const int32_t after_world_y = editor_screen_to_world_y(state, canvas_width, canvas_height, state->mouse_screen_y);
    state->camera.center_x += (double)(before_world_x - after_world_x);
    state->camera.center_y += (double)(before_world_y - after_world_y);
    state->needs_redraw = true;
}

static void handle_key_down(
    AppHost *host,
    EditorState *state,
    const SDL_KeyboardEvent *key_event,
    int32_t canvas_width,
    int32_t canvas_height
) {
    const SDL_Keycode key_code = key_event->key;
    const unsigned int key_mod = (unsigned int)key_event->mod;
    const bool ctrl_held = (key_mod & (unsigned int)SDL_KMOD_CTRL) != 0U;
    const bool shift_held = (key_mod & (unsigned int)SDL_KMOD_SHIFT) != 0U;

    if (state->palette_target != PALETTE_TARGET_NONE) {
        if (key_code == SDLK_ESCAPE) {
            state->palette_target = PALETTE_TARGET_NONE;
            state->needs_redraw = true;
        }
        return;
    }

    if (state->strip_modal_open) {
        if (key_code == SDLK_ESCAPE) {
            state->strip_modal_open = false;
            state->needs_redraw = true;
        } else if (ctrl_held && key_code == SDLK_Z) {
            handle_strip_command(host, state, STRIP_CMD_UNDO, canvas_width, canvas_height);
        } else if (key_code == SDLK_RETURN || key_code == SDLK_KP_ENTER) {
            handle_strip_command(host, state, STRIP_CMD_INSERT, canvas_width, canvas_height);
        }
        return;
    }

    if (key_code == SDLK_SPACE) {
        state->space_is_held = true;
        return;
    }
    if (key_code == SDLK_TAB) {
        execute_toolbar_command(host, state, COMMAND_TOGGLE_RIGHT_PANEL, canvas_width, canvas_height);
        return;
    }
    if (ctrl_held && key_code == SDLK_Z) {
        if (shift_held) {
            execute_toolbar_command(host, state, COMMAND_REDO, canvas_width, canvas_height);
        } else {
            execute_toolbar_command(host, state, COMMAND_UNDO, canvas_width, canvas_height);
        }
        return;
    }
    if (ctrl_held && key_code == SDLK_Y) {
        execute_toolbar_command(host, state, COMMAND_REDO, canvas_width, canvas_height);
        return;
    }
    if (ctrl_held && key_code == SDLK_S) {
        if (shift_held) {
            execute_toolbar_command(host, state, COMMAND_SAVE_AS_DIALOG, canvas_width, canvas_height);
        } else {
            execute_toolbar_command(host, state, COMMAND_SAVE, canvas_width, canvas_height);
        }
        return;
    }
    if (ctrl_held && key_code == SDLK_O) {
        execute_toolbar_command(host, state, COMMAND_OPEN_FILE_DIALOG, canvas_width, canvas_height);
        return;
    }
    if (ctrl_held && key_code == SDLK_M) {
        execute_toolbar_command(host, state, COMMAND_MERGE_FILE_DIALOG, canvas_width, canvas_height);
        return;
    }
    if (ctrl_held && key_code == SDLK_Q) {
        request_quit_with_dirty_guard(host, state);
        return;
    }

    switch (key_code) {
        case SDLK_1:
            execute_toolbar_command(host, state, COMMAND_TOOL_SELECT_ELEMENT, canvas_width, canvas_height);
            break;
        case SDLK_2:
            execute_toolbar_command(host, state, COMMAND_TOOL_SELECT_CROSS, canvas_width, canvas_height);
            break;
        case SDLK_3:
            execute_toolbar_command(host, state, COMMAND_TOOL_DRAW_CROSS, canvas_width, canvas_height);
            break;
        case SDLK_4:
            execute_toolbar_command(host, state, COMMAND_TOOL_CROP, canvas_width, canvas_height);
            break;
        case SDLK_5:
            execute_toolbar_command(host, state, COMMAND_TOOL_CUT, canvas_width, canvas_height);
            break;
        case SDLK_6:
            execute_toolbar_command(host, state, COMMAND_TOOL_SPLIT_V, canvas_width, canvas_height);
            break;
        case SDLK_7:
            execute_toolbar_command(host, state, COMMAND_TOOL_SPLIT_H, canvas_width, canvas_height);
            break;
        case SDLK_8:
            execute_toolbar_command(host, state, COMMAND_TOOL_CLONE, canvas_width, canvas_height);
            break;
        case SDLK_9:
            execute_toolbar_command(host, state, COMMAND_TOOL_PICK, canvas_width, canvas_height);
            break;
        case SDLK_T:
            execute_toolbar_command(host, state, COMMAND_CYCLE_VIEW_MODE, canvas_width, canvas_height);
            break;
        case SDLK_K:
            execute_toolbar_command(host, state, COMMAND_CYCLE_THREAD_THICKNESS, canvas_width, canvas_height);
            break;
        case SDLK_G:
            execute_toolbar_command(host, state, COMMAND_TOGGLE_GRID, canvas_width, canvas_height);
            break;
        case SDLK_J:
            execute_toolbar_command(host, state, COMMAND_TOGGLE_JUMPS, canvas_width, canvas_height);
            break;
        case SDLK_L:
            execute_toolbar_command(host, state, COMMAND_TOGGLE_LOCKS, canvas_width, canvas_height);
            break;
        case SDLK_F:
            execute_toolbar_command(host, state, COMMAND_FIT_VIEW, canvas_width, canvas_height);
            break;
        case SDLK_R:
            execute_toolbar_command(host, state, COMMAND_ROTATE_90, canvas_width, canvas_height);
            break;
        case SDLK_H:
            execute_toolbar_command(host, state, COMMAND_FLIP_X, canvas_width, canvas_height);
            break;
        case SDLK_V:
            execute_toolbar_command(host, state, COMMAND_FLIP_Y, canvas_width, canvas_height);
            break;
        case SDLK_C:
            execute_toolbar_command(host, state, COMMAND_CENTER, canvas_width, canvas_height);
            break;
        case SDLK_P:
            execute_toolbar_command(host, state, COMMAND_CYCLE_PASSES, canvas_width, canvas_height);
            break;
        case SDLK_LEFTBRACKET:
            execute_toolbar_command(host, state, COMMAND_PITCH_DECREASE, canvas_width, canvas_height);
            break;
        case SDLK_RIGHTBRACKET:
            execute_toolbar_command(host, state, COMMAND_PITCH_INCREASE, canvas_width, canvas_height);
            break;
        case SDLK_DELETE:
        case SDLK_BACKSPACE:
            delete_active_selection(host, state);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (state->has_floating_fragment) {
                commit_floating_fragment(host, state, shift_held);
            } else {
                apply_box_edit_or_split(host, state);
            }
            break;
        case SDLK_ESCAPE:
            clear_pending_discard(host);
            if (state->has_floating_fragment) {
                clear_floating_fragment(state);
                set_status_message(state, "Canceled floating fragment");
            } else if (state->has_box_selection) {
                state->has_box_selection = false;
                state->is_dragging_box = false;
                clear_live_preview(state);
                set_status_message(state, "Cleared box selection");
            } else if (state->selected_segment_count > 0U) {
                clear_segment_selection(state);
                set_status_message(state, "Cleared selection");
            }
            break;
        case SDLK_LEFT:
            nudge_active_selection_or_fragment(host, state, -1, 0, shift_held);
            break;
        case SDLK_RIGHT:
            nudge_active_selection_or_fragment(host, state, 1, 0, shift_held);
            break;
        case SDLK_UP:
            nudge_active_selection_or_fragment(host, state, 0, -1, shift_held);
            break;
        case SDLK_DOWN:
            nudge_active_selection_or_fragment(host, state, 0, 1, shift_held);
            break;
        default:
            break;
    }
}

static bool resize_host_surface(AppHost *host, int32_t new_width, int32_t new_height) {
    if (new_width < 320) {
        new_width = 320;
    }
    if (new_height < 240) {
        new_height = 240;
    }
    if (new_width > MAX_CANVAS_DIMENSION) {
        new_width = MAX_CANVAS_DIMENSION;
    }
    if (new_height > MAX_CANVAS_DIMENSION) {
        new_height = MAX_CANVAS_DIMENSION;
    }
    if (host->canvas.pixels != nullptr &&
        host->canvas.width == new_width &&
        host->canvas.height == new_height &&
        host->texture != nullptr) {
        return true;
    }

    PixelCanvas next_canvas = {};
    if (!init_pixel_canvas(&next_canvas, new_width, new_height)) {
        return false;
    }
    SDL_Texture *next_texture = SDL_CreateTexture(
        host->renderer,
        SDL_PIXELFORMAT_XRGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        new_width,
        new_height
    );
    if (next_texture == nullptr) {
        free_pixel_canvas(&next_canvas);
        return false;
    }

    if (host->texture != nullptr) {
        SDL_DestroyTexture(host->texture);
    }
    free_pixel_canvas(&host->canvas);
    host->canvas = next_canvas;
    host->texture = next_texture;
    return true;
}

static void present_frame_to_gpu(AppHost *host, const EditorState *state) {
    if (host->renderer == nullptr || host->texture == nullptr || host->canvas.pixels == nullptr) {
        return;
    }
    const int32_t pitch_bytes = host->canvas.width * (int32_t)sizeof(uint32_t);
    const StitchLayerCache *cache = &state->stitch_cache;
    if (cache->has_dirty_rect &&
        cache->dirty_min_x >= 0 &&
        cache->dirty_min_y >= 0 &&
        cache->dirty_max_x <= host->canvas.width &&
        cache->dirty_max_y <= host->canvas.height &&
        cache->dirty_max_x > cache->dirty_min_x &&
        cache->dirty_max_y > cache->dirty_min_y) {
        const SDL_Rect dirty_rect = {
            .x = cache->dirty_min_x,
            .y = cache->dirty_min_y,
            .w = cache->dirty_max_x - cache->dirty_min_x,
            .h = cache->dirty_max_y - cache->dirty_min_y
        };
        const uint32_t *sub_pixels =
            host->canvas.pixels +
            (size_t)dirty_rect.y * (size_t)host->canvas.width +
            (size_t)dirty_rect.x;
        (void)SDL_UpdateTexture(host->texture, &dirty_rect, sub_pixels, pitch_bytes);
    } else {
        (void)SDL_UpdateTexture(host->texture, nullptr, host->canvas.pixels, pitch_bytes);
    }
    (void)SDL_RenderTexture(host->renderer, host->texture, nullptr, nullptr);
    (void)SDL_RenderPresent(host->renderer);
}

static void sync_host_ui_scale(const AppHost *host, EditorState *state) {
    if (host->window == nullptr) {
        return;
    }
    float display_scale = SDL_GetWindowDisplayScale(host->window);
    if (display_scale <= 0.0f) {
        display_scale = SDL_GetWindowPixelDensity(host->window);
    }
    if (display_scale <= 0.0f) {
        display_scale = 1.0f;
    }
    editor_set_ui_scale(state, (double)display_scale);
}

static void print_usage(FILE *stream) {
    (void)fprintf(stream, "Usage: embroidery-studio [input.{jef,jef+,pes,pec,dst}]\n");
}

int main(int argument_count, char **argument_values) {
    int exit_code = EXIT_FAILURE;
    bool sdl_initialized = false;
    AppHost host = {};
    EditorState state = {};

    if (argument_count > 1 &&
        (strcmp(argument_values[1], "-h") == 0 || strcmp(argument_values[1], "--help") == 0)) {
        print_usage(stdout);
        return EXIT_SUCCESS;
    }

    init_host_dialog_filters(&host);
    init_default_editor_state(&state);

    /* Prefer native Wayland on Linux with seamless fallback to X11. */
    (void)SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        (void)fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        goto cleanup;
    }
    sdl_initialized = true;

    host.dialog_event_type = SDL_RegisterEvents(1);

    const SDL_WindowFlags window_flags =
        (SDL_WindowFlags)((uint64_t)SDL_WINDOW_RESIZABLE | (uint64_t)SDL_WINDOW_HIGH_PIXEL_DENSITY);
    host.window = SDL_CreateWindow(
        "Embroidery Studio",
        WINDOW_INITIAL_WIDTH,
        WINDOW_INITIAL_HEIGHT,
        window_flags
    );
    if (host.window == nullptr) {
        (void)fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    host.renderer = SDL_CreateRenderer(host.window, nullptr);
    if (host.renderer == nullptr) {
        (void)fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        goto cleanup;
    }
    (void)SDL_SetRenderVSync(host.renderer, 1);

    int initial_pixel_width = WINDOW_INITIAL_WIDTH;
    int initial_pixel_height = WINDOW_INITIAL_HEIGHT;
    (void)SDL_GetWindowSizeInPixels(host.window, &initial_pixel_width, &initial_pixel_height);
    sync_host_ui_scale(&host, &state);
    if (!resize_host_surface(&host, (int32_t)initial_pixel_width, (int32_t)initial_pixel_height)) {
        (void)fprintf(stderr, "Failed to create initial SDL3 pixel canvas\n");
        goto cleanup;
    }

    if (argument_count > 1 && argument_values[1][0] != '\0') {
        (void)load_design_into_editor(
            &host,
            &state,
            argument_values[1],
            host.canvas.width,
            host.canvas.height
        );
    }

    while (!state.should_quit) {
        SDL_Event event = {};
        if (SDL_WaitEventTimeout(&event, 16)) {
            do {
                if (host.dialog_event_type != 0U && event.type == host.dialog_event_type) {
                    handle_native_dialog_result(
                        &host,
                        &state,
                        (FileDialogResult *)event.user.data1,
                        host.canvas.width,
                        host.canvas.height
                    );
                    continue;
                }
                switch (event.type) {
                    case SDL_EVENT_QUIT:
                        request_quit_with_dirty_guard(&host, &state);
                        break;
                    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    case SDL_EVENT_WINDOW_RESIZED: {
                        int pixel_width = host.canvas.width;
                        int pixel_height = host.canvas.height;
                        (void)SDL_GetWindowSizeInPixels(host.window, &pixel_width, &pixel_height);
                        sync_host_ui_scale(&host, &state);
                        if (resize_host_surface(&host, (int32_t)pixel_width, (int32_t)pixel_height)) {
                            invalidate_stitch_layer_cache(&state.stitch_cache);
                            state.needs_redraw = true;
                        }
                        break;
                    }
                    case SDL_EVENT_WINDOW_EXPOSED:
                        invalidate_stitch_layer_cache(&state.stitch_cache);
                        state.needs_redraw = true;
                        break;
                    case SDL_EVENT_DROP_FILE:
                        if (event.drop.data != nullptr && event.drop.data[0] != '\0') {
                            if (state.drop_merge_mode && state.design.chain_count > 0U) {
                                (void)load_dropped_file_as_merge_fragment(
                                    &state,
                                    event.drop.data,
                                    host.canvas.width,
                                    host.canvas.height
                                );
                            } else {
                                (void)load_design_into_editor(
                                    &host,
                                    &state,
                                    event.drop.data,
                                    host.canvas.width,
                                    host.canvas.height
                                );
                            }
                        }
                        break;
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                        handle_mouse_button_down(&host, &state, &event.button, host.canvas.width, host.canvas.height);
                        break;
                    case SDL_EVENT_MOUSE_BUTTON_UP:
                        handle_mouse_button_up(&host, &state, &event.button, host.canvas.width, host.canvas.height);
                        break;
                    case SDL_EVENT_MOUSE_MOTION:
                        handle_mouse_motion(&host, &state, &event.motion, host.canvas.width, host.canvas.height);
                        break;
                    case SDL_EVENT_MOUSE_WHEEL:
                        handle_mouse_wheel(&state, &event.wheel, host.canvas.width, host.canvas.height);
                        break;
                    case SDL_EVENT_KEY_DOWN:
                        handle_key_down(&host, &state, &event.key, host.canvas.width, host.canvas.height);
                        break;
                    case SDL_EVENT_KEY_UP:
                        if (event.key.key == SDLK_SPACE) {
                            state.space_is_held = false;
                        }
                        break;
                    default:
                        break;
                }
            } while (!state.should_quit && SDL_PollEvent(&event));
        }

        if (state.needs_redraw) {
            state.needs_redraw = false;
            sync_window_title(&host, &state);
            render_editor_frame(&host.canvas, &state);
            present_frame_to_gpu(&host, &state);
        }
    }

    exit_code = EXIT_SUCCESS;

cleanup:
    free_editor_state(&state);
    free_pixel_canvas(&host.canvas);
    if (host.texture != nullptr) {
        SDL_DestroyTexture(host.texture);
    }
    if (host.renderer != nullptr) {
        SDL_DestroyRenderer(host.renderer);
    }
    if (host.window != nullptr) {
        SDL_DestroyWindow(host.window);
    }
    if (sdl_initialized) {
        SDL_Quit();
    }
    return exit_code;
}
