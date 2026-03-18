// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"
#include "kleidicv/transform/transpose.h"
#include "rotate_transpose_common_neon.h"

namespace kleidicv::neon {

template <typename ScalarType, bool k3Channels>
static kleidicv_error_t transpose_to_dst(Rectangle rect,
                                         Rows<const ScalarType> src_rows,
                                         Rows<ScalarType> dst_rows) {
  auto vector_dst_row_from_src_col = [](size_t hindex, size_t) {
    return hindex;
  };
  auto vector_dst_col_from_src_row = [](size_t vindex, size_t) {
    return vindex;
  };
  auto scalar_dst_row_from_src_col = [](size_t hindex, size_t) {
    return hindex;
  };
  auto scalar_dst_col_from_src_row = [](size_t vindex, size_t) {
    return vindex;
  };
  auto vertical_dst_col_base = [](size_t vindex) { return vindex; };
  auto row_index = [](size_t index, size_t) { return index; };
  auto scalar_store = [](Rows<ScalarType> &rows, size_t h, size_t v, size_t,
                         const auto value, size_t) {
    if constexpr (k3Channels) {
      ScalarType *dst = &rows.at(h, v)[0];
      dst[0] = value[0];
      dst[1] = value[1];
      dst[2] = value[2];
    } else {
      rows.at(h)[v] = value;
    }
  };

  return rotate_transpose_rect<ScalarType, k3Channels, false>(
      rect, src_rows, dst_rows, vector_dst_row_from_src_col,
      vector_dst_col_from_src_row, scalar_dst_row_from_src_col,
      scalar_dst_col_from_src_row, vertical_dst_col_base, row_index,
      scalar_store);
}

template <typename ScalarType>
static kleidicv_error_t transpose_in_place(Rectangle rect,
                                           Rows<ScalarType> data_rows) {
  constexpr size_t num_of_lanes = VecTraits<ScalarType>::num_lanes();

  // rect.width() needs to be equal to rect.height()
  LoopUnroll2 outer_loop(rect.width(), num_of_lanes);

  outer_loop.unroll_once([&](size_t vindex) {
    auto row_index = [](size_t index, size_t) { return index; };
    // Handle tiles on the diagonal line
    rotate_transpose_tile<false, ScalarType>(
        data_rows.at(vindex, vindex), data_rows.at(vindex, vindex), row_index);

    // Handle the top right half
    if (rect.width() > (vindex + num_of_lanes)) {
      // Indexes are running through only the top right half
      LoopUnroll2 inner_loop(vindex + num_of_lanes, rect.width(), num_of_lanes);

      inner_loop.unroll_once([&](size_t hindex) {
        // Allocate temporary memory for one tile
        ScalarType tmp[num_of_lanes * num_of_lanes];  // NOLINT(runtime/arrays)
        Rows<ScalarType> tmp_rows{tmp, num_of_lanes * sizeof(ScalarType)};

        // Transpose a tile from the top right area, save the result
        // into temporary memory
        rotate_transpose_tile<false, ScalarType>(data_rows.at(vindex, hindex),
                                                 tmp_rows, row_index);
        // Transpose its mirror tile from the left bottom area, save the
        // result to its final space
        rotate_transpose_tile<false, ScalarType>(data_rows.at(hindex, vindex),
                                                 data_rows.at(vindex, hindex),
                                                 row_index);
        // Copy the temprory result to its final destination
        Rows<const ScalarType> const_tmp_rows{
            tmp, num_of_lanes * sizeof(ScalarType)};
        CopyNonOverlappingRows<ScalarType>::copy_rows(
            Rectangle{num_of_lanes, num_of_lanes}, const_tmp_rows,
            data_rows.at(hindex, vindex));
      });

      inner_loop.remaining([&](size_t hindex, size_t final_hindex) {
        // As this is the unroll_once path of the outer_loop there is
        // num_of_lanes worth of data in the vertical direction
        for (size_t i = vindex; i < vindex + num_of_lanes; ++i) {
          disable_loop_vectorization();
          for (size_t j = hindex; j < final_hindex; ++j) {
            disable_loop_vectorization();
            std::swap(data_rows.at(i)[j], data_rows.at(j)[i]);
          }
        }
      });
    }
  });

  outer_loop.remaining([&](size_t vindex, size_t final_vindex) {
    for (size_t i = vindex; i < final_vindex; ++i) {
      disable_loop_vectorization();
      // Only the top right half pixels need to be indexed
      for (size_t j = i + 1; j < final_vindex; ++j) {
        disable_loop_vectorization();
        std::swap(data_rows.at(i)[j], data_rows.at(j)[i]);
      }
    }
  });
  return KLEIDICV_OK;
}

template <typename ScalarType>
static kleidicv_error_t transpose_in_place_3ch(Rectangle rect,
                                               Rows<ScalarType> data_rows) {
  constexpr size_t num_of_lanes = VecTraits<ScalarType>::num_lanes();

  // rect.width() needs to be equal to rect.height()
  LoopUnroll2 outer_loop(rect.width(), num_of_lanes);

  outer_loop.unroll_once([&](size_t vindex) {
    auto row_index = [](size_t index, size_t) { return index; };
    // Handle tiles on the diagonal line
    rotate_transpose_tile_3ch<false, ScalarType>(
        data_rows.at(vindex, vindex), data_rows.at(vindex, vindex), row_index);

    // Handle the top right half
    if (rect.width() > (vindex + num_of_lanes)) {
      // Indexes are running through only the top right half
      LoopUnroll2 inner_loop(vindex + num_of_lanes, rect.width(), num_of_lanes);

      inner_loop.unroll_once([&](size_t hindex) {
        // Allocate temporary memory for one tile
        ScalarType
            tmp[num_of_lanes * num_of_lanes * 3];  // NOLINT(runtime/arrays)
        Rows<ScalarType> tmp_rows{tmp, num_of_lanes * sizeof(ScalarType) * 3,
                                  3};

        // Transpose a tile from the top right area, save the result
        // into temporary memory
        rotate_transpose_tile_3ch<false, ScalarType>(
            data_rows.at(vindex, hindex), tmp_rows, row_index);
        // Transpose its mirror tile from the left bottom area, save the
        // result to its final space
        rotate_transpose_tile_3ch<false, ScalarType>(
            data_rows.at(hindex, vindex), data_rows.at(vindex, hindex),
            row_index);
        // Copy the temprory result to its final destination
        Rows<const ScalarType> const_tmp_rows{
            tmp, num_of_lanes * sizeof(ScalarType) * 3, 3};
        CopyNonOverlappingRows<ScalarType>::copy_rows(
            Rectangle{num_of_lanes, num_of_lanes}, const_tmp_rows,
            data_rows.at(hindex, vindex));
      });

      inner_loop.remaining([&](size_t hindex, size_t final_hindex) {
        // As this is the unroll_once path of the outer_loop there is
        // num_of_lanes worth of data in the vertical direction
        for (size_t i = vindex; i < vindex + num_of_lanes; ++i) {
          disable_loop_vectorization();
          for (size_t j = hindex; j < final_hindex; ++j) {
            disable_loop_vectorization();
            ScalarType *a = &data_rows.at(i, j)[0];
            ScalarType *b = &data_rows.at(j, i)[0];
            std::swap(a[0], b[0]);
            std::swap(a[1], b[1]);
            std::swap(a[2], b[2]);
          }
        }
      });
    }
  });

  outer_loop.remaining([&](size_t vindex, size_t final_vindex) {
    for (size_t i = vindex; i < final_vindex; ++i) {
      disable_loop_vectorization();
      // Only the top right half pixels need to be indexed
      for (size_t j = i + 1; j < final_vindex; ++j) {
        disable_loop_vectorization();
        ScalarType *a = &data_rows.at(i, j)[0];
        ScalarType *b = &data_rows.at(j, i)[0];
        std::swap(a[0], b[0]);
        std::swap(a[1], b[1]);
        std::swap(a[2], b[2]);
      }
    }
  });
  return KLEIDICV_OK;
}

template <typename T, bool k3Channels = false>
static kleidicv_error_t transpose(const void *src_void, size_t src_stride,
                                  void *dst_void, size_t dst_stride,
                                  size_t src_width, size_t src_height) {
  MAKE_POINTER_CHECK_ALIGNMENT(const T, src, src_void);
  MAKE_POINTER_CHECK_ALIGNMENT(T, dst, dst_void);
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, src_width);
  CHECK_IMAGE_SIZE(src_width, src_height);

