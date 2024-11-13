// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <cassert>

#include "kleidicv/arithmetics/rotate.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"

namespace kleidicv::neon {

template <const size_t BufferSize, const size_t Order, typename DstVectorType,
          typename SrcType>
static void rotate_vectors_recursively(DstVectorType *dst_vectors,
                                       Rows<const SrcType> src_rows) {
  // order is halved at every recursive call, once it is 2 the recursion should
  // stop and the input data needs to be read.
  if constexpr (Order == 2) {
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t index = 0; index < BufferSize; index += Order) {
      using SrcVectorType = typename VecTraits<SrcType>::VectorType;
      SrcVectorType src_vector[2];

      src_vector[0] = vld1q(&src_rows.at(index + 0)[0]);
      src_vector[1] = vld1q(&src_rows.at(index + 1)[0]);

      // If order is 2 then SrcVectorType is the same as DstVectorType
      dst_vectors[index + 0] = vtrn1q(src_vector[0], src_vector[1]);
      dst_vectors[index + 1] = vtrn2q(src_vector[0], src_vector[1]);
    }
  } else {
    // First the input for the current rotate stage, which is the output of
    // the previous stage, is created. The previous stage rotates
    // elements half the size of the current stage and its order is also half of
    // the current one.
    half_element_width_t<DstVectorType> input[BufferSize];
    constexpr size_t previous_order = Order / 2;

    rotate_vectors_recursively<BufferSize, previous_order>(input, src_rows);

    constexpr size_t half_order = Order / 2;

    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t outer_i = 0; outer_i < BufferSize; outer_i += Order) {
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

// Rotates one tile of data with vector instructions. The tile's width and
// height are the number of Neon lanes for the given type.
template <typename ScalarType>
static void vector_path(Rows<const ScalarType> src_rows,
                        Rows<ScalarType> dst_rows) {
  constexpr size_t num_of_lanes = VecTraits<ScalarType>::num_lanes();
  using SrcVectorType = typename VecTraits<ScalarType>::VectorType;

  // The number of vectors read and write is the same as the lane count of the
  // given element size
  constexpr size_t buffer_size = num_of_lanes;

  // Last rotate step is always done on 64 bit elements
  uint64x2_t trn_result_b64[buffer_size];  // NOLINT(runtime/arrays)

  // The 64 bit rotate spans through all the vectors, so its "order" is the
  // same as the number of vectors
  constexpr size_t rotate_order_b64 = num_of_lanes;

  rotate_vectors_recursively<buffer_size, rotate_order_b64>(trn_result_b64,
                                                            src_rows);

  KLEIDICV_FORCE_LOOP_UNROLL
  for (size_t index = 0; index < buffer_size; ++index) {
    trn_result_b64[index] = vreinterpretq_u64(
        vrev64q(reinterpret_cast<SrcVectorType>(trn_result_b64[index])));
    trn_result_b64[index] = vcombine(vget_high(trn_result_b64[index]),
                                     vget_low(trn_result_b64[index]));
    vst1q(&dst_rows.at(index)[0], trn_result_b64[index]);
  }
}

template <typename ScalarType>
static void scalar_path(Rows<const ScalarType> src_rows,
                        Rows<ScalarType> dst_rows, size_t height,
                        size_t width) {
  for (size_t vindex = 0; vindex < height; ++vindex) {
    disable_loop_vectorization();
    for (size_t hindex = 0; hindex < width; ++hindex) {
      disable_loop_vectorization();
      // dst[j][src_height - i - 1] = src[i][j]
      dst_rows.at(hindex)[height - vindex - 1] = src_rows.at(vindex)[hindex];
    }
  }
}

template <typename ScalarType>
static kleidicv_error_t rotate(Rectangle rect, Rows<const ScalarType> src_rows,
                               Rows<ScalarType> dst_rows) {
  constexpr size_t num_of_lanes = VecTraits<ScalarType>::num_lanes();
  auto handle_lane_number_of_rows = [&](size_t vindex) {
    LoopUnroll2<TryToAvoidTailLoop> horizontal_loop(rect.width(), num_of_lanes);

    horizontal_loop.unroll_once([&](size_t hindex) {
      // if the input is big enough handle it tile by tile
      vector_path<ScalarType>(
          src_rows.at(vindex, hindex),
          dst_rows.at(hindex, rect.height() - vindex - num_of_lanes));
    });

    // This branch is needed even for TryToAvoidTailLoop
    horizontal_loop.remaining([&](size_t hindex, size_t final_hindex) {
      scalar_path(src_rows.at(vindex, hindex),
                  dst_rows.at(hindex, rect.height() - vindex - num_of_lanes),
                  num_of_lanes, final_hindex - hindex);
    });
  };

  LoopUnroll2<TryToAvoidTailLoop> vertical_loop(rect.height(), num_of_lanes);

  vertical_loop.unroll_once(handle_lane_number_of_rows);

  vertical_loop.remaining([&](size_t vindex, size_t final_vindex) {
    scalar_path(src_rows.at(vindex), dst_rows.at(0, 0), final_vindex - vindex,
                rect.width());
  });
  return KLEIDICV_OK;
}

template <typename T>
static kleidicv_error_t rotate(const void *src_void, size_t src_stride,
                               size_t src_width, size_t src_height,
                               void *dst_void, size_t dst_stride) {
  MAKE_POINTER_CHECK_ALIGNMENT(const T, src, src_void);
  MAKE_POINTER_CHECK_ALIGNMENT(T, dst, dst_void);
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, src_width);
  CHECK_IMAGE_SIZE(src_width, src_height);

  Rectangle rect{src_width, src_height};
  Rows<T> dst_rows{dst, dst_stride};
  Rows<const T> src_rows{src, src_stride};

  return rotate(rect, src_rows, dst_rows);
}

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t rotate(const void *src, size_t src_stride, size_t src_width,
                        size_t src_height, void *dst, size_t dst_stride,
                        int angle, size_t element_size) {
  if (!rotate_is_implemented(src, dst, angle, element_size)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  switch (element_size) {
    case sizeof(uint8_t):
      return rotate<uint8_t>(src, src_stride, src_width, src_height, dst,
                             dst_stride);
    case sizeof(uint16_t):
      return rotate<uint16_t>(src, src_stride, src_width, src_height, dst,
                              dst_stride);
    case sizeof(uint32_t):
      return rotate<uint32_t>(src, src_stride, src_width, src_height, dst,
                              dst_stride);
    case sizeof(uint64_t):
      return rotate<uint64_t>(src, src_stride, src_width, src_height, dst,
                              dst_stride);
    // GCOVR_EXCL_START
    default:
      assert(!"element size not implemented");
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
      // GCOVR_EXCL_STOP
  }
}

}  // namespace kleidicv::neon
