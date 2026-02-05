// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <cmath>
#include <cstddef>

#include "kleidicv/neon.h"
#include "kleidicv/resize/resize_linear.h"

namespace kleidicv::neon {

//------------------------------------------------------
/// Generic resize for ratios 1/3 to 1/1, u8
//------------------------------------------------------

// For the coordinate calculation, fixed-point format is used, for better
// performance. Fixed-point format:
// - lowest 16 bits are the fractional part, that is the kFixpBits constant
// - at interpolation, the high 8 bits are used from the fractional part
//   (this is a good compromise between accuracy and performance: because the
//   result is 8bits, the error only affects the least significant 1-2 bits, see
//   the accuracy calculation in kleidicv.h
// - to get the integer part, right shift by 16 bits, or zip/unzip/tbl etc. to
//   get the bytes needed
// - for better accuracy, rounding is needed everywhere, i.e. adding 0.5, which
//   is 1 << 15

// ratio: number of vectors to load and resize to 1 vector
// - supported combinations of (ratio, channel): (2, 1), (2, 2), (3, 1), (3, 2)
template <ptrdiff_t kRatio, ptrdiff_t kChannels>
class ResizeGenericU8Operation final {
 public:
  ResizeGenericU8Operation(const uint8_t *src, size_t src_stride,
                           size_t src_width, size_t src_height, size_t y_begin,
                           size_t y_end, uint8_t *dst, size_t dst_stride,
                           size_t dst_width, size_t dst_height)
      : src_rows_{src, src_stride, kChannels},
        dst_rows_{dst, dst_stride, kChannels},
        src_width_{src_width},
        src_height_{src_height},
        y_begin_{y_begin},
        y_end_{y_end},
        dst_width_{dst_width},
        dst_height_{dst_height},
        // Difference in source x coordinate, for one vector path
        sx_fixp_step_{rounding_div(
            (src_width_ * kStep / kChannels) << kFixpBits, dst_width_)},
        // Difference in source x coordinate, for the half vector path
        sx_fixp_half_step_{static_cast<uint64_t>(
            (((src_width_ * kHalfStep / kChannels) << kFixpBits) +
             dst_width_ / 2) /
            dst_width_)},
        vsidx_tbl_{2, 6, 10, 14, 18, 22, 26, 30},
        vsfrac_tbl_{1,  255, 5,  255, 9,  255, 13, 255,
                    17, 255, 21, 255, 25, 255, 29, 255},
        // These starting values are not aligned to center. The center alignment
        // must be added only once. When added to a center-aligned source_x
        // value, the result will be center-aligned.
        vsx0_0_{scale_x(0), scale_x(1 / kChannels), scale_x(2 / kChannels),
                scale_x(3 / kChannels)},
        vsx0_1_{scale_x(4 / kChannels), scale_x(5 / kChannels),
                scale_x(6 / kChannels), scale_x(7 / kChannels)},
        vsx0_2_{scale_x(8 / kChannels), scale_x(9 / kChannels),
                scale_x(10 / kChannels), scale_x(11 / kChannels)},
        vsx0_3_{scale_x(12 / kChannels), scale_x(13 / kChannels),
                scale_x(14 / kChannels), scale_x(15 / kChannels)},
        precalc_{nullptr, &std::free},
        num_of_vector_path_2x_{
            calculate_num_of_vector_path_2x(src_width, dst_width)} {}

  kleidicv_error_t process_rows() {
    if (num_of_vector_path_2x_ > 0) {
      kleidicv_error_t err = allocate_temp_buffers();
      if (err != KLEIDICV_OK) {
        return err;
      }
    }

    precalculate_indices_fractions_bases();

    for (uint64_t dst_y = y_begin_; dst_y < y_end_; ++dst_y) {
      process_row(dst_y);
    }

    return KLEIDICV_OK;
  }

 private:
  static constexpr ptrdiff_t kFixpBits = 16;
  static constexpr ptrdiff_t kFixpHalf = (1UL << (kFixpBits - 1));
  static constexpr ptrdiff_t kStep = kVectorLength / sizeof(uint8_t);
  static constexpr ptrdiff_t kHalfStep = kStep / 2;
  using FreeDeleter = decltype(&std::free);

  struct PrecalcData {
    uint8_t idx[kStep];
    uint16_t xfrac[kStep];
    ptrdiff_t sxbase;
  };

