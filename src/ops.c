#include "ops.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int32_t first_x;
    int32_t first_y;
    int32_t second_x;
    int32_t second_y;
    uint32_t hit_count;
    bool is_occupied;
} DiagonalEdgeSlot;

typedef struct {
    int32_t position_x;
    int32_t position_y;
    size_t local_chain_index;
    int32_t next_node_index;
} SpatialVertexNode;

typedef struct {
    uint8_t color_index;
    int64_t sum_center_x;
    int64_t sum_center_y;
    int32_t canonical_center_x;
    int32_t canonical_center_y;
    uint32_t diagonal_count;
    bool keep_cluster;
} CrossCluster;

typedef struct {
    size_t cluster_index;
    int32_t next_node_index;
} CrossClusterNode;

static uint64_t hash_four_int32(int32_t value_a, int32_t value_b, int32_t value_c, int32_t value_d) {
    uint64_t hash_value = 14695981039346656037ULL;
    const uint32_t words[4] = {
        (uint32_t)value_a,
        (uint32_t)value_b,
        (uint32_t)value_c,
        (uint32_t)value_d
    };
    for (size_t word_index = 0U; word_index < 4U; word_index += 1U) {
        hash_value ^= (uint64_t)words[word_index];
        hash_value *= 1099511628211ULL;
    }
    return hash_value;
}

static uint64_t hash_color_and_cell(uint8_t color_index, int32_t cell_x, int32_t cell_y) {
    uint64_t hash_value = 14695981039346656037ULL;
    hash_value ^= (uint64_t)color_index;
    hash_value *= 1099511628211ULL;
    hash_value ^= (uint64_t)(uint32_t)cell_x;
    hash_value *= 1099511628211ULL;
    hash_value ^= (uint64_t)(uint32_t)cell_y;
    hash_value *= 1099511628211ULL;
    return hash_value;
}

static uint64_t hash_two_int32(int32_t cell_x, int32_t cell_y) {
    uint64_t hash_value = 14695981039346656037ULL;
    hash_value ^= (uint64_t)(uint32_t)cell_x;
    hash_value *= 1099511628211ULL;
    hash_value ^= (uint64_t)(uint32_t)cell_y;
    hash_value *= 1099511628211ULL;
    return hash_value;
}

static size_t compute_power_of_two_buckets(size_t target_entries, size_t max_buckets) {
    size_t bucket_count = 1024U;
    size_t desired_buckets = 0U;
    if (ckd_mul(&desired_buckets, target_entries, (size_t)2U)) {
        return max_buckets;
    }
    while (bucket_count < desired_buckets && bucket_count < max_buckets) {
        size_t doubled_buckets = 0U;
        if (ckd_mul(&doubled_buckets, bucket_count, (size_t)2U) || doubled_buckets > max_buckets) {
            return max_buckets;
        }
        bucket_count = doubled_buckets;
    }
    return bucket_count;
}

static void insert_diagonal_edge_hit(
    DiagonalEdgeSlot *table,
    size_t table_mask,
    size_t *occupied_slots,
    size_t maximum_occupied_slots,
    StitchPoint start_point,
    StitchPoint end_point
) {
    int32_t first_x = start_point.position_x;
    int32_t first_y = start_point.position_y;
    int32_t second_x = end_point.position_x;
    int32_t second_y = end_point.position_y;
    if (second_x < first_x || (second_x == first_x && second_y < first_y)) {
        first_x = end_point.position_x;
        first_y = end_point.position_y;
        second_x = start_point.position_x;
        second_y = start_point.position_y;
    }

    size_t slot_index = (size_t)hash_four_int32(first_x, first_y, second_x, second_y) & table_mask;
    for (size_t probe_step = 0U; probe_step <= table_mask; probe_step += 1U) {
        DiagonalEdgeSlot *slot = &table[slot_index];
        if (!slot->is_occupied) {
            if (*occupied_slots >= maximum_occupied_slots) {
                return;
            }
            slot->first_x = first_x;
            slot->first_y = first_y;
            slot->second_x = second_x;
            slot->second_y = second_y;
            slot->hit_count = 1U;
            slot->is_occupied = true;
            *occupied_slots += 1U;
            return;
        }
        if (slot->first_x == first_x && slot->first_y == first_y &&
            slot->second_x == second_x && slot->second_y == second_y) {
            if (slot->hit_count < UINT32_MAX) {
                slot->hit_count += 1U;
            }
            return;
        }
        slot_index = (slot_index + 1U) & table_mask;
    }
}

static bool measure_diagonal_pitch(
    StitchPoint start_point,
    StitchPoint end_point,
    int32_t *pitch_out
) {
    int64_t delta_x = (int64_t)end_point.position_x - (int64_t)start_point.position_x;
    if (delta_x < 0LL) {
        delta_x = -delta_x;
    }
    int64_t delta_y = (int64_t)end_point.position_y - (int64_t)start_point.position_y;
    if (delta_y < 0LL) {
        delta_y = -delta_y;
    }
    int64_t axis_difference = delta_x - delta_y;
    if (axis_difference < 0LL) {
        axis_difference = -axis_difference;
    }
    if (axis_difference > 2LL ||
        delta_x < MIN_CROSS_PITCH_UNITS || delta_x > MAX_CROSS_PITCH_UNITS ||
        delta_y < MIN_CROSS_PITCH_UNITS || delta_y > MAX_CROSS_PITCH_UNITS) {
        return false;
    }
    *pitch_out = (int32_t)((delta_x + delta_y) / 2LL);
    return true;
}

CrossGridInfo detect_cross_stitch_grid(const StitchDesign *design) {
    CrossGridInfo grid = {};
    grid.cross_pitch_units = DEFAULT_CROSS_PITCH_UNITS;
    grid.cross_passes = 3;
    grid.raw_detected_passes = 3;

    DiagonalEdgeSlot *edge_table = nullptr;
    size_t pitch_histogram[MAX_CROSS_PITCH_UNITS + 1] = {};

    size_t diagonal_count = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        for (size_t local_index = 0U; local_index + 1U < chain.point_count; local_index += 1U) {
            int32_t pitch_value = 0;
            if (measure_diagonal_pitch(
                    design->points[chain.point_offset + local_index],
                    design->points[chain.point_offset + local_index + 1U],
                    &pitch_value
                )) {
                pitch_histogram[pitch_value] += 1U;
                diagonal_count += 1U;
            }
        }
    }

    grid.diagonal_stitch_count = diagonal_count;
    int32_t best_pitch = DEFAULT_CROSS_PITCH_UNITS;
    size_t best_pitch_hits = 0U;
    for (int32_t candidate = MIN_CROSS_PITCH_UNITS; candidate <= MAX_CROSS_PITCH_UNITS; candidate += 1) {
        if (pitch_histogram[candidate] > best_pitch_hits) {
            best_pitch_hits = pitch_histogram[candidate];
            best_pitch = candidate;
        }
    }

    if (best_pitch_hits < 8U) {
        goto cleanup;
    }

    grid.cross_pitch_units = best_pitch;
    grid.detected_from_design = true;

    size_t origin_x_histogram[MAX_CROSS_PITCH_UNITS + 1] = {};
    size_t origin_y_histogram[MAX_CROSS_PITCH_UNITS + 1] = {};

    size_t padded_hits = best_pitch_hits;
    if (ckd_add(&padded_hits, best_pitch_hits, (size_t)32U)) {
        padded_hits = 524288U;
    }
    const size_t table_capacity = compute_power_of_two_buckets(padded_hits, 1048576U);
    size_t table_bytes = 0U;
    if (!ckd_mul(&table_bytes, table_capacity, sizeof(DiagonalEdgeSlot))) {
        edge_table = (DiagonalEdgeSlot *)calloc(table_capacity, sizeof(DiagonalEdgeSlot));
    }
    const size_t table_mask = table_capacity - 1U;
    const size_t maximum_occupied_slots = table_capacity / 2U;
    size_t occupied_slots = 0U;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        for (size_t local_index = 0U; local_index + 1U < chain.point_count; local_index += 1U) {
            const StitchPoint start_point = design->points[chain.point_offset + local_index];
            const StitchPoint end_point = design->points[chain.point_offset + local_index + 1U];
            int32_t pitch_value = 0;
            if (!measure_diagonal_pitch(start_point, end_point, &pitch_value)) {
                continue;
            }
            int32_t pitch_error = pitch_value - best_pitch;
            if (pitch_error < 0) {
                pitch_error = -pitch_error;
            }
            if (pitch_error > 1) {
                continue;
            }
            origin_x_histogram[positive_modulo_int32(start_point.position_x, best_pitch)] += 1U;
            origin_y_histogram[positive_modulo_int32(start_point.position_y, best_pitch)] += 1U;
            origin_x_histogram[positive_modulo_int32(end_point.position_x, best_pitch)] += 1U;
            origin_y_histogram[positive_modulo_int32(end_point.position_y, best_pitch)] += 1U;
            if (edge_table != nullptr) {
                insert_diagonal_edge_hit(
                    edge_table,
                    table_mask,
                    &occupied_slots,
                    maximum_occupied_slots,
                    start_point,
                    end_point
                );
            }
        }
    }

    size_t best_origin_x_hits = 0U;
    size_t best_origin_y_hits = 0U;
    for (int32_t phase = 0; phase < best_pitch; phase += 1) {
        if (origin_x_histogram[phase] > best_origin_x_hits) {
            best_origin_x_hits = origin_x_histogram[phase];
            grid.grid_origin_x = phase;
        }
        if (origin_y_histogram[phase] > best_origin_y_hits) {
            best_origin_y_hits = origin_y_histogram[phase];
            grid.grid_origin_y = phase;
        }
    }

    if (edge_table != nullptr) {
        size_t pass_histogram[16] = {};
        for (size_t slot_index = 0U; slot_index < table_capacity; slot_index += 1U) {
            if (!edge_table[slot_index].is_occupied) {
                continue;
            }
            uint32_t count = edge_table[slot_index].hit_count;
            if (count > 15U) {
                count = 15U;
            }
            pass_histogram[count] += 1U;
        }

        int32_t best_passes = 3;
        size_t best_pass_hits = 0U;
        for (int32_t pass_value = 1; pass_value <= 15; pass_value += 1) {
            if (pass_histogram[pass_value] > best_pass_hits) {
                best_pass_hits = pass_histogram[pass_value];
                best_passes = pass_value;
            }
        }
        grid.raw_detected_passes = best_passes;
        if (best_passes < MIN_CROSS_PASSES) {
            grid.cross_passes = MIN_CROSS_PASSES;
        } else if (best_passes > MAX_CROSS_PASSES) {
            grid.cross_passes = MAX_CROSS_PASSES;
        } else {
            grid.cross_passes = best_passes;
        }
    }

cleanup:
    free(edge_table);
    return grid;
}

static bool point_kept_by_edit(
    EditOperationKind operation,
    int32_t point_x,
    int32_t point_y,
    CutRectangle rectangle,
    int32_t split_coordinate,
    bool keep_second_half
) {
    if (operation == EDIT_CROP_KEEP) {
        return point_is_strictly_inside_rectangle(point_x, point_y, rectangle);
    }
    if (operation == EDIT_CUT_OUT) {
        return !point_is_strictly_inside_rectangle(point_x, point_y, rectangle);
    }
    if (operation == EDIT_SPLIT_VERTICAL) {
        if (keep_second_half) {
            return point_x >= split_coordinate;
        }
        return point_x < split_coordinate;
    }
    if (keep_second_half) {
        return point_y >= split_coordinate;
    }
    return point_y < split_coordinate;
}

/* Pure integer squared-length check equivalent to 0.80 <= len / (pitch * sqrt(2)) <= 1.20. */
bool is_cross_diagonal_for_pitch(
    StitchPoint start_point,
    StitchPoint end_point,
    int32_t pitch_units
) {
    if (pitch_units < MIN_CROSS_PITCH_UNITS) {
        pitch_units = DEFAULT_CROSS_PITCH_UNITS;
    }
    if (pitch_units > MAX_CROSS_PITCH_UNITS) {
        pitch_units = MAX_CROSS_PITCH_UNITS;
    }

    int64_t delta_x = (int64_t)end_point.position_x - (int64_t)start_point.position_x;
    if (delta_x < 0LL) {
        delta_x = -delta_x;
    }
    int64_t delta_y = (int64_t)end_point.position_y - (int64_t)start_point.position_y;
    if (delta_y < 0LL) {
        delta_y = -delta_y;
    }
    int64_t axis_difference = delta_x - delta_y;
    if (axis_difference < 0LL) {
        axis_difference = -axis_difference;
    }
    const int64_t max_axis_span = (int64_t)pitch_units * 2LL;
    if (axis_difference > 3LL || delta_x > max_axis_span || delta_y > max_axis_span) {
        return false;
    }

    const int64_t length_squared = delta_x * delta_x + delta_y * delta_y;
    const int64_t expected_squared = 2LL * (int64_t)pitch_units * (int64_t)pitch_units;
    return length_squared * 100LL >= expected_squared * 64LL &&
           length_squared * 100LL <= expected_squared * 144LL;
}

bool is_cross_center_connector(
    StitchPoint start_point,
    StitchPoint end_point,
    int32_t pitch_units
) {
    if (pitch_units < MIN_CROSS_PITCH_UNITS) {
        pitch_units = DEFAULT_CROSS_PITCH_UNITS;
    }
    if (pitch_units > MAX_CROSS_PITCH_UNITS) {
        pitch_units = MAX_CROSS_PITCH_UNITS;
    }

    int64_t delta_x = (int64_t)end_point.position_x - (int64_t)start_point.position_x;
    if (delta_x < 0LL) {
        delta_x = -delta_x;
    }
    int64_t delta_y = (int64_t)end_point.position_y - (int64_t)start_point.position_y;
    if (delta_y < 0LL) {
        delta_y = -delta_y;
    }
    const int64_t max_axis_span = (int64_t)pitch_units * 2LL;
    if (delta_x > max_axis_span || delta_y > max_axis_span) {
        return false;
    }

    int64_t error_x = delta_x * 2LL - (int64_t)pitch_units;
    if (error_x < 0LL) {
        error_x = -error_x;
    }
    int64_t error_y = delta_y * 2LL - (int64_t)pitch_units;
    if (error_y < 0LL) {
        error_y = -error_y;
    }
    return error_x <= 6LL && error_y <= 6LL;
}

static bool run_is_cut_artifact(
    const StitchPoint *chain_points,
    size_t run_start_segment,
    size_t run_segment_count,
    int32_t pitch_units
) {
    if (run_segment_count == 1U) {
        return !is_cross_diagonal_for_pitch(
            chain_points[run_start_segment],
            chain_points[run_start_segment + 1U],
            pitch_units
        );
    }
    if (run_segment_count <= 2U) {
        for (size_t offset = 0U; offset < run_segment_count; offset += 1U) {
            const StitchPoint first = chain_points[run_start_segment + offset];
            const StitchPoint second = chain_points[run_start_segment + offset + 1U];
            int64_t delta_x = (int64_t)second.position_x - (int64_t)first.position_x;
            if (delta_x < 0LL) {
                delta_x = -delta_x;
            }
            int64_t delta_y = (int64_t)second.position_y - (int64_t)first.position_y;
            if (delta_y < 0LL) {
                delta_y = -delta_y;
            }
            if (delta_x > 5LL || delta_y > 5LL) {
                return false;
            }
        }
        return true;
    }
    return false;
}

