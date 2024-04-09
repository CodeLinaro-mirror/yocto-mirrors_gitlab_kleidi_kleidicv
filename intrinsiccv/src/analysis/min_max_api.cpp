// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/types.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
intrinsiccv_error_t min_max(const T *src, size_t src_stride, size_t width,
                            size_t height, T *min_value, T *max_value);

template <typename T>
intrinsiccv_error_t min_max_loc(const T *src, size_t src_stride, size_t width,
                                size_t height, size_t *min_offset,
                                size_t *max_offset);

}  // namespace neon

namespace sve2 {}  // namespace sve2

namespace sme2 {}  // namespace sme2

}  // namespace intrinsiccv

#define INTRINSICCV_DEFINE_MINMAX_API(name, type)                         \
  INTRINSICCV_MULTIVERSION_C_API(name, &intrinsiccv::neon::min_max<type>, \
                                 nullptr, nullptr)

INTRINSICCV_DEFINE_MINMAX_API(intrinsiccv_min_max_u8, uint8_t);
INTRINSICCV_DEFINE_MINMAX_API(intrinsiccv_min_max_s8, int8_t);
INTRINSICCV_DEFINE_MINMAX_API(intrinsiccv_min_max_u16, uint16_t);
INTRINSICCV_DEFINE_MINMAX_API(intrinsiccv_min_max_s16, int16_t);
INTRINSICCV_DEFINE_MINMAX_API(intrinsiccv_min_max_s32, int32_t);

#define INTRINSICCV_DEFINE_MINMAXLOC_API(name, type)                          \
  INTRINSICCV_MULTIVERSION_C_API(name, &intrinsiccv::neon::min_max_loc<type>, \
                                 nullptr, nullptr)

INTRINSICCV_DEFINE_MINMAXLOC_API(intrinsiccv_min_max_loc_u8, uint8_t);
