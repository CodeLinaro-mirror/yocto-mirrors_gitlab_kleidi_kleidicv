// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_SRC_TRANSFORM_ADD_PADDING_BY_COPY_SC_H
#define KLEIDICV_SRC_TRANSFORM_ADD_PADDING_BY_COPY_SC_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "add_padding_by_copy_common.h"
#include "kleidicv/sve2.h"
#include "kleidicv/transform/add_padding_by_copy.h"

namespace KLEIDICV_TARGET_NAMESPACE {
class ContiguousByteCopier {
 public:
  static inline void copy_contiguous_bytes(
      const uint8_t *src, uint8_t *dst, size_t size,
      [[maybe_unused]] AddPaddingByCopyContext &context) KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME || KLEIDICV_TARGET_SME2
    using ByteVecTraits = VecTraits<uint8_t>;
    using VectorType = typename ByteVecTraits::VectorType;

    const size_t kStep = ByteVecTraits::num_lanes();
    Context &full_ctx = context;

    auto copy_block_pair = [&](size_t index) KLEIDICV_STREAMING {
      VectorType src_vector_0;
      VectorType src_vector_1;
      ByteVecTraits::load_consecutive(full_ctx, src + index, src_vector_0,
                                      src_vector_1);
      ByteVecTraits::store_consecutive(full_ctx, src_vector_0, src_vector_1,
                                       dst + index);
    };

    LoopUnroll2 loop{size, kStep};
    loop.unroll_twice(copy_block_pair)
        .remaining([&](size_t index, size_t length) KLEIDICV_STREAMING {
          for (; index < length; index += kStep) {
            svbool_t tail_pg = svwhilelt_b8(index, length);
            Context tail_ctx{tail_pg};
            VectorType src_vector;
            ByteVecTraits::load(tail_ctx, src + index, src_vector);
            ByteVecTraits::store(tail_ctx, src_vector, dst + index);
          }
        });
#else
    std::memcpy(static_cast<void *>(dst), static_cast<const void *>(src), size);
#endif
  }
};

template <size_t PixelSize>
class RepeatedPixelWriter : public ContiguousByteCopier {
 public:
  // Use the stripe-selected repeated-value writer for this pixel layout.
  static inline void fill_repeated_pixel(
      size_t pixel_size, const uint8_t *value_bytes, uint8_t *dst,
      size_t row_size, AddPaddingByCopyContext &context) KLEIDICV_STREAMING {
    if constexpr (PixelSize == sizeof(uint8_t)) {
      fill_single_channel<uint8_t>(value_bytes, dst, row_size, context);
    } else if constexpr (PixelSize == sizeof(uint16_t)) {
      fill_single_channel<uint16_t>(value_bytes, dst, row_size, context);
    } else if constexpr (PixelSize == sizeof(uint32_t)) {
      fill_single_channel<uint32_t>(value_bytes, dst, row_size, context);
    } else if constexpr (PixelSize == sizeof(uint64_t)) {
      fill_single_channel<uint64_t>(value_bytes, dst, row_size, context);
    } else if constexpr (PixelSize == 3 * sizeof(uint8_t)) {
      fill_interleaved_3<uint8_t>(value_bytes, dst, row_size, context);
    } else if constexpr (PixelSize == 3 * sizeof(uint16_t)) {
      fill_interleaved_3<uint16_t>(value_bytes, dst, row_size, context);
    } else if constexpr (PixelSize == 3 * sizeof(uint32_t)) {
      fill_interleaved_3<uint32_t>(value_bytes, dst, row_size, context);
    } else {
      fill_repeated_by_bytes(pixel_size, value_bytes, dst, row_size, context);
    }
  }

