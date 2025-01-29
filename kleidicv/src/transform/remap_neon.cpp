// SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <cstdint>  // TODO: check
#include <type_traits>

#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"
#include "kleidicv/transform/remap.h"

namespace kleidicv::neon {

template <typename ScalarType>
class RemapS16Replicate {
 public:
  using MapVecTraits = neon::VecTraits<int16_t>;
  using MapVectorType = typename MapVecTraits::VectorType;
  using MapVector2Type = typename MapVecTraits::Vector2Type;

  RemapS16Replicate(Rows<const ScalarType> src_rows, size_t src_width,
                    size_t src_height)
      : src_rows_{src_rows},
        v_src_element_stride_{vdupq_n_u16(
            static_cast<uint16_t>(src_rows_.stride() / sizeof(ScalarType)))},
        v_xmax_{vdupq_n_s16(static_cast<int16_t>(src_width - 1))},
        v_ymax_{vdupq_n_s16(static_cast<int16_t>(src_height - 1))} {}

  void transform_pixels(uint32x4_t indices_low, uint32x4_t indices_high,
                        Columns<ScalarType> dst) {
    // Copy pixels from source
    dst[0] = src_rows_[vgetq_lane_u32(indices_low, 0)];
    dst[1] = src_rows_[vgetq_lane_u32(indices_low, 1)];
    dst[2] = src_rows_[vgetq_lane_u32(indices_low, 2)];
    dst[3] = src_rows_[vgetq_lane_u32(indices_low, 3)];

    dst[4] = src_rows_[vgetq_lane_u32(indices_high, 0)];
    dst[5] = src_rows_[vgetq_lane_u32(indices_high, 1)];
    dst[6] = src_rows_[vgetq_lane_u32(indices_high, 2)];
    dst[7] = src_rows_[vgetq_lane_u32(indices_high, 3)];
  }

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<ScalarType> dst) {
    auto vector_path = [&](size_t step) {
      MapVector2Type xy = vld2q_s16(&mapxy[0]);
      // Clamp coordinates to within the dimensions of the source image
      uint16x8_t x = vreinterpretq_u16_s16(
          vmaxq_s16(vdupq_n_s16(0), vminq_s16(xy.val[0], v_xmax_)));
      uint16x8_t y = vreinterpretq_u16_s16(
          vmaxq_s16(vdupq_n_s16(0), vminq_s16(xy.val[1], v_ymax_)));
      // Calculate offsets from coordinates (y * stride/sizeof(ScalarType) + x)
      uint32x4_t indices_low =
          vmlal_u16(vmovl_u16(vget_low_u16(x)), vget_low_u16(y),
                    vget_low_u16(v_src_element_stride_));
      uint32x4_t indices_high =
          vmlal_high_u16(vmovl_high_u16(x), y, v_src_element_stride_);

      transform_pixels(indices_low, indices_high, dst);

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
  uint16x8_t v_src_element_stride_;
  int16x8_t v_xmax_;
  int16x8_t v_ymax_;
};  // end of class RemapS16Replicate<ScalarType>

template <typename ScalarType>
class RemapS16ConstantBorder {
 public:
  using SrcVecTraits = neon::VecTraits<ScalarType>;
  using SrcVecType = typename SrcVecTraits::VectorType;

  using MapVecTraits = neon::VecTraits<int16_t>;
  using MapVectorType = typename MapVecTraits::VectorType;
  using MapVector2Type = typename MapVecTraits::Vector2Type;

  RemapS16ConstantBorder(Rows<const ScalarType> src_rows, size_t src_width,
                         size_t src_height, const ScalarType *border_value)
      : src_rows_{src_rows},
        v_src_element_stride_{vdupq_n_u16(
            static_cast<uint16_t>(src_rows_.stride() / sizeof(ScalarType)))},
        v_width_{vdupq_n_u16(static_cast<uint16_t>(src_width))},
        v_height_{vdupq_n_u16(static_cast<uint16_t>(src_height))},
        v_border_{vdupq_n_u16(*border_value)} {}

  void transform_pixels(uint32x4_t indices_low, uint32x4_t indices_high,
                        uint16x8_t in_range, Columns<ScalarType> dst);

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<ScalarType> dst) {
    auto vector_path = [&](size_t step) {
      MapVector2Type xy = vld2q_s16(&mapxy[0]);

      uint16x8_t x = vreinterpretq_u16_s16(xy.val[0]);
      uint16x8_t y = vreinterpretq_u16_s16(xy.val[1]);

      // Find whether coordinates are within the image dimensions.
      // Negative coordinates are interpreted as large values due to the
      // s16->u16 reinterpretation.
      uint16x8_t in_range =
          vandq_u16(vcltq_u16(x, v_width_), vcltq_u16(y, v_height_));

      // Zero out-of-range coordinates.
      x = vandq_u16(in_range, x);
      y = vandq_u16(in_range, y);

      // Calculate offsets from coordinates (y * stride/sizeof(ScalarType) + x)
      uint32x4_t indices_low =
          vmlal_u16(vmovl_u16(vget_low_u16(x)), vget_low_u16(y),
                    vget_low_u16(v_src_element_stride_));
      uint32x4_t indices_high =
          vmlal_high_u16(vmovl_high_u16(x), y, v_src_element_stride_);

      transform_pixels(indices_low, indices_high, in_range, dst);

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
  uint16x8_t v_src_element_stride_;
  uint16x8_t v_width_;
  uint16x8_t v_height_;
  uint16x8_t v_border_;
};  // end of class RemapS16ConstantBorder<ScalarType>

template <>
void RemapS16ConstantBorder<uint8_t>::transform_pixels(uint32x4_t indices_low,
                                                       uint32x4_t indices_high,
                                                       uint16x8_t in_range,
                                                       Columns<uint8_t> dst) {
  uint8x8_t pixels = {
      src_rows_[vgetq_lane_u32(indices_low, 0)],
      src_rows_[vgetq_lane_u32(indices_low, 1)],
      src_rows_[vgetq_lane_u32(indices_low, 2)],
      src_rows_[vgetq_lane_u32(indices_low, 3)],
      src_rows_[vgetq_lane_u32(indices_high, 0)],
      src_rows_[vgetq_lane_u32(indices_high, 1)],
      src_rows_[vgetq_lane_u32(indices_high, 2)],
      src_rows_[vgetq_lane_u32(indices_high, 3)],
  };
  // Select between source pixels and border colour
  uint8x8_t pixels_or_border =
      vbsl_u8(vmovn_u16(in_range), pixels, vmovn_u16(v_border_));

  vst1_u8(&dst[0], pixels_or_border);
}

template <>
void RemapS16ConstantBorder<uint16_t>::transform_pixels(uint32x4_t indices_low,
                                                        uint32x4_t indices_high,
                                                        uint16x8_t in_range,
                                                        Columns<uint16_t> dst) {
  uint16x8_t pixels = {
      src_rows_[vgetq_lane_u32(indices_low, 0)],
      src_rows_[vgetq_lane_u32(indices_low, 1)],
      src_rows_[vgetq_lane_u32(indices_low, 2)],
      src_rows_[vgetq_lane_u32(indices_low, 3)],
      src_rows_[vgetq_lane_u32(indices_high, 0)],
      src_rows_[vgetq_lane_u32(indices_high, 1)],
      src_rows_[vgetq_lane_u32(indices_high, 2)],
      src_rows_[vgetq_lane_u32(indices_high, 3)],
  };

  // Select between source pixels and border colour
  uint16x8_t pixels_or_border = vbslq_u16(in_range, pixels, v_border_);

  vst1q_u16(&dst[0], pixels_or_border);
}

// Most of the complexity comes from parameter checking.
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <typename T>
kleidicv_error_t remap_s16(const T *src, size_t src_stride, size_t src_width,
                           size_t src_height, T *dst, size_t dst_stride,
                           size_t dst_width, size_t dst_height, size_t channels,
                           const int16_t *mapxy, size_t mapxy_stride,
                           kleidicv_border_type_t border_type,
                           [[maybe_unused]] const T *border_value) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_POINTER_AND_STRIDE(mapxy, mapxy_stride, dst_height);
  CHECK_IMAGE_SIZE(src_width, src_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);
  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT && nullptr == border_value) {
    return KLEIDICV_ERROR_NULL_POINTER;
  }

  if (!remap_s16_is_implemented<T>(src_stride, src_width, src_height, dst_width,
                                   border_type, channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  Rows<const T> src_rows{src, src_stride, channels};
  Rows<const int16_t> mapxy_rows{mapxy, mapxy_stride, 2};
  Rows<T> dst_rows{dst, dst_stride, channels};
  Rectangle rect{dst_width, dst_height};
  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT) {
    RemapS16ConstantBorder<T> operation{src_rows, src_width, src_height,
                                        border_value};
    zip_rows(operation, rect, mapxy_rows, dst_rows);
  } else {
    assert(border_type == KLEIDICV_BORDER_TYPE_REPLICATE);
    RemapS16Replicate<T> operation{src_rows, src_width, src_height};
    zip_rows(operation, rect, mapxy_rows, dst_rows);
  }
  return KLEIDICV_OK;
}
// NOLINTEND(readability-function-cognitive-complexity)

template <typename ScalarType>
class RemapS16Point5Replicate;

template <>
class RemapS16Point5Replicate<uint8_t> {
 public:
  using ScalarType = uint8_t;
  using MapVecTraits = neon::VecTraits<int16_t>;
  using MapVectorType = typename MapVecTraits::VectorType;
  using MapVector2Type = typename MapVecTraits::Vector2Type;
  using FracVecTraits = neon::VecTraits<uint16_t>;
  using FracVectorType = typename FracVecTraits::VectorType;

  RemapS16Point5Replicate(Rows<const ScalarType> src_rows, size_t src_width,
                          size_t src_height)
      : src_rows_{src_rows},
        v_src_stride_{vdup_n_u16(static_cast<uint16_t>(src_rows_.stride()))},
        v_xmax_{vdupq_n_s16(static_cast<int16_t>(src_width - 1))},
        v_ymax_{vdupq_n_s16(static_cast<int16_t>(src_height - 1))} {}

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<const uint16_t> mapfrac, Columns<ScalarType> dst) {
    auto vector_path = [&](size_t step) {
      MapVector2Type xy = vld2q_s16(&mapxy[0]);
      FracVectorType frac = vld1q_u16(&mapfrac[0]);
      uint16x8_t xfrac =
          vbslq_u16(vcltq_s16(xy.val[0], vdupq_n_s16(0)), vdupq_n_u16(0),
                    // extract xfrac = frac[0:4]
                    vandq_u16(frac, vdupq_n_u16(REMAP16POINT5_FRAC_MAX - 1)));
      uint16x8_t yfrac =
          vbslq_u16(vcltq_s16(xy.val[1], vdupq_n_s16(0)), vdupq_n_u16(0),
                    // extract yfrac = frac[5:9]
                    vandq_u16(vshrq_n_u16(frac, REMAP16POINT5_FRAC_BITS),
                              vdupq_n_u16(REMAP16POINT5_FRAC_MAX - 1)));
      uint16x8_t nxfrac = vsubq_u16(vdupq_n_u16(REMAP16POINT5_FRAC_MAX), xfrac);
      uint16x8_t nyfrac = vsubq_u16(vdupq_n_u16(REMAP16POINT5_FRAC_MAX), yfrac);

      // Clamp coordinates to within the dimensions of the source image
      uint16x8_t x0 = vreinterpretq_u16_s16(
          vmaxq_s16(vdupq_n_s16(0), vminq_s16(xy.val[0], v_xmax_)));
      uint16x8_t y0 = vreinterpretq_u16_s16(
          vmaxq_s16(vdupq_n_s16(0), vminq_s16(xy.val[1], v_ymax_)));

      // x1 = x0 + 1, except if it's already xmax
      uint16x8_t x1 = vsubq_u16(x0, vcltq_s16(xy.val[0], v_xmax_));
      uint16x8_t y1 = vsubq_u16(y0, vcltq_s16(xy.val[1], v_ymax_));

      uint16x4_t dst_low = load_and_interpolate(
          vmovl_u16(vget_low_u16(x0)), vget_low_u16(y0),
          vmovl_u16(vget_low_u16(x1)), vget_low_u16(y1), vget_low_u16(xfrac),
          vget_low_u16(yfrac), vget_low_u16(nxfrac), vget_low_u16(nyfrac));

      uint16x4_t dst_high = load_and_interpolate(
          vmovl_high_u16(x0), vget_high_u16(y0), vmovl_high_u16(x1),
          vget_high_u16(y1), vget_high_u16(xfrac), vget_high_u16(yfrac),
          vget_high_u16(nxfrac), vget_high_u16(nyfrac));

      vst1_u8(&dst[0], vuzp1_u8(dst_low, dst_high));
      mapxy += ptrdiff_t(step);
      mapfrac += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };
    LoopUnroll loop{width, MapVecTraits::num_lanes()};
    loop.unroll_once(vector_path);
    ptrdiff_t back_step = static_cast<ptrdiff_t>(loop.step()) -
                          static_cast<ptrdiff_t>(loop.remaining_length());
    mapxy -= back_step;
    mapfrac -= back_step;
    dst -= back_step;
    loop.remaining([&](size_t, size_t step) { vector_path(step); });
  }

 private:
  uint16x4_t load_and_interpolate(uint32x4_t x0, uint16x4_t y0, uint32x4_t x1,
                                  uint16x4_t y1, uint16x4_t xfrac,
                                  uint16x4_t yfrac, uint16x4_t nxfrac,
                                  uint16x4_t nyfrac) {
    // Calculate offsets from coordinates (y * stride + x)
    // a: top left, b: top right, c: bottom left, d: bottom right
    uint32x4_t offset = vmlal_u16(x0, y0, v_src_stride_);
    uint64_t acc =
        static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 0)]) |
        (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 1)]) << 16) |
        (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 2)]) << 32) |
        (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 3)]) << 48);
    uint16x4_t a = vreinterpret_u16_u64(vset_lane_u64(acc, vdup_n_u64(0), 0));

    offset = vmlal_u16(x1, y0, v_src_stride_);

    acc = static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 0)]) |
          (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 1)]) << 16) |
          (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 2)]) << 32) |
          (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 3)]) << 48);
    uint16x4_t b = vreinterpret_u16_u64(vset_lane_u64(acc, vdup_n_u64(0), 0));

    uint16x4_t line0 = vmla_u16(vmul_u16(xfrac, b), nxfrac, a);

    offset = vmlal_u16(x0, y1, v_src_stride_);

    acc = static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 0)]) |
          (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 1)]) << 16) |
          (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 2)]) << 32) |
          (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 3)]) << 48);
    uint16x4_t c = vreinterpret_u16_u64(vset_lane_u64(acc, vdup_n_u64(0), 0));

    uint32x4_t line0_lerpd = vmlal_u16(
        vdupq_n_u32(REMAP16POINT5_FRAC_MAX_SQUARE / 2), line0, nyfrac);

    offset = vmlal_u16(x1, y1, v_src_stride_);

    acc = static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 0)]) |
          (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 1)]) << 16) |
          (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 2)]) << 32) |
          (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 3)]) << 48);
    uint16x4_t d = vreinterpret_u16_u64(vset_lane_u64(acc, vdup_n_u64(0), 0));

    uint16x4_t line1 = vmla_u16(vmul_u16(xfrac, d), nxfrac, c);
    return vshrn_n_u32(vmlal_u16(line0_lerpd, line1, yfrac),
                       2 * REMAP16POINT5_FRAC_BITS);
  }

  Rows<const ScalarType> src_rows_;
  uint16x4_t v_src_stride_;
  int16x8_t v_xmax_;
  int16x8_t v_ymax_;
};  // end of class RemapS16Point5Replicate<uint8_t>

template <>
class RemapS16Point5Replicate<uint16_t> {
 public:
  using ScalarType = uint16_t;
  using MapVecTraits = neon::VecTraits<int16_t>;

  RemapS16Point5Replicate(Rows<const ScalarType> src_rows, size_t src_width,
                          size_t src_height)
      : src_rows_{src_rows},
        v_src_element_stride_{vdupq_n_u16(
            static_cast<uint16_t>(src_rows_.stride() / sizeof(ScalarType)))},
        v_xmax_{vdupq_n_s16(static_cast<int16_t>(src_width - 1))},
        v_ymax_{vdupq_n_s16(static_cast<int16_t>(src_height - 1))},
        xfrac_{vdupq_n_u16(0)},
        yfrac_{vdupq_n_u16(0)},
        nxfrac_{vdupq_n_u16(0)},
        nyfrac_{vdupq_n_u16(0)},
        x0_{vdupq_n_s16(0)},
        x1_{vdupq_n_s16(0)},
        y0_{vdupq_n_s16(0)},
        y1_{vdupq_n_s16(0)} {}

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<const uint16_t> mapfrac, Columns<ScalarType> dst) {
    auto vector_path = [&](size_t step) {
      prepare_maps(mapxy, mapfrac);
      transform_pixels(dst);

      mapxy += ptrdiff_t(step);
      mapfrac += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };
    LoopUnroll loop{width, MapVecTraits::num_lanes()};
    loop.unroll_once(vector_path);
    ptrdiff_t back_step = static_cast<ptrdiff_t>(loop.step()) -
                          static_cast<ptrdiff_t>(loop.remaining_length());
    mapxy -= back_step;
    mapfrac -= back_step;
    dst -= back_step;
    loop.remaining([&](size_t, size_t step) { vector_path(step); });
  }

  void prepare_maps(Columns<const int16_t> mapxy,
                    Columns<const uint16_t> mapfrac) {
    int16x8x2_t xy = vld2q_s16(&mapxy[0]);
    uint16x8_t frac = vld1q_u16(&mapfrac[0]);
    uint16x8_t frac_max = vdupq_n_u16(REMAP16POINT5_FRAC_MAX);
    uint16x8_t frac_mask = vdupq_n_u16(REMAP16POINT5_FRAC_MAX - 1);
    xfrac_ = vbslq_u16(vcltq_s16(xy.val[0], vdupq_n_s16(0)), vdupq_n_u16(0),
                       vandq_u16(frac, frac_mask));
    yfrac_ = vbslq_u16(
        vcltq_s16(xy.val[1], vdupq_n_s16(0)), vdupq_n_u16(0),
        vandq_u16(vshrq_n_u16(frac, REMAP16POINT5_FRAC_BITS), frac_mask));
    nxfrac_ = vsubq_u16(frac_max, xfrac_);
    nyfrac_ = vsubq_u16(frac_max, yfrac_);

    // Clamp coordinates to within the dimensions of the source image
    x0_ = vreinterpretq_u16_s16(
        vmaxq_s16(vdupq_n_s16(0), vminq_s16(xy.val[0], v_xmax_)));
    y0_ = vreinterpretq_u16_s16(
        vmaxq_s16(vdupq_n_s16(0), vminq_s16(xy.val[1], v_ymax_)));

    // x1 = x0 + 1, except if it's already xmax
    x1_ = vsubq_u16(x0_, vcltq_s16(xy.val[0], v_xmax_));
    y1_ = vsubq_u16(y0_, vcltq_s16(xy.val[1], v_ymax_));
  }

  void transform_pixels(Columns<uint16_t> dst) {
    uint16x8_t a = load_pixels(x0_, y0_);
    uint16x8_t b = load_pixels(x0_, y1_);
    uint16x8_t c = load_pixels(x1_, y0_);
    uint16x8_t d = load_pixels(x1_, y1_);

    uint16x8_t result = interpolate(a, b, c, d);

    vst1q_u16(&dst[0], result);
  }

  uint16x8_t load_pixels(int16x8_t x, int16x8_t y) {
    // Clamp coordinates to within the dimensions of the source image
    uint16x8_t x_clamped =
        vminq_u16(vreinterpretq_u16_s16(vmaxq_s16(x, vdupq_n_s16(0))), v_xmax_);
    uint16x8_t y_clamped =
        vminq_u16(vreinterpretq_u16_s16(vmaxq_s16(y, vdupq_n_s16(0))), v_ymax_);

    // Calculate offsets from coordinates (y * stride/sizeof(ScalarType) + x)
    uint32x4_t indices_low =
        vmlal_u16(vmovl_u16(vget_low_u16(x_clamped)), vget_low_u16(y_clamped),
                  vget_low_u16(v_src_element_stride_));
    uint32x4_t indices_high = vmlal_high_u16(vmovl_high_u16(x_clamped),
                                             y_clamped, v_src_element_stride_);

    // Read pixels from source
    uint16x8_t pixels = {
        src_rows_[vgetq_lane_u32(indices_low, 0)],
        src_rows_[vgetq_lane_u32(indices_low, 1)],
        src_rows_[vgetq_lane_u32(indices_low, 2)],
        src_rows_[vgetq_lane_u32(indices_low, 3)],
        src_rows_[vgetq_lane_u32(indices_high, 0)],
        src_rows_[vgetq_lane_u32(indices_high, 1)],
        src_rows_[vgetq_lane_u32(indices_high, 2)],
        src_rows_[vgetq_lane_u32(indices_high, 3)],
    };

    return pixels;
  }

  uint16x8_t interpolate(uint16x8_t a, uint16x8_t b, uint16x8_t c,
                         uint16x8_t d) {
    auto interpolate_horizontal = [&](uint16x4_t left, uint16x4_t right,
                                      uint16x4_t frac,
                                      uint16x4_t nfrac) -> uint32x4_t {
      return vmlal_u16(vmull_u16(nfrac, left), frac, right);
    };

    auto interpolate_horizontal_low = [&](uint16x8_t left, uint16x8_t right,
                                          uint16x8_t frac,
                                          uint16x8_t nfrac) -> uint32x4_t {
      return interpolate_horizontal(vget_low_u16(left), vget_low_u16(right),
                                    vget_low_u16(frac), vget_low_u16(nfrac));
    };

    auto interpolate_horizontal_high = [&](uint16x8_t left, uint16x8_t right,
                                           uint16x8_t frac,
                                           uint16x8_t nfrac) -> uint32x4_t {
      return interpolate_horizontal(vget_high_u16(left), vget_high_u16(right),
                                    vget_high_u16(frac), vget_high_u16(nfrac));
    };

    // Offset pixel values by 0.5 before rounding down.
    const uint32x4_t bias = vdupq_n_u32(REMAP16POINT5_FRAC_MAX_SQUARE / 2);

    auto interpolate_vertical = [&](uint32x4_t a, uint32x4_t b, uint32x4_t frac,
                                    uint32x4_t nfrac) -> uint32x4_t {
      uint32x4_t res32 = vmlaq_u32(vmlaq_u32(bias, a, nfrac), b, frac);
      return vshrq_n_u32(res32, 2 * REMAP16POINT5_FRAC_BITS);
    };

    uint32x4_t line0_low = interpolate_horizontal_low(a, c, xfrac_, nxfrac_);
    uint32x4_t line1_low = interpolate_horizontal_low(b, d, xfrac_, nxfrac_);
    uint32x4_t line0_high = interpolate_horizontal_high(a, c, xfrac_, nxfrac_);
    uint32x4_t line1_high = interpolate_horizontal_high(b, d, xfrac_, nxfrac_);

    uint32x4_t lo = interpolate_vertical(line0_low, line1_low,
                                         vmovl_u16(vget_low_u16(yfrac_)),
                                         vmovl_u16(vget_low_u16(nyfrac_)));
    uint32x4_t hi =
        interpolate_vertical(line0_high, line1_high, vmovl_high_u16(yfrac_),
                             vmovl_high_u16(nyfrac_));

    // Discard upper 16 bits of each element (low the precision back to original
    // 16 bits)
    uint16x8_t result =
        vuzp1q_u16(vreinterpretq_u16_u32(lo), vreinterpretq_u16_u32(hi));
    return result;
  }

 private:
  Rows<const ScalarType> src_rows_;
  uint16x8_t v_src_element_stride_;
  int16x8_t v_xmax_;
  int16x8_t v_ymax_;
  uint16x8_t xfrac_;
  uint16x8_t yfrac_;
  uint16x8_t nxfrac_;
  uint16x8_t nyfrac_;
  int16x8_t x0_;
  int16x8_t x1_;
  int16x8_t y0_;
  int16x8_t y1_;
};  // end of class RemapS16Point5Replicate<uint16_t>

template <typename ScalarType>
class RemapS16Point5ConstantBorder;

template <>
class RemapS16Point5ConstantBorder<uint8_t> {
 public:
  using ScalarType = uint8_t;
  using MapVecTraits = neon::VecTraits<int16_t>;

  RemapS16Point5ConstantBorder(Rows<const ScalarType> src_rows,
                               size_t src_width, size_t src_height,
                               const ScalarType *border_value)
      : src_rows_{src_rows},
        v_src_stride_{vdupq_n_u16(static_cast<uint16_t>(src_rows_.stride()))},
        v_width_{vdupq_n_u16(static_cast<uint16_t>(src_width))},
        v_height_{vdupq_n_u16(static_cast<uint16_t>(src_height))},
        v_border_{vdup_n_u8(*border_value)} {}

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<const uint16_t> mapfrac, Columns<ScalarType> dst) {
    auto vector_path = [&](size_t step) {
      int16x8x2_t xy = vld2q_s16(&mapxy[0]);
      uint16x8_t frac = vld1q_u16(&mapfrac[0]);
      uint8x8_t frac_max = vdup_n_u8(REMAP16POINT5_FRAC_MAX);
      uint8x8_t frac_mask = vdup_n_u8(REMAP16POINT5_FRAC_MAX - 1);
      uint8x8_t xfrac = vand_u8(vmovn_u16(frac), frac_mask);
      uint8x8_t yfrac =
          vand_u8(vshrn_n_u16(frac, REMAP16POINT5_FRAC_BITS), frac_mask);
      uint8x8_t nxfrac = vsub_u8(frac_max, xfrac);
      uint8x8_t nyfrac = vsub_u8(frac_max, yfrac);

      uint16x8_t one = vdupq_n_u16(1);
      uint16x8_t x0 = vreinterpretq_u16_s16(xy.val[0]);
      uint16x8_t y0 = vreinterpretq_u16_s16(xy.val[1]);
      uint16x8_t x1 = vaddq_u16(x0, one);
      uint16x8_t y1 = vaddq_u16(y0, one);

      uint8x8_t v00 = load_pixels_or_constant_border(
          src_rows_, v_src_stride_, v_width_, v_height_, v_border_, x0, y0);
      uint8x8_t v01 = load_pixels_or_constant_border(
          src_rows_, v_src_stride_, v_width_, v_height_, v_border_, x0, y1);
      uint8x8_t v10 = load_pixels_or_constant_border(
          src_rows_, v_src_stride_, v_width_, v_height_, v_border_, x1, y0);
      uint8x8_t v11 = load_pixels_or_constant_border(
          src_rows_, v_src_stride_, v_width_, v_height_, v_border_, x1, y1);

      uint8x8_t result = interpolate(v00, v01, v10, v11, xfrac, vmovl_u8(yfrac),
                                     nxfrac, vmovl_u8(nyfrac));

      vst1_u8(&dst[0], result);
      mapxy += ptrdiff_t(step);
      mapfrac += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };
    LoopUnroll loop{width, MapVecTraits::num_lanes()};
    loop.unroll_once(vector_path);
    ptrdiff_t back_step = static_cast<ptrdiff_t>(loop.step()) -
                          static_cast<ptrdiff_t>(loop.remaining_length());
    mapxy -= back_step;
    mapfrac -= back_step;
    dst -= back_step;
    loop.remaining([&](size_t, size_t step) { vector_path(step); });
  }

 private:
  uint8x8_t load_pixels_or_constant_border(Rows<const uint8_t> &src_rows_,
                                           uint16x8_t v_src_element_stride_,
                                           uint16x8_t v_width_,
                                           uint16x8_t v_height_,
                                           uint8x8_t v_border_, uint16x8_t x,
                                           uint16x8_t y) {
    // Find whether coordinates are within the image dimensions.
    // Negative coordinates are interpreted as large values due to the s16->u16
    // reinterpretation.
    uint16x8_t in_range =
        vandq_u16(vcltq_u16(vreinterpretq_u16_s16(x), v_width_),
                  vcltq_u16(vreinterpretq_u16_s16(y), v_height_));

    // Zero out-of-range coordinates.
    x = vandq_u16(in_range, x);
    y = vandq_u16(in_range, y);

    // Calculate offsets from coordinates (y * stride/sizeof(ScalarType) + x)
    uint32x4_t indices_low =
        vmlal_u16(vmovl_u16(vget_low_u16(x)), vget_low_u16(y),
                  vget_low_u16(v_src_element_stride_));
    uint32x4_t indices_high =
        vmlal_high_u16(vmovl_high_u16(x), y, v_src_element_stride_);

    // Read pixels from source
    uint8x8_t pixels = {
        src_rows_[vgetq_lane_u32(indices_low, 0)],
        src_rows_[vgetq_lane_u32(indices_low, 1)],
        src_rows_[vgetq_lane_u32(indices_low, 2)],
        src_rows_[vgetq_lane_u32(indices_low, 3)],
        src_rows_[vgetq_lane_u32(indices_high, 0)],
        src_rows_[vgetq_lane_u32(indices_high, 1)],
        src_rows_[vgetq_lane_u32(indices_high, 2)],
        src_rows_[vgetq_lane_u32(indices_high, 3)],
    };
    // Select between source pixels and border colour
    return vbsl_u8(vmovn_u16(in_range), pixels, v_border_);
  }

  uint8x8_t interpolate(uint8x8_t v00, uint8x8_t v01, uint8x8_t v10,
                        uint8x8_t v11, uint8x8_t xfrac, uint16x8_t yfrac,
                        uint8x8_t nxfrac, uint16x8_t nyfrac) {
    auto interpolate_horizontal = [&](uint8x8_t left, uint8x8_t right) {
      return vmlal_u8(vmull_u8(nxfrac, left), xfrac, right);
    };

    // Offset pixel values from [0,255] to [0.5,255.5] before rounding down.
    const uint32x4_t bias = vdupq_n_u32(REMAP16POINT5_FRAC_MAX_SQUARE / 2);

    auto interpolate_vertical = [&](uint16x4_t a, uint16x4_t b, uint16x4_t frac,
                                    uint16x4_t nfrac) {
      uint32x4_t res32 = vmlal_u16(vmlal_u16(bias, a, nfrac), b, frac);
      return vshrn_n_u32(res32, 2 * REMAP16POINT5_FRAC_BITS);
    };

    uint16x8_t line0 = interpolate_horizontal(v00, v10);
    uint16x8_t line1 = interpolate_horizontal(v01, v11);

    uint16x4_t lo =
        interpolate_vertical(vget_low_u16(line0), vget_low_u16(line1),
                             vget_low_u16(yfrac), vget_low_u16(nyfrac));
    uint16x4_t hi =
        interpolate_vertical(vget_high_u16(line0), vget_high_u16(line1),
                             vget_high_u16(yfrac), vget_high_u16(nyfrac));

    // Discard upper 8 bits of each element and combine low and high parts into
    // a single register.
    return vuzp1_u8(vreinterpret_u8_u16(lo), vreinterpret_u8_u16(hi));
  }

  Rows<const ScalarType> src_rows_;
  uint16x8_t v_src_stride_;
  uint16x8_t v_width_;
  uint16x8_t v_height_;
  uint8x8_t v_border_;
};  // end of class RemapS16Point5ConstantBorder<uint8_t>

template <>
class RemapS16Point5ConstantBorder<uint16_t> {
 public:
  using ScalarType = uint16_t;
  using MapVecTraits = neon::VecTraits<int16_t>;

  RemapS16Point5ConstantBorder(Rows<const ScalarType> src_rows,
                               size_t src_width, size_t src_height,
                               const ScalarType *border_value)
      : src_rows_{src_rows},
        v_src_element_stride_{vdupq_n_u16(
            static_cast<uint16_t>(src_rows_.stride() / sizeof(ScalarType)))},
        v_width_{vdupq_n_u16(static_cast<uint16_t>(src_width))},
        v_height_{vdupq_n_u16(static_cast<uint16_t>(src_height))},
        v_border_{vdupq_n_u16(*border_value)},
        xfrac_{vdupq_n_u16(0)},
        yfrac_{vdupq_n_u16(0)},
        nxfrac_{vdupq_n_u16(0)},
        nyfrac_{vdupq_n_u16(0)},
        x0_{vdupq_n_s16(0)},
        x1_{vdupq_n_s16(0)},
        y0_{vdupq_n_s16(0)},
        y1_{vdupq_n_s16(0)} {}

  void prepare_maps(Columns<const int16_t> mapxy,
                    Columns<const uint16_t> mapfrac) {
    int16x8x2_t xy = vld2q_s16(&mapxy[0]);
    uint16x8_t frac = vld1q_u16(&mapfrac[0]);
    uint16x8_t frac_max = vdupq_n_u16(REMAP16POINT5_FRAC_MAX);
    uint16x8_t frac_mask = vdupq_n_u16(REMAP16POINT5_FRAC_MAX - 1);
    xfrac_ = vandq_u16(frac, frac_mask);
    yfrac_ = vandq_u16(vshrq_n_u16(frac, REMAP16POINT5_FRAC_BITS), frac_mask);
    nxfrac_ = vsubq_u16(frac_max, xfrac_);
    nyfrac_ = vsubq_u16(frac_max, yfrac_);

    uint16x8_t one = vdupq_n_u16(1);
    x0_ = xy.val[0];
    y0_ = xy.val[1];
    x1_ = vaddq_u16(x0_, one);
    y1_ = vaddq_u16(y0_, one);
  }

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<const uint16_t> mapfrac, Columns<ScalarType> dst) {
    auto vector_path = [&](size_t step) {
      prepare_maps(mapxy, mapfrac);
      transform_pixels(dst);

      mapxy += ptrdiff_t(step);
      mapfrac += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };
    LoopUnroll loop{width, MapVecTraits::num_lanes()};
    loop.unroll_once(vector_path);
    ptrdiff_t back_step = static_cast<ptrdiff_t>(loop.step()) -
                          static_cast<ptrdiff_t>(loop.remaining_length());
    mapxy -= back_step;
    mapfrac -= back_step;
    dst -= back_step;
    loop.remaining([&](size_t, size_t step) { vector_path(step); });
  }

  void transform_pixels(Columns<uint16_t> dst) {
    uint16x8_t a = load_pixels(x0_, y0_);
    uint16x8_t b = load_pixels(x0_, y1_);
    uint16x8_t c = load_pixels(x1_, y0_);
    uint16x8_t d = load_pixels(x1_, y1_);

    uint16x8_t result = interpolate(a, b, c, d);

    vst1q_u16(&dst[0], result);
  }

  uint16x8_t load_pixels(uint16x8_t x, uint16x8_t y) {
    // Find whether coordinates are within the image dimensions.
    // Negative coordinates are interpreted as large values due to the s16->u16
    // reinterpretation.
    uint16x8_t in_range =
        vandq_u16(vcltq_u16(vreinterpretq_u16_s16(x), v_width_),
                  vcltq_u16(vreinterpretq_u16_s16(y), v_height_));

    // Zero out-of-range coordinates.
    x = vandq_u16(in_range, x);
    y = vandq_u16(in_range, y);

    // Calculate offsets from coordinates (y * stride/sizeof(ScalarType) + x)
    uint32x4_t indices_low =
        vmlal_u16(vmovl_u16(vget_low_u16(x)), vget_low_u16(y),
                  vget_low_u16(v_src_element_stride_));
    uint32x4_t indices_high =
        vmlal_high_u16(vmovl_high_u16(x), y, v_src_element_stride_);

    // Read pixels from source
    uint16x8_t pixels = {
        src_rows_[vgetq_lane_u32(indices_low, 0)],
        src_rows_[vgetq_lane_u32(indices_low, 1)],
        src_rows_[vgetq_lane_u32(indices_low, 2)],
        src_rows_[vgetq_lane_u32(indices_low, 3)],
        src_rows_[vgetq_lane_u32(indices_high, 0)],
        src_rows_[vgetq_lane_u32(indices_high, 1)],
        src_rows_[vgetq_lane_u32(indices_high, 2)],
        src_rows_[vgetq_lane_u32(indices_high, 3)],
    };
    // Select between source pixels and border colour
    return vbslq_u16(in_range, pixels, v_border_);
  }

  uint16x8_t interpolate(uint16x8_t a, uint16x8_t b, uint16x8_t c,
                         uint16x8_t d) {
    auto interpolate_horizontal = [&](uint16x4_t left, uint16x4_t right,
                                      uint16x4_t frac,
                                      uint16x4_t nfrac) -> uint32x4_t {
      return vmlal_u16(vmull_u16(nfrac, left), frac, right);
    };

    auto interpolate_horizontal_low = [&](uint16x8_t left, uint16x8_t right,
                                          uint16x8_t frac,
                                          uint16x8_t nfrac) -> uint32x4_t {
      return interpolate_horizontal(vget_low_u16(left), vget_low_u16(right),
                                    vget_low_u16(frac), vget_low_u16(nfrac));
    };

    auto interpolate_horizontal_high = [&](uint16x8_t left, uint16x8_t right,
                                           uint16x8_t frac,
                                           uint16x8_t nfrac) -> uint32x4_t {
      return interpolate_horizontal(vget_high_u16(left), vget_high_u16(right),
                                    vget_high_u16(frac), vget_high_u16(nfrac));
    };

    // Offset pixel values by 0.5 before rounding down.
    const uint32x4_t bias = vdupq_n_u32(REMAP16POINT5_FRAC_MAX_SQUARE / 2);

    auto interpolate_vertical = [&](uint32x4_t a, uint32x4_t b, uint32x4_t frac,
                                    uint32x4_t nfrac) -> uint32x4_t {
      uint32x4_t res32 = vmlaq_u32(vmlaq_u32(bias, a, nfrac), b, frac);
      return vshrq_n_u32(res32, 2 * REMAP16POINT5_FRAC_BITS);
    };

    uint32x4_t line0_low = interpolate_horizontal_low(a, c, xfrac_, nxfrac_);
    uint32x4_t line1_low = interpolate_horizontal_low(b, d, xfrac_, nxfrac_);
    uint32x4_t line0_high = interpolate_horizontal_high(a, c, xfrac_, nxfrac_);
    uint32x4_t line1_high = interpolate_horizontal_high(b, d, xfrac_, nxfrac_);

    uint32x4_t lo = interpolate_vertical(line0_low, line1_low,
                                         vmovl_u16(vget_low_u16(yfrac_)),
                                         vmovl_u16(vget_low_u16(nyfrac_)));
    uint32x4_t hi =
        interpolate_vertical(line0_high, line1_high, vmovl_high_u16(yfrac_),
                             vmovl_high_u16(nyfrac_));

    // Discard upper 16 bits of each element (low the precision back to original
    // 16 bits)
    uint16x8_t result =
        vuzp1q_u16(vreinterpretq_u16_u32(lo), vreinterpretq_u16_u32(hi));
    return result;
  }

 private:
  Rows<const ScalarType> src_rows_;
  uint16x8_t v_src_element_stride_;
  uint16x8_t v_width_;
  uint16x8_t v_height_;
  uint16x8_t v_border_;
  uint16x8_t xfrac_;
  uint16x8_t yfrac_;
  uint16x8_t nxfrac_;
  uint16x8_t nyfrac_;
  int16x8_t x0_;
  int16x8_t x1_;
  int16x8_t y0_;
  int16x8_t y1_;
};  // end of class RemapS16Point5ConstantBorder<uint16_t>

// Most of the complexity comes from parameter checking.
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <typename T>
kleidicv_error_t remap_s16point5(
    const T *src, size_t src_stride, size_t src_width, size_t src_height,
    T *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t *mapxy, size_t mapxy_stride,
    const uint16_t *mapfrac, size_t mapfrac_stride,
    [[maybe_unused]] kleidicv_border_type_t border_type,
    [[maybe_unused]] const T *border_value) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_POINTER_AND_STRIDE(mapxy, mapxy_stride, dst_height);
  CHECK_POINTER_AND_STRIDE(mapfrac, mapfrac_stride, dst_height);
  CHECK_IMAGE_SIZE(src_width, src_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);
  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT && nullptr == border_value) {
    return KLEIDICV_ERROR_NULL_POINTER;
  }

  if (!remap_s16point5_is_implemented<T>(src_stride, src_width, src_height,
                                         dst_width, border_type, channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  Rows<const T> src_rows{src, src_stride, channels};
  Rows<const int16_t> mapxy_rows{mapxy, mapxy_stride, 2};
  Rows<const uint16_t> mapfrac_rows{mapfrac, mapfrac_stride, 1};
  Rows<T> dst_rows{dst, dst_stride, channels};
  Rectangle rect{dst_width, dst_height};
  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT) {
    RemapS16Point5ConstantBorder<T> operation{src_rows, src_width, src_height,
                                              border_value};
    zip_rows(operation, rect, mapxy_rows, mapfrac_rows, dst_rows);
  } else {
    assert(border_type == KLEIDICV_BORDER_TYPE_REPLICATE);
    RemapS16Point5Replicate<T> operation{src_rows, src_width, src_height};
    zip_rows(operation, rect, mapxy_rows, mapfrac_rows, dst_rows);
  }
  return KLEIDICV_OK;
}
// NOLINTEND(readability-function-cognitive-complexity)

template <typename ScalarType, bool IsLarge>
class RemapF32Replicate;

template <bool IsLarge>
class RemapF32Replicate<uint8_t, IsLarge> {
 public:
  using ScalarType = uint8_t;
  using MapVecTraits = neon::VecTraits<float>;
  using MapVectorType = typename MapVecTraits::VectorType;  // float32x4_t

  RemapF32Replicate(Rows<const ScalarType> src_rows, size_t src_width,
                    size_t src_height)
      : src_rows_{src_rows},
        v_src_stride_{vdup_n_u32(static_cast<uint32_t>(src_rows_.stride()))},
        vq_src_stride_{vdupq_n_u32(static_cast<uint32_t>(src_rows_.stride()))},
        v_xmax_{vdupq_n_u32(static_cast<uint32_t>(src_width - 1))},
        v_ymax_{vdupq_n_u32(static_cast<uint32_t>(src_height - 1))} {}

  void process_row(size_t width, Columns<const float> mapx,
                   Columns<const float> mapy, Columns<ScalarType> dst) {
    const size_t kStep = VecTraits<float>::num_lanes();

    auto load_src_into_floats_small = [&](uint32x4_t x, uint32x4_t y) {
      uint32x4_t offset = vmlaq_u32(x, y, vq_src_stride_);
      uint64_t acc =
          static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 0)]) |
          (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 1)]) << 32);
      uint64x2_t rawsrc = vdupq_n_u64(acc);
      acc = static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 2)]) |
            (static_cast<uint64_t>(src_rows_[vgetq_lane_u32(offset, 3)]) << 32);
      rawsrc = vsetq_lane_u64(acc, rawsrc, 1);
      return vcvtq_f32_u32(vreinterpretq_u32_u64(rawsrc));
    };

    auto load_src_into_floats_large = [&](uint32x4_t x, uint32x4_t y) {
      uint64x2_t offset_low =
          vmlal_u32(vmovl_u32(vget_low_u32(x)), vget_low_u32(y), v_src_stride_);
      uint64x2_t offset_high =
          vmlal_u32(vmovl_high_u32(x), vget_high_u32(y), v_src_stride_);
      uint64_t acc =
          static_cast<uint64_t>(src_rows_[vgetq_lane_u64(offset_low, 0)]) |
          (static_cast<uint64_t>(src_rows_[vgetq_lane_u64(offset_low, 1)])
           << 32);
      uint64x2_t rawsrc = vdupq_n_u64(acc);
      acc = static_cast<uint64_t>(src_rows_[vgetq_lane_u64(offset_high, 0)]) |
            (static_cast<uint64_t>(src_rows_[vgetq_lane_u64(offset_high, 1)])
             << 32);
      rawsrc = vsetq_lane_u64(acc, rawsrc, 1);
      return vcvtq_f32_u32(vreinterpretq_u32_u64(rawsrc));
    };

    auto load = [&](uint32x4_t x, uint32x4_t y) {
      if constexpr (IsLarge) {
        return load_src_into_floats_large(x, y);
      } else {
        return load_src_into_floats_small(x, y);
      }
    };

    auto vector_path_1 = [&](const float *ptr_mapx, const float *ptr_mapy) {
      MapVectorType x = vld1q_f32(ptr_mapx);
      MapVectorType y = vld1q_f32(ptr_mapy);
      // Truncating convert to int
      uint32x4_t x0 = vminq_u32(vcvtmq_u32_f32(x), v_xmax_);
      uint32x4_t y0 = vminq_u32(vcvtmq_u32_f32(y), v_ymax_);

      // Get fractional part, or 0 if out of range
      float32x4_t zero = vdupq_n_f32(0.F);
      uint32x4_t x_in_range =
          vandq_u32(vcgeq_f32(x, zero), vcltq_u32(x0, v_xmax_));
      uint32x4_t y_in_range =
          vandq_u32(vcgeq_f32(y, zero), vcltq_u32(y0, v_ymax_));
      float32x4_t xfrac =
          vbslq_f32(x_in_range, vsubq_f32(x, vrndmq_f32(x)), zero);
      float32x4_t yfrac =
          vbslq_f32(y_in_range, vsubq_f32(y, vrndmq_f32(y)), zero);

      // x1 = x0 + 1, except if it's already xmax or out of range
      uint32x4_t x1 = vsubq_u32(x0, x_in_range);
      uint32x4_t y1 = vsubq_u32(y0, y_in_range);

      // Calculate offsets from coordinates (y * stride + x)
      // a: top left, b: top right, c: bottom left, d: bottom right
      float32x4_t a = load(x0, y0);
      float32x4_t b = load(x1, y0);
      float32x4_t line0 = vmlaq_f32(a, vsubq_f32(b, a), xfrac);
      float32x4_t c = load(x0, y1);
      float32x4_t d = load(x1, y1);
      float32x4_t line1 = vmlaq_f32(c, vsubq_f32(d, c), xfrac);
      float32x4_t result = vmlaq_f32(line0, vsubq_f32(line1, line0), yfrac);
      return vminq_u32(vdupq_n_u32(0xFF), vcvtaq_u32_f32(result));
    };

    auto vector_path_4 = [&](size_t step) {  // step = 4*4 = 16
      const float *ptr_mapx = &mapx[0];
      const float *ptr_mapy = &mapy[0];
      uint32x4_t res0 = vector_path_1(ptr_mapx, ptr_mapy);

      ptr_mapx += kStep;
      ptr_mapy += kStep;
      uint32x4_t res1 = vector_path_1(ptr_mapx, ptr_mapy);
      uint16x8_t result16_0 = vuzp1q_u16(res0, res1);

      ptr_mapx += kStep;
      ptr_mapy += kStep;
      res0 = vector_path_1(ptr_mapx, ptr_mapy);

      ptr_mapx += kStep;
      ptr_mapy += kStep;
      res1 = vector_path_1(ptr_mapx, ptr_mapy);
      uint16x8_t result16_1 = vuzp1q_u16(res0, res1);
      vst1q_u8(&dst[0], vuzp1q_u8(result16_0, result16_1));
      mapx += ptrdiff_t(step);
      mapy += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };

    LoopUnroll loop{width, kStep};
    loop.unroll_four_times(vector_path_4);
    loop.unroll_once([&](size_t step) {
      const float *ptr_mapx = &mapx[0];
      const float *ptr_mapy = &mapy[0];
      uint32x4_t result = vector_path_1(ptr_mapx, ptr_mapy);
      dst[0] = vgetq_lane_u32(result, 0);
      dst[1] = vgetq_lane_u32(result, 1);
      dst[2] = vgetq_lane_u32(result, 2);
      dst[3] = vgetq_lane_u32(result, 3);
      mapx += ptrdiff_t(step);
      mapy += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    });
    ptrdiff_t back_step = static_cast<ptrdiff_t>(loop.step()) -
                          static_cast<ptrdiff_t>(loop.remaining_length());
    mapx -= back_step;
    mapy -= back_step;
    dst -= back_step;
    loop.remaining([&](size_t, size_t) {
      const float *ptr_mapx = &mapx[0];
      const float *ptr_mapy = &mapy[0];
      uint32x4_t result = vector_path_1(ptr_mapx, ptr_mapy);
      dst[0] = vgetq_lane_u32(result, 0);
      dst[1] = vgetq_lane_u32(result, 1);
      dst[2] = vgetq_lane_u32(result, 2);
      dst[3] = vgetq_lane_u32(result, 3);
    });
  }

 private:
  Rows<const ScalarType> src_rows_;
  uint32x2_t v_src_stride_;   // load_large
  uint32x4_t vq_src_stride_;  // load_small
  uint32x4_t v_xmax_;
  uint32x4_t v_ymax_;
};  // end of class RemapF32Replicate<uint8_t>

