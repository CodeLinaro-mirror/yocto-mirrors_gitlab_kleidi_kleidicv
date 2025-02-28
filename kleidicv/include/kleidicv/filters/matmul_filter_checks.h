// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_MATMUL_FILTER_CHECKS_H
#define KLEIDICV_MATMUL_FILTER_CHECKS_H

#include <cstddef>

#include "kleidicv/config.h"
#include "kleidicv/kleidicv.h"

namespace KLEIDICV_TARGET_NAMESPACE {
#if KLEIDICV_ENABLE_MOPA_CONVOLUTION
static constexpr size_t kMinKernelSize = 7;

inline bool gaussian_blur_sme2_implementation_checks(size_t kernel_width,
                                                     size_t kernel_height,
                                                     size_t channels) {
  return (kernel_height >= kMinKernelSize) &&
         (kernel_width >= kMinKernelSize) && (channels != 2);
}

inline bool gaussian_blur_sme2_implementation_checks(size_t kernel_width,
                                                     size_t kernel_height) {
  return (kernel_height >= kMinKernelSize) && (kernel_width >= kMinKernelSize);
}

#else
inline bool gaussian_blur_sme2_implementation_checks(size_t, size_t, size_t) {
  return false;
}

inline bool gaussian_blur_sme2_implementation_checks(size_t, size_t) {
  return false;
}
#endif
}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif
