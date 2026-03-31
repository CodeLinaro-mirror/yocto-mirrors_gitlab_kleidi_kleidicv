// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_ANALYSIS_MIN_MAX_H
#define KLEIDICV_ANALYSIS_MIN_MAX_H

#include "kleidicv/ctypes.h"

namespace kleidicv {

namespace neon {

template <typename T>
kleidicv_error_t min_max(const T *src, size_t src_stride, size_t width,
                         size_t height, T *min_value, T *max_value);

template <typename T>
kleidicv_error_t min_max_loc(const T *src, size_t src_stride, size_t width,
                             size_t height, size_t *min_offset,
                             size_t *max_offset);

}  // namespace neon

namespace sve2 {

template <typename T>
kleidicv_error_t min_max(const T *src, size_t src_stride, size_t width,
                         size_t height, T *min_value, T *max_value);
}  // namespace sve2

namespace sme {

template <typename T>
kleidicv_error_t min_max(const T *src, size_t src_stride, size_t width,
                         size_t height, T *min_value, T *max_value);
}  // namespace sme

namespace sme2 {

template <typename T>
kleidicv_error_t min_max(const T *src, size_t src_stride, size_t width,
                         size_t height, T *min_value, T *max_value);
}  // namespace sme2

}  // namespace kleidicv

#endif  // KLEIDICV_ANALYSIS_MIN_MAX_H
