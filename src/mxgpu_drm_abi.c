/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_drm_uapi.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_u64(uint8_t *p, uint64_t v)
{
    put_u32(p, (uint32_t)v);
    put_u32(p + 4, (uint32_t)(v >> 32));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t get_u64(const uint8_t *p)
{
    return (uint64_t)get_u32(p) | ((uint64_t)get_u32(p + 4) << 32);
}

static int refuse(uint32_t *out_len, int status)
{
    if (out_len)
        *out_len = 0;
    return status;
}

static int write_record(uint8_t *out, uint32_t cap, uint32_t *out_len, uint32_t kind,
                        const uint8_t *fixed, uint32_t fixed_len, const uint8_t *tail,
                        uint32_t tail_len)
{
    uint32_t total;
    uint32_t i;

    if (!out_len)
        return MXGPU_DRM_ERR_STATE;
    *out_len = 0;
    if ((fixed_len && !fixed) || (tail_len && !tail))
        return MXGPU_DRM_ERR_STATE;
    if (fixed_len > 0xffffffffu - MXGPU_DRM_HEADER_BYTES ||
        tail_len > 0xffffffffu - MXGPU_DRM_HEADER_BYTES - fixed_len)
        return MXGPU_DRM_ERR_LENGTH;
    total = MXGPU_DRM_HEADER_BYTES + fixed_len + tail_len;
    if (!out || cap < total)
        return MXGPU_DRM_ERR_LENGTH;

    put_u32(out + 0, kind);
    put_u32(out + 4, total);
    put_u32(out + 8, 0);
    put_u32(out + 12, 0);
    for (i = 0; i < fixed_len; i++)
        out[MXGPU_DRM_HEADER_BYTES + i] = fixed[i];
    for (i = 0; i < tail_len; i++)
        out[MXGPU_DRM_HEADER_BYTES + fixed_len + i] = tail[i];
    *out_len = total;
    return MXGPU_DRM_OK;
}

static int open_header(const uint8_t *in, uint32_t len, uint32_t kind, uint32_t min_total,
                       uint32_t *byte_len)
{
    uint32_t got;

    if (!in || len < MXGPU_DRM_HEADER_BYTES)
        return MXGPU_DRM_ERR_LENGTH;
    if (get_u32(in + 12) != 0)
        return MXGPU_DRM_ERR_RESERVED;
    if (get_u32(in + 0) != kind)
        return MXGPU_DRM_ERR_KIND;
    got = get_u32(in + 4);
    if (got < min_total || len < got)
        return MXGPU_DRM_ERR_LENGTH;
    *byte_len = got;
    return MXGPU_DRM_OK;
}

static int open_fixed(const uint8_t *in, uint32_t len, uint32_t kind, uint32_t payload_len,
                      const uint8_t **payload)
{
    uint32_t byte_len;
    int status;

    status = open_header(in, len, kind, MXGPU_DRM_HEADER_BYTES + payload_len, &byte_len);
    if (status)
        return status;
    if (byte_len != MXGPU_DRM_HEADER_BYTES + payload_len)
        return MXGPU_DRM_ERR_LENGTH;
    if (payload)
        *payload = in + MXGPU_DRM_HEADER_BYTES;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_get_info_encode(uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_GET_INFO, 0, 0, 0, 0);
}

int mxgpu_drm_get_info_response_encode(uint32_t major, uint32_t minor, uint32_t patch, uint8_t *out,
                                       uint32_t cap, uint32_t *out_len)
{
    uint8_t body[12];

    if (major != MXGPU_DRIVER_VERSION_MAJOR || minor != MXGPU_DRIVER_VERSION_MINOR ||
        patch != MXGPU_DRIVER_VERSION_PATCH)
        return refuse(out_len, MXGPU_DRM_ERR_STATE);
    put_u32(body + 0, major);
    put_u32(body + 4, minor);
    put_u32(body + 8, patch);
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_GET_INFO, body, 12, 0, 0);
}

int mxgpu_drm_get_info_response_decode(const uint8_t *in, uint32_t len, uint32_t *major,
                                       uint32_t *minor, uint32_t *patch)
{
    const uint8_t *payload;
    uint32_t maj;
    uint32_t min;
    uint32_t pat;
    int status;

    if (!major || !minor || !patch)
        return MXGPU_DRM_ERR_STATE;
    status = open_fixed(in, len, MXGPU_DRM_KIND_GET_INFO, 12, &payload);
    if (status)
        return status;
    maj = get_u32(payload + 0);
    min = get_u32(payload + 4);
    pat = get_u32(payload + 8);
    if (maj != MXGPU_DRIVER_VERSION_MAJOR || min != MXGPU_DRIVER_VERSION_MINOR ||
        pat != MXGPU_DRIVER_VERSION_PATCH)
        return MXGPU_DRM_ERR_STATE;
    *major = maj;
    *minor = min;
    *patch = pat;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_ctx_create_encode(uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_CTX_CREATE, 0, 0, 0, 0);
}

static int put_id_pad(uint32_t id, uint32_t kind, uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint8_t body[8];

    if (id == 0)
        return refuse(out_len, MXGPU_DRM_ERR_STATE);
    put_u32(body + 0, id);
    put_u32(body + 4, 0);
    return write_record(out, cap, out_len, kind, body, 8, 0, 0);
}

