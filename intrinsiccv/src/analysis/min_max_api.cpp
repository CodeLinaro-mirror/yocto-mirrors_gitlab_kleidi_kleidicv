// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "dispatch.h"
#include "intrinsiccv.h"
#include "types.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
void min_max(const T *src, size_t src_stride, size_t width, size_t height,
             T *min_value, T *max_value);

template <typename T>
void min_max_loc(const T *src, size_t src_stride, size_t width, size_t height,
                 size_t *min_offset, size_t *max_offset);

}  // namespace neon

namespace sve2 {}  // namespace sve2

namespace sme2 {}  // namespace sme2

#define INTRINSICCV_DEFINE_MINMAX_API(name, type)                              \
  static IFuncImpls name##_impls_builder(void) {                               \
    IFuncImpls impls;                                                          \
    INTRINSICCV_ADD_NEON_IMPL(intrinsiccv::neon::min_max<type>);               \
    return impls;                                                              \
  }                                                                            \
  INTRINSICCV_MULTIVERSION_C_API(name, name##_impls_builder, void,             \
                                 const type *, size_t, size_t, size_t, type *, \
                                 type *)

INTRINSICCV_DEFINE_MINMAX_API(intrinsiccv_min_max_u8, uint8_t);
INTRINSICCV_DEFINE_MINMAX_API(intrinsiccv_min_max_s8, int8_t);
INTRINSICCV_DEFINE_MINMAX_API(intrinsiccv_min_max_u16, uint16_t);
INTRINSICCV_DEFINE_MINMAX_API(intrinsiccv_min_max_s16, int16_t);
INTRINSICCV_DEFINE_MINMAX_API(intrinsiccv_min_max_s32, int32_t);

#define INTRINSICCV_DEFINE_MINMAXLOC_API(name, type)                   \
  static IFuncImpls name##_impls_builder(void) {                       \
    IFuncImpls impls;                                                  \
    INTRINSICCV_ADD_NEON_IMPL(intrinsiccv::neon::min_max_loc<type>);   \
    return impls;                                                      \
  }                                                                    \
  INTRINSICCV_MULTIVERSION_C_API(name, name##_impls_builder, void,     \
                                 const type *, size_t, size_t, size_t, \
                                 size_t *, size_t *)

INTRINSICCV_DEFINE_MINMAXLOC_API(intrinsiccv_min_max_loc_u8, uint8_t);

}  // namespace intrinsiccv