static size_t lookup_cross_cluster(
    uint8_t color_index,
    int32_t target_x,
    int32_t target_y,
    size_t bucket_mask,
    const int32_t *bucket_heads,
    const CrossClusterNode *nodes,
    const CrossCluster *clusters,
    size_t cluster_capacity
) {
    constexpr int32_t CLUSTER_TOLERANCE_UNITS = 3;
    const int32_t base_cell_x = floor_divide_int32(target_x, CLUSTER_TOLERANCE_UNITS);
    const int32_t base_cell_y = floor_divide_int32(target_y, CLUSTER_TOLERANCE_UNITS);

    for (int32_t neighbor_offset = 0; neighbor_offset < 9; neighbor_offset += 1) {
        const int32_t offset_x = (neighbor_offset % 3) - 1;
        const int32_t offset_y = (neighbor_offset / 3) - 1;
        const size_t bucket_index =
            (size_t)hash_color_and_cell(color_index, base_cell_x + offset_x, base_cell_y + offset_y) &
            bucket_mask;

        int32_t node_index = bucket_heads[bucket_index];
        while (node_index >= 0 && (size_t)node_index < cluster_capacity) {
            const CrossClusterNode node = nodes[node_index];
            node_index = node.next_node_index;
            if (node.cluster_index >= cluster_capacity) {
                continue;
            }
            const CrossCluster cluster = clusters[node.cluster_index];
            if (cluster.color_index != color_index) {
                continue;
            }
            int32_t diff_x = target_x - cluster.canonical_center_x;
            if (diff_x < 0) {
                diff_x = -diff_x;
            }
            int32_t diff_y = target_y - cluster.canonical_center_y;
            if (diff_y < 0) {
                diff_y = -diff_y;
            }
            if (diff_x <= CLUSTER_TOLERANCE_UNITS && diff_y <= CLUSTER_TOLERANCE_UNITS) {
                return node.cluster_index;
            }
        }
    }
    return SIZE_MAX;
}

static bool evaluate_segment_keep_with_clusters(
    StitchPoint start_point,
    StitchPoint end_point,
    uint8_t color_index,
    int32_t pitch,
    EditOperationKind operation,
    CutRectangle rectangle,
    int32_t split_coordinate,
    bool keep_second_half,
    size_t cluster_count,
    size_t cluster_capacity,
    size_t bucket_mask,
    const int32_t *bucket_heads,
    const CrossClusterNode *cluster_nodes,
    const CrossCluster *clusters
) {
    const int32_t midpoint_x =
        (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
    const int32_t midpoint_y =
        (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);

    if (cluster_count > 0U && is_cross_diagonal_for_pitch(start_point, end_point, pitch)) {
        const size_t cluster_index = lookup_cross_cluster(
            color_index,
            midpoint_x,
            midpoint_y,
            bucket_mask,
            bucket_heads,
            cluster_nodes,
            clusters,
            cluster_capacity
        );
        if (cluster_index != SIZE_MAX) {
            return clusters[cluster_index].keep_cluster;
        }
    }

    if (cluster_count > 0U && is_cross_center_connector(start_point, end_point, pitch)) {
        size_t cluster_index = lookup_cross_cluster(
            color_index,
            start_point.position_x,
            start_point.position_y,
            bucket_mask,
            bucket_heads,
            cluster_nodes,
            clusters,
            cluster_capacity
        );
        if (cluster_index == SIZE_MAX) {
            cluster_index = lookup_cross_cluster(
                color_index,
                end_point.position_x,
                end_point.position_y,
                bucket_mask,
                bucket_heads,
                cluster_nodes,
                clusters,
                cluster_capacity
            );
        }
        if (cluster_index != SIZE_MAX) {
            return clusters[cluster_index].keep_cluster;
        }
    }

    return point_kept_by_edit(
        operation,
        midpoint_x,
        midpoint_y,
        rectangle,
        split_coordinate,
        keep_second_half
    );
}

static void insert_chain_diagonals_into_clusters(
    const StitchPoint *chain_points,
    size_t point_count,
    uint8_t color_index,
    int32_t pitch,
    size_t bucket_mask,
    int32_t *bucket_heads,
    CrossClusterNode *cluster_nodes,
    CrossCluster *clusters,
    size_t cluster_capacity,
    size_t *cluster_count
) {
    constexpr int32_t CLUSTER_TOLERANCE_UNITS = 3;
    for (size_t local_index = 0U; local_index + 1U < point_count; local_index += 1U) {
        const StitchPoint start_point = chain_points[local_index];
        const StitchPoint end_point = chain_points[local_index + 1U];
        if (!is_cross_diagonal_for_pitch(start_point, end_point, pitch)) {
            continue;
        }
        const int32_t midpoint_x =
            (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
        const int32_t midpoint_y =
            (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);

        const size_t existing_index = lookup_cross_cluster(
            color_index,
            midpoint_x,
            midpoint_y,
            bucket_mask,
            bucket_heads,
            cluster_nodes,
            clusters,
            cluster_capacity
        );
        if (existing_index != SIZE_MAX) {
            clusters[existing_index].sum_center_x += (int64_t)midpoint_x;
            clusters[existing_index].sum_center_y += (int64_t)midpoint_y;
            clusters[existing_index].diagonal_count += 1U;
            clusters[existing_index].canonical_center_x =
                (int32_t)(clusters[existing_index].sum_center_x /
                          (int64_t)clusters[existing_index].diagonal_count);
            clusters[existing_index].canonical_center_y =
                (int32_t)(clusters[existing_index].sum_center_y /
                          (int64_t)clusters[existing_index].diagonal_count);
            continue;
        }

        const size_t new_cluster_index = *cluster_count;
        if (new_cluster_index >= cluster_capacity) {
            return;
        }
        clusters[new_cluster_index] = (CrossCluster){
            .color_index = color_index,
            .sum_center_x = (int64_t)midpoint_x,
            .sum_center_y = (int64_t)midpoint_y,
            .canonical_center_x = midpoint_x,
            .canonical_center_y = midpoint_y,
            .diagonal_count = 1U,
            .keep_cluster = false
        };
        const int32_t cell_x = floor_divide_int32(midpoint_x, CLUSTER_TOLERANCE_UNITS);
        const int32_t cell_y = floor_divide_int32(midpoint_y, CLUSTER_TOLERANCE_UNITS);
        const size_t bucket_index =
            (size_t)hash_color_and_cell(color_index, cell_x, cell_y) & bucket_mask;
        cluster_nodes[new_cluster_index].cluster_index = new_cluster_index;
        cluster_nodes[new_cluster_index].next_node_index = bucket_heads[bucket_index];
        bucket_heads[bucket_index] = (int32_t)new_cluster_index;
        *cluster_count += 1U;
    }
}

void cross_cluster_cache_init(CrossClusterCache *cache) {
    *cache = (CrossClusterCache){};
}

void cross_cluster_cache_free(CrossClusterCache *cache) {
    free(cache->cluster_center_x);
    free(cache->cluster_center_y);
    free(cache->cluster_keep_flags);
    free(cache->segment_cluster_id);
    *cache = (CrossClusterCache){};
}

void cross_cluster_cache_invalidate(CrossClusterCache *cache) {
    cache->valid = false;
}

bool cross_cluster_cache_build(
    CrossClusterCache *cache,
    const StitchDesign *design,
    int32_t pitch_units,
    ScratchArena *scratch
) {
    bool succeeded = false;
    ScratchArena local_scratch = {};
    ScratchArena *active_scratch = scratch;
    if (active_scratch == nullptr) {
        scratch_arena_init(&local_scratch);
        active_scratch = &local_scratch;
    }

    cross_cluster_cache_free(cache);

    int32_t pitch = pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }
    cache->pitch_units = pitch;
    cache->built_revision = design->geometry_revision;

    size_t total_segments = 0U;
    size_t diagonal_count = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t chain_segments = chain.point_count - 1U;
        total_segments += chain_segments;
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled) {
            continue;
        }
        const StitchPoint *chain_points = &design->points[chain.point_offset];
        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (is_cross_diagonal_for_pitch(chain_points[local_index], chain_points[local_index + 1U], pitch)) {
                diagonal_count += 1U;
            }
        }
    }

    cache->total_segments = total_segments;
    if (total_segments == 0U) {
        cache->valid = true;
        succeeded = true;
        goto cleanup;
    }

    size_t seg_id_bytes = 0U;
    if (ckd_mul(&seg_id_bytes, total_segments, sizeof(int32_t))) {
        goto cleanup;
    }
    cache->segment_cluster_id = (int32_t *)malloc(seg_id_bytes);
    if (cache->segment_cluster_id == nullptr) {
        goto cleanup;
    }
    for (size_t segment_index = 0U; segment_index < total_segments; segment_index += 1U) {
        cache->segment_cluster_id[segment_index] = -1;
    }

    if (diagonal_count == 0U) {
        cache->valid = true;
        succeeded = true;
        goto cleanup;
    }

    const size_t bucket_count = compute_power_of_two_buckets(diagonal_count, 1048576U);
    const size_t bucket_mask = bucket_count - 1U;
    size_t scratch_bytes = 0U;
    if (!scratch_arena_add_slice_bytes(&scratch_bytes, diagonal_count, sizeof(CrossCluster)) ||
        !scratch_arena_add_slice_bytes(&scratch_bytes, diagonal_count, sizeof(CrossClusterNode)) ||
        !scratch_arena_add_slice_bytes(&scratch_bytes, bucket_count, sizeof(int32_t)) ||
        !scratch_arena_prepare(active_scratch, scratch_bytes)) {
        goto cleanup;
    }

    CrossCluster *clusters =
        (CrossCluster *)scratch_arena_push(active_scratch, diagonal_count, sizeof(CrossCluster));
    CrossClusterNode *cluster_nodes =
        (CrossClusterNode *)scratch_arena_push(active_scratch, diagonal_count, sizeof(CrossClusterNode));
    int32_t *bucket_heads =
        (int32_t *)scratch_arena_push(active_scratch, bucket_count, sizeof(int32_t));
    if (clusters == nullptr || cluster_nodes == nullptr || bucket_heads == nullptr) {
        goto cleanup;
    }
    memset(clusters, 0, diagonal_count * sizeof(CrossCluster));
    memset(cluster_nodes, 0, diagonal_count * sizeof(CrossClusterNode));
    for (size_t bucket_index = 0U; bucket_index < bucket_count; bucket_index += 1U) {
        bucket_heads[bucket_index] = -1;
    }

    size_t cluster_count = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled ||
            chain.point_count < 2U) {
            continue;
        }
        insert_chain_diagonals_into_clusters(
            &design->points[chain.point_offset],
            chain.point_count,
            design->blocks[chain.color_block_index].janome_color_index,
            pitch,
            bucket_mask,
            bucket_heads,
            cluster_nodes,
            clusters,
            diagonal_count,
            &cluster_count
        );
    }

    if (cluster_count > 0U) {
        size_t coord_bytes = 0U;
        if (ckd_mul(&coord_bytes, cluster_count, sizeof(int32_t))) {
            goto cleanup;
        }
        cache->cluster_center_x = (int32_t *)malloc(coord_bytes);
        cache->cluster_center_y = (int32_t *)malloc(coord_bytes);
        cache->cluster_keep_flags = (uint8_t *)calloc(cluster_count, sizeof(uint8_t));
        if (cache->cluster_center_x == nullptr ||
            cache->cluster_center_y == nullptr ||
            cache->cluster_keep_flags == nullptr) {
            goto cleanup;
        }
        for (size_t cluster_index = 0U; cluster_index < cluster_count; cluster_index += 1U) {
            cache->cluster_center_x[cluster_index] = clusters[cluster_index].canonical_center_x;
            cache->cluster_center_y[cluster_index] = clusters[cluster_index].canonical_center_y;
        }
        cache->cluster_count = cluster_count;

        size_t flat_segment = 0U;
        for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
            const StitchChain chain = design->chains[chain_index];
            if (chain.point_count < 2U) {
                continue;
            }
            const size_t chain_segments = chain.point_count - 1U;
            if (chain.color_block_index >= design->block_count ||
                !design->blocks[chain.color_block_index].enabled) {
                flat_segment += chain_segments;
                continue;
            }
            const uint8_t color_index = design->blocks[chain.color_block_index].janome_color_index;
            const StitchPoint *chain_points = &design->points[chain.point_offset];

            for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
                const StitchPoint start_point = chain_points[local_index];
                const StitchPoint end_point = chain_points[local_index + 1U];
                size_t found_cluster = SIZE_MAX;

                if (is_cross_diagonal_for_pitch(start_point, end_point, pitch)) {
                    const int32_t mid_x =
                        (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
                    const int32_t mid_y =
                        (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);
                    found_cluster = lookup_cross_cluster(
                        color_index,
                        mid_x,
                        mid_y,
                        bucket_mask,
                        bucket_heads,
                        cluster_nodes,
                        clusters,
                        diagonal_count
                    );
                } else if (is_cross_center_connector(start_point, end_point, pitch)) {
                    found_cluster = lookup_cross_cluster(
                        color_index,
                        start_point.position_x,
                        start_point.position_y,
                        bucket_mask,
                        bucket_heads,
                        cluster_nodes,
                        clusters,
                        diagonal_count
                    );
                    if (found_cluster == SIZE_MAX) {
                        found_cluster = lookup_cross_cluster(
                            color_index,
                            end_point.position_x,
                            end_point.position_y,
                            bucket_mask,
                            bucket_heads,
                            cluster_nodes,
                            clusters,
                            diagonal_count
                        );
                    }
                }
                if (found_cluster != SIZE_MAX && flat_segment + local_index < total_segments) {
                    cache->segment_cluster_id[flat_segment + local_index] = (int32_t)found_cluster;
                }
            }
            flat_segment += chain_segments;
        }
    }

    cache->valid = true;
    succeeded = true;

cleanup:
    if (!succeeded) {
        cross_cluster_cache_free(cache);
    }
    scratch_arena_free(&local_scratch);
    return succeeded;
}

/* Zero-alloc drag preview using cached cross clusters (exact match to build_edit_segment_mask). */
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
) {
    if (mask_length == 0U || keep_mask == nullptr) {
        return true;
    }
    if (cache == nullptr) {
        return build_edit_segment_mask(
            design,
            grid,
            operation,
            rectangle,
            split_coordinate,
            keep_second_half,
            keep_mask,
            mask_length
        );
    }

    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }
    if (!cache->valid || cache->built_revision != design->geometry_revision || cache->pitch_units != pitch) {
        if (!cross_cluster_cache_build(cache, design, pitch, scratch)) {
            return false;
        }
    }

    if (rectangle.minimum_x > rectangle.maximum_x) {
        const int32_t swap_x = rectangle.minimum_x;
        rectangle.minimum_x = rectangle.maximum_x;
        rectangle.maximum_x = swap_x;
    }
    if (rectangle.minimum_y > rectangle.maximum_y) {
        const int32_t swap_y = rectangle.minimum_y;
        rectangle.minimum_y = rectangle.maximum_y;
        rectangle.maximum_y = swap_y;
    }

    const int32_t *cluster_x = cache->cluster_center_x;
    const int32_t *cluster_y = cache->cluster_center_y;
    uint8_t *cluster_keep = cache->cluster_keep_flags;
    const size_t cluster_count = cache->cluster_count;

    for (size_t cluster_index = 0U; cluster_index < cluster_count; cluster_index += 1U) {
        cluster_keep[cluster_index] = (uint8_t)point_kept_by_edit(
            operation,
            cluster_x[cluster_index],
            cluster_y[cluster_index],
            rectangle,
            split_coordinate,
            keep_second_half
        );
    }

    const int32_t *segment_clusters = cache->segment_cluster_id;
    const size_t cached_total = cache->total_segments;
    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        if (flat_segment_index >= mask_length) {
            break;
        }
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t chain_segments = chain.point_count - 1U;
        size_t writable_segments = chain_segments;
        if (flat_segment_index + writable_segments > mask_length) {
            writable_segments = mask_length - flat_segment_index;
        }
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled) {
            memset(keep_mask + flat_segment_index, 0, writable_segments);
            flat_segment_index += chain_segments;
            continue;
        }

        const StitchPoint *chain_points = &design->points[chain.point_offset];
        uint8_t *chain_mask = keep_mask + flat_segment_index;
        bool any_dropped = false;
        bool any_kept = false;

        for (size_t local_index = 0U; local_index < writable_segments; local_index += 1U) {
            const size_t current_flat = flat_segment_index + local_index;
            uint8_t keep_flag = 0U;
            if (current_flat < cached_total &&
                segment_clusters != nullptr &&
                segment_clusters[current_flat] >= 0) {
                const size_t cluster_index = (size_t)segment_clusters[current_flat];
                if (cluster_index < cluster_count) {
                    keep_flag = cluster_keep[cluster_index];
                }
            } else {
                const StitchPoint start_point = chain_points[local_index];
                const StitchPoint end_point = chain_points[local_index + 1U];
                const int32_t mid_x =
                    (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
                const int32_t mid_y =
                    (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);
                keep_flag = (uint8_t)point_kept_by_edit(
                    operation,
                    mid_x,
                    mid_y,
                    rectangle,
                    split_coordinate,
                    keep_second_half
                );
            }
            chain_mask[local_index] = keep_flag;
            if (keep_flag != 0U) {
                any_kept = true;
            } else {
                any_dropped = true;
            }
        }

        /* Only scan for cut artifacts on chains actually split by the edit boundary. */
        if (any_kept && any_dropped) {
            size_t local_segment = 0U;
            while (local_segment < writable_segments) {
                while (local_segment < writable_segments && chain_mask[local_segment] == 0U) {
                    local_segment += 1U;
                }
                if (local_segment >= writable_segments) {
                    break;
                }
                const size_t run_start = local_segment;
                while (local_segment < writable_segments && chain_mask[local_segment] != 0U) {
                    local_segment += 1U;
                }
                const size_t run_length = local_segment - run_start;
                const bool run_was_cut = (run_start > 0U || local_segment < chain_segments);
                if (run_length > 0U && run_was_cut &&
                    run_is_cut_artifact(chain_points, run_start, run_length, pitch)) {
                    memset(chain_mask + run_start, 0, run_length);
                }
            }
        }

        flat_segment_index += chain_segments;
    }

    if (flat_segment_index < mask_length) {
        memset(keep_mask + flat_segment_index, 0, mask_length - flat_segment_index);
    }

    return true;
}

/* Unified keep-mask builder used identically by both live preview and edit commit. */
bool build_edit_segment_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    EditOperationKind operation,
    CutRectangle rectangle,
    int32_t split_coordinate,
    bool keep_second_half,
    uint8_t *keep_mask,
    size_t mask_length
) {
    bool succeeded = false;
    CrossCluster *clusters = nullptr;
    CrossClusterNode *cluster_nodes = nullptr;
    int32_t *bucket_heads = nullptr;

    if (mask_length == 0U || keep_mask == nullptr) {
        return true;
    }
    memset(keep_mask, 0, mask_length);

    if (rectangle.minimum_x > rectangle.maximum_x) {
        const int32_t swap_x = rectangle.minimum_x;
        rectangle.minimum_x = rectangle.maximum_x;
        rectangle.maximum_x = swap_x;
    }
    if (rectangle.minimum_y > rectangle.maximum_y) {
        const int32_t swap_y = rectangle.minimum_y;
        rectangle.minimum_y = rectangle.maximum_y;
        rectangle.maximum_y = swap_y;
    }

    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }

    size_t diagonal_count = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled ||
            chain.point_count < 2U) {
            continue;
        }
        const StitchPoint *chain_points = &design->points[chain.point_offset];
        for (size_t local_index = 0U; local_index + 1U < chain.point_count; local_index += 1U) {
            if (is_cross_diagonal_for_pitch(chain_points[local_index], chain_points[local_index + 1U], pitch)) {
                diagonal_count += 1U;
            }
        }
    }

    size_t cluster_count = 0U;
    size_t bucket_mask = 0U;

    if (diagonal_count > 0U) {
        const size_t bucket_count = compute_power_of_two_buckets(diagonal_count, 1048576U);
        bucket_mask = bucket_count - 1U;

        size_t cluster_bytes = 0U;
        size_t node_bytes = 0U;
        size_t bucket_bytes = 0U;
        if (ckd_mul(&cluster_bytes, diagonal_count, sizeof(CrossCluster)) ||
            ckd_mul(&node_bytes, diagonal_count, sizeof(CrossClusterNode)) ||
            ckd_mul(&bucket_bytes, bucket_count, sizeof(int32_t))) {
            goto cleanup;
        }

        clusters = (CrossCluster *)calloc(diagonal_count, sizeof(CrossCluster));
        cluster_nodes = (CrossClusterNode *)calloc(diagonal_count, sizeof(CrossClusterNode));
        bucket_heads = (int32_t *)malloc(bucket_bytes);
        if (clusters == nullptr || cluster_nodes == nullptr || bucket_heads == nullptr) {
            (void)fprintf(stderr, "Failed to allocate cross cluster buffers\n");
            goto cleanup;
        }
        for (size_t bucket_index = 0U; bucket_index < bucket_count; bucket_index += 1U) {
            bucket_heads[bucket_index] = -1;
        }

        for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
            const StitchChain chain = design->chains[chain_index];
            if (chain.color_block_index >= design->block_count ||
                !design->blocks[chain.color_block_index].enabled ||
                chain.point_count < 2U) {
                continue;
            }
            insert_chain_diagonals_into_clusters(
                &design->points[chain.point_offset],
                chain.point_count,
                design->blocks[chain.color_block_index].janome_color_index,
                pitch,
                bucket_mask,
                bucket_heads,
                cluster_nodes,
                clusters,
                diagonal_count,
                &cluster_count
            );
        }

        for (size_t cluster_index = 0U; cluster_index < cluster_count; cluster_index += 1U) {
            clusters[cluster_index].keep_cluster = point_kept_by_edit(
                operation,
                clusters[cluster_index].canonical_center_x,
                clusters[cluster_index].canonical_center_y,
                rectangle,
                split_coordinate,
                keep_second_half
            );
        }
    }

    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        if (flat_segment_index >= mask_length) {
            break;
        }
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

        const uint8_t color_index = design->blocks[chain.color_block_index].janome_color_index;
        const StitchPoint *chain_points = &design->points[chain.point_offset];

        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (flat_segment_index + local_index >= mask_length) {
                break;
            }
            if (evaluate_segment_keep_with_clusters(
                    chain_points[local_index],
                    chain_points[local_index + 1U],
                    color_index,
                    pitch,
                    operation,
                    rectangle,
                    split_coordinate,
                    keep_second_half,
                    cluster_count,
                    diagonal_count,
                    bucket_mask,
                    bucket_heads,
                    cluster_nodes,
                    clusters
                )) {
                keep_mask[flat_segment_index + local_index] = 1U;
            }
        }

        /* Remove only isolated cut artifacts without touching whole crosses or external stitches. */
        size_t local_segment = 0U;
        while (local_segment < chain_segments &&
               flat_segment_index + local_segment < mask_length) {
            while (local_segment < chain_segments &&
                   flat_segment_index + local_segment < mask_length &&
                   keep_mask[flat_segment_index + local_segment] == 0U) {
                local_segment += 1U;
            }
            if (local_segment >= chain_segments ||
                flat_segment_index + local_segment >= mask_length) {
                break;
            }
            const size_t run_start = local_segment;
            while (local_segment < chain_segments &&
                   flat_segment_index + local_segment < mask_length &&
                   keep_mask[flat_segment_index + local_segment] != 0U) {
                local_segment += 1U;
            }
            const size_t run_length = local_segment - run_start;
            const bool run_was_cut = (run_start > 0U || local_segment < chain_segments);
            if (run_length > 0U && run_was_cut &&
                run_is_cut_artifact(chain_points, run_start, run_length, pitch)) {
                memset(keep_mask + flat_segment_index + run_start, 0, run_length);
            }
        }

        flat_segment_index += chain_segments;
    }

    succeeded = true;

cleanup:
    free(bucket_heads);
    free(cluster_nodes);
    free(clusters);
    return succeeded;
}

static void compute_lock_step(
    StitchPoint anchor_point,
    StitchPoint neighbor_point,
    int32_t *step_x_out,
    int32_t *step_y_out
) {
    const double delta_x =
        (double)((int64_t)neighbor_point.position_x - (int64_t)anchor_point.position_x);
    const double delta_y =
        (double)((int64_t)neighbor_point.position_y - (int64_t)anchor_point.position_y);
    const double segment_length = sqrt(delta_x * delta_x + delta_y * delta_y);

    if (segment_length < 0.001) {
        *step_x_out = LOCK_STITCH_UNITS;
        *step_y_out = 0;
        return;
    }

    const double scale = (double)LOCK_STITCH_UNITS / segment_length;
    int32_t step_x = (int32_t)llround(delta_x * scale);
    int32_t step_y = (int32_t)llround(delta_y * scale);
    if (step_x == 0 && step_y == 0) {
        if (fabs(delta_x) >= fabs(delta_y)) {
            if (delta_x < 0.0) {
                step_x = -LOCK_STITCH_UNITS;
            } else {
                step_x = LOCK_STITCH_UNITS;
            }
        } else {
            if (delta_y < 0.0) {
                step_y = -LOCK_STITCH_UNITS;
            } else {
                step_y = LOCK_STITCH_UNITS;
            }
        }
    }
    *step_x_out = step_x;
    *step_y_out = step_y;
}

bool append_locked_subchain(
    StitchDesign *destination,
    const StitchPoint *source_points,
    size_t run_start_index,
    size_t run_point_count,
    size_t color_block_index,
    bool cut_at_start,
    bool cut_at_end
) {
    if (destination == nullptr || source_points == nullptr || run_point_count < 2U) {
        return false;
    }

    const size_t output_point_offset = destination->point_count;
    size_t extra_lock_points = 0U;
    if (cut_at_start) {
        extra_lock_points += 3U;
    }
    if (cut_at_end) {
        extra_lock_points += 3U;
    }

    size_t required_points = 0U;
    if (ckd_add(&required_points, destination->point_count, run_point_count) ||
        ckd_add(&required_points, required_points, extra_lock_points) ||
        !design_reserve_points(destination, required_points)) {
        return false;
    }

    if (cut_at_start) {
        const StitchPoint anchor_point = source_points[run_start_index];
        StitchPoint neighbor_point = source_points[run_start_index + 1U];
        for (size_t offset_index = 1U; offset_index < run_point_count; offset_index += 1U) {
            const StitchPoint candidate = source_points[run_start_index + offset_index];
            if (candidate.position_x != anchor_point.position_x ||
                candidate.position_y != anchor_point.position_y) {
                neighbor_point = candidate;
                break;
            }
        }

        int32_t lock_step_x = 0;
        int32_t lock_step_y = 0;
        compute_lock_step(anchor_point, neighbor_point, &lock_step_x, &lock_step_y);
        const int32_t forward_x = anchor_point.position_x + lock_step_x;
        const int32_t forward_y = anchor_point.position_y + lock_step_y;

        if (!design_append_point(destination, forward_x, forward_y) ||
            !design_append_point(destination, anchor_point.position_x, anchor_point.position_y) ||
            !design_append_point(destination, forward_x, forward_y)) {
            return false;
        }
    }

    for (size_t local_index = 0U; local_index < run_point_count; local_index += 1U) {
        const StitchPoint current_point = source_points[run_start_index + local_index];
        if (!design_append_point(destination, current_point.position_x, current_point.position_y)) {
            return false;
        }
    }

    if (cut_at_end) {
        const StitchPoint anchor_point = source_points[run_start_index + run_point_count - 1U];
        StitchPoint neighbor_point = source_points[run_start_index + run_point_count - 2U];
        for (size_t reverse_step = 2U; reverse_step <= run_point_count; reverse_step += 1U) {
            const StitchPoint candidate =
                source_points[run_start_index + run_point_count - reverse_step];
            if (candidate.position_x != anchor_point.position_x ||
                candidate.position_y != anchor_point.position_y) {
                neighbor_point = candidate;
                break;
            }
        }

        int32_t lock_step_x = 0;
        int32_t lock_step_y = 0;
        compute_lock_step(anchor_point, neighbor_point, &lock_step_x, &lock_step_y);
        const int32_t inward_x = anchor_point.position_x + lock_step_x;
        const int32_t inward_y = anchor_point.position_y + lock_step_y;

        if (!design_append_point(destination, inward_x, inward_y) ||
            !design_append_point(destination, anchor_point.position_x, anchor_point.position_y) ||
            !design_append_point(destination, inward_x, inward_y)) {
            return false;
        }
    }

    const size_t output_point_count = destination->point_count - output_point_offset;
    return design_append_chain(
        destination,
        output_point_offset,
        output_point_count,
        color_block_index,
        cut_at_start,
        cut_at_end
    );
}

