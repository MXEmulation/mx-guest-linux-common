/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_drm_uapi.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "fail: %s\n", msg);
        failures++;
    }
}

static uint32_t load_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void poison(uint8_t *p, uint32_t n)
{
    memset(p, 0xa5, n);
}

static int still_poison(const uint8_t *p, uint32_t n)
{
    uint32_t i;

    for (i = 0; i < n; i++) {
        if (p[i] != 0xa5)
            return 0;
    }
    return 1;
}

static void check_header(const uint8_t *buf, uint32_t out_len, uint32_t kind, uint32_t cap)
{
    expect(out_len >= MXGPU_DRM_HEADER_BYTES, "record shorter than header");
    expect(load_u32(buf + 0) == kind, "kind");
    expect(load_u32(buf + 4) == out_len, "byte_len");
    expect(load_u32(buf + 8) == 0, "flags written as zero");
    expect(load_u32(buf + 12) == 0, "reserved written as zero");
    if (out_len < cap)
        expect(buf[out_len] == 0xa5, "bytes past record stayed poison");
}

static void check_refused(int rc, int want, uint32_t out_len, const uint8_t *buf, uint32_t cap,
                          const char *msg)
{
    expect(rc == want, msg);
    expect(out_len == 0, "refusal clears out_len");
    expect(still_poison(buf, cap), "refusal leaves poison");
}

