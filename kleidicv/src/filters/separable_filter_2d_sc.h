// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_SEPARABLE_FILTER_2D_SC_H
#define KLEIDICV_SEPARABLE_FILTER_2D_SC_H

#include <limits>

#include "kleidicv/kleidicv.h"
#include "kleidicv/separable_filter_5x5_sc.h"
#include "kleidicv/sve2.h"

namespace KLEIDICV_TARGET_NAMESPACE {

template <typename ScalarType, size_t KernelSize>
class SeparableFilter2D;

template <>
class SeparableFilter2D<uint8_t, 5> {
 public:
  using SourceType = uint8_t;
  using SourceVectorType = typename VecTraits<SourceType>::VectorType;
  using BufferType = uint16_t;
  using BufferVectorType = typename VecTraits<BufferType>::VectorType;
  using BufferDoubleVectorType = typename VecTraits<BufferType>::Vector2Type;
  using DestinationType = uint8_t;

  SeparableFilter2D(
      const SourceType *kernel_x, BufferVectorType &kernel_x_0_u16,
      BufferVectorType &kernel_x_1_u16, BufferVectorType &kernel_x_2_u16,
      BufferVectorType &kernel_x_3_u16, BufferVectorType &kernel_x_4_u16,
      SourceVectorType &kernel_y_0_u8, SourceVectorType &kernel_y_1_u8,
      SourceVectorType &kernel_y_2_u8, SourceVectorType &kernel_y_3_u8,
      SourceVectorType &kernel_y_4_u8)
      : kernel_x_(kernel_x),
        kernel_x_0_u16_(kernel_x_0_u16),
        kernel_x_1_u16_(kernel_x_1_u16),
        kernel_x_2_u16_(kernel_x_2_u16),
        kernel_x_3_u16_(kernel_x_3_u16),
        kernel_x_4_u16_(kernel_x_4_u16),

        kernel_y_0_u8_(kernel_y_0_u8),
        kernel_y_1_u8_(kernel_y_1_u8),
        kernel_y_2_u8_(kernel_y_2_u8),
        kernel_y_3_u8_(kernel_y_3_u8),
        kernel_y_4_u8_(kernel_y_4_u8) {}

  void vertical_vector_path(
      svbool_t pg, SourceVectorType src_0, SourceVectorType src_1,
      SourceVectorType src_2, SourceVectorType src_3, SourceVectorType src_4,
      BufferType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    // 0
    BufferVectorType acc_b = svmullb_u16(src_0, kernel_y_0_u8_);
    BufferVectorType acc_t = svmullt_u16(src_0, kernel_y_0_u8_);

    // 1
    acc_b = svmlalb_u16(acc_b, src_1, kernel_y_1_u8_);
    acc_t = svmlalt_u16(acc_t, src_1, kernel_y_1_u8_);

    // 2
    acc_b = svmlalb_u16(acc_b, src_2, kernel_y_2_u8_);
    acc_t = svmlalt_u16(acc_t, src_2, kernel_y_2_u8_);

    // 3
    acc_b = svmlalb_u16(acc_b, src_3, kernel_y_3_u8_);
    acc_t = svmlalt_u16(acc_t, src_3, kernel_y_3_u8_);

    // 4
    acc_b = svmlalb_u16(acc_b, src_4, kernel_y_4_u8_);
    acc_t = svmlalt_u16(acc_t, src_4, kernel_y_4_u8_);

    BufferDoubleVectorType interleaved = svcreate2_u16(acc_b, acc_t);
    svst2(pg, &dst[0], interleaved);
  }

