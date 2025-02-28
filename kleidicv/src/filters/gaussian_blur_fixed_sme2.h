// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef GAUSSIAN_BLUR_SME_H
#define GAUSSIAN_BLUR_SME_H

#include <arm_sme.h>
#include <arm_sve.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>

#include "gaussian_blur_sc.h"
#include "kleidicv/config.h"
#include "kleidicv/ctypes.h"
#include "kleidicv/filters/matmul.h"
#include "kleidicv/filters/matmul_filter_checks.h"
#include "kleidicv/filters/sigma.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"
#include "kleidicv/workspace/border.h"
#include "kleidicv/workspace/border_types.h"
#include "kleidicv/workspace/matmul.h"

namespace KLEIDICV_TARGET_NAMESPACE {

template <size_t KernelSize>
class GaussianBlurMatmul {
 public:
  using SourceType = uint8_t;
  using BufferType = uint8_t;
  using DestinationType = uint8_t;
  using BorderInfoType = BorderInfo<KernelSize>;
  using BorderType = FixedBorderType;

  static constexpr size_t kKernelSize = KernelSize;
  static constexpr size_t kBorderSize = KernelSize / 2;
  static constexpr size_t kKernelIterationStep = 4;

  // Class with iterations steps info for filter class
  template <size_t Channels>
  class IterationsInfo {
   public:
    IterationsInfo() KLEIDICV_STREAMING_COMPATIBLE = default;
    size_t horizontal_col_step() const KLEIDICV_STREAMING_COMPATIBLE
        KLEIDICV_PRESERVES_ZA {
      return svcntw();
    }

    size_t horizontal_row_step() const KLEIDICV_STREAMING_COMPATIBLE
        KLEIDICV_PRESERVES_ZA {
      // Data layout for 1 channel differs on transposition step
      if constexpr (Channels == 4 || Channels == 3) {
        return svcntw();
      } else if constexpr (Channels == 1) {
        return svcntb();
      } else {
        static_assert(false);
      }
    }

    size_t vertical_row_step() const KLEIDICV_STREAMING_COMPATIBLE
        KLEIDICV_PRESERVES_ZA {
      return svcntw();
    }

    size_t vertical_col_step() const KLEIDICV_STREAMING_COMPATIBLE
        KLEIDICV_PRESERVES_ZA {
      return svcntb();
    }

    ptrdiff_t kernel_block_size() const KLEIDICV_STREAMING_COMPATIBLE
        KLEIDICV_PRESERVES_ZA {
      return GaussianBlurMatmul<kKernelSize>::kernel_block_size();
    }

    size_t kernel_iteration_step() const KLEIDICV_STREAMING_COMPATIBLE
        KLEIDICV_PRESERVES_ZA {
      return kKernelIterationStep;
    }
  };

  explicit GaussianBlurMatmul(float sigma, uint8_t *kernel_buffer)
      : kernel(generate_gaussian_float_kernel<KernelSize>(sigma)) {
    build_kernel_helper_buffer(kernel_buffer);
    kernel_rows = Rows{kernel_buffer, svcntsw()};
  }

  // Apply gaussian kernel's Toeplitz matrix horizontally and get
  // [row_start, col_start]x[row_start + horizontal_row_step, col_start +
  // horizontal_col_step] block of output matrix
  template <size_t Channels, bool EnableBorderTranslation>
  KLEIDICV_LOCALLY_STREAMING void horizontal_path(
      Rows<SourceType> transposed_rows, Rows<BufferType> dst_rows,
      Rectangle rect, size_t col_start, size_t row_start,
      BorderInfoType border_info) KLEIDICV_INOUT_ZA {
    const ptrdiff_t col = static_cast<ptrdiff_t>(col_start) - kBorderSize;
    const size_t block_size = kernel_block_size();

    for (size_t kernel_block_row = 0; kernel_block_row < block_size;
         kernel_block_row += kKernelIterationStep) {
      horizontal_fma_part<EnableBorderTranslation>(
          kernel_block_row, col, transposed_rows, border_info);
    }

    horizontal_store_part<Channels>(dst_rows, rect, col_start, row_start);
    svzero_za();
  }

