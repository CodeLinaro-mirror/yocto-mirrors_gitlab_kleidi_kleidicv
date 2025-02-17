// SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <type_traits>

#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"
#include "kleidicv/transform/remap.h"

namespace kleidicv::neon {

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

template <bool IsLarge>
class RemapF32Replicate<uint16_t, IsLarge> {
 public:
  using ScalarType = uint16_t;
  using MapVecTraits = neon::VecTraits<float>;
  using MapVectorType = typename MapVecTraits::VectorType;  // float32x4_t

  RemapF32Replicate(Rows<const ScalarType> src_rows, size_t src_width,
                    size_t src_height)
      : src_rows_{src_rows},
        v_src_element_stride_{vdup_n_u32(
            static_cast<uint32_t>(src_rows_.stride() / sizeof(ScalarType)))},
        vq_src_element_stride_{vdupq_n_u32(
            static_cast<uint32_t>(src_rows_.stride() / sizeof(ScalarType)))},
        v_xmax_{vdupq_n_u32(static_cast<uint32_t>(src_width - 1))},
        v_ymax_{vdupq_n_u32(static_cast<uint32_t>(src_height - 1))} {}

  void process_row(size_t width, Columns<const float> mapx,
                   Columns<const float> mapy, Columns<ScalarType> dst) {
    const size_t kStep = VecTraits<float>::num_lanes();

    auto load_src_into_floats_small = [&](uint32x4_t x, uint32x4_t y) {
      uint32x4_t offset = vmlaq_u32(x, y, vq_src_element_stride_);
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
      uint64x2_t offset_low = vmlal_u32(vmovl_u32(vget_low_u32(x)),
                                        vget_low_u32(y), v_src_element_stride_);
      uint64x2_t offset_high =
          vmlal_u32(vmovl_high_u32(x), vget_high_u32(y), v_src_element_stride_);
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
      return vminq_u32(vdupq_n_u32(0xFFFF), vcvtaq_u32_f32(result));
    };

    auto vector_path_2 = [&](size_t step) {  // step = 2*4 = 8
      const float *ptr_mapx = &mapx[0];
      const float *ptr_mapy = &mapy[0];
      uint32x4_t res0 = vector_path_1(ptr_mapx, ptr_mapy);

      ptr_mapx += kStep;
      ptr_mapy += kStep;
      uint32x4_t res1 = vector_path_1(ptr_mapx, ptr_mapy);
      uint16x8_t result16 = vuzp1q_u16(res0, res1);

      vst1q_u16(&dst[0], result16);
      mapx += ptrdiff_t(step);
      mapy += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };

    LoopUnroll loop{width, kStep};
    loop.unroll_twice(vector_path_2);
    loop.unroll_once([&](size_t step) {
      const float *ptr_mapx = &mapx[0];
      const float *ptr_mapy = &mapy[0];
      uint32x4_t result = vector_path_1(ptr_mapx, ptr_mapy);
      uint16x4_t result16 = vget_low_u16(vuzp1q_u16(result, result));
      vst1_u16(&dst[0], result16);
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
      uint16x4_t result16 = vget_low_u16(vuzp1q_u16(result, result));
      vst1_u16(&dst[0], result16);
    });
  }

 private:
  Rows<const ScalarType> src_rows_;
  uint32x2_t v_src_element_stride_;   // load_large
  uint32x4_t vq_src_element_stride_;  // load_small
  uint32x4_t v_xmax_;
  uint32x4_t v_ymax_;
};  // end of class RemapF32Replicate<uint16_t>

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

// TODO: Need to refactor to reduce the complexity
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <bool IsLarge>
class RemapF32ConstantBorder<uint16_t, IsLarge> {
 public:
  using ScalarType = uint16_t;
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

    auto get_edge_pixels = [&](unsigned &a_result, unsigned &b_result,
                               unsigned &c_result, unsigned &d_result, int x0,
                               int y0, ptrdiff_t index,
                               Rows<const ScalarType> src_rows, int src_width,
                               int src_height) {
      if (y0 >= 0) {
        if (x0 >= 0) {
          a_result = src_rows[index];
        }
        if (x0 + 1 < src_width) {
          b_result = src_rows[index + 1];
        }
      }
      if (y0 + 1 < src_height) {
        index += static_cast<ptrdiff_t>(src_rows.stride() / sizeof(ScalarType));
        if (x0 >= 0) {
          c_result = src_rows[index];
        }
        if (x0 + 1 < src_width) {
          d_result = src_rows[index + 1];
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
        ptrdiff_t index =
            x0i + y0i * static_cast<ptrdiff_t>(src_rows_.stride() /
                                               sizeof(ScalarType));
        //        std::cout << "x0i " << x0i << "   y0i " << y0i << "   index: "
        //        << index
        // xw                  << "\n";
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
                          y0i, index, src_rows_, src_width_, src_height_);
          continue;
        }

        // Completely inside the source image
        a_array[i] = src_rows_[index];
        b_array[i] = src_rows_[index + 1];
        index += src_rows_.stride() / sizeof(ScalarType);
        c_array[i] = src_rows_[index];
        d_array[i] = src_rows_[index + 1];
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

    auto vector_path_2 = [&](size_t step) {  // step = 2*4 = 8
      const float *ptr_mapx = &mapx[0];
      const float *ptr_mapy = &mapy[0];
      uint32x4_t res0 = vector_path_1(ptr_mapx, ptr_mapy);

      ptr_mapx += kStep;
      ptr_mapy += kStep;
      uint32x4_t res1 = vector_path_1(ptr_mapx, ptr_mapy);
      uint16x8_t result16 = vuzp1q_u16(res0, res1);

      vst1q_u16(&dst[0], result16);
      mapx += ptrdiff_t(step);
      mapy += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };

    LoopUnroll loop{width, kStep};
    loop.unroll_twice(vector_path_2);
    loop.unroll_once([&](size_t step) {
      const float *ptr_mapx = &mapx[0];
      const float *ptr_mapy = &mapy[0];
      uint32x4_t result = vector_path_1(ptr_mapx, ptr_mapy);
      uint16x4_t result16 = vget_low_u16(vuzp1q_u16(result, result));
      vst1_u16(&dst[0], result16);
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
      uint16x4_t result16 = vget_low_u16(vuzp1q_u16(result, result));
      vst1_u16(&dst[0], result16);
    });
  }

 private:
  Rows<const ScalarType> src_rows_;
  size_t src_width_;
  size_t src_height_;
  const ScalarType *border_value_;
};  // end of class RemapF32ConstantBorder<uint16_t>
// NOLINTEND(readability-function-cognitive-complexity)

template <typename ScalarType, bool IsLarge>
class RemapF32NearestReplicate;

template <bool IsLarge>
class RemapF32NearestReplicate<uint8_t, IsLarge> {
 public:
  using ScalarType = uint8_t;
  using MapVecTraits = neon::VecTraits<float>;
  using MapVectorType = typename MapVecTraits::VectorType;  // float32x4_t

  RemapF32NearestReplicate(Rows<const ScalarType> src_rows, size_t src_width,
                           size_t src_height)
      : src_rows_{src_rows},
        v_src_stride_{vdupq_n_u32(static_cast<uint32_t>(src_rows_.stride()))},
        v_xmax_{vdupq_n_u32(static_cast<uint32_t>(src_width - 1))},
        v_ymax_{vdupq_n_u32(static_cast<uint32_t>(src_height - 1))} {}

  void get_map_coordinates(Columns<const float> mapx, Columns<const float> mapy,
                           uint32x4_t &x, uint32x4_t &y) {
    MapVectorType x_raw = vld1q_f32(&mapx[0]);
    MapVectorType y_raw = vld1q_f32(&mapy[0]);

    MapVectorType bias = vdupq_n_f32(0.5F);
    // Round to nearest positive value
    uint32x4_t x_nearest = vcvtmq_u32_f32(vaddq_f32(x_raw, bias));
    uint32x4_t y_nearest = vcvtmq_u32_f32(vaddq_f32(y_raw, bias));

    // Clamp coordinates to within the dimensions of the source image
    x = vmaxq_u32(vdupq_n_u32(0), vminq_u32(x_nearest, v_xmax_));
    y = vmaxq_u32(vdupq_n_u32(0), vminq_u32(y_nearest, v_ymax_));
  }

