// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_TRANSPOSE_SME_COMMON_H
#define KLEIDICV_TRANSPOSE_SME_COMMON_H

#include <arm_sme.h>

#include <algorithm>
#include <climits>
#include <cstddef>

#include "kleidicv/config.h"
#include "kleidicv/ctypes.h"

// For 3channels, the indirect load is needed for interleaving:
/* if constexpr (Channels == 3) {
          svuint8x3_t c = svld3(pred, p);
          svwrite_hor_za8_m(0, row_index + 0 * za_channel_padding,
   svptrue_b8(), svget3(c, 0)); svwrite_hor_za8_m(0, row_index + 1 *
   za_channel_padding, svptrue_b8(), svget3(c, 1)); svwrite_hor_za8_m(0,
   row_index + 2 * za_channel_padding, svptrue_b8(), svget3(c, 2));
        }*/
// src:          0  1  2  3  4  5
//              10 11 12 13 14 15
//
// transpose:    0 10 20 30 40 50
//               1 11 21 31 41 51
// -- load hor, store ver
// rotate cw:    50 40 30 20 10  0
//               51 41 31 21 11  1
// -- load hor/reversed, store ver
// rotate ccw:    5 15 25 35 45 55
//                4 14 24 34 44 54
// -- load hor, store ver/reversed

// inplace: ONLY FOR TRANSPOSE NOW!!!
// rotate: 4way: load A, rotate B to A, rotate C to B, rotate D to
// C, rotate loaded to D

namespace kleidicv::sme {

template <size_t kPixelSize, uint64_t tile>
void load_za_row(uint32_t slice, svbool_t pg,
                 const void *ptr) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
  if constexpr (kPixelSize == 1) {
    svld1_hor_za8(tile, slice, pg, ptr);
  } else if constexpr (kPixelSize == 2) {
    svld1_hor_za16(tile, slice, pg, ptr);
  } else if constexpr (kPixelSize == 4) {
    svld1_hor_za32(tile, slice, pg, ptr);
  } else if constexpr (kPixelSize == 8) {
    svld1_hor_za64(tile, slice, pg, ptr);
  }
}

template <size_t kPixelSize, uint64_t tile>
void store_za_col(uint32_t slice, svbool_t pg,
                  void *ptr) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
  if constexpr (kPixelSize == 1) {
    svst1_ver_za8(tile, slice, pg, ptr);
  } else if constexpr (kPixelSize == 2) {
    svst1_ver_za16(tile, slice, pg, ptr);
  } else if constexpr (kPixelSize == 4) {
    svst1_ver_za32(tile, slice, pg, ptr);
  } else if constexpr (kPixelSize == 8) {
    svst1_ver_za64(tile, slice, pg, ptr);
  }
}

template <size_t kPixelSize>
svbool_t whilelt(uint64_t i,
                 uint64_t len) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
  if constexpr (kPixelSize == 1) {
    return svwhilelt_b8_u64(i, len);
  } else if constexpr (kPixelSize == 2) {
    return svwhilelt_b16_u64(i, len);
  } else if constexpr (kPixelSize == 4) {
    return svwhilelt_b32_u64(i, len);
  } else if constexpr (kPixelSize == 8) {
    return svwhilelt_b64_u64(i, len);
  }
}

/*
This should make it faster, but does not really make a difference
TODO have a look at it later again, when all features are in.

template <size_t kPixelSize>
void transpose_full_tile(const uint8_t *src, size_t src_stride, uint8_t *dst,
                         size_t dst_stride,
                         size_t kBlkSize) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
  // Unroll 128bits, Vector Length is always divisible with 128
  const int64_t kStep = 128 / CHAR_BIT / kPixelSize;
  for (size_t row = 0; row < kBlkSize; row += kStep) {
    KLEIDICV_FORCE_LOOP_UNROLL for (int64_t i = 0; i < kStep; ++i) {
      load_za_row<kPixelSize, 0>(row + i, svptrue_b8(), src);
      src += src_stride;
    }
  }

  for (size_t row = 0; row < kBlkSize; row += kStep) {
    KLEIDICV_FORCE_LOOP_UNROLL for (int64_t i = 0; i < kStep; ++i) {
      store_za_col<kPixelSize, 0>(row + i, svptrue_b8(), dst);
      dst += dst_stride;
    }
  }
};*/

template <size_t kPixelSize>
void transpose_partial_tile(const uint8_t *src, size_t src_stride,
                            svbool_t pgsrc, size_t width, uint8_t *dst,
                            size_t dst_stride, svbool_t pgdst, size_t height)
    KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
  for (size_t row = 0; row < height; ++row) {
    load_za_row<kPixelSize, 0>(row, pgsrc, src);
    src += src_stride;
  }

  for (size_t i = 0; i < width; i++) {
    store_za_col<kPixelSize, 0>(i, pgdst, dst);
    dst += dst_stride;
  }
};

template <size_t kPixelSize>
KLEIDICV_NEW_ZA kleidicv_error_t transpose(const uint8_t *src,
                                           size_t src_stride, uint8_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height) KLEIDICV_STREAMING {
  // For 1,2,4,8 size pixels
  const size_t kBlkSize = svcntb() / kPixelSize;
  size_t col = 0;
  /* This should make it faster, but does not really make a difference
     TODO have a look at it later again, when all features are in.
  for (; col + kBlkSize <= width; col += kBlkSize) {
      const uint8_t *psrc = src + col * kPixelSize;
      uint8_t *pdst = dst + col * dst_stride;
      size_t row = 0;
      for (; row + kBlkSize <= height; row += kBlkSize) {
        transpose_full_tile<kPixelSize>(psrc, src_stride, pdst, dst_stride,
                                        kBlkSize);
        psrc += kBlkSize * src_stride;
        pdst += kBlkSize * kPixelSize;
      }
      for (; row < height; row += kBlkSize) {
        svbool_t pgdst = whilelt<kPixelSize>(row, height);
        const size_t kBlkHeight = std::min(kBlkSize, height - row);
        transpose_partial_tile<kPixelSize>(psrc, src_stride, svptrue_b8(),
                                           kBlkSize, pdst, dst_stride, pgdst,
                                           kBlkHeight);
        psrc += kBlkSize * src_stride;
        pdst += kBlkSize * kPixelSize;
      }
    }*/

  for (; col < width; col += kBlkSize) {
    const uint8_t *psrc = src + col * kPixelSize;
    uint8_t *pdst = dst + col * dst_stride;
    svbool_t pgsrc = whilelt<kPixelSize>(col, width);
    const size_t kBlkWidth = std::min(kBlkSize, width - col);
    for (size_t row = 0; row < height; row += kBlkSize) {
      svbool_t pgdst = whilelt<kPixelSize>(row, height);
      const size_t kBlkHeight = std::min(kBlkSize, height - row);
      transpose_partial_tile<kPixelSize>(psrc, src_stride, pgsrc, kBlkWidth,
                                         pdst, dst_stride, pgdst, kBlkHeight);
      psrc += kBlkSize * src_stride;
      pdst += kBlkSize * kPixelSize;
    }
  }
  svzero_za();
  return KLEIDICV_OK;
}
/*
template <size_t kPixelSize>
KLEIDICV_NEW_ZA kleidicv_error_t transpose_in_place(
    Rectangle rect, Rows<ScalarType> data_rows) KLEIDICV_STREAMING {
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
}*/

}  // namespace kleidicv::sme

#endif  // KLEIDICV_TRANSPOSE_SME_COMMON_H