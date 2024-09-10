// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <type_traits>

#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"
#include "kleidicv/remap/remap.h"

namespace kleidicv::neon {

template <typename ScalarType>
class RemapS16;

template <>
class RemapS16<uint8_t> {
 public:
  using ScalarType = uint8_t;
  using MapVecTraits = neon::VecTraits<int16_t>;
  using MapVectorType = typename MapVecTraits::VectorType;
  using MapVector2Type = typename MapVecTraits::Vector2Type;
  using VecTraits = neon::VecTraits<ScalarType>;
  using VectorType = typename VecTraits::VectorType;

  explicit RemapS16(Rows<const ScalarType> src_rows, size_t src_width,
                    size_t src_height)
      : src_rows_{src_rows},
        v_src_stride_{vdupq_n_s16(static_cast<int16_t>(src_rows_.stride()))},
        v_xmax_{vdupq_n_s16(static_cast<int16_t>(src_width - 1))},
        v_ymax_{vdupq_n_s16(static_cast<int16_t>(src_height - 1))} {}

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<ScalarType> dst) {
    auto vector_path = [&](size_t step) {
      MapVector2Type xy = vld2q_s16(&mapxy[0]);
      // Clamp coordinates to within the dimensions of the source image
      uint16x8_t x = vreinterpretq_u16_s16(
          vmaxq_s16(vdupq_n_s16(0), vminq_s16(xy.val[0], v_xmax_)));
      uint16x8_t y = vreinterpretq_u16_s16(
          vmaxq_s16(vdupq_n_s16(0), vminq_s16(xy.val[1], v_ymax_)));
      // Calculate offsets from coordinates (y * stride + x)
      uint32x4_t indices_low =
          vmlal_u16(vmovl_u16(vget_low_u16(x)), vget_low_u16(y),
                    vget_low_u16(v_src_stride_));
      // Copy pixels from source
      dst[0] = src_rows_[vgetq_lane_u32(indices_low, 0)];
      dst[1] = src_rows_[vgetq_lane_u32(indices_low, 1)];
      dst[2] = src_rows_[vgetq_lane_u32(indices_low, 2)];
      dst[3] = src_rows_[vgetq_lane_u32(indices_low, 3)];
      uint32x4_t indices_high =
          vmlal_high_u16(vmovl_high_u16(x), y, v_src_stride_);
      dst[4] = src_rows_[vgetq_lane_u32(indices_high, 0)];
      dst[5] = src_rows_[vgetq_lane_u32(indices_high, 1)];
      dst[6] = src_rows_[vgetq_lane_u32(indices_high, 2)];
      dst[7] = src_rows_[vgetq_lane_u32(indices_high, 3)];
      mapxy += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };

    LoopUnroll loop{width, MapVecTraits::num_lanes()};
    loop.unroll_once(vector_path);
    ptrdiff_t back_step = static_cast<ptrdiff_t>(loop.step()) -
                          static_cast<ptrdiff_t>(loop.remaining_length());
    mapxy -= back_step;
    dst -= back_step;
    loop.remaining([&](size_t, size_t step) { vector_path(step); });
  }

 private:
  Rows<const ScalarType> src_rows_;
  int16x8_t v_src_stride_;
  int16x8_t v_xmax_;
  int16x8_t v_ymax_;
};  // end of class RemapS16<uint8_t>

template <typename T>
kleidicv_error_t remap_s16(
    const T *src, size_t src_stride, size_t src_width, size_t src_height,
    T *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t *mapxy, size_t mapxy_stride,
    kleidicv_border_type_t border_type,
    [[maybe_unused]] kleidicv_border_values_t border_values) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_POINTER_AND_STRIDE(mapxy, mapxy_stride, dst_height);
  CHECK_IMAGE_SIZE(src_width, src_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);

  if (!remap_s16_is_implemented<T>(dst_width, border_type, channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  Rows<const T> src_rows{src, src_stride, channels};
  Rows<const int16_t> mapxy_rows{mapxy, mapxy_stride, 2};
  Rows<T> dst_rows{dst, dst_stride, channels};
  RemapS16<T> operation{src_rows, src_width, src_height};
  Rectangle rect{dst_width, dst_height};
  zip_rows(operation, rect, mapxy_rows, dst_rows);
  return KLEIDICV_OK;
}

#define KLEIDICV_INSTANTIATE_TEMPLATE(type)                                    \
  template KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t remap_s16<type>(          \
      const type *src, size_t src_stride, size_t src_width, size_t src_height, \
      type *dst, size_t dst_stride, size_t dst_width, size_t dst_height,       \
      size_t channels, const int16_t *mapxy, size_t mapxy_stride,              \
      kleidicv_border_type_t border_type,                                      \
      kleidicv_border_values_t border_values)

KLEIDICV_INSTANTIATE_TEMPLATE(uint8_t);

}  // namespace kleidicv::neon
