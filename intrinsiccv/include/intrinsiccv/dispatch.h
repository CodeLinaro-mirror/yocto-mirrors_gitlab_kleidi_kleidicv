// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_DISPATCH_H
#define INTRINSICCV_DISPATCH_H

#include <sys/auxv.h>

#include <cinttypes>
#include <type_traits>

#include "intrinsiccv/config.h"

namespace intrinsiccv {

using HwCapTy = uint64_t;

struct HwCaps final {
  HwCapTy hwcap1;
  HwCapTy hwcap2;
};

static inline HwCaps get_hwcaps() {
  return HwCaps{getauxval(AT_HWCAP), getauxval(AT_HWCAP2)};
}

static inline bool hwcaps_has_sve2(HwCaps hwcaps) {
  return hwcaps.hwcap2 & (1 << 1);
}

static inline bool hwcaps_has_sme2(HwCaps hwcaps) {
  // Actually checks for SME, not SME2, but this will be changed to check for
  // SME2 in future.
  const int kSMEBit = 23;
  return hwcaps.hwcap2 & (1UL << kSMEBit);
}

#if INTRINSICCV_ALWAYS_ENABLE_SVE2
#define INTRINSICCV_SVE2_IMPL_IF(func) func
#else
#define INTRINSICCV_SVE2_IMPL_IF(func) nullptr
#endif

#ifdef INTRINSICCV_HAVE_SVE2
#define INTRINSICCV_SVE2_RESOLVE(sve2_impl)           \
  if (!std::is_null_pointer_v<decltype(sve2_impl)> && \
      hwcaps_has_sve2(hwcaps)) {                      \
    return sve2_impl;                                 \
  }

#else
#define INTRINSICCV_SVE2_RESOLVE(sve2_impl)
#endif

#ifdef INTRINSICCV_HAVE_SME2
#define INTRINSICCV_SME2_RESOLVE(sme2_impl)           \
  if (!std::is_null_pointer_v<decltype(sme2_impl)> && \
      hwcaps_has_sme2(hwcaps)) {                      \
    return sme2_impl;                                 \
  }
#else
#define INTRINSICCV_SME2_RESOLVE(sme2_impl)
#endif

// Creates a multiversioned C API with a resolver for it
#define INTRINSICCV_MULTIVERSION_C_API(api_name, neon_impl, sve2_impl, \
                                       sme2_impl, ...)                 \
  decltype(neon_impl) *api_name##_resolver() {                         \
    [[maybe_unused]] HwCaps hwcaps = get_hwcaps();                     \
    INTRINSICCV_SME2_RESOLVE(sme2_impl);                               \
    INTRINSICCV_SVE2_RESOLVE(sve2_impl);                               \
    return neon_impl;                                                  \
  }                                                                    \
                                                                       \
  extern "C" {                                                         \
  decltype(neon_impl) *api_name = api_name##_resolver();               \
  }

}  // namespace intrinsiccv

#endif  // INTRINSICCV_DISPATCH_H
