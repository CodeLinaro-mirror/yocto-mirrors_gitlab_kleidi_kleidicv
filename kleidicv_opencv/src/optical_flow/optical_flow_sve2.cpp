// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv_opencv/optical_flow.h"
#include "optical_flow_sc.h"

namespace kleidicv_opencv::sve2 {

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

}  // namespace kleidicv_opencv::sve2
