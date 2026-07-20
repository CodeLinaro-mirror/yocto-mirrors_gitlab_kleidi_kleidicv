<!--
SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# Overview {.chapter}

KleidiCV is Arm's high-performance image processing library for AArch64. It
exposes a C API and has only a standard C runtime dependency, making it easy to
integrate into existing pipelines. KleidiCV integrates with CV frameworks such
as OpenCV. It selects optimized implementations at runtime based on available
Arm CPU features.

KleidiCV uses Arm C Language Extensions (ACLE) intrinsics and provides
implementations targeting Neon, SVE2, SME, and SME2. Runtime dispatch selects
an appropriate backend for each API. It focuses on performance-critical,
low-level operations such as color conversions, filtering, morphology, resize,
and geometric transforms.

This documentation covers the public C API:

- Function groups (conversions, filters, morphology, resize, transform,
  analysis, arithmetics)
- Supported data types and constraints for each operation
- Required types, constants, and error handling
- Per-function reference pages

Platform support is organized into tiers (Tier 1 includes AArch64 Ubuntu on
Neoverse N1 and Android on Galaxy S22; Tier 3 includes Apple silicon and
Windows Arm). Integration examples are available, including OpenCV HAL
integration and extracting a single operation into a standalone library.

For build, test, benchmark, and integration workflows, refer to the main
KleidiCV documentation at <https://gitlab.arm.com/kleidi/kleidicv>.

## Backend Selection {.section}

### Single-threaded APIs {.subsection}

Most APIs are exposed in a default form, for example
`kleidicv_resize_linear_u8(...)`. This form uses the default runtime dispatch
path, which prefers Neon or SVE2 backends.

Some APIs also provide a variant whose name ends in `_sme`, for example
`kleidicv_resize_linear_u8_sme(...)`. The `_sme` suffix does not change the
algorithm, parameter contract, or result format. It only changes backend
selection: the `_sme` form prefers SME or SME2 backends when they are
available, and otherwise falls back to the same non-SME paths as the default
form.

The default form can be made to use the SME-preferred dispatch path by setting
`KLEIDICV_PREFER_SME_BACKEND=ON` before the library is loaded. This setting only
affects APIs that provide an `_sme` variant.

### Multithreaded APIs {.subsection}

KleidiCV provides multithreaded APIs but does not create or assign worker
threads. CPU placement is the responsibility of the application-provided
scheduler. See [Threading](threading.md) for details.

On Linux and Android, selected `kleidicv_thread_*` APIs automatically use SME
or SME2 implementations when an SME backend is enabled at build time and
supported by the system.

KleidiCV detects the system's SME compute-unit topology and limits concurrent
SME work independently for each compute-unit domain. Each work item uses SME
only when it is running on a supported CPU and a slot is available in that
CPU's domain. If SME cannot be used (for example, because the topology cannot be
detected, the CPU is not eligible, or all slots in its domain are occupied) the
work item immediately uses the corresponding non-SME implementation instead.

This limiting mechanism is a mitigation rather than a strict guarantee. If the
operating system migrates a worker thread to a CPU in another compute-unit
domain after the work item has acquired a slot, the recorded domain may no
longer match the CPU on which the SME work runs, potentially defeating the
intended concurrency limit. The mitigation assumes that worker threads handling
these computer-vision workloads are short-lived, reducing the opportunity for
such migrations to occur.

Selection is performed separately for each work item, so a multithreaded
operation may use SME or SME2 and non-SME implementations concurrently.

The `KLEIDICV_PREFER_SME_BACKEND` environment variable should not be used with
multithreaded APIs. These APIs select SME automatically and apply the required
topology-aware concurrency limits. Enabling the variable may cause non-SME
fallback paths to select SME without those limits.
