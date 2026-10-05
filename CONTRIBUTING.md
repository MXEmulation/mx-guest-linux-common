<!-- REUSE-IgnoreStart -->
# Contributing to mx-guest-linux-common

Read [README.md](README.md). This repository owns Linux kernel-to-userspace ABI records and versions. OS-neutral device protocol belongs in Core; scheduling, resource management and daemon routing belong in their consumers.

- Changes must build in kernel and userspace contexts.
- Update ABI versions when accepted or emitted records change.
- Update both codecs and their tests when a record changes.
- List every C source and header once in `sources.list`.
- Identify affected consumer pins and compatibility in the commit description.

The repository licence is MIT.

## Contributions

Read the [Developer Certificate of Origin 1.1](https://developercertificate.org) before signing off. Every commit requires a `Signed-off-by` trailer matching its author's name and email. Use `git commit -s`; the pull-request DCO workflow checks this requirement.

Write original implementation code. Do not paste or adapt code from other projects; use their supported interfaces. Record any introduced third-party material in `THIRD-PARTY-NOTICES`, retaining its original notices, licence identifier, copyright holders and source location. Discuss material under another licence before adding it.

## File notices and checks

New source files carry this repository's SPDX licence identifier and copyright notice in the file's comment syntax. Preserve existing notices; add a contributor's copyright when appropriate. Files that cannot carry comments are annotated in `REUSE.toml`.

Run the component checks described in [README.md](README.md) and `reuse lint` before submitting. REUSE runs on pushes and pull requests.

<!-- REUSE-IgnoreEnd -->
