/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_drm_uapi.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    struct mxgpu_drm_batch_response response, decoded, before;
    uint8_t wire[1100], damaged[1100];
    union {
        max_align_t alignment;
        uint8_t bytes[4096];
    } overlap;
    const uint32_t counts[] = {1, 16, 17, 32};
    uint32_t c, i, bit, bytes, mutations = 0;
    for (c = 0; c < sizeof counts / sizeof counts[0]; c++) {
        memset(&response, 0, sizeof response);
        response.context_id = 7;
        response.count = counts[c];
        response.device_generation = 9;
        response.aggregate_result = MXGPU_DRM_BATCH_COMPLETE;
        for (i = 0; i < response.count; i++) {
            response.outcomes[i].state = MXGPU_DRM_BATCH_COMPLETED;
            response.outcomes[i].sequence = i + 1;
            response.outcomes[i].fence_value = i + 10;
            response.outcomes[i].device_generation = response.device_generation;
        }
        assert(mxgpu_drm_batch_response_encode(&response, wire, sizeof wire, &bytes) ==
               MXGPU_DRM_OK);
        assert(mxgpu_drm_batch_response_decode(wire, bytes, &decoded) == MXGPU_DRM_OK);
        assert(memcmp(&decoded, &response, sizeof decoded) == 0);
        memset(&before, 0x6b, sizeof before);
        for (i = 0; i < bytes; i++) {
            decoded = before;
            assert(mxgpu_drm_batch_response_decode(wire, i, &decoded) != MXGPU_DRM_OK);
            assert(memcmp(&decoded, &before, sizeof decoded) == 0);
            mutations++;
        }
        memcpy(damaged, wire, bytes);
        for (i = 0; i < bytes; i++) {
            for (bit = 0; bit < 8; bit++) {
                damaged[i] ^= (uint8_t)(1u << bit);
                decoded = before;
                if (mxgpu_drm_batch_response_decode(damaged, bytes, &decoded) != MXGPU_DRM_OK)
                    assert(memcmp(&decoded, &before, sizeof decoded) == 0);
                damaged[i] ^= (uint8_t)(1u << bit);
                mutations++;
            }
        }
        for (i = 0; i < 3; i++) {
            uint32_t offset = i == 0 ? 1024 : i == 1 ? 1088 : 960;
            struct mxgpu_drm_batch_response *alias = (void *)(overlap.bytes + offset);
            memcpy(overlap.bytes + 1024, wire, bytes);
            assert(mxgpu_drm_batch_response_decode(overlap.bytes + 1024, bytes, alias) ==
                   MXGPU_DRM_OK);
            assert(memcmp(alias, &response, sizeof response) == 0);
        }
        if (argc == 2 && response.count == 32) {
            FILE *file = fopen(argv[1], "wb");
            assert(file && fwrite(wire, 1, bytes, file) == bytes);
            assert(fclose(file) == 0);
        }
    }
    printf("batch responses, %u mutations, atomic refusal and overlapping buffers PASS\n",
           mutations);
    return 0;
}
