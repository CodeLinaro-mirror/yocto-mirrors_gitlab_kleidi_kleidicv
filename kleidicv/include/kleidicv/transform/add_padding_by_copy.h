// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_TRANSFORM_ADD_PADDING_BY_COPY_H
#define KLEIDICV_TRANSFORM_ADD_PADDING_BY_COPY_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>
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

// Base operation class
class AddPaddingByCopyBase {
 public:
  explicit AddPaddingByCopyBase(const AddPaddingByCopyParameters &parameters)
      KLEIDICV_STREAMING
      : src_{parameters.src},
        dst_{parameters.dst},
        src_stride_{parameters.src_stride},
        dst_stride_{parameters.dst_stride},
        src_width_{parameters.src_width},
        src_height_{parameters.src_height},
        top_padding_{parameters.top_padding},
        bottom_padding_{parameters.bottom_padding},
        left_padding_{parameters.left_padding},
        right_padding_{parameters.right_padding},
        pixel_size_{parameters.pixel_size},
        dst_width_{parameters.src_width + parameters.left_padding +
                   parameters.right_padding},
        dst_height_{parameters.src_height + parameters.top_padding +
                    parameters.bottom_padding},
        left_size_{parameters.left_padding * parameters.pixel_size},
        inner_size_{parameters.src_width * parameters.pixel_size},
        right_size_{parameters.right_padding * parameters.pixel_size},
        inner_offset_{parameters.left_padding * parameters.pixel_size},
        right_offset_{(parameters.left_padding + parameters.src_width) *
                      parameters.pixel_size} {}

  virtual kleidicv_error_t process_stripe(size_t dst_y_begin,
                                          size_t dst_y_end) const = 0;

  const uint8_t *const src_;
  uint8_t *const dst_;
  const size_t src_stride_;
  const size_t dst_stride_;
  const size_t src_width_;
  const size_t src_height_;
  const size_t top_padding_;
  const size_t bottom_padding_;
  const size_t left_padding_;
  const size_t right_padding_;
  const size_t pixel_size_;
  const size_t dst_width_;
  const size_t dst_height_;
  const size_t left_size_;
  const size_t inner_size_;
  const size_t right_size_;
  const size_t inner_offset_;
  const size_t right_offset_;

 protected:
  ~AddPaddingByCopyBase() = default;

  // Intersect a destination stripe with the source-backed body rows.
  std::pair<size_t, size_t> split_body_rows(
      size_t dst_y_begin, size_t dst_y_end) const KLEIDICV_STREAMING {
    const size_t body_begin_limit = top_padding_;
    const size_t body_end_limit = top_padding_ + src_height_;

    const size_t body_begin =
        std::min(std::max(dst_y_begin, body_begin_limit), dst_y_end);
    const size_t body_end =
        std::max(body_begin, std::min(dst_y_end, body_end_limit));

    return {body_begin, body_end};
  }

  static inline ptrdiff_t positive_remainder(ptrdiff_t value, ptrdiff_t period)
      KLEIDICV_STREAMING {
    ptrdiff_t remainder = value % period;
    if (remainder < 0) {
      remainder += period;
    }
    return remainder;
  }

  static inline size_t wrap_index(ptrdiff_t position,
                                  size_t length) KLEIDICV_STREAMING {
    const ptrdiff_t signed_length = static_cast<ptrdiff_t>(length);
    return static_cast<size_t>(positive_remainder(position, signed_length));
  }

  static inline size_t reflect_index(ptrdiff_t position,
                                     size_t length) KLEIDICV_STREAMING {
    if (length == 1) {
      return 0;
    }

    const ptrdiff_t period = static_cast<ptrdiff_t>(length << 1);
    ptrdiff_t reflected = positive_remainder(position, period);
    if (reflected >= static_cast<ptrdiff_t>(length)) {
      reflected = period - reflected - 1;
    }
    return static_cast<size_t>(reflected);
  }

  static inline size_t reverse_index(ptrdiff_t position,
                                     size_t length) KLEIDICV_STREAMING {
    if (length == 1) {
      return 0;
    }

    const ptrdiff_t period = static_cast<ptrdiff_t>((length - 1) << 1);
    ptrdiff_t reflected = positive_remainder(position, period);
    if (reflected >= static_cast<ptrdiff_t>(length)) {
      reflected = period - reflected;
    }
    return static_cast<size_t>(reflected);
  }

  template <auto MapIndex>
  static void build_coordinate_sequence(size_t *generated_indices, size_t count,
                                        ptrdiff_t start,
                                        size_t length) KLEIDICV_STREAMING {
    for (size_t i = 0; i < count; ++i) {
      generated_indices[i] =
          MapIndex(start + static_cast<ptrdiff_t>(i), length);
    }
  }

  template <auto MapIndex>
  static void build_vertical_border_rows(size_t *generated_top_rows,
                                         size_t top_padding,
                                         size_t *generated_bottom_rows,
                                         size_t bottom_padding,
                                         size_t height) KLEIDICV_STREAMING {
    build_coordinate_sequence<MapIndex>(generated_top_rows, top_padding,
                                        -static_cast<ptrdiff_t>(top_padding),
                                        height);
    build_coordinate_sequence<MapIndex>(generated_bottom_rows, bottom_padding,
                                        static_cast<ptrdiff_t>(height), height);
  }

  template <auto MapIndex>
  static void build_horizontal_border_indices(size_t *generated_border_indices,
                                              size_t left, size_t right,
                                              size_t width) KLEIDICV_STREAMING {
    build_coordinate_sequence<MapIndex>(generated_border_indices, left,
                                        -static_cast<ptrdiff_t>(left), width);
    build_coordinate_sequence<MapIndex>(generated_border_indices + left, right,
                                        static_cast<ptrdiff_t>(width), width);
  }

  template <typename FillRow>
  void process_vertical_mapped_stripe(
      size_t body_begin, size_t body_end, const size_t *top_rows,
      const size_t *bottom_rows, size_t dst_y_begin, size_t dst_y_end,
      FillRow fill_row) const KLEIDICV_STREAMING {
    uint8_t *dst_row = dst_ + dst_y_begin * dst_stride_;

    for (size_t dst_y = dst_y_begin; dst_y < body_begin;
         ++dst_y, dst_row += dst_stride_) {
      fill_row(src_ + top_rows[dst_y] * src_stride_, dst_row);
    }

    if (body_begin < body_end) {
      const uint8_t *src_row = src_ + (body_begin - top_padding_) * src_stride_;
      for (size_t dst_y = body_begin; dst_y < body_end;
           ++dst_y, dst_row += dst_stride_, src_row += src_stride_) {
        fill_row(src_row, dst_row);
      }
    }

    const size_t bottom_base = top_padding_ + src_height_;
    for (size_t dst_y = body_end; dst_y < dst_y_end;
         ++dst_y, dst_row += dst_stride_) {
      fill_row(src_ + bottom_rows[dst_y - bottom_base] * src_stride_, dst_row);
    }
  }
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

using CreateAddPaddingByCopyOperation =
    AddPaddingByCopyOpPointer (*)(AddPaddingByCopyParameters parameters,
                                  AddPaddingByCopyBorderStrategy strategy);

}  // namespace kleidicv

extern "C" {
extern kleidicv::CreateAddPaddingByCopyOperation
    kleidicv_create_add_padding_by_copy_operation;
}

#endif  // KLEIDICV_TRANSFORM_ADD_PADDING_BY_COPY_H