bool reorder_cutout_chains_by_side(StitchDesign *design, CutRectangle cutout_rectangle) {
    bool succeeded = false;
    uint8_t *chain_sides = nullptr;
    StitchChain *reordered_chains = nullptr;

    if (design->chain_count < 2U) {
        return true;
    }

    size_t left_or_right_chains = 0U;
    size_t top_or_bottom_chains = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count == 0U) {
            continue;
        }
        bool has_chain_bounds = false;
        int64_t chain_min_x = 0LL;
        int64_t chain_max_x = 0LL;
        int64_t chain_min_y = 0LL;
        int64_t chain_max_y = 0LL;
        expand_bounds_from_chain(
            design,
            chain,
            &has_chain_bounds,
            &chain_min_x,
            &chain_max_x,
            &chain_min_y,
            &chain_max_y
        );
        if (!has_chain_bounds) {
            continue;
        }
        if (chain_max_x <= (int64_t)cutout_rectangle.minimum_x ||
            chain_min_x >= (int64_t)cutout_rectangle.maximum_x) {
            left_or_right_chains += 1U;
        }
        if (chain_max_y <= (int64_t)cutout_rectangle.minimum_y ||
            chain_min_y >= (int64_t)cutout_rectangle.maximum_y) {
            top_or_bottom_chains += 1U;
        }
    }

    const int64_t rectangle_width =
        (int64_t)cutout_rectangle.maximum_x - (int64_t)cutout_rectangle.minimum_x;
    const int64_t rectangle_height =
        (int64_t)cutout_rectangle.maximum_y - (int64_t)cutout_rectangle.minimum_y;
    bool split_along_x = (rectangle_height >= rectangle_width);
    if (left_or_right_chains > 0U && top_or_bottom_chains == 0U) {
        split_along_x = true;
    } else if (top_or_bottom_chains > 0U && left_or_right_chains == 0U) {
        split_along_x = false;
    }

    const int64_t rectangle_center_x =
        ((int64_t)cutout_rectangle.minimum_x + (int64_t)cutout_rectangle.maximum_x) / 2LL;
    const int64_t rectangle_center_y =
        ((int64_t)cutout_rectangle.minimum_y + (int64_t)cutout_rectangle.maximum_y) / 2LL;

    size_t side_bytes = 0U;
    size_t chain_bytes = 0U;
    if (ckd_mul(&side_bytes, design->chain_count, sizeof(uint8_t)) ||
        ckd_mul(&chain_bytes, design->chain_count, sizeof(StitchChain))) {
        goto cleanup;
    }

    chain_sides = (uint8_t *)malloc(side_bytes);
    reordered_chains = (StitchChain *)malloc(chain_bytes);
    if (chain_sides == nullptr || reordered_chains == nullptr) {
        (void)fprintf(stderr, "Failed to allocate cutout reorder buffers\n");
        goto cleanup;
    }

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        chain_sides[chain_index] = 0U;
        if (chain.point_count == 0U) {
            continue;
        }
        int64_t sum_x = 0LL;
        int64_t sum_y = 0LL;
        for (size_t local_index = 0U; local_index < chain.point_count; local_index += 1U) {
            const StitchPoint point = design->points[chain.point_offset + local_index];
            sum_x += (int64_t)point.position_x;
            sum_y += (int64_t)point.position_y;
        }
        const int64_t average_x = sum_x / (int64_t)chain.point_count;
        const int64_t average_y = sum_y / (int64_t)chain.point_count;

        if (split_along_x) {
            if (average_x >= rectangle_center_x) {
                chain_sides[chain_index] = 1U;
            }
        } else if (average_y >= rectangle_center_y) {
            chain_sides[chain_index] = 1U;
        }
    }

    size_t write_index = 0U;
    size_t range_start = 0U;
    while (range_start < design->chain_count) {
        const size_t block_index = design->chains[range_start].color_block_index;
        size_t range_end = range_start + 1U;
        while (range_end < design->chain_count &&
               design->chains[range_end].color_block_index == block_index) {
            range_end += 1U;
        }

        for (size_t chain_index = range_start; chain_index < range_end; chain_index += 1U) {
            if (chain_sides[chain_index] == 0U) {
                reordered_chains[write_index] = design->chains[chain_index];
                write_index += 1U;
            }
        }
        for (size_t chain_index = range_start; chain_index < range_end; chain_index += 1U) {
            if (chain_sides[chain_index] != 0U) {
                reordered_chains[write_index] = design->chains[chain_index];
                write_index += 1U;
            }
        }

        range_start = range_end;
    }

    memcpy(design->chains, reordered_chains, chain_bytes);
    succeeded = true;

cleanup:
    free(reordered_chains);
    free(chain_sides);
    return succeeded;
}

bool apply_edit_operation(
    const StitchDesign *source,
    CrossGridInfo grid,
    EditOperationKind operation,
    CutRectangle rectangle,
    int32_t split_coordinate,
    bool keep_second_half,
    StitchDesign *result
) {
    bool succeeded = false;
    uint8_t *keep_mask = nullptr;
    init_stitch_design(result);

    if (rectangle.minimum_x > rectangle.maximum_x) {
        const int32_t swap_x = rectangle.minimum_x;
        rectangle.minimum_x = rectangle.maximum_x;
        rectangle.maximum_x = swap_x;
    }
    if (rectangle.minimum_y > rectangle.maximum_y) {
        const int32_t swap_y = rectangle.minimum_y;
        rectangle.minimum_y = rectangle.maximum_y;
        rectangle.maximum_y = swap_y;
    }

    const size_t total_design_segments = count_design_segments(source);
    if (total_design_segments == 0U) {
        succeeded = true;
        goto cleanup;
    }

    keep_mask = (uint8_t *)calloc(total_design_segments, sizeof(uint8_t));
    if (keep_mask == nullptr) {
        (void)fprintf(stderr, "Failed to allocate edit keep mask\n");
        goto cleanup;
    }

    if (!build_edit_segment_mask(
            source,
            grid,
            operation,
            rectangle,
            split_coordinate,
            keep_second_half,
            keep_mask,
            total_design_segments
        )) {
        goto cleanup;
    }

    if (source->block_count > 0U) {
        size_t block_bytes = 0U;
        if (ckd_mul(&block_bytes, source->block_count, sizeof(ColorBlock)) ||
            !design_reserve_blocks(result, source->block_count)) {
            goto cleanup;
        }
        memcpy(result->blocks, source->blocks, block_bytes);
        result->block_count = source->block_count;
    }
    size_t reserve_points = 0U;
    size_t reserve_chains = 0U;
    if (ckd_add(&reserve_points, source->point_count, (size_t)1024U) ||
        ckd_add(&reserve_chains, source->chain_count, (size_t)128U) ||
        !design_reserve_points(result, reserve_points) ||
        !design_reserve_chains(result, reserve_chains)) {
        goto cleanup;
    }

    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < source->chain_count; chain_index += 1U) {
        const StitchChain chain = source->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t total_segments = chain.point_count - 1U;
        if (chain.color_block_index >= source->block_count ||
            !source->blocks[chain.color_block_index].enabled) {
            flat_segment_index += total_segments;
            continue;
        }

        const StitchPoint *chain_points = &source->points[chain.point_offset];
        size_t segment_index = 0U;

        while (segment_index < total_segments &&
               flat_segment_index + segment_index < total_design_segments) {
            while (segment_index < total_segments &&
                   flat_segment_index + segment_index < total_design_segments &&
                   keep_mask[flat_segment_index + segment_index] == 0U) {
                segment_index += 1U;
            }
            if (segment_index >= total_segments ||
                flat_segment_index + segment_index >= total_design_segments) {
                break;
            }

            const size_t run_start_segment = segment_index;
            while (segment_index < total_segments &&
                   flat_segment_index + segment_index < total_design_segments &&
                   keep_mask[flat_segment_index + segment_index] != 0U) {
                segment_index += 1U;
            }

            const size_t run_segment_count = segment_index - run_start_segment;
            const bool cut_at_start = (run_start_segment > 0U);
            const bool cut_at_end = (segment_index < total_segments);

            if (!append_locked_subchain(
                    result,
                    chain_points,
                    run_start_segment,
                    run_segment_count + 1U,
                    chain.color_block_index,
                    cut_at_start,
                    cut_at_end
                )) {
                goto cleanup;
            }
        }
        flat_segment_index += total_segments;
    }

    if (!compact_non_empty_blocks(result)) {
        goto cleanup;
    }
    if (operation == EDIT_CUT_OUT && !reorder_cutout_chains_by_side(result, rectangle)) {
        goto cleanup;
    }

    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    free(keep_mask);
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

void world_to_grid_cell(
    int32_t world_x,
    int32_t world_y,
    CrossGridInfo grid,
    int32_t *cell_column_out,
    int32_t *cell_row_out
) {
    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }
    *cell_column_out = floor_divide_int32(world_x - grid.grid_origin_x, pitch);
    *cell_row_out = floor_divide_int32(world_y - grid.grid_origin_y, pitch);
}

CutRectangle resolve_cross_box_at_point(
    const StitchDesign *design,
    CrossGridInfo grid,
    int32_t world_x,
    int32_t world_y,
    bool *snapped_to_stitch_out
) {
    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }

    const int64_t maximum_search_radius = ((int64_t)pitch * 65LL) / 100LL + 2LL;
    const int64_t maximum_radius_squared = maximum_search_radius * maximum_search_radius;
    int64_t best_distance_squared = maximum_radius_squared + 1LL;
    int32_t best_center_x = 0;
    int32_t best_center_y = 0;
    bool snapped = false;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled ||
            chain.point_count < 2U) {
            continue;
        }
        for (size_t local_index = 0U; local_index + 1U < chain.point_count; local_index += 1U) {
            const StitchPoint start_point = design->points[chain.point_offset + local_index];
            const StitchPoint end_point = design->points[chain.point_offset + local_index + 1U];
            int32_t diagonal_pitch = 0;
            if (!measure_diagonal_pitch(start_point, end_point, &diagonal_pitch)) {
                continue;
            }
            int32_t pitch_error = diagonal_pitch - pitch;
            if (pitch_error < 0) {
                pitch_error = -pitch_error;
            }
            if (pitch_error > 2) {
                continue;
            }
            const int32_t midpoint_x =
                (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
            const int32_t midpoint_y =
                (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);
            const int64_t delta_x = (int64_t)midpoint_x - (int64_t)world_x;
            const int64_t delta_y = (int64_t)midpoint_y - (int64_t)world_y;
            const int64_t distance_squared = delta_x * delta_x + delta_y * delta_y;
            if (distance_squared < best_distance_squared) {
                best_distance_squared = distance_squared;
                best_center_x = midpoint_x;
                best_center_y = midpoint_y;
                snapped = true;
            }
        }
    }

    if (snapped_to_stitch_out != nullptr) {
        *snapped_to_stitch_out = snapped;
    }

    CutRectangle box = {};
    if (snapped) {
        const int32_t half_pitch = pitch / 2;
        box.minimum_x = best_center_x - half_pitch;
        box.maximum_x = box.minimum_x + pitch;
        box.minimum_y = best_center_y - half_pitch;
        box.maximum_y = box.minimum_y + pitch;
        return box;
    }

    int32_t cell_column = 0;
    int32_t cell_row = 0;
    world_to_grid_cell(world_x, world_y, grid, &cell_column, &cell_row);
    box.minimum_x = grid.grid_origin_x + cell_column * pitch;
    box.maximum_x = box.minimum_x + pitch;
    box.minimum_y = grid.grid_origin_y + cell_row * pitch;
    box.maximum_y = box.minimum_y + pitch;
    return box;
}

static void check_cross_box_candidate_entry(
    const StitchDesign *design,
    SpatialSegmentEntry entry,
    int32_t pitch,
    int32_t world_x,
    int32_t world_y,
    int64_t *best_distance_squared,
    int32_t *best_center_x,
    int32_t *best_center_y,
    bool *snapped
) {
    if ((size_t)entry.chain_index >= design->chain_count) {
        return;
    }
    const StitchChain chain = design->chains[entry.chain_index];
    if (chain.color_block_index >= design->block_count ||
        !design->blocks[chain.color_block_index].enabled ||
        (size_t)entry.local_index + 1U >= chain.point_count) {
        return;
    }
    const StitchPoint start_point = design->points[chain.point_offset + (size_t)entry.local_index];
    const StitchPoint end_point = design->points[chain.point_offset + (size_t)entry.local_index + 1U];
    int32_t diagonal_pitch = 0;
    if (!measure_diagonal_pitch(start_point, end_point, &diagonal_pitch)) {
        return;
    }
    int32_t pitch_error = diagonal_pitch - pitch;
    if (pitch_error < 0) {
        pitch_error = -pitch_error;
    }
    if (pitch_error > 2) {
        return;
    }
    const int32_t midpoint_x =
        (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
    const int32_t midpoint_y =
        (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);
    const int64_t delta_x = (int64_t)midpoint_x - (int64_t)world_x;
    const int64_t delta_y = (int64_t)midpoint_y - (int64_t)world_y;
    const int64_t distance_squared = delta_x * delta_x + delta_y * delta_y;
    if (distance_squared < *best_distance_squared) {
        *best_distance_squared = distance_squared;
        *best_center_x = midpoint_x;
        *best_center_y = midpoint_y;
        *snapped = true;
    }
}

CutRectangle resolve_cross_box_at_point_indexed(
    const StitchDesign *design,
    const DesignSpatialIndex *index,
    CrossGridInfo grid,
    int32_t world_x,
    int32_t world_y,
    bool *snapped_to_stitch_out
) {
    if (index == nullptr || !index->valid || index->cell_offsets == nullptr || index->cell_size <= 0) {
        return resolve_cross_box_at_point(design, grid, world_x, world_y, snapped_to_stitch_out);
    }

    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }

    const int64_t maximum_search_radius = ((int64_t)pitch * 65LL) / 100LL + 2LL;
    const int64_t maximum_radius_squared = maximum_search_radius * maximum_search_radius;
    int64_t best_distance_squared = maximum_radius_squared + 1LL;
    int32_t best_center_x = 0;
    int32_t best_center_y = 0;
    bool snapped = false;

    const int32_t search_span = (int32_t)maximum_search_radius;
    int32_t min_col = floor_divide_int32(world_x - search_span - index->origin_x, index->cell_size);
    int32_t max_col = floor_divide_int32(world_x + search_span - index->origin_x, index->cell_size);
    int32_t min_row = floor_divide_int32(world_y - search_span - index->origin_y, index->cell_size);
    int32_t max_row = floor_divide_int32(world_y + search_span - index->origin_y, index->cell_size);

    if (min_col < 0) {
        min_col = 0;
    }
    if (max_col >= index->grid_cols) {
        max_col = index->grid_cols - 1;
    }
    if (min_row < 0) {
        min_row = 0;
    }
    if (max_row >= index->grid_rows) {
        max_row = index->grid_rows - 1;
    }

    if (index->entries != nullptr && min_col <= max_col && min_row <= max_row) {
        for (int32_t row = min_row; row <= max_row; row += 1) {
            for (int32_t col = min_col; col <= max_col; col += 1) {
                const size_t cell_index = (size_t)row * (size_t)index->grid_cols + (size_t)col;
                const uint32_t begin = index->cell_offsets[cell_index];
                const uint32_t end = index->cell_offsets[cell_index + 1U];
                for (uint32_t slot = begin; slot < end; slot += 1U) {
                    check_cross_box_candidate_entry(
                        design,
                        index->entries[slot],
                        pitch,
                        world_x,
                        world_y,
                        &best_distance_squared,
                        &best_center_x,
                        &best_center_y,
                        &snapped
                    );
                }
            }
        }
    }

    for (size_t overflow_index = 0U; overflow_index < index->overflow_count; overflow_index += 1U) {
        check_cross_box_candidate_entry(
            design,
            index->overflow_entries[overflow_index],
            pitch,
            world_x,
            world_y,
            &best_distance_squared,
            &best_center_x,
            &best_center_y,
            &snapped
        );
    }

    if (snapped_to_stitch_out != nullptr) {
        *snapped_to_stitch_out = snapped;
    }

    CutRectangle box = {};
    if (snapped) {
        const int32_t half_pitch = pitch / 2;
        box.minimum_x = best_center_x - half_pitch;
        box.maximum_x = box.minimum_x + pitch;
        box.minimum_y = best_center_y - half_pitch;
        box.maximum_y = box.minimum_y + pitch;
        return box;
    }

    int32_t cell_column = 0;
    int32_t cell_row = 0;
    world_to_grid_cell(world_x, world_y, grid, &cell_column, &cell_row);
    box.minimum_x = grid.grid_origin_x + cell_column * pitch;
    box.maximum_x = box.minimum_x + pitch;
    box.minimum_y = grid.grid_origin_y + cell_row * pitch;
    box.maximum_y = box.minimum_y + pitch;
    return box;
}

static bool segment_belongs_to_cross_box(
    StitchPoint start_point,
    StitchPoint end_point,
    CutRectangle cross_box
) {
    if (start_point.position_x < cross_box.minimum_x - 2 ||
        start_point.position_x > cross_box.maximum_x + 2 ||
        end_point.position_x < cross_box.minimum_x - 2 ||
        end_point.position_x > cross_box.maximum_x + 2 ||
        start_point.position_y < cross_box.minimum_y - 2 ||
        start_point.position_y > cross_box.maximum_y + 2 ||
        end_point.position_y < cross_box.minimum_y - 2 ||
        end_point.position_y > cross_box.maximum_y + 2) {
        return false;
    }

    const int32_t midpoint_x =
        (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL);
    const int32_t midpoint_y =
        (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL);
    return midpoint_x > cross_box.minimum_x &&
           midpoint_x < cross_box.maximum_x &&
           midpoint_y > cross_box.minimum_y &&
           midpoint_y < cross_box.maximum_y;
}

