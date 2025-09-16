// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTER_SEPARABLE_FILTER_SC_H
#define KLEIDICV_FILTER_SEPARABLE_FILTER_SC_H

#include "kleidicv/sve2.h"
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

template <typename SourceType, typename DestinationType, typename BufferType,
          typename LoaderSourceType, typename LoaderBufferType>
class SeparableFilter3x3VectorOperations {
 public:
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo<SourceType, 3>;
  using BorderOffsets = typename BorderInfoType::Offsets;
  using BufferVecTraits =
      typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<BufferType>;

  template <typename InnerFilterType, typename SourceVectorType>
  static void vertical_vector_path(
      svbool_t pg, Rows<const SourceType> src_rows, Rows<BufferType> dst_rows,
      BorderOffsets border_offsets, size_t index,
      const InnerFilterType& filter_) KLEIDICV_STREAMING {
    SourceVectorType src_0, src_1, src_2;
    std::reference_wrapper<SourceVectorType> sources[3] = {src_0, src_1, src_2};

    auto KernelWindow =
        [&](size_t row)
            KLEIDICV_STREAMING -> SourceVectorType& { return sources[row]; };
    auto load_array_element = [&](const SourceType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderSourceType::load_window(KernelWindow, load_array_element, src_rows,
                                  border_offsets, ZeroOffsets_3_rows{}, index);

    filter_.vertical_vector_path(pg, sources, &dst_rows[index]);
  }

  template <typename InnerFilterType, typename BufferVectorType>
  static void horizontal_vector_path_2x(
      svbool_t pg, Rows<const BufferType> src_rows,
      Rows<DestinationType> dst_rows, BorderOffsets border_offsets,
      size_t index, const InnerFilterType& filter_) KLEIDICV_STREAMING {
    BufferVectorType src_0_0, src_1_0, src_0_1, src_1_1, src_0_2, src_1_2;
    std::reference_wrapper<BufferVectorType> sources_0[3] = {src_0_0, src_0_1,
                                                             src_0_2};
    std::reference_wrapper<BufferVectorType> sources_1[3] = {src_1_0, src_1_1,
                                                             src_1_2};

    auto KernelWindow_a =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources_0[row]; };
    auto KernelWindow_b =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources_1[row]; };
    auto load_array_element = [&](const BufferType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderBufferType::double_load_window(
        KernelWindow_a, KernelWindow_b, load_array_element, src_rows,
        ZeroOffsets_3_rows{}, border_offsets, index);

    filter_.horizontal_vector_path(pg, sources_0, &dst_rows[index]);
    filter_.horizontal_vector_path(
        pg, sources_1, &dst_rows[index + BufferVecTraits::num_lanes()]);
  }

  template <typename InnerFilterType, typename BufferVectorType>
  static void horizontal_vector_path(
      svbool_t pg, Rows<const BufferType> src_rows,
      Rows<DestinationType> dst_rows, BorderOffsets border_offsets,
      size_t index, const InnerFilterType& filter_) KLEIDICV_STREAMING {
    BufferVectorType src_0, src_1, src_2;
    std::reference_wrapper<BufferVectorType> sources[3] = {src_0, src_1, src_2};

    auto KernelWindow =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources[row]; };
    auto load_array_element = [&](const BufferType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderBufferType::load_window(KernelWindow, load_array_element, src_rows,
                                  ZeroOffsets_3_rows{}, border_offsets, index);

    filter_.horizontal_vector_path(pg, sources, &dst_rows[index]);
  }
};

template <typename SourceType, typename DestinationType, typename BufferType,
          typename LoaderSourceType, typename LoaderBufferType>
class SeparableFilter5x5VectorOperations {
 public:
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo<SourceType, 5>;
  using BorderOffsets = typename BorderInfoType::Offsets;
  using BufferVecTraits =
      typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<BufferType>;