  Rectangle rect{src_width, src_height};
  Rows<T> dst_rows{dst, dst_stride, k3Channels ? 3 : 1};

  if (src == dst) {
    if (src_width != src_height) {
      // Inplace transpose only implemented if width and height are the same
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
    }
    if constexpr (k3Channels) {
      return transpose_in_place_3ch(rect, dst_rows);
    } else {
      return transpose_in_place(rect, dst_rows);
    }
  }
  Rows<const T> src_rows{src, src_stride, k3Channels ? 3 : 1};
  return transpose_to_dst<T, k3Channels>(rect, src_rows, dst_rows);
}

KLEIDICV_TARGET_FN_ATTRS
kleidicv_error_t transpose(const void *src, size_t src_stride, void *dst,
                           size_t dst_stride, size_t src_width,
                           size_t src_height, size_t pixel_size) {
  switch (pixel_size) {
    case sizeof(uint8_t):
      return transpose<uint8_t>(src, src_stride, dst, dst_stride, src_width,
                                src_height);
    case sizeof(uint16_t):
      return transpose<uint16_t>(src, src_stride, dst, dst_stride, src_width,
                                 src_height);
    case sizeof(uint32_t):
      return transpose<uint32_t>(src, src_stride, dst, dst_stride, src_width,
                                 src_height);
    case sizeof(uint64_t):
      return transpose<uint64_t>(src, src_stride, dst, dst_stride, src_width,
                                 src_height);
    case sizeof(uint8_t) * 3:
      return transpose<uint8_t, true>(src, src_stride, dst, dst_stride,
                                      src_width, src_height);
    case sizeof(uint16_t) * 3:
      return transpose<uint16_t, true>(src, src_stride, dst, dst_stride,
                                       src_width, src_height);
    default:
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }
}

}  // namespace kleidicv::neon
