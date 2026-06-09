// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "add_padding_by_copy_common.h"
#include "kleidicv/neon.h"
#include "kleidicv/transform/add_padding_by_copy.h"

namespace kleidicv::neon {

class ContiguousByteCopier {
 public:
  static inline void copy_contiguous_bytes(const uint8_t *src, uint8_t *dst,
                                           size_t size) {
    std::memcpy(dst, src, size);
  }
};

template <size_t PixelSize>
class RepeatedPixelWriter : public ContiguousByteCopier {
 public:
  // Use the stripe-selected repeated-value writer for this pixel layout.
  static inline void fill_repeated_pixel(size_t pixel_size,
                                         const uint8_t *value_bytes,
                                         uint8_t *dst, size_t row_size) {
    if constexpr (PixelSize == sizeof(uint8_t)) {
      fill_single_channel<uint8_t>(value_bytes, dst, row_size);
    } else if constexpr (PixelSize == sizeof(uint16_t)) {
      fill_single_channel<uint16_t>(value_bytes, dst, row_size);
    } else if constexpr (PixelSize == sizeof(uint32_t)) {
      fill_single_channel<uint32_t>(value_bytes, dst, row_size);
    } else if constexpr (PixelSize == sizeof(uint64_t)) {
      fill_single_channel<uint64_t>(value_bytes, dst, row_size);
    } else if constexpr (PixelSize == 3 * sizeof(uint8_t)) {
      fill_interleaved_3<uint8_t>(value_bytes, dst, row_size);
    } else if constexpr (PixelSize == 3 * sizeof(uint16_t)) {
      fill_interleaved_3<uint16_t>(value_bytes, dst, row_size);
    } else if constexpr (PixelSize == 3 * sizeof(uint32_t)) {
      fill_interleaved_3<uint32_t>(value_bytes, dst, row_size);
    } else {
      fill_repeated_by_bytes(pixel_size, value_bytes, dst, row_size);
    }
  }

 private:
  // Vector path for packed single-channel pixel layouts.
  template <typename ScalarType>
  static inline void fill_single_channel(const uint8_t *value_bytes,
                                         uint8_t *dst, size_t row_size) {
    using VectorType = typename VecTraits<ScalarType>::VectorType;
    using Vector2Type = typename VecTraits<ScalarType>::Vector2Type;
    using Vector4Type = typename VecTraits<ScalarType>::Vector4Type;

    const ScalarType value = load_unaligned<ScalarType>(value_bytes);
    const VectorType repeated_value = vdupq_n(value);
    Vector4Type repeated_value_quad;
    Vector2Type repeated_value_pair;
    repeated_value_quad.val[0] = repeated_value;
    repeated_value_quad.val[1] = repeated_value;
    repeated_value_quad.val[2] = repeated_value;
    repeated_value_quad.val[3] = repeated_value;
    repeated_value_pair.val[0] = repeated_value;
    repeated_value_pair.val[1] = repeated_value;

    auto store_repeated_quad = [&](size_t index) {
      VecTraits<ScalarType>::store(repeated_value_quad,
                                   reinterpret_cast<ScalarType *>(dst + index));
    };
    auto store_repeated_pair = [&](size_t index) {
      VecTraits<ScalarType>::store(repeated_value_pair,
                                   reinterpret_cast<ScalarType *>(dst + index));
    };
    auto store_repeated_vector = [&](size_t index) {
      VecTraits<ScalarType>::store(repeated_value,
                                   reinterpret_cast<ScalarType *>(dst + index));
    };

    LoopUnroll2 loop{row_size, kVectorLength};
    loop.unroll_four_times(store_repeated_quad);
    loop.unroll_twice(store_repeated_pair);
    loop.unroll_once(store_repeated_vector);
    loop.remaining([&](size_t index, size_t length) {
      fill_repeated_by_bytes(sizeof(ScalarType), value_bytes, dst + index,
                             length - index);
    });
  }

