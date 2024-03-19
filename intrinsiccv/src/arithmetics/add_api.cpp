// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/types.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
intrinsiccv_error_t saturating_add(const T *src_a, size_t src_a_stride,
                                   const T *src_b, size_t src_b_stride, T *dst,
                                   size_t dst_stride, size_t width,
                                   size_t height);

}  // namespace neon

namespace sve2 {

template <typename T>
intrinsiccv_error_t saturating_add(const T *src_a, size_t src_a_stride,
                                   const T *src_b, size_t src_b_stride, T *dst,
                                   size_t dst_stride, size_t width,
                                   size_t height);

}  // namespace sve2

namespace sme2 {
template <typename T>
intrinsiccv_error_t saturating_add(const T *src_a, size_t src_a_stride,
                                   const T *src_b, size_t src_b_stride, T *dst,
                                   size_t dst_stride, size_t width,
                                   size_t height);

}  // namespace sme2

}  // namespace intrinsiccv

#define INTRINSICCV_DEFINE_C_API(name, type)                             \
  INTRINSICCV_MULTIVERSION_C_API(                                        \
      name, intrinsiccv::neon::saturating_add<type>,                     \
      INTRINSICCV_SVE2_IMPL_IF(intrinsiccv::sve2::saturating_add<type>), \
      intrinsiccv::sme2::saturating_add<type>)

INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_add_s8, int8_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_add_u8, uint8_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_add_s16, int16_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_add_u16, uint16_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_add_s32, int32_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_add_u32, uint32_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_add_s64, int64_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_add_u64, uint64_t);
