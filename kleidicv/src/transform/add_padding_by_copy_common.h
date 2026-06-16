// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_SRC_TRANSFORM_ADD_PADDING_BY_COPY_COMMON_H
#define KLEIDICV_SRC_TRANSFORM_ADD_PADDING_BY_COPY_COMMON_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <utility>

#if defined(KLEIDICV_TARGET_NEON) && KLEIDICV_TARGET_NEON
#include "kleidicv/neon.h"
#else
#include "kleidicv/sve2.h"
#endif
#include "kleidicv/transform/add_padding_by_copy.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// GCC 14+ rejects placing these template-generated virtual overrides in
// the same custom target section as non-COMDAT entry points. Leaving the
// section attribute off lets GCC emit each specialization in its normal
// COMDAT text section, while older GCC and Clang keep the target section.
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 14)
#define KLEIDICV_ADD_PADDING_BY_COPY_FN_ATTRS
#else
#define KLEIDICV_ADD_PADDING_BY_COPY_FN_ATTRS KLEIDICV_TARGET_FN_ATTRS
#endif

#if defined(KLEIDICV_TARGET_NEON) && KLEIDICV_TARGET_NEON
using AddPaddingByCopyContext = NeonContextType;
#define KLEIDICV_ADD_PADDING_BY_COPY_CONTEXT(PixelSize, ContextName) \
  AddPaddingByCopyContext ContextName {}
#else
using AddPaddingByCopyContext = Context;
#define KLEIDICV_ADD_PADDING_BY_COPY_CONTEXT(PixelSize, ContextName) \
  svbool_t ContextName##_predicate;                                  \
  AddPaddingByCopyContext ContextName{ContextName##_predicate};      \
  ContextName.set_predicate(VecTraits<uint8_t>::svptrue())
#endif

// Target-local operation base. The public AddPaddingByCopyBase stays a
// target-agnostic interface, while this base carries the state and helpers that
// use KLEIDICV_STREAMING after KLEIDICV_TARGET_NAMESPACE selects the backend.
// This keeps SME multiversion C dispatch from exposing target-specific linkage
// through the public operation pointer type.
class AddPaddingByCopyTargetBase : public AddPaddingByCopyBase {
 protected:
  explicit AddPaddingByCopyTargetBase(
      const AddPaddingByCopyParameters &parameters) KLEIDICV_STREAMING
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

  ~AddPaddingByCopyTargetBase() = default;

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

template <size_t PixelSize, typename Backend>
class AddPaddingByCopyConstant final : public AddPaddingByCopyTargetBase {
 public:
  static size_t storage_word_count(const AddPaddingByCopyParameters &) {
    return 0;
  }

  AddPaddingByCopyConstant(const AddPaddingByCopyParameters &parameters,
                           size_t *)
      : AddPaddingByCopyTargetBase{parameters},
        border_value_{
            reinterpret_cast<const uint8_t *>(parameters.border_value)},
        row_size_{dst_width_ * pixel_size_} {}

  KLEIDICV_LOCALLY_STREAMING kleidicv_error_t
  process_stripe(size_t dst_y_begin, size_t dst_y_end) const override;

 private:
  void fill_constant_rows(size_t dst_y_begin, size_t dst_y_end,
                          AddPaddingByCopyContext &context) const
      KLEIDICV_STREAMING {
    uint8_t *dst_row = dst_ + dst_y_begin * dst_stride_;
    for (size_t dst_y = dst_y_begin; dst_y < dst_y_end;
         ++dst_y, dst_row += dst_stride_) {
      Backend::fill_repeated_pixel(pixel_size_, border_value_, dst_row,
                                   row_size_, context);
    }
  }

  void process_constant_body_rows(size_t dst_y_begin, size_t dst_y_end,
                                  AddPaddingByCopyContext &context) const
      KLEIDICV_STREAMING {
    uint8_t *dst_row = dst_ + dst_y_begin * dst_stride_;
    const uint8_t *src_row = src_ + (dst_y_begin - top_padding_) * src_stride_;
    for (size_t dst_y = dst_y_begin; dst_y < dst_y_end;
         ++dst_y, dst_row += dst_stride_, src_row += src_stride_) {
      fill_constant_body_row(src_row, dst_row, context);
    }
  }

  void fill_constant_body_row(const uint8_t *src_row, uint8_t *dst_row,
                              AddPaddingByCopyContext &context) const
      KLEIDICV_STREAMING {
    Backend::fill_repeated_pixel(pixel_size_, border_value_, dst_row,
                                 left_size_, context);
    Backend::copy_contiguous_bytes(src_row, dst_row + inner_offset_,
                                   inner_size_, context);
    Backend::fill_repeated_pixel(pixel_size_, border_value_,
                                 dst_row + right_offset_, right_size_, context);
  }

  const uint8_t *const border_value_;
  const size_t row_size_;
};

template <size_t PixelSize, typename Backend>
class AddPaddingByCopyReplicate final : public AddPaddingByCopyTargetBase {
 public:
  static size_t storage_word_count(const AddPaddingByCopyParameters &) {
    return 0;
  }

  AddPaddingByCopyReplicate(const AddPaddingByCopyParameters &parameters,
                            size_t *)
      : AddPaddingByCopyTargetBase{parameters} {}

  KLEIDICV_LOCALLY_STREAMING kleidicv_error_t
  process_stripe(size_t dst_y_begin, size_t dst_y_end) const override;

 private:
  void fill_replicate_rows(
      const uint8_t *src_row, size_t dst_y_begin, size_t dst_y_end,
      AddPaddingByCopyContext &context) const KLEIDICV_STREAMING {
    uint8_t *dst_row = dst_ + dst_y_begin * dst_stride_;
    for (size_t dst_y = dst_y_begin; dst_y < dst_y_end;
         ++dst_y, dst_row += dst_stride_) {
      fill_replicate_row(src_row, dst_row, context);
    }
  }

  void fill_replicate_top_rows(size_t dst_y_begin, size_t dst_y_end,
                               AddPaddingByCopyContext &context) const
      KLEIDICV_STREAMING {
    fill_replicate_rows(src_, dst_y_begin, dst_y_end, context);
  }

  void fill_replicate_bottom_rows(size_t dst_y_begin, size_t dst_y_end,
                                  AddPaddingByCopyContext &context) const
      KLEIDICV_STREAMING {
    fill_replicate_rows(src_ + (src_height_ - 1) * src_stride_, dst_y_begin,
                        dst_y_end, context);
  }

  void process_replicate_body_rows(size_t dst_y_begin, size_t dst_y_end,
                                   AddPaddingByCopyContext &context) const
      KLEIDICV_STREAMING {
    uint8_t *dst_row = dst_ + dst_y_begin * dst_stride_;
    const uint8_t *src_row = src_ + (dst_y_begin - top_padding_) * src_stride_;
    for (size_t dst_y = dst_y_begin; dst_y < dst_y_end;
         ++dst_y, dst_row += dst_stride_, src_row += src_stride_) {
      fill_replicate_row(src_row, dst_row, context);
    }
  }

  void fill_replicate_row(const uint8_t *src_row, uint8_t *dst_row,
                          AddPaddingByCopyContext &context) const
      KLEIDICV_STREAMING {
    const uint8_t *const leftmost_src_pixel = src_row;
    const uint8_t *const rightmost_src_pixel =
        src_row + inner_size_ - pixel_size_;
    Backend::fill_repeated_pixel(pixel_size_, leftmost_src_pixel, dst_row,
                                 left_size_, context);
    Backend::copy_contiguous_bytes(src_row, dst_row + inner_offset_,
                                   inner_size_, context);
    Backend::fill_repeated_pixel(pixel_size_, rightmost_src_pixel,
                                 dst_row + right_offset_, right_size_, context);
  }
};

template <size_t PixelSize, typename Backend>
class AddPaddingByCopyReflect final : public AddPaddingByCopyTargetBase {
 public:
  static size_t storage_word_count(
      const AddPaddingByCopyParameters &parameters) {
    return parameters.top_padding + parameters.bottom_padding;
  }

  AddPaddingByCopyReflect(const AddPaddingByCopyParameters &parameters,
                          size_t *storage)
      : AddPaddingByCopyTargetBase{parameters},
        top_rows_{storage},
        bottom_rows_{storage + top_padding_} {
    build_vertical_border_rows<reflect_index>(storage, top_padding_,
                                              storage + top_padding_,
                                              bottom_padding_, src_height_);
  }

  KLEIDICV_LOCALLY_STREAMING kleidicv_error_t
  process_stripe(size_t dst_y_begin, size_t dst_y_end) const override;

 private:
  void fill_row(const uint8_t *src_row, uint8_t *dst_row,
                AddPaddingByCopyContext &context) const KLEIDICV_STREAMING {
    const uint8_t *const left_border_last_src_pixel =
        src_row + (left_size_ == 0 ? 0 : left_size_ - pixel_size_);
    Backend::copy_reversed_pixels(pixel_size_, left_border_last_src_pixel,
                                  dst_row, left_size_, context);

    Backend::copy_contiguous_bytes(src_row, dst_row + inner_offset_,
                                   inner_size_, context);

    const uint8_t *const rightmost_src_pixel =
        src_row + (right_size_ == 0 ? 0 : inner_size_ - pixel_size_);
    Backend::copy_reversed_pixels(pixel_size_, rightmost_src_pixel,
                                  dst_row + right_offset_, right_size_,
                                  context);
  }

  void process_mapped_stripe(
      size_t body_begin, size_t body_end, size_t dst_y_begin, size_t dst_y_end,
      AddPaddingByCopyContext &context) const KLEIDICV_STREAMING {
    process_vertical_mapped_stripe(
        body_begin, body_end, top_rows_, bottom_rows_, dst_y_begin, dst_y_end,
        [&](const uint8_t *src_row, uint8_t *dst_row)
            KLEIDICV_STREAMING { fill_row(src_row, dst_row, context); });
  }

  const size_t *const top_rows_;
  const size_t *const bottom_rows_;
};

template <size_t PixelSize, typename Backend>
class AddPaddingByCopyReverse final : public AddPaddingByCopyTargetBase {
 public:
  static size_t storage_word_count(
      const AddPaddingByCopyParameters &parameters) {
    return parameters.top_padding + parameters.bottom_padding;
  }

  AddPaddingByCopyReverse(const AddPaddingByCopyParameters &parameters,
                          size_t *storage)
      : AddPaddingByCopyTargetBase{parameters},
        top_rows_{storage},
        bottom_rows_{storage + top_padding_} {
    build_vertical_border_rows<reverse_index>(storage, top_padding_,
                                              storage + top_padding_,
                                              bottom_padding_, src_height_);
  }

  KLEIDICV_LOCALLY_STREAMING kleidicv_error_t
  process_stripe(size_t dst_y_begin, size_t dst_y_end) const override;

 private:
  void fill_row(const uint8_t *src_row, uint8_t *dst_row,
                AddPaddingByCopyContext &context) const KLEIDICV_STREAMING {
    const size_t two_pixel_size = pixel_size_ + pixel_size_;

    const uint8_t *const left_border_last_src_pixel =
        src_row + (left_size_ == 0 ? 0 : left_size_);
    Backend::copy_reversed_pixels(pixel_size_, left_border_last_src_pixel,
                                  dst_row, left_size_, context);

    Backend::copy_contiguous_bytes(src_row, dst_row + inner_offset_,
                                   inner_size_, context);

    const uint8_t *const right_border_last_src_pixel =
        src_row + (right_size_ == 0 ? 0 : inner_size_ - two_pixel_size);
    Backend::copy_reversed_pixels(pixel_size_, right_border_last_src_pixel,
                                  dst_row + right_offset_, right_size_,
                                  context);
  }

  void process_mapped_stripe(
      size_t body_begin, size_t body_end, size_t dst_y_begin, size_t dst_y_end,
      AddPaddingByCopyContext &context) const KLEIDICV_STREAMING {
    process_vertical_mapped_stripe(
        body_begin, body_end, top_rows_, bottom_rows_, dst_y_begin, dst_y_end,
        [&](const uint8_t *src_row, uint8_t *dst_row)
            KLEIDICV_STREAMING { fill_row(src_row, dst_row, context); });
  }

  const size_t *const top_rows_;
  const size_t *const bottom_rows_;
};

template <size_t PixelSize, typename Backend>
class AddPaddingByCopyWrap final : public AddPaddingByCopyTargetBase {
 public:
  static size_t storage_word_count(
      const AddPaddingByCopyParameters &parameters) {
    return parameters.top_padding + parameters.bottom_padding;
  }

  AddPaddingByCopyWrap(const AddPaddingByCopyParameters &parameters,
                       size_t *storage)
      : AddPaddingByCopyTargetBase{parameters},
        top_rows_{storage},
        bottom_rows_{storage + top_padding_} {
    build_vertical_border_rows<wrap_index>(storage, top_padding_,
                                           storage + top_padding_,
                                           bottom_padding_, src_height_);
  }

  KLEIDICV_LOCALLY_STREAMING kleidicv_error_t
  process_stripe(size_t dst_y_begin, size_t dst_y_end) const override;

 private:
  void fill_row(const uint8_t *src_row, uint8_t *dst_row,
                AddPaddingByCopyContext &context) const KLEIDICV_STREAMING {
    const uint8_t *const left_border_src = src_row + inner_size_ - left_size_;
    const uint8_t *const right_border_src = src_row;
    Backend::copy_contiguous_bytes(left_border_src, dst_row, left_size_,
                                   context);
    Backend::copy_contiguous_bytes(src_row, dst_row + inner_offset_,
                                   inner_size_, context);
    Backend::copy_contiguous_bytes(right_border_src, dst_row + right_offset_,
                                   right_size_, context);
  }

  void process_mapped_stripe(
      size_t body_begin, size_t body_end, size_t dst_y_begin, size_t dst_y_end,
      AddPaddingByCopyContext &context) const KLEIDICV_STREAMING {
    process_vertical_mapped_stripe(
        body_begin, body_end, top_rows_, bottom_rows_, dst_y_begin, dst_y_end,
        [&](const uint8_t *src_row, uint8_t *dst_row)
            KLEIDICV_STREAMING { fill_row(src_row, dst_row, context); });
  }

  const size_t *const top_rows_;
  const size_t *const bottom_rows_;
};

template <size_t PixelSize, typename Backend>
class AddPaddingByCopyIndexed final : public AddPaddingByCopyTargetBase {
 public:
  static size_t storage_word_count(
      const AddPaddingByCopyParameters &parameters) {
    return parameters.left_padding + parameters.right_padding +
           parameters.top_padding + parameters.bottom_padding;
  }

  AddPaddingByCopyIndexed(const AddPaddingByCopyParameters &parameters,
                          size_t *storage)
      : AddPaddingByCopyTargetBase{parameters},
        border_indices_{storage},
        top_rows_{storage + left_padding_ + right_padding_},
        bottom_rows_{storage + left_padding_ + right_padding_ + top_padding_} {
    size_t *generated_top_rows = storage + left_padding_ + right_padding_;
    size_t *generated_bottom_rows = generated_top_rows + top_padding_;
    prepare_indexed_maps(parameters.border_type, storage, generated_top_rows,
                         generated_bottom_rows);
  }

  KLEIDICV_LOCALLY_STREAMING kleidicv_error_t
  process_stripe(size_t dst_y_begin, size_t dst_y_end) const override;

 private:
  void fill_row(const uint8_t *src_row, uint8_t *dst_row,
                AddPaddingByCopyContext &context) const KLEIDICV_STREAMING {
    Backend::copy_contiguous_bytes(src_row, dst_row + inner_offset_,
                                   inner_size_, context);

    copy_indexed_pixels_common(src_row, dst_row, border_indices_, left_padding_,
                               context);

    const size_t *right_indices = border_indices_ + left_padding_;
    copy_indexed_pixels_common(src_row, dst_row + right_offset_, right_indices,
                               right_padding_, context);
  }

  void copy_indexed_pixels_common(
      const uint8_t *src, uint8_t *dst, const size_t *indices, size_t count,
      AddPaddingByCopyContext &context) const KLEIDICV_STREAMING {
    if constexpr (PixelSize == sizeof(uint8_t)) {
      copy_indexed_pixels_typed<uint8_t>(src, dst, indices, count);
    } else if constexpr (PixelSize == sizeof(uint16_t)) {
      copy_indexed_pixels_typed<uint16_t>(src, dst, indices, count);
    } else if constexpr (PixelSize == sizeof(uint32_t)) {
      copy_indexed_pixels_typed<uint32_t>(src, dst, indices, count);
    } else if constexpr (PixelSize == sizeof(uint64_t)) {
      copy_indexed_pixels_typed<uint64_t>(src, dst, indices, count);
    } else {
      copy_indexed_pixels_by_bytes(src, dst, indices, count, context);
    }
  }

  void prepare_indexed_maps(kleidicv_border_type_t border_type,
                            size_t *generated_border_indices,
                            size_t *generated_top_rows,
                            size_t *generated_bottom_rows) KLEIDICV_STREAMING {
    switch (border_type) {
      case KLEIDICV_BORDER_TYPE_WRAP:
        prepare_indexed_maps<wrap_index>(generated_border_indices,
                                         generated_top_rows,
                                         generated_bottom_rows);
        return;
      case KLEIDICV_BORDER_TYPE_REFLECT:
        prepare_indexed_maps<reflect_index>(generated_border_indices,
                                            generated_top_rows,
                                            generated_bottom_rows);
        return;
      default /* KLEIDICV_BORDER_TYPE_REVERSE */:
        prepare_indexed_maps<reverse_index>(generated_border_indices,
                                            generated_top_rows,
                                            generated_bottom_rows);
        return;
    }
  }

  template <typename PixelType>
  static void copy_indexed_pixels_typed(const uint8_t *src, uint8_t *dst,
                                        const size_t *indices,
                                        size_t count) KLEIDICV_STREAMING {
    auto *typed_dst = reinterpret_cast<PixelType *>(dst);
    auto *typed_src = reinterpret_cast<const PixelType *>(src);

    for (size_t i = 0; i < count; ++i) {
      typed_dst[i] = typed_src[indices[i]];
    }
  }

  void copy_indexed_pixels_by_bytes(
      const uint8_t *src, uint8_t *dst, const size_t *indices, size_t count,
      AddPaddingByCopyContext &context) const KLEIDICV_STREAMING {
    size_t dst_offset = 0;
    for (size_t i = 0; i < count; ++i, dst_offset += pixel_size_) {
      Backend::copy_contiguous_bytes(src + indices[i] * pixel_size_,
                                     dst + dst_offset, pixel_size_, context);
    }
  }

  template <auto MapIndex>
  void prepare_indexed_maps(size_t *generated_border_indices,
                            size_t *generated_top_rows,
                            size_t *generated_bottom_rows) KLEIDICV_STREAMING {
    build_horizontal_border_indices<MapIndex>(
        generated_border_indices, left_padding_, right_padding_, src_width_);
    build_vertical_border_rows<MapIndex>(generated_top_rows, top_padding_,
                                         generated_bottom_rows, bottom_padding_,
                                         src_height_);
  }

  void process_mapped_stripe(
      size_t body_begin, size_t body_end, size_t dst_y_begin, size_t dst_y_end,
      AddPaddingByCopyContext &context) const KLEIDICV_STREAMING {
    process_vertical_mapped_stripe(
        body_begin, body_end, top_rows_, bottom_rows_, dst_y_begin, dst_y_end,
        [&](const uint8_t *src_row, uint8_t *dst_row)
            KLEIDICV_STREAMING { fill_row(src_row, dst_row, context); });
  }

  const size_t *const border_indices_;
  const size_t *const top_rows_;
  const size_t *const bottom_rows_;
};

// -----------------------------------------------------------------------------
// Common process_stripe definitions
// -----------------------------------------------------------------------------
template <size_t PixelSize, typename Backend>
KLEIDICV_LOCALLY_STREAMING KLEIDICV_ADD_PADDING_BY_COPY_FN_ATTRS
    kleidicv_error_t
    AddPaddingByCopyConstant<PixelSize, Backend>::process_stripe(
        size_t dst_y_begin, size_t dst_y_end) const {
  KLEIDICV_ADD_PADDING_BY_COPY_CONTEXT(PixelSize, context);

  if (inner_size_ == 0) {
    fill_constant_rows(dst_y_begin, dst_y_end, context);
    return KLEIDICV_OK;
  }

  const auto [body_begin, body_end] = split_body_rows(dst_y_begin, dst_y_end);

  fill_constant_rows(dst_y_begin, body_begin, context);
  process_constant_body_rows(body_begin, body_end, context);
  fill_constant_rows(body_end, dst_y_end, context);
  return KLEIDICV_OK;
}

template <size_t PixelSize, typename Backend>
KLEIDICV_LOCALLY_STREAMING KLEIDICV_ADD_PADDING_BY_COPY_FN_ATTRS
    kleidicv_error_t
    AddPaddingByCopyReplicate<PixelSize, Backend>::process_stripe(
        size_t dst_y_begin, size_t dst_y_end) const {
  KLEIDICV_ADD_PADDING_BY_COPY_CONTEXT(PixelSize, context);

  const auto [body_begin, body_end] = split_body_rows(dst_y_begin, dst_y_end);

  fill_replicate_top_rows(dst_y_begin, body_begin, context);
  process_replicate_body_rows(body_begin, body_end, context);
  fill_replicate_bottom_rows(body_end, dst_y_end, context);
  return KLEIDICV_OK;
}

template <size_t PixelSize, typename Backend>
KLEIDICV_LOCALLY_STREAMING KLEIDICV_ADD_PADDING_BY_COPY_FN_ATTRS
    kleidicv_error_t
    AddPaddingByCopyReflect<PixelSize, Backend>::process_stripe(
        size_t dst_y_begin, size_t dst_y_end) const {
  KLEIDICV_ADD_PADDING_BY_COPY_CONTEXT(PixelSize, context);

  const auto [body_begin, body_end] = split_body_rows(dst_y_begin, dst_y_end);

  process_mapped_stripe(body_begin, body_end, dst_y_begin, dst_y_end, context);
  return KLEIDICV_OK;
}

template <size_t PixelSize, typename Backend>
KLEIDICV_LOCALLY_STREAMING KLEIDICV_ADD_PADDING_BY_COPY_FN_ATTRS
    kleidicv_error_t
    AddPaddingByCopyReverse<PixelSize, Backend>::process_stripe(
        size_t dst_y_begin, size_t dst_y_end) const {
  KLEIDICV_ADD_PADDING_BY_COPY_CONTEXT(PixelSize, context);

  const auto [body_begin, body_end] = split_body_rows(dst_y_begin, dst_y_end);

  process_mapped_stripe(body_begin, body_end, dst_y_begin, dst_y_end, context);
  return KLEIDICV_OK;
}

template <size_t PixelSize, typename Backend>
KLEIDICV_LOCALLY_STREAMING KLEIDICV_ADD_PADDING_BY_COPY_FN_ATTRS
    kleidicv_error_t
    AddPaddingByCopyWrap<PixelSize, Backend>::process_stripe(
        size_t dst_y_begin, size_t dst_y_end) const {
  KLEIDICV_ADD_PADDING_BY_COPY_CONTEXT(PixelSize, context);

  const auto [body_begin, body_end] = split_body_rows(dst_y_begin, dst_y_end);

  process_mapped_stripe(body_begin, body_end, dst_y_begin, dst_y_end, context);
  return KLEIDICV_OK;
}

template <size_t PixelSize, typename Backend>
KLEIDICV_LOCALLY_STREAMING KLEIDICV_ADD_PADDING_BY_COPY_FN_ATTRS
    kleidicv_error_t
    AddPaddingByCopyIndexed<PixelSize, Backend>::process_stripe(
        size_t dst_y_begin, size_t dst_y_end) const {
  KLEIDICV_ADD_PADDING_BY_COPY_CONTEXT(PixelSize, context);

  const auto [body_begin, body_end] = split_body_rows(dst_y_begin, dst_y_end);

  process_mapped_stripe(body_begin, body_end, dst_y_begin, dst_y_end, context);
  return KLEIDICV_OK;
}

// -----------------------------------------------------------------------------
// Operation allocation helpers
// -----------------------------------------------------------------------------
template <typename Operation>
static AddPaddingByCopyOpPointer allocate_operation(
    const AddPaddingByCopyParameters &parameters) KLEIDICV_STREAMING {
  // The allocation packs [operation object][size_t tables]. Align the table
  // start so vertical row maps and horizontal indices can be accessed as
  // size_t*. storage_words is a count of size_t entries, so convert it to bytes
  // when computing the total malloc size.
  const size_t storage_words = Operation::storage_word_count(parameters);
  const size_t storage_offset =
      KLEIDICV_TARGET_NAMESPACE::align_up(sizeof(Operation), alignof(size_t));
  const size_t total_size = storage_offset + storage_words * sizeof(size_t);

  void *allocation = std::malloc(total_size);
  if (allocation == nullptr) {
    return nullptr;
  }

  auto *storage = reinterpret_cast<size_t *>(
      static_cast<uint8_t *>(allocation) + storage_offset);
  auto *state = new (allocation) Operation{parameters, storage};

  return AddPaddingByCopyOpPointer{static_cast<AddPaddingByCopyBase *>(state)};
}

template <template <size_t> class Operation>
static AddPaddingByCopyOpPointer allocate_pixel_size_operation(
    const AddPaddingByCopyParameters &parameters) KLEIDICV_STREAMING {
  switch (parameters.pixel_size) {
    case sizeof(uint8_t):
      return allocate_operation<Operation<sizeof(uint8_t)>>(parameters);
    case sizeof(uint16_t):
      return allocate_operation<Operation<sizeof(uint16_t)>>(parameters);
    case sizeof(uint32_t):
      return allocate_operation<Operation<sizeof(uint32_t)>>(parameters);
    case sizeof(uint64_t):
      return allocate_operation<Operation<sizeof(uint64_t)>>(parameters);
    case 3 * sizeof(uint8_t):
      return allocate_operation<Operation<3 * sizeof(uint8_t)>>(parameters);
    case 3 * sizeof(uint16_t):
      return allocate_operation<Operation<3 * sizeof(uint16_t)>>(parameters);
    case 3 * sizeof(uint32_t):
      return allocate_operation<Operation<3 * sizeof(uint32_t)>>(parameters);
    default:
      return allocate_operation<Operation<0>>(parameters);
  }
}

#undef KLEIDICV_ADD_PADDING_BY_COPY_CONTEXT

}  // namespace KLEIDICV_TARGET_NAMESPACE

#undef KLEIDICV_ADD_PADDING_BY_COPY_FN_ATTRS

#endif  // KLEIDICV_SRC_TRANSFORM_ADD_PADDING_BY_COPY_COMMON_H
