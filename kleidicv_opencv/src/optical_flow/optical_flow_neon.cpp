// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/neon.h"
#include "kleidicv_opencv/optical_flow.h"
#include "optical_flow_common.h"

namespace kleidicv_opencv::neon {

struct OpticalFlow {
  // Like SVE's svld1ub_s16 but for Neon.
  // Loads 4 bytes and widens them to int16.
  static inline int16x4_t vld1ub_s16(const uint8_t *ptr) {
    uint32_t a = 0;
    memcpy(&a, ptr, sizeof(a));
    uint32x2_t b = vset_lane_u32(a, vdup_n_s32(0), 0);
    uint8x8_t c = vreinterpret_u8_u32(b);
    return vget_low_s16(vmovl_u8(c));
  }

  template <int shift>
  static inline int16x4_t lerp(int16x4_t tl, int16x4_t tr, int16x4_t bl,
                               int16x4_t br, int16x4_t coeff_tl,
                               int16x4_t coeff_tr, int16x4_t coeff_bl,
                               int16x4_t coeff_br) {
    int32x4_t accumulator = vmull_s16(tl, coeff_tl);
    accumulator = vmlal_s16(accumulator, tr, coeff_tr);
    accumulator = vmlal_s16(accumulator, bl, coeff_bl);
    accumulator = vmlal_s16(accumulator, br, coeff_br);
    return vqrshrn_n_s32(accumulator, shift);
  }

  struct OpticalFlowGetWindowProcessRows {
    int16_t *window;
    const uint8_t *prev_data;
    ptrdiff_t prev_data_stride;
    int channels, window_corner_x, window_corner_y, window_width, window_height;

    template <typename F>
    void operator()(F f) const {
      for (int y = 0; y < window_height; y++) {
        const uint8_t *const prev_row0 =
            prev_data + (y + window_corner_y) * prev_data_stride +
            static_cast<ptrdiff_t>(window_corner_x) * channels;
        const uint8_t *const prev_row1 = prev_row0 + prev_data_stride;

        int16_t *const window_row =
            window + static_cast<ptrdiff_t>(y) * window_width;

        f(prev_row0, prev_row1, window_row);
      }
    }
  };

  // NOLINTBEGIN(readability-function-cognitive-complexity)
  static inline void get_window(int16_t *window, const uint8_t *prev_data,
                                ptrdiff_t prev_data_stride, int channels,
                                int window_corner_x, int window_corner_y,
                                int window_width, int window_height,
                                int16_t coeff_tl, int16_t coeff_tr,
                                int16_t coeff_bl, int16_t coeff_br) {
    const int16x4_t coeff_tl_v = vdup_n_s16(coeff_tl);
    const int16x4_t coeff_tr_v = vdup_n_s16(coeff_tr);
    const int16x4_t coeff_bl_v = vdup_n_s16(coeff_bl);
    const int16x4_t coeff_br_v = vdup_n_s16(coeff_br);

    auto process1 = [&](const uint8_t *prev_row0, const uint8_t *prev_row1,
                        int16_t *window_row, int x) {
      window_row[x] = static_cast<int16_t>(
          round_fixed_point<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS - 5>(
              prev_row0[x] * coeff_tl + prev_row0[x + channels] * coeff_tr +
              prev_row1[x] * coeff_bl + prev_row1[x + channels] * coeff_br));
    };
    auto process4 = [&](const uint8_t *prev_row0, const uint8_t *prev_row1,
                        int16_t *window_row, int x) {
      int16x4_t prev_tl = vld1ub_s16(prev_row0 + x);
      int16x4_t prev_tr = vld1ub_s16(prev_row0 + x + channels);
      int16x4_t prev_bl = vld1ub_s16(prev_row1 + x);
      int16x4_t prev_br = vld1ub_s16(prev_row1 + x + channels);

      int16x4_t prev = lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS - 5>(
          prev_tl, prev_tr, prev_bl, prev_br, coeff_tl_v, coeff_tr_v,
          coeff_bl_v, coeff_br_v);
      vst1_s16(window_row + x, prev);
    };
    auto process8 = [&](const uint8_t *prev_row0, const uint8_t *prev_row1,
                        int16_t *window_row, int x) {
      int16x8_t prev_tl =
          vreinterpretq_s16_u16(vmovl_u8(vld1_u8(prev_row0 + x)));
      int16x8_t prev_tr =
          vreinterpretq_s16_u16(vmovl_u8(vld1_u8(prev_row0 + x + channels)));
      int16x8_t prev_bl =
          vreinterpretq_s16_u16(vmovl_u8(vld1_u8(prev_row1 + x)));
      int16x8_t prev_br =
          vreinterpretq_s16_u16(vmovl_u8(vld1_u8(prev_row1 + x + channels)));

      int16x4_t prev_low = lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS - 5>(
          vget_low_u16(prev_tl), vget_low_u16(prev_tr), vget_low_u16(prev_bl),
          vget_low_u16(prev_br), coeff_tl_v, coeff_tr_v, coeff_bl_v,
          coeff_br_v);
      int16x4_t prev_high =
          lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS - 5>(
              vget_high_u16(prev_tl), vget_high_u16(prev_tr),
              vget_high_u16(prev_bl), vget_high_u16(prev_br), coeff_tl_v,
              coeff_tr_v, coeff_bl_v, coeff_br_v);
      vst1q_s16(window_row + x, vcombine_s16(prev_low, prev_high));
    };

    OpticalFlowGetWindowProcessRows process_rows = {
        window,          prev_data,       prev_data_stride, channels,
        window_corner_x, window_corner_y, window_width,     window_height};

    if (window_width * channels <= 8) {
      // Handle small window width
      if (window_width * channels > 4) {
        process_rows(
            [&](const uint8_t *row0, const uint8_t *row1, int16_t *window_row) {
              process4(row0, row1, window_row, 0);
              process4(row0, row1, window_row, window_width * channels - 4);
            });
      } else {
        // Handle tiny window width
        process_rows(
            [&](const uint8_t *row0, const uint8_t *row1, int16_t *window_row) {
              for (int x = 0; x < window_width * channels; ++x) {
                process1(row0, row1, window_row, x);
              }
            });
      }
    } else {
      switch (window_width * channels & 7) {
        case 1:
          process_rows([&](const uint8_t *row0, const uint8_t *row1,
                           int16_t *window_row) {
            int x = 0;
            for (; x + 8 <= window_width * channels; x += 8) {
              process8(row0, row1, window_row, x);
            }
            process1(row0, row1, window_row, x);
          });
          break;
        case 5:
          process_rows([&](const uint8_t *row0, const uint8_t *row1,
                           int16_t *window_row) {
            int x = 0;
            for (; x + 8 <= window_width * channels; x += 8) {
              process8(row0, row1, window_row, x);
            }
            process4(row0, row1, window_row, x);
            process1(row0, row1, window_row, x + 4);
          });
          break;
        case 6:
        case 7:
          process_rows([&](const uint8_t *row0, const uint8_t *row1,
                           int16_t *window_row) {
            for (int x = 0; x + 8 <= window_width * channels; x += 8) {
              process8(row0, row1, window_row, x);
            }
            process8(row0, row1, window_row, window_width * channels - 8);
          });
          break;
        default:
          process_rows([&](const uint8_t *row0, const uint8_t *row1,
                           int16_t *window_row) {
            for (int x = 0; x + 8 <= window_width * channels; x += 8) {
              process8(row0, row1, window_row, x);
            }
            process4(row0, row1, window_row, window_width * channels - 4);
          });
          break;
      }
    }
  }
  // NOLINTEND(readability-function-cognitive-complexity)

  struct OpticalFlowGetScharrProcessRows {
    int16_t *scharr_window;
    const int16_t *scharr_data;
    ptrdiff_t scharr_stride_elements;
    int channels, window_corner_x, window_corner_y, window_width, window_height;

