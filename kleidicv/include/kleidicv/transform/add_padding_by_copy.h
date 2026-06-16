// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_TRANSFORM_ADD_PADDING_BY_COPY_H
#define KLEIDICV_TRANSFORM_ADD_PADDING_BY_COPY_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <variant>

#include "kleidicv/kleidicv.h"
#include "kleidicv/utils.h"

namespace kleidicv {
inline bool add_padding_by_copy_is_implemented(
    size_t src_width, size_t src_height, size_t pixel_size,
    kleidicv_border_type_t border_type) KLEIDICV_STREAMING {
  constexpr size_t kMaxPixelSize =
      static_cast<size_t>(KLEIDICV_MAXIMUM_TYPE_SIZE) *
      KLEIDICV_MAXIMUM_CHANNEL_COUNT;

  if (pixel_size == 0 || pixel_size > kMaxPixelSize) {
    return false;
  }

  if (border_type != KLEIDICV_BORDER_TYPE_CONSTANT &&
      border_type != KLEIDICV_BORDER_TYPE_REPLICATE &&
      border_type != KLEIDICV_BORDER_TYPE_REFLECT &&
      border_type != KLEIDICV_BORDER_TYPE_WRAP &&
      border_type != KLEIDICV_BORDER_TYPE_REVERSE) {
    return false;
  }

  if (border_type != KLEIDICV_BORDER_TYPE_CONSTANT &&
      (src_width == 0 || src_height == 0)) {
    return false;
  }

  return true;
}

using AddPaddingByCopyChecksResult = std::variant<kleidicv_error_t, size_t>;

template <typename T, typename Pointer>
inline kleidicv_error_t add_padding_by_copy_check_pointer_and_stride(
    Pointer pointer, size_t stride, size_t height) KLEIDICV_STREAMING {
  using ElementType =
      std::conditional_t<std::is_const_v<std::remove_pointer_t<Pointer>>,
                         const T, T>;
  MAKE_POINTER_CHECK_ALIGNMENT(ElementType, data, pointer);
  CHECK_POINTER_AND_STRIDE(data, stride, height);
  return KLEIDICV_OK;
}

template <typename Pointer>
inline kleidicv_error_t add_padding_by_copy_check_pointer_and_stride(
    Pointer pointer, size_t stride, size_t height,
    size_t pixel_size) KLEIDICV_STREAMING {
  switch (pixel_size) {
    case sizeof(uint16_t):
    case 3 * sizeof(uint16_t):
      return add_padding_by_copy_check_pointer_and_stride<uint16_t>(
          pointer, stride, height);
    case sizeof(uint32_t):
    case 3 * sizeof(uint32_t):
      return add_padding_by_copy_check_pointer_and_stride<uint32_t>(
          pointer, stride, height);
    case sizeof(uint64_t):
      return add_padding_by_copy_check_pointer_and_stride<uint64_t>(
          pointer, stride, height);
    default:
      return add_padding_by_copy_check_pointer_and_stride<uint8_t>(
          pointer, stride, height);
  }
}

inline AddPaddingByCopyChecksResult add_padding_by_copy_checks(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t src_width, size_t src_height, size_t top_padding,
    size_t bottom_padding, size_t left_padding, size_t right_padding,
    size_t pixel_size, kleidicv_border_type_t border_type,
    const void *border_value) KLEIDICV_STREAMING {
  CHECK_IMAGE_SIZE(src_width, src_height);

  if (!add_padding_by_copy_is_implemented(src_width, src_height, pixel_size,
                                          border_type)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  if (kleidicv_error_t ptr_stride_error =
          add_padding_by_copy_check_pointer_and_stride(
              src, src_stride, src_height, pixel_size)) {
    return ptr_stride_error;
  }

  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT &&
      KLEIDICV_TARGET_NAMESPACE::any_null(border_value)) {
    return KLEIDICV_ERROR_NULL_POINTER;
  }

  constexpr size_t kMaxImageDimension = KLEIDICV_MAX_IMAGE_PIXELS;
  if (src_width > kMaxImageDimension || src_height > kMaxImageDimension ||
      top_padding > kMaxImageDimension || bottom_padding > kMaxImageDimension ||
      left_padding > kMaxImageDimension || right_padding > kMaxImageDimension) {
    return KLEIDICV_ERROR_RANGE;
  }

  const size_t horizontal_padding = left_padding + right_padding;
  const size_t vertical_padding = top_padding + bottom_padding;
  if (horizontal_padding > kMaxImageDimension ||
      vertical_padding > kMaxImageDimension) {
    return KLEIDICV_ERROR_RANGE;
  }
  if (src_width > kMaxImageDimension - horizontal_padding ||
      src_height > kMaxImageDimension - vertical_padding) {
    return KLEIDICV_ERROR_RANGE;
  }

  const size_t dst_width = src_width + horizontal_padding;
  const size_t computed_dst_height = src_height + vertical_padding;
  CHECK_IMAGE_SIZE(dst_width, computed_dst_height);

  if (kleidicv_error_t ptr_stride_error =
          add_padding_by_copy_check_pointer_and_stride(
              dst, dst_stride, computed_dst_height, pixel_size)) {
    return ptr_stride_error;
  }

  return computed_dst_height;
}

enum class AddPaddingByCopyBorderStrategy {
  kConstant,
  kReplicate,
  kWrapFast,
  kReflectFast,
  kReverseFast,
  kIndexed,
};

// Inputs shared by every operation constructor.
struct AddPaddingByCopyParameters {
  const uint8_t *src;
  uint8_t *dst;
  size_t src_stride;
  size_t dst_stride;
  size_t src_width;
  size_t src_height;
  size_t top_padding;
  size_t bottom_padding;
  size_t left_padding;
  size_t right_padding;
  size_t pixel_size;
  kleidicv_border_type_t border_type;
  const void *border_value;
};

// Public operation interface. Keep this target-agnostic so the
// multiversion C API can pass operation pointers across dispatch code without
// binding the public type to one target namespace or streaming-mode linkage.
class AddPaddingByCopyBase {
 public:
  virtual kleidicv_error_t process_stripe(size_t dst_y_begin,
                                          size_t dst_y_end) const = 0;