  template <typename InnerFilterType, typename SourceVectorType>
  static void vertical_vector_path(
      svbool_t pg, Rows<const SourceType> src_rows, Rows<BufferType> dst_rows,
      BorderOffsets border_offsets, size_t index,
      const InnerFilterType& filter_) KLEIDICV_STREAMING {
    SourceVectorType src_0, src_1, src_2, src_3, src_4;
    std::reference_wrapper<SourceVectorType> sources[5] = {src_0, src_1, src_2,
                                                           src_3, src_4};
    auto KernelWindow =
        [&](size_t row)
            KLEIDICV_STREAMING -> SourceVectorType& { return sources[row]; };
    auto load_array_element = [&](const SourceType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderSourceType::load_window(KernelWindow, load_array_element, src_rows,
                                  border_offsets, ZeroOffsets_5_rows{}, index);

    filter_.vertical_vector_path(pg, sources, &dst_rows[index]);
  }

  template <typename InnerFilterType, typename BufferVectorType>
  static void horizontal_vector_path_2x(
      svbool_t pg, Rows<const BufferType> src_rows,
      Rows<DestinationType> dst_rows, BorderOffsets border_offsets,
      size_t index, const InnerFilterType& filter_) KLEIDICV_STREAMING {
    BufferVectorType src_0_0, src_1_0, src_0_1, src_1_1, src_0_2, src_1_2,
        src_0_3, src_1_3, src_0_4, src_1_4;
    std::reference_wrapper<BufferVectorType> sources_0[5] = {
        src_0_0, src_0_1, src_0_2, src_0_3, src_0_4};
    std::reference_wrapper<BufferVectorType> sources_1[5] = {
        src_1_0, src_1_1, src_1_2, src_1_3, src_1_4};

    auto KernelWindow_a =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources_0[row]; };
    auto KernelWindow_b =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources_1[row]; };
    auto load_array_element = [&](const BufferType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderBufferType::double_load_window(
        KernelWindow_a, KernelWindow_b, load_array_element, src_rows,
        ZeroOffsets_5_rows{}, border_offsets, index);

    filter_.horizontal_vector_path(pg, sources_0, &dst_rows[index]);
    filter_.horizontal_vector_path(
        pg, sources_1, &dst_rows[index + BufferVecTraits::num_lanes()]);
  }

  template <typename InnerFilterType, typename BufferVectorType>
  static void horizontal_vector_path(
      svbool_t pg, Rows<const BufferType> src_rows,
      Rows<DestinationType> dst_rows, BorderOffsets border_offsets,
      size_t index, const InnerFilterType& filter_) KLEIDICV_STREAMING {
    BufferVectorType src_0, src_1, src_2, src_3, src_4;
    std::reference_wrapper<BufferVectorType> sources[5] = {src_0, src_1, src_2,
                                                           src_3, src_4};
    auto KernelWindow =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources[row]; };
    auto load_array_element = [&](const BufferType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderBufferType::load_window(KernelWindow, load_array_element, src_rows,
                                  ZeroOffsets_5_rows{}, border_offsets, index);

    filter_.horizontal_vector_path(pg, sources, &dst_rows[index]);
  }
};

template <typename SourceType, typename DestinationType, typename BufferType,
          typename LoaderSourceType, typename LoaderBufferType>
class SeparableFilter7x7VectorOperations {
 public:
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo<SourceType, 7>;
  using BorderOffsets = typename BorderInfoType::Offsets;
  using BufferVecTraits =
      typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<BufferType>;

