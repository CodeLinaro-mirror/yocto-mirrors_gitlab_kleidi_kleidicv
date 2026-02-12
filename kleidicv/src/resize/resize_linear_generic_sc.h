// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_RESIZE_LINEAR_GENERIC_SC_H
#define KLEIDICV_RESIZE_LINEAR_GENERIC_SC_H

#include <algorithm>
#include <cstddef>
#include <memory>

#include "kleidicv/kleidicv.h"
#include "kleidicv/sve2.h"

namespace KLEIDICV_TARGET_NAMESPACE {

//------------------------------------------------------
/// Generic resize for ratios 1/3 to 1/1, u8, 1channel
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
                           size_t y_end, uint8_t *dst,  // NOLINT
                           size_t dst_stride, size_t dst_width,
                           size_t dst_height, svuint32_t &s0, svuint32_t &s1,
                           svuint32_t &s2, svuint32_t &s3, svuint8_t &s4,
                           svuint8_t &s5) KLEIDICV_STREAMING
      : src_rows_{src, src_stride, kChannels},
        dst_rows_{dst, dst_stride, kChannels},
        src_width_{src_width},
        src_height_{src_height},
        y_begin_{y_begin},
        y_end_{y_end},
        dst_width_{dst_width},
        dst_height_{dst_height},
        rounded_width_{0},
        max_2x_vector_path_{0},
        kStep_{static_cast<ptrdiff_t>(svcntb())},
        idx_{nullptr},
        xfrac_{nullptr},
        sxbase_{nullptr},
        precalc_buffer_{nullptr, &std::free},
        vsx0b_{s0},
        vsx0t_{s1},
        vsx1b_{s2},
        vsx1t_{s3},
        vsxfrac_bottom_tbl_{s4},
        vsxfrac_top_tbl_{s5},
        // Difference in source x coordinate, for one vector path
        sx_fixp_step_{
            rounding_div(((src_width_ * kStep_ / kChannels) << kFixpBits),
                         dst_width_)} {
    // These starting values are not aligned to center. The center alignment
    // must be added only once. When added to a center-aligned source_x
    // value, the result will be center-aligned.
    vsx0b_ = make_vsx0(0);
    vsx0t_ = make_vsx0(1);
    vsx1b_ = make_vsx0(2 * svcntw());
    vsx1t_ = make_vsx0(2 * svcntw() + 1);

    // from each even 16bit element, take the low byte, and the high is 0
    vsxfrac_bottom_tbl_ = svreinterpret_u8_u16(svindex_u16(0xFF00, 0x0004));
    // from each odd 16bit element, take the low byte, and the high is 0
    vsxfrac_top_tbl_ = svreinterpret_u8_u16(svindex_u16(0xFF02, 0x0004));
  }

  kleidicv_error_t process_rows() KLEIDICV_STREAMING {
    kleidicv_error_t err = allocate_temp_buffers();
    if (err != KLEIDICV_OK) {
      return err;
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
  using FreeDeleter = decltype(&std::free);

  template <typename T = uint64_t>
  static T rounding_div(uint64_t nom, uint64_t denom) {
    return static_cast<T>((nom + denom / 2) / denom);
  }

  // Scale coordinate using this formula, so the center is aligned:
  //   source_x = (destination_x + 0.5) / scale - 0.5;
  //   plus 1/256/2 for later rounding the fractional part to 8bits
  static uint64_t aligned_scale(uint64_t x, uint64_t nom, uint64_t denom) {
    return rounding_div(((x << kFixpBits) + kFixpHalf) * nom, denom) -
           kFixpHalf + (1 << (kFixpBits - 9));
  }

  uint64_t to_src_x(uint64_t dx) const KLEIDICV_STREAMING {
    return aligned_scale(dx, src_width_, dst_width_);
  }

  uint64_t to_src_y(uint64_t dy) const KLEIDICV_STREAMING {
    return aligned_scale(dy, src_height_, dst_height_);
  }

  kleidicv_error_t allocate_temp_buffers() KLEIDICV_STREAMING {
    // Allocate a bit more so don't have to care about overindexing
    rounded_width_ = align_up(dst_width_ * kChannels, kStep_);
    // When kRatio == 3: store dx0 and dx1 indices (x+1), interleaved
    size_t idx_bytes = kRatio == 3 ? 2 * rounded_width_ : rounded_width_;
    size_t xfrac_begin = idx_bytes;
    size_t sxbase_begin = xfrac_begin + rounded_width_ * sizeof(uint16_t);
    size_t total_bytes =
        sxbase_begin + (rounded_width_ / kStep_) * sizeof(uint64_t);
    precalc_buffer_.reset(static_cast<uint8_t *>(malloc(total_bytes)));
    if (!precalc_buffer_) {
      return KLEIDICV_ERROR_ALLOCATION;
    }
    idx_ = precalc_buffer_.get();
    xfrac_ = reinterpret_cast<uint16_t *>(precalc_buffer_.get() + xfrac_begin);
    sxbase_ =
        reinterpret_cast<uint64_t *>(precalc_buffer_.get() + sxbase_begin);
    return KLEIDICV_OK;
  }

  svuint32_t make_vsx0(uint64_t dx) const KLEIDICV_STREAMING {
    svuint64_t half = svdup_n_u64(dst_width_ / 2);
    svuint64_t sx_b = svmla_n_u64_x(svptrue_b64(), half,
                                    svindex_u64(dx / kChannels, 4 / kChannels),
                                    src_width_ << kFixpBits);
    svuint64_t sx_t = svmla_n_u64_x(
        svptrue_b64(), half, svindex_u64((dx + 2) / kChannels, 4 / kChannels),
        src_width_ << kFixpBits);
    return svtrn1_u32(
        svreinterpret_u32_u64(svlsl_n_u64_x(
            svptrue_b64(), svdiv_n_u64_x(svptrue_b64(), sx_b, dst_width_), 8)),
        svreinterpret_u32_u64(svlsl_n_u64_x(
            svptrue_b64(), svdiv_n_u64_x(svptrue_b64(), sx_t, dst_width_), 8)));
  }

  void calculate_indices_fractions_base(
      size_t i, ptrdiff_t dx, ptrdiff_t sx_base,
      uint64_t sx_fixp) const KLEIDICV_STREAMING {
    sxbase_[i] = sx_base;
    // << 8: to prepare for addhn, have the fractional part in the high half
    uint32_t xfrac0 = static_cast<uint32_t>(sx_fixp - (sx_base << kFixpBits))
                      << 8;
    // get the interesting part: 8+8 bits of integer and fractional part
    svuint8x2_t vsx_delta =
        svcreate2(svreinterpret_u8_u16(svaddhnt_n_u32(
                      svaddhnb_n_u32(vsx0b_, xfrac0), vsx0t_, xfrac0)),
                  svreinterpret_u8_u16(svaddhnt_n_u32(
                      svaddhnb_n_u32(vsx1b_, xfrac0), vsx1t_, xfrac0)));
    // left pixels' indices: integer part
    svuint8_t vsx_left_idx =
        svuzp2_u8(svget2(vsx_delta, 0), svget2(vsx_delta, 1));
    if constexpr (kChannels > 1) {
      vsx_left_idx =
          svlsl_n_u8_x(svptrue_b8(), vsx_left_idx, kChannels == 4 ? 2 : 1);
      vsx_left_idx = svadd_u8_x(
          svptrue_b8(), vsx_left_idx,
          svreinterpret_u8_u32(
              svdup_n_u32(kChannels == 4 ? 0x03020100U : 0x01000100)));
    }
    if constexpr (kRatio == 2) {
      svst1(svptrue_b8(), idx_ + dx * kChannels, vsx_left_idx);
    } else if constexpr (kRatio == 3) {
      svuint8_t vsx_right_idx =
          svadd_n_u8_x(svptrue_b8(), vsx_left_idx, kChannels);
      // left and right pixels' indices, interleaved (LRLRLR...)
      svuint8_t vsx_idx_lo = svzip1_u8(vsx_left_idx, vsx_right_idx);
      svuint8_t vsx_idx_hi = svzip2_u8(vsx_left_idx, vsx_right_idx);
      vsx_idx_hi =
          svsub_n_u8_x(svptrue_b8(), vsx_idx_hi, static_cast<uint8_t>(kStep_));
      svst1(svptrue_b8(), idx_ + 2 * dx * kChannels, vsx_idx_lo);
      svst1_vnum(svptrue_b8(), idx_ + 2 * dx * kChannels, 1, vsx_idx_hi);
    }
    // fractional part is widened to 16 bits for further operations
    svuint16_t vsxfrac_b =
        svreinterpret_u16_u8(svtbl2_u8(vsx_delta, vsxfrac_bottom_tbl_));
    svuint16_t vsxfrac_t =
        svreinterpret_u16_u8(svtbl2_u8(vsx_delta, vsxfrac_top_tbl_));
    svst1(svptrue_b16(), xfrac_ + dx * kChannels, vsxfrac_b);
    svst1_vnum(svptrue_b16(), xfrac_ + dx * kChannels, 1, vsxfrac_t);
  }

  void precalculate_indices_fractions_bases() KLEIDICV_STREAMING {
    // Repeatedly adding sx_fixp_step_ is faster than multiplication, but it
    // accumulates fixed-point error; periodic recalibration resets it. The
    // maximum per-addition error of sx_fixp_step_ is 0.5 / (1 << 16). Only the
    // upper 8 bits of the 16-bit fractional part are used for interpolation, so
    // once the accumulated error reaches 1 / (1 << 8), it can affect later
    // stages. This corresponds to 512 additions.
    constexpr ptrdiff_t kRecalibrateCycle = 512;
    size_t i = 0;
    size_t dx = 0;
    size_t max_i = rounded_width_ / kStep_;
    uint64_t max_sx = std::max(src_width_ * kChannels - kStep_ * kRatio, 0UL);
    while (dx < dst_width_) {
      uint64_t sx_fixp = to_src_x(dx);
      size_t i_end = std::min(max_i, i + kRecalibrateCycle);
      while (i < i_end) {
        uint64_t sx_base = sx_fixp >> kFixpBits;
        max_2x_vector_path_ = sx_base * kChannels <= max_sx
                                  ? static_cast<ptrdiff_t>(i) - 1
                                  : max_2x_vector_path_;
        calculate_indices_fractions_base(i, dx, sx_base, sx_fixp);
        sx_fixp += sx_fixp_step_;
        dx += kStep_ / kChannels;
        i++;
      }
    }
  }

  static svuint16_t svshll8b(svuint8_t a) KLEIDICV_STREAMING {
    return svreinterpret_u16_u8(svtrn1(svdup_n_u8(0), a));
  }
  static svuint16_t svshll8t(svuint8_t a) KLEIDICV_STREAMING {
    return svreinterpret_u16_u8(svtrn2(svdup_n_u8(0), a));
  }

  static svuint8x2_t load8x2_u8(const uint8_t *p) KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME2
    return svld1_x2(svptrue_c8(), p);
#else
    return svcreate2(svld1(svptrue_b8(), p), svld1_vnum(svptrue_b8(), p, 1));
#endif
  }

  svuint8x2_t load8x2_while_u8(const uint8_t *p, uint64_t i,
                               uint64_t n) const KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME2
    return svld1_x2(svwhilelt_c8(i, n, 2), p);
#else
    svbool_t pg1 = svwhilelt_b8(i, n);
    svbool_t pg2 = svwhilelt_b8(i + kStep_, n);
    return svcreate2(svld1(pg1, p), svld1_vnum(pg2, p, 1));
#endif
  }

  static svuint8x3_t load8x3_u8(const uint8_t *p) KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME2
    svuint8x2_t sv2 = svld1_x2(svptrue_c8(), p);
    return svcreate3(svget2(sv2, 0), svget2(sv2, 1),
                     svld1_vnum(svptrue_b8(), p, 2));