 private:
  // Vector path for packed single-channel pixel layouts.
  template <typename ScalarType>
  static inline void fill_single_channel(
      const uint8_t *value_bytes, uint8_t *dst, size_t row_size,
      AddPaddingByCopyContext &context) KLEIDICV_STREAMING {
    using VectorType = typename VecTraits<ScalarType>::VectorType;

    const ScalarType value = load_unaligned<ScalarType>(value_bytes, context);
    const VectorType repeated_value = VecTraits<ScalarType>::svdup(value);
    auto *typed_dst = reinterpret_cast<ScalarType *>(dst);
    const size_t count = row_size / sizeof(ScalarType);
    const size_t kStep = VecTraits<ScalarType>::num_lanes();
    Context &full_ctx = context;

    auto store_repeated_pair = [&](size_t index) KLEIDICV_STREAMING {
      store_full_pair<ScalarType>(full_ctx, repeated_value, typed_dst + index);
    };

    LoopUnroll2 loop{count, kStep};
    loop.unroll_twice(store_repeated_pair)
        .remaining([&](size_t index, size_t length) KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME || KLEIDICV_TARGET_SME2
          for (; index < length; index += kStep) {
            svbool_t tail_pg = VecTraits<ScalarType>::svwhilelt(index, length);
            Context tail_ctx{tail_pg};
            VecTraits<ScalarType>::store(tail_ctx, repeated_value,
                                         typed_dst + index);
          }
#else
          const size_t byte_index = index * sizeof(ScalarType);
          fill_repeated_by_bytes(
              sizeof(ScalarType), value_bytes, dst + byte_index,
              (length - index) * sizeof(ScalarType), context);
#endif
        });
  }

  template <typename ScalarType>
  static inline void store_full_pair(
      const Context &full_ctx,
      const typename VecTraits<ScalarType>::VectorType &value,
      ScalarType *dst) KLEIDICV_STREAMING {
    VecTraits<ScalarType>::store_consecutive(full_ctx, value, value, dst);
  }

  // Vector path for interleaved 3-channel pixel layouts.
  template <typename ScalarType>
  static inline void fill_interleaved_3(
      const uint8_t *value_bytes, uint8_t *dst, size_t row_size,
      AddPaddingByCopyContext &context) KLEIDICV_STREAMING {
    using Vector3Type = typename VecTraits<ScalarType>::Vector3Type;

    constexpr size_t kPixelStride = 3;
    constexpr size_t kPixelSize = kPixelStride * sizeof(ScalarType);

    const Vector3Type vector_value =
        svcreate3(VecTraits<ScalarType>::svdup(
                      load_unaligned<ScalarType>(value_bytes, context)),
                  VecTraits<ScalarType>::svdup(load_unaligned<ScalarType>(
                      value_bytes + sizeof(ScalarType), context)),
                  VecTraits<ScalarType>::svdup(load_unaligned<ScalarType>(
                      value_bytes + (2 * sizeof(ScalarType)), context)));
    auto *typed_dst = reinterpret_cast<ScalarType *>(dst);
    const size_t count = row_size / kPixelSize;
    const size_t kStep = VecTraits<ScalarType>::num_lanes();

    const svbool_t full_pg = context.predicate();
    auto store = [&](size_t index) KLEIDICV_STREAMING {
      svst3(full_pg, typed_dst + (kPixelStride * index), vector_value);
    };
    auto store_twice = [&](size_t index) KLEIDICV_STREAMING {
      store(index);
      store(index + kStep);
    };

    LoopUnroll2 loop{count, kStep};
    loop.unroll_twice(store_twice)
        .remaining([&](size_t index, size_t length) KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME || KLEIDICV_TARGET_SME2
          for (; index < length; index += kStep) {
            svbool_t tail_pg = VecTraits<ScalarType>::svwhilelt(index, length);
            svst3(tail_pg, typed_dst + (kPixelStride * index), vector_value);
          }
#else
          const size_t byte_index = index * kPixelSize;
          fill_repeated_by_bytes(kPixelSize, value_bytes, dst + byte_index,
                                 (length - index) * kPixelSize, context);
#endif
        });
  }

  static inline void fill_repeated_by_bytes(
      size_t pixel_size, const uint8_t *value_bytes, uint8_t *dst,
      size_t row_size, AddPaddingByCopyContext &context) KLEIDICV_STREAMING {
    for (size_t offset = 0; offset < row_size; offset += pixel_size) {
      copy_contiguous_bytes(value_bytes, dst + offset, pixel_size, context);
    }
  }

  // Load a scalar from byte-addressed storage without assuming alignment.
  template <typename ScalarType>
  static inline ScalarType load_unaligned(
      const uint8_t *src, AddPaddingByCopyContext &context) KLEIDICV_STREAMING {
    ScalarType value;
    copy_contiguous_bytes(src, reinterpret_cast<uint8_t *>(&value),
                          sizeof(ScalarType), context);
    return value;
  }
};

