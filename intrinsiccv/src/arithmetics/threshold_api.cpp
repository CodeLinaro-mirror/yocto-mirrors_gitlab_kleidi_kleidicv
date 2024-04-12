// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"

namespace intrinsiccv {

namespace neon {
template <typename T>
intrinsiccv_error_t threshold_binary(const T *src, size_t src_stride, T *dst,
                                     size_t dst_stride, size_t width,
                                     size_t height, T threshold, T value);
}  // namespace neon

namespace sve2 {
template <typename T>
intrinsiccv_error_t threshold_binary(const T *src, size_t src_stride, T *dst,
                                     size_t dst_stride, size_t width,
                                     size_t height, T threshold, T value);
}  // namespace sve2

namespace sme2 {
template <typename T>
intrinsiccv_error_t threshold_binary(const T *src, size_t src_stride, T *dst,
                                     size_t dst_stride, size_t width,
                                     size_t height, T threshold, T value);
}  // namespace sme2

}  // namespace intrinsiccv

#define KLEIDICV_DEFINE_C_API(name, type)                                \
  KLEIDICV_MULTIVERSION_C_API(                                           \
      name, &intrinsiccv::neon::threshold_binary<type>,                  \
      KLEIDICV_SVE2_IMPL_IF(&intrinsiccv::sve2::threshold_binary<type>), \
      &intrinsiccv::sme2::threshold_binary<type>)

KLEIDICV_DEFINE_C_API(intrinsiccv_threshold_binary_u8, uint8_t);
