// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_ROTATE_TRANSPOSE_COMMON_NEON_H
#define KLEIDICV_ROTATE_TRANSPOSE_COMMON_NEON_H

#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"

namespace kleidicv::neon {

// Recursive SIMD transpose/rotate kernel for a single tile.
template <const size_t TileSize, const size_t Order, bool ReverseRows,
          typename DstVectorType, typename SrcType>
static inline void rotate_transpose_tile_recursive(
    DstVectorType *dst_vectors, Rows<const SrcType> src_rows) {
  if constexpr (Order == 2) {
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t index = 0; index < TileSize; index += Order) {
      using SrcVectorType = typename VecTraits<SrcType>::VectorType;
      SrcVectorType src_vector[2];

      src_vector[0] = vld1q(&src_rows.at(index + 0U)[0]);
      src_vector[1] = vld1q(&src_rows.at(index + 1U)[0]);

      // If order is 2 then SrcVectorType is the same as DstVectorType.
      if constexpr (ReverseRows) {
        dst_vectors[TileSize - 1U - (index + 1U)] =
            vtrn1q(src_vector[1], src_vector[0]);
        dst_vectors[TileSize - 1U - (index + 0U)] =
            vtrn2q(src_vector[1], src_vector[0]);
      } else {
        dst_vectors[index + 0] = vtrn1q(src_vector[0], src_vector[1]);
        dst_vectors[index + 1] = vtrn2q(src_vector[0], src_vector[1]);
      }
    }
  } else {
    half_element_width_t<DstVectorType> input[TileSize];
    constexpr size_t previous_order = Order / 2;

    rotate_transpose_tile_recursive<TileSize, previous_order, ReverseRows>(
        input, src_rows);

    constexpr size_t half_order = Order / 2;

    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t outer_i = 0; outer_i < TileSize; outer_i += Order) {
      KLEIDICV_FORCE_LOOP_UNROLL
      for (size_t inner_i = 0; inner_i < half_order; ++inner_i) {
        dst_vectors[outer_i + inner_i] =
            vtrn1q(reinterpret_cast<DstVectorType>(input[outer_i + inner_i]),
                   reinterpret_cast<DstVectorType>(
                       input[outer_i + inner_i + half_order]));

        dst_vectors[outer_i + half_order + inner_i] =
            vtrn2q(reinterpret_cast<DstVectorType>(input[outer_i + inner_i]),
                   reinterpret_cast<DstVectorType>(
                       input[outer_i + inner_i + half_order]));
      }
    }
  }
}

// Recursive SIMD transpose/rotate kernel for one 3-channel tile.
template <const size_t kTileSize, const size_t kOrder, bool kReverseRows,
          typename DstVectorType, typename SrcType>
static inline void rotate_transpose_tile_recursive_3ch(
    DstVectorType *dst_c0, DstVectorType *dst_c1, DstVectorType *dst_c2,
    Rows<const SrcType> src_rows) {
  if constexpr (kOrder == 2) {
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t index = 0; index < kTileSize; index += kOrder) {
      const size_t row0 = index + 0U;
      const size_t row1 = index + 1U;

      auto v0 = vld3q(&src_rows.at(row0, 0)[0]);
      auto v1 = vld3q(&src_rows.at(row1, 0)[0]);

      if constexpr (kReverseRows) {
        const size_t dst0 = kTileSize - 2U - index;
        const size_t dst1 = kTileSize - 1U - index;
        dst_c0[dst0] = vtrn1q(v1.val[0], v0.val[0]);
        dst_c0[dst1] = vtrn2q(v1.val[0], v0.val[0]);
        dst_c1[dst0] = vtrn1q(v1.val[1], v0.val[1]);
        dst_c1[dst1] = vtrn2q(v1.val[1], v0.val[1]);
        dst_c2[dst0] = vtrn1q(v1.val[2], v0.val[2]);
        dst_c2[dst1] = vtrn2q(v1.val[2], v0.val[2]);
      } else {
        dst_c0[index + 0] = vtrn1q(v0.val[0], v1.val[0]);
        dst_c0[index + 1] = vtrn2q(v0.val[0], v1.val[0]);
        dst_c1[index + 0] = vtrn1q(v0.val[1], v1.val[1]);
        dst_c1[index + 1] = vtrn2q(v0.val[1], v1.val[1]);
        dst_c2[index + 0] = vtrn1q(v0.val[2], v1.val[2]);
        dst_c2[index + 1] = vtrn2q(v0.val[2], v1.val[2]);
      }
    }
  } else {
    half_element_width_t<DstVectorType> input_c0[kTileSize];
    half_element_width_t<DstVectorType> input_c1[kTileSize];
    half_element_width_t<DstVectorType> input_c2[kTileSize];
    constexpr size_t previous_order = kOrder / 2;

    rotate_transpose_tile_recursive_3ch<kTileSize, previous_order,
                                        kReverseRows>(input_c0, input_c1,
                                                      input_c2, src_rows);

    constexpr size_t half_order = kOrder / 2;

    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t outer_i = 0; outer_i < kTileSize; outer_i += kOrder) {
      KLEIDICV_FORCE_LOOP_UNROLL
      for (size_t inner_i = 0; inner_i < half_order; ++inner_i) {
        const size_t left = outer_i + inner_i;
        const size_t right = outer_i + inner_i + half_order;

        dst_c0[left] = vtrn1q(reinterpret_cast<DstVectorType>(input_c0[left]),
                              reinterpret_cast<DstVectorType>(input_c0[right]));
        dst_c0[right] =
            vtrn2q(reinterpret_cast<DstVectorType>(input_c0[left]),
                   reinterpret_cast<DstVectorType>(input_c0[right]));

        dst_c1[left] = vtrn1q(reinterpret_cast<DstVectorType>(input_c1[left]),
                              reinterpret_cast<DstVectorType>(input_c1[right]));
        dst_c1[right] =
            vtrn2q(reinterpret_cast<DstVectorType>(input_c1[left]),
                   reinterpret_cast<DstVectorType>(input_c1[right]));

        dst_c2[left] = vtrn1q(reinterpret_cast<DstVectorType>(input_c2[left]),
                              reinterpret_cast<DstVectorType>(input_c2[right]));
        dst_c2[right] =
            vtrn2q(reinterpret_cast<DstVectorType>(input_c2[left]),
                   reinterpret_cast<DstVectorType>(input_c2[right]));
      }
    }
  }
}

