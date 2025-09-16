// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTER_SEPARABLE_FILTER_NEON_H
#define KLEIDICV_FILTER_SEPARABLE_FILTER_NEON_H

#include "kleidicv/neon.h"
#include "separable_filter_loader_15x15.h"
#include "separable_filter_loader_21x21.h"
#include "separable_filter_loader_3x3.h"
#include "separable_filter_loader_5x5.h"
#include "separable_filter_loader_7x7.h"

namespace KLEIDICV_TARGET_NAMESPACE {

template <size_t KSize>
struct ZeroOffsetsForKRows;

template <>
struct ZeroOffsetsForKRows<3UL> : ZeroOffsets_3_rows {};
template <>
struct ZeroOffsetsForKRows<5UL> : ZeroOffsets_5_rows {};
template <>
struct ZeroOffsetsForKRows<7UL> : ZeroOffsets_7_rows {};
template <>
struct ZeroOffsetsForKRows<15UL> : ZeroOffsets_15_rows {};
template <>
struct ZeroOffsetsForKRows<21UL> : ZeroOffsets_21_rows {};

template <typename InnerFilterType, size_t KSize,
          typename SeparableSourceLoaderType,
          typename SeparableBufferLoaderType>
class SeparableFilter {
 public:
  using SourceType = typename InnerFilterType::SourceType;
  using BufferType = typename InnerFilterType::BufferType;
  using DestinationType = typename InnerFilterType::DestinationType;
  using SourceVecTraits =
      typename KLEIDICV_TARGET_NAMESPACE::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using BufferVecTraits =
      typename KLEIDICV_TARGET_NAMESPACE::VecTraits<BufferType>;
  using BufferVectorType = typename BufferVecTraits::VectorType;
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo<SourceType, KSize>;
  using BorderType = FixedBorderType;
  using BorderOffsets = typename BorderInfoType::Offsets;
  static constexpr size_t margin = KSize / 2;

  explicit SeparableFilter(InnerFilterType filter) : filter_{filter} {}

  void process_vertical(size_t width, Rows<const SourceType> src_rows,
                        Rows<BufferType> dst_rows,
                        BorderOffsets border_offsets) const {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         SourceVecTraits::num_lanes()};
    if constexpr (KSize < 5) {
      loop.unroll_twice([&](size_t index) {
        SourceVectorType src_a[KSize], src_b[KSize];

        auto KernelWindow_a = [&](size_t row) -> SourceVectorType& {
          return src_a[row];
        };
        auto KernelWindow_b = [&](size_t row) -> SourceVectorType& {
          return src_b[row];
        };
        auto load_array_element = [](const SourceType& x) { return vld1q(&x); };

        SeparableSourceLoaderType::double_load_window(
            KernelWindow_a, KernelWindow_b, load_array_element, src_rows,
            border_offsets, ZeroOffsetsForK{}, index);

        filter_.vertical_vector_path(src_a, &dst_rows[index]);
        filter_.vertical_vector_path(
            src_b, &dst_rows[index + SourceVecTraits::num_lanes()]);
      });
    }

    loop.unroll_once([&](size_t index) {
      SourceVectorType src[KSize];

      auto KernelWindow = [&](size_t row) -> SourceVectorType& {
        return src[row];
      };
      auto load_array_element = [](const SourceType& x) { return vld1q(&x); };

      SeparableSourceLoaderType::load_window(KernelWindow, load_array_element,
                                             src_rows, border_offsets,
                                             ZeroOffsetsForK{}, index);

      filter_.vertical_vector_path(src, &dst_rows[index]);
    });

    if constexpr (KSize < 21) {
      loop.tail([&](size_t index) {
        SourceType src[KSize];

        auto KernelWindow = [&](size_t row) -> SourceType& { return src[row]; };
        auto load_array_element = [&](const SourceType& x) { return x; };

        SeparableSourceLoaderType::load_window(KernelWindow, load_array_element,
                                               src_rows, border_offsets,
                                               ZeroOffsetsForK{}, index);

        filter_.vertical_scalar_path(src, &dst_rows[index]);
      });
    }
  }

  void process_horizontal(size_t width, Rows<const BufferType> src_rows,
                          Rows<DestinationType> dst_rows,
                          BorderOffsets border_offsets) const {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         BufferVecTraits::num_lanes()};

