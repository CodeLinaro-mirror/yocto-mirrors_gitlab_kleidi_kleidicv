// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_NEON_H
#define KLEIDICV_NEON_H

#include <utility>

#include "kleidicv/neon_intrinsics.h"
#include "kleidicv/operations.h"
#include "kleidicv/utils.h"
#include "kleidicv/workspace/separable.h"

namespace kleidicv::neon {

template <>
class half_element_width<uint16x8_t> {
 public:
  using type = uint8x16_t;
};

template <>
class half_element_width<uint32x4_t> {
 public:
  using type = uint16x8_t;
};

template <>
class half_element_width<uint64x2_t> {
 public:
  using type = uint32x4_t;
};

template <>
class double_element_width<uint8x16_t> {
 public:
  using type = uint16x8_t;
};

template <>
class double_element_width<uint16x8_t> {
 public:
  using type = uint32x4_t;
};

template <>
class double_element_width<uint32x4_t> {
 public:
  using type = uint64x2_t;
};

// Primary template to describe logically grouped peroperties of vectors.
template <typename ScalarType>
class VectorTypes;

template <>
class VectorTypes<int8_t> {
 public:
  using ScalarType = int8_t;
  using VectorType = int8x16_t;
  using Vector2Type = int8x16x2_t;
  using Vector3Type = int8x16x3_t;
  using Vector4Type = int8x16x4_t;
};  // end of class VectorTypes<int8_t>

template <>
class VectorTypes<uint8_t> {
 public:
  using ScalarType = uint8_t;
  using VectorType = uint8x16_t;
  using Vector2Type = uint8x16x2_t;
  using Vector3Type = uint8x16x3_t;
  using Vector4Type = uint8x16x4_t;
};  // end of class VectorTypes<uint8_t>

template <>
class VectorTypes<int16_t> {
 public:
  using ScalarType = int16_t;
  using VectorType = int16x8_t;
  using Vector2Type = int16x8x2_t;
  using Vector3Type = int16x8x3_t;
  using Vector4Type = int16x8x4_t;
};  // end of class VectorTypes<int16_t>

template <>
class VectorTypes<uint16_t> {
 public:
  using ScalarType = uint16_t;
  using VectorType = uint16x8_t;
  using Vector2Type = uint16x8x2_t;
  using Vector3Type = uint16x8x3_t;
  using Vector4Type = uint16x8x4_t;
};  // end of class VectorTypes<uint16_t>

template <>
class VectorTypes<int32_t> {
 public:
  using ScalarType = int32_t;
  using VectorType = int32x4_t;
  using Vector2Type = int32x4x2_t;
  using Vector3Type = int32x4x3_t;
  using Vector4Type = int32x4x4_t;
};  // end of class VectorTypes<int32_t>

template <>
class VectorTypes<uint32_t> {
 public:
  using ScalarType = uint32_t;
  using VectorType = uint32x4_t;
  using Vector2Type = uint32x4x2_t;
  using Vector3Type = uint32x4x3_t;
  using Vector4Type = uint32x4x4_t;
};  // end of class VectorTypes<uint32_t>

template <>
class VectorTypes<int64_t> {
 public:
  using ScalarType = int64_t;
  using VectorType = int64x2_t;
  using Vector2Type = int64x2x2_t;
  using Vector3Type = int64x2x3_t;
  using Vector4Type = int64x2x4_t;
};  // end of class VectorTypes<int64_t>

template <>
class VectorTypes<uint64_t> {
 public:
  using ScalarType = uint64_t;
  using VectorType = uint64x2_t;
  using Vector2Type = uint64x2x2_t;
  using Vector3Type = uint64x2x3_t;
  using Vector4Type = uint64x2x4_t;
};  // end of class VectorTypes<uint64_t>

// Base class for all NEON vector traits.
template <typename ScalarType>
class VecTraitsBase : public VectorTypes<ScalarType> {
 public:
  using typename VectorTypes<ScalarType>::VectorType;
  using typename VectorTypes<ScalarType>::Vector2Type;
  using typename VectorTypes<ScalarType>::Vector3Type;
  using typename VectorTypes<ScalarType>::Vector4Type;

  // Vector length in bytes.
  static constexpr size_t kVectorLength = 16;