  template <typename InnerFilterType, typename SourceVectorType>
  static void vertical_vector_path(
      svbool_t pg, Rows<const SourceType> src_rows, Rows<BufferType> dst_rows,
      BorderOffsets border_offsets, size_t index,
      const InnerFilterType& filter_) KLEIDICV_STREAMING {
    SourceVectorType src_0, src_1, src_2, src_3, src_4, src_5, src_6;
    std::reference_wrapper<SourceVectorType> sources[7] = {
        src_0, src_1, src_2, src_3, src_4, src_5, src_6};

    auto KernelWindow =
        [&](size_t row)
            KLEIDICV_STREAMING -> SourceVectorType& { return sources[row]; };
    auto load_array_element = [&](const SourceType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderSourceType::load_window(KernelWindow, load_array_element, src_rows,
                                  border_offsets, ZeroOffsets_7_rows{}, index);

    filter_.vertical_vector_path(pg, sources, &dst_rows[index]);
  }

  template <typename InnerFilterType, typename BufferVectorType>
  static void horizontal_vector_path_2x(
      svbool_t pg, Rows<const BufferType> src_rows,
      Rows<DestinationType> dst_rows, BorderOffsets border_offsets,
      size_t index, const InnerFilterType& filter_) KLEIDICV_STREAMING {
    BufferVectorType src_0_0, src_1_0, src_0_1, src_1_1, src_0_2, src_1_2,
        src_0_3, src_1_3, src_0_4, src_1_4, src_0_5, src_1_5, src_0_6, src_1_6;
    std::reference_wrapper<BufferVectorType> sources_0[7] = {
        src_0_0, src_0_1, src_0_2, src_0_3, src_0_4, src_0_5, src_0_6};
    std::reference_wrapper<BufferVectorType> sources_1[7] = {
        src_1_0, src_1_1, src_1_2, src_1_3, src_1_4, src_1_5, src_1_6};

    auto KernelWindow_a =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources_0[row]; };
    auto KernelWindow_b =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources_1[row]; };
    auto load_array_element = [&](const BufferType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderBufferType::double_load_window(
        KernelWindow_a, KernelWindow_b, load_array_element, src_rows,
        ZeroOffsets_7_rows{}, border_offsets, index);

    filter_.horizontal_vector_path(pg, sources_0, &dst_rows[index]);
    filter_.horizontal_vector_path(
        pg, sources_1, &dst_rows[index + BufferVecTraits::num_lanes()]);
  }

  template <typename InnerFilterType, typename BufferVectorType>
  static void horizontal_vector_path(
      svbool_t pg, Rows<const BufferType> src_rows,
      Rows<DestinationType> dst_rows, BorderOffsets border_offsets,
      size_t index, const InnerFilterType& filter_) KLEIDICV_STREAMING {
    BufferVectorType src_0, src_1, src_2, src_3, src_4, src_5, src_6;
    std::reference_wrapper<BufferVectorType> sources[7] = {
        src_0, src_1, src_2, src_3, src_4, src_5, src_6};

    auto KernelWindow =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources[row]; };
    auto load_array_element = [&](const BufferType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderBufferType::load_window(KernelWindow, load_array_element, src_rows,
                                  ZeroOffsets_7_rows{}, border_offsets, index);

    filter_.horizontal_vector_path(pg, sources, &dst_rows[index]);
  }
};

template <typename SourceType, typename DestinationType, typename BufferType,
          typename LoaderSourceType, typename LoaderBufferType>
class SeparableFilter15x15VectorOperations {
 public:
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo<SourceType, 15>;
  using BorderOffsets = typename BorderInfoType::Offsets;
  using BufferVecTraits =
      typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<BufferType>;