size_t select_cross_box_in_mask(
    const StitchDesign *design,
    CutRectangle cross_box,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode
) {
    size_t matched_count = 0U;
    size_t already_selected_count = 0U;
    size_t flat_segment_index = 0U;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const bool block_enabled =
            chain.color_block_index < design->block_count &&
            design->blocks[chain.color_block_index].enabled;
        const size_t chain_segments = chain.point_count - 1U;
        if (!block_enabled) {
            flat_segment_index += chain_segments;
            continue;
        }

        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (flat_segment_index < mask_length &&
                segment_belongs_to_cross_box(
                    design->points[chain.point_offset + local_index],
                    design->points[chain.point_offset + local_index + 1U],
                    cross_box
                )) {
                matched_count += 1U;
                if (segment_mask[flat_segment_index] != 0U) {
                    already_selected_count += 1U;
                }
            }
            flat_segment_index += 1U;
        }
    }

    if (matched_count == 0U) {
        return 0U;
    }

    uint8_t target_value = 1U;
    if (toggle_mode && already_selected_count == matched_count) {
        target_value = 0U;
    }

    flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const bool block_enabled =
            chain.color_block_index < design->block_count &&
            design->blocks[chain.color_block_index].enabled;
        const size_t chain_segments = chain.point_count - 1U;
        if (!block_enabled) {
            flat_segment_index += chain_segments;
            continue;
        }

        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            if (flat_segment_index < mask_length &&
                segment_belongs_to_cross_box(
                    design->points[chain.point_offset + local_index],
                    design->points[chain.point_offset + local_index + 1U],
                    cross_box
                )) {
                segment_mask[flat_segment_index] = target_value;
            }
            flat_segment_index += 1U;
        }
    }

    return matched_count;
}

static bool entry_matches_cross_box(
    const StitchDesign *design,
    SpatialSegmentEntry entry,
    CutRectangle cross_box
) {
    if ((size_t)entry.chain_index >= design->chain_count) {
        return false;
    }
    const StitchChain chain = design->chains[entry.chain_index];
    if (chain.color_block_index >= design->block_count ||
        !design->blocks[chain.color_block_index].enabled ||
        (size_t)entry.local_index + 1U >= chain.point_count) {
        return false;
    }
    return segment_belongs_to_cross_box(
        design->points[chain.point_offset + (size_t)entry.local_index],
        design->points[chain.point_offset + (size_t)entry.local_index + 1U],
        cross_box
    );
}

size_t select_cross_box_in_mask_indexed(
    const StitchDesign *design,
    const DesignSpatialIndex *index,
    CutRectangle cross_box,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode
) {
    if (index == nullptr || !index->valid || index->cell_offsets == nullptr || index->cell_size <= 0) {
        return select_cross_box_in_mask(design, cross_box, segment_mask, mask_length, toggle_mode);
    }

    int32_t min_col = floor_divide_int32(cross_box.minimum_x - 2 - index->origin_x, index->cell_size);
    int32_t max_col = floor_divide_int32(cross_box.maximum_x + 2 - index->origin_x, index->cell_size);
    int32_t min_row = floor_divide_int32(cross_box.minimum_y - 2 - index->origin_y, index->cell_size);
    int32_t max_row = floor_divide_int32(cross_box.maximum_y + 2 - index->origin_y, index->cell_size);
    if (min_col < 0) {
        min_col = 0;
    }
    if (max_col >= index->grid_cols) {
        max_col = index->grid_cols - 1;
    }
    if (min_row < 0) {
        min_row = 0;
    }
    if (max_row >= index->grid_rows) {
        max_row = index->grid_rows - 1;
    }

    size_t matched_count = 0U;
    size_t already_selected_count = 0U;

    if (index->entries != nullptr && min_col <= max_col && min_row <= max_row) {
        for (int32_t row = min_row; row <= max_row; row += 1) {
            for (int32_t col = min_col; col <= max_col; col += 1) {
                const size_t cell_index = (size_t)row * (size_t)index->grid_cols + (size_t)col;
                const uint32_t begin = index->cell_offsets[cell_index];
                const uint32_t end = index->cell_offsets[cell_index + 1U];
                for (uint32_t slot = begin; slot < end; slot += 1U) {
                    const SpatialSegmentEntry entry = index->entries[slot];
                    if ((size_t)entry.flat_index < mask_length &&
                        entry_matches_cross_box(design, entry, cross_box)) {
                        matched_count += 1U;
                        if (segment_mask[entry.flat_index] != 0U) {
                            already_selected_count += 1U;
                        }
                    }
                }
            }
        }
    }
    for (size_t overflow_index = 0U; overflow_index < index->overflow_count; overflow_index += 1U) {
        const SpatialSegmentEntry entry = index->overflow_entries[overflow_index];
        if ((size_t)entry.flat_index < mask_length &&
            entry_matches_cross_box(design, entry, cross_box)) {
            matched_count += 1U;
            if (segment_mask[entry.flat_index] != 0U) {
                already_selected_count += 1U;
            }
        }
    }

    if (matched_count == 0U) {
        return 0U;
    }

    uint8_t target_value = 1U;
    if (toggle_mode && already_selected_count == matched_count) {
        target_value = 0U;
    }

    if (index->entries != nullptr && min_col <= max_col && min_row <= max_row) {
        for (int32_t row = min_row; row <= max_row; row += 1) {
            for (int32_t col = min_col; col <= max_col; col += 1) {
                const size_t cell_index = (size_t)row * (size_t)index->grid_cols + (size_t)col;
                const uint32_t begin = index->cell_offsets[cell_index];
                const uint32_t end = index->cell_offsets[cell_index + 1U];
                for (uint32_t slot = begin; slot < end; slot += 1U) {
                    const SpatialSegmentEntry entry = index->entries[slot];
                    if ((size_t)entry.flat_index < mask_length &&
                        entry_matches_cross_box(design, entry, cross_box)) {
                        segment_mask[entry.flat_index] = target_value;
                    }
                }
            }
        }
    }
    for (size_t overflow_index = 0U; overflow_index < index->overflow_count; overflow_index += 1U) {
        const SpatialSegmentEntry entry = index->overflow_entries[overflow_index];
        if ((size_t)entry.flat_index < mask_length &&
            entry_matches_cross_box(design, entry, cross_box)) {
            segment_mask[entry.flat_index] = target_value;
        }
    }

    return matched_count;
}

size_t select_cross_at_point_in_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    int32_t world_x,
    int32_t world_y,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode,
    CutRectangle *resolved_box_out
) {
    bool snapped = false;
    const CutRectangle cross_box = resolve_cross_box_at_point(design, grid, world_x, world_y, &snapped);
    if (resolved_box_out != nullptr) {
        *resolved_box_out = cross_box;
    }
    return select_cross_box_in_mask(design, cross_box, segment_mask, mask_length, toggle_mode);
}

size_t select_crosses_in_rectangle_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    CutRectangle selection_rectangle,
    uint8_t *segment_mask,
    size_t mask_length
) {
    if (selection_rectangle.minimum_x > selection_rectangle.maximum_x) {
        const int32_t swap_x = selection_rectangle.minimum_x;
        selection_rectangle.minimum_x = selection_rectangle.maximum_x;
        selection_rectangle.maximum_x = swap_x;
    }
    if (selection_rectangle.minimum_y > selection_rectangle.maximum_y) {
        const int32_t swap_y = selection_rectangle.minimum_y;
        selection_rectangle.minimum_y = selection_rectangle.maximum_y;
        selection_rectangle.maximum_y = swap_y;
    }

    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }

    size_t selected_count = 0U;
    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const bool block_enabled =
            chain.color_block_index < design->block_count &&
            design->blocks[chain.color_block_index].enabled;
        const size_t chain_segments = chain.point_count - 1U;
        if (!block_enabled) {
            flat_segment_index += chain_segments;
            continue;
        }

        for (size_t local_index = 0U; local_index < chain_segments; local_index += 1U) {
            const StitchPoint start_point = design->points[chain.point_offset + local_index];
            const StitchPoint end_point = design->points[chain.point_offset + local_index + 1U];
            int64_t span_x = (int64_t)end_point.position_x - (int64_t)start_point.position_x;
            if (span_x < 0LL) {
                span_x = -span_x;
            }
            int64_t span_y = (int64_t)end_point.position_y - (int64_t)start_point.position_y;
            if (span_y < 0LL) {
                span_y = -span_y;
            }
            const StitchPoint midpoint = {
                (int32_t)(((int64_t)start_point.position_x + (int64_t)end_point.position_x) / 2LL),
                (int32_t)(((int64_t)start_point.position_y + (int64_t)end_point.position_y) / 2LL)
            };
            if (flat_segment_index < mask_length &&
                span_x <= (int64_t)pitch + 3LL &&
                span_y <= (int64_t)pitch + 3LL &&
                point_is_inside_rectangle(midpoint, selection_rectangle)) {
                segment_mask[flat_segment_index] = 1U;
                selected_count += 1U;
            }
            flat_segment_index += 1U;
        }
    }
    return selected_count;
}

static int64_t point_to_segment_distance_squared(
    int32_t query_x,
    int32_t query_y,
    StitchPoint start_point,
    StitchPoint end_point
) {
    const double delta_x = (double)end_point.position_x - (double)start_point.position_x;
    const double delta_y = (double)end_point.position_y - (double)start_point.position_y;
    const double length_squared = delta_x * delta_x + delta_y * delta_y;
    double projection = 0.0;
    if (length_squared > 0.0001) {
        projection =
            (((double)query_x - (double)start_point.position_x) * delta_x +
             ((double)query_y - (double)start_point.position_y) * delta_y) /
            length_squared;
        if (projection < 0.0) {
            projection = 0.0;
        }
        if (projection > 1.0) {
            projection = 1.0;
        }
    }
    const double closest_x = (double)start_point.position_x + projection * delta_x;
    const double closest_y = (double)start_point.position_y + projection * delta_y;
    const double difference_x = (double)query_x - closest_x;
    const double difference_y = (double)query_y - closest_y;
    return (int64_t)llround(difference_x * difference_x + difference_y * difference_y);
}

bool find_nearest_chain_at_point(
    const StitchDesign *design,
    int32_t world_x,
    int32_t world_y,
    int32_t maximum_distance_units,
    size_t *out_chain_index
) {
    const int64_t maximum_distance_squared =
        (int64_t)maximum_distance_units * (int64_t)maximum_distance_units;
    int64_t best_distance_squared = maximum_distance_squared + 1LL;
    bool found = false;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled ||
            chain.point_count < 2U) {
            continue;
        }
        for (size_t local_index = 0U; local_index + 1U < chain.point_count; local_index += 1U) {
            const int64_t distance_squared = point_to_segment_distance_squared(
                world_x,
                world_y,
                design->points[chain.point_offset + local_index],
                design->points[chain.point_offset + local_index + 1U]
            );
            if (distance_squared < best_distance_squared) {
                best_distance_squared = distance_squared;
                *out_chain_index = chain_index;
                found = true;
            }
        }
    }
    return found;
}

static void check_nearest_chain_candidate(
    const StitchDesign *design,
    SpatialSegmentEntry entry,
    int32_t world_x,
    int32_t world_y,
    int64_t *best_distance_squared,
    size_t *out_chain_index,
    bool *found
) {
    if ((size_t)entry.chain_index >= design->chain_count) {
        return;
    }
    const StitchChain chain = design->chains[entry.chain_index];
    if (chain.color_block_index >= design->block_count ||
        !design->blocks[chain.color_block_index].enabled ||
        (size_t)entry.local_index + 1U >= chain.point_count) {
        return;
    }
    const int64_t distance_squared = point_to_segment_distance_squared(
        world_x,
        world_y,
        design->points[chain.point_offset + (size_t)entry.local_index],
        design->points[chain.point_offset + (size_t)entry.local_index + 1U]
    );
    if (distance_squared < *best_distance_squared) {
        *best_distance_squared = distance_squared;
        *out_chain_index = (size_t)entry.chain_index;
        *found = true;
    }
}

bool find_nearest_chain_at_point_indexed(
    const StitchDesign *design,
    const DesignSpatialIndex *index,
    int32_t world_x,
    int32_t world_y,
    int32_t maximum_distance_units,
    size_t *out_chain_index
) {
    if (index == nullptr || !index->valid || index->cell_offsets == nullptr || index->cell_size <= 0) {
        return find_nearest_chain_at_point(design, world_x, world_y, maximum_distance_units, out_chain_index);
    }

    const int64_t maximum_distance_squared =
        (int64_t)maximum_distance_units * (int64_t)maximum_distance_units;
    int64_t best_distance_squared = maximum_distance_squared + 1LL;
    bool found = false;

    const int64_t expanded_radius =
        (int64_t)maximum_distance_units + (int64_t)index->max_cell_segment_half_span;
    int32_t clamped_radius = INT32_MAX / 4;
    if (expanded_radius < (int64_t)clamped_radius) {
        clamped_radius = (int32_t)expanded_radius;
    }

    int32_t min_col = floor_divide_int32(world_x - clamped_radius - index->origin_x, index->cell_size);
    int32_t max_col = floor_divide_int32(world_x + clamped_radius - index->origin_x, index->cell_size);
    int32_t min_row = floor_divide_int32(world_y - clamped_radius - index->origin_y, index->cell_size);
    int32_t max_row = floor_divide_int32(world_y + clamped_radius - index->origin_y, index->cell_size);
    if (min_col < 0) {
        min_col = 0;
    }
    if (max_col >= index->grid_cols) {
        max_col = index->grid_cols - 1;
    }
    if (min_row < 0) {
        min_row = 0;
    }
    if (max_row >= index->grid_rows) {
        max_row = index->grid_rows - 1;
    }

    if (index->entries != nullptr && min_col <= max_col && min_row <= max_row) {
        for (int32_t row = min_row; row <= max_row; row += 1) {
            for (int32_t col = min_col; col <= max_col; col += 1) {
                const size_t cell_index = (size_t)row * (size_t)index->grid_cols + (size_t)col;
                const uint32_t begin = index->cell_offsets[cell_index];
                const uint32_t end = index->cell_offsets[cell_index + 1U];
                for (uint32_t slot = begin; slot < end; slot += 1U) {
                    check_nearest_chain_candidate(
                        design,
                        index->entries[slot],
                        world_x,
                        world_y,
                        &best_distance_squared,
                        out_chain_index,
                        &found
                    );
                }
            }
        }
    }

    for (size_t overflow_index = 0U; overflow_index < index->overflow_count; overflow_index += 1U) {
        check_nearest_chain_candidate(
            design,
            index->overflow_entries[overflow_index],
            world_x,
            world_y,
            &best_distance_squared,
            out_chain_index,
            &found
        );
    }

    return found;
}

