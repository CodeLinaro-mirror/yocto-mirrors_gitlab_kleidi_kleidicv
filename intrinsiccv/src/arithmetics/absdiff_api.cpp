// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "dispatch.h"
#include "intrinsiccv.h"
#include "types.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
void saturating_absdiff(const T *src_a, size_t src_a_stride, const T *src_b,
                        size_t src_b_stride, T *dst, size_t dst_stride,
                        size_t width, size_t height);

}  // namespace neon

namespace sve2 {

template <typename T>
void saturating_absdiff(const T *src_a, size_t src_a_stride, const T *src_b,
                        size_t src_b_stride, T *dst, size_t dst_stride,
                        size_t width, size_t height);

}  // namespace sve2

namespace sme2 {

template <typename T>
void saturating_absdiff(const T *src_a, size_t src_a_stride, const T *src_b,
                        size_t src_b_stride, T *dst, size_t dst_stride,
                        size_t width, size_t height);

}  // namespace sme2

#define INTRINSICCV_DEFINE_C_API(name, type)                                   \
  INTRINSICCV_MULTIVERSION_C_API(                                              \
      name, intrinsiccv::neon::saturating_absdiff<type>,                       \
      INTRINSICCV_SVE2_IMPL_IF(intrinsiccv::sve2::saturating_absdiff<type>),   \
      intrinsiccv::sme2::saturating_absdiff<type>, void, const type *, size_t, \
      const type *, size_t, type *, size_t, size_t, size_t)

INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_absdiff_u8, uint8_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_absdiff_s8, int8_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_absdiff_u16, uint16_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_absdiff_s16, int16_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_saturating_absdiff_s32, int32_t);

}  // namespace intrinsiccv
