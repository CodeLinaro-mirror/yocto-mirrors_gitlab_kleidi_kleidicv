// SPDX-FileCopyrightText: 2000, Intel Corporation, all rights reserved.
// SPDX-FileCopyrightText: 2013, OpenCV Foundation, all rights reserved.
// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: BSD-3-Clause AND Apache-2.0

#ifndef KLEIDICV_OPENCV_OPTICAL_FLOW_COMMON_H
#define KLEIDICV_OPENCV_OPTICAL_FLOW_COMMON_H

#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "kleidicv/containers/small_buffer.h"
#include "kleidicv_opencv/kleidicv_opencv.h"
#include "kleidicv_opencv/optical_flow.h"

#define KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS 14
#define KLEIDICV_OPENCV_OPTICAL_FLOW_FIXED_POINT_DESCALE (1.0F / (1 << 20))

namespace KLEIDICV_OPENCV_TARGET_NAMESPACE {

template <int shift>
inline std::array<int16_t, 4> get_lerp_params(float x, float y)
    KLEIDICV_STREAMING_COMPATIBLE {
  int16_t a =
      static_cast<int16_t>(rintf((1.0F - x) * (1.0F - y) * (1 << shift)));
  int16_t b = static_cast<int16_t>(rintf(x * (1.0F - y) * (1 << shift)));
  int16_t c = static_cast<int16_t>(rintf((1.0F - x) * y * (1 << shift)));
  int16_t d = (1 << shift) - a - b - c;
  return {a, b, c, d};
}

template <int fraction_bits>
inline int round_fixed_point(int x) KLEIDICV_STREAMING_COMPATIBLE {
  int half = 1 << (fraction_bits - 1);
  return (x + half) >> fraction_bits;
}

// This function is too complex, but disable the warning for now.
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <typename Impl>
KLEIDICV_TARGET_FN_ATTRS static kleidicv_error_t optical_flow_common(
    int16_t *window, int16_t *scharr_window, const uint8_t *prev_data,
    size_t prev_data_stride, const int16_t *scharr_data,
    size_t scharr_stride_bytes, const uint8_t *next_data, size_t next_stride,
    int width, int height, int channels, const float *prev_points,
    float *next_points, size_t point_count, uint8_t *status, float *err,
    int window_width, int window_height, int termination_count,
    double termination_epsilon, bool get_min_eigen_vals,
    float min_eigen_vals_threshold) KLEIDICV_STREAMING_COMPATIBLE {
  if (channels != 1) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  auto point_out_of_bounds = [&](int x, int y) {
    return x < -window_width || x >= width || y < -window_height || y >= height;
  };

  const ptrdiff_t scharr_stride_elements =
      static_cast<ptrdiff_t>(scharr_stride_bytes / sizeof(int16_t));

  const float half_window_width = static_cast<float>(window_width - 1) * 0.5F;
  const float half_window_height = static_cast<float>(window_height - 1) * 0.5F;

  for (size_t point_index = 0; point_index < point_count; point_index++) {
    const float prev_x = prev_points[point_index * 2] - half_window_width;
    const float prev_y = prev_points[point_index * 2 + 1] - half_window_height;
    const int prev_xi = static_cast<int>(floorf(prev_x));
    const int prev_yi = static_cast<int>(floorf(prev_y));

    if (point_out_of_bounds(prev_xi, prev_yi)) {
      if (status) {
        status[point_index] = false;
      }
      continue;
    }

    const auto [coeff_tl, coeff_tr, coeff_bl, coeff_br] =
        get_lerp_params<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS>(
            prev_x - static_cast<float>(prev_xi),
            prev_y - static_cast<float>(prev_yi));

    Impl::get_window(window, prev_data, prev_data_stride, channels, prev_xi,
                     prev_yi, window_width, window_height, coeff_tl, coeff_tr,
                     coeff_bl, coeff_br);

    float sum_scharr_xx = 0, sum_scharr_xy = 0, sum_scharr_yy = 0;
    Impl::get_scharr(scharr_window, scharr_data, scharr_stride_elements,
                     channels, prev_xi, prev_yi, window_width, window_height,
                     coeff_tl, coeff_tr, coeff_bl, coeff_br, sum_scharr_xx,
                     sum_scharr_xy, sum_scharr_yy);

    const float determinant =
        sum_scharr_xx * sum_scharr_yy - powf(sum_scharr_xy, 2);
    float min_eigen_val = (sum_scharr_yy + sum_scharr_xx -
                           sqrtf(powf(sum_scharr_xx - sum_scharr_yy, 2) +
                                 4 * powf(sum_scharr_xy, 2))) /
                          static_cast<float>(2L * window_width * window_height);

    if (err && get_min_eigen_vals) {
      err[point_index] = min_eigen_val;
    }

    if (min_eigen_val < min_eigen_vals_threshold || determinant < FLT_EPSILON) {
      if (status) {
        status[point_index] = false;
      }
      continue;
    }

    const float inverse_determinant = 1.0F / determinant;

    float next_x = next_points[point_index * 2] - half_window_width;
    float next_y = next_points[point_index * 2 + 1] - half_window_height;
    float prev_velocity_x = 0, prev_velocity_y = 0;

    for (int j = 0; j < termination_count; j++) {
      const int next_xi = static_cast<int>(floorf(next_x));
      const int next_yi = static_cast<int>(floorf(next_y));

      if (point_out_of_bounds(next_xi, next_yi)) {
        if (status) {
          status[point_index] = false;
        }
        break;
      }

      const auto [coeff_tl, coeff_tr, coeff_bl, coeff_br] =
          get_lerp_params<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS>(
              next_x - static_cast<float>(next_xi),
              next_y - static_cast<float>(next_yi));

      float sum_diff_scharr_x = 0, sum_diff_scharr_y = 0;
      Impl::get_sum_diff_scharr(next_data, next_stride, window, scharr_window,
                                channels, next_xi, next_yi, window_width,
                                window_height, coeff_tl, coeff_tr, coeff_bl,
                                coeff_br, sum_diff_scharr_x, sum_diff_scharr_y);

      const float velocity_x = (sum_scharr_xy * sum_diff_scharr_y -
                                sum_scharr_yy * sum_diff_scharr_x) *
                               inverse_determinant;
      const float velocity_y = (sum_scharr_xy * sum_diff_scharr_x -
                                sum_scharr_xx * sum_diff_scharr_y) *
                               inverse_determinant;

      next_x += velocity_x;
      next_y += velocity_y;
      next_points[point_index * 2] = next_x + half_window_width;
      next_points[point_index * 2 + 1] = next_y + half_window_height;

      if (velocity_x * velocity_x + velocity_y * velocity_y <=
          termination_epsilon) {
        break;
      }

      if (j != 0 && fabsf(velocity_x + prev_velocity_x) < 0.01F &&
          fabsf(velocity_y + prev_velocity_y) < 0.01F) {
        next_points[point_index * 2] -= velocity_x * 0.5F;
        next_points[point_index * 2 + 1] -= velocity_y * 0.5F;
        break;
      }
      prev_velocity_x = velocity_x;
      prev_velocity_y = velocity_y;
    }

    if (status && status[point_index]) {
      next_x = next_points[point_index * 2] - half_window_width;
      next_y = next_points[point_index * 2 + 1] - half_window_height;
      const int next_xi = static_cast<int>(floorf(next_x));
      const int next_yi = static_cast<int>(floorf(next_y));

      if (point_out_of_bounds(next_xi, next_yi)) {
        status[point_index] = false;
      } else if (err && !get_min_eigen_vals) {
        const auto [coeff_tl, coeff_tr, coeff_bl, coeff_br] =
            get_lerp_params<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS>(
                next_x - static_cast<float>(next_xi),
                next_y - static_cast<float>(next_yi));
        float errval = 0;

        for (int y = 0; y < window_height; y++) {
          int16_t *window_row =
              window + static_cast<ptrdiff_t>(y) * window_width;
          const uint8_t *next_row0 =
              next_data + (y + next_yi) * static_cast<ptrdiff_t>(next_stride) +
              static_cast<ptrdiff_t>(next_xi) * channels;
          const uint8_t *next_row1 = next_row0 + next_stride;

          for (int x = 0; x < window_width * channels; x++) {
            int diff =
                round_fixed_point<KLEIDICV_OPENCV_OPTICAL_FLOW_FRACTION_BITS -
                                  5>(next_row0[x] * coeff_tl +
                                     next_row0[x + channels] * coeff_tr +
                                     next_row1[x] * coeff_bl +
                                     next_row1[x + channels] * coeff_br) -
                window_row[x];
            errval += fabsf(static_cast<float>(diff));
          }
        }
        err[point_index] =
            errval * 1.0F /
            static_cast<float>(32L * window_width * channels * window_height);
      }
    }
  }
  return KLEIDICV_OK;
}
// NOLINTEND(readability-function-cognitive-complexity)

// Allocates optical flow's window buffers.
// If the required buffer size is no greater than StackBufferSize then the
// buffers will be allocated on the stack. Typical window width is 21.
template <int StackBufferSize = 21UL * 21UL * 3UL>
class OpticalFlowWindowBuffer {
 public:
  OpticalFlowWindowBuffer(int window_width, int window_height, int channels)
      : window_buffer_(static_cast<size_t>(window_width) * window_height *
                       channels * 3UL),
        deriv_window_(window_buffer_.get()
                          ? window_buffer_.get() +
                                static_cast<size_t>(window_width) *
                                    window_height * channels
                          : nullptr) {}
  int16_t *window() { return window_buffer_.get(); }
  int16_t *deriv_window() { return deriv_window_; }

 private:
  kleidicv::SmallBuffer<int16_t, StackBufferSize> window_buffer_;
  int16_t *deriv_window_;
};

}  // namespace KLEIDICV_OPENCV_TARGET_NAMESPACE

#endif  // KLEIDICV_OPENCV_OPTICAL_FLOW_COMMON_H