  static size_t calculate_num_of_vector_path_2x(size_t src_width,
                                                size_t dst_width) {
    if ((src_width * kChannels) >= (sizeof(uint8x16_t) * kRatio)) {
      return (dst_width * kChannels) / (2 * kStep);
    }
    return 0;
  }

  template <typename T = uint64_t>
  static T rounding_div(uint64_t nom, uint64_t denom) {
    return static_cast<T>((nom + denom / 2) / denom);
  }

  // Scale destination x coordinate to source x coordinate, into fixed-point,
  // without center correction
  uint32_t scale_x(uint64_t dx) const {
    return rounding_div<uint32_t>(((dx * src_width_) << kFixpBits), dst_width_);
  }

  // Scale coordinate using this formula, so the center is aligned:
  //   source_x = (destination_x + 0.5) / scale - 0.5;
  //   plus 1/256/2 for later rounding the fractional part to 8bits
  static uint64_t aligned_scale(uint64_t x, uint64_t nom, uint64_t denom) {
    return rounding_div(((x << kFixpBits) + kFixpHalf) * nom, denom) -
           kFixpHalf + (1 << (kFixpBits - 9));
  }

  uint64_t to_src_x(uint64_t dx) const {
    return aligned_scale(dx, src_width_, dst_width_);
  }

  uint64_t to_src_y(uint64_t dy) const {
    return aligned_scale(dy, src_height_, dst_height_);
  }

  kleidicv_error_t allocate_temp_buffers() {
    precalc_.reset(static_cast<PrecalcData *>(
        malloc(num_of_vector_path_2x_ * 2 * sizeof(PrecalcData))));
    if (!precalc_) {
      return KLEIDICV_ERROR_ALLOCATION;
    }
    return KLEIDICV_OK;
  }

  void calculate_indices_fractions_base(size_t precalc_index, ptrdiff_t sx_base,
                                        uint64_t sx_fixp) {
    uint32_t xfrac0 = static_cast<uint32_t>(sx_fixp - (sx_base << kFixpBits));
    uint32x4_t vfrac = vdupq_n_u32(xfrac0);
    // Calculate x coordinate delta from sx_base, the integer part of source x
    uint8x16x2_t vsx_delta_lo, vsx_delta_hi;
    vsx_delta_lo.val[0] = vreinterpretq_u8_u32(vaddq_u32(vsx0_0_, vfrac));
    vsx_delta_lo.val[1] = vreinterpretq_u8_u32(vaddq_u32(vsx0_1_, vfrac));
    vsx_delta_hi.val[0] = vreinterpretq_u8_u32(vaddq_u32(vsx0_2_, vfrac));
    vsx_delta_hi.val[1] = vreinterpretq_u8_u32(vaddq_u32(vsx0_3_, vfrac));
    uint8x8_t idx0 = vqtbl2_u8(vsx_delta_lo, vsidx_tbl_);
    uint8x8_t idx1 = vqtbl2_u8(vsx_delta_hi, vsidx_tbl_);
    uint8x16_t vsx0_idx = vcombine_u8(idx0, idx1);
    if constexpr (kChannels > 1) {
      vsx0_idx = vshlq_n_u8(vsx0_idx, kChannels == 4 ? 2 : 1);
      vsx0_idx =
          vaddq_u8(vsx0_idx, vreinterpretq_u8_u32(vdupq_n_u32(
                                 kChannels == 4 ? 0x03020100U : 0x01000100)));
    }
    PrecalcData &precalc1 = precalc_.get()[precalc_index];
    vst1q(precalc1.idx, vsx0_idx);
    uint16x8x2_t vsxfrac;
    vsxfrac.val[0] =
        vreinterpretq_u16_u8(vqtbl2q_u8(vsx_delta_lo, vsfrac_tbl_));
    vsxfrac.val[1] =
        vreinterpretq_u16_u8(vqtbl2q_u8(vsx_delta_hi, vsfrac_tbl_));
    VecTraits<uint16_t>::store(vsxfrac, precalc1.xfrac);
    precalc1.sxbase = sx_base;
  }

