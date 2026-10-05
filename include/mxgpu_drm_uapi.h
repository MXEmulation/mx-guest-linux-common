/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#ifndef MXGPU_DRM_UAPI_H
#define MXGPU_DRM_UAPI_H

#ifdef __KERNEL__
#include <linux/types.h>
#ifndef MXGPU_UINT_DEFINED
#define MXGPU_UINT_DEFINED
typedef u8 uint8_t;
typedef u16 uint16_t;
typedef u32 uint32_t;
typedef u64 uint64_t;
#endif
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MXGPU_DRIVER_VERSION_MAJOR 1
#define MXGPU_DRIVER_VERSION_MINOR 0
#define MXGPU_DRIVER_VERSION_PATCH 0
#define MXGPU_DRIVER_VERSION "1.0.0"

#define MXGPU_DRM_OK 0
#define MXGPU_DRM_ERR_LENGTH 1
#define MXGPU_DRM_ERR_RESERVED 2
#define MXGPU_DRM_ERR_KIND 3
#define MXGPU_DRM_ERR_STATE 4
#define MXGPU_DRM_ERR_RANGE 5

#define MXGPU_DRM_KIND_GET_INFO 1u
#define MXGPU_DRM_KIND_CTX_CREATE 2u
#define MXGPU_DRM_KIND_CTX_DESTROY 3u
#define MXGPU_DRM_KIND_GEM_CREATE 4u
#define MXGPU_DRM_KIND_GEM_CLOSE 5u
#define MXGPU_DRM_KIND_SUBMIT 6u
#define MXGPU_DRM_KIND_WAIT 7u
#define MXGPU_DRM_KIND_BO_WRITE 8u
#define MXGPU_DRM_KIND_BO_READ 9u
#define MXGPU_DRM_KIND_GET_CAPS 10u
#define MXGPU_DRM_KIND_GET_TRANSFER_LIMITS 11u

#define MXGPU_DRM_ABI_MAJOR 1u
#define MXGPU_DRM_ABI_MINOR 1u
#define MXGPU_DRM_ABI_MINIMUM_MINOR 0u
#define MXGPU_DRM_ABI_BATCH_MINOR 1u
#define MXGPU_DRM_IOCTL_GET_BATCH_LIMITS 11u
#define MXGPU_DRM_IOCTL_SUBMIT_BATCH 12u
#define MXGPU_DRM_KIND_GET_BATCH_LIMITS 12u
#define MXGPU_DRM_KIND_SUBMIT_BATCH 13u
#define MXGPU_DRM_BATCH_MAX_COMMANDS 32u
#define MXGPU_DRM_BATCH_MAX_COMMAND_BYTES 1048576u
#define MXGPU_DRM_BATCH_DESCRIPTOR_BYTES 24u
#define MXGPU_DRM_BATCH_OUTCOME_BYTES 32u
#define MXGPU_DRM_BATCH_PREFIX_BYTES 32u
#define MXGPU_DRM_BATCH_MAX_INPUT_BYTES                                                            \
    (MXGPU_DRM_BATCH_PREFIX_BYTES +                                                                \
     MXGPU_DRM_BATCH_MAX_COMMANDS * MXGPU_DRM_BATCH_DESCRIPTOR_BYTES +                             \
     MXGPU_DRM_BATCH_MAX_COMMAND_BYTES)
#define MXGPU_DRM_BATCH_NOT_POSTED 0u
#define MXGPU_DRM_BATCH_PENDING 1u
#define MXGPU_DRM_BATCH_COMPLETED 2u
#define MXGPU_DRM_BATCH_COMPLETE 0u
#define MXGPU_DRM_BATCH_REJECTED 1u
#define MXGPU_DRM_BATCH_TERMINAL 2u

struct mxgpu_drm_batch_limits {
    uint32_t abi_major, abi_minor, max_commands, max_command_bytes;
    uint32_t max_input_bytes, outcome_bytes;
};
struct mxgpu_drm_batch_command {
    uint32_t queue, command_bytes;
    uint64_t fence_value;
    const uint8_t *command;
};
struct mxgpu_drm_batch {
    uint32_t context_id, count;
    struct mxgpu_drm_batch_command commands[MXGPU_DRM_BATCH_MAX_COMMANDS];
};
struct mxgpu_drm_batch_outcome {
    uint32_t state, status;
    uint64_t sequence, fence_value;
    uint32_t device_generation;
};
struct mxgpu_drm_batch_response {
    uint32_t context_id, count, device_generation, aggregate_result;
    struct mxgpu_drm_batch_outcome outcomes[MXGPU_DRM_BATCH_MAX_COMMANDS];
};
int mxgpu_drm_get_batch_limits_encode(uint8_t *, uint32_t, uint32_t *);
int mxgpu_drm_get_batch_limits_decode(const uint8_t *, uint32_t);
int mxgpu_drm_get_batch_limits_response_encode(const struct mxgpu_drm_batch_limits *, uint8_t *,
                                               uint32_t, uint32_t *);
int mxgpu_drm_get_batch_limits_response_decode(const uint8_t *, uint32_t,
                                               struct mxgpu_drm_batch_limits *);
int mxgpu_drm_batch_encode(const struct mxgpu_drm_batch *, uint8_t *, uint32_t, uint32_t *);
int mxgpu_drm_batch_decode(const uint8_t *, uint32_t, struct mxgpu_drm_batch *);
int mxgpu_drm_batch_response_encode(const struct mxgpu_drm_batch_response *, uint8_t *, uint32_t,
                                    uint32_t *);
