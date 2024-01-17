// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_CONFIG_H
#define INTRINSICCV_CONFIG_H

// Main configuration switches.

// Set to '1' if interleaving loads and stores are preferred, otherwise it is
// set to '0'.
#ifndef INTRINSICCV_PREFER_INTERLEAVING_LOAD_STORE
#define INTRINSICCV_PREFER_INTERLEAVING_LOAD_STORE 0
#endif

// Set to '1' if 128-bit SVE2 VL is assumed, otherwise it is set to '0'.
#ifndef INTRINSICCV_SVE2_128
#define INTRINSICCV_SVE2_128 0
#endif

// Set to '1' if compiling NEON code paths, otherwise it is set to '0'.
#ifndef INTRINSICCV_TARGET_NEON
#define INTRINSICCV_TARGET_NEON 0
#endif

// Set to '1' if compiling SVE2 code paths, otherwise it is set to '0'.
#ifndef INTRINSICCV_TARGET_SVE2
#define INTRINSICCV_TARGET_SVE2 0
#endif

// Set to '1' if compiling SME2 code paths, otherwise it is set to '0'.
#ifndef INTRINSICCV_TARGET_SME2
#define INTRINSICCV_TARGET_SME2 0
#endif

// Set to '1' to always enable SVE2 code paths, otherwise enable them
// selectively.
#ifndef INTRINSICCV_ALWAYS_ENABLE_SVE2
#define INTRINSICCV_ALWAYS_ENABLE_SVE2 0
#endif

// OpenCV requires that diagonal non-maxima-suppressions are calculated as
//    curr > prev && curr > next
// This is different from other directions where
//    curr > prev && curr >= next
// is used. Furthermore, the elements in the diagonal directions are reversed.
#define INTRINSICCV_DIRECTIONAL_MASKING_CONFORM_OPENCV 1

// Derived configuration switches and macros below.

#if INTRINSICCV_TARGET_NEON
#define INTRINSICCV_TARGET_FN_ATTS INTRINSICCV_ATTR_SECTION(".text.neon")
#endif

#if INTRINSICCV_TARGET_SVE2
#define INTRINSICCV_TARGET_FN_ATTS INTRINSICCV_ATTR_SECTION(".text.sve2")
#endif

#if INTRINSICCV_TARGET_SME2
#undef INTRINSICCV_SVE2_128
#define INTRINSICCV_SVE2_128 0
#define INTRINSICCV_TARGET_FN_ATTS INTRINSICCV_ATTR_SECTION(".text.sme2")
#define INTRINSICCV_LOCALLY_STREAMING __arm_locally_streaming
#define INTRINSICCV_STREAMING_COMPATIBLE __arm_streaming_compatible
#else
#define INTRINSICCV_LOCALLY_STREAMING
#define INTRINSICCV_STREAMING_COMPATIBLE
#endif

#define INTRINSICCV_ATTR_IFUNC(resolver) __attribute__((ifunc(resolver)))
#define INTRINSICCV_ATTR_SECTION(section_to_use) \
  __attribute__((section(section_to_use)))
#define INTRINSICCV_ATTR_NOINLINE __attribute__((noinline))
#define INTRINSICCV_ATTR_ALIGNED(alignment) __attribute__((aligned(alignment)))

#define INTRINSICCV_LIKELY(cond) __builtin_expect((cond), 1)
#define INTRINSICCV_UNLIKELY(cond) __builtin_expect((cond), 0)

#define INTRINSICCV_IFUNC_RESOLVER \
  INTRINSICCV_ATTR_SECTION(".text.ifunc_resolvers")

#define INTRINSICCV_FORCE_LOOP_UNROLL _Pragma("clang loop unroll(full)")

#endif  // INTRINSICCV_CONFIG_H