#else
    return svcreate3(svld1(svptrue_b8(), p), svld1_vnum(svptrue_b8(), p, 1),
                     svld1_vnum(svptrue_b8(), p, 2));
#endif
  }

  svuint8x3_t load8x3_while_u8(const uint8_t *p, uint64_t i,
                               uint64_t n) const KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME2
    svcount_t pgc = svwhilelt_c8(i, n, 2);
    svbool_t pgb = svwhilelt_b8(i + 2 * kStep_, n);
    svuint8x2_t sv2 = svld1_x2(pgc, p);
    return svcreate3(svget2(sv2, 0), svget2(sv2, 1), svld1_vnum(pgb, p, 2));
#else
    svbool_t pg1 = svwhilelt_b8(i, n);
    svbool_t pg2 = svwhilelt_b8(i + kStep_, n);
    svbool_t pg3 = svwhilelt_b8(i + 2 * kStep_, n);
    return svcreate3(svld1(pg1, p), svld1_vnum(pg2, p, 1),
                     svld1_vnum(pg3, p, 2));
#endif
  }

  svuint8_t interpolate(ptrdiff_t dxidx, uint16_t yfrac, svuint8_t a,
                        svuint8_t b, svuint8_t c,
                        svuint8_t d) const KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME2
    svuint16x2_t vsxfrac = svld1_x2(svptrue_c8(), xfrac_ + dxidx);
    svuint16_t vsxfrac_b = svget2(vsxfrac, 0);
    svuint16_t vsxfrac_t = svget2(vsxfrac, 1);
