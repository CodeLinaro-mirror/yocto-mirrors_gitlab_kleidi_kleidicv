// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_OPENCV_H
#define KLEIDICV_OPENCV_H

#include "kleidicv/kleidicv.h"
#include "kleidicv_opencv/config.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/// Internal - not part of the public API and its direct use is not supported.
/// For details see
/// https://github.com/opencv/opencv/blob/4.11.0/modules/video/src/hal_replacement.hpp#L29
KLEIDICV_API_DECLARATION(kleidicv_opencv_optical_flow_u8,
                         const uint8_t *prev_data, size_t prev_data_step,
                         const int16_t *prev_deriv_data, size_t prev_deriv_step,
                         const uint8_t *next_data, size_t next_step, int width,
                         int height, int channels, const float *prev_points,
                         float *next_points, size_t point_count,
                         uint8_t *status, float *err, int window_width,
                         int window_height, int termination_count,
                         double termination_epsilon, bool get_min_eigen_vals,
                         float min_eigen_vals_threshold);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // KLEIDICV_OPENCV_H
