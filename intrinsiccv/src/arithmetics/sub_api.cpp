// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "dispatch.h"
#include "intrinsiccv.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
void saturating_sub(const T *src_a, size_t src_a_stride, const T *src_b,
                    size_t src_b_stride, T *dst, size_t dst_stride,
                    size_t width, size_t height);

}  // namespace neon

namespace sve2 {

template <typename T>
void saturating_sub(const T *src_a, size_t src_a_stride, const T *src_b,
                    size_t src_b_stride, T *dst, size_t dst_stride,
                    size_t width, size_t height);

}  // namespace sve2

namespace sme2 {

template <typename T>
void saturating_sub(const T *src_a, size_t src_a_stride, const T *src_b,
                    size_t src_b_stride, T *dst, size_t dst_stride,
                    size_t width, size_t height);

}  // namespace sme2

#define INTRINSICCV_DEFINE_C_API(name, type)                               \
  INTRINSICCV_MULTIVERSION_C_API(                                          \
      name, intrinsiccv::neon::saturating_sub<type>,                       \
      INTRINSICCV_SVE2_IMPL_IF(intrinsiccv::sve2::saturating_sub<type>),   \
      intrinsiccv::sme2::saturating_sub<type>, void, const type *, size_t, \
      const type *, size_t, type *, size_t, size_t, size_t)

INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_sub_s8, int8_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_sub_u8, uint8_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_sub_s16, int16_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_sub_u16, uint16_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_sub_s32, int32_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_sub_u32, uint32_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_sub_s64, int64_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_sub_u64, uint64_t);

}  // namespace intrinsiccv