  void precalculate_indices_fractions_bases() {
    // Repeatedly adding sx_fixp_step_ is faster than multiplication, but it
    // accumulates fixed-point error; periodic recalibration resets it. The
    // maximum per-addition error of sx_fixp_step_ is 0.5 / (1 << 16). Only the
    // upper 8 bits of the 16-bit fractional part are used for interpolation, so
    // once the accumulated error reaches 1 / (1 << 8), it can affect later
    // stages. This corresponds to 512 additions. Since two additions are
    // performed per cycle, we recalibrate every 256 cycles.
    constexpr ptrdiff_t kRecalibrateCycle = 256;

    ptrdiff_t dx = 0;
    size_t i = 0;
    ptrdiff_t max_sx =
        std::max(static_cast<ptrdiff_t>(src_width_ * kChannels -
                                        (sizeof(uint8x16_t) * kRatio)),
                 0L) /
        kChannels;
    while (i < num_of_vector_path_2x_) {
      uint64_t sx_fixp = to_src_x(dx);
      size_t i_end = std::min(num_of_vector_path_2x_, i + kRecalibrateCycle);
      // Unroll twice, so it is in sync with process_row
      while (i < i_end) {
        ptrdiff_t sx_candidate = static_cast<ptrdiff_t>(sx_fixp >> kFixpBits);
        calculate_indices_fractions_base(i * 2, std::min(max_sx, sx_candidate),
                                         sx_fixp);
        sx_fixp += sx_fixp_step_;
        dx += kStep / kChannels;
        sx_candidate = static_cast<ptrdiff_t>(sx_fixp >> kFixpBits);
        calculate_indices_fractions_base(
            (i * 2) + 1, std::min(max_sx, sx_candidate), sx_fixp);
        sx_fixp += sx_fixp_step_;
        dx += kStep / kChannels;
        i++;
      }
    }
  }

  void process_row(uint64_t dy) {
    uint64_t sy_fixp = to_src_y(dy);
    ptrdiff_t sy = static_cast<ptrdiff_t>(sy_fixp >> kFixpBits);
    auto src_cols_top = src_rows_.at(sy).as_columns();
    auto src_cols_bottom = src_rows_.at(sy + 1).as_columns();
    auto dst_cols = dst_rows_.at(static_cast<ptrdiff_t>(dy)).as_columns();
    // Get the highest 8 bits of the fractional part
    // This is a good compromise between accuracy and performance
    // Because the result is 8bits, the error only affects the least
    // significant 1-2 bits, see the accuracy calculation in kleidicv.h
    uint16_t yfrac =
        static_cast<uint16_t>((sy_fixp - (sy << kFixpBits)) >> (kFixpBits - 8));

    ptrdiff_t dx = 0;

    for (size_t i = 0; i < num_of_vector_path_2x_; i += 1) {
      auto res = vector_path_2x(i, yfrac, src_cols_top, src_cols_bottom);
      VecTraits<uint8_t>::store(res, &dst_cols.at(dx)[0]);
      dx += (kStep / kChannels) * 2;
    }

    // Remaining part:
    ptrdiff_t sx_fixp = to_src_x(dx);
    ptrdiff_t sx_fixp_one_element =
        rounding_div<ptrdiff_t>(src_width_ << kFixpBits, dst_width_);
    // A pixel consists of kChannel elements: total width is that big, but
    // finally it shall be divided to get the max in pixel coordinate
    ptrdiff_t max_sx =
        std::max(static_cast<ptrdiff_t>(src_width_ * kChannels -
                                        sizeof(std::conditional_t < kRatio == 2,
                                               uint8x16_t, uint8x16x2_t >)),
                 0L) /
        kChannels;
    for (; dx < static_cast<ptrdiff_t>(dst_width_);
         dx += kHalfStep / kChannels) {
      // If (dx + half vector length) would overrun the buffer, pull it back
      ptrdiff_t dx_fixed = std::min(
          dx, static_cast<ptrdiff_t>(dst_width_) - kHalfStep / kChannels);
      sx_fixp -= (dx - dx_fixed) * sx_fixp_one_element;
      dx = dx_fixed;
      // If (sx_base + reading length) would overrun the buffer, pull it back
      ptrdiff_t sx_candidate = static_cast<ptrdiff_t>(sx_fixp >> kFixpBits);
      ptrdiff_t sx_base = std::min(max_sx, sx_candidate);
      vector_path_half(sx_base, dx, sx_fixp, yfrac, src_cols_top,
                       src_cols_bottom, dst_cols);
      sx_fixp += sx_fixp_half_step_;
    }
  }