  template <typename InnerFilterType, typename SourceVectorType>
  static void vertical_vector_path(
      svbool_t pg, Rows<const SourceType> src_rows, Rows<BufferType> dst_rows,
      BorderOffsets border_offsets, size_t index,
      const InnerFilterType& filter_) KLEIDICV_STREAMING {
    SourceVectorType src_0, src_1, src_2, src_3, src_4, src_5, src_6, src_7,
        src_8, src_9, src_10, src_11, src_12, src_13, src_14;
    std::reference_wrapper<SourceVectorType> sources[15] = {
        src_0, src_1, src_2,  src_3,  src_4,  src_5,  src_6, src_7,
        src_8, src_9, src_10, src_11, src_12, src_13, src_14};

    auto KernelWindow =
        [&](size_t row)
            KLEIDICV_STREAMING -> SourceVectorType& { return sources[row]; };
    auto load_array_element = [&](const SourceType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderSourceType::load_window(KernelWindow, load_array_element, src_rows,
                                  border_offsets, ZeroOffsets_15_rows{}, index);

    filter_.vertical_vector_path(pg, sources, &dst_rows[index]);
  }

  template <typename InnerFilterType, typename BufferVectorType>
  static void horizontal_vector_path_2x(
      svbool_t pg, Rows<const BufferType> src_rows,
      Rows<DestinationType> dst_rows, BorderOffsets border_offsets,
      size_t index, const InnerFilterType& filter_) KLEIDICV_STREAMING {
    BufferVectorType src_0_0, src_0_1, src_0_2, src_0_3, src_0_4, src_0_5,
        src_0_6, src_0_7, src_0_8, src_0_9, src_0_10, src_0_11, src_0_12,
        src_0_13, src_0_14, src_1_0, src_1_1, src_1_2, src_1_3, src_1_4,
        src_1_5, src_1_6, src_1_7, src_1_8, src_1_9, src_1_10, src_1_11,
        src_1_12, src_1_13, src_1_14;
    std::reference_wrapper<BufferVectorType> sources_0[15] = {
        src_0_0,  src_0_1,  src_0_2,  src_0_3,  src_0_4,
        src_0_5,  src_0_6,  src_0_7,  src_0_8,  src_0_9,
        src_0_10, src_0_11, src_0_12, src_0_13, src_0_14};
    std::reference_wrapper<BufferVectorType> sources_1[15] = {
        src_1_0,  src_1_1,  src_1_2,  src_1_3,  src_1_4,
        src_1_5,  src_1_6,  src_1_7,  src_1_8,  src_1_9,
        src_1_10, src_1_11, src_1_12, src_1_13, src_1_14};

    auto KernelWindow_a =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources_0[row]; };
    auto KernelWindow_b =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources_1[row]; };
    auto load_array_element = [&](const BufferType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderBufferType::double_load_window(
        KernelWindow_a, KernelWindow_b, load_array_element, src_rows,
        ZeroOffsets_15_rows{}, border_offsets, index);

    filter_.horizontal_vector_path(pg, sources_0, &dst_rows[index]);
    filter_.horizontal_vector_path(
        pg, sources_1, &dst_rows[index + BufferVecTraits::num_lanes()]);
  }

  template <typename InnerFilterType, typename BufferVectorType>
  static void horizontal_vector_path(
      svbool_t pg, Rows<const BufferType> src_rows,
      Rows<DestinationType> dst_rows, BorderOffsets border_offsets,
      size_t index, const InnerFilterType& filter_) KLEIDICV_STREAMING {
    BufferVectorType src_0, src_1, src_2, src_3, src_4, src_5, src_6, src_7,
        src_8, src_9, src_10, src_11, src_12, src_13, src_14;
    std::reference_wrapper<BufferVectorType> sources[15] = {
        src_0, src_1, src_2,  src_3,  src_4,  src_5,  src_6, src_7,
        src_8, src_9, src_10, src_11, src_12, src_13, src_14};

    auto KernelWindow =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources[row]; };

    auto load_array_element = [&](const BufferType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderBufferType::load_window(KernelWindow, load_array_element, src_rows,
                                  ZeroOffsets_15_rows{}, border_offsets, index);

    filter_.horizontal_vector_path(pg, sources, &dst_rows[index]);
  }
};

template <typename SourceType, typename DestinationType, typename BufferType,
          typename LoaderSourceType, typename LoaderBufferType>
class SeparableFilter21x21VectorOperations {
 public:
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo<SourceType, 21>;
  using BorderOffsets = typename BorderInfoType::Offsets;
  using BufferVecTraits =
      typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<BufferType>;