    loop.unroll_twice([&](size_t index) {
      BufferVectorType src_a[KSize], src_b[KSize];

      auto KernelWindow_a = [&](size_t row) -> BufferVectorType& {
        return src_a[row];
      };
      auto KernelWindow_b = [&](size_t row) -> BufferVectorType& {
        return src_b[row];
      };
      auto load_array_element = [](const BufferType& x) { return vld1q(&x); };

      SeparableBufferLoaderType::double_load_window(
          KernelWindow_a, KernelWindow_b, load_array_element, src_rows,
          ZeroOffsetsForK{}, border_offsets, index);

      filter_.horizontal_vector_path(src_a, &dst_rows[index]);
      filter_.horizontal_vector_path(
          src_b, &dst_rows[index + BufferVecTraits::num_lanes()]);
    });

    loop.unroll_once([&](size_t index) {
      BufferVectorType src[KSize];

      auto KernelWindow = [&](size_t row) -> BufferVectorType& {
        return src[row];
      };
      auto load_array_element = [](const BufferType& x) { return vld1q(&x); };

      SeparableBufferLoaderType::load_window(KernelWindow, load_array_element,
                                             src_rows, ZeroOffsetsForK{},
                                             border_offsets, index);

      filter_.horizontal_vector_path(src, &dst_rows[index]);
    });

    loop.tail([&](size_t index) {
      process_horizontal_scalar(src_rows, dst_rows, border_offsets, index);
    });
  }

  void process_horizontal_borders(Rows<const BufferType> src_rows,
                                  Rows<DestinationType> dst_rows,
                                  BorderOffsets border_offsets) const {
    for (size_t index = 0; index < src_rows.channels(); ++index) {
      disable_loop_vectorization();
      process_horizontal_scalar(src_rows, dst_rows, border_offsets, index);
    }
  }

 private:
  void process_horizontal_scalar(Rows<const BufferType> src_rows,
                                 Rows<DestinationType> dst_rows,
                                 BorderOffsets border_offsets,
                                 size_t index) const {
    BufferType src[KSize];

    auto KernelWindow = [&](size_t row) -> BufferType& { return src[row]; };
    auto load_array_element = [&](const BufferType& x) { return x; };

    SeparableBufferLoaderType::load_window(KernelWindow, load_array_element,
                                           src_rows, ZeroOffsetsForK{},
                                           border_offsets, index);

    filter_.horizontal_scalar_path(src, &dst_rows[index]);
  }

  using ZeroOffsetsForK = ZeroOffsetsForKRows<KSize>;
  InnerFilterType filter_;
};

template <typename InnerFilterType>
using SeparableFilter3x3 = SeparableFilter<
    InnerFilterType, 3UL,
    SeparableFilterLoader3x3<typename InnerFilterType::SourceType>,
    SeparableFilterLoader3x3<typename InnerFilterType::BufferType>>;

template <typename InnerFilterType>
using SeparableFilter5x5 = SeparableFilter<
    InnerFilterType, 5UL,
    SeparableFilterLoader5x5<typename InnerFilterType::SourceType>,
    SeparableFilterLoader5x5<typename InnerFilterType::BufferType>>;

template <typename InnerFilterType>
using SeparableFilter7x7 = SeparableFilter<
    InnerFilterType, 7UL,
    SeparableFilterLoader7x7<typename InnerFilterType::SourceType>,
    SeparableFilterLoader7x7<typename InnerFilterType::BufferType>>;

template <typename InnerFilterType>
using SeparableFilter15x15 = SeparableFilter<
    InnerFilterType, 15UL,
    SeparableFilterLoader15x15<typename InnerFilterType::SourceType>,
    SeparableFilterLoader15x15<typename InnerFilterType::BufferType>>;

template <typename InnerFilterType>
using SeparableFilter21x21 = SeparableFilter<
    InnerFilterType, 21UL,
    SeparableFilterLoader21x21<typename InnerFilterType::SourceType>,
    SeparableFilterLoader21x21<typename InnerFilterType::BufferType>>;

template <size_t KSize, typename InnerFilter>
struct SeparableFor;

template <typename InnerFilter>
struct SeparableFor<3, InnerFilter> {
  using type = SeparableFilter3x3<InnerFilter>;
};

template <typename InnerFilter>
struct SeparableFor<5, InnerFilter> {
  using type = SeparableFilter5x5<InnerFilter>;
};

template <typename InnerFilter>
struct SeparableFor<7, InnerFilter> {
  using type = SeparableFilter7x7<InnerFilter>;
};

template <typename InnerFilter>
struct SeparableFor<15, InnerFilter> {
  using type = SeparableFilter15x15<InnerFilter>;
};

template <typename InnerFilter>
struct SeparableFor<21, InnerFilter> {
  using type = SeparableFilter21x21<InnerFilter>;
};

template <size_t KSize, typename InnerFilter>
using SeparableForT = typename SeparableFor<KSize, InnerFilter>::type;

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_FILTER_SEPARABLE_FILTER_NEON_H