#else
    svuint16_t vsxfrac_b = svld1(svptrue_b16(), xfrac_ + dxidx);
    svuint16_t vsxfrac_t = svld1_vnum(svptrue_b16(), xfrac_ + dxidx, 1);
#endif
    svuint16_t half = svdup_n_u16(128);
    svuint8_t left = svaddhnb(
        svshll8b(a), svmla_n_u16_x(svptrue_b16(), half, svsublb(c, a), yfrac));
    svuint8_t right = svaddhnb(
        svshll8b(b), svmla_n_u16_x(svptrue_b16(), half, svsublb(d, b), yfrac));
    left = svaddhnt(left, svshll8t(a),
                    svmla_n_u16_x(svptrue_b16(), half, svsublt(c, a), yfrac));
    right = svaddhnt(right, svshll8t(b),
                     svmla_n_u16_x(svptrue_b16(), half, svsublt(d, b), yfrac));

    svuint8_t res =
        svaddhnb(svshll8b(left),
                 svmla_x(svptrue_b16(), half, svsublb(right, left), vsxfrac_b));
    return svaddhnt(
        res, svshll8t(left),
        svmla_x(svptrue_b16(), half, svsublt(right, left), vsxfrac_t));
  }

  svuint8_t common_vector_path_r2(
      ptrdiff_t dxidx, uint16_t yfrac, svuint8x2_t topsrc,
      svuint8x2_t bottomsrc) const KLEIDICV_STREAMING {
    svuint8_t vsx0_idx = svld1(svptrue_b8(), idx_ + dxidx);
    svuint8_t vsx1_idx = svadd_n_u8_x(svptrue_b8(), vsx0_idx, kChannels);
    svuint8_t a = svtbl2_u8(topsrc, vsx0_idx);
    svuint8_t b = svtbl2_u8(topsrc, vsx1_idx);
    svuint8_t c = svtbl2_u8(bottomsrc, vsx0_idx);
    svuint8_t d = svtbl2_u8(bottomsrc, vsx1_idx);
    return interpolate(dxidx, yfrac, a, b, c, d);
  }

  svuint8x2_t vector_path_2x_r2(ptrdiff_t i, ptrdiff_t dxidx, uint16_t yfrac,
                                Columns<const uint8_t> src_cols_top,
                                Columns<const uint8_t> src_cols_bottom) const
      KLEIDICV_STREAMING {
    ptrdiff_t sx_base0 = sxbase_[i], sx_base1 = sxbase_[i + 1];
    // Load 2*2*step elements, that's enough for 1/2 < scale < 1.0
    svuint8x2_t topsrc0 = load8x2_u8(src_cols_top.ptr_at(sx_base0));
    svuint8x2_t topsrc1 = load8x2_u8(src_cols_top.ptr_at(sx_base1));
    svuint8x2_t bottomsrc0 = load8x2_u8(src_cols_bottom.ptr_at(sx_base0));
    svuint8x2_t bottomsrc1 = load8x2_u8(src_cols_bottom.ptr_at(sx_base1));
    return svcreate2(
        common_vector_path_r2(dxidx, yfrac, topsrc0, bottomsrc0),
        common_vector_path_r2(dxidx + kStep_, yfrac, topsrc1, bottomsrc1));
  }

  svuint8_t vector_path_r2(ptrdiff_t i, ptrdiff_t dxidx, uint16_t yfrac,
                           Columns<const uint8_t> src_cols_top,
                           Columns<const uint8_t> src_cols_bottom) const
      KLEIDICV_STREAMING {
    ptrdiff_t sx_base = sxbase_[i];
    // Load 2*step elements, that's enough for 1/2 < scale < 1.0
    svuint8x2_t topsrc = load8x2_while_u8(
        src_cols_top.ptr_at(sx_base),
        static_cast<uint64_t>(sx_base) * kChannels, src_width_ * kChannels);
    svuint8x2_t bottomsrc = load8x2_while_u8(
        src_cols_bottom.ptr_at(sx_base),
        static_cast<uint64_t>(sx_base) * kChannels, src_width_ * kChannels);
    return common_vector_path_r2(dxidx, yfrac, topsrc, bottomsrc);
  }

  svuint8_t common_vector_path_r3(
      ptrdiff_t dxidx, uint16_t yfrac, svuint8x3_t topsrc,
      svuint8x3_t bottomsrc) const KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME2
    svuint8x2_t vidx = svld1_x2(svptrue_c8(), idx_ + 2 * dxidx);
    svuint8_t vsx_idx_lo = svget2(vidx, 0);
    svuint8_t vsx_idx_hi = svget2(vidx, 1);
