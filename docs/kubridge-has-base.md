# kubridge HAS_BASE emulation

`kuKernelMemCommit(..., KU_KERNEL_MEM_COMMIT_ATTR_HAS_BASE)` maps a range to
shared backing pages from the specified block UID and `baseOffset`. It supports
ordinary Sysmem blocks and kubridge reservations, multiple aliases, remapping,
and offsets aligned to the Vita's 4 KiB page size. Invalid block IDs, options,
ranges and offsets return errors instead of reporting a successful no-op.

Reference: [kubridge mprotect.c, commit 417ddde9](https://github.com/bythos14/kubridge/blob/417ddde9a744eba98d769382c1c1372b8e119139/src/mprotect.c).

## Memory and CPU behavior

The sparse guest mapping table is separate from the GPU external mapping table.
Creating an alias does not copy the destination over the source. Each alias has
its own read/write permissions. Decommit removes that virtual mapping; other
aliases keep their backing. Before a native allocation is released or reused,
backing pages still referenced by aliases are retained.

Guest accesses split at 4 KiB boundaries, including on macOS hosts with 16 KiB
pages. A store is checked in full before it changes memory. `Ptr::get()` resolves
the first page for HLE callers; CPU accesses use the range-aware API. Exclusive
stores hold the mapping lock across permission checking, translation and CAS.

The first kubridge reservation/commit/protection switches Dynarmic to checked
memory callbacks, preserving CPU context. Processes that do not use these
operations keep their existing memory path. Cache invalidation clears all JIT
blocks in checked mode because executable bytes may be changed through any
alias. Get/set elimination is disabled in this mode to preserve registers at a
faulting instruction.

A reproducible patch in `external/patches/dynarmic-arm64-callback-abort.patch`
adds missing immediate abort checks to Dynarmic's ARM64 callback-only paths
(read, write and exclusive accesses). CMake copies the pinned checkout into
the build directory, applies the patch there with a dry-run conflict check,
and builds that copy. The pinned Dynarmic revision is
`86458a0bd369d63ba4c2ef812cacbb6c9080c065`; the submodule remains clean. Clean
application, repeated configure, already-patched rejection and byte-for-byte
source comparison were tested.

Data aborts use Dynarmic's memory-abort exit and pass the complete 0x158-byte
kubridge context to the handler. Its stack is below the saved context. Handler
changes to GPRs, PC, SPSR and VFP/FPSCR are applied on return. An unhandled data
abort terminates the guest thread rather than retrying forever.

## Scope

This is a functional compatibility path, with callback and full-cache-flush
costs. It is not a performance-equivalent host fastmem implementation.

Execute-only permissions, prefetch aborts and undefined-instruction handler
dispatch are not implemented. Raw HLE pointers spanning discontiguous mapped
pages are not made contiguous: such callers must use `access_guest_memory`.
GPU alias coherence and cross-thread exclusive-monitor behavior between
*different virtual aliases* are not validated by the memory/probe tests.

## Validation

```sh
cmake --build build/macos --target mem-tests vita3k --parallel 4
ctest --test-dir build/macos -R '^mem$' --output-on-failure
```

The integration fixture is the standalone `fastmem_probe` in the sibling
VitaSanshiro project. It checks bidirectional aliases, page crossings, offset
remapping, independent protection, exact A32 abort PC/FAR and recovery,
backpatching, code publication/rewrite through aliases, and decommit. Its VPK
must be kept identical when comparing the old and updated Vita3K binaries.

Validated on macOS ARM64, 2026-09-19: **22/22 memory tests passed**, including
9 new cases exercised with and without the native page table. The unchanged
`fastmem-probe-20260919-a1-1a-r2` VPK changed from 15 alias failures on the old
binary to **PASS, fail=0, skipped=0, exit=0** on the final build. See the
[validation receipt and hashes](kubridge-has-base-validation.json) and
[complete probe log](kubridge-has-base-probe.txt). No x86_64/Windows build or
performance measurement was performed. The existing macdeployqt packaging
step reported missing `liblzma.5.dylib`; the resulting bundle nevertheless
launched and completed this probe.
