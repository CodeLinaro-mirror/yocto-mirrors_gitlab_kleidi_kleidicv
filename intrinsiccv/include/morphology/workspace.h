// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_MORPHOLOGY_WORKSPACE_H
#define INTRINSICCV_MORPHOLOGY_WORKSPACE_H

#include <algorithm>
#include <cstdlib>
#include <memory>

#include "intrinsiccv.h"
#include "types.h"

#if INTRINSICCV_TARGET_SME2
#include "sve2.h"
#endif

namespace intrinsiccv {

// Forward declarations.
class MorphologyWorkspace;

// Deleter for MorphologyWorkspace instances.
class MorphologyWorkspaceDeleter {
 public:
  void operator()(MorphologyWorkspace *ptr) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    std::free(ptr);
  };
};

// Workspace for morphological operations.
class MorphologyWorkspace final {
 public:
  // Shorthand for std::unique_ptr<> holding a workspace.
  using Pointer =
      std::unique_ptr<MorphologyWorkspace, MorphologyWorkspaceDeleter>;

  // MorphologyWorkspace is only constructible with create().
  MorphologyWorkspace() = delete;

  // Creates a workspace on the heap.
  static Pointer create(Rectangle rect, Rectangle kernel, Margin margin,
                        size_t channels, size_t buffer_type_size)
      INTRINSICCV_STREAMING_COMPATIBLE {
    // These values are arbitrarily choosen.
    const size_t rows_per_iteration = std::max(2 * kernel.height(), 32ul);
    // To avoid load/store penalties.
    const size_t kAlignment = 16;

    // A single wide row which can hold one row worth of data in addition
    // to left and right margins.
    size_t wide_rows_width = margin.left() + rect.width() + margin.right();
    size_t wide_rows_stride = wide_rows_width * channels;
    wide_rows_stride = __builtin_align_up(wide_rows_stride, kAlignment);
    size_t wide_rows_height = 1ul;  // There is only one wide row.
    size_t wide_rows_size = wide_rows_stride * wide_rows_height;
    wide_rows_size += kAlignment - 1;

    // Multiple buffer rows to hold rows without any borders.
    size_t buffer_rows_width = buffer_type_size * rect.width();
    size_t buffer_rows_stride = buffer_rows_width * channels;
    buffer_rows_stride = __builtin_align_up(buffer_rows_stride, kAlignment);
    size_t buffer_rows_height = 2 * rows_per_iteration;
    size_t buffer_rows_size = buffer_rows_stride * buffer_rows_height;
    buffer_rows_size += kAlignment - 1;

    // Storage for indirect row access.
    size_t indirect_row_storage_size = 3 * rows_per_iteration * sizeof(void *);

    // Try to allocate workspace at once.
    size_t allocation_size = sizeof(MorphologyWorkspace) +
                             indirect_row_storage_size + buffer_rows_size +
                             wide_rows_size;
    auto allocation = std::malloc(allocation_size);
    auto workspace = MorphologyWorkspace::Pointer{
        reinterpret_cast<MorphologyWorkspace *>(allocation)};
    if (!workspace) {
      return workspace;
    }

    workspace->rows_per_iteration_ = rows_per_iteration;
    workspace->wide_rows_src_width_ = rect.width();
    workspace->channels_ = channels;

    auto buffer_rows_address = &workspace->data_[indirect_row_storage_size];
    buffer_rows_address = __builtin_align_up(buffer_rows_address, kAlignment);
    workspace->buffer_rows_offset_ = buffer_rows_address - &workspace->data_[0];
    workspace->buffer_rows_stride_ = buffer_rows_stride;

    auto wide_rows_address =
        &workspace->data_[indirect_row_storage_size + buffer_rows_size];
    wide_rows_address += margin.left() * channels;
    wide_rows_address = __builtin_align_up(wide_rows_address, kAlignment);
    wide_rows_address -= margin.left() * channels;
    workspace->wide_rows_offset_ = wide_rows_address - &workspace->data_[0];
    workspace->wide_rows_stride_ = wide_rows_stride;

    return workspace;
  }

