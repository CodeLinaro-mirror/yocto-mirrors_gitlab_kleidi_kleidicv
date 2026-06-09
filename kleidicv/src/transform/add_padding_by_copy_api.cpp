// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/transform/add_padding_by_copy.h"
#include "kleidicv/utils.h"

KLEIDICV_MULTIVERSION_C_API_WITHOUT_SME(
    kleidicv_create_add_padding_by_copy_operation,
    &kleidicv::neon::create_add_padding_by_copy_operation, nullptr)

namespace kleidicv {

static kleidicv_error_t add_padding_by_copy(
    const void *src, size_t src_stride, void *dst, size_t dst_stride,
    size_t src_width, size_t src_height, size_t top_padding,
    size_t bottom_padding, size_t left_padding, size_t right_padding,
    size_t pixel_size, kleidicv_border_type_t border_type,
    const void *border_value) {
  const auto *src_bytes = reinterpret_cast<const uint8_t *>(src);
  auto *dst_bytes = reinterpret_cast<uint8_t *>(dst);

  auto result = add_padding_by_copy_checks(
      src_bytes, src_stride, dst_bytes, dst_stride, src_width, src_height,
      top_padding, bottom_padding, left_padding, right_padding, pixel_size,
      border_type, border_value);
  if (std::holds_alternative<kleidicv_error_t>(result)) {
    return std::get<kleidicv_error_t>(result);
  }
  const size_t dst_height = std::get<size_t>(result);

  const AddPaddingByCopyBorderStrategy strategy = resolve_border_strategy(
      src_width, left_padding, right_padding, border_type);

  const AddPaddingByCopyParameters parameters{
      src_bytes,  dst_bytes,   src_stride,     dst_stride,   src_width,
      src_height, top_padding, bottom_padding, left_padding, right_padding,
      pixel_size, border_type, border_value};

  auto operation =
      kleidicv_create_add_padding_by_copy_operation(parameters, strategy);
  if (!operation) {
    return KLEIDICV_ERROR_ALLOCATION;
  }

  return operation->process_stripe(0, dst_height);
}

}  // namespace kleidicv

extern "C" {

kleidicv_error_t kleidicv_add_padding_by_copy(
    const void *src, size_t src_stride, void *dst, size_t dst_stride,
    size_t src_width, size_t src_height, size_t top_padding,
    size_t bottom_padding, size_t left_padding, size_t right_padding,
    size_t pixel_size, kleidicv_border_type_t border_type,
    const void *border_value) {
  return kleidicv::add_padding_by_copy(
      src, src_stride, dst, dst_stride, src_width, src_height, top_padding,
      bottom_padding, left_padding, right_padding, pixel_size, border_type,
      border_value);
}

}  // extern "C"