static int take_id_pad(const uint8_t *in, uint32_t len, uint32_t kind, uint32_t *id)
{
    const uint8_t *payload;
    uint32_t value;
    int status;

    if (!id)
        return MXGPU_DRM_ERR_STATE;
    status = open_fixed(in, len, kind, 8, &payload);
    if (status)
        return status;
    if (get_u32(payload + 4) != 0)
        return MXGPU_DRM_ERR_RESERVED;
    value = get_u32(payload + 0);
    if (value == 0)
        return MXGPU_DRM_ERR_STATE;
    *id = value;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_ctx_create_response_encode(uint32_t context_id, uint8_t *out, uint32_t cap,
                                         uint32_t *out_len)
{
    return put_id_pad(context_id, MXGPU_DRM_KIND_CTX_CREATE, out, cap, out_len);
}

int mxgpu_drm_ctx_create_response_decode(const uint8_t *in, uint32_t len, uint32_t *context_id)
{
    return take_id_pad(in, len, MXGPU_DRM_KIND_CTX_CREATE, context_id);
}

int mxgpu_drm_ctx_destroy_encode(uint32_t context_id, uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    return put_id_pad(context_id, MXGPU_DRM_KIND_CTX_DESTROY, out, cap, out_len);
}

int mxgpu_drm_ctx_destroy_decode(const uint8_t *in, uint32_t len, uint32_t *context_id)
{
    return take_id_pad(in, len, MXGPU_DRM_KIND_CTX_DESTROY, context_id);
}

int mxgpu_drm_gem_create_encode(uint64_t size, uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint8_t body[8];

    if (size == 0)
        return refuse(out_len, MXGPU_DRM_ERR_STATE);
    if ((size % MXGPU_DRM_GEM_ALIGN) != 0)
        return refuse(out_len, MXGPU_DRM_ERR_RANGE);
    put_u64(body, size);
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_GEM_CREATE, body, 8, 0, 0);
}

int mxgpu_drm_gem_create_decode(const uint8_t *in, uint32_t len, uint64_t *size)
{
    const uint8_t *payload;
    uint64_t value;
    int status;

    if (!size)
        return MXGPU_DRM_ERR_STATE;
    status = open_fixed(in, len, MXGPU_DRM_KIND_GEM_CREATE, 8, &payload);
    if (status)
        return status;
    value = get_u64(payload);
    if (value == 0)
        return MXGPU_DRM_ERR_STATE;
    if ((value % MXGPU_DRM_GEM_ALIGN) != 0)
        return MXGPU_DRM_ERR_RANGE;
    *size = value;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_gem_create_response_encode(uint32_t handle, uint8_t *out, uint32_t cap,
                                         uint32_t *out_len)
{
    return put_id_pad(handle, MXGPU_DRM_KIND_GEM_CREATE, out, cap, out_len);
}

int mxgpu_drm_gem_create_response_decode(const uint8_t *in, uint32_t len, uint32_t *handle)
{
    return take_id_pad(in, len, MXGPU_DRM_KIND_GEM_CREATE, handle);
}

int mxgpu_drm_gem_close_encode(uint32_t handle, uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    return put_id_pad(handle, MXGPU_DRM_KIND_GEM_CLOSE, out, cap, out_len);
}

int mxgpu_drm_gem_close_decode(const uint8_t *in, uint32_t len, uint32_t *handle)
{
    return take_id_pad(in, len, MXGPU_DRM_KIND_GEM_CLOSE, handle);
}

int mxgpu_drm_submit_encode(uint32_t context_id, uint32_t queue, uint64_t fence_value,
                            uint32_t response_cap, const uint8_t *command, uint32_t command_bytes,
                            uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint8_t fixed[24];

    if (queue > MXGPU_DRM_QUEUE_MAX)
        return refuse(out_len, MXGPU_DRM_ERR_RANGE);
    if (command_bytes == 0 || !command)
        return refuse(out_len, MXGPU_DRM_ERR_STATE);
    put_u32(fixed + 0, context_id);
    put_u32(fixed + 4, queue);
    put_u32(fixed + 8, command_bytes);
    put_u32(fixed + 12, response_cap);
    put_u64(fixed + 16, fence_value);
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_SUBMIT, fixed, 24, command,
                        command_bytes);
}

