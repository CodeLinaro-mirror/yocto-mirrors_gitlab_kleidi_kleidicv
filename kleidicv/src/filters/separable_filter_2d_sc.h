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
    BufferVectorType vec_b = svmullb_u16(src_1, kernel_y_1_u8_);
    BufferVectorType vec_t = svmullt_u16(src_1, kernel_y_1_u8_);
    acc_b = svqadd_u16_x(pg, acc_b, vec_b);
    acc_t = svqadd_u16_x(pg, acc_t, vec_t);

    // 2
    vec_b = svmullb_u16(src_2, kernel_y_2_u8_);
    vec_t = svmullt_u16(src_2, kernel_y_2_u8_);
    acc_b = svqadd_u16_x(pg, acc_b, vec_b);
    acc_t = svqadd_u16_x(pg, acc_t, vec_t);

    // 3
    vec_b = svmullb_u16(src_3, kernel_y_3_u8_);
    vec_t = svmullt_u16(src_3, kernel_y_3_u8_);
    acc_b = svqadd_u16_x(pg, acc_b, vec_b);
    acc_t = svqadd_u16_x(pg, acc_t, vec_t);

    // 4
    vec_b = svmullb_u16(src_4, kernel_y_4_u8_);
    vec_t = svmullt_u16(src_4, kernel_y_4_u8_);
    acc_b = svqadd_u16_x(pg, acc_b, vec_b);
    acc_t = svqadd_u16_x(pg, acc_t, vec_t);

    BufferDoubleVectorType interleaved = svcreate2_u16(acc_b, acc_t);
    svst2(pg, &dst[0], interleaved);
  }

  void horizontal_vector_path(
      svbool_t pg, BufferVectorType src_0, BufferVectorType src_1,
      BufferVectorType src_2, BufferVectorType src_3, BufferVectorType src_4,
      DestinationType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    // 0
    svuint32_t acc_b = svmullb_u32(src_0, kernel_x_0_u16_);
    svuint32_t acc_t = svmullt_u32(src_0, kernel_x_0_u16_);

    // 1
    acc_b = svmlalb_u32(acc_b, src_1, kernel_x_1_u16_);
    acc_t = svmlalt_u32(acc_t, src_1, kernel_x_1_u16_);

    // 2
    acc_b = svmlalb_u32(acc_b, src_2, kernel_x_2_u16_);
    acc_t = svmlalt_u32(acc_t, src_2, kernel_x_2_u16_);

    // 3
    acc_b = svmlalb_u32(acc_b, src_3, kernel_x_3_u16_);
    acc_t = svmlalt_u32(acc_t, src_3, kernel_x_3_u16_);

    // 4
    acc_b = svmlalb_u32(acc_b, src_4, kernel_x_4_u16_);
    acc_t = svmlalt_u32(acc_t, src_4, kernel_x_4_u16_);

    svuint16_t acc_u16_b = svqxtnb_u32(acc_b);
    svuint16_t acc_u16 = svqxtnt_u32(acc_u16_b, acc_t);

    svbool_t greater =
        svcmpgt_n_u16(pg, acc_u16, std::numeric_limits<SourceType>::max());
    acc_u16 =
        svdup_n_u16_m(acc_u16, greater, std::numeric_limits<SourceType>::max());

    svst1b_u16(pg, &dst[0], acc_u16);
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

template <>
class SeparableFilter2D<uint16_t, 5> {
 public:
  using SourceType = uint16_t;
  using SourceVectorType = typename VecTraits<SourceType>::VectorType;
  using BufferType = uint32_t;
  using BufferVectorType = typename VecTraits<BufferType>::VectorType;
  using BufferDoubleVectorType = typename VecTraits<BufferType>::Vector2Type;
  using DestinationType = uint16_t;

  SeparableFilter2D(
      const SourceType *kernel_x, BufferVectorType &kernel_x_0_u32,
      BufferVectorType &kernel_x_1_u32, BufferVectorType &kernel_x_2_u32,
      BufferVectorType &kernel_x_3_u32, BufferVectorType &kernel_x_4_u32,
      SourceVectorType &kernel_y_0_u16, SourceVectorType &kernel_y_1_u16,
      SourceVectorType &kernel_y_2_u16, SourceVectorType &kernel_y_3_u16,
      SourceVectorType &kernel_y_4_u16)
      : kernel_x_(kernel_x),
        kernel_x_0_u32_(kernel_x_0_u32),
        kernel_x_1_u32_(kernel_x_1_u32),
        kernel_x_2_u32_(kernel_x_2_u32),
        kernel_x_3_u32_(kernel_x_3_u32),
        kernel_x_4_u32_(kernel_x_4_u32),

        kernel_y_0_u16_(kernel_y_0_u16),
        kernel_y_1_u16_(kernel_y_1_u16),
        kernel_y_2_u16_(kernel_y_2_u16),
        kernel_y_3_u16_(kernel_y_3_u16),
        kernel_y_4_u16_(kernel_y_4_u16) {}

  void vertical_vector_path(
      svbool_t pg, SourceVectorType src_0, SourceVectorType src_1,
      SourceVectorType src_2, SourceVectorType src_3, SourceVectorType src_4,
      BufferType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    // 0
    BufferVectorType acc_b = svmullb_u32(src_0, kernel_y_0_u16_);
    BufferVectorType acc_t = svmullt_u32(src_0, kernel_y_0_u16_);

    // 1
    BufferVectorType vec_b = svmullb_u32(src_1, kernel_y_1_u16_);
    BufferVectorType vec_t = svmullt_u32(src_1, kernel_y_1_u16_);
    acc_b = svqadd_u32_x(pg, acc_b, vec_b);
    acc_t = svqadd_u32_x(pg, acc_t, vec_t);

    // 2
    vec_b = svmullb_u32(src_2, kernel_y_2_u16_);
    vec_t = svmullt_u32(src_2, kernel_y_2_u16_);
    acc_b = svqadd_u32_x(pg, acc_b, vec_b);
    acc_t = svqadd_u32_x(pg, acc_t, vec_t);

    // 3
    vec_b = svmullb_u32(src_3, kernel_y_3_u16_);
    vec_t = svmullt_u32(src_3, kernel_y_3_u16_);
    acc_b = svqadd_u32_x(pg, acc_b, vec_b);
    acc_t = svqadd_u32_x(pg, acc_t, vec_t);

    // 4
    vec_b = svmullb_u32(src_4, kernel_y_4_u16_);
    vec_t = svmullt_u32(src_4, kernel_y_4_u16_);
    acc_b = svqadd_u32_x(pg, acc_b, vec_b);
    acc_t = svqadd_u32_x(pg, acc_t, vec_t);

    BufferDoubleVectorType interleaved = svcreate2_u32(acc_b, acc_t);
    svst2(pg, &dst[0], interleaved);
  }

  void horizontal_vector_path(
      svbool_t pg, BufferVectorType src_0, BufferVectorType src_1,
      BufferVectorType src_2, BufferVectorType src_3, BufferVectorType src_4,
      DestinationType *dst) const KLEIDICV_STREAMING_COMPATIBLE {
    // 0
    svuint64_t acc_b = svmullb_u64(src_0, kernel_x_0_u32_);
    svuint64_t acc_t = svmullt_u64(src_0, kernel_x_0_u32_);

    // 1
    acc_b = svmlalb_u64(acc_b, src_1, kernel_x_1_u32_);
    acc_t = svmlalt_u64(acc_t, src_1, kernel_x_1_u32_);

    // 2
    acc_b = svmlalb_u64(acc_b, src_2, kernel_x_2_u32_);
    acc_t = svmlalt_u64(acc_t, src_2, kernel_x_2_u32_);

    // 3
    acc_b = svmlalb_u64(acc_b, src_3, kernel_x_3_u32_);
    acc_t = svmlalt_u64(acc_t, src_3, kernel_x_3_u32_);

    // 4
    acc_b = svmlalb_u64(acc_b, src_4, kernel_x_4_u32_);
    acc_t = svmlalt_u64(acc_t, src_4, kernel_x_4_u32_);

    svuint32_t acc_u32_b = svqxtnb_u64(acc_b);
    svuint32_t acc_u32 = svqxtnt_u64(acc_u32_b, acc_t);

    svbool_t greater =
        svcmpgt_n_u32(pg, acc_u32, std::numeric_limits<SourceType>::max());
    acc_u32 =
        svdup_n_u32_m(acc_u32, greater, std::numeric_limits<SourceType>::max());

    svst1h_u32(pg, &dst[0], acc_u32);
  }

  void horizontal_scalar_path(const BufferType src[5], DestinationType *dst)
      const KLEIDICV_STREAMING_COMPATIBLE {
    SourceType acc;  // Avoid cppcoreguidelines-init-variables. NOLINT
    if (__builtin_mul_overflow(src[0], kernel_x_[0], &acc)) {
      dst[0] = std::numeric_limits<SourceType>::max();
      return;
    }

    for (size_t i = 1; i < 5; i++) {
      SourceType temp;  // Avoid cppcoreguidelines-init-variables. NOLINT
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

  BufferVectorType &kernel_x_0_u32_;
  BufferVectorType &kernel_x_1_u32_;
  BufferVectorType &kernel_x_2_u32_;
  BufferVectorType &kernel_x_3_u32_;
  BufferVectorType &kernel_x_4_u32_;

  SourceVectorType &kernel_y_0_u16_;
  SourceVectorType &kernel_y_1_u16_;
  SourceVectorType &kernel_y_2_u16_;
  SourceVectorType &kernel_y_3_u16_;
  SourceVectorType &kernel_y_4_u16_;
};  // end of class SeparableFilter2D<uint16_t, 5>

template <typename T>
static kleidicv_error_t separable_filter_2d_checks(
    const T *src, size_t src_stride, T *dst, size_t dst_stride, size_t width,
    size_t height, size_t channels, const T *kernel_x, size_t kernel_width,
    const T *kernel_y, size_t kernel_height,
    SeparableFilterWorkspace *workspace) KLEIDICV_STREAMING_COMPATIBLE {
  CHECK_POINTERS(workspace, kernel_x, kernel_y);

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

  const Rectangle &context_rect = workspace->image_size();
  if (context_rect.width() < width || context_rect.height() < height) {
    return KLEIDICV_ERROR_CONTEXT_MISMATCH;
  }

  return KLEIDICV_OK;
}

static kleidicv_error_t separable_filter_2d_stripe_u8_sc(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels,
    const uint8_t *kernel_x, size_t kernel_width, const uint8_t *kernel_y,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_filter_context_t *context) KLEIDICV_STREAMING_COMPATIBLE {
  auto *workspace = reinterpret_cast<SeparableFilterWorkspace *>(context);
  kleidicv_error_t checks_result = separable_filter_2d_checks(
      src, src_stride, dst, dst_stride, width, height, channels, kernel_x,
      kernel_width, kernel_y, kernel_height, workspace);

  if (checks_result != KLEIDICV_OK) {
    return checks_result;
  }

  auto fixed_border_type = get_fixed_border_type(border_type);
  // if the std::optional is empty, that means that the border type is not
  // supported, so there's no need to check for specific types
  if (!fixed_border_type) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  Rectangle rect{width, height};

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
  workspace->process(rect, y_begin, y_end, src_rows, dst_rows, channels,
                     *fixed_border_type, filter);

  return KLEIDICV_OK;
}

static kleidicv_error_t separable_filter_2d_stripe_u16_sc(
    const uint16_t *src, size_t src_stride, uint16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels,
    const uint16_t *kernel_x, size_t kernel_width, const uint16_t *kernel_y,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_filter_context_t *context) KLEIDICV_STREAMING_COMPATIBLE {
  auto *workspace = reinterpret_cast<SeparableFilterWorkspace *>(context);
  kleidicv_error_t checks_result = separable_filter_2d_checks(
      src, src_stride, dst, dst_stride, width, height, channels, kernel_x,
      kernel_width, kernel_y, kernel_height, workspace);

  if (checks_result != KLEIDICV_OK) {
    return checks_result;
  }

  auto fixed_border_type = get_fixed_border_type(border_type);
  // if the std::optional is empty, that means that the border type is not
  // supported, so there's no need to check for specific types
  if (!fixed_border_type) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  Rectangle rect{width, height};

  using SeparableFilterClass = SeparableFilter2D<uint16_t, 5>;

  svuint32_t kernel_x_0_u32 = svdup_n_u32(kernel_x[0]);
  svuint32_t kernel_x_1_u32 = svdup_n_u32(kernel_x[1]);
  svuint32_t kernel_x_2_u32 = svdup_n_u32(kernel_x[2]);
  svuint32_t kernel_x_3_u32 = svdup_n_u32(kernel_x[3]);
  svuint32_t kernel_x_4_u32 = svdup_n_u32(kernel_x[4]);

  svuint16_t kernel_y_0_u16 = svdup_n_u16(kernel_y[0]);
  svuint16_t kernel_y_1_u16 = svdup_n_u16(kernel_y[1]);
  svuint16_t kernel_y_2_u16 = svdup_n_u16(kernel_y[2]);
  svuint16_t kernel_y_3_u16 = svdup_n_u16(kernel_y[3]);
  svuint16_t kernel_y_4_u16 = svdup_n_u16(kernel_y[4]);

  SeparableFilterClass filterClass{
      kernel_x,       kernel_x_0_u32, kernel_x_1_u32, kernel_x_2_u32,
      kernel_x_3_u32, kernel_x_4_u32, kernel_y_0_u16, kernel_y_1_u16,
      kernel_y_2_u16, kernel_y_3_u16, kernel_y_4_u16};
  SeparableFilter<SeparableFilterClass, 5> filter{filterClass};

  Rows<const uint16_t> src_rows{src, src_stride, channels};
  Rows<uint16_t> dst_rows{dst, dst_stride, channels};
  workspace->process(rect, y_begin, y_end, src_rows, dst_rows, channels,
                     *fixed_border_type, filter);

  return KLEIDICV_OK;
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_SEPARABLE_FILTER_2D_SC_H
