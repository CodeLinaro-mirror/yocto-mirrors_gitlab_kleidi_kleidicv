// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_SEPARABLE_FILTER_2D_SC_H
#define KLEIDICV_SEPARABLE_FILTER_2D_SC_H

#include <limits>

#include "kleidicv/filter_driver_sc.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/sc.h"

namespace KLEIDICV_TARGET_NAMESPACE {

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

  void vertical_vector_path(svbool_t pg, svuint8_t src_0, svuint8_t src_1,
                            svuint8_t src_2, svuint8_t src_3, svuint8_t src_4,
                            BufferType *dst) const
      KLEIDICV_STREAMING_COMPATIBLE {
    this->vector_path_with_kernel(pg, src_0, src_1, src_2, src_3, src_4, dst,
                                  kernel_y_);
  }

  void horizontal_vector_path(svbool_t pg, svuint8_t src_0, svuint8_t src_1,
                              svuint8_t src_2, svuint8_t src_3, svuint8_t src_4,
                              DestinationType *dst) const
      KLEIDICV_STREAMING_COMPATIBLE {
    this->vector_path_with_kernel(pg, src_0, src_1, src_2, src_3, src_4, dst,
                                  kernel_x_);
  }

  void horizontal_scalar_path(const BufferType src[5], DestinationType *dst)
      const KLEIDICV_STREAMING_COMPATIBLE {
    uint8_t acc;  // NOLINT
    if (__builtin_mul_overflow(src[0], kernel_x_[0], &acc)) {
      dst[0] = std::numeric_limits<SourceType>::max();
      return;
    }

    for (size_t i = 1; i < 5; i++) {
      uint8_t temp;  // NOLINT
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
  void vector_path_with_kernel(
      svbool_t pg, svuint8_t src_0, svuint8_t src_1, svuint8_t src_2,
      svuint8_t src_3, svuint8_t src_4, BufferType *dst,
      const uint8_t *kernel) const KLEIDICV_STREAMING_COMPATIBLE {
    // 0
    svuint16_t acc_b = svmovlb_u16(src_0);
    svuint16_t acc_t = svmovlt_u16(src_0);

    acc_b = svmul_n_u16_x(pg, acc_b, kernel[0]);
    acc_t = svmul_n_u16_x(pg, acc_t, kernel[0]);

    // 1
    svuint16_t vec_1_b = svmovlb_u16(src_1);
    svuint16_t vec_1_t = svmovlt_u16(src_1);

    acc_b = svmla_n_u16_x(pg, acc_b, vec_1_b, kernel[1]);
    acc_t = svmla_n_u16_x(pg, acc_t, vec_1_t, kernel[1]);

    // 2
    svuint16_t vec_2_b = svmovlb_u16(src_2);
    svuint16_t vec_2_t = svmovlt_u16(src_2);

    acc_b = svmla_n_u16_x(pg, acc_b, vec_2_b, kernel[2]);
    acc_t = svmla_n_u16_x(pg, acc_t, vec_2_t, kernel[2]);

    // 3
    svuint16_t vec_3_b = svmovlb_u16(src_3);
    svuint16_t vec_3_t = svmovlt_u16(src_3);

    acc_b = svmla_n_u16_x(pg, acc_b, vec_3_b, kernel[3]);
    acc_t = svmla_n_u16_x(pg, acc_t, vec_3_t, kernel[3]);

    // 4
    svuint16_t vec_4_b = svmovlb_u16(src_4);
    svuint16_t vec_4_t = svmovlt_u16(src_4);

    acc_b = svmla_n_u16_x(pg, acc_b, vec_4_b, kernel[4]);
    acc_t = svmla_n_u16_x(pg, acc_t, vec_4_t, kernel[4]);

    svuint8_t result_b = svqxtnb_u16(acc_b);
    svuint8_t result = svqxtnt_u16(result_b, acc_t);
    svst1_u8(pg, &dst[0], result);
  }

  const uint8_t *kernel_x_;
  const uint8_t *kernel_y_;
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

  SeparableFilterClass filterClass{kernel_x, kernel_y};
  SeparableFilterDriver<SeparableFilterClass, 5> filter{filterClass};

  Rows<const uint8_t> src_rows{src, src_stride, channels};
  Rows<uint8_t> dst_rows{dst, dst_stride, channels};
  workspace->process(rect, src_rows, dst_rows, channels, *fixed_border_type,
                     filter);

  return KLEIDICV_OK;
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_SEPARABLE_FILTER_2D_SC_H
