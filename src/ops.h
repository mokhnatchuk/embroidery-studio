#ifndef EMBROIDERY_OPS_H
#define EMBROIDERY_OPS_H

#include "model.h"

constexpr size_t MAX_HISTORY_STATES = 32U;
constexpr size_t MAX_HISTORY_BYTES = 128U * 1024U * 1024U;

typedef struct {
    StitchDesign states[MAX_HISTORY_STATES];
    size_t state_count;
    size_t current_index;
} EditHistory;

typedef struct {
    int32_t pitch_units;
    size_t cluster_count;
    size_t total_segments;
    int32_t *cluster_center_x;
    int32_t *cluster_center_y;
    uint8_t *cluster_keep_flags;
    int32_t *segment_cluster_id;
    uint64_t built_revision;
    bool valid;
} CrossClusterCache;

CrossGridInfo detect_cross_stitch_grid(const StitchDesign *design);

bool is_cross_diagonal_for_pitch(
    StitchPoint start_point,
    StitchPoint end_point,
    int32_t pitch_units
);
bool is_cross_center_connector(
    StitchPoint start_point,
    StitchPoint end_point,
    int32_t pitch_units
);

void cross_cluster_cache_init(CrossClusterCache *cache);
void cross_cluster_cache_free(CrossClusterCache *cache);
void cross_cluster_cache_invalidate(CrossClusterCache *cache);
bool cross_cluster_cache_build(
    CrossClusterCache *cache,
    const StitchDesign *design,
    int32_t pitch_units,
    ScratchArena *scratch
);

bool build_edit_segment_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    EditOperationKind operation,
    CutRectangle rectangle,
    int32_t split_coordinate,
    bool keep_second_half,
    uint8_t *keep_mask,
    size_t mask_length
);
bool build_edit_segment_mask_cached(
    CrossClusterCache *cache,
    const StitchDesign *design,
    CrossGridInfo grid,
    EditOperationKind operation,
    CutRectangle rectangle,
    int32_t split_coordinate,
    bool keep_second_half,
    uint8_t *keep_mask,
    size_t mask_length,
    ScratchArena *scratch
);
bool append_locked_subchain(
    StitchDesign *destination,
    const StitchPoint *source_points,
    size_t run_start_index,
    size_t run_point_count,
    size_t color_block_index,
    bool cut_at_start,
    bool cut_at_end
);
bool reorder_cutout_chains_by_side(StitchDesign *design, CutRectangle cutout_rectangle);
bool apply_edit_operation(
    const StitchDesign *source,
    CrossGridInfo grid,
    EditOperationKind operation,
    CutRectangle rectangle,
    int32_t split_coordinate,
    bool keep_second_half,
    StitchDesign *result
);

void world_to_grid_cell(
    int32_t world_x,
    int32_t world_y,
    CrossGridInfo grid,
    int32_t *cell_column_out,
    int32_t *cell_row_out
);
CutRectangle resolve_cross_box_at_point(
    const StitchDesign *design,
    CrossGridInfo grid,
    int32_t world_x,
    int32_t world_y,
    bool *snapped_to_stitch_out
);
CutRectangle resolve_cross_box_at_point_indexed(
    const StitchDesign *design,
    const DesignSpatialIndex *index,
    CrossGridInfo grid,
    int32_t world_x,
    int32_t world_y,
    bool *snapped_to_stitch_out
);
size_t select_cross_box_in_mask(
    const StitchDesign *design,
    CutRectangle cross_box,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode
);
size_t select_cross_box_in_mask_indexed(
    const StitchDesign *design,
    const DesignSpatialIndex *index,
    CutRectangle cross_box,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode
);
size_t select_cross_at_point_in_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    int32_t world_x,
    int32_t world_y,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode,
    CutRectangle *resolved_box_out
);
size_t select_crosses_in_rectangle_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    CutRectangle selection_rectangle,
    uint8_t *segment_mask,
    size_t mask_length
);
bool find_nearest_chain_at_point(
    const StitchDesign *design,
    int32_t world_x,
    int32_t world_y,
    int32_t maximum_distance_units,
    size_t *out_chain_index
);
bool find_nearest_chain_at_point_indexed(
    const StitchDesign *design,
    const DesignSpatialIndex *index,
    int32_t world_x,
    int32_t world_y,
    int32_t maximum_distance_units,
    size_t *out_chain_index
);
size_t select_connected_element_in_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    size_t seed_chain_index,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode
);
size_t select_connected_element_in_mask_indexed(
    const StitchDesign *design,
    const DesignSpatialIndex *index,
    size_t seed_chain_index,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode
);

bool delete_masked_segments(
    const StitchDesign *source,
    int32_t cross_pitch_units,
    const uint8_t *segment_mask,
    size_t mask_length,
    StitchDesign *result
);
bool recolor_masked_segments(
    const StitchDesign *source,
    const uint8_t *segment_mask,
    size_t mask_length,
    uint8_t new_janome_index,
    StitchDesign *result
);
bool shift_masked_segments(
    const StitchDesign *source,
    const uint8_t *segment_mask,
    size_t mask_length,
    int32_t delta_x,
    int32_t delta_y,
    StitchDesign *result,
    uint8_t **updated_mask_out,
    size_t *updated_mask_length_out
);

bool move_color_block_order(StitchDesign *design, size_t block_index, int32_t direction);
bool remove_color_block_at(StitchDesign *design, size_t target_block_index);
bool merge_duplicate_color_blocks(StitchDesign *design);

bool draw_single_cross_into_design(
    StitchDesign *design,
    CrossGridInfo grid,
    int32_t cell_column,
    int32_t cell_row,
    size_t color_block_index
);
bool build_strip_design(const StripPattern *pattern, StitchDesign *result);
bool insert_design_fragment(
    const StitchDesign *base_design,
    const StitchDesign *fragment,
    int32_t offset_x,
    int32_t offset_y,
    StitchDesign *result
);

void rotate_design_90_clockwise(StitchDesign *design);
void mirror_design_horizontal(StitchDesign *design);
void mirror_design_vertical(StitchDesign *design);
void shift_design_center(StitchDesign *design, int32_t delta_x, int32_t delta_y);

void history_init(EditHistory *history);
void history_clear(EditHistory *history);
size_t history_estimated_bytes(const EditHistory *history);
bool history_push(EditHistory *history, const StitchDesign *design);
bool history_can_undo(const EditHistory *history);
bool history_can_redo(const EditHistory *history);
bool history_undo(EditHistory *history, StitchDesign *design);
bool history_redo(EditHistory *history, StitchDesign *design);

#endif