  template <typename InnerFilterType, typename SourceVectorType>
  static void vertical_vector_path(
      svbool_t pg, Rows<const SourceType> src_rows, Rows<BufferType> dst_rows,
      BorderOffsets border_offsets, size_t index,
      const InnerFilterType& filter_) KLEIDICV_STREAMING {
    SourceVectorType src_0, src_1, src_2, src_3, src_4, src_5, src_6, src_7,
        src_8, src_9, src_10, src_11, src_12, src_13, src_14, src_15, src_16,
        src_17, src_18, src_19, src_20;
    std::reference_wrapper<SourceVectorType> sources[21] = {
        src_0,  src_1,  src_2,  src_3,  src_4,  src_5,  src_6,
        src_7,  src_8,  src_9,  src_10, src_11, src_12, src_13,
        src_14, src_15, src_16, src_17, src_18, src_19, src_20};

    auto KernelWindow =
        [&](size_t row)
            KLEIDICV_STREAMING -> SourceVectorType& { return sources[row]; };
    auto load_array_element = [&](const SourceType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderSourceType::load_window(KernelWindow, load_array_element, src_rows,
                                  border_offsets, ZeroOffsets_21_rows{}, index);

    filter_.vertical_vector_path(pg, sources, &dst_rows[index]);
  }

  template <typename InnerFilterType, typename BufferVectorType>
  static void horizontal_vector_path_2x(
      svbool_t pg, Rows<const BufferType> src_rows,
      Rows<DestinationType> dst_rows, BorderOffsets border_offsets,
      size_t index, const InnerFilterType& filter_) KLEIDICV_STREAMING {
    BufferVectorType src_0_0, src_0_1, src_0_2, src_0_3, src_0_4, src_0_5,
        src_0_6, src_0_7, src_0_8, src_0_9, src_0_10, src_0_11, src_0_12,
        src_0_13, src_0_14, src_0_15, src_0_16, src_0_17, src_0_18, src_0_19,
        src_0_20, src_1_0, src_1_1, src_1_2, src_1_3, src_1_4, src_1_5, src_1_6,
        src_1_7, src_1_8, src_1_9, src_1_10, src_1_11, src_1_12, src_1_13,
        src_1_14, src_1_15, src_1_16, src_1_17, src_1_18, src_1_19, src_1_20;
    std::reference_wrapper<BufferVectorType> sources_0[21] = {
        src_0_0,  src_0_1,  src_0_2,  src_0_3,  src_0_4,  src_0_5,  src_0_6,
        src_0_7,  src_0_8,  src_0_9,  src_0_10, src_0_11, src_0_12, src_0_13,
        src_0_14, src_0_15, src_0_16, src_0_17, src_0_18, src_0_19, src_0_20};
    std::reference_wrapper<BufferVectorType> sources_1[21] = {
        src_1_0,  src_1_1,  src_1_2,  src_1_3,  src_1_4,  src_1_5,  src_1_6,
        src_1_7,  src_1_8,  src_1_9,  src_1_10, src_1_11, src_1_12, src_1_13,
        src_1_14, src_1_15, src_1_16, src_1_17, src_1_18, src_1_19, src_1_20};

    auto KernelWindow_a =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources_0[row]; };
    auto KernelWindow_b =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources_1[row]; };
    auto load_array_element = [&](const BufferType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderBufferType::double_load_window(
        KernelWindow_a, KernelWindow_b, load_array_element, src_rows,
        ZeroOffsets_21_rows{}, border_offsets, index);

    filter_.horizontal_vector_path(pg, sources_0, &dst_rows[index]);
    filter_.horizontal_vector_path(
        pg, sources_1, &dst_rows[index + BufferVecTraits::num_lanes()]);
  }

  template <typename InnerFilterType, typename BufferVectorType>
  static void horizontal_vector_path(
      svbool_t pg, Rows<const BufferType> src_rows,
      Rows<DestinationType> dst_rows, BorderOffsets border_offsets,
      size_t index, const InnerFilterType& filter_) KLEIDICV_STREAMING {
    BufferVectorType src_0, src_1, src_2, src_3, src_4, src_5, src_6, src_7,
        src_8, src_9, src_10, src_11, src_12, src_13, src_14, src_15, src_16,
        src_17, src_18, src_19, src_20;
    std::reference_wrapper<BufferVectorType> sources[21] = {
        src_0,  src_1,  src_2,  src_3,  src_4,  src_5,  src_6,
        src_7,  src_8,  src_9,  src_10, src_11, src_12, src_13,
        src_14, src_15, src_16, src_17, src_18, src_19, src_20};

    auto KernelWindow =
        [&](size_t row)
            KLEIDICV_STREAMING -> BufferVectorType& { return sources[row]; };
    auto load_array_element = [&](const BufferType& x)
                                  KLEIDICV_STREAMING { return svld1(pg, &x); };

    LoaderBufferType::load_window(KernelWindow, load_array_element, src_rows,
                                  ZeroOffsets_21_rows{}, border_offsets, index);

    filter_.horizontal_vector_path(pg, sources, &dst_rows[index]);
  }
};