static void enqueue_spatial_neighbors_for_point(
    StitchPoint point,
    int32_t threshold,
    int64_t threshold_squared,
    size_t bucket_mask,
    const int32_t *bucket_heads,
    const SpatialVertexNode *vertex_nodes,
    uint8_t *visited_flags,
    size_t *bfs_queue,
    size_t queue_capacity,
    size_t *queue_tail
) {
    const int32_t base_cell_x = floor_divide_int32(point.position_x, threshold);
    const int32_t base_cell_y = floor_divide_int32(point.position_y, threshold);

    for (int32_t neighbor_offset = 0; neighbor_offset < 9; neighbor_offset += 1) {
        const int32_t offset_x = (neighbor_offset % 3) - 1;
        const int32_t offset_y = (neighbor_offset / 3) - 1;
        const size_t bucket_index =
            (size_t)hash_two_int32(base_cell_x + offset_x, base_cell_y + offset_y) & bucket_mask;

        int32_t node_index = bucket_heads[bucket_index];
        while (node_index >= 0) {
            const SpatialVertexNode node = vertex_nodes[node_index];
            node_index = node.next_node_index;
            if (node.local_chain_index >= queue_capacity ||
                visited_flags[node.local_chain_index] != 0U) {
                continue;
            }
            const int64_t difference_x = (int64_t)node.position_x - (int64_t)point.position_x;
            const int64_t difference_y = (int64_t)node.position_y - (int64_t)point.position_y;
            if (difference_x * difference_x + difference_y * difference_y <= threshold_squared) {
                visited_flags[node.local_chain_index] = 1U;
                if (*queue_tail < queue_capacity) {
                    bfs_queue[*queue_tail] = node.local_chain_index;
                    *queue_tail += 1U;
                }
            }
        }
    }
}

/* Iterative spatial-hash BFS finding connected same-color chains within pitch * 1.25. */
size_t select_connected_element_in_mask(
    const StitchDesign *design,
    CrossGridInfo grid,
    size_t seed_chain_index,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode
) {
    size_t selected_segments = 0U;
    size_t *same_color_chains = nullptr;
    size_t *chain_segment_offsets = nullptr;
    uint8_t *visited_flags = nullptr;
    size_t *bfs_queue = nullptr;
    int32_t *bucket_heads = nullptr;
    SpatialVertexNode *vertex_nodes = nullptr;

    if (seed_chain_index >= design->chain_count) {
        return 0U;
    }
    const size_t seed_block_index = design->chains[seed_chain_index].color_block_index;
    if (seed_block_index >= design->block_count || !design->blocks[seed_block_index].enabled) {
        return 0U;
    }
    const uint8_t target_janome_index = design->blocks[seed_block_index].janome_color_index;

    int32_t threshold = (int32_t)llround((double)grid.cross_pitch_units * 1.25);
    if (threshold < 12) {
        threshold = 12;
    }
    const int64_t threshold_squared = (int64_t)threshold * (int64_t)threshold;

    size_t chain_offset_bytes = 0U;
    if (ckd_mul(&chain_offset_bytes, design->chain_count, sizeof(size_t))) {
        goto cleanup;
    }
    chain_segment_offsets = (size_t *)malloc(chain_offset_bytes);
    same_color_chains = (size_t *)malloc(chain_offset_bytes);
    if (chain_segment_offsets == nullptr || same_color_chains == nullptr) {
        goto cleanup;
    }

    size_t running_segment = 0U;
    size_t candidate_chain_count = 0U;
    size_t seed_local_index = SIZE_MAX;
    size_t total_candidate_vertices = 0U;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        const StitchChain chain = design->chains[chain_index];
        chain_segment_offsets[chain_index] = running_segment;
        if (chain.point_count >= 2U) {
            running_segment += chain.point_count - 1U;
        } else {
            continue;
        }
        if (chain.color_block_index >= design->block_count ||
            !design->blocks[chain.color_block_index].enabled) {
            continue;
        }
        if (design->blocks[chain.color_block_index].janome_color_index == target_janome_index) {
            if (chain_index == seed_chain_index) {
                seed_local_index = candidate_chain_count;
            }
            same_color_chains[candidate_chain_count] = chain_index;
            candidate_chain_count += 1U;
            if (ckd_add(&total_candidate_vertices, total_candidate_vertices, chain.point_count)) {
                goto cleanup;
            }
        }
    }

    if (seed_local_index == SIZE_MAX || candidate_chain_count == 0U || total_candidate_vertices == 0U) {
        goto cleanup;
    }

    const size_t bucket_count = compute_power_of_two_buckets(total_candidate_vertices, 1048576U);
    const size_t bucket_mask = bucket_count - 1U;

    size_t queue_bytes = 0U;
    size_t bucket_bytes = 0U;
    size_t node_bytes = 0U;
    if (ckd_mul(&queue_bytes, candidate_chain_count, sizeof(size_t)) ||
        ckd_mul(&bucket_bytes, bucket_count, sizeof(int32_t)) ||
        ckd_mul(&node_bytes, total_candidate_vertices, sizeof(SpatialVertexNode))) {
        goto cleanup;
    }

    visited_flags = (uint8_t *)calloc(candidate_chain_count, sizeof(uint8_t));
    bfs_queue = (size_t *)malloc(queue_bytes);
    bucket_heads = (int32_t *)malloc(bucket_bytes);
    vertex_nodes = (SpatialVertexNode *)malloc(node_bytes);
    if (visited_flags == nullptr || bfs_queue == nullptr || bucket_heads == nullptr || vertex_nodes == nullptr) {
        goto cleanup;
    }

    for (size_t bucket_index = 0U; bucket_index < bucket_count; bucket_index += 1U) {
        bucket_heads[bucket_index] = -1;
    }

    size_t node_write_index = 0U;
    for (size_t local_chain = 0U; local_chain < candidate_chain_count; local_chain += 1U) {
        const StitchChain chain = design->chains[same_color_chains[local_chain]];
        for (size_t local_point_index = 0U; local_point_index < chain.point_count; local_point_index += 1U) {
            const StitchPoint point = design->points[chain.point_offset + local_point_index];
            const int32_t cell_x = floor_divide_int32(point.position_x, threshold);
            const int32_t cell_y = floor_divide_int32(point.position_y, threshold);
            const size_t bucket_index = (size_t)hash_two_int32(cell_x, cell_y) & bucket_mask;

            vertex_nodes[node_write_index].position_x = point.position_x;
            vertex_nodes[node_write_index].position_y = point.position_y;
            vertex_nodes[node_write_index].local_chain_index = local_chain;
            vertex_nodes[node_write_index].next_node_index = bucket_heads[bucket_index];
            bucket_heads[bucket_index] = (int32_t)node_write_index;
            node_write_index += 1U;
        }
    }

    size_t queue_head = 0U;
    size_t queue_tail = 0U;
    visited_flags[seed_local_index] = 1U;
    bfs_queue[queue_tail] = seed_local_index;
    queue_tail += 1U;

    while (queue_head < queue_tail && queue_head < candidate_chain_count) {
        const size_t current_local = bfs_queue[queue_head];
        queue_head += 1U;
        const StitchChain current_chain = design->chains[same_color_chains[current_local]];

        for (size_t local_point_index = 0U; local_point_index < current_chain.point_count; local_point_index += 1U) {
            enqueue_spatial_neighbors_for_point(
                design->points[current_chain.point_offset + local_point_index],
                threshold,
                threshold_squared,
                bucket_mask,
                bucket_heads,
                vertex_nodes,
                visited_flags,
                bfs_queue,
                candidate_chain_count,
                &queue_tail
            );
        }
    }

    uint8_t target_mask_value = 1U;
    if (toggle_mode) {
        const size_t seed_segment_offset = chain_segment_offsets[seed_chain_index];
        if (seed_segment_offset < mask_length && segment_mask[seed_segment_offset] != 0U) {
            target_mask_value = 0U;
        }
    }

    for (size_t queue_index = 0U;
         queue_index < queue_tail && queue_index < candidate_chain_count;
         queue_index += 1U) {
        const size_t local_chain = bfs_queue[queue_index];
        if (local_chain >= candidate_chain_count) {
            continue;
        }
        const size_t chain_index = same_color_chains[local_chain];
        if (chain_index >= design->chain_count) {
            continue;
        }
        const size_t segment_offset = chain_segment_offsets[chain_index];
        const size_t segment_count = design->chains[chain_index].point_count - 1U;
        for (size_t local_segment_index = 0U; local_segment_index < segment_count; local_segment_index += 1U) {
            if (segment_offset + local_segment_index < mask_length) {
                segment_mask[segment_offset + local_segment_index] = target_mask_value;
                selected_segments += 1U;
            }
        }
    }

cleanup:
    free(vertex_nodes);
    free(bucket_heads);
    free(bfs_queue);
    free(visited_flags);
    free(same_color_chains);
    free(chain_segment_offsets);
    return selected_segments;
}

size_t select_connected_element_in_mask_indexed(
    const StitchDesign *design,
    const DesignSpatialIndex *index,
    size_t seed_chain_index,
    uint8_t *segment_mask,
    size_t mask_length,
    bool toggle_mode
) {
    if (index == nullptr || !index->valid ||
        index->chain_component_id == nullptr ||
        index->component_chain_offsets == nullptr ||
        index->component_chains == nullptr ||
        index->chain_segment_offsets == nullptr ||
        seed_chain_index >= index->chain_count ||
        seed_chain_index >= design->chain_count) {
        CrossGridInfo fallback_grid = {};
        fallback_grid.cross_pitch_units = DEFAULT_CROSS_PITCH_UNITS;
        if (index != nullptr && index->component_pitch_units >= MIN_CROSS_PITCH_UNITS) {
            fallback_grid.cross_pitch_units = index->component_pitch_units;
        }
        return select_connected_element_in_mask(
            design,
            fallback_grid,
            seed_chain_index,
            segment_mask,
            mask_length,
            toggle_mode
        );
    }

    const size_t component_id = index->chain_component_id[seed_chain_index];
    if (component_id >= index->component_count) {
        return 0U;
    }

    uint8_t target_mask_value = 1U;
    if (toggle_mode) {
        const size_t seed_offset = index->chain_segment_offsets[seed_chain_index];
        if (seed_offset < mask_length && segment_mask[seed_offset] != 0U) {
            target_mask_value = 0U;
        }
    }

    const size_t begin = index->component_chain_offsets[component_id];
    const size_t end = index->component_chain_offsets[component_id + 1U];
    size_t selected_segments = 0U;

    for (size_t slot = begin; slot < end && slot < index->chain_count; slot += 1U) {
        const size_t chain_index = index->component_chains[slot];
        if (chain_index >= design->chain_count) {
            continue;
        }
        const StitchChain chain = design->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t segment_offset = index->chain_segment_offsets[chain_index];
        const size_t segment_count = chain.point_count - 1U;
        for (size_t local_segment = 0U; local_segment < segment_count; local_segment += 1U) {
            if (segment_offset + local_segment < mask_length) {
                segment_mask[segment_offset + local_segment] = target_mask_value;
                selected_segments += 1U;
            }
        }
    }
    return selected_segments;
}

bool delete_masked_segments(
    const StitchDesign *source,
    int32_t cross_pitch_units,
    const uint8_t *segment_mask,
    size_t mask_length,
    StitchDesign *result
) {
    bool succeeded = false;
    init_stitch_design(result);

    int32_t pitch = cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }

    if (source->block_count > 0U) {
        size_t block_bytes = 0U;
        if (ckd_mul(&block_bytes, source->block_count, sizeof(ColorBlock)) ||
            !design_reserve_blocks(result, source->block_count)) {
            goto cleanup;
        }
        memcpy(result->blocks, source->blocks, block_bytes);
        result->block_count = source->block_count;
    }

    size_t reserve_points = 0U;
    size_t reserve_chains = 0U;
    if (ckd_add(&reserve_points, source->point_count, (size_t)512U) ||
        ckd_add(&reserve_chains, source->chain_count, (size_t)64U) ||
        !design_reserve_points(result, reserve_points) ||
        !design_reserve_chains(result, reserve_chains)) {
        goto cleanup;
    }

    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < source->chain_count; chain_index += 1U) {
        const StitchChain chain = source->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t total_segments = chain.point_count - 1U;
        const StitchPoint *chain_points = &source->points[chain.point_offset];
        size_t local_segment = 0U;

        while (local_segment < total_segments) {
            while (local_segment < total_segments &&
                   flat_segment_index + local_segment < mask_length &&
                   segment_mask[flat_segment_index + local_segment] != 0U) {
                local_segment += 1U;
            }
            if (local_segment >= total_segments) {
                break;
            }

            const size_t run_start = local_segment;
            while (local_segment < total_segments &&
                   (flat_segment_index + local_segment >= mask_length ||
                    segment_mask[flat_segment_index + local_segment] == 0U)) {
                local_segment += 1U;
            }

            const size_t run_length = local_segment - run_start;
            const bool cut_at_start = (run_start > 0U);
            const bool cut_at_end = (local_segment < total_segments);
            if ((cut_at_start || cut_at_end) &&
                run_is_cut_artifact(chain_points, run_start, run_length, pitch)) {
                continue;
            }

            if (!append_locked_subchain(
                    result,
                    chain_points,
                    run_start,
                    run_length + 1U,
                    chain.color_block_index,
                    cut_at_start,
                    cut_at_end
                )) {
                goto cleanup;
            }
        }

        flat_segment_index += total_segments;
    }

    if (!compact_non_empty_blocks(result)) {
        goto cleanup;
    }
    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

bool recolor_masked_segments(
    const StitchDesign *source,
    const uint8_t *segment_mask,
    size_t mask_length,
    uint8_t new_janome_index,
    StitchDesign *result
) {
    bool succeeded = false;
    init_stitch_design(result);

    if (new_janome_index == 0U || new_janome_index >= JANOME_PALETTE_COUNT) {
        new_janome_index = 1U;
    }

    if (source->block_count > 0U) {
        size_t reserve_blocks = 0U;
        size_t block_bytes = 0U;
        if (ckd_add(&reserve_blocks, source->block_count, (size_t)1U) ||
            ckd_mul(&block_bytes, source->block_count, sizeof(ColorBlock)) ||
            !design_reserve_blocks(result, reserve_blocks)) {
            goto cleanup;
        }
        memcpy(result->blocks, source->blocks, block_bytes);
        result->block_count = source->block_count;
    }

    size_t target_block_index = SIZE_MAX;
    for (size_t block_index = 0U; block_index < result->block_count; block_index += 1U) {
        if (result->blocks[block_index].enabled &&
            result->blocks[block_index].janome_color_index == new_janome_index) {
            target_block_index = block_index;
            break;
        }
    }
    if (target_block_index == SIZE_MAX) {
        if (!design_append_block(
                result,
                janome_palette[new_janome_index],
                new_janome_index,
                true,
                &target_block_index
            )) {
            goto cleanup;
        }
    }

    size_t reserve_points = 0U;
    size_t reserve_chains = 0U;
    if (ckd_add(&reserve_points, source->point_count, (size_t)1024U) ||
        ckd_add(&reserve_chains, source->chain_count, (size_t)128U) ||
        !design_reserve_points(result, reserve_points) ||
        !design_reserve_chains(result, reserve_chains)) {
        goto cleanup;
    }

    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < source->chain_count; chain_index += 1U) {
        const StitchChain chain = source->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t total_segments = chain.point_count - 1U;
        const StitchPoint *chain_points = &source->points[chain.point_offset];
        size_t local_segment = 0U;

        while (local_segment < total_segments) {
            bool is_selected = false;
            if (flat_segment_index + local_segment < mask_length &&
                segment_mask[flat_segment_index + local_segment] != 0U) {
                is_selected = true;
            }
            const size_t run_start = local_segment;
            while (local_segment < total_segments) {
                bool next_selected = false;
                if (flat_segment_index + local_segment < mask_length &&
                    segment_mask[flat_segment_index + local_segment] != 0U) {
                    next_selected = true;
                }
                if (next_selected != is_selected) {
                    break;
                }
                local_segment += 1U;
            }

            const size_t run_length = local_segment - run_start;
            const bool cut_at_start = (run_start > 0U);
            const bool cut_at_end = (local_segment < total_segments);
            size_t destination_block = chain.color_block_index;
            if (is_selected) {
                destination_block = target_block_index;
            }

            if (!append_locked_subchain(
                    result,
                    chain_points,
                    run_start,
                    run_length + 1U,
                    destination_block,
                    cut_at_start,
                    cut_at_end
                )) {
                goto cleanup;
            }
        }
        flat_segment_index += total_segments;
    }

    if (!compact_non_empty_blocks(result)) {
        goto cleanup;
    }
    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