  void horizontal_vector_path(
      svbool_t pg, BufferVectorType src_0, BufferVectorType src_1,
      BufferVectorType src_2, BufferVectorType src_3, BufferVectorType src_4,
      DestinationType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    // 0
    BufferVectorType acc = svmul_u16_x(pg, src_0, kernel_x_0_u16_);

    // 1
    acc = svmla_u16_x(pg, acc, src_1, kernel_x_1_u16_);

    // 2
    acc = svmla_u16_x(pg, acc, src_2, kernel_x_2_u16_);

    // 3
    acc = svmla_u16_x(pg, acc, src_3, kernel_x_3_u16_);

    // 4
    acc = svmla_u16_x(pg, acc, src_4, kernel_x_4_u16_);

    svbool_t greater =
        svcmpgt_n_u16(pg, acc, std::numeric_limits<SourceType>::max());
    acc = svdup_n_u16_m(acc, greater, std::numeric_limits<SourceType>::max());
    svst1b_u16(pg, &dst[0], acc);
  }

  void horizontal_scalar_path(const BufferType src[5], DestinationType *dst)
      const KLEIDICV_STREAMING_COMPATIBLE {
    SourceType acc;  // NOLINT
    if (__builtin_mul_overflow(src[0], kernel_x_[0], &acc)) {
      dst[0] = std::numeric_limits<SourceType>::max();
      return;
    }

    for (size_t i = 1; i < 5; i++) {
      SourceType temp;  // NOLINT
      if (__builtin_mul_overflow(src[i], kernel_x_[i], &temp)) {
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

 private:
  const SourceType *kernel_x_;

  BufferVectorType &kernel_x_0_u16_;
  BufferVectorType &kernel_x_1_u16_;
  BufferVectorType &kernel_x_2_u16_;
  BufferVectorType &kernel_x_3_u16_;
  BufferVectorType &kernel_x_4_u16_;

  SourceVectorType &kernel_y_0_u8_;
  SourceVectorType &kernel_y_1_u8_;
  SourceVectorType &kernel_y_2_u8_;
  SourceVectorType &kernel_y_3_u8_;
  SourceVectorType &kernel_y_4_u8_;
};  // end of class SeparableFilter2D<uint8_t, 5>

static kleidicv_error_t separable_filter_2d_u8_sc(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, const uint8_t *kernel_x,
    size_t kernel_width, const uint8_t *kernel_y, size_t kernel_height,
    kleidicv_border_type_t border_type,
    kleidicv_filter_context_t *context) KLEIDICV_STREAMING_COMPATIBLE {
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

  svuint16_t kernel_x_0_u16 = svdup_n_u16(kernel_x[0]);
  svuint16_t kernel_x_1_u16 = svdup_n_u16(kernel_x[1]);
  svuint16_t kernel_x_2_u16 = svdup_n_u16(kernel_x[2]);
  svuint16_t kernel_x_3_u16 = svdup_n_u16(kernel_x[3]);
  svuint16_t kernel_x_4_u16 = svdup_n_u16(kernel_x[4]);

  svuint8_t kernel_y_0_u8 = svdup_n_u8(kernel_y[0]);
  svuint8_t kernel_y_1_u8 = svdup_n_u8(kernel_y[1]);
  svuint8_t kernel_y_2_u8 = svdup_n_u8(kernel_y[2]);
  svuint8_t kernel_y_3_u8 = svdup_n_u8(kernel_y[3]);
  svuint8_t kernel_y_4_u8 = svdup_n_u8(kernel_y[4]);

  SeparableFilterClass filterClass{
      kernel_x,       kernel_x_0_u16, kernel_x_1_u16, kernel_x_2_u16,
      kernel_x_3_u16, kernel_x_4_u16, kernel_y_0_u8,  kernel_y_1_u8,
      kernel_y_2_u8,  kernel_y_3_u8,  kernel_y_4_u8};
  SeparableFilter<SeparableFilterClass, 5> filter{filterClass};

  Rows<const uint8_t> src_rows{src, src_stride, channels};
  Rows<uint8_t> dst_rows{dst, dst_stride, channels};
  workspace->process(rect, src_rows, dst_rows, channels, *fixed_border_type,
                     filter);

  return KLEIDICV_OK;
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_SEPARABLE_FILTER_2D_SC_H
