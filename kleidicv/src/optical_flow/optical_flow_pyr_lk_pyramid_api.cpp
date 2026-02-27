// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <cstddef>

#include "kleidicv/optical_flow/optical_flow_lk_pyramid.h"
#include "kleidicv/utils.h"

extern "C" {

using KLEIDICV_TARGET_NAMESPACE::OpticalFlowLKPyramid;

kleidicv_error_t kleidicv_build_optical_flow_pyr_lk_pyramid(
    kleidicv_optical_flow_pyr_lk_pyramid_t** pyramid, const uint8_t* src,
    size_t src_stride, size_t width, size_t height, size_t channels,
    size_t level_count, size_t window_width, size_t window_height) {
  CHECK_POINTERS(pyramid);
  *pyramid = nullptr;

  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  // Validate user-provided parameters.
  if (width == 0 || height == 0 || channels == 0 ||
      channels > KLEIDICV_MAXIMUM_CHANNEL_COUNT || level_count == 0 ||
      window_width <= 2 || window_height <= 2) {
    return KLEIDICV_ERROR_RANGE;
  }
  if (height > 1 && src_stride < (width * channels)) {
    return KLEIDICV_ERROR_RANGE;
  }

  // Current underlying kernels used by this builder only support 1 channel.
  if (channels != 1) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  // Determine how many levels can actually be built.
  size_t actual_level_count = 0;
  size_t level_width = width;
  size_t level_height = height;
  for (size_t requested = 0; requested < level_count; ++requested) {
    ++actual_level_count;
    const size_t next_width = (level_width + 1) / 2;
    const size_t next_height = (level_height + 1) / 2;
    if (next_width <= window_width || next_height <= window_height) {
      break;
    }
    level_width = next_width;
    level_height = next_height;
  }

  OpticalFlowLKPyramid::Pointer pyramid_storage;
  if (kleidicv_error_t err = OpticalFlowLKPyramid::create(
          pyramid_storage, actual_level_count, width, height, channels)) {
    return err;
  }

  if (kleidicv_error_t err = pyramid_storage->process(src, src_stride)) {
    return err;
  }

  *pyramid = reinterpret_cast<kleidicv_optical_flow_pyr_lk_pyramid_t*>(
      pyramid_storage.release());
  return KLEIDICV_OK;
}

kleidicv_error_t kleidicv_optical_flow_pyr_lk_pyramid_release(
    kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid) {
  CHECK_POINTERS(pyramid);
  // Deliberately create and immediately destroy a unique_ptr to delete the
  // opaque pyramid storage.
  // NOLINTBEGIN(bugprone-unused-raii)
  OpticalFlowLKPyramid::Pointer{
      reinterpret_cast<OpticalFlowLKPyramid*>(pyramid)};
  // NOLINTEND(bugprone-unused-raii)
  return KLEIDICV_OK;
}

kleidicv_error_t kleidicv_optical_flow_pyr_lk_pyramid_get_level_count(
    const kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid,
    size_t* level_count) {
  CHECK_POINTERS(pyramid, level_count);
  const auto* typed = reinterpret_cast<const OpticalFlowLKPyramid*>(pyramid);
  *level_count = typed->level_count();
  return KLEIDICV_OK;
}

kleidicv_error_t kleidicv_optical_flow_pyr_lk_pyramid_get_image_level(
    const kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid, size_t level,
    const uint8_t** data, size_t* stride, size_t* width, size_t* height) {
  CHECK_POINTERS(pyramid, data, stride, width, height);
  const auto* typed = reinterpret_cast<const OpticalFlowLKPyramid*>(pyramid);
  if (level >= typed->level_count()) {
    return KLEIDICV_ERROR_RANGE;
  }

  const auto& entry = typed->level(level);
  *data = entry.image_data;
  *stride = entry.image_stride;
  *width = entry.width;
  *height = entry.height;
  return KLEIDICV_OK;
}

kleidicv_error_t kleidicv_optical_flow_pyr_lk_pyramid_get_scharr_level(
    const kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid, size_t level,
    const int16_t** data, size_t* stride, size_t* width, size_t* height) {
  CHECK_POINTERS(pyramid, data, stride, width, height);
  const auto* typed = reinterpret_cast<const OpticalFlowLKPyramid*>(pyramid);
  if (level >= typed->level_count()) {
    return KLEIDICV_ERROR_RANGE;
  }

  const auto& entry = typed->level(level);
  *data = entry.scharr_data;
  *stride = entry.scharr_stride;
  *width = entry.width;
  *height = entry.height;
  return KLEIDICV_OK;
}

}  // extern "C"