template <typename InnerFilterType, size_t KSize,
          typename VectorOperationProviderType, typename BufferLoader>
class SeparableFilter {
 public:
  using SourceType = typename InnerFilterType::SourceType;
  using BufferType = typename InnerFilterType::BufferType;
  using DestinationType = typename InnerFilterType::DestinationType;
  using SourceVecTraits =
      typename KLEIDICV_TARGET_NAMESPACE::VecTraits<SourceType>;
  using DestinationVecTraits =
      typename KLEIDICV_TARGET_NAMESPACE::VecTraits<DestinationType>;
  using BufferVecTraits =
      typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<BufferType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using DestinationVectorType = typename DestinationVecTraits::VectorType;
  using BufferVectorType = typename BufferVecTraits::VectorType;
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo<SourceType, KSize>;
  using BorderType = FixedBorderType;
  using BorderOffsets = typename BorderInfoType::Offsets;
  static constexpr size_t margin = KSize / 2UL;

  explicit SeparableFilter(InnerFilterType filter) KLEIDICV_STREAMING
      : filter_{filter} {}

  void process_vertical(size_t width, Rows<const SourceType> src_rows,
                        Rows<BufferType> dst_rows,
                        BorderOffsets border_offsets) const KLEIDICV_STREAMING {
    LoopUnroll2 loop{width * src_rows.channels(), SourceVecTraits::num_lanes()};

    loop.unroll_once([&](size_t index) KLEIDICV_STREAMING {
      svbool_t pg_all = SourceVecTraits::svptrue();
      VectorOperationProviderType::template vertical_vector_path<
          InnerFilterType, SourceVectorType>(pg_all, src_rows, dst_rows,
                                             border_offsets, index, filter_);
    });

    loop.remaining([&](size_t index, size_t length) KLEIDICV_STREAMING {
      svbool_t pg = SourceVecTraits::svwhilelt(index, length);
      VectorOperationProviderType::template vertical_vector_path<
          InnerFilterType, SourceVectorType>(pg, src_rows, dst_rows,
                                             border_offsets, index, filter_);
    });
  }

  void process_horizontal(size_t width, Rows<const BufferType> src_rows,
                          Rows<DestinationType> dst_rows,
                          BorderOffsets border_offsets) const
      KLEIDICV_STREAMING {
    svbool_t pg_all = BufferVecTraits::svptrue();
    LoopUnroll2 loop{width * src_rows.channels(), BufferVecTraits::num_lanes()};

    loop.unroll_twice([&](size_t index) KLEIDICV_STREAMING {
      VectorOperationProviderType::template horizontal_vector_path_2x<
          InnerFilterType, BufferVectorType>(pg_all, src_rows, dst_rows,
                                             border_offsets, index, filter_);
    });

    loop.unroll_once([&](size_t index) KLEIDICV_STREAMING {
      VectorOperationProviderType::template horizontal_vector_path<
          InnerFilterType, BufferVectorType>(pg_all, src_rows, dst_rows,
                                             border_offsets, index, filter_);
    });

    loop.remaining([&](size_t index, size_t length) KLEIDICV_STREAMING {
      svbool_t pg = BufferVecTraits::svwhilelt(index, length);
      VectorOperationProviderType::template horizontal_vector_path<
          InnerFilterType, BufferVectorType>(pg, src_rows, dst_rows,
                                             border_offsets, index, filter_);
    });
  }

