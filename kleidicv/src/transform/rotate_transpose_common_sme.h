// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_TRANSPOSE_SME_COMMON_H
#define KLEIDICV_TRANSPOSE_SME_COMMON_H

#include <arm_sme.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstddef>

#include "kleidicv/config.h"

// Transpose, Rotate-90 and Rotate+90 are done similarly:
// - load rows (horizontally)
// - store vertical columns into horizontal output rows
//
// Example source:
//              0  1  2
//             10 11 12
//             20 21 22
//
// The difference is a flipping somewhere:
// - transpose: no flipping, load rows from 0 to height-1, store columns from 0
// to width-1:
//              0 10 20
//              1 11 21
//              2 12 22
// - rotate clockwise: load rows in reverse order, from height-1 to 0:
//             20 10  0
//             21 11  1
//             22 12  2
// - rotate clockwise: store columns in reverse order, from width-1 to 0:
//              2 12 22
//              1 11 21
//              0 10 20

namespace kleidicv::sme {

template <size_t kPixelSize>
inline constexpr size_t kChannelCount =
    (kPixelSize == 3 || kPixelSize == 6) ? 3 : 1;

template <size_t kPixelSize>
inline constexpr size_t kElementSize = kPixelSize / kChannelCount<kPixelSize>;

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

template <size_t kElementSize, uint64_t tile>
auto read_za_col(uint32_t slice,
                 svbool_t pg) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
  if constexpr (kElementSize == 1) {
    return svread_ver_za8_u8_m(svdup_n_u8(0), pg, tile, slice);
  } else if constexpr (kElementSize == 2) {
    return svread_ver_za16_u16_m(svdup_n_u16(0), pg, tile, slice);
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
  } else {
    assert(kPixelSize == 8);
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
  }
  assert(kPixelSize == 8);
  return svwhilelt_b64_u64(i, len);
}

template <size_t kPixelSize, bool kReverseLoadRows = false,
          bool kReverseStoreCols = false>
void transform_tile_3ch(const uint8_t *src, size_t src_stride, svbool_t pgsrc,
                        size_t width, uint8_t *dst, size_t dst_stride,
                        svbool_t pgdst,
                        size_t height) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING;

template <size_t kPixelSize, bool kReverseLoadRows = false,
          bool kReverseStoreCols = false>
void transform_tile_1ch(const uint8_t *src, size_t src_stride, svbool_t pgsrc,
                        size_t width, uint8_t *dst, size_t dst_stride,
                        svbool_t pgdst,
                        size_t height) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING;

