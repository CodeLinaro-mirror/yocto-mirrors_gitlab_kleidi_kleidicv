// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv_opencv/optical_flow.h"
#include "optical_flow_sc.h"

namespace kleidicv_opencv::sme2 {

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS static kleidicv_error_t
optical_flow_u8_streaming(int16_t *window, int16_t *deriv_window,
                          const uint8_t *prev_data, size_t prev_data_step,
                          const int16_t *prev_deriv_data,
                          size_t prev_deriv_step, const uint8_t *next_data,
                          size_t next_step, int width, int height, int channels,
                          const float *prev_points, float *next_points,
                          size_t point_count, uint8_t *status, float *err,
                          int window_width, int window_height,
                          int termination_count, double termination_epsilon,
                          bool get_min_eigen_vals,
                          float min_eigen_vals_threshold) {
  return optical_flow_common<OpticalFlow>(
      window, deriv_window, prev_data, prev_data_step, prev_deriv_data,
      prev_deriv_step, next_data, next_step, width, height, channels,
      prev_points, next_points, point_count, status, err, window_width,
      window_height, termination_count, termination_epsilon, get_min_eigen_vals,
      min_eigen_vals_threshold);
}

KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t optical_flow_u8(
    const uint8_t *prev_data, size_t prev_data_step,
    const int16_t *prev_deriv_data, size_t prev_deriv_step,
    const uint8_t *next_data, size_t next_step, int width, int height,
    int channels, const float *prev_points, float *next_points,
    size_t point_count, uint8_t *status, float *err, int window_width,
    int window_height, int termination_count, double termination_epsilon,
    bool get_min_eigen_vals, float min_eigen_vals_threshold) {
  // When targetting SME, always allocate on the heap to avoid the memory page
  // being written by both Streaming Mode Compute Unit and CPU.
  constexpr int kStackBufferSize = 0;
  OpticalFlowWindowBuffer<kStackBufferSize> window_buffer(
      window_width, window_height, channels);
  if (!window_buffer.window()) {
    return KLEIDICV_ERROR_ALLOCATION;
  }
  return optical_flow_u8_streaming(
      window_buffer.window(), window_buffer.deriv_window(), prev_data,
      prev_data_step, prev_deriv_data, prev_deriv_step, next_data, next_step,
      width, height, channels, prev_points, next_points, point_count, status,
      err, window_width, window_height, termination_count, termination_epsilon,
      get_min_eigen_vals, min_eigen_vals_threshold);
}

}  // namespace kleidicv_opencv::sme2
