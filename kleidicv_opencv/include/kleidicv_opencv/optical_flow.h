// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_OPENCV_OPTICAL_FLOW_H
#define KLEIDICV_OPENCV_OPTICAL_FLOW_H

#include "kleidicv/kleidicv.h"

namespace kleidicv_opencv {

namespace neon {

kleidicv_error_t optical_flow_u8(
    const uint8_t *prev_data, size_t prev_data_step,
    const int16_t *prev_deriv_data, size_t prev_deriv_step,
    const uint8_t *next_data, size_t next_step, int width, int height,
    int channels, const float *prev_points, float *next_points,
    size_t point_count, uint8_t *status, float *err, int window_width,
    int window_height, int termination_count, double termination_epsilon,
    bool get_min_eigen_vals, float min_eigen_vals_threshold);

}  // namespace neon

namespace sve2 {

kleidicv_error_t optical_flow_u8(
    const uint8_t *prev_data, size_t prev_data_step,
    const int16_t *prev_deriv_data, size_t prev_deriv_step,
    const uint8_t *next_data, size_t next_step, int width, int height,
    int channels, const float *prev_points, float *next_points,
    size_t point_count, uint8_t *status, float *err, int window_width,
    int window_height, int termination_count, double termination_epsilon,
    bool get_min_eigen_vals, float min_eigen_vals_threshold);

}  // namespace sve2

namespace sme2 {

kleidicv_error_t optical_flow_u8(
    const uint8_t *prev_data, size_t prev_data_step,
    const int16_t *prev_deriv_data, size_t prev_deriv_step,
    const uint8_t *next_data, size_t next_step, int width, int height,
    int channels, const float *prev_points, float *next_points,
    size_t point_count, uint8_t *status, float *err, int window_width,
    int window_height, int termination_count, double termination_epsilon,
    bool get_min_eigen_vals, float min_eigen_vals_threshold);

}  // namespace sme2

}  // namespace kleidicv_opencv

#endif  // KLEIDICV_OPENCV_OPTICAL_FLOW_H
