/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_drm_uapi.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static void put(uint8_t *p, uint32_t x)
{
    p[0] = x;
    p[1] = x >> 8;
    p[2] = x >> 16;
    p[3] = x >> 24;
}
int main(void)
{
    uint8_t commands[65], tiny[1100], before[1100];
    uint8_t *buf = malloc(MXGPU_DRM_BATCH_MAX_INPUT_BYTES + 1u);
    uint8_t *large = malloc(MXGPU_DRM_BATCH_MAX_COMMAND_BYTES + 1u);
    struct mxgpu_drm_batch v = {0}, decoded = {0}, poison;
    struct mxgpu_drm_batch_response r = {0}, got = {0}, old;
    struct mxgpu_drm_batch_limits limits = {1, 1, 32, 1048576, 1049376, 32}, gl;
    uint32_t n, i, size;
    assert(buf && large);
    memset(commands, 0x5c, sizeof commands);
    memset(large, 0x6d, 1048577);
    v.context_id = 7;
    v.count = 32;
    for (i = 0; i < 32; i++) {
        v.commands[i].queue = i % 2 ? 3 : 1;
        v.commands[i].command = commands;
        v.commands[i].command_bytes = 33 + i;
        v.commands[i].fence_value = i + 1;
    }
    assert(!mxgpu_drm_batch_encode(&v, buf, MXGPU_DRM_BATCH_MAX_INPUT_BYTES, &n));
    assert(!mxgpu_drm_batch_decode(buf, n, &decoded));
    assert(decoded.count == 32 && decoded.context_id == 7);
    for (i = 0; i < 32; i++)
        assert(decoded.commands[i].queue == v.commands[i].queue &&
               decoded.commands[i].fence_value == i + 1 &&
               decoded.commands[i].command_bytes == 33 + i &&
               !memcmp(decoded.commands[i].command, commands, 33 + i));
    memset(&poison, 0xa5, sizeof poison);
    decoded = poison;
    for (i = 0; i < 32; i++) {
        uint32_t off = 32u + i * 24u + 12u;
        put(buf + off, 1);
        assert(mxgpu_drm_batch_decode(buf, n, &decoded) == MXGPU_DRM_ERR_RESERVED);
        assert(!memcmp(&decoded, &poison, sizeof decoded));
        put(buf + off, 0);
    }
    put(buf + 28, 1);
    assert(mxgpu_drm_batch_decode(buf, n, &decoded) == MXGPU_DRM_ERR_RESERVED);
    put(buf + 28, 0);
    put(buf + 12, 1);
    assert(mxgpu_drm_batch_decode(buf, n, &decoded) == MXGPU_DRM_ERR_RESERVED);
    put(buf + 12, 0);
    put(buf + 36, 799);
    assert(mxgpu_drm_batch_decode(buf, n, &decoded));
    assert(!memcmp(&decoded, &poison, sizeof decoded));
    put(buf + 36, 800);

    put(buf + 36, 801);
    assert(mxgpu_drm_batch_decode(buf, n, &decoded));
    assert(!memcmp(&decoded, &poison, sizeof decoded));
    put(buf + 36, 800);
    put(buf + 44, 1);
    assert(mxgpu_drm_batch_decode(buf, n, &decoded) == MXGPU_DRM_ERR_RESERVED);
    put(buf + 44, 0);
    put(buf + 40, 0xffffffffu);
    assert(mxgpu_drm_batch_decode(buf, n, &decoded));
    put(buf + 40, 33);
    put(buf + 8, 1);
    assert(mxgpu_drm_batch_decode(buf, n, &decoded) == MXGPU_DRM_ERR_RESERVED);
    put(buf + 8, 0);
    assert(mxgpu_drm_batch_decode(buf, n - 1, &decoded));
    put(buf + 4, n + 1);
    assert(mxgpu_drm_batch_decode(buf, n + 1, &decoded));
    for (int shift = -1; shift <= 1; shift++) {
        uint8_t actual[160], expected[160], header[160];
        struct mxgpu_drm_batch overlap = {0};
        uint32_t written;
        for (i = 0; i < 160; i++)
            actual[i] = expected[i] = (uint8_t)(i * 37u + 19u);
        overlap.context_id = 7;
        overlap.count = 1;
        overlap.commands[0].queue = 3;
        overlap.commands[0].command_bytes = 65;
        overlap.commands[0].fence_value = 5;
        overlap.commands[0].command = commands;
        assert(!mxgpu_drm_batch_encode(&overlap, header, sizeof header, &written));
        memcpy(expected, header, 56);
        for (i = 0; i < 65; i++)
            expected[56 + i] = expected[56 + shift + i];
        overlap.commands[0].command = actual + 56 + shift;
        assert(!mxgpu_drm_batch_encode(&overlap, actual, sizeof actual, &written));
        assert(!memcmp(actual, expected, sizeof actual));
    }
    v.count = 1;
    v.commands[0].command = large;
    v.commands[0].command_bytes = 1048576;
    assert(!mxgpu_drm_batch_encode(&v, buf, MXGPU_DRM_BATCH_MAX_INPUT_BYTES, &n));
    assert(n == 1048632);
    assert(!mxgpu_drm_batch_decode(buf, n, &decoded));
    memset(tiny, 0xa5, sizeof tiny);
    memcpy(before, tiny, sizeof tiny);
    v.commands[0].command_bytes = 1048577;
    n = 9;
    assert(mxgpu_drm_batch_encode(&v, tiny, sizeof tiny, &n) == MXGPU_DRM_ERR_RANGE && !n &&
           !memcmp(tiny, before, sizeof tiny));
    v.commands[0].command_bytes = 33;
    for (i = 0; i < 3; i++) {
        v.count = i == 0 ? 0 : 33;
        n = 9;
        assert(mxgpu_drm_batch_encode(&v, tiny, sizeof tiny, &n) && !n &&
               !memcmp(tiny, before, sizeof tiny));
    }
    v.count = 1;
    v.commands[0].queue = 2;
    assert(mxgpu_drm_batch_encode(&v, tiny, sizeof tiny, &n));
    assert(!mxgpu_drm_get_batch_limits_response_encode(&limits, tiny, sizeof tiny, &n));
    assert(!mxgpu_drm_get_batch_limits_response_decode(tiny, n, &gl) && gl.max_commands == 32);
    tiny[8] = 1;
    assert(mxgpu_drm_get_batch_limits_response_decode(tiny, n, &gl) == MXGPU_DRM_ERR_RESERVED);
    tiny[8] = 0;
    assert(!mxgpu_drm_get_batch_limits_encode(tiny, sizeof tiny, &n));
    assert(!mxgpu_drm_get_batch_limits_decode(tiny, n));
    tiny[8] = 1;
    assert(mxgpu_drm_get_batch_limits_decode(tiny, n) == MXGPU_DRM_ERR_RESERVED);
    tiny[8] = 0;
    limits.max_commands = 0;
    limits.max_command_bytes = 0;
    limits.max_input_bytes = 0;
    assert(!mxgpu_drm_get_batch_limits_response_encode(&limits, tiny, sizeof tiny, &n));
    assert(!mxgpu_drm_get_batch_limits_response_decode(tiny, n, &gl) && !gl.max_commands);
    limits.outcome_bytes = 31;
    assert(mxgpu_drm_get_batch_limits_response_encode(&limits, tiny, sizeof tiny, &n));
    r.context_id = 7;
    r.count = 32;
    r.device_generation = 9;
    for (i = 0; i < 32; i++) {
        r.outcomes[i].state = 2;
        r.outcomes[i].sequence = i + 1;
        r.outcomes[i].fence_value = i + 1;
        r.outcomes[i].device_generation = 9;
    }
    assert(!mxgpu_drm_batch_response_encode(&r, tiny, sizeof tiny, &n));
    assert(n == 1056);
    assert(!mxgpu_drm_batch_response_decode(tiny, n, &got) && got.count == 32 &&
           got.outcomes[31].sequence == 32);
    old = got;
    put(tiny + 60, 1);
    assert(mxgpu_drm_batch_response_decode(tiny, n, &got) == MXGPU_DRM_ERR_RESERVED &&
           !memcmp(&got, &old, sizeof got));
    r.aggregate_result = 2;
    r.count = 4;
    r.outcomes[0].state = 1;
    r.outcomes[2].status = 4;
    memset(&r.outcomes[3], 0, sizeof r.outcomes[3]);
    assert(!mxgpu_drm_batch_response_encode(&r, tiny, sizeof tiny, &n));
    assert(!mxgpu_drm_batch_response_decode(tiny, n, &got) && got.outcomes[0].state == 1 &&
           got.outcomes[2].status == 4);
    r.outcomes[2].device_generation = 8;
    assert(mxgpu_drm_batch_response_encode(&r, tiny, sizeof tiny, &n));
    r.outcomes[2].device_generation = 9;
    r.outcomes[0].status = 4;
    assert(mxgpu_drm_batch_response_encode(&r, tiny, sizeof tiny, &n));
    r.outcomes[0].status = 0;
    memset(&r.outcomes[0], 0, sizeof r.outcomes[0]);
    assert(mxgpu_drm_batch_response_encode(&r, tiny, sizeof tiny, &n));
    memset(r.outcomes, 0, sizeof r.outcomes);
    r.aggregate_result = 1;
    assert(!mxgpu_drm_batch_response_encode(&r, tiny, sizeof tiny, &size));
    assert(!mxgpu_drm_batch_response_decode(tiny, size, &got));
    free(buf);
    free(large);
    puts(
        "PASS batch canonical exact/odd/1MiB/32 vectors, malformed framing, atomic refusal, limits fallback, terminal out-of-order outcomes");
    return 0;
}