// To transform a 3-channel tile, a 3x1-tile big tile is loaded deinterleaved,
// and the channels are transformed one by one. In this way, the full tile is
// utilized, and the loads and stores are organized to minimize the memory
// operations. Finally, the three channels are stored interleaved.
template <size_t kPixelSize, bool kReverseLoadRows, bool kReverseStoreCols>
void transform_tile_3ch(const uint8_t *src, size_t src_stride, svbool_t pgsrc,
                        size_t width, uint8_t *dst, size_t dst_stride,
                        svbool_t pgdst,
                        size_t height) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
  static_assert(kChannelCount<kPixelSize> == 3);
  uint8_t tmp_r[256 * 256];  // NOLINT(runtime/arrays)
  uint8_t tmp_g[256 * 256];  // NOLINT(runtime/arrays)
  uint8_t tmp_b[256 * 256];  // NOLINT(runtime/arrays)
  const size_t tmp_stride = svcntb();

  const uint8_t *src_row = src;
  if constexpr (kReverseLoadRows) {
    src_row += src_stride * (height - 1);
  }
  uint8_t *tmp_g_row = tmp_g;
  uint8_t *tmp_b_row = tmp_b;
  for (size_t row = 0; row < height; ++row) {
    if constexpr (kPixelSize == 3) {
      svuint8x3_t rgb = svld3_u8(pgsrc, src_row);
      svwrite_hor_za8_m(0, row, pgsrc, svget3(rgb, 0));
      svst1_u8(pgsrc, tmp_g_row, svget3(rgb, 1));
      svst1_u8(pgsrc, tmp_b_row, svget3(rgb, 2));
    } else {
      svuint16x3_t rgb =
          svld3_u16(pgsrc, reinterpret_cast<const uint16_t *>(src_row));
      svwrite_hor_za16_m(0, row, pgsrc, svget3(rgb, 0));
      svst1_u16(pgsrc, reinterpret_cast<uint16_t *>(tmp_g_row), svget3(rgb, 1));
      svst1_u16(pgsrc, reinterpret_cast<uint16_t *>(tmp_b_row), svget3(rgb, 2));
    }
    tmp_g_row += tmp_stride;
    tmp_b_row += tmp_stride;
    if constexpr (kReverseLoadRows) {
      src_row -= src_stride;
    } else {
      src_row += src_stride;
    }
  }

  uint8_t *tmp_r_row = tmp_r;
  for (size_t i = 0; i < width; ++i) {
    const size_t za_col = kReverseStoreCols ? (width - i - 1) : i;
    store_za_col<kElementSize<kPixelSize>, 0>(za_col, pgdst, tmp_r_row);
    tmp_r_row += tmp_stride;
  }

  transform_tile_1ch<kElementSize<kPixelSize>, false, kReverseStoreCols>(
      tmp_g, tmp_stride, pgsrc, width, tmp_g, tmp_stride, pgdst, height);

  const uint8_t *tmp_b_row_in = tmp_b;
  for (size_t row = 0; row < height; ++row) {
    load_za_row<kElementSize<kPixelSize>, 0>(row, pgsrc, tmp_b_row_in);
    tmp_b_row_in += tmp_stride;
  }

  const uint8_t *tmp_r_row_in = tmp_r;
  const uint8_t *tmp_g_row_in = tmp_g;
  uint8_t *dst_row = dst;
  for (size_t i = 0; i < width; ++i) {
    const size_t za_col = kReverseStoreCols ? (width - i - 1) : i;
    if constexpr (kPixelSize == 3) {
      svuint8_t red = svld1_u8(pgdst, tmp_r_row_in);
      svuint8_t green = svld1_u8(pgdst, tmp_g_row_in);
      svuint8_t blue = read_za_col<1, 0>(za_col, pgdst);
      svst3_u8(pgdst, dst_row, svcreate3(red, green, blue));
    } else {
      svuint16_t red =
          svld1_u16(pgdst, reinterpret_cast<const uint16_t *>(tmp_r_row_in));
      svuint16_t green =
          svld1_u16(pgdst, reinterpret_cast<const uint16_t *>(tmp_g_row_in));
      svuint16_t blue = read_za_col<2, 0>(za_col, pgdst);
      svst3_u16(pgdst, reinterpret_cast<uint16_t *>(dst_row),
                svcreate3(red, green, blue));
    }
    tmp_r_row_in += tmp_stride;
    tmp_g_row_in += tmp_stride;
    dst_row += dst_stride;
  }
}

template <size_t kPixelSize, bool kReverseLoadRows, bool kReverseStoreCols>
void transform_tile_1ch(const uint8_t *src, size_t src_stride, svbool_t pgsrc,
                        size_t width, uint8_t *dst, size_t dst_stride,
                        svbool_t pgdst,
                        size_t height) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
  static_assert(kChannelCount<kPixelSize> == 1);
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
}

template <size_t kPixelSize, bool kReverseLoadRows = false,
          bool kReverseStoreCols = false>
void transform_tile(const uint8_t *src, size_t src_stride, svbool_t pgsrc,
                    size_t width, uint8_t *dst, size_t dst_stride,
                    svbool_t pgdst,
                    size_t height) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
  if constexpr (kChannelCount<kPixelSize> == 3) {
    transform_tile_3ch<kPixelSize, kReverseLoadRows, kReverseStoreCols>(
        src, src_stride, pgsrc, width, dst, dst_stride, pgdst, height);
  } else {
    transform_tile_1ch<kPixelSize, kReverseLoadRows, kReverseStoreCols>(
        src, src_stride, pgsrc, width, dst, dst_stride, pgdst, height);
  }
}