  // Number of lanes in a vector.
  static constexpr size_t num_lanes() {
    return kVectorLength / sizeof(ScalarType);
  }

  // Loads a single vector from 'src'.
  static inline void load(const ScalarType *src, VectorType &vec) {
    vec = vld1q(&src[0]);
  }

  // Loads two consecutive vectors from 'src'.
  static inline void load(const ScalarType *src, Vector2Type &vec) {
    vec = vld1q_x2(&src[0]);
  }

  // Loads three consecutive vectors from 'src'.
  static inline void load(const ScalarType *src, Vector3Type &vec) {
    vec = vld1q_x3(&src[0]);
  }

  // Loads four consecutive vectors from 'src'.
  static inline void load(const ScalarType *src, Vector4Type &vec) {
    vec = vld1q_x4(&src[0]);
  }

  // Loads two consecutive vectors from 'src'.
  static inline void load_consecutive(const ScalarType *src, VectorType &vec_0,
                                      VectorType &vec_1) {
    vec_0 = vld1q(&src[0]);
    vec_1 = vld1q(&src[num_lanes()]);
  }

  // Loads 2x2 consecutive vectors from 'src'.
  static inline void load_consecutive(const ScalarType *src, Vector2Type &vec_0,
                                      Vector2Type &vec_1) {
    vec_0 = vld1q_x2(&src[0]);
    vec_1 = vld1q_x2(&src[num_lanes() * 2]);
  }

  // Loads 2x3 consecutive vectors from 'src'.
  static inline void load_consecutive(const ScalarType *src, Vector3Type &vec_0,
                                      Vector3Type &vec_1) {
    vec_0 = vld1q_x3(&src[0]);
    vec_1 = vld1q_x3(&src[num_lanes() * 3]);
  }

  // Loads 2x4 consecutive vectors from 'src'.
  static inline void load_consecutive(const ScalarType *src, Vector4Type &vec_0,
                                      Vector4Type &vec_1) {
    vec_0 = vld1q_x4(&src[0]);
    vec_1 = vld1q_x4(&src[num_lanes() * 4]);
  }

  // Stores a single vector to 'dst'.
  static inline void store(VectorType vec, ScalarType *dst) {
    vst1q(&dst[0], vec);
  }

  // Stores two consecutive vectors to 'dst'.
  static inline void store(Vector2Type vec, ScalarType *dst) {
    vst1q_x2(&dst[0], vec);
  }

  // Stores two consecutive vectors to 'dst'.
  static inline void store_consecutive(VectorType vec_0, VectorType vec_1,
                                       ScalarType *dst) {
    vst1q(&dst[0], vec_0);
    vst1q(&dst[num_lanes()], vec_1);
  }
};  // end of class VecTraitsBase<ScalarType>

// Available NEON vector traits.
template <typename ScalarType>
class VecTraits : public VecTraitsBase<ScalarType> {};

// NEON has no associated context yet.
using NeonContextType = Monostate;

// Adapter which simply adds context and forwards all arguments.
template <typename OperationType>
class OperationContextAdapter : public OperationBase<OperationType> {
  // Shorten rows: no need to write 'this->'.
  using OperationBase<OperationType>::operation;

 public:
  using ContextType = NeonContextType;

  explicit OperationContextAdapter(OperationType &operation)
      : OperationBase<OperationType>(operation) {}

  // Forwards vector_path_2x() calls to the inner operation.
  template <typename... ArgTypes>
  void vector_path_2x(ArgTypes &&...args) {
    operation().vector_path_2x(ContextType{}, std::forward<ArgTypes>(args)...);
  }

  // Forwards vector_path() calls to the inner operation.
  template <typename... ArgTypes>
  void vector_path(ArgTypes &&...args) {
    operation().vector_path(ContextType{}, std::forward<ArgTypes>(args)...);
  }

  // Forwards remaining_path() calls to the inner operation.
  template <typename... ArgTypes>
  void remaining_path(ArgTypes &&...args) {
    operation().remaining_path(ContextType{}, std::forward<ArgTypes>(args)...);
  }
};  // end of class OperationContextAdapter<OperationType>

// Adapter which implements remaining_path() for general NEON operations.
template <typename OperationType>
class RemainingPathAdapter : public OperationBase<OperationType> {
 public:
  using ContextType = NeonContextType;