  template <typename O>
  void process(Rectangle rect, Rows<const typename O::SourceType> src_rows,
               Rows<typename O::DestinationType> dst_rows, Margin margin,
               Border<typename O::SourceType> border,
               intrinsiccv_border_type_t border_type,
               O operation) INTRINSICCV_STREAMING_COMPATIBLE {
    using S = typename O::SourceType;
    using B = typename O::BufferType;

    // Wide rows which can hold data with left and right margins.
    auto wide_rows = Rows{reinterpret_cast<S *>(&data_[wide_rows_offset_]),
                          wide_rows_stride_, channels_};

    // Double buffered indirect rows to access the buffer rows.
    auto db_indirect_rows = DoubleBufferedIndirectRows{
        reinterpret_cast<B **>(&data_[0]), rows_per_iteration_,
        Rows{reinterpret_cast<B *>(&data_[buffer_rows_offset_]),
             buffer_rows_stride_, channels_}};

    // [Step 1] Initialize workspace.
    horizontal_height_ = margin.top() + rect.height() + margin.bottom();
    vertical_height_ = rect.height();
    row_index_ = 0;

    // Constant border values.
    auto left_border_value = saturating_cast<double, S>(border.left());
    auto top_border_value = saturating_cast<double, S>(border.top());
    auto right_border_value = saturating_cast<double, S>(border.right());
    auto bottom_border_value = saturating_cast<double, S>(border.bottom());

    // Used by replicate border type.
    auto first_src_rows = src_rows;
    auto last_src_rows = src_rows.at(rect.height() - 1);

    if (size_t horizontal_height = get_next_horizontal_height()) {
      for (size_t index = 0; index < horizontal_height; ++index) {
        switch (border_type) {
          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_CONSTANT: {
            make_constant_border(wide_rows, 0, margin.left(),
                                 left_border_value);

            if (row_index_ < margin.top()) {
              make_constant_border(wide_rows, margin.left(),
                                   wide_rows_src_width_, top_border_value);
            } else if (row_index_ < (margin.top() + rect.height())) {
              copy_data(src_rows, wide_rows.at(0, margin.left()),
                        wide_rows_src_width_);
              // Advance source rows.
              ++src_rows;
            } else if (row_index_ >= (margin.top() + rect.height())) {
              make_constant_border(wide_rows, margin.left(),
                                   wide_rows_src_width_, bottom_border_value);
            }

            make_constant_border(wide_rows,
                                 margin.left() + wide_rows_src_width_,
                                 margin.right(), right_border_value);

            // Advance counters.
            ++row_index_;
          } break;

          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_REPLICATE: {
            Rows<const S> current_src_row;

            if (row_index_ < margin.top()) {
              current_src_row = first_src_rows;
            } else if (row_index_ < (margin.top() + rect.height())) {
              current_src_row = src_rows;
              // Advance source rows.
              ++src_rows;
            } else {
              current_src_row = last_src_rows;
            }

            replicate_border(current_src_row, wide_rows, 0, 0, margin.left());
            copy_data(current_src_row, wide_rows.at(0, margin.left()),
                      wide_rows_src_width_);
            replicate_border(
                current_src_row, wide_rows, wide_rows_src_width_ - 1,
                margin.left() + wide_rows_src_width_, margin.right());

            // Advance counters.
            ++row_index_;
          } break;

          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_REFLECT:
          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_REVERSE:
          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_WRAP:
          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_TRANSPARENT:
          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_NONE:
          default:
            break;
        }  // switch (border_type)

        // [Step 2] Process the preloaded data.
        operation.process_horizontal(Rectangle{rect.width(), 1ul}, wide_rows,
                                     db_indirect_rows.write_at().at(index));

      }  // for (...; index < horizontal_height; ...)

      db_indirect_rows.swap();
    }

    // [Step 3] Process any remaining data.
    while (vertical_height_) {
      size_t horizontal_height = get_next_horizontal_height();
      for (size_t index = 0; index < horizontal_height; ++index) {
        switch (border_type) {
          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_CONSTANT: {
            if (row_index_ < (margin.top() + rect.height())) {
              // Constant left and right borders with source data.
              copy_data(src_rows, wide_rows.at(0, margin.left()),
                        wide_rows_src_width_);
              // Advance source rows.
              ++src_rows;
            } else if (row_index_ >= (margin.top() + rect.height())) {
              make_constant_border(wide_rows, margin.left(),
                                   wide_rows_src_width_, bottom_border_value);
            }

            // Advance row counter.
            ++row_index_;
          } break;

          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_REPLICATE: {
            Rows<const S> current_src_row;

            if (row_index_ < (margin.top() + rect.height())) {
              current_src_row = src_rows;
              // Advance source rows.
              ++src_rows;
            } else {
              current_src_row = last_src_rows;
            }

            replicate_border(current_src_row, wide_rows, 0, 0, margin.left());
            copy_data(current_src_row, wide_rows.at(0, margin.left()),
                      wide_rows_src_width_);
            replicate_border(
                current_src_row, wide_rows, wide_rows_src_width_ - 1,
                margin.left() + wide_rows_src_width_, margin.right());

            // Advance counters.
            ++row_index_;
          } break;

          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_REFLECT:
          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_REVERSE:
          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_WRAP:
          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_TRANSPARENT:
          case intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_NONE:
          default:
            break;
        }  // switch (border_type)

        operation.process_horizontal(Rectangle{rect.width(), 1ul}, wide_rows,
                                     db_indirect_rows.write_at().at(index));
      }  // for (...; index < horizontal_height; ...)

      size_t next_vertical_height = get_next_vertical_height();
      operation.process_vertical(Rectangle{rect.width(), next_vertical_height},
                                 db_indirect_rows.read_at(), dst_rows);
      dst_rows += next_vertical_height;

      db_indirect_rows.swap();
    }
  }