template <size_t PixelSize>
class ReversedPixelCopier : public ContiguousByteCopier {
 public:
  // Copy row_size bytes using the stripe-selected reverse-copy strategy.
  static inline void copy_reversed_pixels(
      size_t pixel_size, const uint8_t *src_last, uint8_t *dst, size_t row_size,
      AddPaddingByCopyContext &context) KLEIDICV_STREAMING {
    if constexpr (PixelSize == sizeof(uint8_t)) {
      copy_reversed_packed<uint8_t>(src_last, dst, row_size, context);
    } else if constexpr (PixelSize == sizeof(uint16_t)) {
      copy_reversed_packed<uint16_t>(src_last, dst, row_size, context);
    } else if constexpr (PixelSize == sizeof(uint32_t)) {
      copy_reversed_packed<uint32_t>(src_last, dst, row_size, context);
    } else if constexpr (PixelSize == sizeof(uint64_t)) {
      copy_reversed_packed<uint64_t>(src_last, dst, row_size, context);
    } else if constexpr (PixelSize == 3 * sizeof(uint8_t)) {
      copy_reversed_interleaved_3<uint8_t>(src_last, dst, row_size, context);
    } else if constexpr (PixelSize == 3 * sizeof(uint16_t)) {
      copy_reversed_interleaved_3<uint16_t>(src_last, dst, row_size, context);
    } else if constexpr (PixelSize == 3 * sizeof(uint32_t)) {
      copy_reversed_interleaved_3<uint32_t>(src_last, dst, row_size, context);
    } else {
      copy_reversed_by_bytes(pixel_size, src_last, dst, row_size, context);
    }
  }

 private:
  // Reverse pixel order in an interleaved 3-channel vector block.
  template <typename Vector3Type>
  static inline Vector3Type reverse_interleaved_3_vector_lanes(
      const Vector3Type &vector_value) KLEIDICV_STREAMING {
    return svcreate3(svrev(svget3(vector_value, 0)),
                     svrev(svget3(vector_value, 1)),
                     svrev(svget3(vector_value, 2)));
  }

  template <typename PixelType>
  static inline void store_full_pair(
      const Context &full_ctx,
      const typename VecTraits<PixelType>::VectorType &value_0,
      const typename VecTraits<PixelType>::VectorType &value_1,
      PixelType *dst) KLEIDICV_STREAMING {
    VecTraits<PixelType>::store_consecutive(full_ctx, value_0, value_1, dst);
  }

  template <typename PixelType>
  static inline typename VecTraits<PixelType>::VectorType
  make_reverse_tail_indices(size_t length) KLEIDICV_STREAMING {
    if constexpr (sizeof(PixelType) == sizeof(uint8_t)) {
      return svindex_u8(static_cast<uint8_t>(length - 1),
                        static_cast<uint8_t>(-1));
    } else if constexpr (sizeof(PixelType) == sizeof(uint16_t)) {
      return svindex_u16(static_cast<uint16_t>(length - 1),
                         static_cast<uint16_t>(-1));
    } else if constexpr (sizeof(PixelType) == sizeof(uint32_t)) {
      return svindex_u32(static_cast<uint32_t>(length - 1),
                         static_cast<uint32_t>(-1));
    } else {
      return svindex_u64(static_cast<uint64_t>(length - 1),
                         static_cast<uint64_t>(-1));
    }
  }

  // Vector reverse-copy path for packed pixels.
  template <typename PixelType>
  static inline void copy_reversed_packed(
      const uint8_t *src_last, uint8_t *dst, size_t row_size,
      AddPaddingByCopyContext &context) KLEIDICV_STREAMING {
    using VectorType = typename VecTraits<PixelType>::VectorType;

    auto *typed_dst = reinterpret_cast<PixelType *>(dst);
    const auto *typed_src_last = reinterpret_cast<const PixelType *>(src_last);
    const size_t count = row_size / sizeof(PixelType);
    const size_t kStep = VecTraits<PixelType>::num_lanes();
#if KLEIDICV_TARGET_SME || KLEIDICV_TARGET_SME2
    constexpr bool kUseVectorRemaining = true;
#else
    constexpr bool kUseVectorRemaining = false;
#endif
    Context &full_ctx = context;

    auto copy_block = [&](size_t index) KLEIDICV_STREAMING {
      VectorType src_vector;
      VecTraits<PixelType>::load(
          full_ctx, typed_src_last + 1 - static_cast<ptrdiff_t>(index + kStep),
          src_vector);
      VecTraits<PixelType>::store(full_ctx, svrev(src_vector),
                                  typed_dst + index);
    };

    auto copy_block_pair = [&](size_t index) KLEIDICV_STREAMING {
      VectorType src_vector_0;
      VectorType src_vector_1;
      VecTraits<PixelType>::load_consecutive(
          full_ctx,
          typed_src_last + 1 - static_cast<ptrdiff_t>(index + (2 * kStep)),
          src_vector_0, src_vector_1);
      store_full_pair<PixelType>(full_ctx, svrev(src_vector_1),
                                 svrev(src_vector_0), typed_dst + index);
    };

    LoopUnroll2 loop{count, kStep};
    loop.unroll_twice(copy_block_pair)
        .template unroll_once_if<kUseVectorRemaining>(copy_block)
        .remaining([&](size_t index, size_t length) KLEIDICV_STREAMING {
          if constexpr (kUseVectorRemaining) {
            svbool_t tail_pg = VecTraits<PixelType>::svwhilelt(index, length);
            Context tail_ctx{tail_pg};
            VectorType src_vector;
            VecTraits<PixelType>::load(
                tail_ctx, typed_src_last + 1 - static_cast<ptrdiff_t>(length),
                src_vector);

            const size_t tail_length = length - index;
            const VectorType indices =
                make_reverse_tail_indices<PixelType>(tail_length);
            VecTraits<PixelType>::store(tail_ctx, svtbl(src_vector, indices),
                                        typed_dst + index);
          } else {
            const size_t byte_index = index * sizeof(PixelType);
            copy_reversed_by_bytes(
                sizeof(PixelType),
                src_last - static_cast<ptrdiff_t>(byte_index), dst + byte_index,
                (length - index) * sizeof(PixelType), context);
          }
        });
  }

  // Vector reverse-copy path for interleaved 3-channel pixels.
  template <typename PixelType>
  static inline void copy_reversed_interleaved_3(
      const uint8_t *src_last, uint8_t *dst, size_t row_size,
      AddPaddingByCopyContext &context) KLEIDICV_STREAMING {
    using VectorType = typename VecTraits<PixelType>::VectorType;
    using Vector3Type = typename VecTraits<PixelType>::Vector3Type;

    constexpr size_t kPixelStride = 3;
    constexpr size_t kPixelSize = kPixelStride * sizeof(PixelType);
#if KLEIDICV_TARGET_SME || KLEIDICV_TARGET_SME2
    constexpr bool kUseVectorRemaining = true;
#else
    constexpr bool kUseVectorRemaining = false;
#endif

    auto *typed_dst = reinterpret_cast<PixelType *>(dst);
    const auto *typed_src_last = reinterpret_cast<const PixelType *>(src_last);
    const size_t count = row_size / kPixelSize;
    const size_t kStep = VecTraits<PixelType>::num_lanes();
    const svbool_t full_pg = context.predicate();

    auto copy_block = [&](size_t index) KLEIDICV_STREAMING {
      const Vector3Type src_vector = svld3(
          full_pg, typed_src_last + kPixelStride -
                       static_cast<ptrdiff_t>(kPixelStride * (index + kStep)));
      svst3(full_pg, typed_dst + (kPixelStride * index),
            reverse_interleaved_3_vector_lanes(src_vector));
    };
    auto copy_block_twice = [&](size_t index) KLEIDICV_STREAMING {
      copy_block(index);
      copy_block(index + kStep);
    };

    LoopUnroll2 loop{count, kStep};
    loop.unroll_twice(copy_block_twice)
        .template unroll_once_if<kUseVectorRemaining>(copy_block)
        .remaining([&](size_t index, size_t length) KLEIDICV_STREAMING {
          if constexpr (kUseVectorRemaining) {
            svbool_t tail_pg = VecTraits<PixelType>::svwhilelt(index, length);
            const Vector3Type src_vector = svld3(
                tail_pg, typed_src_last + kPixelStride -
                             static_cast<ptrdiff_t>(kPixelStride * length));
            const size_t tail_length = length - index;
            const VectorType indices =
                make_reverse_tail_indices<PixelType>(tail_length);
            const Vector3Type reversed_src_vector =
                svcreate3(svtbl(svget3(src_vector, 0), indices),
                          svtbl(svget3(src_vector, 1), indices),
                          svtbl(svget3(src_vector, 2), indices));
            svst3(tail_pg, typed_dst + (kPixelStride * index),
                  reversed_src_vector);
          } else {
            const size_t byte_index = index * kPixelSize;
            copy_reversed_by_bytes(
                kPixelSize, src_last - static_cast<ptrdiff_t>(byte_index),
                dst + byte_index, (length - index) * kPixelSize, context);
          }
        });
  }

  static inline void copy_reversed_by_bytes(
      size_t pixel_size, const uint8_t *src_last, uint8_t *dst, size_t row_size,
      AddPaddingByCopyContext &context) KLEIDICV_STREAMING {
    for (size_t offset = 0; offset < row_size; offset += pixel_size) {
      copy_contiguous_bytes(src_last - static_cast<ptrdiff_t>(offset),
                            dst + offset, pixel_size, context);
    }
  }
};

template <size_t PixelSize>
using ConstantBorder =
    AddPaddingByCopyConstant<PixelSize, RepeatedPixelWriter<PixelSize>>;

template <size_t PixelSize>
using ReplicateBorder =
    AddPaddingByCopyReplicate<PixelSize, RepeatedPixelWriter<PixelSize>>;

template <size_t PixelSize>
using ReflectBorder =
    AddPaddingByCopyReflect<PixelSize, ReversedPixelCopier<PixelSize>>;

template <size_t PixelSize>
using ReverseBorder =
    AddPaddingByCopyReverse<PixelSize, ReversedPixelCopier<PixelSize>>;

template <size_t PixelSize>
using WrapBorder = AddPaddingByCopyWrap<PixelSize, ContiguousByteCopier>;

template <size_t PixelSize>
using IndexedBorder = AddPaddingByCopyIndexed<PixelSize, ContiguousByteCopier>;

static AddPaddingByCopyOpPointer create_add_padding_by_copy_operation_sc(
    AddPaddingByCopyParameters parameters,
    AddPaddingByCopyBorderStrategy strategy) KLEIDICV_STREAMING {
  switch (strategy) {
    case AddPaddingByCopyBorderStrategy::kConstant:
      return allocate_pixel_size_operation<ConstantBorder>(parameters);
    case AddPaddingByCopyBorderStrategy::kReplicate:
      return allocate_pixel_size_operation<ReplicateBorder>(parameters);
    case AddPaddingByCopyBorderStrategy::kWrapFast:
      return allocate_pixel_size_operation<WrapBorder>(parameters);
    case AddPaddingByCopyBorderStrategy::kReflectFast:
      return allocate_pixel_size_operation<ReflectBorder>(parameters);
    case AddPaddingByCopyBorderStrategy::kReverseFast:
      return allocate_pixel_size_operation<ReverseBorder>(parameters);
    default:
      return allocate_pixel_size_operation<IndexedBorder>(parameters);
  }
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_SRC_TRANSFORM_ADD_PADDING_BY_COPY_SC_H