  explicit RemainingPathAdapter(OperationType &operation)
      : OperationBase<OperationType>(operation) {}

  // Forwards remaining_path() calls to scalar_path() of the inner operation
  // element by element.
  template <typename... ColumnTypes>
  void remaining_path(ContextType ctx, size_t length, ColumnTypes... columns) {
    for (size_t index = 0; index < length; ++index) {
      disable_loop_vectorization();
      this->operation().scalar_path(ctx, columns.at(index)...);
    }
  }
};  // end of class RemainingPathAdapter<OperationType>

// Adapter which implements remaining_path() for NEON operations which
// implementation custom processing of remaining elements.
template <typename OperationType>
class RemainingPathToScalarPathAdapter : public OperationBase<OperationType> {
 public:
  using ContextType = NeonContextType;

  explicit RemainingPathToScalarPathAdapter(OperationType &operation)
      : OperationBase<OperationType>(operation) {}

  // Forwards remaining_path() calls to scalar_path() of the inner operation.
  template <typename... ArgTypes>
  void remaining_path(ArgTypes &&...args) {
    this->operation().scalar_path(std::forward<ArgTypes>(args)...);
  }
};  // end of class RemainingPathToScalarPathAdapter<OperationType>

// Shorthand for applying a generic unrolled NEON operation.
template <typename OperationType, typename... ArgTypes>
void apply_operation_by_rows(OperationType &operation, ArgTypes &&...args) {
  RemoveContextAdapter remove_context_adapter{operation};
  OperationAdapter operation_adapter{remove_context_adapter};
  RemainingPathAdapter remaining_path_adapter{operation_adapter};
  OperationContextAdapter context_adapter{remaining_path_adapter};
  RowBasedOperation row_based_operation{context_adapter};
  zip_rows(row_based_operation, std::forward<ArgTypes>(args)...);
}

// Shorthand for applying a generic unrolled and block-based NEON operation.
template <typename OperationType, typename... ArgTypes>
void apply_block_operation_by_rows(OperationType &operation,
                                   ArgTypes &&...args) {
  RemoveContextAdapter remove_context_adapter{operation};
  OperationAdapter operation_adapter{remove_context_adapter};
  RemainingPathAdapter remaining_path_adapter{operation_adapter};
  OperationContextAdapter context_adapter{remaining_path_adapter};
  RowBasedBlockOperation block_operation{context_adapter};
  zip_rows(block_operation, std::forward<ArgTypes>(args)...);
}

// Template for drivers of separable NxM filters.
template <typename FilterType, const size_t S>
class SeparableFilter;

// Driver for a separable 3x3 filter.
template <typename FilterType>
class SeparableFilter<FilterType, 3UL> {
 public:
  using SourceType = typename FilterType::SourceType;
  using BufferType = typename FilterType::BufferType;
  using DestinationType = typename FilterType::DestinationType;
  using SourceVecTraits = typename neon::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using BufferVecTraits = typename neon::VecTraits<BufferType>;
  using BufferVectorType = typename BufferVecTraits::VectorType;
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo3x3<SourceType>;
  using BorderType = FixedBorderType;
  using BorderOffsets = typename BorderInfoType::Offsets;

  explicit SeparableFilter(FilterType filter) : filter_{filter} {}

  static constexpr Margin margin() { return Margin{1UL}; }

  void process_vertical(size_t width, Rows<const SourceType> src_rows,
                        Rows<BufferType> dst_rows,
                        BorderOffsets border_offsets) const {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         SourceVecTraits::num_lanes()};

    loop.unroll_twice([&](size_t index) {
      auto src_0 = &src_rows.at(border_offsets.c0())[index];
      auto src_1 = &src_rows.at(border_offsets.c1())[index];
      auto src_2 = &src_rows.at(border_offsets.c2())[index];

      auto src_0_x2 = vld1q_x2(&src_0[0]);
      auto src_1_x2 = vld1q_x2(&src_1[0]);
      auto src_2_x2 = vld1q_x2(&src_2[0]);

      SourceVectorType src_a[3], src_b[3];
      src_a[0] = src_0_x2.val[0];
      src_b[0] = src_0_x2.val[1];
      src_a[1] = src_1_x2.val[0];
      src_b[1] = src_1_x2.val[1];
      src_a[2] = src_2_x2.val[0];
      src_b[2] = src_2_x2.val[1];

      filter_.vertical_vector_path(src_a, &dst_rows[index]);
      filter_.vertical_vector_path(
          src_b, &dst_rows[index + SourceVecTraits::num_lanes()]);
    });