// Processes one tile of data with vector instructions. The tile's width and
// height are the number of NEON lanes for the given type.
// Vectorized transpose/rotate of one square tile (lanes x lanes).
template <bool ReverseRows, typename ScalarType, typename RowIndexFn>
static inline void rotate_transpose_tile(Rows<const ScalarType> src_rows,
                                         Rows<ScalarType> dst_rows,
                                         RowIndexFn row_index) {
  constexpr size_t num_of_lanes = VecTraits<ScalarType>::num_lanes();

  // The number of vectors read and write is the same as the lane count of the
  // given element size.
  constexpr size_t buffer_size = num_of_lanes;

  // Last transpose/rotate step is always done on 64-bit elements.
  uint64x2_t trn_result_b64[buffer_size];  // NOLINT(runtime/arrays)

  // The 64-bit transpose/rotate spans through all the vectors, so its order
  // is the same as the number of vectors.
  constexpr size_t transpose_order_b64 = num_of_lanes;

  rotate_transpose_tile_recursive<buffer_size, transpose_order_b64,
                                  ReverseRows>(trn_result_b64, src_rows);

  KLEIDICV_FORCE_LOOP_UNROLL
  for (size_t index = 0; index < buffer_size; ++index) {
    vst1q(&dst_rows.at(row_index(index, buffer_size))[0],
          trn_result_b64[index]);
  }
}

// Vectorized transpose/rotate of one 3-channel tile (lanes x lanes).
template <bool ReverseRows, typename ScalarType, typename RowIndexFn>
static inline void rotate_transpose_tile_3ch(Rows<const ScalarType> src_rows,
                                             Rows<ScalarType> dst_rows,
                                             RowIndexFn row_index) {
  constexpr size_t num_of_lanes = VecTraits<ScalarType>::num_lanes();
  constexpr size_t buffer_size = num_of_lanes;
  using VecType = typename VecTraits<ScalarType>::VectorType;

  uint64x2_t dst_c0[buffer_size];  // NOLINT(runtime/arrays)
  uint64x2_t dst_c1[buffer_size];  // NOLINT(runtime/arrays)
  uint64x2_t dst_c2[buffer_size];  // NOLINT(runtime/arrays)

  constexpr size_t rotate_order = num_of_lanes;
  rotate_transpose_tile_recursive_3ch<num_of_lanes, rotate_order, ReverseRows>(
      dst_c0, dst_c1, dst_c2, src_rows);

  KLEIDICV_FORCE_LOOP_UNROLL
  for (size_t index = 0; index < num_of_lanes; ++index) {
    VecType v0;
    VecType v1;
    VecType v2;
    if constexpr (std::is_same_v<ScalarType, uint8_t>) {
      v0 = vreinterpretq_u8_u64(dst_c0[index]);
      v1 = vreinterpretq_u8_u64(dst_c1[index]);
      v2 = vreinterpretq_u8_u64(dst_c2[index]);
    } else {
      v0 = vreinterpretq_u16_u64(dst_c0[index]);
      v1 = vreinterpretq_u16_u64(dst_c1[index]);
      v2 = vreinterpretq_u16_u64(dst_c2[index]);
    }
    auto v = decltype(vld3q(&dst_rows.at(0, 0)[0])){v0, v1, v2};
    vst3q(&dst_rows.at(row_index(index, num_of_lanes), 0)[0], v);
  }
}

template <bool ReverseRows, typename ScalarType, typename RowIndexFn>
static inline void rotate_transpose_tile_dispatch(
    std::false_type, Rows<const ScalarType> src_rows, Rows<ScalarType> dst_rows,
    RowIndexFn row_index) {
  rotate_transpose_tile<ReverseRows, ScalarType>(src_rows, dst_rows, row_index);
}