 protected:
  ~AddPaddingByCopyBase() = default;
};

struct AddPaddingByCopyBaseDeleter {
  void operator()(AddPaddingByCopyBase *state) const { std::free(state); }
};

using AddPaddingByCopyOpPointer =
    std::unique_ptr<AddPaddingByCopyBase, AddPaddingByCopyBaseDeleter>;

// WRAP can use the fast row-copy path when both horizontal borders fit within
// one source row.
inline bool can_use_wrap_fast_path(size_t src_width, size_t left_padding,
                                   size_t right_padding) KLEIDICV_STREAMING {
  return left_padding <= src_width && right_padding <= src_width;
}

// REFLECT can use the fast reverse-copy path for multi-pixel rows when both
// horizontal borders fit within one source row.
inline bool can_use_reflect_fast_path(size_t src_width, size_t left_padding,
                                      size_t right_padding) KLEIDICV_STREAMING {
  return src_width > 1 && left_padding <= src_width &&
         right_padding <= src_width;
}

// REVERSE can use the fast reverse-copy path for multi-pixel rows when neither
// horizontal border reaches past the opposite edge.
inline bool can_use_reverse_fast_path(size_t src_width, size_t left_padding,
                                      size_t right_padding) KLEIDICV_STREAMING {
  return src_width > 1 && left_padding < src_width && right_padding < src_width;
}

// Map the public border mode and padding shape to the execution strategy.
// WRAP/REFLECT/REVERSE use their fast row-copy paths when possible and fall
// back to the indexed path when the requested padding exceeds those limits.
inline AddPaddingByCopyBorderStrategy resolve_border_strategy(
    size_t src_width, size_t left_padding, size_t right_padding,
    kleidicv_border_type_t border_type) KLEIDICV_STREAMING {
  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT) {
    return AddPaddingByCopyBorderStrategy::kConstant;
  }

  if (border_type == KLEIDICV_BORDER_TYPE_REPLICATE) {
    return AddPaddingByCopyBorderStrategy::kReplicate;
  }

  if (border_type == KLEIDICV_BORDER_TYPE_WRAP &&
      can_use_wrap_fast_path(src_width, left_padding, right_padding)) {
    return AddPaddingByCopyBorderStrategy::kWrapFast;
  }

  if (border_type == KLEIDICV_BORDER_TYPE_REFLECT &&
      can_use_reflect_fast_path(src_width, left_padding, right_padding)) {
    return AddPaddingByCopyBorderStrategy::kReflectFast;
  }

  if (border_type == KLEIDICV_BORDER_TYPE_REVERSE &&
      can_use_reverse_fast_path(src_width, left_padding, right_padding)) {
    return AddPaddingByCopyBorderStrategy::kReverseFast;
  }

  return AddPaddingByCopyBorderStrategy::kIndexed;
}

namespace neon {

AddPaddingByCopyOpPointer create_add_padding_by_copy_operation(
    AddPaddingByCopyParameters parameters,
    AddPaddingByCopyBorderStrategy strategy);

}  // namespace neon

namespace sve2 {

AddPaddingByCopyOpPointer create_add_padding_by_copy_operation(
    AddPaddingByCopyParameters parameters,
    AddPaddingByCopyBorderStrategy strategy);

}  // namespace sve2

namespace sme {

AddPaddingByCopyOpPointer create_add_padding_by_copy_operation(
    AddPaddingByCopyParameters parameters,
    AddPaddingByCopyBorderStrategy strategy);

}  // namespace sme

namespace sme2 {

AddPaddingByCopyOpPointer create_add_padding_by_copy_operation(
    AddPaddingByCopyParameters parameters,
    AddPaddingByCopyBorderStrategy strategy);

}  // namespace sme2

using CreateAddPaddingByCopyOperation =
    AddPaddingByCopyOpPointer (*)(AddPaddingByCopyParameters parameters,
                                  AddPaddingByCopyBorderStrategy strategy);

}  // namespace kleidicv

extern "C" {
extern kleidicv::CreateAddPaddingByCopyOperation
    kleidicv_create_add_padding_by_copy_operation;
}

#endif  // KLEIDICV_TRANSFORM_ADD_PADDING_BY_COPY_H
