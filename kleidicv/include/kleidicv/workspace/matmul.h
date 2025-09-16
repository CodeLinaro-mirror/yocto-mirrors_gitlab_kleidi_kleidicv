// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_WORKSPACE_MATMUL_H
#define KLEIDICV_WORKSPACE_MATMUL_H

#include <arm_sme.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>

#include "kleidicv/types.h"
#include "kleidicv/utils.h"
#include "separable.h"

namespace KLEIDICV_TARGET_NAMESPACE {

class MatmulBufferSizesPolicy : public DefaultBufferSizesPolicy {
 public:
  MatmulBufferSizesPolicy(const Rectangle &rect, const Rectangle &kernel_rect,
                          size_t channels)
      : DefaultBufferSizesPolicy(rect, channels), kernel_rect_(kernel_rect) {}

  size_t compute_buffer_size() const {
    size_t max_border_width = kernel_rect_.width() >> 1;
    Rectangle padded_rect(rect_.width() + (max_border_width << 1),
                          rect_.height() + (max_border_width << 1));

    size_t matmul_allocation_size = (compute_helper_buffer_size(padded_rect) +
                                     compute_kernel_buffer_size());
    return std::max(matmul_allocation_size,
                    DefaultBufferSizesPolicy::compute_buffer_size());
  }

  size_t compute_transpose_buffer_cols() const KLEIDICV_STREAMING {
    return svcntsb();
  }

  size_t compute_transpose_buffer_rows(Rectangle padded_rect, size_t channels,
                                       size_t max_kernel_width) const
      KLEIDICV_STREAMING {
    size_t rows = padded_rect.width() * channels;

    // To avoid using predicates and reminder loops while processing
    // the buffer it should be aligned by SVLB (while building the buffer)
    // and 4 (since it's being processed with 4 rows iteration because of the
    // UMOPA)
    size_t align1 = svcntsb();
    size_t align2 = max_kernel_width + svcntsw() - 1;
    size_t lcm = (align2 >> __builtin_ctz(align2)) << __builtin_ctz(align1);
    rows = align_up(rows, lcm);
    return rows;
  }

  size_t compute_helper_buffer_size(Rectangle padded_rect) const
      KLEIDICV_STREAMING {
    return compute_transpose_buffer_cols() *
           compute_transpose_buffer_rows(padded_rect, channels_,
                                         kernel_rect_.width());
  }

  size_t compute_kernel_buffer_size() const KLEIDICV_STREAMING {
    // Same logic here with alignment: since UMOPA processing
    // 4 rows at a time, alignment to 4 is needed
    size_t rows = svcntsw() + kernel_rect_.width() - 1;
    rows = align_up(rows, static_cast<size_t>(4));

    size_t cols = svcntsw();
    return cols * rows;
  }

 private:
  const Rectangle kernel_rect_;
};

// Workspace for separable fixed-size filters that uses matmul approach.
//
// Theory of operation remains the same as in the separable.h
//
// Operation will be done with 2 matrix multiplications: one corresponds to
// vertical path, second to horizontal path. First matrix is src_rows, second
// matrix is Toeplitz matrix built from kernel vectors data. This matrix is
// being constructed in the following way:
//
//    M[i][j] = k_{i - j}, if i \in [j, j + K]
//    M[i][j] = 0, otherwise
//
// where K stands for kernel size and k_i are kernel vector elements. In other
// words, kernel matrix is zero matrix with kernel values being placed on the
// diagonal strip with size K.
//
// Consequently, in matrix multiplication expression the following happens:
//
//    K_v^T * M * K_h
//
class MatmulSeparableFilterWorkspace final : public SeparableFilterWorkspace {
 public:
  // Processes rows horizontally and vertically
  template <typename FilterType>
  void process(Rectangle rect, size_t y_begin, size_t y_end,
               Rows<const typename FilterType::SourceType> src_rows,
               Rows<typename FilterType::DestinationType> dst_rows,
               typename FilterType::BorderType border_type,
               FilterType filter) KLEIDICV_STREAMING {
    constexpr size_t kChannels = FilterType::kChannels;
    constexpr size_t kKernelSize = FilterType::kKernelSize;
    constexpr size_t kBorderSize = FilterType::kBorderSize;
    static_assert(kChannels == 1 || kChannels == 3 || kChannels == 4);

    src_rows = src_rows.at(y_begin);
    size_t height = y_end - y_begin;

    Rectangle corrected_rect{rect.width(), height};
    Rectangle padded_rect(rect.width() + (kBorderSize << 1),
                          rect.height() + (kBorderSize << 1));

    MatmulBufferSizesPolicy matmul_policy(
        corrected_rect, Rectangle(kKernelSize, kKernelSize), kChannels);

    typename FilterType::BorderInfoType vertical_border{corrected_rect.width(),
                                                        border_type};
    typename FilterType::BorderInfoType horizontal_border{
        corrected_rect.height(), border_type};

    size_t kernel_buffer_size = matmul_policy.compute_kernel_buffer_size();
    auto *helper_buffer = reinterpret_cast<typename FilterType::SourceType *>(
        &data_[buffer_rows_offset_ + kernel_buffer_size]);
    helper_buffer = align_up(helper_buffer, kAlignment);

    // Channels is 1 due to the fact that data is transposed
    auto transposed_buffer_rows =
        Rows{helper_buffer, matmul_policy.compute_transpose_buffer_cols() *
                                sizeof(typename FilterType::SourceType)};
    filter.process(src_rows, transposed_buffer_rows, dst_rows, rect,
                   padded_rect, horizontal_border, vertical_border);
  }

  // Get allocated chunk of memory for helper kernel buffer
  void *get_kernel_buffer() { return &data_[buffer_rows_offset_]; }
};  // end of class SeparableFilterWorkspace

static_assert(sizeof(MatmulSeparableFilterWorkspace) ==
              sizeof(SeparableFilterWorkspace));

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_WORKSPACE_SEPARABLE_H