template <bool ReverseRows, typename ScalarType, typename RowIndexFn>
static inline void rotate_transpose_tile_dispatch(
    std::true_type, Rows<const ScalarType> src_rows, Rows<ScalarType> dst_rows,
    RowIndexFn row_index) {
  rotate_transpose_tile_3ch<ReverseRows, ScalarType>(src_rows, dst_rows,
                                                     row_index);
}

// Scalar fallback for tail tiles/segments with a caller-provided store mapping.
template <bool k3Channels, typename ScalarType, typename StoreFn>
static inline void rotate_transpose_scalar(Rows<const ScalarType> src_rows,
                                           Rows<ScalarType> dst_rows,
                                           size_t height, size_t width,
                                           StoreFn store) {
  for (size_t vindex = 0; vindex < height; ++vindex) {
    disable_loop_vectorization();
    for (size_t hindex = 0; hindex < width; ++hindex) {
      disable_loop_vectorization();
      if constexpr (k3Channels) {
        const ScalarType *src = &src_rows.at(vindex, hindex)[0];
        store(dst_rows, hindex, vindex, height, src);
      } else {
        store(dst_rows, hindex, vindex, height, src_rows.at(vindex)[hindex]);
      }
    }
  }
}

// Driver for whole-rectangle rotate/transpose with vector tiles and scalar
// tails.
template <bool k3Channels, bool kReverseRows, typename ScalarType,
          typename VectorDstRowFn, typename VectorDstColFn,
          typename ScalarDstRowFn, typename ScalarDstColFn,
          typename VerticalDstColFn, typename RowIndexFn,
          typename ScalarStoreFn>
static inline kleidicv_error_t rotate_transpose_rect(
    Rectangle rect, Rows<const ScalarType> src_rows, Rows<ScalarType> dst_rows,
    VectorDstRowFn vector_dst_row_from_src_col,
    VectorDstColFn vector_dst_col_from_src_row,
    ScalarDstRowFn scalar_dst_row_from_src_col,
    ScalarDstColFn scalar_dst_col_from_src_row,
    VerticalDstColFn vertical_dst_col_base, RowIndexFn row_index,
    ScalarStoreFn scalar_store) {
  constexpr size_t num_of_lanes = VecTraits<ScalarType>::num_lanes();
  const size_t rect_width = rect.width();
  const size_t rect_height = rect.height();

  const auto store = [&](size_t segment_width) {
    return [&, segment_width](Rows<ScalarType> &rows, size_t h, size_t v,
                              size_t height, const auto value) {
      scalar_store(rows, h, v, height, value, segment_width);
    };
  };

  const auto rotate_vector_tile = [&](size_t vindex, size_t hindex) {
    rotate_transpose_tile_dispatch<kReverseRows, ScalarType>(
        std::bool_constant<k3Channels>{}, src_rows.at(vindex, hindex),
        dst_rows.at(vector_dst_row_from_src_col(hindex, num_of_lanes),
                    vector_dst_col_from_src_row(vindex, num_of_lanes)),
        row_index);
  };

  const auto rotate_scalar_segment = [&](size_t vindex, size_t hindex,
                                         size_t final_hindex) {
    const size_t segment_width = final_hindex - hindex;
    rotate_transpose_scalar<k3Channels>(
        src_rows.at(vindex, hindex),
        dst_rows.at(scalar_dst_row_from_src_col(hindex, final_hindex),
                    scalar_dst_col_from_src_row(vindex, num_of_lanes)),
        num_of_lanes, segment_width, store(segment_width));
  };

  auto handle_lane_number_of_rows = [&](size_t vindex) {
    LoopUnroll2<TryToAvoidTailLoop> horizontal_loop(rect_width, num_of_lanes);

    horizontal_loop.unroll_once([&](size_t hindex) {
      // If the input is big enough, handle it tile by tile. ReverseRows
      // toggles CW/CCW behavior; keep this direct call to simplify adding CCW
      // support later.
      rotate_vector_tile(vindex, hindex);
    });

    // This branch is needed even for TryToAvoidTailLoop.
    horizontal_loop.remaining([&](size_t hindex, size_t final_hindex) {
      rotate_scalar_segment(vindex, hindex, final_hindex);
    });
  };

  LoopUnroll2<TryToAvoidTailLoop> vertical_loop(rect_height, num_of_lanes);

  vertical_loop.unroll_once(handle_lane_number_of_rows);

  vertical_loop.remaining([&](size_t vindex, size_t final_vindex) {
    rotate_transpose_scalar<k3Channels>(
        src_rows.at(vindex), dst_rows.at(0, vertical_dst_col_base(vindex)),
        final_vindex - vindex, rect_width, store(rect_width));
  });
  return KLEIDICV_OK;
}

}  // namespace kleidicv::neon

#endif  // KLEIDICV_ROTATE_TRANSPOSE_COMMON_NEON_H
