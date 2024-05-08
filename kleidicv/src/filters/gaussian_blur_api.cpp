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

kleidicv_error_t kleidicv_filter_create(kleidicv_filter_context_t **context,
                                        size_t channels,
                                        size_t intermediate_size,
                                        kleidicv_rectangle_t image) {
  CHECK_POINTERS(context);
  CHECK_RECTANGLE_SIZE(image);

  if (intermediate_size > KLEIDICV_MAXIMUM_TYPE_SIZE) {
    return KLEIDICV_ERROR_RANGE;
  }

  if (channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT) {
    return KLEIDICV_ERROR_RANGE;
  }

  auto workspace = SeparableFilterWorkspace::create(Rectangle{image}, channels,
                                                    intermediate_size);
  if (!workspace) {
    *context = nullptr;
    return KLEIDICV_ERROR_ALLOCATION;
  }

  *context = reinterpret_cast<kleidicv_filter_context_t *>(workspace.release());
  return KLEIDICV_OK;
}

kleidicv_error_t kleidicv_filter_release(kleidicv_filter_context_t *context) {
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
    kleidicv_gaussian_blur_3x3_u8, &kleidicv::neon::gaussian_blur_3x3_u8,
    KLEIDICV_SVE2_IMPL_IF(kleidicv::sve2::gaussian_blur_3x3_u8),
    &kleidicv::sme2::gaussian_blur_3x3_u8);

KLEIDICV_MULTIVERSION_C_API(
    kleidicv_gaussian_blur_5x5_u8, &kleidicv::neon::gaussian_blur_5x5_u8,
    KLEIDICV_SVE2_IMPL_IF(kleidicv::sve2::gaussian_blur_5x5_u8),
    &kleidicv::sme2::gaussian_blur_5x5_u8);