int mxgpu_drm_submit_decode(const uint8_t *in, uint32_t len, uint32_t *context_id, uint32_t *queue,
                            uint64_t *fence_value, uint32_t *response_cap, const uint8_t **command,
                            uint32_t *command_bytes)
{
    uint32_t byte_len;
    uint32_t id;
    uint32_t q;
    uint32_t bytes;
    uint32_t cap_field;
    uint64_t fence;
    int status;

    if (!context_id || !queue || !fence_value || !response_cap || !command || !command_bytes)
        return MXGPU_DRM_ERR_STATE;
    status = open_header(in, len, MXGPU_DRM_KIND_SUBMIT, MXGPU_DRM_HEADER_BYTES + 24u, &byte_len);
    if (status)
        return status;
    id = get_u32(in + 16);
    q = get_u32(in + 20);
    bytes = get_u32(in + 24);
    cap_field = get_u32(in + 28);
    fence = get_u64(in + 32);
    if (byte_len - (MXGPU_DRM_HEADER_BYTES + 24u) != bytes)
        return MXGPU_DRM_ERR_LENGTH;
    if (q > MXGPU_DRM_QUEUE_MAX)
        return MXGPU_DRM_ERR_RANGE;
    if (bytes == 0)
        return MXGPU_DRM_ERR_STATE;
    *context_id = id;
    *queue = q;
    *fence_value = fence;
    *response_cap = cap_field;
    *command = in + MXGPU_DRM_HEADER_BYTES + 24u;
    *command_bytes = bytes;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_submit_response_encode(uint32_t completion_status, uint64_t completed_fence,
                                     const uint8_t *response, uint32_t response_written,
                                     uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint8_t fixed[16];

    if (response_written && !response)
        return refuse(out_len, MXGPU_DRM_ERR_STATE);
    put_u32(fixed + 0, completion_status);
    put_u32(fixed + 4, response_written);
    put_u64(fixed + 8, completed_fence);
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_SUBMIT, fixed, 16,
                        response_written ? response : 0, response_written);
}

int mxgpu_drm_submit_response_decode(const uint8_t *in, uint32_t len, uint32_t *completion_status,
                                     uint64_t *completed_fence, const uint8_t **response,
                                     uint32_t *response_written)
{
    uint32_t byte_len;
    uint32_t status_field;
    uint32_t written;
    uint64_t fence;
    int status;

    if (!completion_status || !completed_fence || !response || !response_written)
        return MXGPU_DRM_ERR_STATE;
    status = open_header(in, len, MXGPU_DRM_KIND_SUBMIT, MXGPU_DRM_HEADER_BYTES + 16u, &byte_len);
    if (status)
        return status;
    status_field = get_u32(in + 16);
    written = get_u32(in + 20);
    fence = get_u64(in + 24);
    if (byte_len - (MXGPU_DRM_HEADER_BYTES + 16u) != written)
        return MXGPU_DRM_ERR_LENGTH;
    *completion_status = status_field;
    *completed_fence = fence;
    *response = in + MXGPU_DRM_HEADER_BYTES + 16u;
    *response_written = written;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_wait_encode(uint32_t context_id, uint32_t queue, uint64_t fence_value,
                          uint32_t timeout_ms, uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint8_t body[24];

    if (queue > MXGPU_DRM_QUEUE_MAX)
        return refuse(out_len, MXGPU_DRM_ERR_RANGE);
    if (context_id == 0 || fence_value == 0)
        return refuse(out_len, MXGPU_DRM_ERR_STATE);
    put_u32(body + 0, context_id);
    put_u32(body + 4, queue);
    put_u64(body + 8, fence_value);
    put_u32(body + 16, timeout_ms);
    put_u32(body + 20, 0);
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_WAIT, body, 24, 0, 0);
}

int mxgpu_drm_wait_decode(const uint8_t *in, uint32_t len, uint32_t *context_id, uint32_t *queue,
                          uint64_t *fence_value, uint32_t *timeout_ms)
{
    const uint8_t *payload;
    uint32_t id;
    uint32_t q;
    uint64_t fence;
    uint32_t timeout;
    int status;

    if (!context_id || !queue || !fence_value || !timeout_ms)
        return MXGPU_DRM_ERR_STATE;
    status = open_fixed(in, len, MXGPU_DRM_KIND_WAIT, 24, &payload);
    if (status)
        return status;
    id = get_u32(payload + 0);
    q = get_u32(payload + 4);
    fence = get_u64(payload + 8);
    timeout = get_u32(payload + 16);
    if (get_u32(payload + 20) != 0)
        return MXGPU_DRM_ERR_RESERVED;
    if (q > MXGPU_DRM_QUEUE_MAX)
        return MXGPU_DRM_ERR_RANGE;
    if (id == 0 || fence == 0)
        return MXGPU_DRM_ERR_STATE;
    *context_id = id;
    *queue = q;
    *fence_value = fence;
    *timeout_ms = timeout;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_bo_write_encode(uint32_t handle, uint64_t offset, const uint8_t *data, uint32_t size,
                              uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint8_t fixed[24];

    if (handle == 0 || size == 0 || !data)
        return refuse(out_len, MXGPU_DRM_ERR_STATE);
    put_u32(fixed + 0, handle);
    put_u32(fixed + 4, 0);
    put_u64(fixed + 8, offset);
    put_u32(fixed + 16, size);
    put_u32(fixed + 20, 0);
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_BO_WRITE, fixed, 24, data, size);
}

