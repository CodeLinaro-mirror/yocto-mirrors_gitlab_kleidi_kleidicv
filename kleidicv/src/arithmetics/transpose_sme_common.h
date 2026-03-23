// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_TRANSPOSE_SME_COMMON_H
#define KLEIDICV_TRANSPOSE_SME_COMMON_H

#include <arm_sme.h>

#include <algorithm>
#include <climits>
#include <cstddef>

namespace kleidicv::sme {

template <size_t kPixelSize>
KLEIDICV_NEW_ZA kleidicv_error_t transpose(const void *src, size_t src_stride,
                                           void *dst, size_t dst_stride,
                                           size_t width,
                                           size_t height) KLEIDICV_STREAMING {
  // For 1,2,4,8 size pixels
  const size_t kBlkSize = svcntb() / kPixelSize;
  auto process_full_block =
      [&](const uint8_t *&psrc, size_t row,
          size_t col) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
        svzero_za();
        // Unroll 128bits, Vector Length is always divisible with 128
        const int64_t kStep = 128 / CHAR_BIT / kPixelSize;
        for (size_t inner_row = 0; inner_row < kBlkSize; inner_row += kStep) {
          KLEIDICV_FORCE_LOOP_UNROLL for (int64_t i = 0; i < kStep; ++i) {
            svld1_hor_vnum_za8(0, inner_row + i, svptrue_b8(), psrc, i);
          }
          psrc += src_stride;
        }

        uint8_t *pdst =
            reinterpret_cast<uint8_t *>(dst) + col * dst_stride + row;
        for (size_t i = 0; i < kBlkSize; i++) {
          if constexpr (kPixelSize == 1) {
            svst1_ver_za8(0, i, svptrue_b8(), pdst);
            pdst += dst_stride;
          }
        }
      };

  auto process_block = [&](const uint8_t *&psrc, svbool_t pgsrc, svbool_t pgdst,
                           size_t blk_width, size_t blk_height, size_t row,
                           size_t col) KLEIDICV_INOUT_ZA KLEIDICV_STREAMING {
    svzero_za();

    for (size_t inner_row = 0; inner_row < blk_height; ++inner_row) {
      svld1_hor_za8(0, inner_row, pgsrc, psrc);
      psrc += src_stride;
    }

    uint8_t *pdst = reinterpret_cast<uint8_t *>(dst) + col * dst_stride + row;
    for (size_t i = 0; i < blk_width; i++) {
      if constexpr (kPixelSize == 1) {
        svst1_ver_za8(0, i, pgdst, pdst);
        pdst += dst_stride;
      }
    }
  };

  for (size_t col = 0; col + kBlkSize <= width; col += kBlkSize) {
    const uint8_t *psrc = reinterpret_cast<const uint8_t *>(src) + col;
    for (size_t row = 0; row + kBlkSize < height; row += kBlkSize) {
      process_full_block(psrc, row, col);
    }
    for (size_t row = 0; row < height; row += kBlkSize) {
      svbool_t pgdst = svwhilelt_b8_u64(row, height);
      const size_t kBlkHeight = std::min(kBlkSize, height - row);
      process_block(psrc, svptrue_b8(), pgdst, kBlkSize, kBlkHeight, row, col);
    }
  }

  for (size_t col = 0; col < width; col += kBlkSize) {
    const uint8_t *psrc = reinterpret_cast<const uint8_t *>(src) + col;
    svbool_t pgsrc = svwhilelt_b8_u64(col, width);
    const size_t kBlkWidth = std::min(kBlkSize, width - col);
    for (size_t row = 0; row < height; row += kBlkSize) {
      svbool_t pgdst = svwhilelt_b8_u64(row, height);
      const size_t kBlkHeight = std::min(kBlkSize, height - row);
      process_block(psrc, pgsrc, pgdst, kBlkWidth, kBlkHeight, row, col);
    }
  }
  svzero_za();
  return KLEIDICV_OK;
}

}  // namespace kleidicv::sme

#endif  // KLEIDICV_TRANSPOSE_SME_COMMON_H