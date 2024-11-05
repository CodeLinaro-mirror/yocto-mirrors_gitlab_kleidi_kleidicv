// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_OPENCV_OPTICAL_FLOW_SC_H
#define KLEIDICV_OPENCV_OPTICAL_FLOW_SC_H

#include <cstddef>

#include "kleidicv/sve2.h"
#include "optical_flow_common.h"

namespace KLEIDICV_OPENCV_TARGET_NAMESPACE {

struct OpticalFlow {
  template <uint64_t shift>
  static inline svint16_t lerp(svint16_t tl, svint16_t tr, svint16_t bl,
                               svint16_t br, int16_t coeff_tl, int16_t coeff_tr,
                               int16_t coeff_bl,
                               int16_t coeff_br) KLEIDICV_STREAMING_COMPATIBLE {
    svint32_t b = svmullb_n_s32(tl, coeff_tl);
    svint32_t t = svmullt_n_s32(tl, coeff_tl);

    b = svmlalb_n_s32(b, tr, coeff_tr);
    t = svmlalt_n_s32(t, tr, coeff_tr);

    b = svmlalb_n_s32(b, bl, coeff_bl);
    t = svmlalt_n_s32(t, bl, coeff_bl);

    b = svmlalb_n_s32(b, br, coeff_br);
    t = svmlalt_n_s32(t, br, coeff_br);

    svint16_t result = svqrshrnb_n_s32(b, shift);
    result = svqrshrnt_n_s32(result, t, shift);
    return result;
  }

  static inline void get_window(
      int16_t *window, const uint8_t *prev_data, ptrdiff_t prev_data_stride,
      int channels, int window_corner_x, int window_corner_y, int window_width,
      int window_height, int16_t coeff_tl, int16_t coeff_tr, int16_t coeff_bl,
      int16_t coeff_br) KLEIDICV_STREAMING_COMPATIBLE {
    for (int y = 0; y < window_height; y++) {
      const uint8_t *const prev_row0 =
          prev_data + (y + window_corner_y) * prev_data_stride +
          static_cast<ptrdiff_t>(window_corner_x) * channels;
      const uint8_t *const prev_row1 = prev_row0 + prev_data_stride;

      int16_t *const window_row =
          window + static_cast<ptrdiff_t>(y) * window_width;

      for (size_t x = 0; x < static_cast<size_t>(window_width) * channels;
           x += svcnth()) {
        svbool_t pg16 =
            svwhilelt_b16(x, static_cast<size_t>(window_width) * channels);

        svint16_t prev_tl = svld1ub_s16(pg16, prev_row0 + x);
        svint16_t prev_tr = svld1ub_s16(pg16, prev_row0 + x + channels);
        svint16_t prev_bl = svld1ub_s16(pg16, prev_row1 + x);
        svint16_t prev_br = svld1ub_s16(pg16, prev_row1 + x + channels);

        svint16_t prev = lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS - 5>(
            prev_tl, prev_tr, prev_bl, prev_br, coeff_tl, coeff_tr, coeff_bl,
            coeff_br);
        svst1_s16(pg16, window_row + x, prev);
      }
    }
  }

  static inline void get_scharr(
      int16_t *scharr_window, const int16_t *scharr_data,
      ptrdiff_t scharr_stride_elements, int channels, int window_corner_x,
      int window_corner_y, int window_width, int window_height,
      int16_t coeff_tl, int16_t coeff_tr, int16_t coeff_bl, int16_t coeff_br,
      float &sum_scharr_xx, float &sum_scharr_xy,
      float &sum_scharr_yy) KLEIDICV_STREAMING_COMPATIBLE {
    svfloat32_t sum_scharr_xx_v = svdup_n_f32(0),
                sum_scharr_xy_v = svdup_n_f32(0),
                sum_scharr_yy_v = svdup_n_f32(0);
    for (int y = 0; y < window_height; y++) {
      const int16_t *const scharr_row0 =
          scharr_data + (y + window_corner_y) * scharr_stride_elements +
          window_corner_x * 2L * channels;
      const int16_t *const scharr_row1 = scharr_row0 + scharr_stride_elements;

      int16_t *const scharr_window_row = scharr_window + y * 2L * window_width;

      for (size_t x = 0; x < static_cast<size_t>(window_width) * channels;
           x += svcnth()) {
        svbool_t pg16 =
            svwhilelt_b16(x, static_cast<size_t>(window_width) * channels);
        svbool_t pg32 =
            svwhilelt_b32(x, static_cast<size_t>(window_width) * channels);

        svint16x2_t scharr_tl = svld2_s16(pg16, scharr_row0 + x * 2L);
        svint16x2_t scharr_tr =
            svld2_s16(pg16, scharr_row0 + (x + channels) * 2L);
        svint16x2_t scharr_bl = svld2_s16(pg16, scharr_row1 + x * 2L);
        svint16x2_t scharr_br =
            svld2_s16(pg16, scharr_row1 + (x + channels) * 2L);

        svint16_t scharr_x = lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS>(
            svget2(scharr_tl, 0), svget2(scharr_tr, 0), svget2(scharr_bl, 0),
            svget2(scharr_br, 0), coeff_tl, coeff_tr, coeff_bl, coeff_br);

        svint16_t scharr_y = lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS>(
            svget2(scharr_tl, 1), svget2(scharr_tr, 1), svget2(scharr_bl, 1),
            svget2(scharr_br, 1), coeff_tl, coeff_tr, coeff_bl, coeff_br);

        svst2_s16(pg16, scharr_window_row + x * 2L,
                  svcreate2_s16(scharr_x, scharr_y));

        // sum_scharr_xx += scharr_x * scharr_x;
        sum_scharr_xx_v =
            svadd_f32_m(pg32, sum_scharr_xx_v,
                        svcvt_f32_s32_x(pg32, svmullb_s32(scharr_x, scharr_x)));
        sum_scharr_xx_v =
            svadd_f32_m(pg32, sum_scharr_xx_v,
                        svcvt_f32_s32_x(pg32, svmullt_s32(scharr_x, scharr_x)));

        // sum_scharr_xy += scharr_x * scharr_y;
        sum_scharr_xy_v =
            svadd_f32_m(pg32, sum_scharr_xy_v,
                        svcvt_f32_s32_x(pg32, svmullb_s32(scharr_x, scharr_y)));
        sum_scharr_xy_v =
            svadd_f32_m(pg32, sum_scharr_xy_v,
                        svcvt_f32_s32_x(pg32, svmullt_s32(scharr_x, scharr_y)));

        // sum_scharr_yy += scharr_y * scharr_y;
        sum_scharr_yy_v =
            svadd_f32_m(pg32, sum_scharr_yy_v,
                        svcvt_f32_s32_x(pg32, svmullb_s32(scharr_y, scharr_y)));
        sum_scharr_yy_v =
            svadd_f32_m(pg32, sum_scharr_yy_v,
                        svcvt_f32_s32_x(pg32, svmullt_s32(scharr_y, scharr_y)));
      }
    }
    sum_scharr_xx = svaddv_f32(svptrue_b32(), sum_scharr_xx_v) *
                    KLEIDICV_OPENCV_OPTICAL_FLOW_FIXED_POINT_DESCALE;
    sum_scharr_xy = svaddv_f32(svptrue_b32(), sum_scharr_xy_v) *
                    KLEIDICV_OPENCV_OPTICAL_FLOW_FIXED_POINT_DESCALE;
    sum_scharr_yy = svaddv_f32(svptrue_b32(), sum_scharr_yy_v) *
                    KLEIDICV_OPENCV_OPTICAL_FLOW_FIXED_POINT_DESCALE;
  }

  static inline void get_sum_diff_scharr(
      const uint8_t *next_data, ptrdiff_t next_stride, const int16_t *window,
      const int16_t *scharr_window, int channels, int window_corner_x,
      int window_corner_y, int window_width, int window_height,
      int16_t coeff_tl, int16_t coeff_tr, int16_t coeff_bl, int16_t coeff_br,
      float &sum_diff_scharr_x,
      float &sum_diff_scharr_y) KLEIDICV_STREAMING_COMPATIBLE {
    svfloat32_t sum_diff_scharr_x_v = svdup_n_f32(0),
                sum_diff_scharr_y_v = svdup_n_f32(0);

    for (int y = 0; y < window_height; y++) {
      const uint8_t *next_row0 =
          next_data + (y + window_corner_y) * next_stride +
          static_cast<ptrdiff_t>(window_corner_x) * channels;
      const uint8_t *next_row1 = next_row0 + next_stride;
      const int16_t *window_row =
          window + static_cast<ptrdiff_t>(y) * window_width;
      const int16_t *scharr_window_row = scharr_window + y * 2L * window_width;

      for (size_t x = 0; x < static_cast<size_t>(window_width) * channels;
           x += svcnth()) {
        svbool_t pg16 =
            svwhilelt_b16(x, static_cast<size_t>(window_width) * channels);
        svbool_t pg32 =
            svwhilelt_b32(x, static_cast<size_t>(window_width) * channels);

        svint16_t tl = svld1ub_s16(pg16, next_row0 + x);
        svint16_t tr = svld1ub_s16(pg16, next_row0 + x + channels);
        svint16_t bl = svld1ub_s16(pg16, next_row1 + x);
        svint16_t br = svld1ub_s16(pg16, next_row1 + x + channels);
        svint16_t diff = lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS - 5>(
            tl, tr, bl, br, coeff_tl, coeff_tr, coeff_bl, coeff_br);
        svint16_t win = svld1_s16(pg16, window_row + x);
        diff = svsub_s16_x(pg16, diff, win);

        svint16x2_t scharr_xy = svld2_s16(pg16, scharr_window_row + x * 2L);
        svint16_t scharr_x = svget2(scharr_xy, 0);
        svint16_t scharr_y = svget2(scharr_xy, 1);

        sum_diff_scharr_x_v =
            svadd_f32_m(pg32, sum_diff_scharr_x_v,
                        svcvt_f32_s32_x(pg32, svmullb_s32(scharr_x, diff)));
        sum_diff_scharr_x_v =
            svadd_f32_m(pg32, sum_diff_scharr_x_v,
                        svcvt_f32_s32_x(pg32, svmullt_s32(scharr_x, diff)));
        sum_diff_scharr_y_v =
            svadd_f32_m(pg32, sum_diff_scharr_y_v,
                        svcvt_f32_s32_x(pg32, svmullb_s32(scharr_y, diff)));
        sum_diff_scharr_y_v =
            svadd_f32_m(pg32, sum_diff_scharr_y_v,
                        svcvt_f32_s32_x(pg32, svmullt_s32(scharr_y, diff)));
      }
    }
    sum_diff_scharr_x = svaddv_f32(svptrue_b32(), sum_diff_scharr_x_v) *
                        KLEIDICV_OPENCV_OPTICAL_FLOW_FIXED_POINT_DESCALE;
    sum_diff_scharr_y = svaddv_f32(svptrue_b32(), sum_diff_scharr_y_v) *
                        KLEIDICV_OPENCV_OPTICAL_FLOW_FIXED_POINT_DESCALE;
  }
};

}  // namespace KLEIDICV_OPENCV_TARGET_NAMESPACE

#endif  // KLEIDICV_OPENCV_OPTICAL_FLOW_SC_H
