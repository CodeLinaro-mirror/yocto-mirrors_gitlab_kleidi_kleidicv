// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

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
  using BufferType = uint32_t;
  using DestinationType = uint8_t;

  explicit SeparableFilter2D(const uint8_t *kernel_x, const uint8_t *kernel_y)
      : kernel_x_(kernel_x), kernel_y_(kernel_y) {}

  void vertical_vector_path(uint8x16_t src[5], BufferType *dst) const {
    uint16x8_t initial_l = vmovl_u8(vget_low_u8(src[0]));
    uint16x8_t initial_h = vmovl_u8(vget_high_u8(src[0]));

    uint32x4_t acc_l_l = vmull_n_u16(vget_low_u16(initial_l), kernel_y_[0]);
    uint32x4_t acc_l_h = vmull_n_u16(vget_high_u16(initial_l), kernel_y_[0]);
    uint32x4_t acc_h_l = vmull_n_u16(vget_low_u16(initial_h), kernel_y_[0]);
    uint32x4_t acc_h_h = vmull_n_u16(vget_high_u16(initial_h), kernel_y_[0]);

    // Optimization to avoid unnecessary branching in vector code.
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t i = 1; i < 5; i++) {
      uint16x8_t vec_l = vmovl_u8(vget_low_u8(src[i]));
      uint16x8_t vec_h = vmovl_u8(vget_high_u8(src[i]));

      acc_l_l = vmlal_n_u16(acc_l_l, vget_low_u16(vec_l), kernel_y_[i]);
      acc_l_h = vmlal_n_u16(acc_l_h, vget_high_u16(vec_l), kernel_y_[i]);
      acc_h_l = vmlal_n_u16(acc_h_l, vget_low_u16(vec_h), kernel_y_[i]);
      acc_h_h = vmlal_n_u16(acc_h_h, vget_high_u16(vec_h), kernel_y_[i]);
    }

    uint32x4x4_t result = {acc_l_l, acc_l_h, acc_h_l, acc_h_h};

    vst1q_u32_x4(&dst[0], result);
  }

  void vertical_scalar_path(const SourceType src[5], BufferType *dst) const {
    uint32_t acc = static_cast<uint32_t>(src[0]) * kernel_y_[0] +
                   static_cast<uint32_t>(src[1]) * kernel_y_[1] +
                   static_cast<uint32_t>(src[2]) * kernel_y_[2] +
                   static_cast<uint32_t>(src[3]) * kernel_y_[3] +
                   static_cast<uint32_t>(src[4]) * kernel_y_[4];

    dst[0] = acc;
  }

  void horizontal_vector_path(uint32x4_t src[5], DestinationType *dst) const {
    uint32x4_t acc = vmulq_n_u32(src[0], kernel_x_[0]);
    acc = vmlaq_n_u32(acc, src[1], kernel_x_[1]);
    acc = vmlaq_n_u32(acc, src[2], kernel_x_[2]);
    acc = vmlaq_n_u32(acc, src[3], kernel_x_[3]);
    acc = vmlaq_n_u32(acc, src[4], kernel_x_[4]);

    uint16x4_t narrowed = vmovn_u32(acc);
    uint8x8_t interleaved =
        vuzp1_u8(vreinterpret_u8_u16(narrowed), vreinterpret_u8_u16(narrowed));
    uint32_t result = vget_lane_u32(vreinterpret_u32_u8(interleaved), 0);
    memcpy(&dst[0], &result, sizeof(result));
  }

  void horizontal_scalar_path(const BufferType src[5],
                              DestinationType *dst) const {
    uint32_t acc = src[0] * kernel_x_[0] + src[1] * kernel_x_[1] +
                   src[2] * kernel_x_[2] + src[3] * kernel_x_[3] +
                   src[4] * kernel_x_[4];

    dst[0] = static_cast<uint8_t>(acc);
  }

 private:
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

  if (!fixed_border_type || *fixed_border_type != FixedBorderType::REPLICATE) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  using SeparableFilterClass = SeparableFilter2D<uint8_t, 5>;

  SeparableFilterClass filterClass{kernel_x, kernel_y};
  SeparableFilter<SeparableFilterClass, 5> filter{filterClass};

  Rows<const uint8_t> src_rows{src, src_stride, channels};
  Rows<uint8_t> dst_rows{dst, dst_stride, channels};
  workspace->process(rect, src_rows, dst_rows, channels,
                     FixedBorderType::REPLICATE, filter);

  return KLEIDICV_OK;
}

}  // namespace kleidicv::neon
