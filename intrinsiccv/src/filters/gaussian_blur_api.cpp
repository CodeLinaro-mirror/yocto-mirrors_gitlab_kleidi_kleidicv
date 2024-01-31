// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "dispatch.h"
#include "filters/gaussian_blur.h"
#include "intrinsiccv.h"
#include "workspace/separable.h"

namespace intrinsiccv {

extern "C" {

intrinsiccv_error_t intrinsiccv_filter_create(
    intrinsiccv_filter_params_t *params, intrinsiccv_rectangle_t image) {
  auto workspace = SeparableFilterWorkspace::create(
      Rectangle{image}, params->channels, params->type_size);
  if (!workspace) {
    return INTRINSICCV_ERROR_ALLOCATION;
  }

  params->workspace = reinterpret_cast<void *>(workspace.release());
  return INTRINSICCV_OK;
}

intrinsiccv_error_t intrinsiccv_filter_release(
    intrinsiccv_filter_params_t *params) {
  if (!params->workspace) {
    return INTRINSICCV_ERROR_NULL_POINTER;
  }

  // Deliberately create and immediately destroy a unique_ptr to delete the
  // workspace.
  // NOLINTBEGIN(bugprone-unused-raii)
  SeparableFilterWorkspace::Pointer{
      reinterpret_cast<SeparableFilterWorkspace *>(params->workspace)};
  // NOLINTEND(bugprone-unused-raii)
  params->workspace = nullptr;
  return INTRINSICCV_OK;
}

}  // extern "C"

INTRINSICCV_MULTIVERSION_C_API(intrinsiccv_gaussian_blur_3x3_u8,
                               intrinsiccv::neon::gaussian_blur_3x3_u8, nullptr,
                               nullptr, const uint8_t *src, size_t src_stride,
                               uint8_t *dst, size_t dst_stride, size_t width,
                               size_t height, size_t channels,
                               intrinsiccv_border_type_t border_type,
                               const intrinsiccv_filter_params_t *params);

INTRINSICCV_MULTIVERSION_C_API(
    intrinsiccv_gaussian_blur_5x5_u8, intrinsiccv::neon::gaussian_blur_5x5_u8,
    INTRINSICCV_SVE2_IMPL_IF(intrinsiccv::sve2::gaussian_blur_5x5_u8),
    intrinsiccv::sme2::gaussian_blur_5x5_u8, const uint8_t *src,
    size_t src_stride, uint8_t *dst, size_t dst_stride, size_t width,
    size_t height, size_t channels, intrinsiccv_border_type_t border_type,
    const intrinsiccv_filter_params_t *params);

}  // namespace intrinsiccv