bool shift_masked_segments(
    const StitchDesign *source,
    const uint8_t *segment_mask,
    size_t mask_length,
    int32_t delta_x,
    int32_t delta_y,
    StitchDesign *result,
    uint8_t **updated_mask_out,
    size_t *updated_mask_length_out
) {
    bool succeeded = false;
    uint8_t *chain_is_selected = nullptr;
    size_t chain_flag_capacity = 0U;
    uint8_t *next_mask = nullptr;
    init_stitch_design(result);

    if (source->block_count > 0U) {
        size_t block_bytes = 0U;
        if (ckd_mul(&block_bytes, source->block_count, sizeof(ColorBlock)) ||
            !design_reserve_blocks(result, source->block_count)) {
            goto cleanup;
        }
        memcpy(result->blocks, source->blocks, block_bytes);
        result->block_count = source->block_count;
    }

    size_t reserve_points = 0U;
    size_t reserve_chains = 0U;
    if (ckd_add(&reserve_points, source->point_count, (size_t)512U) ||
        ckd_add(&reserve_chains, source->chain_count, (size_t)64U) ||
        !design_reserve_points(result, reserve_points) ||
        !design_reserve_chains(result, reserve_chains)) {
        goto cleanup;
    }

    size_t flat_segment_index = 0U;
    for (size_t chain_index = 0U; chain_index < source->chain_count; chain_index += 1U) {
        const StitchChain chain = source->chains[chain_index];
        if (chain.point_count < 2U) {
            continue;
        }
        const size_t total_segments = chain.point_count - 1U;
        const StitchPoint *chain_points = &source->points[chain.point_offset];
        size_t local_segment = 0U;

        while (local_segment < total_segments) {
            bool is_selected = false;
            if (flat_segment_index + local_segment < mask_length &&
                segment_mask[flat_segment_index + local_segment] != 0U) {
                is_selected = true;
            }
            const size_t run_start = local_segment;
            while (local_segment < total_segments) {
                bool next_selected = false;
                if (flat_segment_index + local_segment < mask_length &&
                    segment_mask[flat_segment_index + local_segment] != 0U) {
                    next_selected = true;
                }
                if (next_selected != is_selected) {
                    break;
                }
                local_segment += 1U;
            }

            const size_t run_length = local_segment - run_start;
            const bool cut_at_start = (run_start > 0U);
            const bool cut_at_end = (local_segment < total_segments);
            const size_t before_points = result->point_count;

            if (!append_locked_subchain(
                    result,
                    chain_points,
                    run_start,
                    run_length + 1U,
                    chain.color_block_index,
                    cut_at_start,
                    cut_at_end
                )) {
                goto cleanup;
            }

            if (result->chain_count > chain_flag_capacity) {
                size_t next_capacity = chain_flag_capacity;
                if (next_capacity < 64U) {
                    next_capacity = 64U;
                }
                while (next_capacity < result->chain_count) {
                    size_t doubled_capacity = 0U;
                    if (ckd_mul(&doubled_capacity, next_capacity, (size_t)2U)) {
                        next_capacity = result->chain_count;
                        break;
                    }
                    next_capacity = doubled_capacity;
                }
                uint8_t *grown_flags = (uint8_t *)realloc(chain_is_selected, next_capacity);
                if (grown_flags == nullptr) {
                    goto cleanup;
                }
                chain_is_selected = grown_flags;
                chain_flag_capacity = next_capacity;
            }
            if (result->chain_count == 0U || chain_is_selected == nullptr) {
                goto cleanup;
            }
            if (is_selected) {
                chain_is_selected[result->chain_count - 1U] = 1U;
            } else {
                chain_is_selected[result->chain_count - 1U] = 0U;
            }

            if (is_selected) {
                for (size_t point_index = before_points; point_index < result->point_count; point_index += 1U) {
                    const int64_t next_x =
                        (int64_t)result->points[point_index].position_x + (int64_t)delta_x;
                    const int64_t next_y =
                        (int64_t)result->points[point_index].position_y + (int64_t)delta_y;
                    if (next_x < -MAX_COORDINATE_MAGNITUDE || next_x > MAX_COORDINATE_MAGNITUDE ||
                        next_y < -MAX_COORDINATE_MAGNITUDE || next_y > MAX_COORDINATE_MAGNITUDE) {
                        goto cleanup;
                    }
                    result->points[point_index].position_x = (int32_t)next_x;
                    result->points[point_index].position_y = (int32_t)next_y;
                }
            }
        }
        flat_segment_index += total_segments;
    }

    if (updated_mask_out != nullptr && updated_mask_length_out != nullptr) {
        const size_t result_segments = count_design_segments(result);
        if (result_segments > 0U) {
            next_mask = (uint8_t *)calloc(result_segments, sizeof(uint8_t));
            if (next_mask == nullptr) {
                goto cleanup;
            }
            size_t write_segment = 0U;
            for (size_t chain_index = 0U; chain_index < result->chain_count; chain_index += 1U) {
                const size_t chain_segments = result->chains[chain_index].point_count - 1U;
                if (chain_is_selected != nullptr &&
                    chain_index < chain_flag_capacity &&
                    chain_is_selected[chain_index] != 0U) {
                    memset(next_mask + write_segment, 1, chain_segments);
                }
                write_segment += chain_segments;
            }
        }
        *updated_mask_out = next_mask;
        *updated_mask_length_out = result_segments;
        next_mask = nullptr;
    }

    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    free(next_mask);
    free(chain_is_selected);
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

bool move_color_block_order(StitchDesign *design, size_t block_index, int32_t direction) {
    if (design->block_count < 2U || block_index >= design->block_count) {
        return false;
    }
    if (direction < 0 && block_index == 0U) {
        return false;
    }
    if (direction > 0 && block_index + 1U >= design->block_count) {
        return false;
    }

    size_t swap_index = block_index + 1U;
    if (direction < 0) {
        swap_index = block_index - 1U;
    }

    const ColorBlock saved_block = design->blocks[block_index];
    design->blocks[block_index] = design->blocks[swap_index];
    design->blocks[swap_index] = saved_block;

    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        if (design->chains[chain_index].color_block_index == block_index) {
            design->chains[chain_index].color_block_index = swap_index;
        } else if (design->chains[chain_index].color_block_index == swap_index) {
            design->chains[chain_index].color_block_index = block_index;
        }
    }

    return sort_chains_by_color_block(design);
}

bool remove_color_block_at(StitchDesign *design, size_t target_block_index) {
    if (target_block_index >= design->block_count) {
        return false;
    }

    size_t write_chain_index = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        if (design->chains[chain_index].color_block_index != target_block_index) {
            StitchChain moved = design->chains[chain_index];
            if (moved.color_block_index > target_block_index) {
                moved.color_block_index -= 1U;
            }
            design->chains[write_chain_index] = moved;
            write_chain_index += 1U;
        }
    }
    design->chain_count = write_chain_index;

    for (size_t block_index = target_block_index + 1U;
         block_index < design->block_count;
         block_index += 1U) {
        design->blocks[block_index - 1U] = design->blocks[block_index];
    }
    design->block_count -= 1U;

    if (!compact_design_points(design)) {
        return false;
    }
    recompute_design_bounds(design);
    return true;
}

bool merge_duplicate_color_blocks(StitchDesign *design) {
    bool succeeded = false;
    size_t *block_remap = nullptr;

    if (design->block_count < 2U) {
        return true;
    }

    const size_t original_block_count = design->block_count;
    size_t first_block_for_janome[JANOME_PALETTE_COUNT] = {};
    for (size_t palette_index = 0U; palette_index < JANOME_PALETTE_COUNT; palette_index += 1U) {
        first_block_for_janome[palette_index] = SIZE_MAX;
    }

    size_t remap_bytes = 0U;
    if (ckd_mul(&remap_bytes, original_block_count, sizeof(size_t))) {
        goto cleanup;
    }
    block_remap = (size_t *)malloc(remap_bytes);
    if (block_remap == nullptr) {
        goto cleanup;
    }

    size_t merged_block_count = 0U;
    for (size_t block_index = 0U; block_index < original_block_count; block_index += 1U) {
        uint8_t janome_index = design->blocks[block_index].janome_color_index;
        if (janome_index >= JANOME_PALETTE_COUNT) {
            janome_index = 1U;
        }
        if (first_block_for_janome[janome_index] == SIZE_MAX) {
            first_block_for_janome[janome_index] = merged_block_count;
            design->blocks[merged_block_count] = design->blocks[block_index];
            block_remap[block_index] = merged_block_count;
            merged_block_count += 1U;
        } else {
            const size_t target = first_block_for_janome[janome_index];
            if (design->blocks[block_index].enabled) {
                design->blocks[target].enabled = true;
            }
            block_remap[block_index] = target;
        }
    }
    design->block_count = merged_block_count;

    size_t kept_chains = 0U;
    for (size_t chain_index = 0U; chain_index < design->chain_count; chain_index += 1U) {
        StitchChain chain = design->chains[chain_index];
        if (chain.color_block_index < original_block_count) {
            chain.color_block_index = block_remap[chain.color_block_index];
            design->chains[kept_chains] = chain;
            kept_chains += 1U;
        }
    }
    design->chain_count = kept_chains;

    if (!sort_chains_by_color_block(design) || !compact_design_points(design)) {
        goto cleanup;
    }
    recompute_design_bounds(design);
    succeeded = true;

cleanup:
    free(block_remap);
    return succeeded;
}

static bool append_cross_cell_stitches(
    StitchDesign *design,
    int32_t cell_x,
    int32_t cell_y,
    int32_t pitch_units,
    int32_t passes
) {
    if (passes < MIN_CROSS_PASSES) {
        passes = MIN_CROSS_PASSES;
    }
    if (passes > MAX_CROSS_PASSES) {
        passes = MAX_CROSS_PASSES;
    }

    const int32_t left_x = cell_x;
    const int32_t right_x = cell_x + pitch_units;
    const int32_t top_y = cell_y;
    const int32_t bottom_y = cell_y + pitch_units;
    const int32_t center_x = cell_x + pitch_units / 2;
    const int32_t center_y = cell_y + pitch_units / 2;

    const StitchPoint corners[4] = {
        {left_x, bottom_y},
        {right_x, top_y},
        {right_x, bottom_y},
        {left_x, top_y}
    };

    size_t start_corner_index = 0U;
    if (design->point_count > 0U) {
        const StitchPoint previous_point = design->points[design->point_count - 1U];
        int64_t best_distance_squared = INT64_MAX;
        for (size_t corner_index = 0U; corner_index < 4U; corner_index += 1U) {
            const int64_t delta_x =
                (int64_t)corners[corner_index].position_x - (int64_t)previous_point.position_x;
            const int64_t delta_y =
                (int64_t)corners[corner_index].position_y - (int64_t)previous_point.position_y;
            const int64_t distance_squared = delta_x * delta_x + delta_y * delta_y;
            if (distance_squared < best_distance_squared) {
                best_distance_squared = distance_squared;
                start_corner_index = corner_index;
            }
        }
    }

    StitchPoint first_start = corners[0];
    StitchPoint first_end = corners[1];
    StitchPoint second_start = corners[2];
    StitchPoint second_end = corners[3];
    if (start_corner_index == 1U) {
        first_start = corners[1];
        first_end = corners[0];
        second_start = corners[3];
        second_end = corners[2];
    } else if (start_corner_index == 2U) {
        first_start = corners[2];
        first_end = corners[3];
        second_start = corners[0];
        second_end = corners[1];
    } else if (start_corner_index == 3U) {
        first_start = corners[3];
        first_end = corners[2];
        second_start = corners[1];
        second_end = corners[0];
    }

    if (!design_append_point(design, first_start.position_x, first_start.position_y)) {
        return false;
    }
    for (int32_t pass_index = 0; pass_index < passes; pass_index += 1) {
        if ((pass_index % 2) == 0) {
            if (!design_append_point(design, first_end.position_x, first_end.position_y)) {
                return false;
            }
        } else {
            if (!design_append_point(design, first_start.position_x, first_start.position_y)) {
                return false;
            }
        }
    }

    /* Route to second diagonal through cell center so the travel stays hidden under the cross. */
    if (!design_append_point(design, center_x, center_y) ||
        !design_append_point(design, second_start.position_x, second_start.position_y)) {
        return false;
    }
    for (int32_t pass_index = 0; pass_index < passes; pass_index += 1) {
        if ((pass_index % 2) == 0) {
            if (!design_append_point(design, second_end.position_x, second_end.position_y)) {
                return false;
            }
        } else {
            if (!design_append_point(design, second_start.position_x, second_start.position_y)) {
                return false;
            }
        }
    }
    return true;
}

bool draw_single_cross_into_design(
    StitchDesign *design,
    CrossGridInfo grid,
    int32_t cell_column,
    int32_t cell_row,
    size_t color_block_index
) {
    bool succeeded = false;
    StitchDesign scratch = {};

    if (color_block_index >= design->block_count) {
        goto cleanup;
    }

    int32_t pitch = grid.cross_pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = DEFAULT_CROSS_PITCH_UNITS;
    }
    const int32_t cell_x = grid.grid_origin_x + cell_column * pitch;
    const int32_t cell_y = grid.grid_origin_y + cell_row * pitch;

    if (!append_cross_cell_stitches(&scratch, cell_x, cell_y, pitch, grid.cross_passes)) {
        goto cleanup;
    }

    if (!append_locked_subchain(
            design,
            scratch.points,
            0U,
            scratch.point_count,
            color_block_index,
            true,
            true
        )) {
        goto cleanup;
    }

    /* Insert newly appended chain contiguously into its color block without allocating. */
    if (design->chain_count >= 2U) {
        const StitchChain new_chain = design->chains[design->chain_count - 1U];
        size_t insert_index = design->chain_count - 1U;
        while (insert_index > 0U &&
               design->chains[insert_index - 1U].color_block_index > color_block_index) {
            insert_index -= 1U;
        }
        if (insert_index < design->chain_count - 1U) {
            const size_t move_count = (design->chain_count - 1U) - insert_index;
            memmove(
                &design->chains[insert_index + 1U],
                &design->chains[insert_index],
                move_count * sizeof(StitchChain)
            );
            design->chains[insert_index] = new_chain;
        }
    }

    int64_t min_x = (int64_t)design->minimum_x;
    int64_t max_x = (int64_t)design->maximum_x;
    int64_t min_y = (int64_t)design->minimum_y;
    int64_t max_y = (int64_t)design->maximum_y;
    bool has_bounds = design->has_bounds;
    const StitchChain last_added = design->chains[design->chain_count - 1U];
    if (last_added.color_block_index == color_block_index) {
        expand_bounds_from_chain(design, last_added, &has_bounds, &min_x, &max_x, &min_y, &max_y);
        design->has_bounds = has_bounds;
        design->minimum_x = (int32_t)min_x;
        design->maximum_x = (int32_t)max_x;
        design->minimum_y = (int32_t)min_y;
        design->maximum_y = (int32_t)max_y;
    } else {
        recompute_design_bounds(design);
    }
    design_mark_mutated(design);
    succeeded = true;