#else
    svuint8_t vsx_idx_lo = svld1(svptrue_b8(), idx_ + 2 * dxidx);
    svuint8_t vsx_idx_hi = svld1_vnum(svptrue_b8(), idx_ + 2 * dxidx, 1);
#endif
    svuint8_t ab_lo =
        svtbl2_u8(svcreate2(svget3(topsrc, 0), svget3(topsrc, 1)), vsx_idx_lo);
    svuint8_t ab_hi =
        svtbl2_u8(svcreate2(svget3(topsrc, 1), svget3(topsrc, 2)), vsx_idx_hi);
    svuint8_t cd_lo = svtbl2_u8(
        svcreate2(svget3(bottomsrc, 0), svget3(bottomsrc, 1)), vsx_idx_lo);
    svuint8_t cd_hi = svtbl2_u8(
        svcreate2(svget3(bottomsrc, 1), svget3(bottomsrc, 2)), vsx_idx_hi);
    svuint8_t a = svuzp1_u8(ab_lo, ab_hi);
    svuint8_t b = svuzp2_u8(ab_lo, ab_hi);
    svuint8_t c = svuzp1_u8(cd_lo, cd_hi);
    svuint8_t d = svuzp2_u8(cd_lo, cd_hi);
    return interpolate(dxidx, yfrac, a, b, c, d);
  }

  svuint8x2_t vector_path_2x_r3(ptrdiff_t i, ptrdiff_t dxidx, uint16_t yfrac,
                                Columns<const uint8_t> src_cols_top,
                                Columns<const uint8_t> src_cols_bottom) const
      KLEIDICV_STREAMING {
    ptrdiff_t sx_base0 = sxbase_[i], sx_base1 = sxbase_[i + 1];
    // Load 3*2*step elements, that's enough for 1/3 < scale < 1.0
    svuint8x3_t topsrc0 = load8x3_u8(src_cols_top.ptr_at(sx_base0));
    svuint8x3_t topsrc1 = load8x3_u8(src_cols_top.ptr_at(sx_base1));
    svuint8x3_t bottomsrc0 = load8x3_u8(src_cols_bottom.ptr_at(sx_base0));
    svuint8x3_t bottomsrc1 = load8x3_u8(src_cols_bottom.ptr_at(sx_base1));
    return svcreate2(
        common_vector_path_r3(dxidx, yfrac, topsrc0, bottomsrc0),
        common_vector_path_r3(dxidx + kStep_, yfrac, topsrc1, bottomsrc1));
  }

  svuint8_t vector_path_r3(ptrdiff_t i, ptrdiff_t dxidx, uint16_t yfrac,
                           Columns<const uint8_t> src_cols_top,
                           Columns<const uint8_t> src_cols_bottom) const
      KLEIDICV_STREAMING {
    ptrdiff_t sx_base = sxbase_[i];
    // Load 3*step elements, that's enough for 1/3 < scale < 1.0
    svuint8x3_t topsrc = load8x3_while_u8(
        src_cols_top.ptr_at(sx_base),
        static_cast<uint64_t>(sx_base) * kChannels, src_width_ * kChannels);
    svuint8x3_t bottomsrc = load8x3_while_u8(
        src_cols_bottom.ptr_at(sx_base),
        static_cast<uint64_t>(sx_base) * kChannels, src_width_ * kChannels);
    return common_vector_path_r3(dxidx, yfrac, topsrc, bottomsrc);
  }

  void process_row(uint64_t dy) const KLEIDICV_STREAMING {
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

    // dx is in pixel units, dxidx is in elements, i.e. dxidx = dx * kChannels
    ptrdiff_t dxidx = 0;
    ptrdiff_t i = 0;
    while (i < max_2x_vector_path_) {
      svuint8x2_t res;
      if constexpr (kRatio == 3) {
        res = vector_path_2x_r3(i, dxidx, yfrac, src_cols_top, src_cols_bottom);
      } else if constexpr (kRatio == 2) {
        res = vector_path_2x_r2(i, dxidx, yfrac, src_cols_top, src_cols_bottom);
      }
#if KLEIDICV_TARGET_SME2
      svst1(svptrue_c8(), &dst_cols[dxidx], res);
#else
      svst1(svptrue_b8(), &dst_cols[dxidx], svget2(res, 0));
      svst1_vnum(svptrue_b8(), &dst_cols[dxidx], 1, svget2(res, 1));
#endif  // KLEIDICV_TARGET_SME2
      dxidx += 2 * kStep_;
      i += 2;
    }

    // similar to above, but only a single vector path and with predicates
    for (; dxidx < static_cast<ptrdiff_t>(dst_width_) * kChannels;
         dxidx += kStep_) {
      svbool_t pgdst =
          svwhilelt_b8(dxidx, static_cast<ptrdiff_t>(dst_width_) * kChannels);
      svuint8_t res;
      if constexpr (kRatio == 2) {
        res = vector_path_r2(i, dxidx, yfrac, src_cols_top, src_cols_bottom);
      } else if constexpr (kRatio == 3) {
        res = vector_path_r3(i, dxidx, yfrac, src_cols_top, src_cols_bottom);
      }
      svst1(pgdst, &dst_cols[dxidx], res);
      ++i;
    }
  }

  const Rows<const uint8_t> src_rows_;
  const Rows<uint8_t> dst_rows_;
  const size_t src_width_;
  const size_t src_height_;
  const size_t y_begin_;
  const size_t y_end_;
  const size_t dst_width_;
  const size_t dst_height_;
  size_t rounded_width_;
  ptrdiff_t max_2x_vector_path_;
  const ptrdiff_t kStep_;

  uint8_t *idx_;
  uint16_t *xfrac_;
  uint64_t *sxbase_;
  std::unique_ptr<uint8_t, FreeDeleter> precalc_buffer_;

  svuint32_t &vsx0b_;
  svuint32_t &vsx0t_;
  svuint32_t &vsx1b_;
  svuint32_t &vsx1t_;
  svuint8_t &vsxfrac_bottom_tbl_;
  svuint8_t &vsxfrac_top_tbl_;
  const uint64_t sx_fixp_step_;
};