  // Vector path for interleaved 3-channel pixel layouts.
  template <typename ScalarType>
  static inline void fill_interleaved_3(const uint8_t *value_bytes,
                                        uint8_t *dst, size_t row_size) {
    using Vector3Type = typename VecTraits<ScalarType>::Vector3Type;

    Vector3Type vector_value;
    vector_value.val[0] = vdupq_n(load_unaligned<ScalarType>(value_bytes));
    vector_value.val[1] =
        vdupq_n(load_unaligned<ScalarType>(value_bytes + sizeof(ScalarType)));
    vector_value.val[2] = vdupq_n(
        load_unaligned<ScalarType>(value_bytes + (2 * sizeof(ScalarType))));

    constexpr size_t kVectorStoreSize = 3 * kVectorLength;

    auto store = [&](size_t index) {
      vst3q(reinterpret_cast<ScalarType *>(dst + index), vector_value);
    };
    auto store_twice = [&](size_t index) {
      store(index);
      store(index + kVectorStoreSize);
    };

    LoopUnroll2 loop{row_size, kVectorStoreSize};
    loop.unroll_twice(store_twice);
    loop.unroll_once(store);
    loop.remaining([&](size_t index, size_t length) {
      fill_repeated_by_bytes(3 * sizeof(ScalarType), value_bytes, dst + index,
                             length - index);
    });
  }

  static inline void fill_repeated_by_bytes(size_t pixel_size,
                                            const uint8_t *value_bytes,
                                            uint8_t *dst, size_t row_size) {
    for (size_t offset = 0; offset < row_size; offset += pixel_size) {
      copy_contiguous_bytes(value_bytes, dst + offset, pixel_size);
    }
  }

  // Load a scalar from byte-addressed storage without assuming alignment.
  template <typename ScalarType>
  static inline ScalarType load_unaligned(const uint8_t *src) {
    ScalarType value;
    std::memcpy(&value, src, sizeof(ScalarType));
    return value;
  }
};

template <size_t PixelSize>
class ReversedPixelCopier : public ContiguousByteCopier {
 public:
  // Copy row_size bytes using the stripe-selected reverse-copy strategy.
  static inline void copy_reversed_pixels(size_t pixel_size,
                                          const uint8_t *src_last, uint8_t *dst,
                                          size_t row_size) {
    if constexpr (PixelSize == sizeof(uint8_t)) {
      copy_reversed_packed<uint8_t>(src_last, dst, row_size);
    } else if constexpr (PixelSize == sizeof(uint16_t)) {
      copy_reversed_packed<uint16_t>(src_last, dst, row_size);
    } else if constexpr (PixelSize == sizeof(uint32_t)) {
      copy_reversed_packed<uint32_t>(src_last, dst, row_size);
    } else if constexpr (PixelSize == sizeof(uint64_t)) {
      copy_reversed_packed<uint64_t>(src_last, dst, row_size);
    } else if constexpr (PixelSize == 3 * sizeof(uint8_t)) {
      copy_reversed_interleaved_3<uint8_t>(src_last, dst, row_size);
    } else if constexpr (PixelSize == 3 * sizeof(uint16_t)) {
      copy_reversed_interleaved_3<uint16_t>(src_last, dst, row_size);
    } else if constexpr (PixelSize == 3 * sizeof(uint32_t)) {
      copy_reversed_interleaved_3<uint32_t>(src_last, dst, row_size);
    } else {
      copy_reversed_by_bytes(pixel_size, src_last, dst, row_size);
    }
  }

 private:
  // Reverse lane order inside one packed vector.
  template <typename VectorType>
  static inline VectorType reverse_packed_vector_lanes(
      VectorType vector_value) {
    const auto reversed_vector_64 = vrev64q(vector_value);
    return vcombine(vget_high(reversed_vector_64),
                    vget_low(reversed_vector_64));
  }

  // Reverse two consecutive vectors as one larger reversed byte range.
  template <typename Vector2Type>
  static inline Vector2Type reverse_packed_vector2_order_and_lanes(
      Vector2Type vector_value) {
    Vector2Type reversed_vector{};
    reversed_vector.val[0] = reverse_packed_vector_lanes(vector_value.val[1]);
    reversed_vector.val[1] = reverse_packed_vector_lanes(vector_value.val[0]);
    return reversed_vector;
  }

  // Reverse four consecutive vectors as one larger reversed byte range.
  template <typename Vector4Type>
  static inline Vector4Type reverse_packed_vector4_order_and_lanes(
      Vector4Type vector_value) {
    Vector4Type reversed_vector{};
    reversed_vector.val[0] = reverse_packed_vector_lanes(vector_value.val[3]);
    reversed_vector.val[1] = reverse_packed_vector_lanes(vector_value.val[2]);
    reversed_vector.val[2] = reverse_packed_vector_lanes(vector_value.val[1]);
    reversed_vector.val[3] = reverse_packed_vector_lanes(vector_value.val[0]);
    return reversed_vector;
  }

  // Reverse pixel order in an interleaved 3-channel vector block.
  template <typename Vector3Type>
  static inline Vector3Type reverse_interleaved_3_vector_lanes(
      Vector3Type vector_value) {
    vector_value.val[0] = reverse_packed_vector_lanes(vector_value.val[0]);
    vector_value.val[1] = reverse_packed_vector_lanes(vector_value.val[1]);
    vector_value.val[2] = reverse_packed_vector_lanes(vector_value.val[2]);
    return vector_value;
  }

  // Vector reverse-copy path for packed pixels.
  template <typename PixelType>
  static inline void copy_reversed_packed(const uint8_t *src_last, uint8_t *dst,
                                          size_t row_size) {
    using VectorType = typename VecTraits<PixelType>::VectorType;
    using Vector2Type = typename VecTraits<PixelType>::Vector2Type;
    using Vector4Type = typename VecTraits<PixelType>::Vector4Type;
    constexpr size_t kStep = kVectorLength;

    auto copy_block = [&](size_t index) {
      VectorType src_vector;
      VecTraits<PixelType>::load(
          reinterpret_cast<const PixelType *>(src_last + sizeof(PixelType) -
                                              index - kStep),
          src_vector);
      VecTraits<PixelType>::store(reverse_packed_vector_lanes(src_vector),
                                  reinterpret_cast<PixelType *>(dst + index));
    };
    auto copy_block_pair = [&](size_t index) {
      Vector2Type src_vector;
      VecTraits<PixelType>::load(
          reinterpret_cast<const PixelType *>(src_last + sizeof(PixelType) -
                                              index - (2 * kStep)),
          src_vector);
      VecTraits<PixelType>::store(
          reverse_packed_vector2_order_and_lanes(src_vector),
          reinterpret_cast<PixelType *>(dst + index));
    };
    auto copy_block_quad = [&](size_t index) {
      Vector4Type src_vector;
      VecTraits<PixelType>::load(
          reinterpret_cast<const PixelType *>(src_last + sizeof(PixelType) -
                                              index - (4 * kStep)),
          src_vector);
      VecTraits<PixelType>::store(
          reverse_packed_vector4_order_and_lanes(src_vector),
          reinterpret_cast<PixelType *>(dst + index));
    };

    LoopUnroll2 loop{row_size, kStep};
    loop.unroll_four_times(copy_block_quad);
    loop.unroll_twice(copy_block_pair);
    loop.unroll_once(copy_block);
    loop.remaining([&](size_t index, size_t length) {
      copy_reversed_by_bytes(sizeof(PixelType),
                             src_last - static_cast<ptrdiff_t>(index),
                             dst + index, length - index);
    });
  }

  // Vector reverse-copy path for interleaved 3-channel pixels.
  template <typename PixelType>
  static inline void copy_reversed_interleaved_3(const uint8_t *src_last,
                                                 uint8_t *dst,
                                                 size_t row_size) {
    using Vector3Type = typename VecTraits<PixelType>::Vector3Type;

    constexpr size_t kPixelStride = 3;
    constexpr size_t kPixelSize = kPixelStride * sizeof(PixelType);
    constexpr size_t kVectorStoreSize = 3 * kVectorLength;

    auto copy_block = [&](size_t index) {
      const Vector3Type src_vector = vld3q(reinterpret_cast<const PixelType *>(
          src_last + kPixelSize - index - kVectorStoreSize));
      vst3q(reinterpret_cast<PixelType *>(dst + index),
            reverse_interleaved_3_vector_lanes(src_vector));
    };
    auto copy_block_twice = [&](size_t index) {
      copy_block(index);
      copy_block(index + kVectorStoreSize);
    };

    LoopUnroll2 loop{row_size, kVectorStoreSize};
    loop.unroll_twice(copy_block_twice);
    loop.unroll_once(copy_block);
    loop.remaining([&](size_t index, size_t length) {
      copy_reversed_by_bytes(kPixelSize,
                             src_last - static_cast<ptrdiff_t>(index),
                             dst + index, length - index);
    });
  }

  static inline void copy_reversed_by_bytes(size_t pixel_size,
                                            const uint8_t *src_last,
                                            uint8_t *dst, size_t row_size) {
    for (size_t offset = 0; offset < row_size; offset += pixel_size) {
      copy_contiguous_bytes(src_last - static_cast<ptrdiff_t>(offset),
                            dst + offset, pixel_size);
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

KLEIDICV_TARGET_FN_ATTRS
AddPaddingByCopyOpPointer create_add_padding_by_copy_operation(
    AddPaddingByCopyParameters parameters,
    AddPaddingByCopyBorderStrategy strategy) {
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

}  // namespace kleidicv::neon
