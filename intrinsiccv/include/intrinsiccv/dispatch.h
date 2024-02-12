// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_DISPATCH_H
#define INTRINSICCV_DISPATCH_H

#include <cinttypes>

#include "intrinsiccv/config.h"
#include "sys/ifunc.h"

namespace intrinsiccv {

using HwCapTy = uint64_t;

struct HwCaps final {
  HwCapTy hwcap1;
  HwCapTy hwcap2;
};

static inline HwCaps make_hwcaps(HwCapTy hwcap, __ifunc_arg_t *arg) {
  HwCapTy hwcap2 = hwcap & _IFUNC_ARG_HWCAP ? arg->_hwcap2 : 0;
  return HwCaps{hwcap, hwcap2};
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
#define INTRINSICCV_SVE2_RESOLVE_IFUNC(sve2_impl) \
  if (sve2_impl && hwcaps_has_sve2(hwcaps)) {     \
    return sve2_impl;                             \
  }

#else
#define INTRINSICCV_SVE2_RESOLVE_IFUNC(sve2_impl)
#endif

#ifdef INTRINSICCV_HAVE_SME2
#define INTRINSICCV_SME2_RESOLVE_IFUNC(sme2_impl) \
  if (sme2_impl && hwcaps_has_sme2(hwcaps)) {     \
    return sme2_impl;                             \
  }
#else
#define INTRINSICCV_SME2_RESOLVE_IFUNC(sme2_impl)
#endif

// Creates a multiversioned C API with an ifunc resolver for it
#define INTRINSICCV_MULTIVERSION_C_API(api_name, neon_impl, sve2_impl,  \
                                       sme2_impl, ...)                  \
  typedef intrinsiccv_error_t (*api_name##_function_type)(__VA_ARGS__); \
                                                                        \
  extern "C" INTRINSICCV_IFUNC_RESOLVER api_name##_function_type        \
      api_name##_ifunc_resolver(HwCapTy, __ifunc_arg_t *arg);           \
                                                                        \
  extern "C" api_name##_function_type api_name##_ifunc_resolver(        \
      HwCapTy hwcap, __ifunc_arg_t *arg) {                              \
    [[maybe_unused]] HwCaps hwcaps = make_hwcaps(hwcap, arg);           \
    INTRINSICCV_SME2_RESOLVE_IFUNC(sme2_impl);                          \
    INTRINSICCV_SVE2_RESOLVE_IFUNC(sve2_impl);                          \
    return neon_impl;                                                   \
  }                                                                     \
                                                                        \
  extern "C" intrinsiccv_error_t api_name(__VA_ARGS__)                  \
      INTRINSICCV_ATTR_IFUNC(#api_name "_ifunc_resolver")

}  // namespace intrinsiccv

#endif  // INTRINSICCV_DISPATCH_H