template <size_t kPixelSize>
void copy_tile(const uint8_t *src, size_t src_stride, svbool_t pgsrc,
               uint8_t *dst, size_t dst_stride,
               size_t height) KLEIDICV_STREAMING {
  for (size_t row = 0; row < height; ++row) {
    if constexpr (kChannelCount<kPixelSize> == 3) {
      if constexpr (kPixelSize == 3) {
        svst3_u8(pgsrc, dst, svld3_u8(pgsrc, src));
      } else {
        svst3_u16(pgsrc, reinterpret_cast<uint16_t *>(dst),
                  svld3_u16(pgsrc, reinterpret_cast<const uint16_t *>(src)));
      }
    } else if constexpr (kPixelSize == 1) {
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
  const size_t kBlkSize = svcntb() / kElementSize<kPixelSize>;
  size_t col = 0;
  for (; col < width; col += kBlkSize) {
    svbool_t pgsrc = whilelt<kElementSize<kPixelSize>>(col, width);
    const size_t kBlkWidth = std::min(kBlkSize, width - col);
    for (size_t row = 0; row < height; row += kBlkSize) {
      const uint8_t *psrc = src + row * src_stride + col * kPixelSize;
      svbool_t pgdst = whilelt<kElementSize<kPixelSize>>(row, height);
      const size_t kBlkHeight = std::min(kBlkSize, height - row);
      size_t dst_row = kReverseStoreCols ? width - col - kBlkWidth : col;
      size_t dst_col = kReverseLoadRows ? height - row - kBlkHeight : row;
      uint8_t *pdst = dst + dst_row * dst_stride + dst_col * kPixelSize;
      transform_tile<kPixelSize, kReverseLoadRows, kReverseStoreCols>(
          psrc, src_stride, pgsrc, kBlkWidth, pdst, dst_stride, pgdst,
          kBlkHeight);
    }
  }
  return KLEIDICV_OK;
}

template <size_t kPixelSize>
KLEIDICV_NEW_ZA kleidicv_error_t transpose_in_place(
    uint8_t *img, size_t img_stride, size_t width) KLEIDICV_STREAMING {
  const size_t kBlkSize = svcntb() / kElementSize<kPixelSize>;
  // Allocate temporary memory for one tile. Maximum length is 2048 bits =
  // 256 bytes.
  uint8_t tmp[256 * 256 * kChannelCount<kPixelSize>];  // NOLINT(runtime/arrays)
  const size_t tmp_stride = svcntb() * kChannelCount<kPixelSize>;

  for (size_t vindex = 0; vindex < width; vindex += kBlkSize) {
    // Handle tiles on the diagonal line
    uint8_t *ptile = img + vindex * kPixelSize + vindex * img_stride;
    svbool_t pg_v = whilelt<kElementSize<kPixelSize>>(vindex, width);
    const size_t size_v = std::min(kBlkSize, width - vindex);
    transform_tile<kPixelSize>(ptile, img_stride, pg_v, size_v, ptile,
                               img_stride, pg_v, size_v);

    // Transpose tiles from/to the right/bottom of diagonal
    for (size_t hindex = vindex + kBlkSize; hindex < width;
         hindex += kBlkSize) {
      // Going downwards
      uint8_t *tile1 = img + vindex * kPixelSize + hindex * img_stride;

      // Going right
      svbool_t pg_h = whilelt<kElementSize<kPixelSize>>(hindex, width);
      const size_t size_h = std::min(kBlkSize, width - hindex);
      uint8_t *tile2 = img + hindex * kPixelSize + vindex * img_stride;

      // Transpose a tile from the left bottom area, save the result
      // into temporary memory
      transform_tile<kPixelSize>(tile1, img_stride, pg_v, size_v, tmp,
                                 tmp_stride, pg_h, size_h);
      // Transpose its mirror tile from the top right area, save the
      // result to its final space
      transform_tile<kPixelSize>(tile2, img_stride, pg_h, size_h, tile1,
                                 img_stride, pg_v, size_v);
      // Copy the temporary result to its final destination
      copy_tile<kPixelSize>(tmp, tmp_stride, pg_h, tile2, img_stride, size_v);
    }
  }
  return KLEIDICV_OK;
}

// In-place operation is only supported for transpose.
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