  uint8x8_t load_pixels_large(uint32x4_t x, uint32x4_t y) {
    // Calculate offsets from coordinates (y * stride + x)
    uint64x2_t indices_low =
        vmlal_u32(vmovl_u32(vget_low_u32(x)), vget_low_u32(y),
                  vget_low_u32(v_src_stride_));
    uint64x2_t indices_high =
        vmlal_high_u32(vmovl_high_u32(x), y, v_src_stride_);

    uint8x8_t pixels = {src_rows_[vgetq_lane_u64(indices_low, 0)],
                        src_rows_[vgetq_lane_u64(indices_low, 1)],
                        src_rows_[vgetq_lane_u64(indices_high, 0)],
                        src_rows_[vgetq_lane_u64(indices_high, 1)],
                        0,
                        0,
                        0,
                        0};
    return pixels;
  }

  uint8x8_t load_pixels_small(uint32x4_t x, uint32x4_t y) {
    // Calculate offsets from coordinates (y * stride + x)
    uint32x4_t indices = vmlaq_u32(x, y, v_src_stride_);

    uint8x8_t pixels = {src_rows_[vgetq_lane_u32(indices, 0)],
                        src_rows_[vgetq_lane_u32(indices, 1)],
                        src_rows_[vgetq_lane_u32(indices, 2)],
                        src_rows_[vgetq_lane_u32(indices, 3)],
                        0,
                        0,
                        0,
                        0};
    return pixels;
  }

  void store_pixels(uint8x8_t pixels, Columns<ScalarType> dst) {
    dst[0] = vget_lane_u8(pixels, 0);
    dst[1] = vget_lane_u8(pixels, 1);
    dst[2] = vget_lane_u8(pixels, 2);
    dst[3] = vget_lane_u8(pixels, 3);
  }

  void process_row(size_t width, Columns<const float> mapx,
                   Columns<const float> mapy, Columns<ScalarType> dst) {
    const size_t kStep = VecTraits<float>::num_lanes();

    auto vector_path = [&](size_t step) {
      uint32x4_t x, y;
      get_map_coordinates(mapx, mapy, x, y);

      uint8x8_t pixels;
      if constexpr (IsLarge) {
        pixels = load_pixels_large(x, y);
      } else {
        pixels = load_pixels_small(x, y);
      }

      store_pixels(pixels, dst);

      mapx += ptrdiff_t(step);
      mapy += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };

    LoopUnroll loop{width, kStep};
    loop.unroll_once(vector_path);
    ptrdiff_t back_step = static_cast<ptrdiff_t>(loop.step()) -
                          static_cast<ptrdiff_t>(loop.remaining_length());
    mapx -= back_step;
    mapy -= back_step;
    dst -= back_step;
    loop.remaining([&](size_t, size_t step) { vector_path(step); });
  }

 private:
  Rows<const ScalarType> src_rows_;
  uint32x4_t v_src_stride_;
  uint32x4_t v_xmax_;
  uint32x4_t v_ymax_;
};  // end of class RemapF32NearestReplicate<uint8_t>

template <bool IsLarge>
class RemapF32NearestReplicate<uint16_t, IsLarge> {
 public:
  using ScalarType = uint16_t;
  using MapVecTraits = neon::VecTraits<float>;
  using MapVectorType = typename MapVecTraits::VectorType;  // float32x4_t

  RemapF32NearestReplicate(Rows<const ScalarType> src_rows, size_t src_width,
                           size_t src_height)
      : src_rows_{src_rows},
        v_src_element_stride_{vdupq_n_u32(
            static_cast<uint32_t>(src_rows_.stride() / sizeof(ScalarType)))},
        v_xmax_{vdupq_n_u32(static_cast<uint32_t>(src_width - 1))},
        v_ymax_{vdupq_n_u32(static_cast<uint32_t>(src_height - 1))} {}

  void get_map_coordinates(Columns<const float> mapx, Columns<const float> mapy,
                           uint32x4_t &x, uint32x4_t &y) {
    MapVectorType x_raw = vld1q_f32(&mapx[0]);
    MapVectorType y_raw = vld1q_f32(&mapy[0]);

    MapVectorType bias = vdupq_n_f32(0.5F);
    // Round to nearest positive value
    uint32x4_t x_nearest = vcvtmq_u32_f32(vaddq_f32(x_raw, bias));
    uint32x4_t y_nearest = vcvtmq_u32_f32(vaddq_f32(y_raw, bias));

    // Clamp coordinates to within the dimensions of the source image
    x = vmaxq_u32(vdupq_n_u32(0), vminq_u32(x_nearest, v_xmax_));
    y = vmaxq_u32(vdupq_n_u32(0), vminq_u32(y_nearest, v_ymax_));
  }

