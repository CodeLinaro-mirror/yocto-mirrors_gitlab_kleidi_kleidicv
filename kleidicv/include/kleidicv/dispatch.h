// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_DISPATCH_H
#define KLEIDICV_DISPATCH_H

#include "kleidicv/config.h"

#if KLEIDICV_ENABLE_SME2 || KLEIDICV_ENABLE_SME || KLEIDICV_ENABLE_SVE2

#include <cstdint>
#include <type_traits>

namespace kleidicv {
uint64_t svcntb_sve();
#if KLEIDICV_ENABLE_SME2 || KLEIDICV_ENABLE_SME
uint64_t svcntb_sme() KLEIDICV_STREAMING;
#endif
}  // namespace kleidicv

#ifdef __APPLE__

#include <sys/sysctl.h>

namespace KLEIDICV_TARGET_NAMESPACE {

static bool query_sysctl(const char* attribute_name) {
  uint64_t attribute_value = 0;
  size_t max_attribute_size = sizeof(attribute_value);
  if (sysctlbyname(attribute_name, &attribute_value, &max_attribute_size, NULL,
                   0)) {
    return false;
  }

  return attribute_value;
}

#if KLEIDICV_ENABLE_SVE2
#define KLEIDICV_SVE2_RESOLVE(sve2_impl)                                      \
  if (!std::is_null_pointer_v<decltype(sve2_impl)> &&                         \
      KLEIDICV_TARGET_NAMESPACE::query_sysctl("hw.optional.arm.FEAT_SVE2")) { \
    return sve2_impl;                                                         \
  }
#define KLEIDICV_SVE2_RESOLVE_VECLEN(sve2_impl, minlen, maxlen)               \
  if (!std::is_null_pointer_v<decltype(sve2_impl)> &&                         \
      KLEIDICV_TARGET_NAMESPACE::query_sysctl("hw.optional.arm.FEAT_SVE2") && \
      kleidicv::svcntb_sve() >= minlen && kleidicv::svcntb_sve() <= maxlen) { \
    return sve2_impl;                                                         \
  }
#else
#define KLEIDICV_SVE2_RESOLVE(x)
#define KLEIDICV_SVE2_RESOLVE_VECLEN(x, minlen, maxlen)
#endif  // KLEIDICV_ENABLE_SVE2

#if KLEIDICV_ENABLE_SME
#define KLEIDICV_SME_RESOLVE(sme_impl)                                       \
  if (!std::is_null_pointer_v<decltype(sme_impl)> &&                         \
      KLEIDICV_TARGET_NAMESPACE::query_sysctl("hw.optional.arm.FEAT_SME")) { \
    return sme_impl;                                                         \
  }
#define KLEIDICV_SME_RESOLVE_VECLEN(sme_impl, minlen, maxlen)                 \
  if (!std::is_null_pointer_v<decltype(sme_impl)> &&                          \
      KLEIDICV_TARGET_NAMESPACE::query_sysctl("hw.optional.arm.FEAT_SME") &&  \
      kleidicv::svcntb_sme() >= minlen && kleidicv::svcntb_sme() <= maxlen) { \
    return sme_impl;                                                          \
  }
#else
#define KLEIDICV_SME_RESOLVE(x)
#define KLEIDICV_SME_RESOLVE_VECLEN(x, minlen, maxlen)
#endif  // KLEIDICV_ENABLE_SME

#if KLEIDICV_ENABLE_SME2
#define KLEIDICV_SME2_RESOLVE(sme2_impl)                                      \
  if (!std::is_null_pointer_v<decltype(sme2_impl)> &&                         \
      KLEIDICV_TARGET_NAMESPACE::query_sysctl("hw.optional.arm.FEAT_SME2")) { \
    return sme2_impl;                                                         \
  }
#define KLEIDICV_SME2_RESOLVE_VECLEN(sme2_impl, minlen, maxlen)               \
  if (!std::is_null_pointer_v<decltype(sme2_impl)> &&                         \
      KLEIDICV_TARGET_NAMESPACE::query_sysctl("hw.optional.arm.FEAT_SME2") && \
      kleidicv::svcntb_sme() >= minlen && kleidicv::svcntb_sme() <= maxlen) { \
    return sme2_impl;                                                         \
  }
#else
#define KLEIDICV_SME2_RESOLVE(x)
#define KLEIDICV_SME2_RESOLVE_VECLEN(x, minlen, maxlen)
#endif  // KLEIDICV_ENABLE_SME2

}  // namespace KLEIDICV_TARGET_NAMESPACE

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