  // Apply gaussian kernel's Toeplitz matrix vertically and get
  // [row_start, col_start]x[row_start + horizontal_row_step, col_start +
  // horizontal_col_step] block of output matrix
  template <size_t Channels, bool EnableBorderTranslation>
  KLEIDICV_LOCALLY_STREAMING void vertical_path(
      Rows<const SourceType> src, Rows<BufferType> dst, Rectangle rect,
      Rectangle padded_rect, size_t col_start, size_t row_start,
      BorderInfoType border_info) KLEIDICV_INOUT_ZA {
    svbool_t pred_row = svwhilelt_b8(col_start, rect.width() * Channels);
    const ptrdiff_t row = static_cast<ptrdiff_t>(row_start) - kBorderSize;
    const ptrdiff_t col = static_cast<ptrdiff_t>(col_start);
    const ptrdiff_t block_size = static_cast<ptrdiff_t>(kernel_block_size());
    const ptrdiff_t padded_height =
        static_cast<ptrdiff_t>(padded_rect.height());

    for (ptrdiff_t kernel_block_row = 0; kernel_block_row < block_size &&
                                         kernel_block_row + row < padded_height;
         kernel_block_row += kKernelIterationStep) {
      vertical_fma_part<EnableBorderTranslation>(kernel_block_row, row, col,
                                                 pred_row, src, border_info);
    }

    vertical_store_part<Channels>(dst, rect, col_start, row_start);
    svzero_za();
  }

 private:
  // To avoid iterating over whole kernel's matrix, iterations are
  // done over non-zero blocks that covers diagonal strip of kernel
  // values. Since vector length that covers kernel's matrix is
  // SVLW, then amount of non-zero rows is SVLW + K - 1
  static constexpr size_t kernel_block_size() KLEIDICV_STREAMING_COMPATIBLE {
    return svcntw() + kKernelSize - 1;
  }

  // Build helper buffer in which kernel's Toeplitz matrix block pattern
  // will be stored. Since UMOPA for uint8 is being used, this
  // buffer will contain rows in the format (zipped) that UMOPA
  // expects to avoid zipping vectors on processing stage
  void build_kernel_helper_buffer(uint8_t *kernel_buffer) {
    size_t svlw = svcntsw();
    size_t kernel_buffer_size = svlw + kKernelSize - 1;
    kernel_buffer_size = (kernel_buffer_size + kKernelIterationStep - 1) /
                         kKernelIterationStep * kKernelIterationStep;

    size_t index = 0;
    for (size_t row = 0; row < kernel_buffer_size;
         row += kKernelIterationStep) {
      size_t row_start = row;
      for (size_t col = 0; col < svlw; col++) {
        for (size_t r = 0; r < kKernelIterationStep; r++) {
          size_t res_id = row_start - col + r;
          uint8_t tmp = (res_id >= kKernelSize ? 0 : kernel[res_id]);
          uint8_t v = res_id < 0 ? 0 : tmp;
          kernel_buffer[index++] = v;
        }
      }
    }
  }