  uint16x4_t load_pixels_large(uint32x4_t x, uint32x4_t y) {
    // Calculate offsets from coordinates (y * element_stride + x)
    uint64x2_t indices_low =
        vmlal_u32(vmovl_u32(vget_low_u32(x)), vget_low_u32(y),
                  vget_low_u32(v_src_element_stride_));
    uint64x2_t indices_high =
        vmlal_high_u32(vmovl_high_u32(x), y, v_src_element_stride_);

    uint16x4_t pixels = {src_rows_[vgetq_lane_u64(indices_low, 0)],
                         src_rows_[vgetq_lane_u64(indices_low, 1)],
                         src_rows_[vgetq_lane_u64(indices_high, 0)],
                         src_rows_[vgetq_lane_u64(indices_high, 1)]};
    return pixels;
  }

  uint16x4_t load_pixels_small(uint32x4_t x, uint32x4_t y) {
    // Calculate offsets from coordinates (y * element_stride + x)
    uint32x4_t indices = vmlaq_u32(x, y, v_src_element_stride_);

    uint16x4_t pixels = {src_rows_[vgetq_lane_u32(indices, 0)],
                         src_rows_[vgetq_lane_u32(indices, 1)],
                         src_rows_[vgetq_lane_u32(indices, 2)],
                         src_rows_[vgetq_lane_u32(indices, 3)]};
    return pixels;
  }

  void store_pixels(uint16x4_t pixels, Columns<ScalarType> dst) {
    vst1_u16(&dst[0], pixels);
  }

  void process_row(size_t width, Columns<const float> mapx,
                   Columns<const float> mapy, Columns<ScalarType> dst) {
    const size_t kStep = VecTraits<float>::num_lanes();

    auto vector_path = [&](size_t step) {
      uint32x4_t x, y;
      get_map_coordinates(mapx, mapy, x, y);

      uint16x4_t pixels;
      if constexpr (IsLarge) {
        pixels = load_pixels_large(x, y);
      } else {
        pixels = load_pixels_small(x, y);
      }

      store_pixels(pixels, dst);

      mapx += ptrdiff_t(step);
      mapy += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };

    LoopUnroll loop{width, kStep};
    loop.unroll_once(vector_path);
    ptrdiff_t back_step = static_cast<ptrdiff_t>(loop.step()) -
                          static_cast<ptrdiff_t>(loop.remaining_length());
    mapx -= back_step;
    mapy -= back_step;
    dst -= back_step;
    loop.remaining([&](size_t, size_t step) { vector_path(step); });
  }

 private:
  Rows<const ScalarType> src_rows_;
  uint32x4_t v_src_element_stride_;
  uint32x4_t v_xmax_;
  uint32x4_t v_ymax_;
};  // end of class RemapF32NearestReplicate<uint16_t>

template <typename ScalarType, bool IsLarge>
class RemapF32NearestConstant;

template <bool IsLarge>
class RemapF32NearestConstant<uint8_t, IsLarge> {
 public:
  using ScalarType = uint8_t;
  using MapVecTraits = neon::VecTraits<float>;
  using MapVectorType = typename MapVecTraits::VectorType;  // float32x4_t

  RemapF32NearestConstant(Rows<const ScalarType> src_rows, size_t src_width,
                          size_t src_height, const ScalarType *border_value)
      : src_rows_{src_rows},
        v_src_stride_{vdupq_n_u32(static_cast<uint32_t>(src_rows_.stride()))},
        v_width_{vdupq_n_u32(static_cast<uint32_t>(src_width))},
        v_height_{vdupq_n_u32(static_cast<uint32_t>(src_height))},
        v_border_{vdup_n_u8(*border_value)} {}