// ratio: number of vectors to load and resize to 1 vector
// - supported combinations of (ratio, channel): (2, 1), (2, 2), (3, 1), (3, 2)
template <ptrdiff_t kRatio, ptrdiff_t kChannels>
kleidicv_error_t kleidicv_resize_generic_stripe_u8_sc(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    size_t y_begin, size_t y_end,
    uint8_t *dst,  // NOLINT
    size_t dst_stride, size_t dst_width, size_t dst_height) KLEIDICV_STREAMING {
  svuint32_t s0, s1, s2, s3;
  svuint8_t s4, s5;
  ResizeGenericU8Operation<kRatio, kChannels> operation(
      src, src_stride, src_width, src_height, y_begin, y_end, dst, dst_stride,
      dst_width, dst_height, s0, s1, s2, s3, s4, s5);
  return operation.process_rows();
}

#define KLEIDICV_INSTANTIATE_TEMPLATE_SC(ratio, channels)            \
  template kleidicv_error_t                                          \
  kleidicv_resize_generic_stripe_u8_sc<ratio, channels>(             \
      const uint8_t *src, size_t src_stride, size_t src_width,       \
      size_t src_height, size_t y_begin, size_t y_end, uint8_t *dst, \
      size_t dst_stride, size_t dst_width, size_t dst_height)        \
      KLEIDICV_STREAMING

KLEIDICV_INSTANTIATE_TEMPLATE_SC(2L, 1L);
KLEIDICV_INSTANTIATE_TEMPLATE_SC(2L, 2L);
KLEIDICV_INSTANTIATE_TEMPLATE_SC(3L, 1L);
KLEIDICV_INSTANTIATE_TEMPLATE_SC(3L, 2L);

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_RESIZE_LINEAR_GENERIC_SC_H