  template <bool EnableBorderTranslation>
  KLEIDICV_LOCALLY_STREAMING void horizontal_fma_part(
      ptrdiff_t kernel_block_row, ptrdiff_t col,
      Rows<SourceType> transposed_rows,
      BorderInfoType border_info) KLEIDICV_INOUT_ZA {
    // Kernel data is already prepared for use in build_kernel_helper_buffer.
    // No additional zip is needed.
    svuint8_t svkern =
        svld1_u8(svptrue_b8(), &kernel_rows.at(kernel_block_row)[0]);

    const ptrdiff_t col0 = col + kernel_block_row + 0;
    const ptrdiff_t col1 = col + kernel_block_row + 1;
    const ptrdiff_t col2 = col + kernel_block_row + 2;
    const ptrdiff_t col3 = col + kernel_block_row + 3;
    svuint8_t svcol0, svcol1, svcol2, svcol3;

    if constexpr (EnableBorderTranslation) {
      svcol0 =
          svld1_u8(svptrue_b8(),
                   &transposed_rows.at(border_info.translate_index(col0))[0]);
      svcol1 =
          svld1_u8(svptrue_b8(),
                   &transposed_rows.at(border_info.translate_index(col1))[0]);
      svcol2 =
          svld1_u8(svptrue_b8(),
                   &transposed_rows.at(border_info.translate_index(col2))[0]);
      svcol3 =
          svld1_u8(svptrue_b8(),
                   &transposed_rows.at(border_info.translate_index(col3))[0]);
    } else {
      svcol0 = svld1_u8(svptrue_b8(), &transposed_rows.at(col0)[0]);
      svcol1 = svld1_u8(svptrue_b8(), &transposed_rows.at(col1)[0]);
      svcol2 = svld1_u8(svptrue_b8(), &transposed_rows.at(col2)[0]);
      svcol3 = svld1_u8(svptrue_b8(), &transposed_rows.at(col3)[0]);
    }

    svuint8x4_t svcols = svzip_u8_x4(svcreate4(svcol0, svcol1, svcol2, svcol3));

    svmopa_za32_u8_m(0, svptrue_b8(), svptrue_b8(), svget4(svcols, 0), svkern);
    svmopa_za32_u8_m(1, svptrue_b8(), svptrue_b8(), svget4(svcols, 1), svkern);
    svmopa_za32_u8_m(2, svptrue_b8(), svptrue_b8(), svget4(svcols, 2), svkern);
    svmopa_za32_u8_m(3, svptrue_b8(), svptrue_b8(), svget4(svcols, 3), svkern);
  }

  template <bool EnableBorderTranslation>
  KLEIDICV_LOCALLY_STREAMING void vertical_fma_part(
      ptrdiff_t kernel_block_row, ptrdiff_t row, ptrdiff_t col,
      svbool_t pred_row, Rows<const SourceType> src,
      BorderInfoType border_info) KLEIDICV_INOUT_ZA {
    // Kernel data is already prepared for use in build_kernel_helper_buffer.
    // No additional zip is needed.
    svuint8_t svkern =
        svld1_u8(svptrue_b8(), &kernel_rows.at(kernel_block_row)[0]);

    const ptrdiff_t row0 = row + kernel_block_row + 0;
    const ptrdiff_t row1 = row + kernel_block_row + 1;
    const ptrdiff_t row2 = row + kernel_block_row + 2;
    const ptrdiff_t row3 = row + kernel_block_row + 3;

    svuint8_t svcol0, svcol1, svcol2, svcol3;
    if constexpr (EnableBorderTranslation) {
      svcol0 =
          svld1_u8(pred_row, &src.at(border_info.translate_index(row0))[col]);
      svcol1 =
          svld1_u8(pred_row, &src.at(border_info.translate_index(row1))[col]);
      svcol2 =
          svld1_u8(pred_row, &src.at(border_info.translate_index(row2))[col]);
      svcol3 =
          svld1_u8(pred_row, &src.at(border_info.translate_index(row3))[col]);
    } else {
      svcol0 = svld1_u8(pred_row, &src.at(row0)[col]);
      svcol1 = svld1_u8(pred_row, &src.at(row1)[col]);
      svcol2 = svld1_u8(pred_row, &src.at(row2)[col]);
      svcol3 = svld1_u8(pred_row, &src.at(row3)[col]);
    }

    svuint8x4_t svcols = svzip_u8_x4(svcreate4(svcol0, svcol1, svcol2, svcol3));

    svmopa_za32_u8_m(0, svptrue_b8(), svptrue_b8(), svkern, svget4(svcols, 0));
    svmopa_za32_u8_m(1, svptrue_b8(), svptrue_b8(), svkern, svget4(svcols, 1));
    svmopa_za32_u8_m(2, svptrue_b8(), svptrue_b8(), svkern, svget4(svcols, 2));
    svmopa_za32_u8_m(3, svptrue_b8(), svptrue_b8(), svkern, svget4(svcols, 3));
  }

