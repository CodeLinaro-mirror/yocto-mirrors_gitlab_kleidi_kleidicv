// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_SEPARABLE_FILTER_2D_SC_H
#define KLEIDICV_SEPARABLE_FILTER_2D_SC_H

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
  using BufferType = uint32_t;
  using DestinationType = uint8_t;

  explicit SeparableFilter2D(const uint8_t *kernel_x, const uint8_t *kernel_y)
      : kernel_x_(kernel_x), kernel_y_(kernel_y) {}

  void vertical_vector_path(svbool_t pg, svuint8_t src_0, svuint8_t src_1,
                            svuint8_t src_2, svuint8_t src_3, svuint8_t src_4,
                            BufferType *dst) const
      KLEIDICV_STREAMING_COMPATIBLE {
    // 2
    svuint16_t vec_0_b = svmovlb_u16(src_0);
    svuint16_t vec_0_t = svmovlt_u16(src_0);

    svuint32_t acc_b_b = svmullb_n_u32(vec_0_b, kernel_y_[0]);
    svuint32_t acc_b_t = svmullb_n_u32(vec_0_t, kernel_y_[0]);
    svuint32_t acc_t_b = svmullt_n_u32(vec_0_b, kernel_y_[0]);
    svuint32_t acc_t_t = svmullt_n_u32(vec_0_t, kernel_y_[0]);

    // 1
    svuint16_t vec_1_b = svmovlb_u16(src_1);
    svuint16_t vec_1_t = svmovlt_u16(src_1);

    acc_b_b = svmlalb_n_u32(acc_b_b, vec_1_b, kernel_y_[1]);
    acc_b_t = svmlalb_n_u32(acc_b_t, vec_1_t, kernel_y_[1]);
    acc_t_b = svmlalt_n_u32(acc_t_b, vec_1_b, kernel_y_[1]);
    acc_t_t = svmlalt_n_u32(acc_t_t, vec_1_t, kernel_y_[1]);

    // 2
    svuint16_t vec_2_b = svmovlb_u16(src_2);
    svuint16_t vec_2_t = svmovlt_u16(src_2);

    acc_b_b = svmlalb_n_u32(acc_b_b, vec_2_b, kernel_y_[2]);
    acc_b_t = svmlalb_n_u32(acc_b_t, vec_2_t, kernel_y_[2]);
    acc_t_b = svmlalt_n_u32(acc_t_b, vec_2_b, kernel_y_[2]);
    acc_t_t = svmlalt_n_u32(acc_t_t, vec_2_t, kernel_y_[2]);

    // 3
    svuint16_t vec_3_b = svmovlb_u16(src_3);
    svuint16_t vec_3_t = svmovlt_u16(src_3);

    acc_b_b = svmlalb_n_u32(acc_b_b, vec_3_b, kernel_y_[3]);
    acc_b_t = svmlalb_n_u32(acc_b_t, vec_3_t, kernel_y_[3]);
    acc_t_b = svmlalt_n_u32(acc_t_b, vec_3_b, kernel_y_[3]);
    acc_t_t = svmlalt_n_u32(acc_t_t, vec_3_t, kernel_y_[3]);

    // 4
    svuint16_t vec_4_b = svmovlb_u16(src_4);
    svuint16_t vec_4_t = svmovlt_u16(src_4);

    acc_b_b = svmlalb_n_u32(acc_b_b, vec_4_b, kernel_y_[4]);
    acc_b_t = svmlalb_n_u32(acc_b_t, vec_4_t, kernel_y_[4]);
    acc_t_b = svmlalt_n_u32(acc_t_b, vec_4_b, kernel_y_[4]);
    acc_t_t = svmlalt_n_u32(acc_t_t, vec_4_t, kernel_y_[4]);

    svuint32x4_t interleaved = svcreate4(acc_b_b, acc_b_t, acc_t_b, acc_t_t);
    svst4(pg, &dst[0], interleaved);
  }

  void horizontal_vector_path(svbool_t pg, svuint32_t src_0, svuint32_t src_1,
                              svuint32_t src_2, svuint32_t src_3,
                              svuint32_t src_4, DestinationType *dst) const
      KLEIDICV_STREAMING_COMPATIBLE {
    // 0
    svuint32_t acc = svmul_n_u32_x(pg, src_0, kernel_x_[0]);

    // 1
    acc = svmla_n_u32_x(pg, acc, src_1, kernel_x_[1]);

    // 2
    acc = svmla_n_u32_x(pg, acc, src_2, kernel_x_[2]);

    // 3
    acc = svmla_n_u32_x(pg, acc, src_3, kernel_x_[3]);

    // 4
    acc = svmla_n_u32_x(pg, acc, src_4, kernel_x_[4]);

    svst1b_u32(pg, &dst[0], acc);
  }

  void horizontal_scalar_path(const BufferType src[5], DestinationType *dst)
      const KLEIDICV_STREAMING_COMPATIBLE {
    uint32_t acc = src[0] * kernel_x_[0] + src[1] * kernel_x_[1] +
                   src[2] * kernel_x_[2] + src[3] * kernel_x_[3] +
                   src[4] * kernel_x_[4];
    dst[0] = static_cast<uint8_t>(acc);
  }

 private:
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
  SeparableFilter<SeparableFilterClass, 5> filter{filterClass};

  Rows<const uint8_t> src_rows{src, src_stride, channels};
  Rows<uint8_t> dst_rows{dst, dst_stride, channels};
  workspace->process(rect, src_rows, dst_rows, channels, *fixed_border_type,
                     filter);

  return KLEIDICV_OK;
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_SEPARABLE_FILTER_2D_SC_H