  // Processing of horizontal borders is always scalar because border offsets
  // change for each and every element in the border.
  void process_horizontal_borders(
      Rows<const BufferType> src_rows, Rows<DestinationType> dst_rows,
      BorderOffsets border_offsets) const KLEIDICV_STREAMING {
    for (size_t index = 0; index < src_rows.channels(); ++index) {
      disable_loop_vectorization();
      process_horizontal_border(src_rows, dst_rows, border_offsets, index);
    }
  }

 private:
  void process_horizontal_border(Rows<const BufferType> src_rows,
                                 Rows<DestinationType> dst_rows,
                                 BorderOffsets border_offsets,
                                 size_t index) const KLEIDICV_STREAMING {
    BufferType src[KSize];
    auto KernelWindow = [&](size_t row) KLEIDICV_STREAMING -> BufferType& {
      return src[row];
    };

    auto load_array_element = [&](const BufferType& x)
                                  KLEIDICV_STREAMING { return x; };

    BufferLoader::load_window(KernelWindow, load_array_element, src_rows,
                              ZeroOffsetsForK{}, border_offsets, index);

    filter_.horizontal_scalar_path(src, &dst_rows[index]);
  }
  using ZeroOffsetsForK = ZeroOffsetsForKRows<KSize>;
  InnerFilterType filter_;
};

// Shorthand for 3x3 2D filters driver type.
template <class InnerFilterType>
using SeparableFilter3x3 = SeparableFilter<
    InnerFilterType, 3UL,
    SeparableFilter3x3VectorOperations<
        typename InnerFilterType::SourceType,
        typename InnerFilterType::DestinationType,
        typename InnerFilterType::BufferType,
        SeparableFilterLoader3x3<typename InnerFilterType::SourceType>,
        SeparableFilterLoader3x3<typename InnerFilterType::BufferType>>,
    SeparableFilterLoader3x3<typename InnerFilterType::BufferType>>;

template <class InnerFilterType>
using SeparableFilter5x5 = SeparableFilter<
    InnerFilterType, 5UL,
    SeparableFilter5x5VectorOperations<
        typename InnerFilterType::SourceType,
        typename InnerFilterType::DestinationType,
        typename InnerFilterType::BufferType,
        SeparableFilterLoader5x5<typename InnerFilterType::SourceType>,
        SeparableFilterLoader5x5<typename InnerFilterType::BufferType>>,
    SeparableFilterLoader5x5<typename InnerFilterType::BufferType>>;

template <class InnerFilterType>
using SeparableFilter7x7 = SeparableFilter<
    InnerFilterType, 7UL,
    SeparableFilter7x7VectorOperations<
        typename InnerFilterType::SourceType,
        typename InnerFilterType::DestinationType,
        typename InnerFilterType::BufferType,
        SeparableFilterLoader7x7<typename InnerFilterType::SourceType>,
        SeparableFilterLoader7x7<typename InnerFilterType::BufferType>>,
    SeparableFilterLoader7x7<typename InnerFilterType::BufferType>>;

template <class InnerFilterType>
using SeparableFilter15x15 = SeparableFilter<
    InnerFilterType, 15UL,
    SeparableFilter15x15VectorOperations<
        typename InnerFilterType::SourceType,
        typename InnerFilterType::DestinationType,
        typename InnerFilterType::BufferType,
        SeparableFilterLoader15x15<typename InnerFilterType::SourceType>,
        SeparableFilterLoader15x15<typename InnerFilterType::BufferType>>,
    SeparableFilterLoader15x15<typename InnerFilterType::BufferType>>;
template <class InnerFilterType>
using SeparableFilter21x21 = SeparableFilter<
    InnerFilterType, 21UL,
    SeparableFilter21x21VectorOperations<
        typename InnerFilterType::SourceType,
        typename InnerFilterType::DestinationType,
        typename InnerFilterType::BufferType,
        SeparableFilterLoader21x21<typename InnerFilterType::SourceType>,
        SeparableFilterLoader21x21<typename InnerFilterType::BufferType>>,
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

#endif  // KLEIDICV_FILTER_SEPARABLE_FILTER_SC_H