  // Store approach depends on channels. 1 channel images processed by SVLB
  // stripes, while 3/4 channels processed by SVLW stripes and need additional
  // interleaved stores/zips to respect elements order.
  template <size_t Channels>
  KLEIDICV_LOCALLY_STREAMING void horizontal_store_part(
      Rows<BufferType> dst, Rectangle rect, size_t col,
      size_t row_start) KLEIDICV_INOUT_ZA;

  template <>
  KLEIDICV_LOCALLY_STREAMING void horizontal_store_part<1>(
      Rows<BufferType> dst, Rectangle rect, size_t col,
      size_t row_start) KLEIDICV_INOUT_ZA {
    constexpr size_t za32_tiles_count = 4;
    horizontal_1_channel_store_all_za_tiles(
        std::make_index_sequence<za32_tiles_count>{}, dst, rect, row_start, col,
        svwhilelt_b32(col, rect.width()));
  }

  template <size_t... I>
  KLEIDICV_LOCALLY_STREAMING void horizontal_1_channel_store_all_za_tiles(
      std::index_sequence<I...>, Rows<DestinationType> dst, Rectangle rect,
      size_t row_start, size_t col, svbool_t col_pred) KLEIDICV_INOUT_ZA {
    (horizontal_1_channel_store_single_za_tile<I>(dst, rect, row_start, col,
                                                  col_pred),
     ...);
  }

  template <size_t I>
  KLEIDICV_LOCALLY_STREAMING void horizontal_1_channel_store_single_za_tile(
      Rows<DestinationType> dst, Rectangle rect, size_t row_start, size_t col,
      svbool_t col_pred) KLEIDICV_INOUT_ZA {
    for (size_t row = I * svcntw();
         row < (I + 1) * svcntw() && row_start + row < rect.height(); row++) {
      svuint32_t res = postprocess_vector(
          svread_hor_za32_u32_m(svundef_u32(), svptrue_b32(), I, row));
      auto *dst_row = &dst.at(static_cast<ptrdiff_t>(row_start + row),
                              static_cast<ptrdiff_t>(col))[0];
      svst1b_u32(col_pred, dst_row, res);
    }
  }

  template <>
  KLEIDICV_LOCALLY_STREAMING void horizontal_store_part<3>(
      Rows<BufferType> dst, Rectangle rect, size_t col,
      size_t row_start) KLEIDICV_INOUT_ZA {
    constexpr size_t channels = 3;
    svbool_t col_pred = svwhilelt_b8(channels * col, channels * rect.width());
    // Current implementation underutilizes tiles for 3 channels and
    // last quarter of the result vector will always be empty.
    svbool_t chs3_pred =
        svwhilelt_b8(static_cast<size_t>(0), channels * svcntw());
    svbool_t pred = svand_b_z(svptrue_b8(), col_pred, chs3_pred);

    // To interleave vectors in 3-channel way svtlb is used.
    // To build result vector indices should be in the following format:
    // 0, SVL/4, SVL/2, 1, SVL/4 + 1, SVL/2 + 1, ...
    // <--0th pixel-->  <----  1st pixel  ---->
    // This corresponds to the sequence (i % 3) * SVL / 4 + i / 3
    svuint8_t indices = svindex_u8(0, 1);
    // floor(2^8 / 3) + 1 == 86, used to divide by 3 without
    // division.
    svuint8_t indices_div3 =
        svmulh_u8_z(svptrue_b8(), indices, svdup_n_u8(86U));
    svuint8_t indices_mod3 =
        svsub_u8_z(svptrue_b8(), indices,
                   svmul_z(svptrue_b8(), svdup_u8(3), indices_div3));
    svuint8_t indices_final = svadd_z(
        svptrue_b8(), indices_div3,
        svrshr_n_u8_z(svptrue_b8(),
                      svmul_z(svptrue_b8(), svdup_u8(svcntb()), indices_mod3),
                      2));

    for (size_t row = 0; row < svcntw() && row_start + row < rect.height();
         row++) {
      svuint32x4_t resu =
          (svcreate4(postprocess_vector_no_clamp(svread_hor_za32_u32_m(
                         svdup_u32(0), svptrue_b32(), 0, row)),
                     postprocess_vector_no_clamp(svread_hor_za32_u32_m(
                         svdup_u32(0), svptrue_b32(), 1, row)),
                     postprocess_vector_no_clamp(svread_hor_za32_u32_m(
                         svdup_u32(0), svptrue_b32(), 2, row)),
                     postprocess_vector_no_clamp(svread_hor_za32_u32_m(
                         svdup_u32(0), svptrue_b32(), 3, row))));

      svuint8_t svres = svqcvt_u8(resu);
      svres = svtbl(svres, indices_final);
      svst1(pred,
            &dst.at(static_cast<ptrdiff_t>(row_start + row),
                    static_cast<ptrdiff_t>(col))[0],
            svres);
    }
  }