#define KLEIDICV_MULTIVERSION_C_API_VECLEN(                              \
    api_name, neon_impl, sve2_impl, sme_impl, sme2_impl, minlen, maxlen) \
  static decltype(neon_impl) api_name##_resolver() {                     \
    [[maybe_unused]] KLEIDICV_TARGET_NAMESPACE::HwCaps hwcaps =          \
        KLEIDICV_TARGET_NAMESPACE::get_hwcaps();                         \
    KLEIDICV_SME2_RESOLVE_VECLEN(sme2_impl, minlen, maxlen);             \
    KLEIDICV_SME_RESOLVE_VECLEN(sme_impl, minlen, maxlen);               \
    KLEIDICV_SVE2_RESOLVE_VECLEN(sve2_impl, minlen, maxlen);             \
    return neon_impl;                                                    \
  }                                                                      \
  extern "C" {                                                           \
  decltype(neon_impl) api_name = api_name##_resolver();                  \
  }

#else  // __APPLE__

#include <sys/auxv.h>
namespace KLEIDICV_TARGET_NAMESPACE {

using HwCapTy = uint64_t;

struct HwCaps final {
  HwCapTy hwcap1;
  HwCapTy hwcap2;
};

static inline HwCaps get_hwcaps() {
  return HwCaps{getauxval(AT_HWCAP), getauxval(AT_HWCAP2)};
}

#if KLEIDICV_ENABLE_SVE2
static inline bool hwcaps_has_sve2(HwCaps hwcaps) {
  return hwcaps.hwcap2 & (1 << 1);
}

#define KLEIDICV_SVE2_RESOLVE(sve2_impl)                    \
  if (!std::is_null_pointer_v<decltype(sve2_impl)> &&       \
      KLEIDICV_TARGET_NAMESPACE::hwcaps_has_sve2(hwcaps)) { \
    return sve2_impl;                                       \
  }
#define KLEIDICV_SVE2_RESOLVE_VECLEN(sve2_impl, minlen, maxlen)               \
  if (!std::is_null_pointer_v<decltype(sve2_impl)> &&                         \
      KLEIDICV_TARGET_NAMESPACE::hwcaps_has_sve2(hwcaps) &&                   \
      kleidicv::svcntb_sve() >= minlen && kleidicv::svcntb_sve() <= maxlen) { \
    return sve2_impl;                                                         \
  }
#else
#define KLEIDICV_SVE2_RESOLVE(x)
#define KLEIDICV_SVE2_RESOLVE_VECLEN(x, minlen, maxlen)
#endif  // KLEIDICV_ENABLE_SVE2

#if KLEIDICV_ENABLE_SME
static inline bool hwcaps_has_sme(HwCaps hwcaps) {
  const int kSMEBit = 23;
  return hwcaps.hwcap2 & (1UL << kSMEBit);
}

#define KLEIDICV_SME_RESOLVE(sme_impl)                     \
  if (!std::is_null_pointer_v<decltype(sme_impl)> &&       \
      KLEIDICV_TARGET_NAMESPACE::hwcaps_has_sme(hwcaps)) { \
    return sme_impl;                                       \
  }
#define KLEIDICV_SME_RESOLVE_VECLEN(sme_impl, minlen, maxlen)                 \
  if (!std::is_null_pointer_v<decltype(sme_impl)> &&                          \
      KLEIDICV_TARGET_NAMESPACE::hwcaps_has_sme(hwcaps) &&                    \
      kleidicv::svcntb_sme() >= minlen && kleidicv::svcntb_sme() <= maxlen) { \
    return sme_impl;                                                          \
  }
#else
#define KLEIDICV_SME_RESOLVE(x)
#define KLEIDICV_SME_RESOLVE_VECLEN(x, minlen, maxlen)
#endif  // KLEIDICV_ENABLE_SME

#if KLEIDICV_ENABLE_SME2
static inline bool hwcaps_has_sme2(HwCaps hwcaps) {
  const int kSME2Bit = 37;
  return hwcaps.hwcap2 & (1UL << kSME2Bit);
}

#define KLEIDICV_SME2_RESOLVE(sme2_impl)                    \
  if (!std::is_null_pointer_v<decltype(sme2_impl)> &&       \
      KLEIDICV_TARGET_NAMESPACE::hwcaps_has_sme2(hwcaps)) { \
    return sme2_impl;                                       \
  }
#define KLEIDICV_SME2_RESOLVE_VECLEN(sme2_impl, minlen, maxlen)               \
  if (!std::is_null_pointer_v<decltype(sme2_impl)> &&                         \
      KLEIDICV_TARGET_NAMESPACE::hwcaps_has_sme2(hwcaps) &&                   \
      kleidicv::svcntb_sme() >= minlen && kleidicv::svcntb_sme() <= maxlen) { \
    return sme2_impl;                                                         \
  }
#else
#define KLEIDICV_SME2_RESOLVE(x)
#define KLEIDICV_SME2_RESOLVE_VECLEN(x, minlen, maxlen)
#endif  // KLEIDICV_ENABLE_SME2

}  // namespace KLEIDICV_TARGET_NAMESPACE

