#include "formats.h"
#include "model.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    StitchDesign design = {};
    bool parsed = false;

#if defined(FUZZ_TARGET_JEF)
    parsed = parse_jef_design(data, size, &design);
#elif defined(FUZZ_TARGET_PES)
    parsed = parse_pes_design(data, size, &design);
#elif defined(FUZZ_TARGET_PEC)
    parsed = parse_pec_design(data, size, &design);
#elif defined(FUZZ_TARGET_DST)
    parsed = parse_dst_design(data, size, &design);
#else
    if (size > 0U) {
        const uint8_t selector = data[0] % 4U;
        const uint8_t *payload = data + 1U;
        const size_t payload_size = size - 1U;
        if (selector == 0U) {
            parsed = parse_jef_design(payload, payload_size, &design);
        } else if (selector == 1U) {
            parsed = parse_pes_design(payload, payload_size, &design);
        } else if (selector == 2U) {
            parsed = parse_pec_design(payload, payload_size, &design);
        } else {
            parsed = parse_dst_design(payload, payload_size, &design);
        }
    }
#endif

    if (parsed) {
        recompute_design_bounds(&design);
        (void)compute_design_stats(&design);
        (void)count_design_segments(&design);
    }

    free_stitch_design(&design);
    return 0;
}
