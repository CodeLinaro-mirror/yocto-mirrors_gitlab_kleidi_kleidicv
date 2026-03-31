// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_ANALYSIS_SUM_H
#define KLEIDICV_ANALYSIS_SUM_H

#include "kleidicv/ctypes.h"

namespace kleidicv {

namespace neon {

template <typename T, typename TInternal>
kleidicv_error_t sum(const T *src, size_t src_stride, size_t width,
                     size_t height, T *sum);

}  // namespace neon

namespace sve2 {

template <typename T, typename TInternal>
kleidicv_error_t sum(const T *src, size_t src_stride, size_t width,
                     size_t height, T *sum);

}  // namespace sve2

namespace sme {

template <typename T, typename TInternal>
kleidicv_error_t sum(const T *src, size_t src_stride, size_t width,
                     size_t height, T *sum);

}  // namespace sme

namespace sme2 {

template <typename T, typename TInternal>
kleidicv_error_t sum(const T *src, size_t src_stride, size_t width,
                     size_t height, T *sum);

}  // namespace sme2

}  // namespace kleidicv

#endif  // KLEIDICV_ANALYSIS_SUM_H
