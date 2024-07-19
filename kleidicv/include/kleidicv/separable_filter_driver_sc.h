// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_SEPARABLE_FILTER_DRIVER_SC_H
#define KLEIDICV_SEPARABLE_FILTER_DRIVER_SC_H

#include "kleidicv/config.h"
#include "kleidicv/sve2.h"
#include "kleidicv/workspace/border.h"

// It is used by SVE2 and SME2, the actual namespace will reflect it.
namespace KLEIDICV_TARGET_NAMESPACE {

// Template for drivers of separable NxM filters.
template <typename FilterType, const size_t KernelSize>
class SeparableFilterDriver {
 public:
  using SourceType = typename FilterType::SourceType;
  using BufferType = typename FilterType::BufferType;
  using DestinationType = typename FilterType::DestinationType;
  using SourceVecTraits =
      typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using BufferVecTraits =
      typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<BufferType>;
  using BufferVectorType = typename BufferVecTraits::VectorType;
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo<SourceType,
                                                            KernelSize>;
  using BorderType = FixedBorderType;
  using BorderOffsets = typename BorderInfoType::Offsets;

  explicit SeparableFilterDriver(FilterType filter)
      KLEIDICV_STREAMING_COMPATIBLE : filter_{filter} {}

  static constexpr size_t margin = KernelSize >> 1;

  void process_vertical(
      size_t width, Rows<const SourceType> src_rows, Rows<BufferType> dst_rows,
      BorderOffsets border_offsets) const KLEIDICV_STREAMING_COMPATIBLE {
    LoopUnroll2 loop{width * src_rows.channels(), SourceVecTraits::num_lanes()};
    constexpr auto seq = std::make_index_sequence<KernelSize>{};

    loop.unroll_once([&](size_t index) KLEIDICV_STREAMING_COMPATIBLE {
      svbool_t pg_all = SourceVecTraits::svptrue();
      vertical_vector_path(pg_all, src_rows, dst_rows, border_offsets, index,
                           seq);
    });

    loop.remaining([&](size_t index,
                       size_t length) KLEIDICV_STREAMING_COMPATIBLE {
      svbool_t pg = SourceVecTraits::svwhilelt(index, length);
      vertical_vector_path(pg, src_rows, dst_rows, border_offsets, index, seq);
    });
  }

  void process_horizontal(size_t width, Rows<const BufferType> src_rows,
                          Rows<DestinationType> dst_rows,
                          BorderOffsets border_offsets) const
      KLEIDICV_STREAMING_COMPATIBLE {
    svbool_t pg_all = BufferVecTraits::svptrue();
    LoopUnroll2 loop{width * src_rows.channels(), BufferVecTraits::num_lanes()};
    constexpr auto seq = std::make_index_sequence<KernelSize>{};

    loop.unroll_twice([&](size_t index) KLEIDICV_STREAMING_COMPATIBLE {
      horizontal_vector_path_2x(pg_all, src_rows, dst_rows, border_offsets,
                                index, seq);
    });

    loop.unroll_once([&](size_t index) KLEIDICV_STREAMING_COMPATIBLE {
      horizontal_vector_path(pg_all, src_rows, dst_rows, border_offsets, index,
                             seq);
    });

    loop.remaining(
        [&](size_t index, size_t length) KLEIDICV_STREAMING_COMPATIBLE {
          svbool_t pg = BufferVecTraits::svwhilelt(index, length);
          horizontal_vector_path(pg, src_rows, dst_rows, border_offsets, index,
                                 seq);
        });
  }

  // Processing of horizontal borders is always scalar because border offsets
  // change for each and every element in the border.
  void process_horizontal_borders(
      Rows<const BufferType> src_rows, Rows<DestinationType> dst_rows,
      BorderOffsets border_offsets) const KLEIDICV_STREAMING_COMPATIBLE {
    for (size_t index = 0; index < src_rows.channels(); ++index) {
      disable_loop_vectorization();
      process_horizontal_border(src_rows, dst_rows, border_offsets, index);
    }
  }

 private:
  template <size_t... SeqNum>
  void vertical_vector_path(
      svbool_t pg, Rows<const SourceType> src_rows, Rows<BufferType> dst_rows,
      BorderOffsets border_offsets, size_t index,
      std::index_sequence<SeqNum...>) const KLEIDICV_STREAMING_COMPATIBLE {
    filter_.vertical_vector_path(
        pg, svld1(pg, &src_rows.at(border_offsets.c(SeqNum))[index])...,
        &dst_rows[index]);
  }

  template <size_t... SeqNum>
  void horizontal_vector_path_2x(svbool_t pg, Rows<const BufferType> src_rows,
                                 Rows<DestinationType> dst_rows,
                                 BorderOffsets border_offsets, size_t index,
                                 std::index_sequence<SeqNum...>) const
      KLEIDICV_STREAMING_COMPATIBLE {
    filter_.horizontal_vector_path(
        pg, svld1(pg, &src_rows.at(0, border_offsets.c(SeqNum))[index])...,
        &dst_rows[index]);

    filter_.horizontal_vector_path(
        pg,
        svld1_vnum(pg, &src_rows.at(0, border_offsets.c(SeqNum))[index], 1)...,
        &dst_rows[index + BufferVecTraits::num_lanes()]);
  }

  template <size_t... SeqNum>
  void horizontal_vector_path(svbool_t pg, Rows<const BufferType> src_rows,
                              Rows<DestinationType> dst_rows,
                              BorderOffsets border_offsets, size_t index,
                              std::index_sequence<SeqNum...>) const
      KLEIDICV_STREAMING_COMPATIBLE {
    filter_.horizontal_vector_path(
        pg, svld1(pg, &src_rows.at(0, border_offsets.c(SeqNum))[index])...,
        &dst_rows[index]);
  }

  void process_horizontal_border(
      Rows<const BufferType> src_rows, Rows<DestinationType> dst_rows,
      BorderOffsets border_offsets,
      size_t index) const KLEIDICV_STREAMING_COMPATIBLE {
    BufferType src[KernelSize];
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t i = 0; i < KernelSize; i++) {
      src[i] = src_rows.at(0, border_offsets.c(i))[index];
    }
    filter_.horizontal_scalar_path(src, &dst_rows[index]);
  }

  FilterType filter_;
};  // end of class SeparableFilterDriver<FilterType, KernelSize>

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_SEPARABLE_FILTER_DRIVER_SC_H
