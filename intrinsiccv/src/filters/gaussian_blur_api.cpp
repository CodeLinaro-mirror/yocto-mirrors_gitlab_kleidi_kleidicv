// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/filters/gaussian_blur.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/workspace/separable.h"

namespace intrinsiccv {

extern "C" {

intrinsiccv_error_t intrinsiccv_filter_create(
    intrinsiccv_filter_context_t **context, size_t channels, size_t type_size,
    intrinsiccv_rectangle_t image) {
  CHECK_POINTERS(context);
  CHECK_RECTANGLE_SIZE(image);

  if (type_size > INTRINSICCV_MAXIMUM_TYPE_SIZE) {
    return INTRINSICCV_ERROR_RANGE;
  }

  if (channels > INTRINSICCV_MAXIMUM_CHANNEL_COUNT) {
    return INTRINSICCV_ERROR_RANGE;
  }

  auto workspace =
      SeparableFilterWorkspace::create(Rectangle{image}, channels, type_size);
  if (!workspace) {
    *context = nullptr;
    return INTRINSICCV_ERROR_ALLOCATION;
  }

  *context =
      reinterpret_cast<intrinsiccv_filter_context_t *>(workspace.release());
  return INTRINSICCV_OK;
}

intrinsiccv_error_t intrinsiccv_filter_release(
    intrinsiccv_filter_context_t *context) {
  CHECK_POINTERS(context);

  // Deliberately create and immediately destroy a unique_ptr to delete the
  // workspace.
  // NOLINTBEGIN(bugprone-unused-raii)
  SeparableFilterWorkspace::Pointer{
      reinterpret_cast<SeparableFilterWorkspace *>(context)};
  // NOLINTEND(bugprone-unused-raii)
  return INTRINSICCV_OK;
}

}  // extern "C"

INTRINSICCV_MULTIVERSION_C_API(intrinsiccv_gaussian_blur_3x3_u8,
                               intrinsiccv::neon::gaussian_blur_3x3_u8, nullptr,
                               nullptr, const uint8_t *src, size_t src_stride,
                               uint8_t *dst, size_t dst_stride, size_t width,
                               size_t height, size_t channels,
                               intrinsiccv_border_type_t border_type,
                               intrinsiccv_filter_context_t *context);

INTRINSICCV_MULTIVERSION_C_API(
    intrinsiccv_gaussian_blur_5x5_u8, intrinsiccv::neon::gaussian_blur_5x5_u8,
    INTRINSICCV_SVE2_IMPL_IF(intrinsiccv::sve2::gaussian_blur_5x5_u8),
    intrinsiccv::sme2::gaussian_blur_5x5_u8, const uint8_t *src,
    size_t src_stride, uint8_t *dst, size_t dst_stride, size_t width,
    size_t height, size_t channels, intrinsiccv_border_type_t border_type,
    intrinsiccv_filter_context_t *context);

}  // namespace intrinsiccv