  template <>
  KLEIDICV_LOCALLY_STREAMING void horizontal_store_part<4>(
      Rows<BufferType> dst, Rectangle rect, size_t col,
      size_t row_start) KLEIDICV_INOUT_ZA {
    constexpr size_t channels = 4;
    svbool_t col_pred = svwhilelt_b8(channels * col, channels * rect.width());
    for (size_t row = 0; row < svcntw() && row_start + row < rect.height();
         row++) {
      svuint32x4_t resu =
          (svcreate4(postprocess_vector_no_clamp(svread_hor_za32_u32_m(
                         svdup_u32(0), svptrue_b32(), 0, row)),
                     postprocess_vector_no_clamp(svread_hor_za32_u32_m(
                         svdup_u32(0), svptrue_b32(), 1, row)),
                     postprocess_vector_no_clamp(svread_hor_za32_u32_m(
                         svdup_u32(0), svptrue_b32(), 2, row)),
                     postprocess_vector_no_clamp(svread_hor_za32_u32_m(
                         svdup_u32(0), svptrue_b32(), 3, row))));

      resu = svzip_u32_x4(resu);
      svst1(col_pred,
            &dst.at(static_cast<ptrdiff_t>(row_start + row),
                    static_cast<ptrdiff_t>(col))[0],
            svqcvt_u8(resu));
    }
  }

  template <size_t Channels>
  KLEIDICV_LOCALLY_STREAMING void vertical_store_part(
      Rows<DestinationType> dst, Rectangle rect, size_t col_start,
      size_t row_start) KLEIDICV_INOUT_ZA {
    size_t svl = svcntw();
    svbool_t pred_row0 =
        svwhilelt_b32(col_start + 0 * svcntw(), rect.width() * Channels);
    svbool_t pred_row1 =
        svwhilelt_b32(col_start + 1 * svcntw(), rect.width() * Channels);
    svbool_t pred_row2 =
        svwhilelt_b32(col_start + 2 * svcntw(), rect.width() * Channels);
    svbool_t pred_row3 =
        svwhilelt_b32(col_start + 3 * svcntw(), rect.width() * Channels);

    for (size_t row = 0; row < svl && (row_start + row) < rect.height();
         row++) {
      svuint32x4_t resu =
          svcreate4(postprocess_vector(svread_hor_za32_u32_m(
                        svdup_u32(0.0), svptrue_b32(), 0, row)),
                    postprocess_vector(svread_hor_za32_u32_m(
                        svdup_u32(0.0), svptrue_b32(), 1, row)),
                    postprocess_vector(svread_hor_za32_u32_m(
                        svdup_u32(0.0), svptrue_b32(), 2, row)),
                    postprocess_vector(svread_hor_za32_u32_m(
                        svdup_u32(0.0), svptrue_b32(), 3, row)));

      auto *dst_row = &dst.at(static_cast<ptrdiff_t>(
          row_start + row))[static_cast<ptrdiff_t>(col_start)];

      auto *KLEIDICV_RESTRICT dst_row0 = dst_row + 0 * svcntw();
      auto *KLEIDICV_RESTRICT dst_row1 = dst_row + 1 * svcntw();
      auto *KLEIDICV_RESTRICT dst_row2 = dst_row + 2 * svcntw();
      auto *KLEIDICV_RESTRICT dst_row3 = dst_row + 3 * svcntw();

      svst1b_u32(pred_row0, dst_row0, svget4(resu, 0));
      svst1b_u32(pred_row1, dst_row1, svget4(resu, 1));
      svst1b_u32(pred_row2, dst_row2, svget4(resu, 2));
      svst1b_u32(pred_row3, dst_row3, svget4(resu, 3));
    }
  }