  void get_map_coordinates(Columns<const float> mapx, Columns<const float> mapy,
                           uint32x4_t &x, uint32x4_t &y, uint32x4_t &in_range) {
    MapVectorType x_raw = vld1q_f32(&mapx[0]);
    MapVectorType y_raw = vld1q_f32(&mapy[0]);

    MapVectorType bias = vdupq_n_f32(0.5F);
    float32x4_t x_biased = vaddq_f32(x_raw, bias);
    float32x4_t y_biased = vaddq_f32(y_raw, bias);

    // Round to nearest positive value
    uint32x4_t x_nearest = vcvtmq_u32_f32(x_biased);
    uint32x4_t y_nearest = vcvtmq_u32_f32(y_biased);

    // Find whether coordinates are within the image dimensions.
    uint32x4_t above_zero =
        vandq_u32(vcgezq_f32(x_biased), vcgezq_f32(y_biased));
    uint32x4_t below_limits = vandq_u32(vcltq_u32(x_nearest, v_width_),
                                        vcltq_u32(y_nearest, v_height_));
    in_range = vandq_u32(above_zero, below_limits);

    // Zero out-of-range coordinates.
    x = vandq_u32(in_range, x_nearest);
    y = vandq_u32(in_range, y_nearest);
  }

  uint8x8_t load_pixels_large(uint32x4_t x, uint32x4_t y) {
    // Calculate offsets from coordinates (y * stride + x)
    uint64x2_t indices_low =
        vmlal_u32(vmovl_u32(vget_low_u32(x)), vget_low_u32(y),
                  vget_low_u32(v_src_stride_));
    uint64x2_t indices_high =
        vmlal_high_u32(vmovl_high_u32(x), y, v_src_stride_);

    uint8x8_t pixels = {src_rows_[vgetq_lane_u64(indices_low, 0)],
                        src_rows_[vgetq_lane_u64(indices_low, 1)],
                        src_rows_[vgetq_lane_u64(indices_high, 0)],
                        src_rows_[vgetq_lane_u64(indices_high, 1)],
                        0,
                        0,
                        0,
                        0};
    return pixels;
  }

  uint8x8_t load_pixels_small(uint32x4_t x, uint32x4_t y) {
    // Calculate offsets from coordinates (y * stride + x)
    uint32x4_t indices = vmlaq_u32(x, y, v_src_stride_);

    uint8x8_t pixels = {src_rows_[vgetq_lane_u32(indices, 0)],
                        src_rows_[vgetq_lane_u32(indices, 1)],
                        src_rows_[vgetq_lane_u32(indices, 2)],
                        src_rows_[vgetq_lane_u32(indices, 3)],
                        0,
                        0,
                        0,
                        0};
    return pixels;
  }

  void store_pixels(uint8x8_t pixels, Columns<ScalarType> dst) {
    dst[0] = vget_lane_u8(pixels, 0);
    dst[1] = vget_lane_u8(pixels, 1);
    dst[2] = vget_lane_u8(pixels, 2);
    dst[3] = vget_lane_u8(pixels, 3);
  }

  void process_row(size_t width, Columns<const float> mapx,
                   Columns<const float> mapy, Columns<ScalarType> dst) {
    const size_t kStep = VecTraits<float>::num_lanes();

    auto vector_path = [&](size_t step) {
      uint32x4_t x, y;
      uint32x4_t in_range;
      get_map_coordinates(mapx, mapy, x, y, in_range);

      uint8x8_t pixels;
      if constexpr (IsLarge) {
        pixels = load_pixels_large(x, y);
      } else {
        pixels = load_pixels_small(x, y);
      }

      // Select between source pixels and border colour
      uint8x8_t in_range_narrowed =
          vmovn_u16(vcombine_u16(vmovn_u32(in_range), vdup_n_u16(0)));
      uint8x8_t pixels_or_border =
          vbsl_u8(in_range_narrowed, pixels, v_border_);

      store_pixels(pixels_or_border, dst);

      mapx += ptrdiff_t(step);
      mapy += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };

    LoopUnroll loop{width, kStep};
    loop.unroll_once(vector_path);
    ptrdiff_t back_step = static_cast<ptrdiff_t>(loop.step()) -
                          static_cast<ptrdiff_t>(loop.remaining_length());
    mapx -= back_step;
    mapy -= back_step;
    dst -= back_step;
    loop.remaining([&](size_t, size_t step) { vector_path(step); });
  }

