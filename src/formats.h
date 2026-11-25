#ifndef EMBROIDERY_FORMATS_H
#define EMBROIDERY_FORMATS_H

#include "model.h"

EmbroideryFormat detect_embroidery_format(
    const char *input_path,
    const uint8_t *file_buffer,
    size_t file_size
);
bool parse_jef_design(
    const uint8_t *file_buffer,
    size_t file_size,
    StitchDesign *design
);
bool parse_pes_design(
    const uint8_t *file_buffer,
    size_t file_size,
    StitchDesign *design
);
bool parse_pec_design(
    const uint8_t *file_buffer,
    size_t file_size,
    StitchDesign *design
);
bool parse_dst_design(
    const uint8_t *file_buffer,
    size_t file_size,
    StitchDesign *design
);
bool load_embroidery_file(const char *input_path, StitchDesign *design);
bool write_jef_file(const StitchDesign *design, const char *output_path);
bool build_suffixed_jef_path(
    const char *base_path,
    const char *suffix,
    char *output_path,
    size_t output_capacity
);
bool file_already_exists(const char *file_path);
bool build_unique_suffixed_jef_path(
    const char *base_path,
    const char *suffix,
    char *output_path,
    size_t output_capacity
);

#endif