  KLEIDICV_LOCALLY_STREAMING svuint32_t
  postprocess_vector_no_clamp(svuint32_t v) KLEIDICV_PRESERVES_ZA {
    return svrshr_n_u32_x(svptrue_b32(), v, 8);
  }

  KLEIDICV_LOCALLY_STREAMING svuint32_t postprocess_vector(svuint32_t v)
      KLEIDICV_PRESERVES_ZA {
    return svclamp_u32(svdup_u32(0), svdup_u32(255),
                       postprocess_vector_no_clamp(v));
  }

  std::array<uint8_t, KernelSize> kernel;
  Rows<SourceType> kernel_rows;
};

// Class to transpose image data for horizontal processing
template <size_t Channels>
class Transposer {
 public:
  // Data layout depends on channels amount.
  //
  //  1) For 1 channel, row in the output buffer will correspond to a column
  //  part
  //    of an image for rows [row_start, row_start + SVLB].
  //
  //  2) For 4 channel, row in the output buffer will correspond to a column
  //  part
  //    of an image for rows [row_start, row_start + SVLW], where first SVLW
  //    elements are 0th channel elements, second SVLW elements are
  //    corresponding 1th channel elements and so on.
  //
  //  3) For 3 channel the layout is the same as for 4 channel, however last
  //  SVLW elements are
  //    not used (consequently last tile won't be used for processing).
  template <typename SourceType>
  KLEIDICV_LOCALLY_STREAMING void transpose(Rows<const SourceType> src_rows,
                                            Rows<SourceType> transpose_buffer,
                                            Rectangle rect, size_t row_start,
                                            size_t rows) KLEIDICV_INOUT_ZA {
    const size_t svlb = svcntb();
    const size_t za_channel_padding = svlb >> 2;

    svzero_za();

    for (size_t col = 0; col < rect.width(); col += svlb) {
      svbool_t pred = svwhilelt_b8(col, rect.width());

      for (size_t row = 0; row < rows; row++) {
        auto *p = &src_rows.at(static_cast<ptrdiff_t>(row_start + row),
                               static_cast<ptrdiff_t>(col))[0];
        if constexpr (Channels == 4) {
          svuint8x4_t c = svld4(pred, p);
          svwrite_hor_za8_m(0, row + 0 * za_channel_padding, svptrue_b8(),
                            svget4(c, 0));
          svwrite_hor_za8_m(0, row + 1 * za_channel_padding, svptrue_b8(),
                            svget4(c, 1));
          svwrite_hor_za8_m(0, row + 2 * za_channel_padding, svptrue_b8(),
                            svget4(c, 2));
          svwrite_hor_za8_m(0, row + 3 * za_channel_padding, svptrue_b8(),
                            svget4(c, 3));
        } else if constexpr (Channels == 1) {
          svuint8_t c = svld1(pred, p);
          svwrite_hor_za8_m(0, row, svptrue_b8(), c);
        } else if constexpr (Channels == 3) {
          svuint8x3_t c = svld3(pred, p);
          svwrite_hor_za8_m(0, row + 0 * za_channel_padding, svptrue_b8(),
                            svget3(c, 0));
          svwrite_hor_za8_m(0, row + 1 * za_channel_padding, svptrue_b8(),
                            svget3(c, 1));
          svwrite_hor_za8_m(0, row + 2 * za_channel_padding, svptrue_b8(),
                            svget3(c, 2));
        } else {
          static_assert(false, "Unsupported amount of channels for transpose");
        }
      }

      for (size_t i = 0; i < svlb; i++) {
        svst1_ver_za8(0, i, svptrue_b8(), &transpose_buffer.at(col + i)[0]);
      }
      svzero_za();
    }
  }

