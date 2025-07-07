// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/ctypes.h"
#include "kleidicv/filters/gaussian_blur.h"
#include "kleidicv/utils.h"
#include "kleidicv/workspace/separable.h"

// Copied from kleidicv/src/filters/separable_filter_2d_api.cpp
static kleidicv_error_t filter_context_create(
    kleidicv_filter_context_t **context, size_t max_channels,
    size_t max_kernel_width, size_t max_kernel_height, size_t max_image_width,
    size_t max_image_height) {
  CHECK_POINTERS(context);

  if (max_kernel_width != max_kernel_height) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (max_channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  CHECK_IMAGE_SIZE(max_image_width, max_image_height);

  // As we cannot predict the intermediate size based on the parameters given,
  // just use the largest possible size out of all available operations.
  constexpr size_t intermediate_size = sizeof(uint32_t);
  auto workspace = kleidicv::sme2::SeparableFilterWorkspace::create(
      kleidicv::sme2::Rectangle{max_image_width, max_image_height},
      max_channels, intermediate_size);
  if (!workspace) {
    *context = nullptr;
    return KLEIDICV_ERROR_ALLOCATION;
  }

  *context = reinterpret_cast<kleidicv_filter_context_t *>(workspace.release());
  return KLEIDICV_OK;
}

// Copied from kleidicv/src/filters/separable_filter_2d_api.cpp
static kleidicv_error_t filter_context_release(
    kleidicv_filter_context_t *context) {
  CHECK_POINTERS(context);

  // Deliberately create and immediately destroy a unique_ptr to delete the
  // workspace.
  // NOLINTBEGIN(bugprone-unused-raii)
  kleidicv::sme2::SeparableFilterWorkspace::Pointer{
      reinterpret_cast<kleidicv::sme2::SeparableFilterWorkspace *>(context)};
  // NOLINTEND(bugprone-unused-raii)
  return KLEIDICV_OK;
}

extern "C" {

// Implemented based on kleidicv_gaussian_blur_u8 function (placed in
// kleidicv/src/filters/gaussian_blur_api.cpp), but the filter context is
// created (no need to pass it as a an input) and the SME backend is called
// directly. (Original implementation calls the dispatcher to choose between
// backends.)
kleidicv_error_t sme_gaussian_blur_u8(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height,
                                      size_t channels, size_t kernel_width,
                                      size_t kernel_height, float sigma_x,
                                      float sigma_y,
                                      kleidicv_border_type_t border_type) {
  auto fixed_border_type = kleidicv::get_fixed_border_type(border_type);
  if (!fixed_border_type) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (!kleidicv::gaussian_blur_is_implemented(width, height, kernel_width,
                                              kernel_height, sigma_x, sigma_y,
                                              channels, *fixed_border_type)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (kernel_width <= 7 || kernel_width == 15 || kernel_width == 21) {
    kleidicv_filter_context_t *context = nullptr;
    if (kleidicv_error_t create_err = filter_context_create(
            &context, channels, kernel_width, kernel_height, width, height)) {
      return create_err;
    }

    kleidicv_error_t blur_err = kleidicv::sme2::gaussian_blur_fixed_stripe_u8(
        src, src_stride, dst, dst_stride, width, height, 0, height, channels,
        kernel_width, kernel_height, sigma_x, sigma_y, *fixed_border_type,
        context);

    kleidicv_error_t release_err = filter_context_release(context);
    return blur_err ? blur_err : release_err;
  }

  return KLEIDICV_ERROR_NOT_IMPLEMENTED;
}

}  // extern "C"