    loop.unroll_once([&](size_t index) {
      SourceVectorType src[3];
      src[0] = vld1q(&src_rows.at(border_offsets.c0())[index]);
      src[1] = vld1q(&src_rows.at(border_offsets.c1())[index]);
      src[2] = vld1q(&src_rows.at(border_offsets.c2())[index]);
      filter_.vertical_vector_path(src, &dst_rows[index]);
    });

    loop.tail([&](size_t index) {
      SourceType src[3];
      src[0] = src_rows.at(border_offsets.c0())[index];
      src[1] = src_rows.at(border_offsets.c1())[index];
      src[2] = src_rows.at(border_offsets.c2())[index];
      filter_.vertical_scalar_path(src, &dst_rows[index]);
    });
  }

  void process_horizontal(size_t width, Rows<const BufferType> src_rows,
                          Rows<DestinationType> dst_rows,
                          BorderOffsets border_offsets) const {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         BufferVecTraits::num_lanes()};

    loop.unroll_twice([&](size_t index) {
      auto src_0 = &src_rows.at(0, border_offsets.c0())[index];
      auto src_1 = &src_rows.at(0, border_offsets.c1())[index];
      auto src_2 = &src_rows.at(0, border_offsets.c2())[index];

      auto src_0_x2 = vld1q_x2(&src_0[0]);
      auto src_1_x2 = vld1q_x2(&src_1[0]);
      auto src_2_x2 = vld1q_x2(&src_2[0]);

      BufferVectorType src_a[3], src_b[3];
      src_a[0] = src_0_x2.val[0];
      src_b[0] = src_0_x2.val[1];
      src_a[1] = src_1_x2.val[0];
      src_b[1] = src_1_x2.val[1];
      src_a[2] = src_2_x2.val[0];
      src_b[2] = src_2_x2.val[1];

      filter_.horizontal_vector_path(src_a, &dst_rows[index]);
      filter_.horizontal_vector_path(
          src_b, &dst_rows[index + BufferVecTraits::num_lanes()]);
    });

    loop.unroll_once([&](size_t index) {
      BufferVectorType src[3];
      src[0] = vld1q(&src_rows.at(0, border_offsets.c0())[index]);
      src[1] = vld1q(&src_rows.at(0, border_offsets.c1())[index]);
      src[2] = vld1q(&src_rows.at(0, border_offsets.c2())[index]);
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
    BufferType src[3];
    src[0] = src_rows.at(0, border_offsets.c0())[index];
    src[1] = src_rows.at(0, border_offsets.c1())[index];
    src[2] = src_rows.at(0, border_offsets.c2())[index];
    filter_.horizontal_scalar_path(src, &dst_rows[index]);
  }

  FilterType filter_;
};  // end of class SeparableFilter<FilterType, 3UL>

// Driver for a separable 5x5 filter.
template <typename FilterType>
class SeparableFilter<FilterType, 5UL> {
 public:
  using SourceType = typename FilterType::SourceType;
  using BufferType = typename FilterType::BufferType;
  using DestinationType = typename FilterType::DestinationType;
  using SourceVecTraits = typename neon::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using BufferVecTraits = typename neon::VecTraits<BufferType>;
  using BufferVectorType = typename BufferVecTraits::VectorType;
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo5x5<SourceType>;
  using BorderType = FixedBorderType;
  using BorderOffsets = typename BorderInfoType::Offsets;

  explicit SeparableFilter(FilterType filter) : filter_{filter} {}

  static constexpr Margin margin() { return Margin{2UL}; }

  void process_vertical(size_t width, Rows<const SourceType> src_rows,
                        Rows<BufferType> dst_rows,
                        BorderOffsets border_offsets) const {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         SourceVecTraits::num_lanes()};