int mxgpu_drm_bo_write_decode(const uint8_t *in, uint32_t len, uint32_t *handle, uint64_t *offset,
                              const uint8_t **data, uint32_t *size)
{
    uint32_t byte_len;
    uint32_t id;
    uint32_t bytes;
    uint64_t off;
    int status;

    if (!handle || !offset || !data || !size)
        return MXGPU_DRM_ERR_STATE;
    status = open_header(in, len, MXGPU_DRM_KIND_BO_WRITE, MXGPU_DRM_HEADER_BYTES + 24u, &byte_len);
    if (status)
        return status;
    if (get_u32(in + 20) != 0 || get_u32(in + 36) != 0)
        return MXGPU_DRM_ERR_RESERVED;
    id = get_u32(in + 16);
    off = get_u64(in + 24);
    bytes = get_u32(in + 32);
    if (byte_len - (MXGPU_DRM_HEADER_BYTES + 24u) != bytes)
        return MXGPU_DRM_ERR_LENGTH;
    if (id == 0 || bytes == 0)
        return MXGPU_DRM_ERR_STATE;
    *handle = id;
    *offset = off;
    *data = in + MXGPU_DRM_HEADER_BYTES + 24u;
    *size = bytes;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_bo_read_encode(uint32_t handle, uint64_t offset, uint32_t size, uint8_t *out,
                             uint32_t cap, uint32_t *out_len)
{
    uint8_t body[24];

    if (handle == 0 || size == 0)
        return refuse(out_len, MXGPU_DRM_ERR_STATE);
    put_u32(body + 0, handle);
    put_u32(body + 4, 0);
    put_u64(body + 8, offset);
    put_u32(body + 16, size);
    put_u32(body + 20, 0);
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_BO_READ, body, 24, 0, 0);
}

int mxgpu_drm_bo_read_decode(const uint8_t *in, uint32_t len, uint32_t *handle, uint64_t *offset,
                             uint32_t *size)
{
    const uint8_t *payload;
    uint32_t id;
    uint32_t bytes;
    uint64_t off;
    int status;

    if (!handle || !offset || !size)
        return MXGPU_DRM_ERR_STATE;
    status = open_fixed(in, len, MXGPU_DRM_KIND_BO_READ, 24, &payload);
    if (status)
        return status;
    if (get_u32(payload + 4) != 0 || get_u32(payload + 20) != 0)
        return MXGPU_DRM_ERR_RESERVED;
    id = get_u32(payload + 0);
    off = get_u64(payload + 8);
    bytes = get_u32(payload + 16);
    if (id == 0 || bytes == 0)
        return MXGPU_DRM_ERR_STATE;
    *handle = id;
    *offset = off;
    *size = bytes;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_get_caps_encode(uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_GET_CAPS, 0, 0, 0, 0);
}

int mxgpu_drm_get_caps_response_encode(const struct mxgpu_drm_caps *in, uint8_t *out, uint32_t cap,
                                       uint32_t *out_len)
{
    uint8_t body[28];

    if (!in || !in->major || !in->max_command_bytes || !in->max_queues ||
        !in->max_descriptors_per_queue)
        return refuse(out_len, MXGPU_DRM_ERR_STATE);
    put_u32(body, in->major);
    put_u32(body + 4, in->minor);
    put_u64(body + 8, in->features);
    put_u32(body + 16, in->max_command_bytes);
    put_u32(body + 20, in->max_queues);
    put_u32(body + 24, in->max_descriptors_per_queue);
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_GET_CAPS, body, sizeof(body), 0, 0);
}

int mxgpu_drm_get_caps_response_decode(const uint8_t *in, uint32_t len, struct mxgpu_drm_caps *out)
{
    const uint8_t *body;
    struct mxgpu_drm_caps caps;
    int status;

    if (!out)
        return MXGPU_DRM_ERR_STATE;
    status = open_fixed(in, len, MXGPU_DRM_KIND_GET_CAPS, 28, &body);
    if (status)
        return status;
    if (len != MXGPU_DRM_HEADER_BYTES + 28)
        return MXGPU_DRM_ERR_LENGTH;
    if (get_u32(in + 8))
        return MXGPU_DRM_ERR_RESERVED;
    caps.major = get_u32(body);
    caps.minor = get_u32(body + 4);
    caps.features = get_u64(body + 8);
    caps.max_command_bytes = get_u32(body + 16);
    caps.max_queues = get_u32(body + 20);
    caps.max_descriptors_per_queue = get_u32(body + 24);
    if (!caps.major || !caps.max_command_bytes || !caps.max_queues ||
        !caps.max_descriptors_per_queue)
        return MXGPU_DRM_ERR_STATE;
    *out = caps;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_get_transfer_limits_encode(uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_GET_TRANSFER_LIMITS, 0, 0, 0, 0);
}

int mxgpu_drm_get_transfer_limits_response_encode(const struct mxgpu_drm_transfer_limits *in,
                                                  uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint8_t body[8];
    if (!in || !in->max_transfer_to_host_bytes || !in->max_transfer_from_host_bytes)
        return refuse(out_len, MXGPU_DRM_ERR_STATE);
    put_u32(body, in->max_transfer_to_host_bytes);
    put_u32(body + 4, in->max_transfer_from_host_bytes);
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_GET_TRANSFER_LIMITS, body, sizeof(body),
                        0, 0);
}

int mxgpu_drm_get_transfer_limits_response_decode(const uint8_t *in, uint32_t len,
                                                  struct mxgpu_drm_transfer_limits *out)
{
    const uint8_t *body;
    struct mxgpu_drm_transfer_limits limits;
    int status;
    if (!out)
        return MXGPU_DRM_ERR_STATE;
    status = open_fixed(in, len, MXGPU_DRM_KIND_GET_TRANSFER_LIMITS, 8, &body);
    if (status)
        return status;
    if (len != MXGPU_DRM_HEADER_BYTES + 8)
        return MXGPU_DRM_ERR_LENGTH;
    if (get_u32(in + 8))
        return MXGPU_DRM_ERR_RESERVED;
    limits.max_transfer_to_host_bytes = get_u32(body);
    limits.max_transfer_from_host_bytes = get_u32(body + 4);
    if (!limits.max_transfer_to_host_bytes || !limits.max_transfer_from_host_bytes)
        return MXGPU_DRM_ERR_STATE;
    *out = limits;
    return MXGPU_DRM_OK;
}

