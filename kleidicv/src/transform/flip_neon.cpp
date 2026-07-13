// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>

#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"
#include "kleidicv/transform/flip.h"
#include "kleidicv/types.h"
#include "kleidicv/utils.h"

namespace kleidicv::neon {

template <typename VectorType>
static inline void reverse_lanes(VectorType &vector) {
  vector = vrev64q(vector);
  vector = vcombine(vget_high(vector), vget_low(vector));
}

template <typename Vector3Type>
static inline void reverse_3_lanes(Vector3Type &vectors) {
  reverse_lanes(vectors.val[0]);
  reverse_lanes(vectors.val[1]);
  reverse_lanes(vectors.val[2]);
}

template <typename ScalarType, size_t kScalarPerPixel, bool kInPlace>
static kleidicv_error_t flip_horiz(Rectangle rect,
                                   Rows<const ScalarType> src_rows,
                                   Rows<ScalarType> dst_rows) {
  using Traits = VecTraits<ScalarType>;
  using Vector = typename Traits::VectorType;
  using Vector3 = typename Traits::Vector3Type;
  constexpr size_t num_lanes = Traits::num_lanes();

  if constexpr (kInPlace) {
    for (size_t row_idx = 0; row_idx < rect.height(); ++row_idx) {
      // Only need to iterate up to half the row; odd-width middle pixel is
      // unchanged
      LoopUnroll2 col_loop{rect.width() / 2, num_lanes};

      col_loop.unroll_once([&](size_t col_idx) {
        ScalarType *left = &dst_rows.at(row_idx, col_idx)[0];
        ScalarType *right =
            &dst_rows.at(row_idx, rect.width() - col_idx - num_lanes)[0];

        if constexpr (kScalarPerPixel == 1) {
          Vector left_pixels = vld1q(left);
          Vector right_pixels = vld1q(right);
          reverse_lanes(left_pixels);
          reverse_lanes(right_pixels);
          vst1q(right, left_pixels);
          vst1q(left, right_pixels);
        } else {
          Vector3 left_vectors = vld3q(left);
          Vector3 right_vectors = vld3q(right);

          reverse_3_lanes(left_vectors);
          reverse_3_lanes(right_vectors);

          vst3q(right, left_vectors);
          vst3q(left, right_vectors);
        }
      });

      col_loop.tail([&](size_t col_idx) {
        ScalarType *left = &dst_rows.at(row_idx, col_idx)[0];
        ScalarType *right =
            &dst_rows.at(row_idx, rect.width() - col_idx - 1)[0];

        std::swap_ranges(left, left + kScalarPerPixel, right);
      });
    }
  } else {
    for (size_t row_idx = 0; row_idx < rect.height(); ++row_idx) {
      LoopUnroll2 col_loop{rect.width(), num_lanes};

      col_loop.unroll_once([&](size_t col_idx) {
        const ScalarType *src = &src_rows.at(row_idx, col_idx)[0];
        ScalarType *dst =
            &dst_rows.at(row_idx, rect.width() - col_idx - num_lanes)[0];

        if constexpr (kScalarPerPixel == 1) {
          Vector pixels = vld1q(src);
          reverse_lanes(pixels);
          vst1q(dst, pixels);
        } else {
          Vector3 vectors = vld3q(src);
          reverse_3_lanes(vectors);
          vst3q(dst, vectors);
        }
      });

      col_loop.tail([&](size_t col_idx) {
        const ScalarType *src = &src_rows.at(row_idx, col_idx)[0];
        ScalarType *dst = &dst_rows.at(row_idx, rect.width() - col_idx - 1)[0];

        std::copy_n(src, kScalarPerPixel, dst);
      });
    }
  }

  return KLEIDICV_OK;
}

template <typename ScalarType, size_t kScalarPerPixel>
static kleidicv_error_t flip(const void *src_void, size_t src_stride,
                             size_t width, size_t height, void *dst_void,
                             size_t dst_stride, int flip_mode) {
  MAKE_POINTER_CHECK_ALIGNMENT(const ScalarType, src, src_void);
  MAKE_POINTER_CHECK_ALIGNMENT(ScalarType, dst, dst_void);
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  Rectangle rect{width, height};
  Rows<const ScalarType> src_rows{src, src_stride, kScalarPerPixel};
  Rows<ScalarType> dst_rows{dst, dst_stride, kScalarPerPixel};

  const bool in_place = (src == dst);

  // Only horizontal flip is implemented at this time
  if (flip_mode <= 0) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (in_place) {
    return flip_horiz<ScalarType, kScalarPerPixel, true>(rect, src_rows,
                                                         dst_rows);
  }
  return flip_horiz<ScalarType, kScalarPerPixel, false>(rect, src_rows,
                                                        dst_rows);
}

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t flip(const void *src, size_t src_stride, size_t width,
                      size_t height, void *dst, size_t dst_stride,
                      int flip_mode, size_t pixel_size) {
  switch (pixel_size) {
    case sizeof(uint8_t):
      return flip<uint8_t, 1>(src, src_stride, width, height, dst, dst_stride,
                              flip_mode);
    case sizeof(uint16_t):
      return flip<uint16_t, 1>(src, src_stride, width, height, dst, dst_stride,
                               flip_mode);
    case sizeof(uint32_t):
      return flip<uint32_t, 1>(src, src_stride, width, height, dst, dst_stride,
                               flip_mode);
    case sizeof(uint64_t):
      return flip<uint64_t, 1>(src, src_stride, width, height, dst, dst_stride,
                               flip_mode);
    case sizeof(uint8_t) * 3:
      return flip<uint8_t, 3>(src, src_stride, width, height, dst, dst_stride,
                              flip_mode);
    case sizeof(uint16_t) * 3:
      return flip<uint16_t, 3>(src, src_stride, width, height, dst, dst_stride,
                               flip_mode);
    default:
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}

}  // namespace kleidicv::neon