template <typename ScalarType, bool IsLarge>
class RemapF32ConstantBorder;

// TODO: Need to refactor to reduce the complexity
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <bool IsLarge>
class RemapF32ConstantBorder<uint8_t, IsLarge> {
 public:
  using ScalarType = uint8_t;
  using MapVecTraits = neon::VecTraits<float>;
  using MapVectorType = typename MapVecTraits::VectorType;  // float32x4_t

  RemapF32ConstantBorder(Rows<const ScalarType> src_rows, size_t src_width,
                         size_t src_height, const ScalarType *border_value)
      : src_rows_{src_rows},
        src_width_{src_width},
        src_height_{src_height},
        border_value_{border_value} {}

  void process_row(size_t width, Columns<const float> mapx,
                   Columns<const float> mapy, Columns<ScalarType> dst) {
    const size_t kStep = VecTraits<float>::num_lanes();

    auto get_edge_pixels =
        [&](unsigned &a_result, unsigned &b_result, unsigned &c_result,
            unsigned &d_result, int x0, int y0, ptrdiff_t offset,
            Rows<const ScalarType> src_rows, int src_width, int src_height) {
          if (y0 >= 0) {
            if (x0 >= 0) {
              a_result = src_rows[offset];
            }
            if (x0 + 1 < src_width) {
              b_result = src_rows[offset + 1];
            }
          }
          if (y0 + 1 < src_height) {
            offset += static_cast<ptrdiff_t>(src_rows.stride());
            if (x0 >= 0) {
              c_result = src_rows[offset];
            }
            if (x0 + 1 < src_width) {
              d_result = src_rows[offset + 1];
            }
          }
        };

    auto vector_path_1 = [&](const float *ptr_mapx, const float *ptr_mapy) {
      MapVectorType xf = vld1q_f32(ptr_mapx);
      MapVectorType yf = vld1q_f32(ptr_mapy);
      // Convert obviously out-of-range coordinates to values that are just
      // beyond the largest permitted image width & height. This avoids the need
      // for special case handling elsewhere.
      float32x4_t big = vdupq_n_f32(1 << 24);
      xf = vbslq_f32(vcleq_f32(vabsq_f32(xf), big), xf, big);
      yf = vbslq_f32(vcleq_f32(vabsq_f32(yf), big), yf, big);

      int32x4_t x0 = vcvtmq_s32_f32(xf);
      int32x4_t y0 = vcvtmq_s32_f32(yf);
      int x0_array[4], y0_array[4];
      unsigned a_array[4], b_array[4], c_array[4], d_array[4];
      vst1q_s32(x0_array, x0);
      vst1q_s32(y0_array, y0);
      for (int i = 0; i < 4; ++i) {
        int x0i = x0_array[i];
        int y0i = y0_array[i];
        ptrdiff_t offset = x0i + y0i * src_rows_.stride();

        // src_width < (1ULL << 24) && src_height_ < (1ULL << 24) is guaranteed
        if (x0i < 0 || x0i + 1 >= static_cast<int>(src_width_) || y0i < 0 ||
            y0i + 1 >= static_cast<int>(src_height_)) {
          // Not entirely within the source image

          a_array[i] = b_array[i] = c_array[i] = d_array[i] = border_value_[0];

          if (x0i < -1 || x0i >= static_cast<int>(src_width_) || y0i < -1 ||
              y0i >= static_cast<int>(src_height_)) {
            // Completely outside the source image
            continue;
          }

          get_edge_pixels(a_array[i], b_array[i], c_array[i], d_array[i], x0i,
                          y0i, offset, src_rows_, src_width_, src_height_);
          continue;
        }

        // Completely inside the source image
        a_array[i] = src_rows_[offset];
        b_array[i] = src_rows_[offset + 1];
        offset += src_rows_.stride();
        c_array[i] = src_rows_[offset];
        d_array[i] = src_rows_[offset + 1];
      }

      float32x4_t xfrac = vsubq_f32(xf, vrndmq_f32(xf));
      float32x4_t yfrac = vsubq_f32(yf, vrndmq_f32(yf));
      float32x4_t a = vcvtq_f32_u32(vld1q_u32(a_array));
      float32x4_t b = vcvtq_f32_u32(vld1q_u32(b_array));
      float32x4_t line0 = vmlaq_f32(a, vsubq_f32(b, a), xfrac);
      float32x4_t c = vcvtq_f32_u32(vld1q_u32(c_array));
      float32x4_t d = vcvtq_f32_u32(vld1q_u32(d_array));
      float32x4_t line1 = vmlaq_f32(c, vsubq_f32(d, c), xfrac);
      float32x4_t result = vmlaq_f32(line0, vsubq_f32(line1, line0), yfrac);
      return vcvtaq_u32_f32(result);
    };

    auto vector_path_4 = [&](size_t step) {  // step = 4*4 = 16
      const float *ptr_mapx = &mapx[0];
      const float *ptr_mapy = &mapy[0];
      uint32x4_t res0 = vector_path_1(ptr_mapx, ptr_mapy);

      ptr_mapx += kStep;
      ptr_mapy += kStep;
      uint32x4_t res1 = vector_path_1(ptr_mapx, ptr_mapy);
      uint16x8_t result16_0 = vuzp1q_u16(res0, res1);

      ptr_mapx += kStep;
      ptr_mapy += kStep;
      res0 = vector_path_1(ptr_mapx, ptr_mapy);

      ptr_mapx += kStep;
      ptr_mapy += kStep;
      res1 = vector_path_1(ptr_mapx, ptr_mapy);
      uint16x8_t result16_1 = vuzp1q_u16(res0, res1);
      vst1q_u8(&dst[0], vuzp1q_u8(result16_0, result16_1));
      mapx += ptrdiff_t(step);
      mapy += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };

    LoopUnroll loop{width, kStep};
    loop.unroll_four_times(vector_path_4);
    loop.unroll_once([&](size_t step) {
      const float *ptr_mapx = &mapx[0];
      const float *ptr_mapy = &mapy[0];
      uint32x4_t result = vector_path_1(ptr_mapx, ptr_mapy);
      dst[0] = vgetq_lane_u32(result, 0);
      dst[1] = vgetq_lane_u32(result, 1);
      dst[2] = vgetq_lane_u32(result, 2);
      dst[3] = vgetq_lane_u32(result, 3);
      mapx += ptrdiff_t(step);
      mapy += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    });
    ptrdiff_t back_step = static_cast<ptrdiff_t>(loop.step()) -
                          static_cast<ptrdiff_t>(loop.remaining_length());
    mapx -= back_step;
    mapy -= back_step;
    dst -= back_step;
    loop.remaining([&](size_t, size_t) {
      const float *ptr_mapx = &mapx[0];
      const float *ptr_mapy = &mapy[0];
      uint32x4_t result = vector_path_1(ptr_mapx, ptr_mapy);
      dst[0] = vgetq_lane_u32(result, 0);
      dst[1] = vgetq_lane_u32(result, 1);
      dst[2] = vgetq_lane_u32(result, 2);
      dst[3] = vgetq_lane_u32(result, 3);
    });
  }

 private:
  Rows<const ScalarType> src_rows_;
  size_t src_width_;
  size_t src_height_;
  const ScalarType *border_value_;
};  // end of class RemapF32ConstantBorder<uint8_t>
// NOLINTEND(readability-function-cognitive-complexity)

// Most of the complexity comes from parameter checking.
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <typename T>
kleidicv_error_t remap_f32(const T *src, size_t src_stride, size_t src_width,
                           size_t src_height, T *dst, size_t dst_stride,
                           size_t dst_width, size_t dst_height, size_t channels,
                           float *mapx, size_t mapx_stride, float *mapy,
                           size_t mapy_stride,
                           kleidicv_interpolation_type_t interpolation,
                           kleidicv_border_type_t border_type,
                           [[maybe_unused]] const T *border_value) {
  // may need to remove the maybe_unused
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_POINTER_AND_STRIDE(mapx, mapx_stride, dst_height);
  CHECK_POINTER_AND_STRIDE(mapy, mapy_stride, dst_height);
  CHECK_IMAGE_SIZE(src_width, src_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);
  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT && nullptr == border_value) {
    return KLEIDICV_ERROR_NULL_POINTER;
  }

  if (!remap_f32_is_implemented<T>(src_stride, src_width, src_height, dst_width,
                                   border_type, channels, interpolation)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  // Calculating in float32_t will only be precise until 24 bits
  if (src_width >= (1ULL << 24) || src_height >= (1ULL << 24) ||
      dst_width >= (1ULL << 24) || dst_height >= (1ULL << 24)) {
    return KLEIDICV_ERROR_RANGE;
  }

  Rows<const T> src_rows{src, src_stride, channels};
  Rows<const float> mapx_rows{mapx, mapx_stride, 1};
  Rows<const float> mapy_rows{mapy, mapy_stride, 1};
  Rows<T> dst_rows{dst, dst_stride, channels};
  Rectangle rect{dst_width, dst_height};

  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT) {
    if (KLEIDICV_UNLIKELY(src_rows.stride() * src_height >= (1ULL << 32))) {
      RemapF32ConstantBorder<T, true> operation{src_rows, src_width, src_height,
                                                border_value};
      zip_rows(operation, rect, mapx_rows, mapy_rows, dst_rows);
    } else {
      RemapF32ConstantBorder<T, false> operation{src_rows, src_width,
                                                 src_height, border_value};
      zip_rows(operation, rect, mapx_rows, mapy_rows, dst_rows);
    }
  } else {
    assert(border_type == KLEIDICV_BORDER_TYPE_REPLICATE);
    if (KLEIDICV_UNLIKELY(src_rows.stride() * src_height >= (1ULL << 32))) {
      RemapF32Replicate<T, true> operation{src_rows, src_width, src_height};
      zip_rows(operation, rect, mapx_rows, mapy_rows, dst_rows);
    } else {
      RemapF32Replicate<T, false> operation{src_rows, src_width, src_height};
      zip_rows(operation, rect, mapx_rows, mapy_rows, dst_rows);
    }
  }

  return KLEIDICV_OK;
}
// NOLINTEND(readability-function-cognitive-complexity)

#define KLEIDICV_INSTANTIATE_TEMPLATE_REMAP_S16(type)                          \
  template KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t remap_s16<type>(          \
      const type *src, size_t src_stride, size_t src_width, size_t src_height, \
      type *dst, size_t dst_stride, size_t dst_width, size_t dst_height,       \
      size_t channels, const int16_t *mapxy, size_t mapxy_stride,              \
      kleidicv_border_type_t border_type, const type *border_value)

KLEIDICV_INSTANTIATE_TEMPLATE_REMAP_S16(uint8_t);
KLEIDICV_INSTANTIATE_TEMPLATE_REMAP_S16(uint16_t);

#define KLEIDICV_INSTANTIATE_TEMPLATE_REMAP_S16Point5(type)                    \
  template KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t remap_s16point5<type>(    \
      const type *src, size_t src_stride, size_t src_width, size_t src_height, \
      type *dst, size_t dst_stride, size_t dst_width, size_t dst_height,       \
      size_t channels, const int16_t *mapxy, size_t mapxy_stride,              \
      const uint16_t *mapfrac, size_t mapfrac_stride,                          \
      kleidicv_border_type_t border_type, const type *border_value)

KLEIDICV_INSTANTIATE_TEMPLATE_REMAP_S16Point5(uint8_t);
KLEIDICV_INSTANTIATE_TEMPLATE_REMAP_S16Point5(uint16_t);

#define KLEIDICV_INSTANTIATE_TEMPLATE_REMAP_F32(type)                          \
  template KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t remap_f32<type>(          \
      const type *src, size_t src_stride, size_t src_width, size_t src_height, \
      type *dst, size_t dst_stride, size_t dst_width, size_t dst_height,       \
      size_t channels, float *mapx, size_t mapx_stride, float *mapy,           \
      size_t mapy_stride, kleidicv_interpolation_type_t interpolation,         \
      kleidicv_border_type_t border_type, const type *border_value)

KLEIDICV_INSTANTIATE_TEMPLATE_REMAP_F32(uint8_t);

}  // namespace kleidicv::neon
