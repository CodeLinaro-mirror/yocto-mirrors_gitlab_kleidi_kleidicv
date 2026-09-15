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

namespace {

template <typename VectorTraits>
inline typename VectorTraits::VectorType reverse_lanes(
    typename VectorTraits::VectorType vector) {
  constexpr size_t kHalfNumLanes = VectorTraits::num_lanes() / 2;
  vector = vrev64q(vector);
  return vextq<kHalfNumLanes>(vector, vector);
}

template <typename VectorTraits>
inline typename VectorTraits::Vector3Type reverse_lanes(
    typename VectorTraits::Vector3Type vectors) {
  vectors.val[0] = reverse_lanes<VectorTraits>(vectors.val[0]);
  vectors.val[1] = reverse_lanes<VectorTraits>(vectors.val[1]);
  vectors.val[2] = reverse_lanes<VectorTraits>(vectors.val[2]);
  return vectors;
}

template <typename ScalarType, size_t kScalarsPerPixel>
inline void swap_reversed_pixel_block_pair(ScalarType *forward_block,
                                           ScalarType *backward_block) {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;
  using Vector3 = typename VectorTraits::Vector3Type;

  if constexpr (kScalarsPerPixel == 3) {
    Vector3 fwd = vld3q(forward_block);
    Vector3 bwd = vld3q(backward_block);
    vst3q(backward_block, reverse_lanes<VectorTraits>(fwd));
    vst3q(forward_block, reverse_lanes<VectorTraits>(bwd));
  } else {
    // kScalarsPerPixel == 1
    Vector fwd = vld1q(forward_block);
    Vector bwd = vld1q(backward_block);
    vst1q(backward_block, reverse_lanes<VectorTraits>(fwd));
    vst1q(forward_block, reverse_lanes<VectorTraits>(bwd));
  }
}

template <typename ScalarType>
inline void swap_reversed_vector_pair_2x(ScalarType *forward_vectors,
                                         ScalarType *backward_vectors) {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;

  Vector fwd_0, fwd_1, bwd_0, bwd_1;
  VectorTraits::load_consecutive(forward_vectors, fwd_0, fwd_1);
  VectorTraits::load_consecutive(backward_vectors, bwd_0, bwd_1);

  VectorTraits::store_consecutive(reverse_lanes<VectorTraits>(bwd_1),
                                  reverse_lanes<VectorTraits>(bwd_0),
                                  forward_vectors);
  VectorTraits::store_consecutive(reverse_lanes<VectorTraits>(fwd_1),
                                  reverse_lanes<VectorTraits>(fwd_0),
                                  backward_vectors);
}

template <typename ScalarType, size_t kScalarsPerPixel>
inline void copy_row_to_dst(const ScalarType *src_row, ScalarType *dst_row,
                            size_t row_width_in_pixels) {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;

  constexpr size_t kScalarsPerVec = VectorTraits::num_lanes();
  constexpr size_t kScalarsPer2Vecs = kScalarsPerVec * 2;

  LoopUnroll2 column_loop{row_width_in_pixels * kScalarsPerPixel,
                          kScalarsPerVec};
  const ScalarType *src = src_row;
  ScalarType *dst = dst_row;

  column_loop.unroll_twice([&](size_t) {
    Vector scalars_0, scalars_1;
    VectorTraits::load_consecutive(src, scalars_0, scalars_1);
    VectorTraits::store_consecutive(scalars_0, scalars_1, dst);
    src += kScalarsPer2Vecs;
    dst += kScalarsPer2Vecs;
  });

  column_loop.unroll_once([&](size_t) {
    Vector scalars = vld1q(src);
    vst1q(dst, scalars);
    src += kScalarsPerVec;
    dst += kScalarsPerVec;
  });

  column_loop.remaining([&](size_t tail_begin, size_t tail_end) {
    std::copy_n(src, tail_end - tail_begin, dst);
  });
}

template <typename ScalarType, size_t kScalarsPerPixel>
inline void copy_reversed_row_to_dst(const ScalarType *src_row,
                                     ScalarType *dst_row,
                                     size_t row_width_in_pixels) {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;
  using Vector3 = typename VectorTraits::Vector3Type;

  constexpr size_t kNumLanes = VectorTraits::num_lanes();
  constexpr size_t kScalarsPerBlock = kNumLanes * kScalarsPerPixel;
  constexpr size_t kScalarsPer2Blocks = kScalarsPerBlock * 2;

  LoopUnroll2 column_loop{row_width_in_pixels, kNumLanes};
  const ScalarType *src = src_row;
  ScalarType *dst = dst_row + row_width_in_pixels * kScalarsPerPixel;

  column_loop.unroll_twice([&](size_t) {
    dst -= kScalarsPer2Blocks;
    if constexpr (kScalarsPerPixel == 3) {
      // 2x unroll to encourage out-of-order optimisation
      Vector3 vectors_0 = vld3q(src);
      Vector3 vectors_1 = vld3q(src + kScalarsPerBlock);
      vst3q(dst, reverse_lanes<VectorTraits>(vectors_1));
      vst3q(dst + kScalarsPerBlock, reverse_lanes<VectorTraits>(vectors_0));
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

  column_loop.unroll_once([&](size_t) {
    dst -= kScalarsPerBlock;
    if constexpr (kScalarsPerPixel == 3) {
      Vector3 vectors = vld3q(src);
      vst3q(dst, reverse_lanes<VectorTraits>(vectors));
    } else {
      Vector pixels = vld1q(src);
      vst1q(dst, reverse_lanes<VectorTraits>(pixels));
    }
    src += kScalarsPerBlock;
  });

  // Copy to reversed position pixel-by-pixel
  column_loop.tail([&](size_t) {
    dst -= kScalarsPerPixel;
    std::copy_n(src, kScalarsPerPixel, dst);
    src += kScalarsPerPixel;
  });
}

template <typename ScalarType, size_t kScalarsPerPixel>
inline void swap_row_pair_in_place(ScalarType *top_row, ScalarType *bottom_row,
                                   size_t row_width_in_pixels) {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;

  constexpr size_t kScalarsPerVec = VectorTraits::num_lanes();
  constexpr size_t kScalarsPer2Vecs = kScalarsPerVec * 2;

  LoopUnroll2 column_loop{row_width_in_pixels * kScalarsPerPixel,
                          kScalarsPerVec};

  column_loop.unroll_twice([&](size_t) {
    Vector top_scalars_0, top_scalars_1, bottom_scalars_0, bottom_scalars_1;
    VectorTraits::load_consecutive(top_row, top_scalars_0, top_scalars_1);
    VectorTraits::load_consecutive(bottom_row, bottom_scalars_0,
                                   bottom_scalars_1);
    VectorTraits::store_consecutive(top_scalars_0, top_scalars_1, bottom_row);
    VectorTraits::store_consecutive(bottom_scalars_0, bottom_scalars_1,
                                    top_row);
    top_row += kScalarsPer2Vecs;
    bottom_row += kScalarsPer2Vecs;
  });

  column_loop.unroll_once([&](size_t) {
    Vector top_scalars = vld1q(top_row);
    Vector bottom_scalars = vld1q(bottom_row);
    vst1q(top_row, bottom_scalars);
    vst1q(bottom_row, top_scalars);
    top_row += kScalarsPerVec;
    bottom_row += kScalarsPerVec;
  });

  column_loop.remaining([&](size_t tail_begin, size_t tail_end) {
    std::swap_ranges(top_row, top_row + (tail_end - tail_begin), bottom_row);
  });
}

template <typename ScalarType, size_t kScalarsPerPixel>
inline void swap_reversed_row_pair_in_place(ScalarType *top_row_start,
                                            ScalarType *bottom_row_start,
                                            size_t row_width_in_pixels,
                                            size_t num_pixel_pairs_to_swap) {
  using VectorTraits = VecTraits<ScalarType>;

  constexpr size_t kScalarsPerVector = VectorTraits::num_lanes();
  constexpr size_t kScalarsPerBlock = kScalarsPerVector * kScalarsPerPixel;
  constexpr size_t kScalarsPer2Blocks = kScalarsPerBlock * 2;

  LoopUnroll2 column_loop{num_pixel_pairs_to_swap, kScalarsPerVector};
  ScalarType *forward = top_row_start;
  ScalarType *backward =
      bottom_row_start + row_width_in_pixels * kScalarsPerPixel;

  // 2x unroll to encourage ldp/stp
  column_loop.unroll_twice_if<kScalarsPerPixel != 3>([&](size_t) {
    backward -= kScalarsPer2Blocks;
    swap_reversed_vector_pair_2x<ScalarType>(forward, backward);
    forward += kScalarsPer2Blocks;
  });

  column_loop.unroll_once([&](size_t) {
    backward -= kScalarsPerBlock;
    swap_reversed_pixel_block_pair<ScalarType, kScalarsPerPixel>(forward,
                                                                 backward);
    forward += kScalarsPerBlock;
  });

  // Pixel-by-pixel, swap pixel with one occupying reversed position
  column_loop.tail([&](size_t) {
    backward -= kScalarsPerPixel;
    std::swap_ranges(forward, forward + kScalarsPerPixel, backward);
    forward += kScalarsPerPixel;
  });
}

// Out-of-place flip: each row is an independent work item
template <typename ScalarType, size_t kScalarsPerPixel>
kleidicv_error_t flip_to_dst(Rectangle rect, Rows<const ScalarType> src_rows,
                             Rows<ScalarType> dst_rows,
                             kleidicv_flip_mode_t flip_mode,
                             size_t work_items_begin, size_t work_items_end) {
  switch (flip_mode) {
    case KLEIDICV_FLIP_HORIZONTAL:
      for (size_t src_row_idx = work_items_begin; src_row_idx < work_items_end;
           ++src_row_idx) {
        const ScalarType *src_row = &src_rows.at(src_row_idx, 0)[0];
        ScalarType *dst_row = &dst_rows.at(src_row_idx, 0)[0];

        copy_reversed_row_to_dst<ScalarType, kScalarsPerPixel>(src_row, dst_row,
                                                               rect.width());
      }
      return KLEIDICV_OK;

    case KLEIDICV_FLIP_VERTICAL:
      for (size_t src_row_idx = work_items_begin; src_row_idx < work_items_end;
           ++src_row_idx) {
        const ScalarType *src_row = &src_rows.at(src_row_idx, 0)[0];
        ScalarType *dst_row =
            &dst_rows.at(rect.height() - src_row_idx - 1, 0)[0];

        copy_row_to_dst<ScalarType, kScalarsPerPixel>(src_row, dst_row,
                                                      rect.width());
      }
      return KLEIDICV_OK;

    case KLEIDICV_FLIP_BOTH:
      for (size_t src_row_idx = work_items_begin; src_row_idx < work_items_end;
           ++src_row_idx) {
        const ScalarType *src_row = &src_rows.at(src_row_idx, 0)[0];
        ScalarType *dst_row =
            &dst_rows.at(rect.height() - src_row_idx - 1, 0)[0];

        copy_reversed_row_to_dst<ScalarType, kScalarsPerPixel>(src_row, dst_row,
                                                               rect.width());
      }
      return KLEIDICV_OK;

    default:
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}

template <typename ScalarType, size_t kScalarsPerPixel>
kleidicv_error_t flip_in_place(Rectangle rect, Rows<ScalarType> rows,
                               kleidicv_flip_mode_t flip_mode,
                               size_t work_items_begin, size_t work_items_end) {
  switch (flip_mode) {
    // In-place horizontal flip can treat each row as an independent work item
    case KLEIDICV_FLIP_HORIZONTAL:
      for (size_t row_idx = work_items_begin; row_idx < work_items_end;
           ++row_idx) {
        ScalarType *row = &rows.at(row_idx, 0)[0];

        // For odd-width same-row, the middle pixel can stay in its place
        swap_reversed_row_pair_in_place<ScalarType, kScalarsPerPixel>(
            row, row, rect.width(), rect.width() / 2);
      }
      return KLEIDICV_OK;

    // Each in-place vertical-flip work item owns two mirrored rows.
    // For odd heights, the centre row is unchanged and has no work item.
    case KLEIDICV_FLIP_VERTICAL:
      for (size_t top_row_idx = work_items_begin; top_row_idx < work_items_end;
           ++top_row_idx) {
        const size_t bottom_row_idx = rect.height() - top_row_idx - 1;
        ScalarType *top_row = &rows.at(top_row_idx, 0)[0];
        ScalarType *bottom_row = &rows.at(bottom_row_idx, 0)[0];

        swap_row_pair_in_place<ScalarType, kScalarsPerPixel>(
            top_row, bottom_row, rect.width());
      }
      return KLEIDICV_OK;

    // Each in-place both-axes-flip work item owns two mirrored rows, except
    // for odd heights, where the final work item owns only the centre row.
    case KLEIDICV_FLIP_BOTH:
      for (size_t top_row_idx = work_items_begin; top_row_idx < work_items_end;
           ++top_row_idx) {
        const size_t bottom_row_idx = rect.height() - top_row_idx - 1;
        ScalarType *top_row = &rows.at(top_row_idx, 0)[0];
        ScalarType *bottom_row = &rows.at(bottom_row_idx, 0)[0];

        // For odd heights, the final work item owns the centre row and only
        // needs to iterate up to half way as pixels will be swapped with those
        // on the other side of the row.
        const size_t num_pixel_pairs_to_swap =
            top_row_idx == bottom_row_idx ? rect.width() / 2 : rect.width();
        swap_reversed_row_pair_in_place<ScalarType, kScalarsPerPixel>(
            top_row, bottom_row, rect.width(), num_pixel_pairs_to_swap);
      }

      return KLEIDICV_OK;

    default:
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}

template <typename ScalarType, size_t kScalarsPerPixel>
kleidicv_error_t flip_work_items_impl(const void *src_void, size_t src_stride,
                                      size_t width, size_t height,
                                      void *dst_void, size_t dst_stride,
                                      kleidicv_flip_mode_t flip_mode,
                                      size_t work_items_begin,
                                      size_t work_items_end) {
  const auto *src = static_cast<const ScalarType *>(src_void);
  auto *dst = static_cast<ScalarType *>(dst_void);

  Rectangle rect{width, height};
  Rows<const ScalarType> src_rows{src, src_stride, kScalarsPerPixel};
  Rows<ScalarType> dst_rows{dst, dst_stride, kScalarsPerPixel};

  if (src == dst) {
    return flip_in_place<ScalarType, kScalarsPerPixel>(
        rect, dst_rows, flip_mode, work_items_begin, work_items_end);
  }
  return flip_to_dst<ScalarType, kScalarsPerPixel>(
      rect, src_rows, dst_rows, flip_mode, work_items_begin, work_items_end);
}

}  // namespace

// The half-open work-item range follows the mapping documented in flip.h.
KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t flip_work_items(const void *src, size_t src_stride,
                                 size_t width, size_t height, void *dst,
                                 size_t dst_stride,
                                 kleidicv_flip_mode_t flip_mode,
                                 size_t pixel_size, size_t work_items_begin,
                                 size_t work_items_end) {
  switch (pixel_size) {
    case sizeof(uint8_t):
      return flip_work_items_impl<uint8_t, 1>(src, src_stride, width, height,
                                              dst, dst_stride, flip_mode,
                                              work_items_begin, work_items_end);
    case sizeof(uint16_t):
      return flip_work_items_impl<uint16_t, 1>(
          src, src_stride, width, height, dst, dst_stride, flip_mode,
          work_items_begin, work_items_end);
    case sizeof(uint32_t):
      return flip_work_items_impl<uint32_t, 1>(
          src, src_stride, width, height, dst, dst_stride, flip_mode,
          work_items_begin, work_items_end);
    case sizeof(uint64_t):
      return flip_work_items_impl<uint64_t, 1>(
          src, src_stride, width, height, dst, dst_stride, flip_mode,
          work_items_begin, work_items_end);
    case sizeof(uint8_t) * 3:
      return flip_work_items_impl<uint8_t, 3>(src, src_stride, width, height,
                                              dst, dst_stride, flip_mode,
                                              work_items_begin, work_items_end);
    case sizeof(uint16_t) * 3:
      return flip_work_items_impl<uint16_t, 3>(
          src, src_stride, width, height, dst, dst_stride, flip_mode,
          work_items_begin, work_items_end);
    default:
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}

}  // namespace kleidicv::neon
