<!-- REUSE-IgnoreStart -->
# mx-guest-linux-common

Public Linux DRM ioctl records and byte codecs shared by the MXGPU kernel module and userspace graphics drivers.

[`include/mxgpu_drm_uapi.h`](include/mxgpu_drm_uapi.h) defines the ioctl indices, record layouts and ABI versions. [`src/mxgpu_drm_abi.c`](src/mxgpu_drm_abi.c) implements their encoders and decoders. The local guest-agent ABI is not implemented.

## Build and tests

A C11 compiler and Make are required.

```sh
make test
```

The tests cover records, batch requests and responses, malformed input and overlapping decoder input/output.

## Consumption

Consumers include the public headers and compile the `src/*.c` entries in [`sources.list`](sources.list). The manifest lists every C source and header once, including tests. Kernel and userspace consumers must use compatible ABI versions.

[mx-guest-linux-kernel](https://github.com/MXEmulation/mx-guest-linux-kernel) and [mx-guest-linux-agent](https://github.com/MXEmulation/mx-guest-linux-agent) pin this repository at `deps/linux-common`. [mesa-mxgpu](https://github.com/MXEmulation/mesa-mxgpu) pins it through a Meson wrap.

## Optional DRM batches

ABI 1.1 adds `GET_BATCH_LIMITS` and `SUBMIT_BATCH`. An unavailable limits query or a zero `max_commands` selects legacy submission. A supported batch contains zero-response transfers to the host or extended render commands without vertex layouts whose objects, including any depth-stencil target and state, belong to one owned context, and preserves each command's logical sequence and fence.

The kernel validates the complete request before posting. Responses identify each command as not posted, pending or completed and retain the actual completion status. A terminal submission failure faults its context; callers must not treat a partial batch as completed. Supported commands, limits and record fields are declared in the public header.

## Licence

MIT. See [LICENCE](LICENCE) and [THIRD-PARTY-NOTICES](THIRD-PARTY-NOTICES). Contribution requirements are in [CONTRIBUTING.md](CONTRIBUTING.md).

<!-- REUSE-IgnoreEnd -->
