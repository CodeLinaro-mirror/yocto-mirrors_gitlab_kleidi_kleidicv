// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_MATMUL_FILTER_GENERAL_H
#define KLEIDICV_MATMUL_FILTER_GENERAL_H

#include <arm_sme.h>
#include <sys/types.h>

#include <iomanip>
#include <iostream>
#include <utility>

#include "kleidicv/config.h"
#include "kleidicv/types.h"
#include "kleidicv/workspace/border_15x15.h"
#include "kleidicv/workspace/border_types.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Filter class for matmul approach
template <size_t Channels, typename FilterType, class TransposerType>
class MatmulFilter {
 public:
  using SourceType = typename FilterType::SourceType;
  using BufferType = typename FilterType::BufferType;
  using DestinationType = typename FilterType::DestinationType;
  using BorderInfoType = typename FilterType::BorderInfoType;
  using BorderType = typename FilterType::BorderType;

  static constexpr size_t kKernelSize = FilterType::kKernelSize;
  static constexpr size_t kBorderSize = FilterType::kBorderSize;
  static constexpr size_t kChannels = Channels;

  explicit MatmulFilter(FilterType filter) : filter_(filter) {}

  // Process rows horizontally and vertically
  KLEIDICV_NEW_ZA void process(
      Rows<const typename FilterType::SourceType> src_rows,
      Rows<typename FilterType::SourceType> transposed_buffer_rows,
      Rows<typename FilterType::DestinationType> dst_rows, Rectangle rect,
      Rectangle padded_rect,
      typename FilterType::BorderInfoType horizontal_border,
      typename FilterType::BorderInfoType vertical_border) KLEIDICV_STREAMING {
    vertical_process(src_rows, dst_rows, rect, padded_rect, horizontal_border);
    horizontal_process(dst_rows, transposed_buffer_rows, dst_rows, rect,
                       padded_rect, vertical_border);
  }

 private:
  void horizontal_process(
      Rows<const typename FilterType::SourceType> src_rows,
      Rows<typename FilterType::SourceType> transpose_buffer_rows,
      Rows<typename FilterType::BufferType> dst_rows, Rectangle rect,
      Rectangle padded_rect,
      typename FilterType::BorderInfoType border_info) KLEIDICV_STREAMING KLEIDICV_INOUT_ZA {
    typename FilterType::template IterationsInfo<Channels> iterations_info;
    const size_t col_iteration_step = iterations_info.horizontal_col_step();
    const size_t row_iteration_step = iterations_info.horizontal_row_step();

    const size_t kernel_block_size = iterations_info.kernel_block_size();
    const size_t border_size = FilterType::kBorderSize;
    const ssize_t kernel_block_border_padding =
        rect.width() + border_size - kernel_block_size;
    const size_t borderless_end =
        kernel_block_border_padding < 0 ? 0 : kernel_block_border_padding;

    for (size_t row = 0; row < rect.height(); row += row_iteration_step) {
      size_t batch = row_iteration_step;
      // Regular branch instead of ternary operator
      // to avoid csel. Relying on branch predictor
      // since branch is predictable.
      if (batch > padded_rect.height() - row) {  // NOLINT
        batch = padded_rect.height() - row;
      }

      transposer_.transpose(src_rows, transpose_buffer_rows, rect, row, batch);

      for (size_t col = 0; col < border_size; col += col_iteration_step) {
        filter_.template horizontal_path<Channels, true>(
            transpose_buffer_rows, dst_rows, rect, col, row, border_info);
      }

      for (size_t col = border_size; col < borderless_end;
           col += col_iteration_step) {
        filter_.template horizontal_path<Channels, false>(
            transpose_buffer_rows, dst_rows, rect, col, row, border_info);
      }

      for (size_t col = borderless_end; col < rect.width();
           col += col_iteration_step) {
        filter_.template horizontal_path<Channels, true>(
            transpose_buffer_rows, dst_rows, rect, col, row, border_info);
      }
    }
  }

  void vertical_process(
      Rows<const typename FilterType::SourceType> src_rows,
      Rows<typename FilterType::BufferType> dst_rows, Rectangle rect,
      Rectangle padded_rect, BorderInfoType border_info) KLEIDICV_STREAMING KLEIDICV_INOUT_ZA {
    typename FilterType::template IterationsInfo<Channels> iterations_info;
    const size_t col_iteration_step = iterations_info.vertical_col_step();
    const size_t row_iteration_step = iterations_info.vertical_row_step();
    const size_t elements_width = rect.width() * Channels;

    const size_t kernel_block_size = iterations_info.kernel_block_size();
    const size_t border_size = FilterType::kBorderSize;
    const ssize_t kernel_block_border_padding =
        rect.height() + border_size - kernel_block_size;
    const size_t borderless_end =
        kernel_block_border_padding < 0 ? 0 : kernel_block_border_padding;

    for (size_t col = 0; col < elements_width; col += col_iteration_step) {
      for (size_t row = 0; row < border_size; row += row_iteration_step) {
        filter_.template vertical_path<Channels, true>(
            src_rows, dst_rows, rect, padded_rect, col, row, border_info);
      }
      for (size_t row = border_size; row < borderless_end;
           row += row_iteration_step) {
        filter_.template vertical_path<Channels, false>(
            src_rows, dst_rows, rect, padded_rect, col, row, border_info);
      }
      for (size_t row = borderless_end; row < rect.height();
           row += row_iteration_step) {
        filter_.template vertical_path<Channels, true>(
            src_rows, dst_rows, rect, padded_rect, col, row, border_info);
      }
    }
  }

  FilterType filter_;
  TransposerType transposer_;
};

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif
