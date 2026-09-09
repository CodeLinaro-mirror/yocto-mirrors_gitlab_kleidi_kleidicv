// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_TRANSFORM_FLIP_SC_H
#define KLEIDICV_TRANSFORM_FLIP_SC_H

#include "kleidicv/config.h"
#include "kleidicv/ctypes.h"
#include "kleidicv/sve2.h"
#include "kleidicv/utils.h"

namespace KLEIDICV_TARGET_NAMESPACE {

template <typename VectorTraits>
static inline typename VectorTraits::Vector3Type reverse_lanes(
    typename VectorTraits::Vector3Type vectors) KLEIDICV_STREAMING {
  using Vector = typename VectorTraits::VectorType;

  Vector v0_rev = svrev(svget3(vectors, 0));
  Vector v1_rev = svrev(svget3(vectors, 1));
  Vector v2_rev = svrev(svget3(vectors, 2));

  return svcreate3(v0_rev, v1_rev, v2_rev);
}

template <typename VectorTraits>
static inline typename VectorTraits::Vector3Type apply_tbl_per_vector(
    typename VectorTraits::Vector3Type vectors,
    typename VectorTraits::VectorType indices) KLEIDICV_STREAMING {
  using Vector = typename VectorTraits::VectorType;

  Vector v0 = svtbl(svget3(vectors, 0), indices);
  Vector v1 = svtbl(svget3(vectors, 1), indices);
  Vector v2 = svtbl(svget3(vectors, 2), indices);

  return svcreate3(v0, v1, v2);
}

template <typename ScalarType, size_t kScalarsPerPixel>
static inline void swap_reversed_pixel_block_pair(
    ScalarType *forward_block, ScalarType *backward_block) KLEIDICV_STREAMING {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;
  using Vector3 = typename VectorTraits::Vector3Type;

  svbool_t pg_true = VectorTraits::svptrue();

  if constexpr (kScalarsPerPixel == 3) {
    Vector3 fwd = svld3(pg_true, forward_block);
    Vector3 bwd = svld3(pg_true, backward_block);
    svst3(pg_true, backward_block, reverse_lanes<VectorTraits>(fwd));
    svst3(pg_true, forward_block, reverse_lanes<VectorTraits>(bwd));
  } else {
    // kScalarsPerPixel == 1
    Vector fwd = svld1(pg_true, forward_block);
    Vector bwd = svld1(pg_true, backward_block);
    svst1(pg_true, backward_block, svrev(fwd));
    svst1(pg_true, forward_block, svrev(bwd));
  }
}

template <typename ScalarType, size_t kScalarsPerPixel>
static inline void swap_reversed_pixel_block_pair_2x(
    ScalarType *forward_blocks,
    ScalarType *backward_blocks) KLEIDICV_STREAMING {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;
  using Vector3 = typename VectorTraits::Vector3Type;

  svbool_t pg_true = VectorTraits::svptrue();
  Context ctx_true{pg_true};

  if constexpr (kScalarsPerPixel == 3) {
    Vector3 fwd_0 = svld3(pg_true, forward_blocks);
    Vector3 fwd_1 = svld3_vnum(pg_true, forward_blocks, 3);
    Vector3 bwd_0 = svld3(pg_true, backward_blocks);
    Vector3 bwd_1 = svld3_vnum(pg_true, backward_blocks, 3);

    svst3(pg_true, forward_blocks, reverse_lanes<VectorTraits>(bwd_1));
    svst3_vnum(pg_true, forward_blocks, 3, reverse_lanes<VectorTraits>(bwd_0));
    svst3(pg_true, backward_blocks, reverse_lanes<VectorTraits>(fwd_1));
    svst3_vnum(pg_true, backward_blocks, 3, reverse_lanes<VectorTraits>(fwd_0));
  } else {
    // kScalarsPerPixel == 1
    Vector fwd_0, fwd_1, bwd_0, bwd_1;
    VectorTraits::load_consecutive(ctx_true, forward_blocks, fwd_0, fwd_1);
    VectorTraits::load_consecutive(ctx_true, backward_blocks, bwd_0, bwd_1);
    VectorTraits::store_consecutive(ctx_true, svrev(bwd_1), svrev(bwd_0),
                                    forward_blocks);
    VectorTraits::store_consecutive(ctx_true, svrev(fwd_1), svrev(fwd_0),
                                    backward_blocks);
  }
}

template <typename ScalarType, size_t kScalarsPerPixel>
static inline void copy_row_to_dst(
    const ScalarType *src_row, ScalarType *dst_row,
    size_t row_width_in_pixels) KLEIDICV_STREAMING {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;

  const size_t scalars_per_vec = VectorTraits::num_lanes();
  const size_t scalars_per_2_vecs = scalars_per_vec * 2;
  const size_t scalars_per_4_vecs = scalars_per_2_vecs * 2;

  LoopUnroll2 column_loop{row_width_in_pixels * kScalarsPerPixel,
                          scalars_per_vec};
  const ScalarType *src = src_row;
  ScalarType *dst = dst_row;

  svbool_t pg_true = VectorTraits::svptrue();
  Context ctx_true{pg_true};

  column_loop.unroll_four_times(
      [&](size_t) KLEIDICV_STREAMING {  // NOLINT(readability/casting)
        Vector scalars_0, scalars_1, scalars_2, scalars_3;
        VectorTraits::load_consecutive(ctx_true, src, scalars_0, scalars_1,
                                       scalars_2, scalars_3);
        VectorTraits::store_consecutive(ctx_true, scalars_0, scalars_1,
                                        scalars_2, scalars_3, dst);
        src += scalars_per_4_vecs;
        dst += scalars_per_4_vecs;
      });

  column_loop.unroll_twice(
      [&](size_t) KLEIDICV_STREAMING {  // NOLINT(readability/casting)
        Vector scalars_0, scalars_1;
        VectorTraits::load_consecutive(ctx_true, src, scalars_0, scalars_1);
        VectorTraits::store_consecutive(ctx_true, scalars_0, scalars_1, dst);
        src += scalars_per_2_vecs;
        dst += scalars_per_2_vecs;
      });

  column_loop.unroll_once(
      [&](size_t) KLEIDICV_STREAMING {  // NOLINT(readability/casting)
        Vector scalars = svld1(pg_true, src);
        svst1(pg_true, dst, scalars);
        src += scalars_per_vec;
        dst += scalars_per_vec;
      });

  column_loop.remaining(
      [&](size_t tail_begin, size_t tail_end) KLEIDICV_STREAMING {
        svbool_t pg = VectorTraits::svwhilelt(tail_begin, tail_end);
        Vector scalars = svld1(pg, src);
        svst1(pg, dst, scalars);
      });
}

template <typename ScalarType, size_t kScalarsPerPixel>
static inline void copy_reversed_row_to_dst(
    const ScalarType *src_row, ScalarType *dst_row,
    size_t row_width_in_pixels) KLEIDICV_STREAMING {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;
  using Vector3 = typename VectorTraits::Vector3Type;

  const size_t num_lanes = VectorTraits::num_lanes();
  const size_t scalars_per_block = num_lanes * kScalarsPerPixel;
  const size_t scalars_per_2_blocks = scalars_per_block * 2;
  const size_t scalars_per_4_blocks = scalars_per_2_blocks * 2;

  LoopUnroll2 column_loop{row_width_in_pixels, num_lanes};
  const ScalarType *src = src_row;
  ScalarType *dst = dst_row + row_width_in_pixels * kScalarsPerPixel;

  svbool_t pg_true = VectorTraits::svptrue();
  Context ctx_true{pg_true};

  column_loop.unroll_four_times(
      [&](size_t) KLEIDICV_STREAMING {  // NOLINT(readability/casting)
        dst -= scalars_per_4_blocks;

        if constexpr (kScalarsPerPixel == 3) {
          Vector3 vectors_0 = svld3(pg_true, src);
          Vector3 vectors_1 = svld3_vnum(pg_true, src, 3);
          Vector3 vectors_2 = svld3_vnum(pg_true, src, 6);
          Vector3 vectors_3 = svld3_vnum(pg_true, src, 9);

          svst3(pg_true, dst, reverse_lanes<VectorTraits>(vectors_3));
          svst3_vnum(pg_true, dst, 3, reverse_lanes<VectorTraits>(vectors_2));
          svst3_vnum(pg_true, dst, 6, reverse_lanes<VectorTraits>(vectors_1));
          svst3_vnum(pg_true, dst, 9, reverse_lanes<VectorTraits>(vectors_0));
        } else {
          Vector pixels_0, pixels_1, pixels_2, pixels_3;
          VectorTraits::load_consecutive(ctx_true, src, pixels_0, pixels_1,
                                         pixels_2, pixels_3);
          VectorTraits::store_consecutive(ctx_true, svrev(pixels_3),
                                          svrev(pixels_2), svrev(pixels_1),
                                          svrev(pixels_0), dst);
        }

        src += scalars_per_4_blocks;
      });

  column_loop.unroll_twice(
      [&](size_t) KLEIDICV_STREAMING {  // NOLINT(readability/casting)
        dst -= scalars_per_2_blocks;

        if constexpr (kScalarsPerPixel == 3) {
          Vector3 vectors_0 = svld3(pg_true, src);
          Vector3 vectors_1 = svld3_vnum(pg_true, src, 3);
          svst3(pg_true, dst, reverse_lanes<VectorTraits>(vectors_1));
          svst3_vnum(pg_true, dst, 3, reverse_lanes<VectorTraits>(vectors_0));
        } else {
          Vector pixels_0, pixels_1;
          VectorTraits::load_consecutive(ctx_true, src, pixels_0, pixels_1);
          VectorTraits::store_consecutive(ctx_true, svrev(pixels_1),
                                          svrev(pixels_0), dst);
        }

        src += scalars_per_2_blocks;
      });

  column_loop.unroll_once(
      [&](size_t) KLEIDICV_STREAMING {  // NOLINT(readability/casting)
        dst -= num_lanes * kScalarsPerPixel;

        if constexpr (kScalarsPerPixel == 3) {
          Vector3 vectors = svld3(pg_true, src);
          svst3(pg_true, dst, reverse_lanes<VectorTraits>(vectors));
        } else {
          Vector pixels = svld1(pg_true, src);
          Vector rev_pixels = svrev(pixels);
          svst1(pg_true, dst, rev_pixels);
        }

        src += num_lanes * kScalarsPerPixel;
      });

  column_loop.remaining(
      [&](size_t tail_begin, size_t tail_end) KLEIDICV_STREAMING {
        const size_t tail_length = tail_end - tail_begin;
        dst -= tail_length * kScalarsPerPixel;

        // Predicate & manual reversal indices for partial vector
        svbool_t pred = VectorTraits::svwhilelt(tail_begin, tail_end);
        Vector rev_indices = VectorTraits::svindex(tail_length - 1, -1);

        if constexpr (kScalarsPerPixel == 3) {
          Vector3 vectors = svld3(pred, src);
          svst3(pred, dst,
                apply_tbl_per_vector<VectorTraits>(vectors, rev_indices));
        } else {
          Vector pixels = svld1(pred, src);
          Vector rev_pixels = svtbl(pixels, rev_indices);
          svst1(pred, dst, rev_pixels);
        }
      });
}

template <typename ScalarType, size_t kScalarsPerPixel>
static inline void swap_row_pair_in_place(
    ScalarType *top_row, ScalarType *bottom_row,
    size_t row_width_in_pixels) KLEIDICV_STREAMING {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;

  const size_t scalars_per_vec = VectorTraits::num_lanes();
  const size_t scalars_per_2_vecs = scalars_per_vec * 2;
  const size_t scalars_per_4_vecs = scalars_per_2_vecs * 2;

  const size_t scalars_per_row = row_width_in_pixels * kScalarsPerPixel;

  svbool_t pg_true = VectorTraits::svptrue();
  Context ctx_true{pg_true};

  LoopUnroll2 column_loop{scalars_per_row, scalars_per_vec};

  column_loop.unroll_four_times(
      [&](size_t) KLEIDICV_STREAMING {  // NOLINT(readability/casting)
        Vector top_scalars_0, top_scalars_1, top_scalars_2, top_scalars_3;
        Vector bottom_scalars_0, bottom_scalars_1, bottom_scalars_2,
            bottom_scalars_3;

        VectorTraits::load_consecutive(ctx_true, top_row, top_scalars_0,
                                       top_scalars_1, top_scalars_2,
                                       top_scalars_3);
        VectorTraits::load_consecutive(ctx_true, bottom_row, bottom_scalars_0,
                                       bottom_scalars_1, bottom_scalars_2,
                                       bottom_scalars_3);
        VectorTraits::store_consecutive(ctx_true, top_scalars_0, top_scalars_1,
                                        top_scalars_2, top_scalars_3,
                                        bottom_row);
        VectorTraits::store_consecutive(ctx_true, bottom_scalars_0,
                                        bottom_scalars_1, bottom_scalars_2,
                                        bottom_scalars_3, top_row);

        top_row += scalars_per_4_vecs;
        bottom_row += scalars_per_4_vecs;
      });

  column_loop.unroll_twice(
      [&](size_t) KLEIDICV_STREAMING {  // NOLINT(readability/casting)
        Vector top_scalars_0, top_scalars_1, bottom_scalars_0, bottom_scalars_1;

        VectorTraits::load_consecutive(ctx_true, top_row, top_scalars_0,
                                       top_scalars_1);
        VectorTraits::load_consecutive(ctx_true, bottom_row, bottom_scalars_0,
                                       bottom_scalars_1);
        VectorTraits::store_consecutive(ctx_true, top_scalars_0, top_scalars_1,
                                        bottom_row);
        VectorTraits::store_consecutive(ctx_true, bottom_scalars_0,
                                        bottom_scalars_1, top_row);

        top_row += scalars_per_2_vecs;
        bottom_row += scalars_per_2_vecs;
      });

  column_loop.unroll_once(
      [&](size_t) KLEIDICV_STREAMING {  // NOLINT(readability/casting)
        Vector top_scalars = svld1(pg_true, top_row);
        Vector bottom_scalars = svld1(pg_true, bottom_row);
        svst1(pg_true, bottom_row, top_scalars);
        svst1(pg_true, top_row, bottom_scalars);

        top_row += scalars_per_vec;
        bottom_row += scalars_per_vec;
      });

  column_loop.remaining(
      [&](size_t tail_begin, size_t tail_end) KLEIDICV_STREAMING {
        svbool_t pg = VectorTraits::svwhilelt(tail_begin, tail_end);

        Vector top_scalars = svld1(pg, top_row);
        Vector bottom_scalars = svld1(pg, bottom_row);
        svst1(pg, bottom_row, top_scalars);
        svst1(pg, top_row, bottom_scalars);
      });
}

template <typename ScalarType, size_t kScalarsPerPixel>
static inline void swap_reversed_row_pair_in_place(
    ScalarType *top_row_start, ScalarType *bottom_row_start,
    size_t row_width_in_pixels, size_t num_pixels_to_swap) KLEIDICV_STREAMING {
  using VectorTraits = VecTraits<ScalarType>;
  using Vector = typename VectorTraits::VectorType;
  using Vector3 = typename VectorTraits::Vector3Type;

  const size_t num_lanes = VectorTraits::num_lanes();
  const size_t scalars_per_block = num_lanes * kScalarsPerPixel;
  const size_t scalars_per_2_blocks = scalars_per_block * 2;

  LoopUnroll2 column_loop{num_pixels_to_swap, num_lanes};
  ScalarType *forward = top_row_start;
  ScalarType *backward =
      bottom_row_start + row_width_in_pixels * kScalarsPerPixel;

  column_loop.unroll_twice(
      [&](size_t) KLEIDICV_STREAMING {  // NOLINT(readability/casting)
        backward -= scalars_per_2_blocks;
        swap_reversed_pixel_block_pair_2x<ScalarType, kScalarsPerPixel>(
            forward, backward);
        forward += scalars_per_2_blocks;
      });

  column_loop.unroll_once(
      [&](size_t) KLEIDICV_STREAMING {  // NOLINT(readability/casting)
        backward -= scalars_per_block;
        swap_reversed_pixel_block_pair<ScalarType, kScalarsPerPixel>(forward,
                                                                     backward);
        forward += scalars_per_block;
      });

  column_loop.remaining([&](size_t tail_begin,
                            size_t tail_end) KLEIDICV_STREAMING {
    // The forward pointer iterates over one half of the tail only.
    const size_t tail_length = tail_end - tail_begin;
    backward -= tail_length * kScalarsPerPixel;

    // Predicate & manual reversal indices for partial vector
    svbool_t pg = VectorTraits::svwhilelt(tail_begin, tail_end);
    Vector reversal_indices = VectorTraits::svindex(tail_length - 1, -1);

    if constexpr (kScalarsPerPixel == 3) {
      Vector3 forward_vectors = svld3(pg, forward);
      Vector3 backward_vectors = svld3(pg, backward);

      Vector3 forward_vectors_reversed =
          apply_tbl_per_vector<VectorTraits>(forward_vectors, reversal_indices);
      Vector3 backward_vectors_reversed = apply_tbl_per_vector<VectorTraits>(
          backward_vectors, reversal_indices);

      svst3(pg, backward, forward_vectors_reversed);
      svst3(pg, forward, backward_vectors_reversed);
    } else {
      Vector forward_pixels_reversed =
          svtbl(svld1(pg, forward), reversal_indices);
      Vector backward_pixels_reversed =
          svtbl(svld1(pg, backward), reversal_indices);

      svst1(pg, backward, forward_pixels_reversed);
      svst1(pg, forward, backward_pixels_reversed);
    }
  });
}

template <typename ScalarType, size_t kScalarsPerPixel>
static kleidicv_error_t flip_to_dst(
    Rectangle rect, Rows<const ScalarType> src_rows, Rows<ScalarType> dst_rows,
    kleidicv_flip_mode_t flip_mode) KLEIDICV_STREAMING {
  switch (flip_mode) {
    case KLEIDICV_FLIP_HORIZONTAL:
      for (size_t row_idx = 0; row_idx < rect.height(); ++row_idx) {
        const ScalarType *src_row = &src_rows.at(row_idx, 0)[0];
        ScalarType *dst_row = &dst_rows.at(row_idx, 0)[0];

        copy_reversed_row_to_dst<ScalarType, kScalarsPerPixel>(src_row, dst_row,
                                                               rect.width());
      }
      return KLEIDICV_OK;

    case KLEIDICV_FLIP_VERTICAL:
      for (size_t row_idx = 0; row_idx < rect.height(); ++row_idx) {
        const ScalarType *src_row = &src_rows.at(row_idx, 0)[0];
        ScalarType *dst_row = &dst_rows.at(rect.height() - row_idx - 1, 0)[0];

        copy_row_to_dst<ScalarType, kScalarsPerPixel>(src_row, dst_row,
                                                      rect.width());
      }
      return KLEIDICV_OK;

    case KLEIDICV_FLIP_BOTH:
      for (size_t src_row_idx = 0; src_row_idx < rect.height(); ++src_row_idx) {
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
static kleidicv_error_t flip_in_place(Rectangle rect, Rows<ScalarType> rows,
                                      kleidicv_flip_mode_t flip_mode)
    KLEIDICV_STREAMING {
  switch (flip_mode) {
    case KLEIDICV_FLIP_HORIZONTAL:
      for (size_t row_idx = 0; row_idx < rect.height(); ++row_idx) {
        ScalarType *row = &rows.at(row_idx, 0)[0];

        // For odd-width same-row, the remaining middle pixel can stay in its
        // place
        swap_reversed_row_pair_in_place<ScalarType, kScalarsPerPixel>(
            row, row, rect.width(), rect.width() / 2);
      }
      return KLEIDICV_OK;

    case KLEIDICV_FLIP_VERTICAL:
      // For odd heights, the middle row will stay in its place unchanged
      for (size_t top_row_idx = 0, bottom_row_idx = rect.height() - 1;
           top_row_idx < rect.height() / 2; ++top_row_idx, --bottom_row_idx) {
        ScalarType *top_row = &rows.at(top_row_idx, 0)[0];
        ScalarType *bottom_row = &rows.at(bottom_row_idx, 0)[0];

        swap_row_pair_in_place<ScalarType, kScalarsPerPixel>(
            top_row, bottom_row, rect.width());
      }
      return KLEIDICV_OK;

    case KLEIDICV_FLIP_BOTH:
      for (size_t row_idx = 0; row_idx < rect.height() / 2; ++row_idx) {
        ScalarType *top_row = &rows.at(row_idx, 0)[0];
        ScalarType *bottom_row = &rows.at(rect.height() - row_idx - 1, 0)[0];

        swap_reversed_row_pair_in_place<ScalarType, kScalarsPerPixel>(
            top_row, bottom_row, rect.width(), rect.width());
      }

      // Middle row (for odd heights) can be processed more efficiently
      if (rect.height() % 2 != 0) {
        ScalarType *middle_row = &rows.at(rect.height() / 2, 0)[0];

        swap_reversed_row_pair_in_place<ScalarType, kScalarsPerPixel>(
            middle_row, middle_row, rect.width(), rect.width() / 2);
      }

      return KLEIDICV_OK;

    default:
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}

template <typename ScalarType, size_t kScalarsPerPixel>
static kleidicv_error_t flip_sc(
    const void *src_void, size_t src_stride, size_t width, size_t height,
    void *dst_void, size_t dst_stride,
    kleidicv_flip_mode_t flip_mode) KLEIDICV_STREAMING {
  MAKE_POINTER_CHECK_ALIGNMENT(const ScalarType, src, src_void);
  MAKE_POINTER_CHECK_ALIGNMENT(ScalarType, dst, dst_void);
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  const bool in_place = (src == dst);
  if (in_place && src_stride != dst_stride) {
    return KLEIDICV_ERROR_RANGE;
  }

  Rectangle rect{width, height};
  Rows<const ScalarType> src_rows{src, src_stride, kScalarsPerPixel};
  Rows<ScalarType> dst_rows{dst, dst_stride, kScalarsPerPixel};

  if (in_place) {
    return flip_in_place<ScalarType, kScalarsPerPixel>(rect, dst_rows,
                                                       flip_mode);
  }
  return flip_to_dst<ScalarType, kScalarsPerPixel>(rect, src_rows, dst_rows,
                                                   flip_mode);
}

static kleidicv_error_t flip_sc(const void *src, size_t src_stride,
                                size_t width, size_t height, void *dst,
                                size_t dst_stride,
                                kleidicv_flip_mode_t flip_mode,
                                size_t pixel_size) KLEIDICV_STREAMING {
  switch (pixel_size) {
    case sizeof(uint8_t):
      return flip_sc<uint8_t, 1>(src, src_stride, width, height, dst,
                                 dst_stride, flip_mode);
    case sizeof(uint16_t):
      return flip_sc<uint16_t, 1>(src, src_stride, width, height, dst,
                                  dst_stride, flip_mode);
    case sizeof(uint32_t):
      return flip_sc<uint32_t, 1>(src, src_stride, width, height, dst,
                                  dst_stride, flip_mode);
    case sizeof(uint64_t):
      return flip_sc<uint64_t, 1>(src, src_stride, width, height, dst,
                                  dst_stride, flip_mode);
    case sizeof(uint8_t) * 3:
      return flip_sc<uint8_t, 3>(src, src_stride, width, height, dst,
                                 dst_stride, flip_mode);
    case sizeof(uint16_t) * 3:
      return flip_sc<uint16_t, 3>(src, src_stride, width, height, dst,
                                  dst_stride, flip_mode);
    default:
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_TRANSFORM_FLIP_SC_H