int main(void)
{
    uint8_t buf[256];
    uint8_t command[5] = {0x10, 0x00, 0x7f, 0x80, 0xff};
    uint8_t response[3] = {0x01, 0x02, 0x03};
    uint8_t data[7] = {0xaa, 0x00, 0x01, 0x7f, 0x80, 0xfe, 0xff};
    uint32_t out_len;
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint32_t context_id;
    uint32_t handle;
    uint32_t queue;
    uint32_t response_cap;
    uint32_t command_bytes;
    uint32_t completion_status;
    uint32_t response_written;
    uint32_t timeout_ms;
    uint32_t size;
    uint64_t fence_value;
    uint64_t completed_fence;
    uint64_t gem_size;
    uint64_t offset;
    const uint8_t *command_out;
    const uint8_t *response_out;
    const uint8_t *data_out;
    int rc;

    expect(MXGPU_DRIVER_VERSION_MAJOR == 1, "driver major");
    expect(MXGPU_DRIVER_VERSION_MINOR == 0, "driver minor");
    expect(MXGPU_DRIVER_VERSION_PATCH == 0, "driver patch");
    expect(strcmp(MXGPU_DRIVER_VERSION, "1.0.0") == 0, "driver version string");

    poison(buf, sizeof buf);
    out_len = 0xffffffffu;
    rc = mxgpu_drm_get_info_encode(buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "get_info encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES, "get_info length");
    check_header(buf, out_len, MXGPU_DRM_KIND_GET_INFO, sizeof buf);
    rc = mxgpu_drm_get_info_response_decode(buf, out_len, &major, &minor, &patch);
    expect(rc == MXGPU_DRM_ERR_LENGTH, "get_info request is not a response");

    poison(buf, sizeof buf);
    out_len = 0xffffffffu;
    rc = mxgpu_drm_get_info_response_encode(MXGPU_DRIVER_VERSION_MAJOR, MXGPU_DRIVER_VERSION_MINOR,
                                            MXGPU_DRIVER_VERSION_PATCH, buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "get_info response encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 12u, "get_info response length");
    check_header(buf, out_len, MXGPU_DRM_KIND_GET_INFO, sizeof buf);
    major = 0;
    minor = 0;
    patch = 9;
    rc = mxgpu_drm_get_info_response_decode(buf, out_len, &major, &minor, &patch);
    expect(rc == MXGPU_DRM_OK, "get_info response decode");
    expect(major == 1 && minor == 0 && patch == 0, "driver version triple decodes as 1, 0, 0");
    rc = mxgpu_drm_get_info_response_decode(buf, out_len + 8u, &major, &minor, &patch);
    expect(rc == MXGPU_DRM_OK, "longer input buffer still decodes");

    poison(buf, sizeof buf);
    out_len = 0xffffffffu;
    rc = mxgpu_drm_get_info_response_encode(1, 2, 0, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf, "encoding 1, 2, 0 is refused");
    poison(buf, sizeof buf);
    out_len = 7;
    rc = mxgpu_drm_get_info_response_encode(1, 1, 1, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf, "encoding 1, 1, 1 is refused");
    poison(buf, sizeof buf);
    out_len = 7;
    rc = mxgpu_drm_get_info_response_encode(1, 0, 0, buf, 8, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_LENGTH, out_len, buf, sizeof buf, "short cap refuses get_info");

    poison(buf, sizeof buf);
    out_len = 0;
    rc = mxgpu_drm_ctx_create_encode(buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "ctx_create encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES, "ctx_create length");
    check_header(buf, out_len, MXGPU_DRM_KIND_CTX_CREATE, sizeof buf);

    poison(buf, sizeof buf);
    out_len = 0;
    rc = mxgpu_drm_ctx_create_response_encode(0, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf, "ctx id 0 refused");
    rc = mxgpu_drm_ctx_create_response_encode(0xa1b2c3d4u, buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "ctx_create response encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 8u, "ctx_create response length");
    check_header(buf, out_len, MXGPU_DRM_KIND_CTX_CREATE, sizeof buf);
    expect(buf[16] == 0xd4 && buf[17] == 0xc3 && buf[18] == 0xb2 && buf[19] == 0xa1,
           "context id is little endian");
    expect(load_u32(buf + 20) == 0, "ctx response pad");
    context_id = 0;
    rc = mxgpu_drm_ctx_create_response_decode(buf, out_len, &context_id);
    expect(rc == MXGPU_DRM_OK && context_id == 0xa1b2c3d4u, "ctx_create response round trip");
    buf[12] ^= 0x01u;
    rc = mxgpu_drm_ctx_create_response_decode(buf, out_len, &context_id);
    expect(rc == MXGPU_DRM_ERR_RESERVED, "flipping reserved fails ctx_create response decode");
    buf[12] ^= 0x01u;
    buf[20] = 1;
    rc = mxgpu_drm_ctx_create_response_decode(buf, out_len, &context_id);
    expect(rc == MXGPU_DRM_ERR_RESERVED, "nonzero ctx pad refused");

    poison(buf, sizeof buf);
    out_len = 0;
    rc = mxgpu_drm_ctx_destroy_encode(0, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf, "destroy id 0 refused");
    rc = mxgpu_drm_ctx_destroy_encode(42, buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "ctx_destroy encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 8u, "ctx_destroy length");
    check_header(buf, out_len, MXGPU_DRM_KIND_CTX_DESTROY, sizeof buf);
    context_id = 0;
    rc = mxgpu_drm_ctx_destroy_decode(buf, out_len, &context_id);
    expect(rc == MXGPU_DRM_OK && context_id == 42u, "ctx_destroy round trip");
    buf[12] = 0x10;
    rc = mxgpu_drm_ctx_destroy_decode(buf, out_len, &context_id);
    expect(rc == MXGPU_DRM_ERR_RESERVED, "flipping reserved fails ctx_destroy decode");
    buf[12] = 0;
    buf[0] = MXGPU_DRM_KIND_WAIT;
    rc = mxgpu_drm_ctx_destroy_decode(buf, out_len, &context_id);
    expect(rc == MXGPU_DRM_ERR_KIND, "wrong kind");

    poison(buf, sizeof buf);
    out_len = 9;
    rc = mxgpu_drm_gem_create_encode(0, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf, "gem size 0 refused");
    poison(buf, sizeof buf);
    out_len = 9;
    rc = mxgpu_drm_gem_create_encode(4097, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_RANGE, out_len, buf, sizeof buf, "gem size alignment refused");
    gem_size = 0x200000000000ull;
    rc = mxgpu_drm_gem_create_encode(gem_size, buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "gem_create encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 8u, "gem_create length");
    check_header(buf, out_len, MXGPU_DRM_KIND_GEM_CREATE, sizeof buf);
    expect(buf[16] == 0x00 && buf[21] == 0x20 && buf[22] == 0x00 && buf[23] == 0x00,
           "gem size little endian");
    gem_size = 0;
    rc = mxgpu_drm_gem_create_decode(buf, out_len, &gem_size);
    expect(rc == MXGPU_DRM_OK && gem_size == 0x200000000000ull, "gem_create round trip");
    buf[12] ^= 0xff;
    rc = mxgpu_drm_gem_create_decode(buf, out_len, &gem_size);
    expect(rc == MXGPU_DRM_ERR_RESERVED, "flipping reserved fails gem_create decode");
    buf[12] ^= 0xff;
    buf[16] = 1;
    rc = mxgpu_drm_gem_create_decode(buf, out_len, &gem_size);
    expect(rc == MXGPU_DRM_ERR_RANGE, "decode rejects unaligned gem size");

    poison(buf, sizeof buf);
    out_len = 3;
    rc = mxgpu_drm_gem_create_response_encode(0, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf, "gem handle 0 refused");
    rc = mxgpu_drm_gem_create_response_encode(0x01020304u, buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "gem_create response encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 8u, "gem response length");
    check_header(buf, out_len, MXGPU_DRM_KIND_GEM_CREATE, sizeof buf);
    handle = 0;
    rc = mxgpu_drm_gem_create_response_decode(buf, out_len, &handle);
    expect(rc == MXGPU_DRM_OK && handle == 0x01020304u, "gem response round trip");
    buf[12] ^= 0x01u;
    rc = mxgpu_drm_gem_create_response_decode(buf, out_len, &handle);
    expect(rc == MXGPU_DRM_ERR_RESERVED, "flipping reserved fails gem response decode");

    poison(buf, sizeof buf);
    out_len = 3;
    rc = mxgpu_drm_gem_close_encode(7, buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "gem_close encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 8u, "gem_close length");
    check_header(buf, out_len, MXGPU_DRM_KIND_GEM_CLOSE, sizeof buf);
    handle = 0;
    rc = mxgpu_drm_gem_close_decode(buf, out_len, &handle);
    expect(rc == MXGPU_DRM_OK && handle == 7u, "gem_close round trip");
    buf[12] ^= 0x01u;
    rc = mxgpu_drm_gem_close_decode(buf, out_len, &handle);
    expect(rc == MXGPU_DRM_ERR_RESERVED, "flipping reserved fails gem_close decode");

    poison(buf, sizeof buf);
    out_len = 0xffffffffu;
    rc = mxgpu_drm_submit_encode(1, 0, 1, 0, command, 0, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf,
                  "submit with command_bytes 0 leaves poison");
    poison(buf, sizeof buf);
    out_len = 0xffffffffu;
    rc = mxgpu_drm_submit_encode(1, 6, 1, 0, command, 5, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_RANGE, out_len, buf, sizeof buf, "submit queue 6 refused");
    poison(buf, sizeof buf);
    out_len = 0xffffffffu;
    rc = mxgpu_drm_submit_encode(0, 1, 9, 4, command, 5, buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK && load_u32(buf + 16) == 0, "submit wrapper permits global context");
    rc = mxgpu_drm_submit_decode(buf, out_len, &context_id, &queue, &fence_value, &response_cap,
                                 &command_out, &command_bytes);
    expect(rc == MXGPU_DRM_OK && context_id == 0 && command_bytes == 5,
           "global context wrapper round trip");

    poison(buf, sizeof buf);
    out_len = 0;
    rc = mxgpu_drm_submit_encode(0x11u, 5, 0x8000000000000001ull, 0x11223344u, command, 5, buf,
                                 sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "submit encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 24u + 5u, "submit length");
    check_header(buf, out_len, MXGPU_DRM_KIND_SUBMIT, sizeof buf);
    expect(load_u32(buf + 16) == 0x11u, "submit context field");
    expect(load_u32(buf + 20) == 5u, "submit queue field");
    expect(load_u32(buf + 24) == 5u, "submit command_bytes field");
    expect(load_u32(buf + 28) == 0x11223344u, "submit response_cap field");
    expect(buf[32] == 0x01 && buf[39] == 0x80, "submit fence little endian");
    expect(memcmp(buf + 40, command, 5) == 0, "submit command bytes");
    context_id = 0;
    queue = 9;
    fence_value = 0;
    response_cap = 0;
    command_out = 0;
    command_bytes = 0;
    rc = mxgpu_drm_submit_decode(buf, out_len, &context_id, &queue, &fence_value, &response_cap,
                                 &command_out, &command_bytes);
    expect(rc == MXGPU_DRM_OK, "submit decode");
    expect(context_id == 0x11u && queue == 5u && fence_value == 0x8000000000000001ull,
           "submit scalars");
    expect(response_cap == 0x11223344u && command_bytes == 5u, "submit cap and length");
    expect(command_out == buf + 40, "submit command points into input");
    expect(memcmp(command_out, command, 5) == 0, "submit command round trip");
    buf[12] ^= 0x01u;
    rc = mxgpu_drm_submit_decode(buf, out_len, &context_id, &queue, &fence_value, &response_cap,
                                 &command_out, &command_bytes);
    expect(rc == MXGPU_DRM_ERR_RESERVED, "flipping reserved fails submit decode");
    buf[12] ^= 0x01u;
    rc = mxgpu_drm_submit_decode(buf, out_len - 1u, &context_id, &queue, &fence_value,
                                 &response_cap, &command_out, &command_bytes);
    expect(rc == MXGPU_DRM_ERR_LENGTH, "truncated submit refused");

    poison(buf, sizeof buf);
    out_len = 0;
    rc = mxgpu_drm_submit_response_encode(0, 0, 0, 0, buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "empty submit response encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 16u, "empty submit response length");
    check_header(buf, out_len, MXGPU_DRM_KIND_SUBMIT, sizeof buf);
    completion_status = 9;
    completed_fence = 9;
    response_out = 0;
    response_written = 9;
    rc = mxgpu_drm_submit_response_decode(buf, out_len, &completion_status, &completed_fence,
                                          &response_out, &response_written);
    expect(rc == MXGPU_DRM_OK, "empty submit response decode");
    expect(completion_status == 0 && completed_fence == 0 && response_written == 0,
           "empty submit response fields");
    expect(response_out == buf + 32, "empty response pointer at payload");
    buf[12] ^= 0x01u;
    rc = mxgpu_drm_submit_response_decode(buf, out_len, &completion_status, &completed_fence,
                                          &response_out, &response_written);
    expect(rc == MXGPU_DRM_ERR_RESERVED, "flipping reserved fails submit response decode");

    poison(buf, sizeof buf);
    out_len = 0;
    rc = mxgpu_drm_submit_response_encode(0xffffffffu, 0x100000002ull, response, 3, buf, sizeof buf,
                                          &out_len);
    expect(rc == MXGPU_DRM_OK, "submit response encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 16u + 3u, "submit response length");
    check_header(buf, out_len, MXGPU_DRM_KIND_SUBMIT, sizeof buf);
    rc = mxgpu_drm_submit_response_decode(buf, out_len, &completion_status, &completed_fence,
                                          &response_out, &response_written);
    expect(rc == MXGPU_DRM_OK, "submit response decode");
    expect(completion_status == 0xffffffffu && completed_fence == 0x100000002ull,
           "submit response scalars");
    expect(response_written == 3u && response_out == buf + 32, "submit response payload pointer");
    expect(memcmp(response_out, response, 3) == 0, "submit response bytes");

    poison(buf, sizeof buf);
    out_len = 4;
    rc = mxgpu_drm_wait_encode(3, 0, 0, 10, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf, "wait fence 0 refused");
    poison(buf, sizeof buf);
    out_len = 4;
    rc = mxgpu_drm_wait_encode(3, 6, 1, 10, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_RANGE, out_len, buf, sizeof buf, "wait queue refused");
    rc = mxgpu_drm_wait_encode(0xfffffffeu, 0, 0x0102030405060708ull, 0, buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "wait encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 24u, "wait length");
    check_header(buf, out_len, MXGPU_DRM_KIND_WAIT, sizeof buf);
    expect(load_u32(buf + 36) == 0, "wait pad");
    context_id = 0;
    queue = 9;
    fence_value = 0;
    timeout_ms = 1;
    rc = mxgpu_drm_wait_decode(buf, out_len, &context_id, &queue, &fence_value, &timeout_ms);
    expect(rc == MXGPU_DRM_OK, "wait decode");
    expect(context_id == 0xfffffffeu && queue == 0 && timeout_ms == 0, "wait scalars");
    expect(fence_value == 0x0102030405060708ull, "wait fence round trip");
    buf[12] ^= 0x01u;
    rc = mxgpu_drm_wait_decode(buf, out_len, &context_id, &queue, &fence_value, &timeout_ms);
    expect(rc == MXGPU_DRM_ERR_RESERVED, "flipping reserved fails wait decode");

    poison(buf, sizeof buf);
    out_len = 5;
    rc = mxgpu_drm_bo_write_encode(0, 0, data, 7, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf, "bo_write handle 0 refused");
    poison(buf, sizeof buf);
    out_len = 5;
    rc = mxgpu_drm_bo_write_encode(2, 0, data, 0, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf, "bo_write size 0 refused");
    rc = mxgpu_drm_bo_write_encode(2, 0x100000000ull, data, 7, buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "bo_write encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 24u + 7u, "bo_write length");
    check_header(buf, out_len, MXGPU_DRM_KIND_BO_WRITE, sizeof buf);
    expect(load_u32(buf + 20) == 0 && load_u32(buf + 36) == 0, "bo_write zero words");
    handle = 0;
    offset = 0;
    data_out = 0;
    size = 0;
    rc = mxgpu_drm_bo_write_decode(buf, out_len, &handle, &offset, &data_out, &size);
    expect(rc == MXGPU_DRM_OK, "bo_write decode");
    expect(handle == 2u && offset == 0x100000000ull && size == 7u, "bo_write fields");
    expect(data_out == buf + 40, "bo_write pointer at payload");
    expect(memcmp(data_out, data, 7) == 0, "bo_write bytes");
    buf[12] ^= 0x01u;
    rc = mxgpu_drm_bo_write_decode(buf, out_len, &handle, &offset, &data_out, &size);
    expect(rc == MXGPU_DRM_ERR_RESERVED, "flipping reserved fails bo_write decode");

    poison(buf, sizeof buf);
    out_len = 6;
    rc = mxgpu_drm_bo_read_encode(9, 0, 0, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf, "bo_read size 0 refused");
    rc = mxgpu_drm_bo_read_encode(9, 4096, 32, buf, sizeof buf, &out_len);
    expect(rc == MXGPU_DRM_OK, "bo_read encode");
    expect(out_len == MXGPU_DRM_HEADER_BYTES + 24u, "bo_read length");
    check_header(buf, out_len, MXGPU_DRM_KIND_BO_READ, sizeof buf);
    handle = 0;
    offset = 1;
    size = 0;
    rc = mxgpu_drm_bo_read_decode(buf, out_len, &handle, &offset, &size);
    expect(rc == MXGPU_DRM_OK && handle == 9u && offset == 4096u && size == 32u,
           "bo_read round trip");
    buf[12] ^= 0x01u;
    rc = mxgpu_drm_bo_read_decode(buf, out_len, &handle, &offset, &size);
    expect(rc == MXGPU_DRM_ERR_RESERVED, "flipping reserved fails bo_read decode");
    buf[12] ^= 0x01u;
    buf[8] = 0x3u;
    rc = mxgpu_drm_bo_read_decode(buf, out_len, &handle, &offset, &size);
    expect(rc == MXGPU_DRM_OK, "nonzero flags are not reserved");

    poison(buf, sizeof buf);
    out_len = 1;
    rc = mxgpu_drm_submit_encode(1, 0, 1, 0, 0, 4, buf, sizeof buf, &out_len);
    check_refused(rc, MXGPU_DRM_ERR_STATE, out_len, buf, sizeof buf, "null command refused");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