 private:
  // The number of wide rows to process in the next iteration.
  [[nodiscard]] size_t get_next_horizontal_height()
      INTRINSICCV_STREAMING_COMPATIBLE {
    size_t height = std::min(horizontal_height_, rows_per_iteration_);
    horizontal_height_ -= height;
    return height;
  }

  // The number of indirect rows to process in the next iteration.
  [[nodiscard]] size_t get_next_vertical_height()
      INTRINSICCV_STREAMING_COMPATIBLE {
    size_t height = std::min(vertical_height_, rows_per_iteration_);
    vertical_height_ -= height;
    return height;
  }

#if INTRINSICCV_TARGET_SME2
  template <typename ScalarType>
  class CopyOperation final : public UnrollTwice {
   public:
    using ContextType = sve2::Context;
    using VecTraits = sve2::VecTraits<ScalarType>;
    using VectorType = typename VecTraits::VectorType;

    VectorType vector_path(ContextType,
                           VectorType src) INTRINSICCV_STREAMING_COMPATIBLE {
      return src;
    }
  };  // end of class CopyOperation<ScalarType>
#endif

  template <typename T>
  void copy_data(Rows<const T> src_rows, Rows<T> dst_rows,
                 size_t length) INTRINSICCV_STREAMING_COMPATIBLE {
#if INTRINSICCV_TARGET_SME2
    Rectangle rect{length, static_cast<size_t>(1)};
    CopyOperation<T> operation;
    sve2::apply_operation_by_rows(operation, rect, src_rows, dst_rows);
#else
    std::memcpy(static_cast<void *>(&dst_rows[0]),
                static_cast<const void *>(&src_rows[0]),
                length * sizeof(T) * dst_rows.channels());
#endif
  }

  template <typename T, typename BorderType>
  void make_constant_border(Rows<T> dst_rows, size_t dst_index, size_t count,
                            BorderType value) INTRINSICCV_STREAMING_COMPATIBLE {
    auto dst = &dst_rows.at(0, dst_index)[0];
    for (size_t index = 0; index < count * dst_rows.channels(); ++index) {
      dst[index] = value;
    }
  }

  template <typename T>
  void replicate_border(Rows<const T> src_rows, Rows<T> dst_rows,
                        size_t src_index, size_t dst_index,
                        size_t count) INTRINSICCV_STREAMING_COMPATIBLE {
    if (!count) {
      return;
    }

    for (size_t channel = 0; channel < src_rows.channels(); ++channel) {
      for (size_t index = dst_index; index < dst_index + count; ++index) {
        dst_rows.at(0, index)[channel] = src_rows.at(0, src_index)[channel];
      }
    }
  }

  static_assert(sizeof(Pointer) == sizeof(void *), "Unexpected type size");

  // Number of wide rows in this workspace.
  size_t rows_per_iteration_;
  // Size of the data in bytes within a row.
  size_t wide_rows_src_width_;
  // The number of channels.
  size_t channels_;
  // Remaining height to process in horizontal direction.
  size_t horizontal_height_;
  // Remaining height to process in vertical direction.
  size_t vertical_height_;
  // Index of the processed row.
  size_t row_index_;
  // Offset in bytes to the buffer rows from &data_[0].
  size_t buffer_rows_offset_;
  // Stride of the buffer rows.
  size_t buffer_rows_stride_;
  // Offset in bytes to the wide rows from &data_[0].
  size_t wide_rows_offset_;
  // Stride of the wide rows.
  size_t wide_rows_stride_;
  // Workspace area begins here.
  uint8_t data_[0] INTRINSICCV_ATTR_ALIGNED(sizeof(void *));
};  // end of class MorphologyWorkspace

}  // namespace intrinsiccv

#endif  // INTRINSICCV_MORPHOLOGY_WORKSPACE_H
