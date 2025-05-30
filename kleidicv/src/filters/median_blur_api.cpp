// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/filters/median_blur.h"
#include "kleidicv/kleidicv.h"
namespace kleidicv {

namespace neon {

template <typename T>
kleidicv_error_t median_blur_stripe(const T *src, size_t src_stride, T *dst,
                                    size_t dst_stride, size_t width,
                                    size_t height, size_t y_begin, size_t y_end,
                                    size_t channels, size_t kernel_width,
                                    size_t kernel_height,
                                    FixedBorderType border_type);

}  // namespace neon

namespace sve2 {

template <typename T>
kleidicv_error_t median_blur_stripe(const T *src, size_t src_stride, T *dst,
                                    size_t dst_stride, size_t width,
                                    size_t height, size_t y_begin, size_t y_end,
                                    size_t channels, size_t kernel_width,
                                    size_t kernel_height,
                                    FixedBorderType border_type);

}  // namespace sve2

namespace sme2 {

template <typename T>
kleidicv_error_t median_blur_stripe(const T *src, size_t src_stride, T *dst,
                                    size_t dst_stride, size_t width,
                                    size_t height, size_t y_begin, size_t y_end,
                                    size_t channels, size_t kernel_width,
                                    size_t kernel_height,
                                    FixedBorderType border_type);

}  // namespace sme2

}  // namespace kleidicv

#define KLEIDICV_DEFINE_C_API(name, type)                              \
  KLEIDICV_MULTIVERSION_C_API(                                         \
      name, &kleidicv::neon::median_blur_stripe<type>,                 \
      KLEIDICV_SVE2_IMPL_IF(kleidicv::sve2::median_blur_stripe<type>), \
      &kleidicv::sme2::median_blur_stripe<type>)

KLEIDICV_DEFINE_C_API(kleidicv_median_blur_stripe_s8, int8_t);
KLEIDICV_DEFINE_C_API(kleidicv_median_blur_stripe_u8, uint8_t);
KLEIDICV_DEFINE_C_API(kleidicv_median_blur_stripe_u16, uint16_t);
KLEIDICV_DEFINE_C_API(kleidicv_median_blur_stripe_s16, int16_t);
KLEIDICV_DEFINE_C_API(kleidicv_median_blur_stripe_u32, uint32_t);
KLEIDICV_DEFINE_C_API(kleidicv_median_blur_stripe_s32, int32_t);
KLEIDICV_DEFINE_C_API(kleidicv_median_blur_stripe_f32, float);

extern "C" {

kleidicv_error_t kleidicv_median_blur_s8(const int8_t *src, size_t src_stride,
                                         int8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         size_t channels, size_t kernel_width,
                                         size_t kernel_height,
                                         kleidicv_border_type_t border_type) {
  auto [checks_result, fixed_border_type] =
      kleidicv::median_blur_is_implemented(
          src, src_stride, dst, dst_stride, width, height, channels,
          kernel_width, kernel_height, border_type);

  if (checks_result != KLEIDICV_OK) {
    return checks_result;
  }

  return kleidicv_median_blur_stripe_s8(
      src, src_stride, dst, dst_stride, width, height, 0, height, channels,
      kernel_width, kernel_height, fixed_border_type);
}

kleidicv_error_t kleidicv_median_blur_u8(const uint8_t *src, size_t src_stride,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         size_t channels, size_t kernel_width,
                                         size_t kernel_height,
                                         kleidicv_border_type_t border_type) {
  auto [checks_result, fixed_border_type] =
      kleidicv::median_blur_is_implemented(
          src, src_stride, dst, dst_stride, width, height, channels,
          kernel_width, kernel_height, border_type);

  if (checks_result != KLEIDICV_OK) {
    return checks_result;
  }

  return kleidicv_median_blur_stripe_u8(
      src, src_stride, dst, dst_stride, width, height, 0, height, channels,
      kernel_width, kernel_height, fixed_border_type);
}

kleidicv_error_t kleidicv_median_blur_s16(const int16_t *src, size_t src_stride,
                                          int16_t *dst, size_t dst_stride,
                                          size_t width, size_t height,
                                          size_t channels, size_t kernel_width,
                                          size_t kernel_height,
                                          kleidicv_border_type_t border_type) {
  auto [checks_result, fixed_border_type] =
      kleidicv::median_blur_is_implemented(
          src, src_stride, dst, dst_stride, width, height, channels,
          kernel_width, kernel_height, border_type);

  if (checks_result != KLEIDICV_OK) {
    return checks_result;
  }

  return kleidicv_median_blur_stripe_s16(
      src, src_stride, dst, dst_stride, width, height, 0, height, channels,
      kernel_width, kernel_height, fixed_border_type);
}

kleidicv_error_t kleidicv_median_blur_u16(
    const uint16_t *src, size_t src_stride, uint16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, kleidicv_border_type_t border_type) {
  auto [checks_result, fixed_border_type] =
      kleidicv::median_blur_is_implemented(
          src, src_stride, dst, dst_stride, width, height, channels,
          kernel_width, kernel_height, border_type);

  if (checks_result != KLEIDICV_OK) {
    return checks_result;
  }

  return kleidicv_median_blur_stripe_u16(
      src, src_stride, dst, dst_stride, width, height, 0, height, channels,
      kernel_width, kernel_height, fixed_border_type);
}

kleidicv_error_t kleidicv_median_blur_s32(const int32_t *src, size_t src_stride,
                                          int32_t *dst, size_t dst_stride,
                                          size_t width, size_t height,
                                          size_t channels, size_t kernel_width,
                                          size_t kernel_height,
                                          kleidicv_border_type_t border_type) {
  auto [checks_result, fixed_border_type] =
      kleidicv::median_blur_is_implemented(
          src, src_stride, dst, dst_stride, width, height, channels,
          kernel_width, kernel_height, border_type);

  if (checks_result != KLEIDICV_OK) {
    return checks_result;
  }

  return kleidicv_median_blur_stripe_s32(
      src, src_stride, dst, dst_stride, width, height, 0, height, channels,
      kernel_width, kernel_height, fixed_border_type);
}

kleidicv_error_t kleidicv_median_blur_u32(
    const uint32_t *src, size_t src_stride, uint32_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, kleidicv_border_type_t border_type) {
  auto [checks_result, fixed_border_type] =
      kleidicv::median_blur_is_implemented(
          src, src_stride, dst, dst_stride, width, height, channels,
          kernel_width, kernel_height, border_type);

  if (checks_result != KLEIDICV_OK) {
    return checks_result;
  }

  return kleidicv_median_blur_stripe_u32(
      src, src_stride, dst, dst_stride, width, height, 0, height, channels,
      kernel_width, kernel_height, fixed_border_type);
}

kleidicv_error_t kleidicv_median_blur_f32(const float *src, size_t src_stride,
                                          float *dst, size_t dst_stride,
                                          size_t width, size_t height,
                                          size_t channels, size_t kernel_width,
                                          size_t kernel_height,
                                          kleidicv_border_type_t border_type) {
  auto [checks_result, fixed_border_type] =
      kleidicv::median_blur_is_implemented(
          src, src_stride, dst, dst_stride, width, height, channels,
          kernel_width, kernel_height, border_type);

  if (checks_result != KLEIDICV_OK) {
    return checks_result;
  }

  return kleidicv_median_blur_stripe_f32(
      src, src_stride, dst, dst_stride, width, height, 0, height, channels,
      kernel_width, kernel_height, fixed_border_type);
}

}  // extern "C"