    loop.unroll_once([&](size_t index) {
      SourceVectorType src[5];
      src[0] = vld1q(&src_rows.at(border_offsets.c0())[index]);
      src[1] = vld1q(&src_rows.at(border_offsets.c1())[index]);
      src[2] = vld1q(&src_rows.at(border_offsets.c2())[index]);
      src[3] = vld1q(&src_rows.at(border_offsets.c3())[index]);
      src[4] = vld1q(&src_rows.at(border_offsets.c4())[index]);
      filter_.vertical_vector_path(src, &dst_rows[index]);
    });

    loop.tail([&](size_t index) {
      SourceType src[5];
      src[0] = src_rows.at(border_offsets.c0())[index];
      src[1] = src_rows.at(border_offsets.c1())[index];
      src[2] = src_rows.at(border_offsets.c2())[index];
      src[3] = src_rows.at(border_offsets.c3())[index];
      src[4] = src_rows.at(border_offsets.c4())[index];
      filter_.vertical_scalar_path(src, &dst_rows[index]);
    });
  }

  void process_horizontal(size_t width, Rows<const BufferType> src_rows,
                          Rows<DestinationType> dst_rows,
                          BorderOffsets border_offsets) const {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         BufferVecTraits::num_lanes()};

    loop.unroll_twice([&](size_t index) {
      auto src_0 = &src_rows.at(0, border_offsets.c0())[index];
      auto src_1 = &src_rows.at(0, border_offsets.c1())[index];
      auto src_2 = &src_rows.at(0, border_offsets.c2())[index];
      auto src_3 = &src_rows.at(0, border_offsets.c3())[index];
      auto src_4 = &src_rows.at(0, border_offsets.c4())[index];

      BufferVectorType src_a[5], src_b[5];
      src_a[0] = vld1q(&src_0[0]);
      src_b[0] = vld1q(&src_0[BufferVecTraits::num_lanes()]);
      src_a[1] = vld1q(&src_1[0]);
      src_b[1] = vld1q(&src_1[BufferVecTraits::num_lanes()]);
      src_a[2] = vld1q(&src_2[0]);
      src_b[2] = vld1q(&src_2[BufferVecTraits::num_lanes()]);
      src_a[3] = vld1q(&src_3[0]);
      src_b[3] = vld1q(&src_3[BufferVecTraits::num_lanes()]);
      src_a[4] = vld1q(&src_4[0]);
      src_b[4] = vld1q(&src_4[BufferVecTraits::num_lanes()]);

      filter_.horizontal_vector_path(src_a, &dst_rows[index]);
      filter_.horizontal_vector_path(
          src_b, &dst_rows[index + BufferVecTraits::num_lanes()]);
    });

    loop.unroll_once([&](size_t index) {
      BufferVectorType src[5];
      src[0] = vld1q(&src_rows.at(0, border_offsets.c0())[index]);
      src[1] = vld1q(&src_rows.at(0, border_offsets.c1())[index]);
      src[2] = vld1q(&src_rows.at(0, border_offsets.c2())[index]);
      src[3] = vld1q(&src_rows.at(0, border_offsets.c3())[index]);
      src[4] = vld1q(&src_rows.at(0, border_offsets.c4())[index]);
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
    BufferType src[5];
    src[0] = src_rows.at(0, border_offsets.c0())[index];
    src[1] = src_rows.at(0, border_offsets.c1())[index];
    src[2] = src_rows.at(0, border_offsets.c2())[index];
    src[3] = src_rows.at(0, border_offsets.c3())[index];
    src[4] = src_rows.at(0, border_offsets.c4())[index];
    filter_.horizontal_scalar_path(src, &dst_rows[index]);
  }

  FilterType filter_;
};  // end of class SeparableFilter<FilterType, 5UL>

// Shorthand for 3x3 separable filters driver type.
template <class FilterType>
using SeparableFilter3x3 = SeparableFilter<FilterType, 3UL>;

// Shorthand for 5x5 separable filters driver type.
template <class FilterType>
using SeparableFilter5x5 = SeparableFilter<FilterType, 5UL>;

}  // namespace kleidicv::neon

#endif  // KLEIDICV_NEON_H
