// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "dispatch.h"
#include "intrinsiccv.h"
#include "types.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
void scale(const T *src, size_t src_stride, T *dst, size_t dst_stride,
           size_t width, size_t height, float scale, float shift);
}  // namespace neon

namespace sve2 {}  // namespace sve2

namespace sme2 {}  // namespace sme2

#define INTRINSICCV_DEFINE_SCALE_API(name, type)                               \
  INTRINSICCV_MULTIVERSION_C_API(name, intrinsiccv::neon::scale<type>,         \
                                 nullptr, nullptr, void, const type *, size_t, \
                                 type *, size_t, size_t, size_t, float, float)

INTRINSICCV_DEFINE_SCALE_API(intrinsiccv_scale_u8, uint8_t);
// INTRINSICCV_DEFINE_SCALE_API(intrinsiccv_scale_s8, int8_t);
// INTRINSICCV_DEFINE_SCALE_API(intrinsiccv_scale_u16, uint16_t);
// INTRINSICCV_DEFINE_SCALE_API(intrinsiccv_scale_s16, int16_t);
// INTRINSICCV_DEFINE_SCALE_API(intrinsiccv_scale_s32, int32_t);

}  // namespace intrinsiccv