int mxgpu_drm_batch_response_decode(const uint8_t *, uint32_t, struct mxgpu_drm_batch_response *);

#define MXGPU_DRM_HEADER_BYTES 16u
#define MXGPU_DRM_HEADER_SIZE MXGPU_DRM_HEADER_BYTES
#define MXGPU_DRM_QUEUE_MAX 5u
#define MXGPU_DRM_GEM_ALIGN 4096u
#define MXGPU_DRM_PAGE_SIZE MXGPU_DRM_GEM_ALIGN

struct mxgpu_drm_user {
    uint64_t pointer;
    uint32_t size;
    uint32_t capacity;
};

struct mxgpu_drm_caps {
    uint32_t major;
    uint32_t minor;
    uint64_t features;
    uint32_t max_command_bytes;
    uint32_t max_queues;
    uint32_t max_descriptors_per_queue;
};

struct mxgpu_drm_transfer_limits {
    uint32_t max_transfer_to_host_bytes;
    uint32_t max_transfer_from_host_bytes;
};

int mxgpu_drm_get_transfer_limits_encode(uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_drm_get_transfer_limits_response_encode(const struct mxgpu_drm_transfer_limits *in,
                                                  uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_drm_get_transfer_limits_response_decode(const uint8_t *in, uint32_t len,
                                                  struct mxgpu_drm_transfer_limits *out);

int mxgpu_drm_get_caps_encode(uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_drm_get_caps_response_encode(const struct mxgpu_drm_caps *in, uint8_t *out, uint32_t cap,
                                       uint32_t *out_len);
int mxgpu_drm_get_caps_response_decode(const uint8_t *in, uint32_t len, struct mxgpu_drm_caps *out);

/* Every record is a 16-byte header (kind, byte_len, flags, reserved)
 * plus the kind's payload. byte_len counts the header. reserved must be 0.
 * Encoders write little-endian bytes and flags 0. A refusal sets *out_len
 * to 0 and leaves the output buffer untouched.
 */

int mxgpu_drm_get_info_encode(uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_drm_get_info_response_encode(uint32_t major, uint32_t minor, uint32_t patch, uint8_t *out,
                                       uint32_t cap, uint32_t *out_len);
int mxgpu_drm_get_info_response_decode(const uint8_t *in, uint32_t len, uint32_t *major,
                                       uint32_t *minor, uint32_t *patch);

int mxgpu_drm_ctx_create_encode(uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_drm_ctx_create_response_encode(uint32_t context_id, uint8_t *out, uint32_t cap,
                                         uint32_t *out_len);
int mxgpu_drm_ctx_create_response_decode(const uint8_t *in, uint32_t len, uint32_t *context_id);

int mxgpu_drm_ctx_destroy_encode(uint32_t context_id, uint8_t *out, uint32_t cap,
                                 uint32_t *out_len);
int mxgpu_drm_ctx_destroy_decode(const uint8_t *in, uint32_t len, uint32_t *context_id);

int mxgpu_drm_gem_create_encode(uint64_t size, uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_drm_gem_create_decode(const uint8_t *in, uint32_t len, uint64_t *size);
int mxgpu_drm_gem_create_response_encode(uint32_t handle, uint8_t *out, uint32_t cap,
                                         uint32_t *out_len);
int mxgpu_drm_gem_create_response_decode(const uint8_t *in, uint32_t len, uint32_t *handle);

int mxgpu_drm_gem_close_encode(uint32_t handle, uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_drm_gem_close_decode(const uint8_t *in, uint32_t len, uint32_t *handle);

int mxgpu_drm_submit_encode(uint32_t context_id, uint32_t queue, uint64_t fence_value,
                            uint32_t response_cap, const uint8_t *command, uint32_t command_bytes,
                            uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_drm_submit_decode(const uint8_t *in, uint32_t len, uint32_t *context_id, uint32_t *queue,
                            uint64_t *fence_value, uint32_t *response_cap, const uint8_t **command,
                            uint32_t *command_bytes);

int mxgpu_drm_submit_response_encode(uint32_t completion_status, uint64_t completed_fence,
                                     const uint8_t *response, uint32_t response_written,
                                     uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_drm_submit_response_decode(const uint8_t *in, uint32_t len, uint32_t *completion_status,
                                     uint64_t *completed_fence, const uint8_t **response,
                                     uint32_t *response_written);

int mxgpu_drm_wait_encode(uint32_t context_id, uint32_t queue, uint64_t fence_value,
                          uint32_t timeout_ms, uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_drm_wait_decode(const uint8_t *in, uint32_t len, uint32_t *context_id, uint32_t *queue,
                          uint64_t *fence_value, uint32_t *timeout_ms);

int mxgpu_drm_bo_write_encode(uint32_t handle, uint64_t offset, const uint8_t *data, uint32_t size,
                              uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_drm_bo_write_decode(const uint8_t *in, uint32_t len, uint32_t *handle, uint64_t *offset,
                              const uint8_t **data, uint32_t *size);

int mxgpu_drm_bo_read_encode(uint32_t handle, uint64_t offset, uint32_t size, uint8_t *out,
                             uint32_t cap, uint32_t *out_len);
int mxgpu_drm_bo_read_decode(const uint8_t *in, uint32_t len, uint32_t *handle, uint64_t *offset,
                             uint32_t *size);

#ifdef __cplusplus
}
#endif

#endif