 private:
  Rows<const ScalarType> src_rows_;
  uint32x4_t v_src_stride_;
  uint32x4_t v_width_;
  uint32x4_t v_height_;
  uint8x8_t v_border_;
};  // end of class RemapF32NearestConstant<uint8_t>

template <bool IsLarge>
class RemapF32NearestConstant<uint16_t, IsLarge> {
 public:
  using ScalarType = uint16_t;
  using MapVecTraits = neon::VecTraits<float>;
  using MapVectorType = typename MapVecTraits::VectorType;  // float32x4_t

  RemapF32NearestConstant(Rows<const ScalarType> src_rows, size_t src_width,
                          size_t src_height, const ScalarType *border_value)
      : src_rows_{src_rows},
        v_src_element_stride_{vdupq_n_u32(
            static_cast<uint32_t>(src_rows_.stride() / sizeof(ScalarType)))},
        v_width_{vdupq_n_u32(static_cast<uint32_t>(src_width))},
        v_height_{vdupq_n_u32(static_cast<uint32_t>(src_height))},
        v_border_{vdup_n_u16(*border_value)} {}

  void get_map_coordinates(Columns<const float> mapx, Columns<const float> mapy,
                           uint32x4_t &x, uint32x4_t &y, uint32x4_t &in_range) {
    MapVectorType x_raw = vld1q_f32(&mapx[0]);
    MapVectorType y_raw = vld1q_f32(&mapy[0]);

    MapVectorType bias = vdupq_n_f32(0.5F);
    float32x4_t x_biased = vaddq_f32(x_raw, bias);
    float32x4_t y_biased = vaddq_f32(y_raw, bias);

    // Round to nearest positive value
    uint32x4_t x_nearest = vcvtmq_u32_f32(x_biased);
    uint32x4_t y_nearest = vcvtmq_u32_f32(y_biased);

    // Find whether coordinates are within the image dimensions.
    uint32x4_t above_zero =
        vandq_u32(vcgezq_f32(x_biased), vcgezq_f32(y_biased));
    uint32x4_t below_limits = vandq_u32(vcltq_u32(x_nearest, v_width_),
                                        vcltq_u32(y_nearest, v_height_));
    in_range = vandq_u32(above_zero, below_limits);

    // Zero out-of-range coordinates.
    x = vandq_u32(in_range, x_nearest);
    y = vandq_u32(in_range, y_nearest);
  }

  uint16x4_t load_pixels_large(uint32x4_t x, uint32x4_t y) {
    // Calculate offsets from coordinates (y * stride + x)
    uint64x2_t indices_low =
        vmlal_u32(vmovl_u32(vget_low_u32(x)), vget_low_u32(y),
                  vget_low_u32(v_src_element_stride_));
    uint64x2_t indices_high =
        vmlal_high_u32(vmovl_high_u32(x), y, v_src_element_stride_);

    uint16x4_t pixels = {src_rows_[vgetq_lane_u64(indices_low, 0)],
                         src_rows_[vgetq_lane_u64(indices_low, 1)],
                         src_rows_[vgetq_lane_u64(indices_high, 0)],
                         src_rows_[vgetq_lane_u64(indices_high, 1)]};
    return pixels;
  }

  uint16x4_t load_pixels_small(uint32x4_t x, uint32x4_t y) {
    // Calculate offsets from coordinates (y * stride + x)
    uint32x4_t indices = vmlaq_u32(x, y, v_src_element_stride_);

    uint16x4_t pixels = {src_rows_[vgetq_lane_u32(indices, 0)],
                         src_rows_[vgetq_lane_u32(indices, 1)],
                         src_rows_[vgetq_lane_u32(indices, 2)],
                         src_rows_[vgetq_lane_u32(indices, 3)]};
    return pixels;
  }

  void store_pixels(uint16x4_t pixels, Columns<ScalarType> dst) {
    vst1_u16(&dst[0], pixels);
  }