  void vector_path_half(ptrdiff_t sx_base, ptrdiff_t dx, uint64_t sx_fixp,
                        uint16_t yfrac,
                        const Columns<const uint8_t> &src_cols_top,
                        const Columns<const uint8_t> &src_cols_bottom,
                        Columns<uint8_t> dst_cols) const {
    uint32_t xfrac0 = static_cast<uint32_t>(sx_fixp - (sx_base << kFixpBits));
    using SrcVecType =
        std::conditional_t<kRatio == 2, uint8x16_t, uint8x16x2_t>;
    SrcVecType topsrc, bottomsrc;
    VecTraits<uint8_t>::load(src_cols_top.ptr_at(sx_base), topsrc);
    VecTraits<uint8_t>::load(src_cols_bottom.ptr_at(sx_base), bottomsrc);
    uint8x16x2_t vsx_delta;
    vsx_delta.val[0] =
        vreinterpretq_u8_u32(vaddq_u32(vsx0_0_, vdupq_n_u32(xfrac0)));
    vsx_delta.val[1] =
        vreinterpretq_u8_u32(vaddq_u32(vsx0_1_, vdupq_n_u32(xfrac0)));
    uint8x8_t vsx0_idx = vqtbl2_u8(vsx_delta, vsidx_tbl_);
    if constexpr (kChannels > 1) {
      vsx0_idx = vshl_n_u8(vsx0_idx, kChannels == 4 ? 2 : 1);
      vsx0_idx = vadd_u8(
          vsx0_idx, vreinterpret_u8_u32(
                        vdup_n_u32(kChannels == 4 ? 0x03020100U : 0x01000100)));
    }
    uint8x8_t vsx1_idx = vadd_u8(vsx0_idx, vdup_n_u8(kChannels));
    uint16x8_t vsxfrac =
        vreinterpretq_u16_u8(vqtbl2q_u8(vsx_delta, vsfrac_tbl_));

    uint8x8_t a, b, c, d;
    if constexpr (kRatio == 2) {
      a = vqtbl1_u8(topsrc, vsx0_idx);
      b = vqtbl1_u8(topsrc, vsx1_idx);
      c = vqtbl1_u8(bottomsrc, vsx0_idx);
      d = vqtbl1_u8(bottomsrc, vsx1_idx);
    } else if constexpr (kRatio == 3) {
      a = vqtbl2_u8(topsrc, vsx0_idx);
      b = vqtbl2_u8(topsrc, vsx1_idx);
      c = vqtbl2_u8(bottomsrc, vsx0_idx);
      d = vqtbl2_u8(bottomsrc, vsx1_idx);
    }
    uint8x8_t left =
        vraddhn_u16(vshll_n_u8(a, 8), vmulq_n_u16(vsubl_u8(c, a), yfrac));
    uint8x8_t right =
        vraddhn_u16(vshll_n_u8(b, 8), vmulq_n_u16(vsubl_u8(d, b), yfrac));
    uint8x8_t res = vraddhn_u16(vshll_n_u8(left, 8),
                                vmulq_u16(vsubl_u8(right, left), vsxfrac));
    vst1(dst_cols.ptr_at(dx), res);
  }

  // In this function, dx is in elements, not pixels
  // One pixel consists of kChannel elements
  uint8x16x2_t vector_path_2x(
      size_t vector_path_2x_index, uint16_t yfrac,
      const Columns<const uint8_t> &src_cols_top,
      const Columns<const uint8_t> &src_cols_bottom) const {
    return {vector_path(vector_path_2x_index * 2, src_cols_top, src_cols_bottom,
                        yfrac),
            vector_path((vector_path_2x_index * 2) + 1, src_cols_top,
                        src_cols_bottom, yfrac)};
  }

