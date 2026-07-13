// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_TRANSFORM_FLIP_H
#define KLEIDICV_TRANSFORM_FLIP_H

#include <cstddef>

#include "kleidicv/ctypes.h"

namespace kleidicv {
namespace neon {
kleidicv_error_t flip(const void *src, size_t src_stride, size_t width,
                      size_t height, void *dst, size_t dst_stride,
                      int flip_mode, size_t pixel_size);
}  // namespace neon

}  // namespace kleidicv

#endif  // KLEIDICV_TRANSFORM_FLIP_H
