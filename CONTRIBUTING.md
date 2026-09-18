<!-- REUSE-IgnoreStart -->
# Contributing to mx-guest-linux-common

Read [README.md](README.md) first. This repository holds only the two Linux kernel-to-userspace ABIs and their version constants. Implementation on either side of an ABI belongs in the kernel or agent repository, and anything that is identical on every guest operating system belongs in mx-guest-core.

## ABI changes

- Both sides compile these files: the kernel modules in kernel context and the userspace consumers (the Mesa driver and the agent) in userspace. A change must build in both.
- A change that alters what either side may send or accept changes the ABI's version constants in the same commit.
- Where a record has an encoder and decoder pair here, a change to the record changes both, and the tests that check one side against the other.
- Say in the commit message which consumers must move their pin, and whether an older consumer remains compatible.

## Developer Certificate of Origin

Contributions are accepted under the Developer Certificate of Origin, version 1.1: https://developercertificate.org

Read the full text before signing off. Adding a sign-off to a commit is your certification of that text for that commit.

Sign off every commit with:

```
git commit -s
```

This appends a trailer of exactly this form to the commit message:

```
Signed-off-by: Name <email>
```

The name and email in the trailer must match the commit's author name and author email exactly, including case. The DCO check (`.github/workflows/dco.yml`) runs on every pull request, examines every non-merge commit in it, and fails the pull request if any commit lacks a `Signed-off-by` trailer equal to that commit's `Author Name <author email>`. The check runs only on pull requests. Maintainers who push directly to a branch must still sign off every commit; the requirement is the same whether or not the check runs.

`git commit -s` writes the trailer from your configured `user.name` and `user.email`. If the commit's author is someone else, for example when you commit a change on another person's behalf, the author must add their own sign-off. To add missing sign-offs to your own commits on a branch, use `git rebase --signoff <base>` or, for the last commit only, `git commit --amend -s --no-edit`, then force-push the branch.

## Licence headers

Every new source file carries a two-line SPDX header as its first lines. For C sources and headers:

```c
/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
```

For files that use `#` comments, such as build files and scripts:

```
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Zak Noble-Clarke
```

If you hold copyright in your contribution to a file, add your own `SPDX-FileCopyrightText: <year> <name>` line below the existing ones. Never remove or alter an existing copyright or licence line.

Documentation and repository metadata that cannot carry a header are listed in `REUSE.toml`. Do not add a new header-less file without adding it there, and do not use `REUSE.toml` to avoid putting a header on a source file.

## REUSE compliance

`reuse lint` must pass. It runs in CI (`.github/workflows/reuse.yml`) on every push and pull request. Run it locally before pushing; the tool is described at https://reuse.software.

## Do not copy code from other projects

Write the code yourself. Do not paste or adapt code from the Linux kernel, libdrm, Mesa or any other project, including other drivers' uapi headers, even where the licence would appear to permit it.

These files are offered to everyone under MIT and are compiled into GPL-2.0-only kernel modules, a GPL-2.0-only agent and a public Mesa fork. Code whose owner never agreed to MIT terms would propagate into all of them and be hard to withdraw once released. A uapi header is exactly where an existing driver's header is a tempting starting point, and it is exactly where copying would import someone else's licence into an MIT file.

A similarity gate that scans changes against a corpus of plausible third-party sources is part of the planned CI for MX's MIT protocol code. It is not implemented, and a clean run would not in any case prove provenance. The rule stands on its own.

## Material under another licence

No material under a licence other than MIT may enter this repository without an entry in `THIRD-PARTY-NOTICES` in the same commit, naming the work, its SPDX identifier, its copyright holders and its source location, with the original notices kept intact. Raise any such case before opening a pull request.

<!-- REUSE-IgnoreEnd -->
