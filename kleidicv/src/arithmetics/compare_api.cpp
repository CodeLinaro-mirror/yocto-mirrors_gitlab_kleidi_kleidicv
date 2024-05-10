// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"

namespace kleidicv {

namespace neon {
template <typename T>
kleidicv_error_t compare(const T *src_a, size_t src_a_stride, const T *src_b,
                         size_t src_b_stride, T *dst, size_t dst_stride,
                         size_t width, size_t height,
                         kleidicv_cmp_type_t cmp_type);

}  // namespace neon

namespace sve2 {}  // namespace sve2

namespace sme2 {}  // namespace sme2

}  // namespace kleidicv

#define KLEIDICV_DEFINE_C_API(name, type)                                    \
  KLEIDICV_MULTIVERSION_C_API(name, &kleidicv::neon::compare<type>, nullptr, \
                              nullptr);

KLEIDICV_DEFINE_C_API(kleidicv_compare_u8, uint8_t);
