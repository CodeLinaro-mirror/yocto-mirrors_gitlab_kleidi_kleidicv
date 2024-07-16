// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <limits>

#include "kleidicv/ctypes.h"
#include "kleidicv/filters/separable_filter_2d.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"
#include "kleidicv/separable_filter_5x5_neon.h"

namespace kleidicv::neon {

template <typename ScalarType, size_t KernelSize>
class SeparableFilter2D;

template <>
class SeparableFilter2D<uint8_t, 5> {
 public:
  using SourceType = uint8_t;
  using BufferType = uint8_t;
  using DestinationType = uint8_t;

  explicit SeparableFilter2D(const uint8_t *kernel_x, const uint8_t *kernel_y)
      : kernel_x_(kernel_x), kernel_y_(kernel_y) {}

  void vertical_vector_path(uint8x16_t src[5], BufferType *dst) const {
    this->vector_path_with_kernel(src, dst, kernel_y_);
  }

  void vertical_scalar_path(const SourceType src[5], BufferType *dst) const {
    this->scalar_path_with_kernel(src, dst, kernel_y_);
  }

  void horizontal_vector_path(uint8x16_t src[5], DestinationType *dst) const {
    this->vector_path_with_kernel(src, dst, kernel_x_);
  }

  void horizontal_scalar_path(const BufferType src[5],
                              DestinationType *dst) const {
    this->scalar_path_with_kernel(src, dst, kernel_x_);
  }

 private:
  void vector_path_with_kernel(uint8x16_t src[5], uint8_t *dst,
                               const uint8_t *kernel) const {
    uint16x8_t acc_l = vmovl_u8(vget_low_u8(src[0]));
    uint16x8_t acc_h = vmovl_u8(vget_high_u8(src[0]));

    acc_l = vmulq_n_u16(acc_l, kernel[0]);
    acc_h = vmulq_n_u16(acc_h, kernel[0]);

    // Optimization to avoid unnecessary branching in vector code.
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t i = 1; i < 5; i++) {
      uint16x8_t vec_l = vmovl_u8(vget_low_u8(src[i]));
      uint16x8_t vec_h = vmovl_u8(vget_high_u8(src[i]));

      acc_l = vmlaq_n_u16(acc_l, vec_l, kernel[i]);
      acc_h = vmlaq_n_u16(acc_h, vec_h, kernel[i]);
    }

    uint8x8_t result_l = vqmovn_u16(acc_l);
    uint8x16_t result = vqmovn_high_u16(result_l, acc_h);

    vst1q_u8(&dst[0], result);
  }

  void scalar_path_with_kernel(const uint8_t src[5], uint8_t *dst,
                               const uint8_t *kernel) const {
    uint8_t acc;  // NOLINT
    if (__builtin_mul_overflow(src[0], kernel[0], &acc)) {
      dst[0] = std::numeric_limits<SourceType>::max();
      return;
    }

    for (size_t i = 1; i < 5; i++) {
      uint8_t temp;  // NOLINT
      if (__builtin_mul_overflow(src[i], kernel[i], &temp)) {
        dst[0] = std::numeric_limits<SourceType>::max();
        return;
      }
      if (__builtin_add_overflow(acc, temp, &acc)) {
        dst[0] = std::numeric_limits<SourceType>::max();
        return;
      }
    }

    dst[0] = acc;
  }

  const uint8_t *kernel_x_;
  const uint8_t *kernel_y_;
};

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t separable_filter_2d_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, const uint8_t *kernel_x,
    size_t kernel_width, const uint8_t *kernel_y, size_t kernel_height,
    kleidicv_border_type_t border_type, kleidicv_filter_context_t *context) {
  CHECK_POINTERS(context, kernel_x, kernel_y);
  auto *workspace = reinterpret_cast<SeparableFilterWorkspace *>(context);
  auto fixed_border_type = get_fixed_border_type(border_type);

  if (kernel_width != 5 || kernel_height != 5) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  if (width < kernel_width - 1 || height < kernel_width - 1) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT) {
    return KLEIDICV_ERROR_RANGE;
  }

  if (workspace->channels() < channels) {
    return KLEIDICV_ERROR_CONTEXT_MISMATCH;
  }

  Rectangle rect{width, height};
  const Rectangle &context_rect = workspace->image_size();
  if (context_rect.width() < width || context_rect.height() < height) {
    return KLEIDICV_ERROR_CONTEXT_MISMATCH;
  }

  // if the std::optional is empty, that means that the border type is not
  // supported, so there's no need to check for specific types
  if (!fixed_border_type) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  using SeparableFilterClass = SeparableFilter2D<uint8_t, 5>;

  SeparableFilterClass filterClass{kernel_x, kernel_y};
  SeparableFilter<SeparableFilterClass, 5> filter{filterClass};

  Rows<const uint8_t> src_rows{src, src_stride, channels};
  Rows<uint8_t> dst_rows{dst, dst_stride, channels};
  workspace->process(rect, src_rows, dst_rows, channels, *fixed_border_type,
                     filter);

  return KLEIDICV_OK;
}

}  // namespace kleidicv::neon
