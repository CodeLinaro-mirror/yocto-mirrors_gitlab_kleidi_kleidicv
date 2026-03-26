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
// [src_row, src_col] --> [src_col, height - 1 - src_row]
// rotate ccw:    5 15 25 35 45 55
//                4 14 24 34 44 54
// -- load hor, store ver/reversed
// [src_row, src_col] --> [width - 1 - src_col, src_row]

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

template <size_t kPixelSize, bool kReverseLoadRows = false,
          bool kReverseStoreCols = false>
void transpose_partial_tile(const uint8_t *src, size_t src_stride,
                            svbool_t pgsrc, size_t width, uint8_t *dst,
                            size_t dst_stride, svbool_t pgdst, size_t height)
    KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
  if (kReverseLoadRows) {
    src = src + src_stride * (height - 1);
  }
  for (size_t row = 0; row < height; ++row) {
    load_za_row<kPixelSize, 0>(row, pgsrc, src);
    if constexpr (kReverseLoadRows) {
      src -= src_stride;
    } else {
      src += src_stride;
    }
  }

  for (size_t i = 0; i < width; i++) {
    const size_t src_col = kReverseStoreCols ? (width - i - 1) : i;
    store_za_col<kPixelSize, 0>(src_col, pgdst, dst);
    dst += dst_stride;
  }
};

template <size_t kPixelSize>
void copy_tile(const uint8_t *src, size_t src_stride, svbool_t pgsrc,
               uint8_t *dst, size_t dst_stride,
               size_t height) KLEIDICV_STREAMING {
  for (size_t row = 0; row < height; ++row) {
    if constexpr (kPixelSize == 1) {
      svst1(pgsrc, dst, svld1_u8(pgsrc, src));
    } else if constexpr (kPixelSize == 2) {
      svst1(pgsrc, reinterpret_cast<uint16_t *>(dst),
            svld1_u16(pgsrc, reinterpret_cast<const uint16_t *>(src)));
    } else if constexpr (kPixelSize == 4) {
      svst1(pgsrc, reinterpret_cast<uint32_t *>(dst),
            svld1_u32(pgsrc, reinterpret_cast<const uint32_t *>(src)));
    } else if constexpr (kPixelSize == 8) {
      svst1(pgsrc, reinterpret_cast<uint64_t *>(dst),
            svld1_u64(pgsrc, reinterpret_cast<const uint64_t *>(src)));
    }
    src += src_stride;
    dst += dst_stride;
  }
}

template <size_t kPixelSize, bool kReverseLoadRows = false,
          bool kReverseStoreCols = false>
KLEIDICV_NEW_ZA kleidicv_error_t rotate_transpose_to_dst(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height) KLEIDICV_STREAMING {
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
    svbool_t pgsrc = whilelt<kPixelSize>(col, width);
    const size_t kBlkWidth = std::min(kBlkSize, width - col);
    for (size_t row = 0; row < height; row += kBlkSize) {
      const uint8_t *psrc = src + row * src_stride + col * kPixelSize;
      svbool_t pgdst = whilelt<kPixelSize>(row, height);
      const size_t kBlkHeight = std::min(kBlkSize, height - row);
      size_t dst_row = kReverseStoreCols ? width - col - kBlkWidth : col;
      size_t dst_col = kReverseLoadRows ? height - row - kBlkHeight : row;
      uint8_t *pdst = dst + dst_row * dst_stride + dst_col * kPixelSize;
      transpose_partial_tile<kPixelSize, kReverseLoadRows, kReverseStoreCols>(
          psrc, src_stride, pgsrc, kBlkWidth, pdst, dst_stride, pgdst,
          kBlkHeight);
    }
  }
  svzero_za();
  return KLEIDICV_OK;
}

template <size_t kPixelSize>
KLEIDICV_NEW_ZA kleidicv_error_t transpose_in_place(
    uint8_t *img, size_t img_stride, size_t width) KLEIDICV_STREAMING {
  const size_t kBlkSize = svcntb() / kPixelSize;
  // Allocate temporary memory for one tile - maximum length is 2048 bits
  uint8_t tmp[256 * 256];  // NOLINT(runtime/arrays)
  uint8_t tmp_stride = svcntb();

  for (size_t vindex = 0; vindex < width; vindex += kBlkSize) {
    // Handle tiles on the diagonal line
    uint8_t *ptile = img + vindex * kPixelSize + vindex * img_stride;
    svbool_t pg_v = whilelt<kPixelSize>(vindex, width);
    const size_t size_v = std::min(kBlkSize, width - vindex);
    transpose_partial_tile<kPixelSize>(ptile, img_stride, pg_v, size_v, ptile,
                                       img_stride, pg_v, size_v);

    // Transpose tiles from/to the right/bottom of diagonal
    for (size_t hindex = vindex + kBlkSize; hindex < width;
         hindex += kBlkSize) {
      // Going downwards
      uint8_t *tile1 = img + vindex * kPixelSize + hindex * img_stride;

      // Going right
      svbool_t pg_h = whilelt<kPixelSize>(hindex, width);
      const size_t size_h = std::min(kBlkSize, width - hindex);
      uint8_t *tile2 = img + hindex * kPixelSize + vindex * img_stride;

      // Transpose a tile from the left bottom area, save the result
      // into temporary memory
      transpose_partial_tile<kPixelSize>(tile1, img_stride, pg_v, size_v, tmp,
                                         tmp_stride, pg_h, size_h);
      // Transpose its mirror tile from the top right area, save the
      // result to its final space
      transpose_partial_tile<kPixelSize>(tile2, img_stride, pg_h, size_h, tile1,
                                         img_stride, pg_v, size_v);
      // Copy the temporary result to its final destination
      copy_tile<kPixelSize>(tmp, tmp_stride, pg_h, tile2, img_stride, size_v);
    }
  }
  return KLEIDICV_OK;
}

template <size_t kPixelSize>
kleidicv_error_t transpose(const uint8_t *src, size_t src_stride, uint8_t *dst,
                           size_t dst_stride, size_t width,
                           size_t height) KLEIDICV_STREAMING {
  if (src == dst) {
    return transpose_in_place<kPixelSize>(dst, dst_stride, width);
  }
  return rotate_transpose_to_dst<kPixelSize, false, false>(
      src, src_stride, dst, dst_stride, width, height);
}

template <size_t kPixelSize>
kleidicv_error_t rotate_cw(const uint8_t *src, size_t src_stride, uint8_t *dst,
                           size_t dst_stride, size_t width,
                           size_t height) KLEIDICV_STREAMING {
  return rotate_transpose_to_dst<kPixelSize, true, false>(
      src, src_stride, dst, dst_stride, width, height);
}

template <size_t kPixelSize>
kleidicv_error_t rotate_ccw(const uint8_t *src, size_t src_stride, uint8_t *dst,
                            size_t dst_stride, size_t width,
                            size_t height) KLEIDICV_STREAMING {
  return rotate_transpose_to_dst<kPixelSize, false, true>(
      src, src_stride, dst, dst_stride, width, height);
}

}  // namespace kleidicv::sme

#endif  // KLEIDICV_TRANSPOSE_SME_COMMON_H
