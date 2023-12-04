// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_DISPATCH_H
#define INTRINSICCV_DISPATCH_H

#include <cinttypes>

#include "config.h"
#include "sys/ifunc.h"

namespace intrinsiccv {

using IFuncImplType = void *;
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
  return hwcaps.hwcap2 & (1UL << 37);
}

struct IFuncImpls final {
  IFuncImplType neon_impl{nullptr};
#ifdef INTRINSICCV_HAVE_SVE2
  IFuncImplType sve2_impl{nullptr};
#endif
#ifdef INTRINSICCV_HAVE_SME2
  IFuncImplType sme2_impl{nullptr};
#endif
};

#define INTRINSICCV_ADD_NEON_IMPL(func)                         \
  do {                                                          \
    impls.neon_impl = reinterpret_cast<IFuncImplType>(&(func)); \
  } while (false)

#if INTRINSICCV_ALWAYS_ENABLE_SVE2
#define INTRINSICCV_ADD_SVE2_IMPL_IF(func) INTRINSICCV_ADD_SVE2_IMPL(func)
#else
#define INTRINSICCV_ADD_SVE2_IMPL_IF(func)
#endif

#ifdef INTRINSICCV_HAVE_SVE2
#define INTRINSICCV_ADD_SVE2_IMPL(func)                         \
  do {                                                          \
    impls.sve2_impl = reinterpret_cast<IFuncImplType>(&(func)); \
  } while (false)
#else
#define INTRINSICCV_ADD_SVE2_IMPL(func) ((void)0)
#endif

#ifdef INTRINSICCV_HAVE_SME2
#define INTRINSICCV_ADD_SME2_IMPL(func)                         \
  do {                                                          \
    impls.sme2_impl = reinterpret_cast<IFuncImplType>(&(func)); \
  } while (false)
#else
#define INTRINSICCV_ADD_SME2_IMPL(func) ((void)0)
#endif

static inline IFuncImplType default_ifunc_resolver(
    [[maybe_unused]] const HwCaps hwcaps, const IFuncImpls impls) {
#ifdef INTRINSICCV_HAVE_SME2
  if (impls.sme2_impl && hwcaps_has_sme2(hwcaps)) {
    return impls.sme2_impl;
  }
#endif

#ifdef INTRINSICCV_HAVE_SVE2
  if (impls.sve2_impl && hwcaps_has_sve2(hwcaps)) {
    return impls.sve2_impl;
  }
#endif

  return impls.neon_impl;
}

// Creates a multiversioned C API with an ifunc resolver for it
#define INTRINSICCV_MULTIVERSION_C_API(api_name, impls_builder, retty, ...) \
  extern "C" INTRINSICCV_IFUNC_RESOLVER IFuncImplType                       \
      intrinsiccv_##api_name##_ifunc_resolver(HwCapTy, __ifunc_arg_t *arg); \
                                                                            \
  extern "C" IFuncImplType intrinsiccv_##api_name##_ifunc_resolver(         \
      HwCapTy hwcap, __ifunc_arg_t *arg) {                                  \
    IFuncImpls impls = impls_builder();                                     \
    return default_ifunc_resolver(make_hwcaps(hwcap, arg), impls);          \
  };                                                                        \
                                                                            \
  extern "C" retty intrinsiccv_##api_name(__VA_ARGS__)                      \
      INTRINSICCV_ATTR_IFUNC("intrinsiccv_" #api_name "_ifunc_resolver")

}  // namespace intrinsiccv

#endif  // INTRINSICCV_DISPATCH_H
