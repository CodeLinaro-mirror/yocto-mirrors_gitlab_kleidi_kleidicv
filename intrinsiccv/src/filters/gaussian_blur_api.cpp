// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/filters/gaussian_blur.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/workspace/separable.h"

extern "C" {

using KLEIDICV_TARGET_NAMESPACE::Rectangle;
using KLEIDICV_TARGET_NAMESPACE::SeparableFilterWorkspace;

intrinsiccv_error_t intrinsiccv_filter_create(
    intrinsiccv_filter_context_t **context, size_t channels, size_t type_size,
    intrinsiccv_rectangle_t image) {
  CHECK_POINTERS(context);
  CHECK_RECTANGLE_SIZE(image);

  if (type_size > KLEIDICV_MAXIMUM_TYPE_SIZE) {
    return KLEIDICV_ERROR_RANGE;
  }

  if (channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT) {
    return KLEIDICV_ERROR_RANGE;
  }

  auto workspace =
      SeparableFilterWorkspace::create(Rectangle{image}, channels, type_size);
  if (!workspace) {
    *context = nullptr;
    return KLEIDICV_ERROR_ALLOCATION;
  }

  *context =
      reinterpret_cast<intrinsiccv_filter_context_t *>(workspace.release());
  return KLEIDICV_OK;
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
  return KLEIDICV_OK;
}

}  // extern "C"

KLEIDICV_MULTIVERSION_C_API(intrinsiccv_gaussian_blur_3x3_u8,
                            &intrinsiccv::neon::gaussian_blur_3x3_u8, nullptr,
                            nullptr);

KLEIDICV_MULTIVERSION_C_API(
    intrinsiccv_gaussian_blur_5x5_u8, &intrinsiccv::neon::gaussian_blur_5x5_u8,
    KLEIDICV_SVE2_IMPL_IF(intrinsiccv::sve2::gaussian_blur_5x5_u8),
    &intrinsiccv::sme2::gaussian_blur_5x5_u8);