  void process_row(size_t width, Columns<const float> mapx,
                   Columns<const float> mapy, Columns<ScalarType> dst) {
    const size_t kStep = VecTraits<float>::num_lanes();

    auto vector_path = [&](size_t step) {
      uint32x4_t x, y;
      uint32x4_t in_range;
      get_map_coordinates(mapx, mapy, x, y, in_range);

      uint16x4_t pixels;
      if constexpr (IsLarge) {
        pixels = load_pixels_large(x, y);
      } else {
        pixels = load_pixels_small(x, y);
      }

      // Select between source pixels and border colour
      uint16x4_t in_range_narrowed = vmovn_u32(in_range);
      uint16x4_t pixels_or_border =
          vbsl_u16(in_range_narrowed, pixels, v_border_);

      store_pixels(pixels_or_border, dst);

      mapx += ptrdiff_t(step);
      mapy += ptrdiff_t(step);
      dst += ptrdiff_t(step);
    };

    LoopUnroll loop{width, kStep};
    loop.unroll_once(vector_path);
    ptrdiff_t back_step = static_cast<ptrdiff_t>(loop.step()) -
                          static_cast<ptrdiff_t>(loop.remaining_length());
    mapx -= back_step;
    mapy -= back_step;
    dst -= back_step;
    loop.remaining([&](size_t, size_t step) { vector_path(step); });
  }

 private:
  Rows<const ScalarType> src_rows_;
  uint32x4_t v_src_element_stride_;
  uint32x4_t v_width_;
  uint32x4_t v_height_;
  uint16x4_t v_border_;
};  // end of class RemapF32NearestConstant<uint16_t>

// Most of the complexity comes from parameter checking.
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <typename T>
kleidicv_error_t remap_f32(const T *src, size_t src_stride, size_t src_width,
                           size_t src_height, T *dst, size_t dst_stride,
                           size_t dst_width, size_t dst_height, size_t channels,
                           const float *mapx, size_t mapx_stride,
                           const float *mapy, size_t mapy_stride,
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

  if (interpolation == KLEIDICV_INTERPOLATION_LINEAR) {
    if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT) {
      if (KLEIDICV_UNLIKELY(src_rows.stride() * src_height >= (1ULL << 32))) {
        RemapF32ConstantBorder<T, true> operation{src_rows, src_width,
                                                  src_height, border_value};
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
  } else {
    assert(interpolation == KLEIDICV_INTERPOLATION_NEAREST);
    if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT) {
      if (KLEIDICV_UNLIKELY(src_rows.stride() * src_height >= (1ULL << 32))) {
        RemapF32NearestConstant<T, true> operation{src_rows, src_width,
                                                   src_height, border_value};
        zip_rows(operation, rect, mapx_rows, mapy_rows, dst_rows);
      } else {
        RemapF32NearestConstant<T, false> operation{src_rows, src_width,
                                                    src_height, border_value};
        zip_rows(operation, rect, mapx_rows, mapy_rows, dst_rows);
      }
    } else {
      assert(border_type == KLEIDICV_BORDER_TYPE_REPLICATE);
      if (KLEIDICV_UNLIKELY(src_rows.stride() * src_height >= (1ULL << 32))) {
        RemapF32NearestReplicate<T, true> operation{src_rows, src_width,
                                                    src_height};
        zip_rows(operation, rect, mapx_rows, mapy_rows, dst_rows);
      } else {
        RemapF32NearestReplicate<T, false> operation{src_rows, src_width,
                                                     src_height};
        zip_rows(operation, rect, mapx_rows, mapy_rows, dst_rows);
      }
    }
  }

  return KLEIDICV_OK;
}
// NOLINTEND(readability-function-cognitive-complexity)

#define KLEIDICV_INSTANTIATE_TEMPLATE_REMAP_F32(type)                          \
  template KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t remap_f32<type>(          \
      const type *src, size_t src_stride, size_t src_width, size_t src_height, \
      type *dst, size_t dst_stride, size_t dst_width, size_t dst_height,       \
      size_t channels, const float *mapx, size_t mapx_stride,                  \
      const float *mapy, size_t mapy_stride,                                   \
      kleidicv_interpolation_type_t interpolation,                             \
      kleidicv_border_type_t border_type, const type *border_value)

KLEIDICV_INSTANTIATE_TEMPLATE_REMAP_F32(uint8_t);
KLEIDICV_INSTANTIATE_TEMPLATE_REMAP_F32(uint16_t);

}  // namespace kleidicv::neon
