// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/filters/gaussian_blur.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/workspace/separable.h"

extern "C" {

using KLEIDICV_TARGET_NAMESPACE::Rectangle;
using KLEIDICV_TARGET_NAMESPACE::SeparableFilterWorkspace;

kleidicv_error_t kleidicv_filter_context_create(
    kleidicv_filter_context_t **context, size_t max_channels,
    size_t max_kernel_width, size_t max_kernel_height, size_t max_image_width,
    size_t max_image_height) {
  CHECK_POINTERS(context);

  if (max_kernel_width != max_kernel_height) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (max_channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT) {
    return KLEIDICV_ERROR_RANGE;
  }

  CHECK_IMAGE_SIZE(max_image_width, max_image_height);

  // naive check because non-square kernels are not supported anyway
  size_t intermediate_size = (max_kernel_width == 15 || max_kernel_height == 15)
                                 ? sizeof(uint32_t)
                                 : sizeof(uint16_t);
  auto workspace = SeparableFilterWorkspace::create(
      Rectangle{max_image_width, max_image_height}, max_channels,
      intermediate_size);
  if (!workspace) {
    *context = nullptr;
    return KLEIDICV_ERROR_ALLOCATION;
  }

  *context = reinterpret_cast<kleidicv_filter_context_t *>(workspace.release());
  return KLEIDICV_OK;
}

kleidicv_error_t kleidicv_filter_context_release(
    kleidicv_filter_context_t *context) {
  CHECK_POINTERS(context);

  // Deliberately create and immediately destroy a unique_ptr to delete the
  // workspace.
  // NOLINTBEGIN(bugprone-unused-raii)
  SeparableFilterWorkspace::Pointer{
      reinterpret_cast<SeparableFilterWorkspace *>(context)};
  // NOLINTEND(bugprone-unused-raii)
  return KLEIDICV_OK;
}

}  // extern "C"

KLEIDICV_MULTIVERSION_C_API(
    kleidicv_gaussian_blur_u8, &kleidicv::neon::gaussian_blur_u8,
    KLEIDICV_SVE2_IMPL_IF(kleidicv::sve2::gaussian_blur_u8),
    &kleidicv::sme2::gaussian_blur_u8);