    template <typename F>
    void operator()(F f) const {
      for (int y = 0; y < window_height; y++) {
        const int16_t *const scharr_row0 =
            scharr_data + (y + window_corner_y) * scharr_stride_elements +
            static_cast<ptrdiff_t>(window_corner_x) * channels * 2L;
        const int16_t *const scharr_row1 = scharr_row0 + scharr_stride_elements;

        int16_t *const scharr_window_row =
            scharr_window + static_cast<ptrdiff_t>(y) * window_width * 2L;

        f(scharr_row0, scharr_row1, scharr_window_row);
      }
    }
  };

  static inline void get_scharr(
      int16_t *scharr_window, const int16_t *scharr_data,
      ptrdiff_t scharr_stride_elements, int channels, int window_corner_x,
      int window_corner_y, int window_width, int window_height,
      int16_t coeff_tl, int16_t coeff_tr, int16_t coeff_bl, int16_t coeff_br,
      float &sum_scharr_xx, float &sum_scharr_xy, float &sum_scharr_yy) {
    sum_scharr_xx = 0;
    sum_scharr_xy = 0;
    sum_scharr_yy = 0;

    float32x4_t sum_scharr_xx_v = vdupq_n_f32(0),
                sum_scharr_xy_v = vdupq_n_f32(0),
                sum_scharr_yy_v = vdupq_n_f32(0);

    const int16x4_t coeff_tl_v = vdup_n_s16(coeff_tl);
    const int16x4_t coeff_tr_v = vdup_n_s16(coeff_tr);
    const int16x4_t coeff_bl_v = vdup_n_s16(coeff_bl);
    const int16x4_t coeff_br_v = vdup_n_s16(coeff_br);

    auto make_leftover_accumulate_mask_s16 = [](int width) {
      int count3 = width & 3;
      int count4 = count3 == 0 ? 4 : count3;
      return vcreate_s16(0x000000000000ffffULL * (count4 >= 4) +
                         0x00000000ffff0000ULL * (count4 >= 3) +
                         0x0000ffff00000000ULL * (count4 >= 2) +
                         0xffff000000000000ULL * (count4 >= 1));
    };
    const int16x4_t leftover_mask =
        make_leftover_accumulate_mask_s16(window_width * channels);

    auto process1_and_accumulate = [&](const int16_t *row0, const int16_t *row1,
                                       int16_t *scharr_window_row, int x) {
      int scharr_x =
          round_fixed_point<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS>(
              row0[x * 2L] * coeff_tl + row0[(x + channels) * 2L] * coeff_tr +
              row1[x * 2L] * coeff_bl + row1[(x + channels) * 2L] * coeff_br);
      int scharr_y =
          round_fixed_point<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS>(
              row0[x * 2L + 1] * coeff_tl +
              row0[(x + channels) * 2L + 1] * coeff_tr +
              row1[x * 2L + 1] * coeff_bl +
              row1[(x + channels) * 2L + 1] * coeff_br);

      scharr_window_row[x * 2L] = static_cast<int16_t>(scharr_x);
      scharr_window_row[x * 2L + 1] = static_cast<int16_t>(scharr_y);

      sum_scharr_xx += static_cast<float>(scharr_x * scharr_x);
      sum_scharr_xy += static_cast<float>(scharr_x * scharr_y);
      sum_scharr_yy += static_cast<float>(scharr_y * scharr_y);
    };
    auto process4 = [&](const int16_t *row0, const int16_t *row1,
                        int16_t *scharr_window_row, int x) {
      int16x4x2_t scharr_tl = vld2_s16(row0 + x * 2L);
      int16x4x2_t scharr_tr = vld2_s16(row0 + (x + channels) * 2L);
      int16x4x2_t scharr_bl = vld2_s16(row1 + x * 2L);
      int16x4x2_t scharr_br = vld2_s16(row1 + (x + channels) * 2L);

      int16x4_t scharr_x = lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS>(
          scharr_tl.val[0], scharr_tr.val[0], scharr_bl.val[0],
          scharr_br.val[0], coeff_tl_v, coeff_tr_v, coeff_bl_v, coeff_br_v);

      int16x4_t scharr_y = lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS>(
          scharr_tl.val[1], scharr_tr.val[1], scharr_bl.val[1],
          scharr_br.val[1], coeff_tl_v, coeff_tr_v, coeff_bl_v, coeff_br_v);

      int16x4x2_t scharr_xy{scharr_x, scharr_y};

      vst2_s16(scharr_window_row + x * 2L, scharr_xy);

      return scharr_xy;
    };
    auto accumulate = [&](int16x4_t scharr_x, int16x4_t scharr_y) {
      // sum_scharr_xx += scharr_x * scharr_x;
      sum_scharr_xx_v = vaddq_f32(sum_scharr_xx_v,
                                  vcvtq_f32_s32(vmull_s16(scharr_x, scharr_x)));

      // sum_scharr_xy += scharr_x * scharr_y;
      sum_scharr_xy_v = vaddq_f32(sum_scharr_xy_v,
                                  vcvtq_f32_s32(vmull_s16(scharr_x, scharr_y)));

      // sum_scharr_yy += scharr_y * scharr_y;
      sum_scharr_yy_v = vaddq_f32(sum_scharr_yy_v,
                                  vcvtq_f32_s32(vmull_s16(scharr_y, scharr_y)));
    };
    auto process4_and_accumulate = [&](const int16_t *row0, const int16_t *row1,
                                       int16_t *scharr_window_row, int x) {
      int16x4x2_t scharr_xy = process4(row0, row1, scharr_window_row, x);
      accumulate(scharr_xy.val[0], scharr_xy.val[1]);
    };
    auto process4_and_accumulate_leftovers = [&](const int16_t *row0,
                                                 const int16_t *row1,
                                                 int16_t *scharr_window_row) {
      int16x4x2_t scharr_xy =
          process4(row0, row1, scharr_window_row, window_width * channels - 4);
      int16x4_t masked_x = vand_s16(scharr_xy.val[0], leftover_mask);
      int16x4_t masked_y = vand_s16(scharr_xy.val[1], leftover_mask);
      accumulate(masked_x, masked_y);
    };

    OpticalFlowGetScharrProcessRows process_rows = {
        scharr_window, scharr_data,     scharr_stride_elements,
        channels,      window_corner_x, window_corner_y,
        window_width,  window_height};

    if ((window_width * channels & 3) == 1) {
      // Optimization for the most common case that the window width is a
      // multiple of four plus one.
      process_rows(
          [&](const int16_t *row0, const int16_t *row1, int16_t *window_row) {
            int x = 0;
            for (; x + 4 <= window_width * channels; x += 4) {
              process4_and_accumulate(row0, row1, window_row, x);
            }
            process1_and_accumulate(row0, row1, window_row, x);
          });
    } else if (window_width * channels <= 4) {
      // Handle small window width
      process_rows(
          [&](const int16_t *row0, const int16_t *row1, int16_t *window_row) {
            for (int x = 0; x < window_width * channels; ++x) {
              process1_and_accumulate(row0, row1, window_row, x);
            }
          });
    } else {
      process_rows(
          [&](const int16_t *row0, const int16_t *row1, int16_t *window_row) {
            for (int x = 0; x + 4 < window_width * channels; x += 4) {
              process4_and_accumulate(row0, row1, window_row, x);
            }
            process4_and_accumulate_leftovers(row0, row1, window_row);
          });
    }

    sum_scharr_xx += vaddvq_f32(sum_scharr_xx_v);
    sum_scharr_xy += vaddvq_f32(sum_scharr_xy_v);
    sum_scharr_yy += vaddvq_f32(sum_scharr_yy_v);
    sum_scharr_xx *= KLEIDICV_OPENCV_OPTICAL_FLOW_FIXED_POINT_DESCALE;
    sum_scharr_xy *= KLEIDICV_OPENCV_OPTICAL_FLOW_FIXED_POINT_DESCALE;
    sum_scharr_yy *= KLEIDICV_OPENCV_OPTICAL_FLOW_FIXED_POINT_DESCALE;
  }

  struct OpticalFlowGetSumDiffScharrProcessRows {
    const uint8_t *next_data;
    ptrdiff_t next_stride;
    const int16_t *window;
    const int16_t *scharr_window;
    int channels, window_corner_x, window_corner_y, window_width, window_height;

    template <typename F>
    void operator()(F f) const {
      for (int y = 0; y < window_height; y++) {
        const uint8_t *row0 =
            next_data + (y + window_corner_y) * next_stride +
            static_cast<ptrdiff_t>(window_corner_x) * channels;
        const uint8_t *row1 = row0 + next_stride;
        const int16_t *window_row =
            window + static_cast<ptrdiff_t>(y) * window_width;
        const int16_t *scharr_window_row =
            scharr_window + static_cast<ptrdiff_t>(y) * window_width * 2L;

        f(row0, row1, window_row, scharr_window_row);
      }
    }
  };

  // This function is too complex, but disable the warning for now.
  // NOLINTBEGIN(readability-function-cognitive-complexity)
  static inline void get_sum_diff_scharr(
      const uint8_t *next_data, ptrdiff_t next_stride, const int16_t *window,
      const int16_t *scharr_window, int channels, int window_corner_x,
      int window_corner_y, int window_width, int window_height,
      int16_t coeff_tl, int16_t coeff_tr, int16_t coeff_bl, int16_t coeff_br,
      float &sum_diff_scharr_x, float &sum_diff_scharr_y) {
    OpticalFlowGetSumDiffScharrProcessRows process_rows = {
        next_data,       next_stride,     window,       scharr_window, channels,
        window_corner_x, window_corner_y, window_width, window_height};

    sum_diff_scharr_x = 0;
    sum_diff_scharr_y = 0;

    float32x4_t sum_diff_scharr_x_v = vdupq_n_f32(0),
                sum_diff_scharr_y_v = vdupq_n_f32(0);

    const int16x4_t coeff_tl_v = vdup_n_s16(coeff_tl);
    const int16x4_t coeff_tr_v = vdup_n_s16(coeff_tr);
    const int16x4_t coeff_bl_v = vdup_n_s16(coeff_bl);
    const int16x4_t coeff_br_v = vdup_n_s16(coeff_br);

    auto make_leftover_accumulate_mask_s32 = [](int width) {
      int count7 = width & 7;
      int count3 = width & 3;
      int count4 = count7 == 4 ? 4 : count3;
      return vcombine_s32(vcreate_s32(0x00000000ffffffffULL * (count4 >= 4) +
                                      0xffffffff00000000ULL * (count4 >= 3)),
                          vcreate_s32(0x00000000ffffffffULL * (count4 >= 2) +
                                      0xffffffff00000000ULL * (count4 >= 1)));
    };

    const int32x4_t leftover_mask =
        make_leftover_accumulate_mask_s32(window_width * channels);

    auto process8_and_accumulate = [&](const uint8_t *row0, const uint8_t *row1,
                                       const int16_t *window_row,
                                       const int16_t *scharr_window_row,
                                       int x) {
      int16x8_t tl = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(row0 + x)));
      int16x8_t tr =
          vreinterpretq_s16_u16(vmovl_u8(vld1_u8(row0 + x + channels)));
      int16x8_t bl = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(row1 + x)));
      int16x8_t br =
          vreinterpretq_s16_u16(vmovl_u8(vld1_u8(row1 + x + channels)));

      int16x4_t low = lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS - 5>(
          vget_low_s16(tl), vget_low_s16(tr), vget_low_s16(bl),
          vget_low_s16(br), coeff_tl_v, coeff_tr_v, coeff_bl_v, coeff_br_v);

      int16x4_t high = lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS - 5>(
          vget_high_s16(tl), vget_high_s16(tr), vget_high_s16(bl),
          vget_high_s16(br), coeff_tl_v, coeff_tr_v, coeff_bl_v, coeff_br_v);

      int16x8_t next = vcombine_s16(low, high);

      int16x8_t diff = vsubq_s16(next, vld1q_s16(window_row + x));

      int16x8x2_t scharr_xy = vld2q_s16(scharr_window_row + x * 2L);
      int16x8_t scharr_x = scharr_xy.val[0];
      int16x8_t scharr_y = scharr_xy.val[1];

      int32x4_t diff_scharr_x =
          vmlal_high_s16(vmull_s16(vget_low_s16(scharr_x), vget_low_s16(diff)),
                         scharr_x, diff);
      int32x4_t diff_scharr_y =
          vmlal_high_s16(vmull_s16(vget_low_s16(scharr_y), vget_low_s16(diff)),
                         scharr_y, diff);

      sum_diff_scharr_x_v =
          vaddq_f32(sum_diff_scharr_x_v, vcvtq_f32_s32(diff_scharr_x));
      sum_diff_scharr_y_v =
          vaddq_f32(sum_diff_scharr_y_v, vcvtq_f32_s32(diff_scharr_y));
    };
    auto process4 = [&](const uint8_t *row0, const uint8_t *row1,
                        const int16_t *window_row,
                        const int16_t *scharr_window_row, int x) {
      uint16x4_t tl = vld1ub_s16(row0 + x);
      uint16x4_t tr = vld1ub_s16(row0 + x + channels);
      uint16x4_t bl = vld1ub_s16(row1 + x);
      uint16x4_t br = vld1ub_s16(row1 + x + channels);

      int16x4_t next = lerp<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS - 5>(
          tl, tr, bl, br, coeff_tl_v, coeff_tr_v, coeff_bl_v, coeff_br_v);

      int16x4_t diff = vsub_s16(next, vld1_s16(window_row + x));

      int16x4x2_t scharr_xy = vld2_s16(scharr_window_row + x * 2L);
      int16x4_t scharr_x = scharr_xy.val[0];
      int16x4_t scharr_y = scharr_xy.val[1];

      int32x4_t diff_scharr_x = vmull_s16(scharr_x, diff);
      int32x4_t diff_scharr_y = vmull_s16(scharr_y, diff);
      return int32x4x2_t{diff_scharr_x, diff_scharr_y};
    };
    auto process4_and_accumulate =
        [&](const uint8_t *row0, const uint8_t *row1, const int16_t *window_row,
            const int16_t *scharr_window_row, int x) {
          int32x4x2_t diff_scharr =
              process4(row0, row1, window_row, scharr_window_row, x);
          sum_diff_scharr_x_v =
              vaddq_f32(sum_diff_scharr_x_v, vcvtq_f32_s32(diff_scharr.val[0]));
          sum_diff_scharr_y_v =
              vaddq_f32(sum_diff_scharr_y_v, vcvtq_f32_s32(diff_scharr.val[1]));
        };
    auto process4_and_accumulate_leftovers =
        [&](const uint8_t *row0, const uint8_t *row1, const int16_t *window_row,
            const int16_t *scharr_window_row) {
          int32x4x2_t diff_scharr =
              process4(row0, row1, window_row, scharr_window_row,
                       window_width * channels - 4);
          int32x4_t masked_x = vandq_s32(diff_scharr.val[0], leftover_mask);
          int32x4_t masked_y = vandq_s32(diff_scharr.val[1], leftover_mask);
          sum_diff_scharr_x_v =
              vaddq_f32(sum_diff_scharr_x_v, vcvtq_f32_s32(masked_x));
          sum_diff_scharr_y_v =
              vaddq_f32(sum_diff_scharr_y_v, vcvtq_f32_s32(masked_y));
        };
    auto process1_and_accumulate = [&](const uint8_t *row0, const uint8_t *row1,
                                       const int16_t *window_row,
                                       const int16_t *scharr_window_row,
                                       int x) {
      int next =
          round_fixed_point<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS - 5>(
              row0[x] * coeff_tl + row0[x + channels] * coeff_tr +
              row1[x] * coeff_bl + row1[x + channels] * coeff_br);
      int diff = next - window_row[x];
      sum_diff_scharr_x += static_cast<float>(diff * scharr_window_row[x * 2L]);
      sum_diff_scharr_y +=
          static_cast<float>(diff * scharr_window_row[x * 2L + 1]);
    };

    if (window_width * channels < 8) {
      // Handle small window width
      if (window_width * channels > 4) {
        process_rows([&](const uint8_t *row0, const uint8_t *row1,
                         const int16_t *window_row,
                         const int16_t *scharr_window_row) {
          process4_and_accumulate(row0, row1, window_row, scharr_window_row, 0);
          process4_and_accumulate_leftovers(row0, row1, window_row,
                                            scharr_window_row);
        });
      } else {
        // Handle tiny window width
        process_rows([&](const uint8_t *row0, const uint8_t *row1,
                         const int16_t *window_row,
                         const int16_t *scharr_window_row) {
          for (int x = 0; x < window_width * channels; ++x) {
            process1_and_accumulate(row0, row1, window_row, scharr_window_row,
                                    x);
          }
        });
      }
    } else {
      switch (window_width * channels & 7) {
        case 1:
          process_rows([&](const uint8_t *row0, const uint8_t *row1,
                           const int16_t *window_row,
                           const int16_t *scharr_window_row) {
            int x = 0;
            for (; x + 8 <= window_width * channels; x += 8) {
              process8_and_accumulate(row0, row1, window_row, scharr_window_row,
                                      x);
            }
            process1_and_accumulate(row0, row1, window_row, scharr_window_row,
                                    x);
          });
          break;
        case 5:
          process_rows([&](const uint8_t *row0, const uint8_t *row1,
                           const int16_t *window_row,
                           const int16_t *scharr_window_row) {
            int x = 0;
            for (; x + 8 <= window_width * channels; x += 8) {
              process8_and_accumulate(row0, row1, window_row, scharr_window_row,
                                      x);
            }
            process4_and_accumulate(row0, row1, window_row, scharr_window_row,
                                    x);
            process1_and_accumulate(row0, row1, window_row, scharr_window_row,
                                    x + 4);
          });
          break;
        case 6:
        case 7:
          process_rows([&](const uint8_t *row0, const uint8_t *row1,
                           const int16_t *window_row,
                           const int16_t *scharr_window_row) {
            int x = 0;
            for (; x + 8 <= window_width * channels; x += 8) {
              process8_and_accumulate(row0, row1, window_row, scharr_window_row,
                                      x);
            }
            process4_and_accumulate(row0, row1, window_row, scharr_window_row,
                                    x);
            process4_and_accumulate_leftovers(row0, row1, window_row,
                                              scharr_window_row);
          });
          break;
        default:
          process_rows([&](const uint8_t *row0, const uint8_t *row1,
                           const int16_t *window_row,
                           const int16_t *scharr_window_row) {
            for (int x = 0; x + 8 <= window_width * channels; x += 8) {
              process8_and_accumulate(row0, row1, window_row, scharr_window_row,
                                      x);
            }
            process4_and_accumulate_leftovers(row0, row1, window_row,
                                              scharr_window_row);
          });
          break;
      }
    }

    sum_diff_scharr_x += vaddvq_f32(sum_diff_scharr_x_v);
    sum_diff_scharr_y += vaddvq_f32(sum_diff_scharr_y_v);
    sum_diff_scharr_x *= KLEIDICV_OPENCV_OPTICAL_FLOW_FIXED_POINT_DESCALE;
    sum_diff_scharr_y *= KLEIDICV_OPENCV_OPTICAL_FLOW_FIXED_POINT_DESCALE;
  }
  // NOLINTEND(readability-function-cognitive-complexity)
};

KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t optical_flow_u8(
    const uint8_t *prev_data, size_t prev_data_step,
    const int16_t *prev_deriv_data, size_t prev_deriv_step,
    const uint8_t *next_data, size_t next_step, int width, int height,
    int channels, const float *prev_points, float *next_points,
    size_t point_count, uint8_t *status, float *err, int window_width,
    int window_height, int termination_count, double termination_epsilon,
    bool get_min_eigen_vals, float min_eigen_vals_threshold) {
  OpticalFlowWindowBuffer window_buffer(window_width, window_height, channels);
  if (!window_buffer.window()) {
    return KLEIDICV_ERROR_ALLOCATION;
  }
  return optical_flow_common<OpticalFlow>(
      window_buffer.window(), window_buffer.deriv_window(), prev_data,
      prev_data_step, prev_deriv_data, prev_deriv_step, next_data, next_step,
      width, height, channels, prev_points, next_points, point_count, status,
      err, window_width, window_height, termination_count, termination_epsilon,
      get_min_eigen_vals, min_eigen_vals_threshold);
}

}  // namespace kleidicv_opencv::neon