 private:
};

template <size_t Channels>
static kleidicv_error_t gaussian_blur(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end,
    size_t kernel_width, size_t, float sigma_x, float,
    FixedBorderType fixed_border_type,
    MatmulSeparableFilterWorkspace *workspace) KLEIDICV_STREAMING_COMPATIBLE {
  Rectangle rect{width, height};
  Rows<const uint8_t> src_rows{src, src_stride, Channels};
  Rows<uint8_t> dst_rows{dst, dst_stride, Channels};
  uint8_t *kernel_buffer =
      reinterpret_cast<uint8_t *>(workspace->get_kernel_buffer());

  switch (kernel_width) {
    case 7: {
      using GaussianBlur = GaussianBlurMatmul<7>;
      GaussianBlur inner_filter(sigma_x, kernel_buffer);
      MatmulFilter<Channels, GaussianBlur, Transposer<Channels>> filter(
          inner_filter);
      workspace->process(rect, y_begin, y_end, src_rows, dst_rows,
                         fixed_border_type, filter);
      break;
    }
    case 15: {
      using GaussianBlur = GaussianBlurMatmul<15>;
      GaussianBlur inner_filter(sigma_x, kernel_buffer);
      MatmulFilter<Channels, GaussianBlur, Transposer<Channels>> filter(
          inner_filter);
      workspace->process(rect, y_begin, y_end, src_rows, dst_rows,
                         fixed_border_type, filter);
      break;
    }
    case 21: {
      using GaussianBlur = GaussianBlurMatmul<21>;
      GaussianBlur inner_filter(sigma_x, kernel_buffer);
      MatmulFilter<Channels, GaussianBlur, Transposer<Channels>> filter(
          inner_filter);
      workspace->process(rect, y_begin, y_end, src_rows, dst_rows,
                         fixed_border_type, filter);
      break;
    }

    default:
      assert(!"kernel size not implemented");
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  return KLEIDICV_OK;
}

template <size_t... C>
static kleidicv_error_t call_gaussian_blur(
    std::index_sequence<C...>, const uint8_t *src, size_t src_stride,
    uint8_t *dst, size_t dst_stride, size_t width, size_t height,
    size_t y_begin, size_t y_end, size_t channels, size_t kernel_width,
    size_t kernel_height, float sigma_x, float sigma_y,
    FixedBorderType fixed_border_type,
    kleidicv_filter_context_t *context) KLEIDICV_STREAMING_COMPATIBLE {
  auto *workspace = reinterpret_cast<MatmulSeparableFilterWorkspace *>(context);
  kleidicv_error_t checks_result = gaussian_blur_checks(
      src, src_stride, dst, dst_stride, width, height, channels, workspace);
  if (kernel_width != kernel_height) {
    checks_result = KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (checks_result != KLEIDICV_OK) {
    return checks_result;
  }

  // If no case triggered result = ..., then `channels` variable
  // is out of range
  kleidicv_error_t exec_result = KLEIDICV_ERROR_NOT_IMPLEMENTED;
  ((channels == C ? (exec_result = gaussian_blur<C>(
                         src, src_stride, dst, dst_stride, width, height,
                         y_begin, y_end, kernel_width, kernel_height, sigma_x,
                         sigma_y, fixed_border_type, workspace),
                     void())
                  : void()),
   ...);
  return exec_result;
}

static kleidicv_error_t gaussian_blur_fixed_stripe_u8_sme(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t y_begin, size_t y_end, size_t channels,
    size_t kernel_width, size_t kernel_height, float sigma_x, float sigma_y,
    FixedBorderType fixed_border_type,
    kleidicv_filter_context_t *context) KLEIDICV_STREAMING_COMPATIBLE {
  if (!gaussian_blur_sme2_implementation_checks(kernel_width, kernel_height,
                                                channels)) {
    return gaussian_blur_fixed_stripe_u8_sc(
        src, src_stride, dst, dst_stride, width, height, y_begin, y_end,
        channels, kernel_width, kernel_height, sigma_x, sigma_y,
        fixed_border_type, context);
  }

  constexpr std::index_sequence<1, 3, 4> supported_channels;
  return call_gaussian_blur(supported_channels, src, src_stride, dst,
                            dst_stride, width, height, y_begin, y_end, channels,
                            kernel_width, kernel_height, sigma_x, sigma_y,
                            fixed_border_type, context);
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif
