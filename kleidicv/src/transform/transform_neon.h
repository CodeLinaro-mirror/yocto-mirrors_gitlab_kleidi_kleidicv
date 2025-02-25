// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <arm_neon.h>

#include "kleidicv/neon.h"
#include "kleidicv/types.h"
#include "transform_common.h"

namespace kleidicv::neon {

typedef struct {
  float32x4_t x, y;
} FloatVectorPair;

template <typename ScalarType, bool IsLarge>
float32x4_t inline load_xy(uint32x4_t x, uint32x4_t y, uint32x4_t v_src_stride,
                           Rows<const ScalarType>& src_rows) {
  if constexpr (IsLarge) {
    uint64x2_t offset_low =
        vmlal_u32(vmovl_u32(vget_low_u32(x)), vget_low_u32(y),
                  vget_low_u32(v_src_stride));
    uint64x2_t offset_high = vmlal_u32(vmovl_high_u32(x), vget_high_u32(y),
                                       vget_low_u32(v_src_stride));
    uint64_t acc =
        static_cast<uint64_t>(src_rows[vgetq_lane_u64(offset_low, 0)]) |
        (static_cast<uint64_t>(src_rows[vgetq_lane_u64(offset_low, 1)]) << 32);
    uint64x2_t rawsrc = vdupq_n_u64(acc);
    acc =
        static_cast<uint64_t>(src_rows[vgetq_lane_u64(offset_high, 0)]) |
        (static_cast<uint64_t>(src_rows[vgetq_lane_u64(offset_high, 1)]) << 32);
    rawsrc = vsetq_lane_u64(acc, rawsrc, 1);
    return vcvtq_f32_u32(vreinterpretq_u32_u64(rawsrc));
  } else {
    uint32x4_t offset = vmlaq_u32(x, y, v_src_stride);
    uint64_t acc =
        static_cast<uint64_t>(src_rows[vgetq_lane_u32(offset, 0)]) |
        (static_cast<uint64_t>(src_rows[vgetq_lane_u32(offset, 1)]) << 32);
    uint64x2_t rawsrc = vdupq_n_u64(acc);
    acc = static_cast<uint64_t>(src_rows[vgetq_lane_u32(offset, 2)]) |
          (static_cast<uint64_t>(src_rows[vgetq_lane_u32(offset, 3)]) << 32);
    rawsrc = vsetq_lane_u64(acc, rawsrc, 1);
    return vcvtq_f32_u32(vreinterpretq_u32_u64(rawsrc));
  }
}

template <typename ScalarType, bool IsLarge>
float32x4_t inline load_xy_or_border(uint32x4_t x, uint32x4_t y,
                                     uint32x4_t in_range,
                                     ScalarType border_value,
                                     uint32x4_t v_src_stride,
                                     Rows<const ScalarType> src_rows) {
  if constexpr (IsLarge) {
    uint64x2_t offset_low =
        vmlal_u32(vmovl_u32(vget_low_u32(x)), vget_low_u32(y),
                  vget_low_u32(v_src_stride));
    uint64x2_t offset_high = vmlal_u32(vmovl_high_u32(x), vget_high_u32(y),
                                       vget_low_u32(v_src_stride));
    uint64_t pixel0 = vgetq_lane_u32(in_range, 0)
                          ? src_rows[vgetq_lane_u64(offset_low, 0)]
                          : border_value;
    uint64_t pixel1 = vgetq_lane_u32(in_range, 1)
                          ? src_rows[vgetq_lane_u64(offset_low, 1)]
                          : border_value;
    uint64_t pixel2 = vgetq_lane_u32(in_range, 2)
                          ? src_rows[vgetq_lane_u64(offset_high, 0)]
                          : border_value;
    uint64_t pixel3 = vgetq_lane_u32(in_range, 3)
                          ? src_rows[vgetq_lane_u64(offset_high, 1)]
                          : border_value;
    uint64x2_t rawsrc = vdupq_n_u64(pixel0 | (pixel1 << 32));
    rawsrc = vdupq_n_u64(pixel0 | (pixel1 << 32));
    rawsrc = vsetq_lane_u64(pixel2 | (pixel3 << 32), rawsrc, 1);
    return vcvtq_f32_u32(vreinterpretq_u32_u64(rawsrc));
  } else {
    uint32x4_t offset = vmlaq_u32(x, y, v_src_stride);
    uint64_t pixel0 = vgetq_lane_u32(in_range, 0)
                          ? src_rows[vgetq_lane_u32(offset, 0)]
                          : border_value;
    uint64_t pixel1 = vgetq_lane_u32(in_range, 1)
                          ? src_rows[vgetq_lane_u32(offset, 1)]
                          : border_value;
    uint64_t pixel2 = vgetq_lane_u32(in_range, 2)
                          ? src_rows[vgetq_lane_u32(offset, 2)]
                          : border_value;
    uint64_t pixel3 = vgetq_lane_u32(in_range, 3)
                          ? src_rows[vgetq_lane_u32(offset, 3)]
                          : border_value;
    uint64x2_t rawsrc = vdupq_n_u64(pixel0 | (pixel1 << 32));
    rawsrc = vdupq_n_u64(pixel0 | (pixel1 << 32));
    rawsrc = vsetq_lane_u64(pixel2 | (pixel3 << 32), rawsrc, 1);
    return vcvtq_f32_u32(vreinterpretq_u32_u64(rawsrc));
  }
}

template <typename ScalarType, bool IsLarge>
void load_quad_pixels_replicate(FloatVectorPair xy, uint32x4_t v_xmax,
                                uint32x4_t v_ymax, uint32x4_t v_src_stride,
                                Rows<const ScalarType> src_rows,
                                float32x4_t& xfrac, float32x4_t& yfrac,
                                float32x4_t& a, float32x4_t& b, float32x4_t& c,
                                float32x4_t& d) {
  auto&& [xf, yf] = xy;
  // Truncating convert to int
  uint32x4_t x0 = vminq_u32(vcvtmq_u32_f32(xf), v_xmax);
  uint32x4_t y0 = vminq_u32(vcvtmq_u32_f32(yf), v_ymax);

  // Get fractional part, or 0 if out of range
  float32x4_t zero = vdupq_n_f32(0.F);
  uint32x4_t x_in_range = vandq_u32(vcgeq_f32(xf, zero), vcltq_u32(x0, v_xmax));
  uint32x4_t y_in_range = vandq_u32(vcgeq_f32(yf, zero), vcltq_u32(y0, v_ymax));
  xfrac = vsubq_f32(xf, vrndmq_f32(xf));
  yfrac = vsubq_f32(yf, vrndmq_f32(yf));

  // x1 = x0 + 1, except if it's already xmax or out of range
  uint32x4_t x1 = vsubq_u32(x0, x_in_range);
  uint32x4_t y1 = vsubq_u32(y0, y_in_range);

  // a: top left, b: top right, c: bottom left, d: bottom right
  a = load_xy<ScalarType, IsLarge>(x0, y0, v_src_stride, src_rows);
  b = load_xy<ScalarType, IsLarge>(x1, y0, v_src_stride, src_rows);
  c = load_xy<ScalarType, IsLarge>(x0, y1, v_src_stride, src_rows);
  d = load_xy<ScalarType, IsLarge>(x1, y1, v_src_stride, src_rows);
}

template <typename ScalarType, bool IsLarge>
void load_quad_pixels_constant(FloatVectorPair xy, uint32x4_t v_xmax,
                               uint32x4_t v_ymax, uint32x4_t v_src_stride,
                               ScalarType border_value,
                               Rows<const ScalarType> src_rows,
                               float32x4_t& xfrac, float32x4_t& yfrac,
                               float32x4_t& a, float32x4_t& b, float32x4_t& c,
                               float32x4_t& d) {
  auto&& [xf, yf] = xy;
  // Convert coordinates to integers, truncating towards minus infinity.
  // Negative numbers will become large positive numbers.
  // Since the source width and height is known to be <=2^24 these large
  // positive numbers will always be treated as outside the source image
  // bounds.
  uint32x4_t x0 = vreinterpretq_u32_s32(vcvtmq_s32_f32(xf));
  uint32x4_t y0 = vreinterpretq_u32_s32(vcvtmq_s32_f32(yf));
  uint32x4_t x1 = vaddq(x0, vdupq_n_u32(1));
  uint32x4_t y1 = vaddq(y0, vdupq_n_u32(1));
  xfrac = vsubq_f32(xf, vrndmq_f32(xf));
  yfrac = vsubq_f32(yf, vrndmq_f32(yf));
  uint32x4_t a_in_range, b_in_range, c_in_range, d_in_range;
  {
    uint32x4_t x0_in_range = vcleq_u32(x0, v_xmax);
    uint32x4_t y0_in_range = vcleq_u32(y0, v_ymax);
    uint32x4_t x1_in_range = vcleq_u32(x1, v_xmax);
    uint32x4_t y1_in_range = vcleq_u32(y1, v_ymax);
    a_in_range = vandq(x0_in_range, y0_in_range);
    b_in_range = vandq(x1_in_range, y0_in_range);
    c_in_range = vandq(x0_in_range, y1_in_range);
    d_in_range = vandq(x1_in_range, y1_in_range);
  }
  a = load_xy_or_border<ScalarType, IsLarge>(x0, y0, a_in_range, border_value,
                                             v_src_stride, src_rows);
  b = load_xy_or_border<ScalarType, IsLarge>(x1, y0, b_in_range, border_value,
                                             v_src_stride, src_rows);
  c = load_xy_or_border<ScalarType, IsLarge>(x0, y1, c_in_range, border_value,
                                             v_src_stride, src_rows);
  d = load_xy_or_border<ScalarType, IsLarge>(x1, y1, d_in_range, border_value,
                                             v_src_stride, src_rows);
}

inline uint32x4_t lerp_2d(float32x4_t xfrac, float32x4_t yfrac, float32x4_t a,
                          float32x4_t b, float32x4_t c, float32x4_t d) {
  float32x4_t line0 = vmlaq_f32(a, vsubq_f32(b, a), xfrac);
  float32x4_t line1 = vmlaq_f32(c, vsubq_f32(d, c), xfrac);
  float32x4_t result = vmlaq_f32(line0, vsubq_f32(line1, line0), yfrac);
  return vcvtaq_u32_f32(result);
}

template <typename ScalarType, bool IsLarge>
void transform_pixels_replicate(float32x4_t xf, float32x4_t yf,
                                uint32x4_t v_xmax, uint32x4_t v_ymax,
                                uint32x4_t v_src_stride,
                                Rows<const ScalarType> src_rows,
                                Columns<ScalarType> dst) {
  // Round to nearest, with Ties To Away (i.e. round 0.5 up)
  // Clamp coordinates to within the dimensions of the source image
  // (vcvtaq already converted negative values to 0)
  uint32x4_t x = vminq_u32(vcvtaq_u32_f32(xf), v_xmax);
  uint32x4_t y = vminq_u32(vcvtaq_u32_f32(yf), v_ymax);

  // Copy pixels from source
  if constexpr (IsLarge) {
    uint64x2_t indices_low =
        vmlal_u32(vmovl_u32(vget_low_u32(x)), vget_low_u32(y),
                  vget_low_u32(v_src_stride));
    uint64x2_t indices_high = vmlal_u32(vmovl_high_u32(x), vget_high_u32(y),
                                        vget_low_u32(v_src_stride));
    dst[0] = src_rows[vgetq_lane_u64(indices_low, 0)];
    dst[1] = src_rows[vgetq_lane_u64(indices_low, 1)];
    dst[2] = src_rows[vgetq_lane_u64(indices_high, 0)];
    dst[3] = src_rows[vgetq_lane_u64(indices_high, 1)];
  } else {
    uint32x4_t indices = vmlaq_u32(x, y, v_src_stride);
    dst[0] = src_rows[vgetq_lane_u32(indices, 0)];
    dst[1] = src_rows[vgetq_lane_u32(indices, 1)];
    dst[2] = src_rows[vgetq_lane_u32(indices, 2)];
    dst[3] = src_rows[vgetq_lane_u32(indices, 3)];
  }
}

template <typename ScalarType, bool IsLarge>
void transform_pixels_constant(float32x4_t xf, float32x4_t yf,
                               uint32x4_t v_xmax, uint32x4_t v_ymax,
                               uint32x4_t v_src_stride,
                               Rows<const ScalarType> src_rows,
                               Columns<ScalarType> dst,
                               ScalarType border_value) {
  // Convert coordinates to integers.
  // Negative numbers will become large positive numbers.
  // Since the source width and height is known to be <=2^24 these large
  // positive numbers will always be treated as outside the source image
  // bounds.
  uint32x4_t x = vreinterpretq_u32_s32(vcvtaq_s32_f32(xf));
  uint32x4_t y = vreinterpretq_u32_s32(vcvtaq_s32_f32(yf));
  uint32x4_t in_range = vandq_u32(vcleq_u32(x, v_xmax), vcleq_u32(y, v_ymax));

  // Copy pixels from source
  if constexpr (IsLarge) {
    uint64x2_t indices_low =
        vmlal_u32(vmovl_u32(vget_low_u32(x)), vget_low_u32(y),
                  vget_low_u32(v_src_stride));
    uint64x2_t indices_high = vmlal_u32(vmovl_high_u32(x), vget_high_u32(y),
                                        vget_low_u32(v_src_stride));
    dst[0] = vgetq_lane_u32(in_range, 0)
                 ? src_rows[vgetq_lane_u64(indices_low, 0)]
                 : border_value;
    dst[1] = vgetq_lane_u32(in_range, 1)
                 ? src_rows[vgetq_lane_u64(indices_low, 1)]
                 : border_value;
    dst[2] = vgetq_lane_u32(in_range, 2)
                 ? src_rows[vgetq_lane_u64(indices_high, 0)]
                 : border_value;
    dst[3] = vgetq_lane_u32(in_range, 3)
                 ? src_rows[vgetq_lane_u64(indices_high, 1)]
                 : border_value;
  } else {
    uint32x4_t indices = vmlaq_u32(x, y, v_src_stride);
    dst[0] = vgetq_lane_u32(in_range, 0) ? src_rows[vgetq_lane_u32(indices, 0)]
                                         : border_value;
    dst[1] = vgetq_lane_u32(in_range, 1) ? src_rows[vgetq_lane_u32(indices, 1)]
                                         : border_value;
    dst[2] = vgetq_lane_u32(in_range, 2) ? src_rows[vgetq_lane_u32(indices, 2)]
                                         : border_value;
    dst[3] = vgetq_lane_u32(in_range, 3) ? src_rows[vgetq_lane_u32(indices, 3)]
                                         : border_value;
  }
}

}  // namespace kleidicv::neon
