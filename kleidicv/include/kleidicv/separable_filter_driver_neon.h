// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_SEPARABLE_FILTER_DRIVER_NEON_H
#define KLEIDICV_SEPARABLE_FILTER_DRIVER_NEON_H

#include "kleidicv/config.h"
#include "kleidicv/neon.h"
#include "kleidicv/workspace/border.h"

namespace kleidicv::neon {

// Template for drivers of separable NxM filters.
template <typename FilterType, const size_t KernelSize>
class SeparableFilterDriver {
 public:
  using SourceType = typename FilterType::SourceType;
  using BufferType = typename FilterType::BufferType;
  using DestinationType = typename FilterType::DestinationType;
  using SourceVecTraits = typename neon::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using BufferVecTraits = typename neon::VecTraits<BufferType>;
  using BufferVectorType = typename BufferVecTraits::VectorType;
  using BorderInfoType = typename neon::FixedBorderInfo<SourceType, KernelSize>;
  using BorderType = FixedBorderType;
  using BorderOffsets = typename BorderInfoType::Offsets;

  explicit SeparableFilterDriver(FilterType filter) : filter_{filter} {}

  static constexpr size_t margin = KernelSize >> 1;

  void process_vertical(size_t width, Rows<const SourceType> src_rows,
                        Rows<BufferType> dst_rows,
                        BorderOffsets border_offsets) const {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         SourceVecTraits::num_lanes()};
    constexpr auto seq = std::make_index_sequence<KernelSize>{};

    loop.unroll_once([&](size_t index) {
      vertical_vector_path(src_rows, dst_rows, border_offsets, index, seq);
    });

    loop.tail([&](size_t index) {
      vertical_scalar_path(src_rows, dst_rows, border_offsets, index, seq);
    });
  }

  void process_horizontal(size_t width, Rows<const BufferType> src_rows,
                          Rows<DestinationType> dst_rows,
                          BorderOffsets border_offsets) const {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         BufferVecTraits::num_lanes()};
    constexpr auto seq = std::make_index_sequence<KernelSize>{};

    loop.unroll_twice([&](size_t index) {
      horizontal_vector_path_2x(src_rows, dst_rows, border_offsets, index, seq);
    });

    loop.unroll_once([&](size_t index) {
      horizontal_vector_path(src_rows, dst_rows, border_offsets, index, seq);
    });

    loop.tail([&](size_t index) {
      process_horizontal_scalar(src_rows, dst_rows, border_offsets, index, seq);
    });
  }

  void process_horizontal_borders(Rows<const BufferType> src_rows,
                                  Rows<DestinationType> dst_rows,
                                  BorderOffsets border_offsets) const {
    constexpr auto seq = std::make_index_sequence<KernelSize>{};
    for (size_t index = 0; index < src_rows.channels(); ++index) {
      disable_loop_vectorization();
      process_horizontal_scalar(src_rows, dst_rows, border_offsets, index, seq);
    }
  }

 private:
  template <size_t... SeqNum>
  void vertical_vector_path(Rows<const SourceType> src_rows,
                            Rows<BufferType> dst_rows,
                            BorderOffsets border_offsets, size_t index,
                            std::index_sequence<SeqNum...>) const {
    SourceVectorType src[KernelSize] = {
        vld1q(&src_rows.at(border_offsets.c(SeqNum))[index])...};
    filter_.vertical_vector_path(src, &dst_rows[index]);
  }

  template <size_t... SeqNum>
  void vertical_scalar_path(Rows<const SourceType> src_rows,
                            Rows<BufferType> dst_rows,
                            BorderOffsets border_offsets, size_t index,
                            std::index_sequence<SeqNum...>) const {
    SourceType src[KernelSize] = {
        src_rows.at(border_offsets.c(SeqNum))[index]...};
    filter_.vertical_scalar_path(src, &dst_rows[index]);
  }

  template <size_t... SeqNum>
  void horizontal_vector_path_2x(Rows<const BufferType> src_rows,
                                 Rows<DestinationType> dst_rows,
                                 BorderOffsets border_offsets, size_t index,
                                 std::index_sequence<SeqNum...>) const {
    BufferVectorType src_a[KernelSize] = {
        vld1q(&src_rows.at(0, border_offsets.c(SeqNum))[index])...};
    BufferVectorType src_b[KernelSize] = {vld1q(&src_rows.at(
        0, border_offsets.c(SeqNum))[index + BufferVecTraits::num_lanes()])...};

    filter_.horizontal_vector_path(src_a, &dst_rows[index]);
    filter_.horizontal_vector_path(
        src_b, &dst_rows[index + BufferVecTraits::num_lanes()]);
  }

  template <size_t... SeqNum>
  void horizontal_vector_path(Rows<const BufferType> src_rows,
                              Rows<DestinationType> dst_rows,
                              BorderOffsets border_offsets, size_t index,
                              std::index_sequence<SeqNum...>) const {
    BufferVectorType src[KernelSize] = {
        vld1q(&src_rows.at(0, border_offsets.c(SeqNum))[index])...};
    filter_.horizontal_vector_path(src, &dst_rows[index]);
  }

  template <size_t... SeqNum>
  void process_horizontal_scalar(Rows<const BufferType> src_rows,
                                 Rows<DestinationType> dst_rows,
                                 BorderOffsets border_offsets, size_t index,
                                 std::index_sequence<SeqNum...>) const {
    BufferType src[KernelSize] = {
        src_rows.at(0, border_offsets.c(SeqNum))[index]...};
    filter_.horizontal_scalar_path(src, &dst_rows[index]);
  }

  FilterType filter_;
};  // end of class SeparableFilterDriver<FilterType, KernelSize>

}  // namespace kleidicv::neon

#endif  // KLEIDICV_SEPARABLE_FILTER_DRIVER_NEON_H