static int batch_header(const uint8_t *in, uint32_t len, uint32_t kind, uint32_t minimum)
{
    uint32_t bytes;
    int rc = open_header(in, len, kind, minimum, &bytes);
    if (rc)
        return rc;
    if (bytes != len)
        return MXGPU_DRM_ERR_LENGTH;
    if (get_u32(in + 8))
        return MXGPU_DRM_ERR_RESERVED;
    return MXGPU_DRM_OK;
}

static int batch_limits_ok(const struct mxgpu_drm_batch_limits *v)
{
    if (!v)
        return MXGPU_DRM_ERR_STATE;
    if (v->abi_major != MXGPU_DRM_ABI_MAJOR || v->abi_minor < MXGPU_DRM_ABI_BATCH_MINOR ||
        v->max_commands > MXGPU_DRM_BATCH_MAX_COMMANDS ||
        v->max_command_bytes > MXGPU_DRM_BATCH_MAX_COMMAND_BYTES ||
        v->max_input_bytes > MXGPU_DRM_BATCH_MAX_INPUT_BYTES ||
        v->outcome_bytes != MXGPU_DRM_BATCH_OUTCOME_BYTES)
        return MXGPU_DRM_ERR_RANGE;
    if (v->max_commands &&
        (!v->max_command_bytes ||
         v->max_input_bytes < MXGPU_DRM_BATCH_PREFIX_BYTES +
                                  v->max_commands * MXGPU_DRM_BATCH_DESCRIPTOR_BYTES +
                                  v->max_command_bytes))
        return MXGPU_DRM_ERR_RANGE;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_get_batch_limits_encode(uint8_t *out, uint32_t cap, uint32_t *len)
{
    return write_record(out, cap, len, MXGPU_DRM_KIND_GET_BATCH_LIMITS, 0, 0, 0, 0);
}

int mxgpu_drm_get_batch_limits_response_encode(const struct mxgpu_drm_batch_limits *v, uint8_t *out,
                                               uint32_t cap, uint32_t *len)
{
    uint8_t p[24];
    int rc = batch_limits_ok(v);
    if (rc)
        return refuse(len, rc);
    put_u32(p, v->abi_major);
    put_u32(p + 4, v->abi_minor);
    put_u32(p + 8, v->max_commands);
    put_u32(p + 12, v->max_command_bytes);
    put_u32(p + 16, v->max_input_bytes);
    put_u32(p + 20, v->outcome_bytes);
    return write_record(out, cap, len, MXGPU_DRM_KIND_GET_BATCH_LIMITS, p, 24, 0, 0);
}

int mxgpu_drm_get_batch_limits_response_decode(const uint8_t *in, uint32_t len,
                                               struct mxgpu_drm_batch_limits *out)
{
    struct mxgpu_drm_batch_limits v;
    int rc = batch_header(in, len, MXGPU_DRM_KIND_GET_BATCH_LIMITS, 40);
    if (rc)
        return rc;
    if (len != 40)
        return MXGPU_DRM_ERR_LENGTH;
    if (!out)
        return MXGPU_DRM_ERR_STATE;
    v.abi_major = get_u32(in + 16);
    v.abi_minor = get_u32(in + 20);
    v.max_commands = get_u32(in + 24);
    v.max_command_bytes = get_u32(in + 28);
    v.max_input_bytes = get_u32(in + 32);
    v.outcome_bytes = get_u32(in + 36);
    rc = batch_limits_ok(&v);
    if (!rc)
        *out = v;
    return rc;
}

static int batch_ok(const struct mxgpu_drm_batch *v, uint32_t *bytes)
{
    uint32_t i, total = 0;
    if (!v || !v->context_id)
        return MXGPU_DRM_ERR_STATE;
    if (!v->count || v->count > MXGPU_DRM_BATCH_MAX_COMMANDS)
        return MXGPU_DRM_ERR_RANGE;
    for (i = 0; i < v->count; i++) {
        const struct mxgpu_drm_batch_command *c = &v->commands[i];
        if (c->queue != 1u && c->queue != 3u)
            return MXGPU_DRM_ERR_RANGE;
        if (!c->command || !c->command_bytes)
            return MXGPU_DRM_ERR_STATE;
        if (c->command_bytes > MXGPU_DRM_BATCH_MAX_COMMAND_BYTES - total)
            return MXGPU_DRM_ERR_RANGE;
        total += c->command_bytes;
    }
    *bytes = total;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_batch_encode(const struct mxgpu_drm_batch *v, uint8_t *out, uint32_t cap,
                           uint32_t *len)
{
    uint32_t command_bytes, total, offset, i, j;
    int rc = batch_ok(v, &command_bytes);
    if (rc)
        return refuse(len, rc);
    total =
        MXGPU_DRM_BATCH_PREFIX_BYTES + v->count * MXGPU_DRM_BATCH_DESCRIPTOR_BYTES + command_bytes;
    if (!len || !out)
        return refuse(len, MXGPU_DRM_ERR_STATE);
    if (cap < total)
        return refuse(len, MXGPU_DRM_ERR_LENGTH);
    put_u32(out, MXGPU_DRM_KIND_SUBMIT_BATCH);
    put_u32(out + 4, total);
    put_u32(out + 8, 0);
    put_u32(out + 12, 0);
    put_u32(out + 16, v->context_id);
    put_u32(out + 20, v->count);
    put_u32(out + 24, command_bytes);
    put_u32(out + 28, 0);
    offset = MXGPU_DRM_BATCH_PREFIX_BYTES + v->count * MXGPU_DRM_BATCH_DESCRIPTOR_BYTES;
    for (i = 0; i < v->count; i++) {
        const struct mxgpu_drm_batch_command *c = &v->commands[i];
        uint8_t *d = out + MXGPU_DRM_BATCH_PREFIX_BYTES + i * MXGPU_DRM_BATCH_DESCRIPTOR_BYTES;
        put_u32(d, c->queue);
        put_u32(d + 4, offset);
        put_u32(d + 8, c->command_bytes);
        put_u32(d + 12, 0);
        put_u64(d + 16, c->fence_value);
        unsigned long destination = (unsigned long)(out + offset);
        unsigned long source = (unsigned long)c->command;
        if ((destination <= source && source - destination >= c->command_bytes) ||
            (source < destination && destination - source >= c->command_bytes))
            memcpy(out + offset, c->command, c->command_bytes);
        else
            for (j = 0; j < c->command_bytes; j++)
                out[offset + j] = c->command[j];
        offset += c->command_bytes;
    }
    *len = total;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_batch_decode(const uint8_t *in, uint32_t len, struct mxgpu_drm_batch *out)
{
    uint32_t count, bytes, offset, i;
    int rc = batch_header(in, len, MXGPU_DRM_KIND_SUBMIT_BATCH, 32);
    if (rc)
        return rc;
    if (!out)
        return MXGPU_DRM_ERR_STATE;
    count = get_u32(in + 20);
    bytes = get_u32(in + 24);
    if (!get_u32(in + 16))
        return MXGPU_DRM_ERR_STATE;
    if (get_u32(in + 28))
        return MXGPU_DRM_ERR_RESERVED;
    if (!count || count > MXGPU_DRM_BATCH_MAX_COMMANDS || bytes > MXGPU_DRM_BATCH_MAX_COMMAND_BYTES)
        return MXGPU_DRM_ERR_RANGE;
    offset = MXGPU_DRM_BATCH_PREFIX_BYTES + count * MXGPU_DRM_BATCH_DESCRIPTOR_BYTES;
    if (len != offset + bytes)
        return MXGPU_DRM_ERR_LENGTH;
    for (i = 0; i < count; i++) {
        const uint8_t *d = in + MXGPU_DRM_BATCH_PREFIX_BYTES + i * MXGPU_DRM_BATCH_DESCRIPTOR_BYTES;
        uint32_t n = get_u32(d + 8), q = get_u32(d);
        if (get_u32(d + 12))
            return MXGPU_DRM_ERR_RESERVED;
        if (q != 1u && q != 3u)
            return MXGPU_DRM_ERR_RANGE;
        if (get_u32(d + 4) != offset || !n || n > len - offset)
            return MXGPU_DRM_ERR_LENGTH;
        offset += n;
    }
    if (offset != len)
        return MXGPU_DRM_ERR_LENGTH;
    out->context_id = get_u32(in + 16);
    out->count = count;
    for (i = 0; i < count; i++) {
        const uint8_t *d = in + MXGPU_DRM_BATCH_PREFIX_BYTES + i * MXGPU_DRM_BATCH_DESCRIPTOR_BYTES;
        out->commands[i].queue = get_u32(d);
        out->commands[i].command_bytes = get_u32(d + 8);
        out->commands[i].fence_value = get_u64(d + 16);
        out->commands[i].command = in + get_u32(d + 4);
    }
    return MXGPU_DRM_OK;
}

static int batch_response_header_ok(uint32_t context_id, uint32_t count, uint32_t result)
{
    if (!context_id)
        return MXGPU_DRM_ERR_STATE;
    if (!count || count > MXGPU_DRM_BATCH_MAX_COMMANDS || result > MXGPU_DRM_BATCH_TERMINAL)
        return MXGPU_DRM_ERR_RANGE;
    return MXGPU_DRM_OK;
}

static int batch_outcome_ok(const struct mxgpu_drm_batch_outcome *o, uint32_t generation,
                            uint32_t result, uint32_t *unposted)
{
    if (o->state > MXGPU_DRM_BATCH_COMPLETED)
        return MXGPU_DRM_ERR_RANGE;
    if (o->state == MXGPU_DRM_BATCH_NOT_POSTED) {
        *unposted = 1;
        if (o->status || o->sequence || o->fence_value || o->device_generation)
            return MXGPU_DRM_ERR_STATE;
    } else {
        if (*unposted || !o->sequence || o->device_generation != generation)
            return MXGPU_DRM_ERR_STATE;
        if (o->state == MXGPU_DRM_BATCH_PENDING && o->status)
            return MXGPU_DRM_ERR_STATE;
    }
    if (result == MXGPU_DRM_BATCH_COMPLETE && (o->state != MXGPU_DRM_BATCH_COMPLETED || o->status))
        return MXGPU_DRM_ERR_STATE;
    if (result == MXGPU_DRM_BATCH_REJECTED && o->state != MXGPU_DRM_BATCH_NOT_POSTED)
        return MXGPU_DRM_ERR_STATE;
    return MXGPU_DRM_OK;
}

static int batch_response_ok(const struct mxgpu_drm_batch_response *v)
{
    uint32_t i, unposted = 0;
    int rc;
    if (!v)
        return MXGPU_DRM_ERR_STATE;
    rc = batch_response_header_ok(v->context_id, v->count, v->aggregate_result);
    if (rc)
        return rc;
    for (i = 0; i < v->count; i++) {
        rc =
            batch_outcome_ok(&v->outcomes[i], v->device_generation, v->aggregate_result, &unposted);
        if (rc)
            return rc;
    }
    return MXGPU_DRM_OK;
}

int mxgpu_drm_batch_response_encode(const struct mxgpu_drm_batch_response *v, uint8_t *out,
                                    uint32_t cap, uint32_t *len)
{
    uint32_t i, total;
    int rc = batch_response_ok(v);
    if (rc)
        return refuse(len, rc);
    total = MXGPU_DRM_BATCH_PREFIX_BYTES + v->count * MXGPU_DRM_BATCH_OUTCOME_BYTES;
    if (!out || !len)
        return refuse(len, MXGPU_DRM_ERR_STATE);
    if (cap < total)
        return refuse(len, MXGPU_DRM_ERR_LENGTH);
    put_u32(out, MXGPU_DRM_KIND_SUBMIT_BATCH);
    put_u32(out + 4, total);
    put_u32(out + 8, 0);
    put_u32(out + 12, 0);
    put_u32(out + 16, v->context_id);
    put_u32(out + 20, v->count);
    put_u32(out + 24, v->device_generation);
    put_u32(out + 28, v->aggregate_result);
    for (i = 0; i < v->count; i++) {
        const struct mxgpu_drm_batch_outcome *o = &v->outcomes[i];
        uint8_t *d = out + MXGPU_DRM_BATCH_PREFIX_BYTES + i * MXGPU_DRM_BATCH_OUTCOME_BYTES;
        put_u32(d, o->state);
        put_u32(d + 4, o->status);
        put_u64(d + 8, o->sequence);
        put_u64(d + 16, o->fence_value);
        put_u32(d + 24, o->device_generation);
        put_u32(d + 28, 0);
    }
    *len = total;
    return MXGPU_DRM_OK;
}

static void batch_outcome_read(const uint8_t *in, struct mxgpu_drm_batch_outcome *out)
{
    memset(out, 0, sizeof *out);
    out->state = get_u32(in);
    out->status = get_u32(in + 4);
    out->sequence = get_u64(in + 8);
    out->fence_value = get_u64(in + 16);
    out->device_generation = get_u32(in + 24);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__noinline__))
#endif
static void batch_response_alias_tail(const uint8_t *in, uint32_t context_id, uint32_t count,
                                      uint32_t generation, uint32_t result,
                                      const struct mxgpu_drm_batch_outcome *first,
                                      struct mxgpu_drm_batch_response *out)
{
    struct mxgpu_drm_batch_outcome last[16] = {{0}};
    uint32_t i;
    for (i = 16; i < count; i++)
        batch_outcome_read(in + MXGPU_DRM_BATCH_PREFIX_BYTES + i * MXGPU_DRM_BATCH_OUTCOME_BYTES,
                           &last[i - 16]);
    memset(out, 0, sizeof *out);
    out->context_id = context_id;
    out->count = count;
    out->device_generation = generation;
    out->aggregate_result = result;
    memcpy(out->outcomes, first, 16 * sizeof *first);
    memcpy(out->outcomes + 16, last, sizeof last);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__noinline__))
#endif
static void batch_response_alias(const uint8_t *in, uint32_t context_id, uint32_t count,
                                 uint32_t generation, uint32_t result,
                                 struct mxgpu_drm_batch_response *out)
{
    struct mxgpu_drm_batch_outcome first[16] = {{0}};
    uint32_t i;
    for (i = 0; i < count && i < 16; i++)
        batch_outcome_read(in + MXGPU_DRM_BATCH_PREFIX_BYTES + i * MXGPU_DRM_BATCH_OUTCOME_BYTES,
                           &first[i]);
    batch_response_alias_tail(in, context_id, count, generation, result, first, out);
}

int mxgpu_drm_batch_response_decode(const uint8_t *in, uint32_t len,
                                    struct mxgpu_drm_batch_response *out)
{
    struct mxgpu_drm_batch_outcome outcome;
    uint32_t i, count, context_id, generation, result, unposted = 0;
    int rc = batch_header(in, len, MXGPU_DRM_KIND_SUBMIT_BATCH, MXGPU_DRM_BATCH_PREFIX_BYTES);
    if (rc)
        return rc;
    if (!out)
        return MXGPU_DRM_ERR_STATE;
    context_id = get_u32(in + 16);
    count = get_u32(in + 20);
    generation = get_u32(in + 24);
    result = get_u32(in + 28);
    if (!count || count > MXGPU_DRM_BATCH_MAX_COMMANDS)
        return MXGPU_DRM_ERR_RANGE;
    if (len != MXGPU_DRM_BATCH_PREFIX_BYTES + count * MXGPU_DRM_BATCH_OUTCOME_BYTES)
        return MXGPU_DRM_ERR_LENGTH;
    for (i = 0; i < count; i++)
        if (get_u32(in + MXGPU_DRM_BATCH_PREFIX_BYTES + i * MXGPU_DRM_BATCH_OUTCOME_BYTES + 28))
            return MXGPU_DRM_ERR_RESERVED;
    rc = batch_response_header_ok(context_id, count, result);
    if (rc)
        return rc;
    for (i = 0; i < count; i++) {
        batch_outcome_read(in + MXGPU_DRM_BATCH_PREFIX_BYTES + i * MXGPU_DRM_BATCH_OUTCOME_BYTES,
                           &outcome);
        rc = batch_outcome_ok(&outcome, generation, result, &unposted);
        if (rc)
            return rc;
    }
    {
        unsigned long input = (unsigned long)in, output = (unsigned long)out;
        if ((output <= input && input - output < sizeof *out) ||
            (input < output && output - input < len)) {
            batch_response_alias(in, context_id, count, generation, result, out);
            return MXGPU_DRM_OK;
        }
    }
    memset(out, 0, sizeof *out);
    out->context_id = context_id;
    out->count = count;
    out->device_generation = generation;
    out->aggregate_result = result;
    for (i = 0; i < count; i++)
        batch_outcome_read(in + MXGPU_DRM_BATCH_PREFIX_BYTES + i * MXGPU_DRM_BATCH_OUTCOME_BYTES,
                           &out->outcomes[i]);
    return MXGPU_DRM_OK;
}

int mxgpu_drm_get_batch_limits_decode(const uint8_t *in, uint32_t len)
{
    int rc = batch_header(in, len, MXGPU_DRM_KIND_GET_BATCH_LIMITS, MXGPU_DRM_HEADER_BYTES);
    if (rc)
        return rc;
    return len == MXGPU_DRM_HEADER_BYTES ? MXGPU_DRM_OK : MXGPU_DRM_ERR_LENGTH;
}

static int compute_limits_ok(const struct mxgpu_drm_compute_limits *v)
{
    if (!v)
        return MXGPU_DRM_ERR_STATE;
    if (!v->max_work_group_size[0] || !v->max_work_group_size[1] || !v->max_work_group_size[2] ||
        !v->max_work_group_invocations)
        return MXGPU_DRM_ERR_RANGE;
    return MXGPU_DRM_OK;
}

int mxgpu_drm_get_compute_limits_encode(uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_GET_COMPUTE_LIMITS, 0, 0, 0, 0);
}

int mxgpu_drm_get_compute_limits_decode(const uint8_t *in, uint32_t len)
{
    int rc = batch_header(in, len, MXGPU_DRM_KIND_GET_COMPUTE_LIMITS, MXGPU_DRM_HEADER_BYTES);
    if (rc)
        return rc;
    return len == MXGPU_DRM_HEADER_BYTES ? MXGPU_DRM_OK : MXGPU_DRM_ERR_LENGTH;
}

int mxgpu_drm_get_compute_limits_response_encode(const struct mxgpu_drm_compute_limits *in,
                                                 uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint8_t body[MXGPU_DRM_COMPUTE_LIMITS_BYTES];
    int rc = compute_limits_ok(in);
    if (rc)
        return refuse(out_len, rc);
    put_u32(body, in->max_work_group_size[0]);
    put_u32(body + 4, in->max_work_group_size[1]);
    put_u32(body + 8, in->max_work_group_size[2]);
    put_u32(body + 12, in->max_work_group_invocations);
    return write_record(out, cap, out_len, MXGPU_DRM_KIND_GET_COMPUTE_LIMITS, body, sizeof(body),
                        0, 0);
}

int mxgpu_drm_get_compute_limits_response_decode(const uint8_t *in, uint32_t len,
                                                 struct mxgpu_drm_compute_limits *out)
{
    struct mxgpu_drm_compute_limits v;
    int rc = batch_header(in, len, MXGPU_DRM_KIND_GET_COMPUTE_LIMITS,
                          MXGPU_DRM_HEADER_BYTES + MXGPU_DRM_COMPUTE_LIMITS_BYTES);
    if (rc)
        return rc;
    if (len != MXGPU_DRM_HEADER_BYTES + MXGPU_DRM_COMPUTE_LIMITS_BYTES)
        return MXGPU_DRM_ERR_LENGTH;
    if (!out)
        return MXGPU_DRM_ERR_STATE;
    v.max_work_group_size[0] = get_u32(in + 16);
    v.max_work_group_size[1] = get_u32(in + 20);
    v.max_work_group_size[2] = get_u32(in + 24);
    v.max_work_group_invocations = get_u32(in + 28);
    rc = compute_limits_ok(&v);
    if (!rc)
        *out = v;
    return rc;
}