cleanup:
    free_stitch_design(&scratch);
    return succeeded;
}

static bool trace_strip_color_chain(
    const uint8_t *expanded_cells,
    uint8_t *visited_cells,
    size_t total_columns,
    size_t total_rows,
    size_t start_column,
    size_t start_row,
    uint8_t active_color,
    int32_t pitch,
    int32_t passes,
    StitchDesign *temporary_chain_points
) {
    /* Prioritize horizontal row continuity before vertical/diagonal steps. */
    static const int32_t neighbor_delta_column[8] = {1, -1, 0, 0, 1, -1, 1, -1};
    static const int32_t neighbor_delta_row[8] = {0, 0, 1, -1, 1, 1, -1, -1};

    temporary_chain_points->point_count = 0U;
    size_t current_column = start_column;
    size_t current_row = start_row;

    while (true) {
        visited_cells[current_row * total_columns + current_column] = 1U;
        if (!append_cross_cell_stitches(
                temporary_chain_points,
                (int32_t)current_column * pitch,
                (int32_t)current_row * pitch,
                pitch,
                passes
            )) {
            return false;
        }

        bool found_next = false;
        for (size_t direction_index = 0U; direction_index < 8U; direction_index += 1U) {
            const int64_t next_column =
                (int64_t)current_column + (int64_t)neighbor_delta_column[direction_index];
            const int64_t next_row =
                (int64_t)current_row + (int64_t)neighbor_delta_row[direction_index];
            if (next_column < 0LL || (size_t)next_column >= total_columns ||
                next_row < 0LL || (size_t)next_row >= total_rows) {
                continue;
            }
            const size_t next_flat = (size_t)next_row * total_columns + (size_t)next_column;
            if (expanded_cells[next_flat] == active_color && visited_cells[next_flat] == 0U) {
                current_column = (size_t)next_column;
                current_row = (size_t)next_row;
                found_next = true;
                break;
            }
        }
        if (!found_next) {
            break;
        }
    }
    return true;
}

bool build_strip_design(const StripPattern *pattern, StitchDesign *result) {
    bool succeeded = false;
    uint8_t *expanded_cells = nullptr;
    uint8_t *visited_cells = nullptr;
    StitchDesign temporary_chain_points = {};
    init_stitch_design(result);

    if (pattern->width < 1 || pattern->width > STRIP_MAX_GRID_DIM ||
        pattern->height < 1 || pattern->height > STRIP_MAX_GRID_DIM ||
        pattern->repeat_x < 1 || pattern->repeat_x > STRIP_MAX_REPEAT ||
        pattern->repeat_y < 1 || pattern->repeat_y > STRIP_MAX_REPEAT) {
        (void)fprintf(stderr, "Invalid strip pattern dimensions\n");
        goto cleanup;
    }

    size_t total_columns = 0U;
    size_t total_rows = 0U;
    size_t total_cells = 0U;
    if (ckd_mul(&total_columns, (size_t)pattern->width, (size_t)pattern->repeat_x) ||
        ckd_mul(&total_rows, (size_t)pattern->height, (size_t)pattern->repeat_y) ||
        ckd_mul(&total_cells, total_columns, total_rows) ||
        total_cells > STRIP_MAX_EXPANDED_CELLS) {
        (void)fprintf(stderr, "Expanded strip exceeds cell limit\n");
        goto cleanup;
    }

    expanded_cells = (uint8_t *)calloc(total_cells, sizeof(uint8_t));
    visited_cells = (uint8_t *)calloc(total_cells, sizeof(uint8_t));
    if (expanded_cells == nullptr || visited_cells == nullptr) {
        (void)fprintf(stderr, "Failed to allocate strip expansion buffers\n");
        goto cleanup;
    }

    uint8_t used_colors[JANOME_PALETTE_COUNT] = {};
    size_t used_color_count = 0U;
    bool color_seen[JANOME_PALETTE_COUNT] = {};

    for (size_t row = 0U; row < total_rows; row += 1U) {
        const size_t source_row = row % (size_t)pattern->height;
        for (size_t column = 0U; column < total_columns; column += 1U) {
            const size_t source_column = column % (size_t)pattern->width;
            const uint8_t color_index =
                pattern->cells[source_row * STRIP_MAX_GRID_DIM + source_column];
            if (color_index > 0U && color_index < JANOME_PALETTE_COUNT) {
                expanded_cells[row * total_columns + column] = color_index;
                if (!color_seen[color_index]) {
                    color_seen[color_index] = true;
                    used_colors[used_color_count] = color_index;
                    used_color_count += 1U;
                }
            }
        }
    }

    if (used_color_count == 0U) {
        (void)fprintf(stderr, "Strip pattern contains no crosses\n");
        goto cleanup;
    }

    int32_t pitch = pattern->pitch_units;
    if (pitch < MIN_CROSS_PITCH_UNITS) {
        pitch = MIN_CROSS_PITCH_UNITS;
    }
    if (pitch > MAX_CROSS_PITCH_UNITS) {
        pitch = MAX_CROSS_PITCH_UNITS;
    }

    for (size_t color_order = 0U; color_order < used_color_count; color_order += 1U) {
        const uint8_t active_color = used_colors[color_order];
        size_t block_index = 0U;
        if (!design_append_block(
                result,
                janome_palette[active_color],
                active_color,
                true,
                &block_index
            )) {
            goto cleanup;
        }

        for (size_t start_cell = 0U; start_cell < total_cells; start_cell += 1U) {
            if (expanded_cells[start_cell] != active_color || visited_cells[start_cell] != 0U) {
                continue;
            }
            const size_t start_row = start_cell / total_columns;
            const size_t start_column = start_cell % total_columns;
            if (!trace_strip_color_chain(
                    expanded_cells,
                    visited_cells,
                    total_columns,
                    total_rows,
                    start_column,
                    start_row,
                    active_color,
                    pitch,
                    pattern->passes,
                    &temporary_chain_points
                )) {
                goto cleanup;
            }
            if (!append_locked_subchain(
                    result,
                    temporary_chain_points.points,
                    0U,
                    temporary_chain_points.point_count,
                    block_index,
                    true,
                    true
                )) {
                goto cleanup;
            }
        }
    }

    /* Center by whole pitch cells so all cell corners stay exact multiples of pitch for grid snapping. */
    const int32_t center_shift_x = -(int32_t)(total_columns / 2U) * pitch;
    const int32_t center_shift_y = -(int32_t)(total_rows / 2U) * pitch;
    for (size_t point_index = 0U; point_index < result->point_count; point_index += 1U) {
        result->points[point_index].position_x += center_shift_x;
        result->points[point_index].position_y += center_shift_y;
    }
    result->has_manual_center_shift = false;
    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    free_stitch_design(&temporary_chain_points);
    free(visited_cells);
    free(expanded_cells);
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

bool insert_design_fragment(
    const StitchDesign *base_design,
    const StitchDesign *fragment,
    int32_t offset_x,
    int32_t offset_y,
    StitchDesign *result
) {
    bool succeeded = false;
    if (!clone_stitch_design(base_design, result)) {
        return false;
    }

    if (fragment->block_count == 0U || fragment->chain_count == 0U) {
        return true;
    }

    const size_t base_block_count = result->block_count;
    for (size_t block_index = 0U; block_index < fragment->block_count; block_index += 1U) {
        const ColorBlock block = fragment->blocks[block_index];
        if (!design_append_block(
                result,
                block.display_color,
                block.janome_color_index,
                block.enabled,
                nullptr
            )) {
            goto cleanup;
        }
    }

    for (size_t chain_index = 0U; chain_index < fragment->chain_count; chain_index += 1U) {
        const StitchChain chain = fragment->chains[chain_index];
        if (chain.color_block_index >= fragment->block_count ||
            !fragment->blocks[chain.color_block_index].enabled ||
            chain.point_count < 2U) {
            continue;
        }
        const size_t destination_point_offset = result->point_count;
        for (size_t local_index = 0U; local_index < chain.point_count; local_index += 1U) {
            const StitchPoint source_point = fragment->points[chain.point_offset + local_index];
            const int64_t placed_x = (int64_t)source_point.position_x + (int64_t)offset_x;
            const int64_t placed_y = (int64_t)source_point.position_y + (int64_t)offset_y;
            if (placed_x < -MAX_COORDINATE_MAGNITUDE || placed_x > MAX_COORDINATE_MAGNITUDE ||
                placed_y < -MAX_COORDINATE_MAGNITUDE || placed_y > MAX_COORDINATE_MAGNITUDE) {
                goto cleanup;
            }
            if (!design_append_point(result, (int32_t)placed_x, (int32_t)placed_y)) {
                goto cleanup;
            }
        }
        if (!design_append_chain(
                result,
                destination_point_offset,
                chain.point_count,
                base_block_count + chain.color_block_index,
                chain.start_was_cut,
                chain.end_was_cut
            )) {
            goto cleanup;
        }
    }

    if (!compact_non_empty_blocks(result)) {
        goto cleanup;
    }
    recompute_design_bounds(result);
    succeeded = true;

cleanup:
    if (!succeeded) {
        free_stitch_design(result);
    }
    return succeeded;
}

void rotate_design_90_clockwise(StitchDesign *design) {
    if (!design_ensure_unique_points(design)) {
        return;
    }
    for (size_t point_index = 0U; point_index < design->point_count; point_index += 1U) {
        const int32_t old_x = design->points[point_index].position_x;
        const int32_t old_y = design->points[point_index].position_y;
        design->points[point_index].position_x = -old_y;
        design->points[point_index].position_y = old_x;
    }
    design_mark_mutated(design);
    recompute_design_bounds(design);
}

void mirror_design_horizontal(StitchDesign *design) {
    if (!design_ensure_unique_points(design)) {
        return;
    }
    for (size_t point_index = 0U; point_index < design->point_count; point_index += 1U) {
        design->points[point_index].position_x = -design->points[point_index].position_x;
    }
    design_mark_mutated(design);
    recompute_design_bounds(design);
}

void mirror_design_vertical(StitchDesign *design) {
    if (!design_ensure_unique_points(design)) {
        return;
    }
    for (size_t point_index = 0U; point_index < design->point_count; point_index += 1U) {
        design->points[point_index].position_y = -design->points[point_index].position_y;
    }
    design_mark_mutated(design);
    recompute_design_bounds(design);
}

void shift_design_center(StitchDesign *design, int32_t delta_x, int32_t delta_y) {
    if (!design_ensure_unique_points(design)) {
        return;
    }
    for (size_t point_index = 0U; point_index < design->point_count; point_index += 1U) {
        const int64_t next_x = (int64_t)design->points[point_index].position_x + (int64_t)delta_x;
        const int64_t next_y = (int64_t)design->points[point_index].position_y + (int64_t)delta_y;
        if (next_x < -MAX_COORDINATE_MAGNITUDE || next_x > MAX_COORDINATE_MAGNITUDE ||
            next_y < -MAX_COORDINATE_MAGNITUDE || next_y > MAX_COORDINATE_MAGNITUDE) {
            return;
        }
    }
    for (size_t point_index = 0U; point_index < design->point_count; point_index += 1U) {
        design->points[point_index].position_x += delta_x;
        design->points[point_index].position_y += delta_y;
    }
    design->has_manual_center_shift = true;
    design_mark_mutated(design);
    recompute_design_bounds(design);
}

void history_init(EditHistory *history) {
    *history = (EditHistory){};
}

void history_clear(EditHistory *history) {
    for (size_t state_index = 0U; state_index < history->state_count; state_index += 1U) {
        free_stitch_design(&history->states[state_index]);
    }
    history->state_count = 0U;
    history->current_index = 0U;
}

/* Sum metadata + unique SharedPointBuffer memory across all stored history snapshots. */
size_t history_estimated_bytes(const EditHistory *history) {
    size_t total_bytes = 0U;
    for (size_t state_index = 0U; state_index < history->state_count; state_index += 1U) {
        const StitchDesign *entry = &history->states[state_index];
        const size_t metadata = design_metadata_bytes(entry);
        if (ckd_add(&total_bytes, total_bytes, metadata)) {
            return SIZE_MAX;
        }
        if (entry->point_buffer == nullptr) {
            continue;
        }
        bool already_counted = false;
        for (size_t prior = 0U; prior < state_index; prior += 1U) {
            if (history->states[prior].point_buffer == entry->point_buffer) {
                already_counted = true;
                break;
            }
        }
        if (!already_counted) {
            const size_t point_bytes = design_point_buffer_bytes(entry);
            if (ckd_add(&total_bytes, total_bytes, point_bytes)) {
                return SIZE_MAX;
            }
        }
    }
    return total_bytes;
}

static void history_evict_oldest(EditHistory *history) {
    if (history->state_count <= 1U) {
        return;
    }
    free_stitch_design(&history->states[0]);
    for (size_t shift_index = 1U; shift_index < history->state_count; shift_index += 1U) {
        history->states[shift_index - 1U] = history->states[shift_index];
    }
    init_stitch_design(&history->states[history->state_count - 1U]);
    history->state_count -= 1U;
    if (history->current_index > 0U) {
        history->current_index -= 1U;
    }
}

bool history_push(EditHistory *history, const StitchDesign *design) {
    StitchDesign snapshot = {};
    if (!clone_stitch_design(design, &snapshot)) {
        return false;
    }

    if (history->state_count > 0U && history->current_index + 1U < history->state_count) {
        for (size_t drop_index = history->current_index + 1U;
             drop_index < history->state_count;
             drop_index += 1U) {
            free_stitch_design(&history->states[drop_index]);
        }
        history->state_count = history->current_index + 1U;
    }

    if (history->state_count == MAX_HISTORY_STATES) {
        history_evict_oldest(history);
    }

    history->states[history->state_count] = snapshot;
    history->current_index = history->state_count;
    history->state_count += 1U;

    while (history->state_count > 1U && history_estimated_bytes(history) > MAX_HISTORY_BYTES) {
        history_evict_oldest(history);
    }
    return true;
}

bool history_can_undo(const EditHistory *history) {
    return history->state_count > 0U && history->current_index > 0U;
}

bool history_can_redo(const EditHistory *history) {
    return history->current_index + 1U < history->state_count;
}

/* Transactional undo: clone target snapshot first so current design is preserved on OOM. */
bool history_undo(EditHistory *history, StitchDesign *design) {
    if (!history_can_undo(history)) {
        return false;
    }
    const size_t target_index = history->current_index - 1U;
    StitchDesign restored = {};
    if (!clone_stitch_design(&history->states[target_index], &restored)) {
        return false;
    }
    free_stitch_design(design);
    *design = restored;
    history->current_index = target_index;
    return true;
}

/* Transactional redo: clone target snapshot first so current design is preserved on OOM. */
bool history_redo(EditHistory *history, StitchDesign *design) {
    if (!history_can_redo(history)) {
        return false;
    }
    const size_t target_index = history->current_index + 1U;
    StitchDesign restored = {};
    if (!clone_stitch_design(&history->states[target_index], &restored)) {
        return false;
    }
    free_stitch_design(design);
    *design = restored;
    history->current_index = target_index;
    return true;
}
