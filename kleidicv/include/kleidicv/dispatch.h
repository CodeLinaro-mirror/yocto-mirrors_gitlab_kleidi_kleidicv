// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_DISPATCH_H
#define KLEIDICV_DISPATCH_H

#include "kleidicv/config.h"

#if defined(KLEIDICV_HAVE_SVE2) || defined(KLEIDICV_HAVE_SME)
#include <sys/auxv.h>

#include <cstdint>
#include <type_traits>

namespace KLEIDICV_TARGET_NAMESPACE {

using HwCapTy = uint64_t;

struct HwCaps final {
  HwCapTy hwcap1;
  HwCapTy hwcap2;
};

static inline HwCaps get_hwcaps() {
  return HwCaps{getauxval(AT_HWCAP), getauxval(AT_HWCAP2)};
}

#ifdef KLEIDICV_HAVE_SVE2
static inline bool hwcaps_has_sve2(HwCaps hwcaps) {
  return hwcaps.hwcap2 & (1 << 1);
}

#define KLEIDICV_SVE2_RESOLVE(sve2_impl)                    \
  if (!std::is_null_pointer_v<decltype(sve2_impl)> &&       \
      KLEIDICV_TARGET_NAMESPACE::hwcaps_has_sve2(hwcaps)) { \
    return sve2_impl;                                       \
  }
#else
#define KLEIDICV_SVE2_RESOLVE(x)
#endif  // KLEIDICV_HAVE_SVE2

#ifdef KLEIDICV_HAVE_SME
static inline bool hwcaps_has_sme(HwCaps hwcaps) {
  const int kSMEBit = 23;
  return hwcaps.hwcap2 & (1UL << kSMEBit);
}

#define KLEIDICV_SME_RESOLVE(sme_impl)                     \
  if (!std::is_null_pointer_v<decltype(sme_impl)> &&       \
      KLEIDICV_TARGET_NAMESPACE::hwcaps_has_sme(hwcaps)) { \
    return sme_impl;                                       \
  }
#else
#define KLEIDICV_SME_RESOLVE(x)
#endif  // KLEIDICV_HAVE_SME

}  // namespace KLEIDICV_TARGET_NAMESPACE

#define KLEIDICV_MULTIVERSION_C_API(api_name, neon_impl, sve2_impl, sme_impl) \
  static decltype(neon_impl) api_name##_resolver() {                          \
    [[maybe_unused]] KLEIDICV_TARGET_NAMESPACE::HwCaps hwcaps =               \
        KLEIDICV_TARGET_NAMESPACE::get_hwcaps();                              \
    KLEIDICV_SME_RESOLVE(sme_impl);                                           \
    KLEIDICV_SVE2_RESOLVE(sve2_impl);                                         \
    return neon_impl;                                                         \
  }                                                                           \
  extern "C" {                                                                \
  decltype(neon_impl) api_name = api_name##_resolver();                       \
  }

#else  // KLEIDICV_HAVE_SVE2 || KLEIDICV_HAVE_SME

#define KLEIDICV_MULTIVERSION_C_API(api_name, neon_impl, sve2_impl, sme_impl) \
                                                                              \
  extern "C" {                                                                \
  decltype(neon_impl) api_name = neon_impl;                                   \
  }

#endif  // KLEIDICV_HAVE_SVE2 || KLEIDICV_HAVE_SME

#if KLEIDICV_ALWAYS_ENABLE_SME
#define KLEIDICV_SME_IMPL_IF(func) func
#else
#define KLEIDICV_SME_IMPL_IF(func) nullptr
#endif  // KLEIDICV_ALWAYS_ENABLE_SME

#if KLEIDICV_ALWAYS_ENABLE_SVE2
#define KLEIDICV_SVE2_IMPL_IF(func) func
#else
#define KLEIDICV_SVE2_IMPL_IF(func) nullptr
#endif  // KLEIDICV_ALWAYS_ENABLE_SVE2

#endif  // KLEIDICV_DISPATCH_H
