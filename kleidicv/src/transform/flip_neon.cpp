// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>

#include "kleidicv/config.h"
#include "kleidicv/ctypes.h"
#include "kleidicv/neon.h"
#include "kleidicv/transform/flip.h"
#include "kleidicv/types.h"
#include "kleidicv/utils.h"

namespace kleidicv::neon {

template <typename VectorTraits>
static inline typename VectorTraits::VectorType reverse_lanes(
    typename VectorTraits::VectorType vector) {
  constexpr size_t kHalfNumLanes = VectorTraits::num_lanes() / 2;
  vector = vrev64q(vector);
  return vextq<kHalfNumLanes>(vector, vector);
}

template <typename VectorTraits>
static inline typename VectorTraits::Vector3Type reverse_3_lanes(
    typename VectorTraits::Vector3Type vectors) {
  vectors.val[0] = reverse_lanes<VectorTraits>(vectors.val[0]);
  vectors.val[1] = reverse_lanes<VectorTraits>(vectors.val[1]);
  vectors.val[2] = reverse_lanes<VectorTraits>(vectors.val[2]);
  return vectors;
}

template <typename ScalarType, size_t kScalarsPerPixel>
static inline void swap_reversed_1_block_pair(ScalarType *left,
                                              ScalarType *right) {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;
  using Vector3 = typename VectorTraits::Vector3Type;

  if constexpr (kScalarsPerPixel == 3) {
    Vector3 left_vectors = vld3q(left);
    Vector3 right_vectors = vld3q(right);
    vst3q(right, reverse_3_lanes<VectorTraits>(left_vectors));
    vst3q(left, reverse_3_lanes<VectorTraits>(right_vectors));

    return;
  }

  // kScalarsPerPixel == 1
  Vector left_pixels = vld1q(left);
  Vector right_pixels = vld1q(right);
  vst1q(right, reverse_lanes<VectorTraits>(left_pixels));
  vst1q(left, reverse_lanes<VectorTraits>(right_pixels));
}

// [L0 L1 ... R0 R1] -> [rev(R1) rev(R0) ... rev(L1) rev(L0)]
// NOTE: 2x unroll is only used in-place for 1-scalar-per-pixel blocks
template <typename ScalarType>
static inline void swap_reversed_2_block_pairs(ScalarType *left,
                                               ScalarType *right) {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;

  Vector left_0, left_1, right_0, right_1;
  VectorTraits::load_consecutive(left, left_0, left_1);
  VectorTraits::load_consecutive(right, right_0, right_1);

  VectorTraits::store_consecutive(reverse_lanes<VectorTraits>(right_1),
                                  reverse_lanes<VectorTraits>(right_0), left);
  VectorTraits::store_consecutive(reverse_lanes<VectorTraits>(left_1),
                                  reverse_lanes<VectorTraits>(left_0), right);
}

template <typename ScalarType, size_t kScalarsPerPixel>
static kleidicv_error_t flip_horizontal_in_place(Rectangle rect,
                                                 Rows<ScalarType> rows) {
  using VectorTraits = VecTraits<ScalarType>;

  constexpr size_t kNumLanes = VectorTraits::num_lanes();
  constexpr size_t kScalarsPerBlock = kNumLanes * kScalarsPerPixel;
  constexpr size_t kScalarsPer2Blocks = kScalarsPerBlock * 2;

  // Two-pointer approach since swapping needed for in-place
  for (size_t row_idx = 0; row_idx < rect.height(); ++row_idx) {
    // For odd lengths the remaining middle pixel can stay in its place
    LoopUnroll2 col_loop{rect.width() / 2, kNumLanes};

    // Factor out .at usage since expensive within unroll loop
    ScalarType *left = &rows.at(row_idx, 0)[0];
    ScalarType *right =
        left + rect.width() * kScalarsPerPixel;  // avoids dereferencing

    // 2x unroll to encourage ldp/stp
    col_loop.unroll_twice_if<kScalarsPerPixel != 3>([&](size_t) {
      right -= kScalarsPer2Blocks;
      swap_reversed_2_block_pairs<ScalarType>(left, right);
      left += kScalarsPer2Blocks;
    });

    col_loop.unroll_once([&](size_t) {
      right -= kScalarsPerBlock;
      swap_reversed_1_block_pair<ScalarType, kScalarsPerPixel>(left, right);
      left += kScalarsPerBlock;
    });

    // Pixel-by-pixel, swap pixel with one occupying reversed position
    col_loop.tail([&](size_t) {
      right -= kScalarsPerPixel;
      std::swap_ranges(left, left + kScalarsPerPixel, right);
      left += kScalarsPerPixel;
    });
  }

  return KLEIDICV_OK;
}

template <typename ScalarType, size_t kScalarsPerPixel>
static kleidicv_error_t flip_horizontal_to_dst(Rectangle rect,
                                               Rows<const ScalarType> src_rows,
                                               Rows<ScalarType> dst_rows) {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;
  using Vector3 = typename VectorTraits::Vector3Type;

  constexpr size_t kNumLanes = VectorTraits::num_lanes();
  constexpr size_t kScalarsPerBlock = kNumLanes * kScalarsPerPixel;
  constexpr size_t kScalarsPer2Blocks = kScalarsPerBlock * 2;

  for (size_t row_idx = 0; row_idx < rect.height(); ++row_idx) {
    // Left-to-right has better performance for out-of-place
    LoopUnroll2 col_loop{rect.width(), kNumLanes};

    // Factor out .at usage since expensive within unroll loop
    const ScalarType *src = &src_rows.at(row_idx, 0)[0];
    ScalarType *dst = &dst_rows.at(row_idx, 0)[0] +
                      rect.width() * kScalarsPerPixel;  // avoids dereferencing

    col_loop.unroll_twice([&](size_t) {
      dst -= kScalarsPer2Blocks;

      if constexpr (kScalarsPerPixel == 3) {
        // 2x unroll to encourage out-of-order optimisation
        Vector3 vectors_0 = vld3q(src);
        Vector3 vectors_1 = vld3q(src + kScalarsPerBlock);
        vst3q(dst, reverse_3_lanes<VectorTraits>(vectors_1));
        vst3q(dst + kScalarsPerBlock, reverse_3_lanes<VectorTraits>(vectors_0));
      } else {
        // 2x unroll to encourage ldp/stp
        Vector pixels_0, pixels_1;
        VectorTraits::load_consecutive(src, pixels_0, pixels_1);
        VectorTraits::store_consecutive(reverse_lanes<VectorTraits>(pixels_1),
                                        reverse_lanes<VectorTraits>(pixels_0),
                                        dst);
      }

      src += kScalarsPer2Blocks;
    });

    col_loop.unroll_once([&](size_t) {
      dst -= kScalarsPerBlock;

      if constexpr (kScalarsPerPixel == 3) {
        Vector3 vectors = vld3q(src);
        vst3q(dst, reverse_3_lanes<VectorTraits>(vectors));
      } else {
        Vector pixels = vld1q(src);
        vst1q(dst, reverse_lanes<VectorTraits>(pixels));
      }

      src += kScalarsPerBlock;
    });

    // Copy to reversed position pixel-by-pixel
    col_loop.tail([&](size_t) {
      dst -= kScalarsPerPixel;
      std::copy_n(src, kScalarsPerPixel, dst);
      src += kScalarsPerPixel;
    });
  }

  return KLEIDICV_OK;
}

template <typename ScalarType, size_t kScalarsPerPixel>
static kleidicv_error_t flip(const void *src_void, size_t src_stride,
                             size_t width, size_t height, void *dst_void,
                             size_t dst_stride, int flip_mode) {
  MAKE_POINTER_CHECK_ALIGNMENT(const ScalarType, src, src_void);
  MAKE_POINTER_CHECK_ALIGNMENT(ScalarType, dst, dst_void);
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  Rectangle rect{width, height};
  Rows<const ScalarType> src_rows{src, src_stride, kScalarsPerPixel};
  Rows<ScalarType> dst_rows{dst, dst_stride, kScalarsPerPixel};

  const bool in_place = (src == dst);

  if (flip_mode <= 0) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (in_place) {
    return flip_horizontal_in_place<ScalarType, kScalarsPerPixel>(rect,
                                                                  dst_rows);
  }
  return flip_horizontal_to_dst<ScalarType, kScalarsPerPixel>(rect, src_rows,
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