#define KLEIDICV_MULTIVERSION_C_API(api_name, neon_impl, sve2_impl, sme_impl, \
                                    sme2_impl)                                \
  static decltype(neon_impl) api_name##_resolver() {                          \
    [[maybe_unused]] KLEIDICV_TARGET_NAMESPACE::HwCaps hwcaps =               \
        KLEIDICV_TARGET_NAMESPACE::get_hwcaps();                              \
    KLEIDICV_SME2_RESOLVE(sme2_impl);                                         \
    KLEIDICV_SME_RESOLVE(sme_impl);                                           \
    KLEIDICV_SVE2_RESOLVE(sve2_impl);                                         \
    return neon_impl;                                                         \
  }                                                                           \
  extern "C" {                                                                \
  decltype(neon_impl) api_name = api_name##_resolver();                       \
  }

#define KLEIDICV_MULTIVERSION_C_API_VECLEN(                              \
    api_name, neon_impl, sve2_impl, sme_impl, sme2_impl, minlen, maxlen) \
  static decltype(neon_impl) api_name##_resolver() {                     \
    [[maybe_unused]] KLEIDICV_TARGET_NAMESPACE::HwCaps hwcaps =          \
        KLEIDICV_TARGET_NAMESPACE::get_hwcaps();                         \
    KLEIDICV_SME2_RESOLVE_VECLEN(sme2_impl, minlen, maxlen);             \
    KLEIDICV_SME_RESOLVE_VECLEN(sme_impl, minlen, maxlen);               \
    KLEIDICV_SVE2_RESOLVE_VECLEN(sve2_impl, minlen, maxlen);             \
    return neon_impl;                                                    \
  }                                                                      \
  extern "C" {                                                           \
  decltype(neon_impl) api_name = api_name##_resolver();                  \
  }

#endif  // __APPLE__

#else  // KLEIDICV_HAVE_SVE2 || KLEIDICV_HAVE_SME || KLEIDICV_HAVE_SME2

#define KLEIDICV_MULTIVERSION_C_API(api_name, neon_impl, sve2_impl, sme_impl, \
                                    sme2_impl)                                \
                                                                              \
  extern "C" {                                                                \
  decltype(neon_impl) api_name = neon_impl;                                   \
  }

#define KLEIDICV_MULTIVERSION_C_API_VECLEN(                              \
    api_name, neon_impl, sve2_impl, sme_impl, sme2_impl, minlen, maxlen) \
  KLEIDICV_MULTIVERSION_C_API(api_name, neon_impl, sve2_impl, sme_impl,  \
                              sme2_impl)

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
