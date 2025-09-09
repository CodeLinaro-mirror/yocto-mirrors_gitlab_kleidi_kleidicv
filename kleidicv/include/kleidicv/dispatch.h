// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_DISPATCH_H
#define KLEIDICV_DISPATCH_H

#include "kleidicv/config.h"

#if KLEIDICV_ENABLE_SME2 || KLEIDICV_ENABLE_SME || KLEIDICV_ENABLE_SVE2
#include <type_traits>

#include "device_capabilities.h"

#if KLEIDICV_ENABLE_SVE2
#define KLEIDICV_SVE2_RESOLVE(sve2_impl)              \
  if (!std::is_null_pointer_v<decltype(sve2_impl)> && \
      kleidicv::DeviceCapabilities::has_sve2) {       \
    return sve2_impl;                                 \
  }
#else
#define KLEIDICV_SVE2_RESOLVE(x)
#endif  // KLEIDICV_ENABLE_SVE2

#if KLEIDICV_ENABLE_SME
#define KLEIDICV_SME_RESOLVE(sme_impl)               \
  if (!std::is_null_pointer_v<decltype(sme_impl)> && \
      kleidicv::DeviceCapabilities::has_sme) {       \
    return sme_impl;                                 \
  }
#else
#define KLEIDICV_SME_RESOLVE(x)
#endif  // KLEIDICV_ENABLE_SME

#if KLEIDICV_ENABLE_SME2

#define KLEIDICV_SME2_RESOLVE(sme2_impl)              \
  if (!std::is_null_pointer_v<decltype(sme2_impl)> && \
      kleidicv::DeviceCapabilities::has_sme2) {       \
    return sme2_impl;                                 \
  }
#else
#define KLEIDICV_SME2_RESOLVE(x)
#endif  // KLEIDICV_ENABLE_SME2

#define KLEIDICV_MULTIVERSION_C_API(api_name, neon_impl, sve2_impl, sme_impl, \
                                    sme2_impl)                                \
  static decltype(neon_impl) api_name##_resolver() {                          \
    KLEIDICV_SME2_RESOLVE(sme2_impl);                                         \
    KLEIDICV_SME_RESOLVE(sme_impl);                                           \
    KLEIDICV_SVE2_RESOLVE(sve2_impl);                                         \
    return neon_impl;                                                         \
  }                                                                           \
  extern "C" {                                                                \
  decltype(neon_impl) api_name = api_name##_resolver();                       \
  }

#else  // KLEIDICV_HAVE_SVE2 || KLEIDICV_HAVE_SME || KLEIDICV_HAVE_SME2

#define KLEIDICV_MULTIVERSION_C_API(api_name, neon_impl, sve2_impl, sme_impl, \
                                    sme2_impl)                                \
                                                                              \
  extern "C" {                                                                \
  decltype(neon_impl) api_name = neon_impl;                                   \
  }

#endif  // KLEIDICV_ENABLE_SME2 || KLEIDICV_ENABLE_SME ||  KLEIDICV_ENABLE_SVE2

#if KLEIDICV_ALWAYS_ENABLE_SME2
#define KLEIDICV_SME2_IMPL_IF(func) func
#else
#define KLEIDICV_SME2_IMPL_IF(func) nullptr
#endif  // KLEIDICV_ALWAYS_ENABLE_SME2

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
