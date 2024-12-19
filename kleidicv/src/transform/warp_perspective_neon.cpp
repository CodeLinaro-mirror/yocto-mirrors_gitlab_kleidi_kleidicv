// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <arm_neon.h>

#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"
#include "kleidicv/traits.h"
#include "kleidicv/transform/warp_perspective.h"
#include "kleidicv/utils.h"

namespace kleidicv::neon {

// Template for WarpPerspective transformation.
// Destination pixels are filled from the source, by taking pixels using the
// transformed coordinates that are calculated as follows:
//
//                    [ T0, T1, T2 ]   [ x ]
//      (x',y',w') =  [ T3, T4, T5 ] * [ y ]
//                    [ T6, T7, T8 ]   [ 1 ]
//  then
//
//      xt = x' / w'
//      yt = y' / w'
//
//  or putting it together:
//
//      xt = (T0*x + T1*y + T2) / (T6*x + T7*y + T8)
//      yt = (T3*x + T4*y + T5) / (T6*x + T7*y + T8)
//

typedef struct {
  float32x4_t x, y;
} CoordVectorPair;

template <typename ScalarType, kleidicv_interpolation_type_t Inter,
          bool IsLarge>
void warp_perspective_operation(Rows<const ScalarType> src_rows,
                                size_t src_width, size_t src_height,
                                const float transform[9],
                                Rows<ScalarType> dst_rows, size_t dst_width,
                                size_t y_begin, size_t y_end) {
  static constexpr uint32_t first_few_x[] = {0, 1, 2, 3};
  uint32x4_t x0123_ = vld1q_u32(first_few_x);

  uint32x2_t v_src_stride =
      vdup_n_u32(static_cast<uint32_t>(src_rows.stride()));
  uint32x4_t vq_src_stride =
      vdupq_n_u32(static_cast<uint32_t>(src_rows.stride()));
  uint32x4_t v_xmax = vdupq_n_u32(static_cast<uint32_t>(src_width - 1));
  uint32x4_t v_ymax = vdupq_n_u32(static_cast<uint32_t>(src_height - 1));

  float32x4_t tx0, ty0, tw0;

  auto calculate_coordinates = [&](uint32_t x) {
    // The next few values can be calculated by adding the corresponding Tn*x
    float32x4_t fx = vcvtq_f32_u32(vaddq_u32(x0123_, vdupq_n_u32(x)));
    float32x4_t tx = vmlaq_n_f32(tx0, fx, transform[0]);
    float32x4_t ty = vmlaq_n_f32(ty0, fx, transform[3]);
    float32x4_t tw = vmlaq_n_f32(tw0, fx, transform[6]);

    // Calculate inverse weight because division is expensive
    float32x4_t iw = vdivq_f32(vdupq_n_f32(1.F), tw);
    // Calculate coordinates into the source image
    float32x4_t xf = vmulq_f32(tx, iw);
    float32x4_t yf = vmulq_f32(ty, iw);
    return CoordVectorPair{xf, yf};
  };

  auto vector_path_nearest_small = [&](uint32_t x, Columns<ScalarType> dst) {
    auto &&[xf, yf] = calculate_coordinates(x);
    // Take the integer part, clamp it to within the dimensions of the
    // source image (negative values are already saturated to 0)
    uint32x4_t xi = vminq_u32(v_xmax, vcvtaq_u32_f32(xf));
    uint32x4_t yi = vminq_u32(v_ymax, vcvtaq_u32_f32(yf));
    // Calculate offsets from coordinates (y * stride + x)
    // Use this path only when the final offsets fit into 32 bits
    uint32x4_t offsets = vmlaq_u32(xi, yi, vq_src_stride);
    // Copy pixels from source
    ptrdiff_t ix = static_cast<ptrdiff_t>(x);
    dst[ix] = src_rows[vgetq_lane_u32(offsets, 0)];
    dst[ix + 1] = src_rows[vgetq_lane_u32(offsets, 1)];
    dst[ix + 2] = src_rows[vgetq_lane_u32(offsets, 2)];
    dst[ix + 3] = src_rows[vgetq_lane_u32(offsets, 3)];
  };

  auto vector_path_nearest_large = [&](uint32_t x, Columns<ScalarType> dst) {
    auto &&[xf, yf] = calculate_coordinates(x);
    // Take the integer part, clamp it to within the dimensions of the
    // source image (negative values are already saturated to 0)
    uint32x4_t xi = vminq_u32(v_xmax, vcvtaq_u32_f32(xf));
    uint32x4_t yi = vminq_u32(v_ymax, vcvtaq_u32_f32(yf));
    // Calculate offsets from coordinates (y * stride + x)
    // To avoid losing precision, the final offsets should be in 64 bits
    uint64x2_t offsets_low =
        vmlal_u32(vmovl_u32(vget_low_u32(xi)), vget_low_u32(yi), v_src_stride);
    uint64x2_t offsets_high =
        vmlal_u32(vmovl_high_u32(xi), vget_high_u32(yi), v_src_stride);
    // Copy pixels from source
    ptrdiff_t ix = static_cast<ptrdiff_t>(x);
    dst[ix] = src_rows[vgetq_lane_u64(offsets_low, 0)];
    dst[ix + 1] = src_rows[vgetq_lane_u64(offsets_low, 1)];
    dst[ix + 2] = src_rows[vgetq_lane_u64(offsets_high, 0)];
    dst[ix + 3] = src_rows[vgetq_lane_u64(offsets_high, 1)];
  };

  auto load_src_into_floats_small = [&](uint32x4_t x, uint32x4_t y) {
    uint32x4_t offset = vmlaq_u32(x, y, vq_src_stride);
    uint64_t acc =
        static_cast<uint64_t>(src_rows[vgetq_lane_u32(offset, 0)]) |
        (static_cast<uint64_t>(src_rows[vgetq_lane_u32(offset, 1)]) << 32);
    uint64x2_t rawsrc = vdupq_n_u64(acc);
    acc = static_cast<uint64_t>(src_rows[vgetq_lane_u32(offset, 2)]) |
          (static_cast<uint64_t>(src_rows[vgetq_lane_u32(offset, 3)]) << 32);
    rawsrc = vsetq_lane_u64(acc, rawsrc, 1);
    return vcvtq_f32_u32(vreinterpretq_u32_u64(rawsrc));
  };

  auto load_src_into_floats_large = [&](uint32x4_t x, uint32x4_t y) {
    uint64x2_t offset_low =
        vmlal_u32(vmovl_u32(vget_low_u32(x)), vget_low_u32(y), v_src_stride);
    uint64x2_t offset_high =
        vmlal_u32(vmovl_high_u32(x), vget_high_u32(y), v_src_stride);
    uint64_t acc =
        static_cast<uint64_t>(src_rows[vgetq_lane_u64(offset_low, 0)]) |
        (static_cast<uint64_t>(src_rows[vgetq_lane_u64(offset_low, 1)]) << 32);
    uint64x2_t rawsrc = vdupq_n_u64(acc);
    acc =
        static_cast<uint64_t>(src_rows[vgetq_lane_u64(offset_high, 0)]) |
        (static_cast<uint64_t>(src_rows[vgetq_lane_u64(offset_high, 1)]) << 32);
    rawsrc = vsetq_lane_u64(acc, rawsrc, 1);
    return vcvtq_f32_u32(vreinterpretq_u32_u64(rawsrc));
  };

  auto calculate_linear = [&](uint32_t x) {
    auto load_floats = [&](uint32x4_t x, uint32x4_t y) {
      if constexpr (IsLarge) {
        return load_src_into_floats_large(x, y);
      } else {
        return load_src_into_floats_small(x, y);
      }
    };
    auto &&[xf, yf] = calculate_coordinates(x);
    // Truncating convert to int
    uint32x4_t x0 = vminq_u32(vcvtmq_u32_f32(xf), v_xmax);
    uint32x4_t y0 = vminq_u32(vcvtmq_u32_f32(yf), v_ymax);

    // Get fractional part, or 0 if out of range
    float32x4_t zero = vdupq_n_f32(0.F);
    uint32x4_t x_in_range =
        vandq_u32(vcgeq_f32(xf, zero), vcltq_u32(x0, v_xmax));
    uint32x4_t y_in_range =
        vandq_u32(vcgeq_f32(yf, zero), vcltq_u32(y0, v_ymax));
    float32x4_t xfrac =
        vbslq_f32(x_in_range, vsubq_f32(xf, vrndmq_f32(xf)), zero);
    float32x4_t yfrac =
        vbslq_f32(y_in_range, vsubq_f32(yf, vrndmq_f32(yf)), zero);

    // x1 = x0 + 1, except if it's already xmax or out of range
    uint32x4_t x1 = vsubq_u32(x0, x_in_range);
    uint32x4_t y1 = vsubq_u32(y0, y_in_range);

    // Calculate offsets from coordinates (y * stride + x)
    // a: top left, b: top right, c: bottom left, d: bottom right
    float32x4_t a = load_floats(x0, y0);
    float32x4_t b = load_floats(x1, y0);
    float32x4_t line0 = vmlaq_f32(a, vsubq_f32(b, a), xfrac);
    float32x4_t c = load_floats(x0, y1);
    float32x4_t d = load_floats(x1, y1);
    float32x4_t line1 = vmlaq_f32(c, vsubq_f32(d, c), xfrac);
    float32x4_t result = vmlaq_f32(line0, vsubq_f32(line1, line0), yfrac);
    return vminq_u32(vdupq_n_u32(0xFF), vcvtaq_u32_f32(result));
  };

  auto process_row = [&](uint32_t y, Columns<ScalarType> dst) {
    float dy = static_cast<float>(y);
    // Calculate half-transformed values at the first pixel (nominators)
    // tw =  T6*x + T7*y + T8
    // tx = (T0*x + T1*y + T2) / tw
    // ty = (T3*x + T4*y + T5) / tw
    tx0 = vdupq_n_f32(transform[1] * dy + transform[2]);
    ty0 = vdupq_n_f32(transform[4] * dy + transform[5]);
    tw0 = vdupq_n_f32(transform[7] * dy + transform[8]);

    static const size_t kStep = VecTraits<float>::num_lanes();
    LoopUnroll2<TryToAvoidTailLoop> loop{dst_width, kStep};
    if constexpr (Inter == KLEIDICV_INTERPOLATION_NEAREST) {
      if constexpr (IsLarge) {
        loop.unroll_once([&](size_t x) {
          vector_path_nearest_large(static_cast<uint32_t>(x), dst);
        });
      } else {
        loop.unroll_once([&](size_t x) {
          vector_path_nearest_small(static_cast<uint32_t>(x), dst);
        });
      }
    } else if constexpr (Inter == KLEIDICV_INTERPOLATION_LINEAR) {
      loop.unroll_four_times([&](size_t _x) {
        uint32_t x = static_cast<uint32_t>(_x);
        ScalarType *p_dst = &dst[static_cast<ptrdiff_t>(_x)];
        uint32x4_t res0 = calculate_linear(x);
        x += kStep;
        uint32x4_t res1 = calculate_linear(x);
        uint16x8_t result16_0 = vuzp1q_u16(res0, res1);
        x += kStep;
        res0 = calculate_linear(x);
        x += kStep;
        res1 = calculate_linear(x);
        uint16x8_t result16_1 = vuzp1q_u16(res0, res1);
        vst1q_u8(p_dst, vuzp1q_u8(result16_0, result16_1));
      });
      loop.unroll_once([&](size_t x) {
        ScalarType *p_dst = &dst[static_cast<ptrdiff_t>(x)];
        uint32x4_t res = calculate_linear(static_cast<uint32_t>(x));
        p_dst[0] = vgetq_lane_u32(res, 0);
        p_dst[1] = vgetq_lane_u32(res, 1);
        p_dst[2] = vgetq_lane_u32(res, 2);
        p_dst[3] = vgetq_lane_u32(res, 3);
      });
    } else {
      static_assert(Inter == KLEIDICV_INTERPOLATION_NEAREST ||
                        Inter == KLEIDICV_INTERPOLATION_LINEAR,
                    ": Unknown interpolation type!");
    }
  };

  for (size_t y = y_begin; y < y_end; ++y) {
    process_row(y, dst_rows.as_columns());
    ++dst_rows;
  }
}

// Most of the complexity comes from parameter checking.
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <typename T>
kleidicv_error_t warp_perspective_stripe(
    const T *src, size_t src_stride, size_t src_width, size_t src_height,
    T *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t y_begin, size_t y_end, const float transformation[9],
    size_t channels, kleidicv_interpolation_type_t interpolation,
    kleidicv_border_type_t, const T *) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_POINTERS(transformation);
  CHECK_IMAGE_SIZE(src_width, src_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);

  // Calculating in float32_t will only be precise until 24 bits, and
  // multiplication can only be done with 32x32 bits
  if (src_width >= (1ULL << 24) || src_height >= (1ULL << 24) ||
      dst_width >= (1ULL << 24) || dst_height >= (1ULL << 24) ||
      src_stride >= (1ULL << 32)) {
    return KLEIDICV_ERROR_RANGE;
  }

  Rows<const T> src_rows{src, src_stride, channels};
  Rows<T> dst_rows{dst, dst_stride, channels};
  dst_rows += y_begin;

  if (KLEIDICV_UNLIKELY(src_rows.stride() * src_height >= (1ULL << 32))) {
    switch (interpolation) {
      case KLEIDICV_INTERPOLATION_NEAREST:
        warp_perspective_operation<T, KLEIDICV_INTERPOLATION_NEAREST, true>(
            src_rows, src_width, src_height, transformation, dst_rows,
            dst_width, y_begin, y_end);
        break;
      case KLEIDICV_INTERPOLATION_LINEAR:
        warp_perspective_operation<T, KLEIDICV_INTERPOLATION_LINEAR, true>(
            src_rows, src_width, src_height, transformation, dst_rows,
            dst_width, y_begin, y_end);
        break;
      default:
        break;  // GCOVR_EXCL_LINE
    }
  } else {
    switch (interpolation) {
      case KLEIDICV_INTERPOLATION_NEAREST:
        warp_perspective_operation<T, KLEIDICV_INTERPOLATION_NEAREST, false>(
            src_rows, src_width, src_height, transformation, dst_rows,
            dst_width, y_begin, y_end);
        break;
      case KLEIDICV_INTERPOLATION_LINEAR:
        warp_perspective_operation<T, KLEIDICV_INTERPOLATION_LINEAR, false>(
            src_rows, src_width, src_height, transformation, dst_rows,
            dst_width, y_begin, y_end);
        break;
      default:
        break;  // GCOVR_EXCL_LINE
    }
  }
  // NOLINTEND(readability-function-cognitive-complexity)
  return KLEIDICV_OK;
}

#define KLEIDICV_INSTANTIATE_WARP_PERSPECTIVE(type)                            \
  template KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t                           \
  warp_perspective_stripe<type>(                                               \
      const type *src, size_t src_stride, size_t src_width, size_t src_height, \
      type *dst, size_t dst_stride, size_t dst_width, size_t dst_height,       \
      size_t y_begin, size_t y_end, const float transformation[9],             \
      size_t channels, kleidicv_interpolation_type_t interpolation,            \
      kleidicv_border_type_t border_type, const type *border_value)

KLEIDICV_INSTANTIATE_WARP_PERSPECTIVE(uint8_t);

}  // namespace kleidicv::neon