  uint8x16_t vector_path(size_t precalc_index,
                         const Columns<const uint8_t> &src_cols_top,
                         const Columns<const uint8_t> &src_cols_bottom,
                         uint16_t yfrac) const {
    PrecalcData &precalc1 = precalc_.get()[precalc_index];
    uint8x16_t vsx0_idx = vld1q(precalc1.idx);
    uint8x16_t vsx1_idx = vaddq_u8(vsx0_idx, vdupq_n_u8(kChannels));
    uint16x8x2_t vsxfrac2;
    VecTraits<uint16_t>::load(precalc1.xfrac, vsxfrac2);
    ptrdiff_t sx_base = precalc1.sxbase;

    using SrcVecType =
        std::conditional_t<kRatio == 2, uint8x16x2_t, uint8x16x3_t>;
    SrcVecType topsrc, bottomsrc;
    VecTraits<uint8_t>::load(src_cols_top.ptr_at(sx_base), topsrc);
    VecTraits<uint8_t>::load(src_cols_bottom.ptr_at(sx_base), bottomsrc);
    uint8x16_t a, b, c, d;
    if constexpr (kRatio == 2) {
      a = vqtbl2q_u8(topsrc, vsx0_idx);
      b = vqtbl2q_u8(topsrc, vsx1_idx);
      c = vqtbl2q_u8(bottomsrc, vsx0_idx);
      d = vqtbl2q_u8(bottomsrc, vsx1_idx);
    } else if constexpr (kRatio == 3) {
      a = vqtbl3q_u8(topsrc, vsx0_idx);
      b = vqtbl3q_u8(topsrc, vsx1_idx);
      c = vqtbl3q_u8(bottomsrc, vsx0_idx);
      d = vqtbl3q_u8(bottomsrc, vsx1_idx);
    }
    uint8x8_t left_lo = lerp_low_half(a, c, yfrac);
    uint8x8_t left_hi = lerp_high_half(a, c, yfrac);
    uint8x8_t right_lo = lerp_low_half(b, d, yfrac);
    uint8x8_t right_hi = lerp_high_half(b, d, yfrac);
    uint8x8_t res_lo = lerp_full(left_lo, right_lo, vsxfrac2.val[0]);
    uint8x8_t res_hi = lerp_full(left_hi, right_hi, vsxfrac2.val[1]);
    return vcombine_u8(res_lo, res_hi);
  }

  static uint8x8_t lerp_low_half(uint8x16_t a, uint8x16_t b, uint16_t w) {
    return vraddhn_u16(
        vshll_n_u8(vget_low_u8(a), 8),
        vmulq_n_u16(vsubl_u8(vget_low_u8(b), vget_low_u8(a)), w));
  }

  static uint8x8_t lerp_high_half(uint8x16_t a, uint8x16_t b, uint16_t w) {
    return vraddhn_u16(vshll_high_n_u8(a, 8),
                       vmulq_n_u16(vsubl_high_u8(b, a), w));
  }

  static uint8x8_t lerp_full(uint8x8_t a, uint8x8_t b, uint16x8_t w) {
    return vraddhn_u16(vshll_n_u8(a, 8), vmulq_u16(vsubl_u8(b, a), w));
  }

  const Rows<const uint8_t> src_rows_;
  const Rows<uint8_t> dst_rows_;
  const size_t src_width_;
  const size_t src_height_;
  const size_t y_begin_;
  const size_t y_end_;
  const size_t dst_width_;
  const size_t dst_height_;
  const uint64_t sx_fixp_step_;
  const uint64_t sx_fixp_half_step_;
  const uint8x8_t vsidx_tbl_;
  const uint8x16_t vsfrac_tbl_;
  const uint32x4_t vsx0_0_;
  const uint32x4_t vsx0_1_;
  const uint32x4_t vsx0_2_;
  const uint32x4_t vsx0_3_;
  std::unique_ptr<PrecalcData, FreeDeleter> precalc_;
  const size_t num_of_vector_path_2x_;
};

// ratio: number of vectors to load and resize to 1 vector
// supported combinations of (ratio, channel): (2, 1), (2, 2), (3, 1), (3, 2)
template <ptrdiff_t kRatio, ptrdiff_t kChannels>
kleidicv_error_t kleidicv_resize_generic_stripe_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    size_t y_begin, size_t y_end,
    uint8_t *dst,  // NOLINT
    size_t dst_stride, size_t dst_width, size_t dst_height) {
  ResizeGenericU8Operation<kRatio, kChannels> operation(
      src, src_stride, src_width, src_height, y_begin, y_end, dst, dst_stride,
      dst_width, dst_height);
  return operation.process_rows();
}

#define KLEIDICV_INSTANTIATE_TEMPLATE(ratio, channels)               \
  template kleidicv_error_t                                          \
  kleidicv_resize_generic_stripe_u8<ratio, channels>(                \
      const uint8_t *src, size_t src_stride, size_t src_width,       \
      size_t src_height, size_t y_begin, size_t y_end, uint8_t *dst, \
      size_t dst_stride, size_t dst_width, size_t dst_height)

KLEIDICV_INSTANTIATE_TEMPLATE(2L, 1L);
KLEIDICV_INSTANTIATE_TEMPLATE(2L, 2L);
KLEIDICV_INSTANTIATE_TEMPLATE(3L, 1L);
KLEIDICV_INSTANTIATE_TEMPLATE(3L, 2L);

}  // namespace kleidicv::neon
