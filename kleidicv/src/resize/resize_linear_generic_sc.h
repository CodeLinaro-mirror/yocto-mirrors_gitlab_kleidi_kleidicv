// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_RESIZE_LINEAR_GENERIC_SC_H
#define KLEIDICV_RESIZE_LINEAR_GENERIC_SC_H

#include <algorithm>
#include <cstddef>
#include <memory>

#include "kleidicv/config.h"
#include "kleidicv/sve2.h"

namespace KLEIDICV_TARGET_NAMESPACE {

//------------------------------------------------------
/// Generic uint8 linear downscaling and upscaling for 1-4 channels.
/// Supports horizontal scaling factors >= 1/3 and any vertical scaling factor.
//------------------------------------------------------

namespace resize_generic_u8 {

// Fixed-point coordinates are used for better performance:
// - horizontal vector anchors are advanced in Q32, then narrowed to Q16
// - in Q16 coordinates the lowest kFixpBits bits are the fractional part
// - at interpolation, the high 15 bits are used from the fractional part
//   with signed Q15 rounded multiplication
// - to get the integer part, right shift by 16 bits, or zip/unzip/tbl etc. to
//   get the bytes needed
// - center alignment uses 0.5 in Q16, which is 1 << 15

static constexpr ptrdiff_t kFixpBits = 16;
static constexpr ptrdiff_t kFixpHalf = (1UL << (kFixpBits - 1));
static constexpr ptrdiff_t kInterpolationBits = 15;
static constexpr ptrdiff_t kCoordinateBits = 32;

static uint16_t interpolation_fraction(int64_t coordinate) KLEIDICV_STREAMING {
  return static_cast<uint16_t>(coordinate) >> (kFixpBits - kInterpolationBits);
}

static svint8_t make_channel_offsets(size_t start) KLEIDICV_STREAMING {
  svbool_t pg = svptrue_b8();
  svuint8_t index = svindex_u8(static_cast<uint8_t>(start % 3), 1);
  // x % 3 = x - [x/3], and x/3 is calculated as x * 171 / 512
  // which is x * 0.333984375
  // this accuracy is enough here, it does not result any error in any valid SVE
  // length
  svuint8_t quotient = svlsr_x(pg, svmulh_x(pg, index, svdup_n_u8(171)), 1);
  return svreinterpret_s8_u8(svsub_x(pg, index, svmul_n_u8_x(pg, quotient, 3)));
}

// Precalc 1 item:
// Frac:       2 vectors u16
// Idx:        2 vectors  u8 (left_idx, right_idx)
// Src_index:  uint64 (separate array)
template <size_t kRatio>
struct PrecalcIterator {
  size_t index_;
  uint64_t *src_index_ptr_;
  const size_t kStep, kIdxFracStep;
  uint8_t *idx0_ptr_;
  uint8_t *idx1_ptr_;
  uint16_t *frac_ptr_;
  PrecalcIterator(size_t kStepDst, uint64_t *src_indices,
                  uint8_t *p_idx_frac) KLEIDICV_STREAMING
      : index_{0},
        src_index_ptr_{src_indices},
        kStep{kStepDst},
        kIdxFracStep{kStep * (2 + 2)},
        idx0_ptr_{p_idx_frac},
        idx1_ptr_{p_idx_frac + kStep},
        frac_ptr_{reinterpret_cast<uint16_t *>(p_idx_frac + 2 * kStep)} {}

  PrecalcIterator &operator++() KLEIDICV_STREAMING {
    ++index_;
    ++src_index_ptr_;
    idx0_ptr_ += kIdxFracStep;
    idx1_ptr_ += kIdxFracStep;
    frac_ptr_ += kIdxFracStep / 2;
    return *this;
  }
};

template <int kRatio, int kChannels, bool kUpsize>
class PrecalcIndicesFractions final {
 public:
  PrecalcIndicesFractions(size_t src_width, size_t dst_width,
                          ptrdiff_t kStep) KLEIDICV_STREAMING
      : src_width_{src_width},
        dst_width_{dst_width},
        n_iterations_{0},
        n_iterations_2x_{0},
        kStep_{kStep},
        precalc_src_bases_{nullptr, &std::free},
        precalc_idx_frac_{nullptr, &std::free} {}

  PrecalcIterator<kRatio> begin() const KLEIDICV_STREAMING {
    return PrecalcIterator<kRatio>(kStep_, precalc_src_bases_.get(),
                                   precalc_idx_frac_.get());
  }

  bool precalculate_indices_fractions_srcindices() KLEIDICV_STREAMING {
    if (!allocate_temp_buffers()) {
      return false;
    }

    const size_t kSrcReadSize = kStep_ * kRatio;

    // These starting values are not aligned to center. The center alignment
    // must be added only once. When added to a center-aligned source_x
    // value, the result will be center-aligned.
    svint32_t vsx0b = make_vsx0(0);
    svint32_t vsx0t = make_vsx0(1);
    svint32_t vsx1b = make_vsx0(2 * svcntw());
    svint32_t vsx1t = make_vsx0(2 * svcntw() + 1);

    svint8_t vchannels = svreinterpret_s8_u32(
        svdup_n_u32(kChannels == 4 ? 0x03020100U : 0x01000100));

    // Q32 difference in source x coordinate for one vector path. Q32 keeps
    // accumulated error smaller while avoiding periodic recalibration.
    const int64_t sx_coordinate_step =
        coordinate_step(kStep_ / kChannels, src_width_, dst_width_);
    int64_t sx_coordinate = initial_coordinate(src_width_, dst_width_);
    const int64_t max_src_index =
        std::max<int64_t>(src_width_ * kChannels - kSrcReadSize, 0L);
    for (auto pcit = begin(); pcit.index_ < n_iterations_; ++pcit) {
      int64_t sx_fixp = narrow_coordinate(sx_coordinate);

      n_iterations_2x_ = (sx_fixp >> kFixpBits) * kChannels <= max_src_index
                             ? pcit.index_
                             : n_iterations_2x_;
      calculate_indices_fractions_srcindex(pcit, sx_fixp, vsx0b, vsx0t, vsx1b,
                                           vsx1t, vchannels);
      sx_coordinate += sx_coordinate_step;
    }
    return true;
  }

  bool precalculate_indices_fractions_srcindices_3ch() KLEIDICV_STREAMING {
    if (!allocate_temp_buffers()) {
      return false;
    }

    // These starting values are not aligned to center. The center alignment
    // must be added only once. When added to a center-aligned source_x
    // value, the result will be center-aligned.
    svint32_t vsx0b_R = make_vsx0(0);
    svint32_t vsx0t_R = make_vsx0(1);
    svint32_t vsx1b_R = make_vsx0(2 * svcntw());
    svint32_t vsx1t_R = make_vsx0(2 * svcntw() + 1);

    svint32_t vsx0b_G = make_vsx0(4 * svcntw());
    svint32_t vsx0t_G = make_vsx0(4 * svcntw() + 1);
    svint32_t vsx1b_G = make_vsx0(6 * svcntw());
    svint32_t vsx1t_G = make_vsx0(6 * svcntw() + 1);

    svint32_t vsx0b_B = make_vsx0(8 * svcntw());
    svint32_t vsx0t_B = make_vsx0(8 * svcntw() + 1);
    svint32_t vsx1b_B = make_vsx0(10 * svcntw());
    svint32_t vsx1t_B = make_vsx0(10 * svcntw() + 1);

    size_t kVL = svcntb();
    svint8_t vchannels_R = make_channel_offsets(0);
    svint8_t vchannels_G = make_channel_offsets(kVL);
    svint8_t vchannels_B = make_channel_offsets(2 * kVL);

    // Difference in source x coordinate, for three vector paths (one iteration
    // in this calculation)
    const int64_t sx_coordinate_step3 =
        coordinate_step(kStep_, src_width_, dst_width_);
    int64_t sx_coordinate = initial_coordinate(src_width_, dst_width_);
    const uint64_t max_src_index =
        std::max<int64_t>(src_width_ * kChannels - kStep_ * kRatio, 0L);
    auto pcit = begin();
    while (pcit.index_ < n_iterations_) {
      int64_t sx_fixp = narrow_coordinate(sx_coordinate);

      calculate_indices_fractions_srcindex(pcit, sx_fixp, vsx0b_R, vsx0t_R,
                                           vsx1b_R, vsx1t_R, vchannels_R);
      n_iterations_2x_ = *pcit.src_index_ptr_ <= max_src_index
                             ? pcit.index_
                             : n_iterations_2x_;
      ++pcit;
      if (pcit.index_ >= n_iterations_) {
        break;
      }
      calculate_indices_fractions_srcindex(pcit, sx_fixp, vsx0b_G, vsx0t_G,
                                           vsx1b_G, vsx1t_G, vchannels_G);
      n_iterations_2x_ = *pcit.src_index_ptr_ <= max_src_index
                             ? pcit.index_
                             : n_iterations_2x_;
      ++pcit;
      if (pcit.index_ >= n_iterations_) {
        break;
      }
      calculate_indices_fractions_srcindex(pcit, sx_fixp, vsx0b_B, vsx0t_B,
                                           vsx1b_B, vsx1t_B, vchannels_B);
      n_iterations_2x_ = *pcit.src_index_ptr_ <= max_src_index
                             ? pcit.index_
                             : n_iterations_2x_;
      ++pcit;
      sx_coordinate += sx_coordinate_step3;
    }
    return true;
  }

  size_t n_iterations() const KLEIDICV_STREAMING { return n_iterations_; }
  size_t n_iterations_2x() const KLEIDICV_STREAMING { return n_iterations_2x_; }
  uint64_t *src_bases() const KLEIDICV_STREAMING {
    return precalc_src_bases_.get();
  }
  uint8_t *idx_frac() const KLEIDICV_STREAMING {
    return precalc_idx_frac_.get();
  }

 private:
  using FreeDeleter = decltype(&std::free);

  bool allocate_temp_buffers() KLEIDICV_STREAMING {
    // Allocate a bit more so don't have to care about overindexing
    ptrdiff_t rounded_width = align_up(dst_width_ * kChannels, kStep_);
    n_iterations_ = rounded_width / kStep_;
    size_t idx_bytes = sizeof(uint8_t) * 2 * rounded_width;
    size_t xfrac_bytes = sizeof(uint16_t) * rounded_width;
    precalc_idx_frac_.reset(
        static_cast<uint8_t *>(malloc(idx_bytes + xfrac_bytes)));
    if (!precalc_idx_frac_) {
      return false;
    }
    size_t src_bases_bytes = sizeof(uint64_t) * rounded_width / kStep_;
    precalc_src_bases_.reset(static_cast<uint64_t *>(malloc(src_bases_bytes)));
    return static_cast<bool>(precalc_src_bases_);
  }

  template <typename T = size_t>
  static T rounding_div(size_t nom, size_t denom) KLEIDICV_STREAMING {
    return static_cast<T>((nom + denom / 2) / denom);
  }

  // Keep the repeatedly advanced coordinate in Q32. The bias combines
  // rounding to Q16 with the half-Q15-step bias used by direct Q16 calculation.
  static int64_t initial_coordinate(size_t nom,
                                    size_t denom) KLEIDICV_STREAMING {
    constexpr int64_t kNarrowingBias =
        (int64_t{1} << (kCoordinateBits - kFixpBits - 1)) +
        (int64_t{1} << (kCoordinateBits - kFixpBits));
    return rounding_div<int64_t>(nom << (kCoordinateBits - 1), denom) -
           (int64_t{1} << (kCoordinateBits - 1)) + kNarrowingBias;
  }

  static int64_t coordinate_step(size_t destination_pixels, size_t nom,
                                 size_t denom) KLEIDICV_STREAMING {
    // This numerator, including rounding, fits uint64_t at the maximum
    // supported width and architectural maximum 256-byte vector length.
    return rounding_div<int64_t>((nom * destination_pixels) << kCoordinateBits,
                                 denom);
  }

  static int64_t narrow_coordinate(int64_t coordinate) KLEIDICV_STREAMING {
    return coordinate >> (kCoordinateBits - kFixpBits);
  }

  uint64_t clamp_src_x(int64_t sx, int64_t max_sx) const KLEIDICV_STREAMING {
    return static_cast<uint64_t>(std::clamp(sx, int64_t{0}, max_sx));
  }

  // Scale destination x coordinate to source x coordinate, into fixed-point,
  // without center correction
  uint32_t scale_x(uint64_t dx) const KLEIDICV_STREAMING {
    return rounding_div<uint32_t>(((dx * src_width_) << kFixpBits), dst_width_);
  }

  svint32_t make_vsx0(uint64_t dx) const KLEIDICV_STREAMING {
    // Creates Q16 source x coordinates starting with dx and stepping by 2.
    int32_t sx[64];  // maximum possible vector length with 32-bit lanes
    for (size_t i = 0; i < svcntw(); ++i) {
      sx[i] = static_cast<int32_t>(scale_x((dx + 2 * i) / kChannels));
    }
    return svld1(svptrue_b32(), sx);
  }

  void calculate_indices_fractions_srcindex(
      PrecalcIterator<kRatio> &pcit, int64_t sx_fixp, const svint32_t &vsx0b,
      const svint32_t &vsx0t, const svint32_t &vsx1b, const svint32_t &vsx1t,
      [[maybe_unused]] const svint8_t &vchannels) const KLEIDICV_STREAMING {
    int32_t xfrac0 = static_cast<uint16_t>(sx_fixp);
    svbool_t pg32 = svptrue_b32();
    svint32_t vsx0b_coord = svadd_n_s32_x(pg32, vsx0b, xfrac0);
    svint32_t vsx0t_coord = svadd_n_s32_x(pg32, vsx0t, xfrac0);
    svint32_t vsx1b_coord = svadd_n_s32_x(pg32, vsx1b, xfrac0);
    svint32_t vsx1t_coord = svadd_n_s32_x(pg32, vsx1t, xfrac0);

    // Interleave the high halfwords containing the integer coordinates.
    svint16x2_t vsx_delta =
        svcreate2(svtrn2_s16(svreinterpret_s16_s32(vsx0b_coord),
                             svreinterpret_s16_s32(vsx0t_coord)),
                  svtrn2_s16(svreinterpret_s16_s32(vsx1b_coord),
                             svreinterpret_s16_s32(vsx1t_coord)));
    if constexpr (kChannels == 3) {
      // When vsx0 starts from other than zero, this offset must be subtracted
      // It is done before multiplying with channels to prevent 8-bit overflow
      int16_t start{};
      svst1(svptrue_pat_b16(SV_VL1), &start, svget2(vsx_delta, 0));
      vsx_delta =
          svcreate2(svsub_n_s16_x(svptrue_b16(), svget2(vsx_delta, 0), start),
                    svsub_n_s16_x(svptrue_b16(), svget2(vsx_delta, 1), start));
      sx_fixp += static_cast<int64_t>(start) * (int64_t{1} << kFixpBits);
    }
    svint8x2_t vsx_delta8 =
        svcreate2(svreinterpret_s8_s16(svget2(vsx_delta, 0)),
                  svreinterpret_s8_s16(svget2(vsx_delta, 1)));
    // left pixels' indices: integer part
    svint8_t vsx0_idx = svuzp1_s8(svget2(vsx_delta8, 0), svget2(vsx_delta8, 1));
    svint8_t vsx1_idx = svadd_n_s8_x(svptrue_b8(), vsx0_idx, 1);

    if constexpr (kUpsize) {
      // Clamp coordinates
      // At this point the lanes contain x-coordinates based on sx_base.
      int64_t sx_base = sx_fixp >> kFixpBits;
      svint8_t min_idx = svdup_n_s8(saturating_cast<int64_t, int8_t>(-sx_base));
      vsx0_idx = svmax_x(svptrue_b8(), vsx0_idx, min_idx);
      vsx1_idx = svmax_x(svptrue_b8(), vsx1_idx, min_idx);
      svint8_t max_idx = svdup_n_s8(
          saturating_cast<int64_t, int8_t>(src_width_ - 1 - sx_base));
      vsx0_idx = svmin_x(svptrue_b8(), vsx0_idx, max_idx);
      vsx1_idx = svmin_x(svptrue_b8(), vsx1_idx, max_idx);
    }

    if constexpr (kChannels > 1) {
      if constexpr (kChannels == 3) {
        vsx0_idx = svmul_n_s8_x(svptrue_b8(), vsx0_idx, 3);
        vsx1_idx = svmul_n_s8_x(svptrue_b8(), vsx1_idx, 3);
      } else {
        static_assert(kChannels == 2 || kChannels == 4);
        vsx0_idx = svlsl_n_s8_x(svptrue_b8(), vsx0_idx, kChannels == 4 ? 2 : 1);
        vsx1_idx = svlsl_n_s8_x(svptrue_b8(), vsx1_idx, kChannels == 4 ? 2 : 1);
      }
      // The channel offsets are relative to the base index
      vsx0_idx = svadd_s8_x(svptrue_b8(), vsx0_idx, vchannels);
      vsx1_idx = svadd_s8_x(svptrue_b8(), vsx1_idx, vchannels);
    }

    int64_t srcindex = (sx_fixp >> kFixpBits) * kChannels;
    if constexpr (kChannels == 3 || kUpsize) {
      // When vsx0_idx starts from other than zero, this offset must be
      // subtracted
      // The first three lanes shall be checked, because they can reference the
      // same pixel and if the first element is Green, then the first Red pixel
      // has the lowest index
      int8_t start[3]{};
      svst1(svptrue_pat_b8(SV_VL3), start, vsx0_idx);
      int8_t lowest = std::min(start[0], std::min(start[1], start[2]));
      vsx0_idx = svsub_n_s8_x(svptrue_b8(), vsx0_idx, lowest);
      vsx1_idx = svsub_n_s8_x(svptrue_b8(), vsx1_idx, lowest);
      srcindex += lowest;
    }

    *pcit.src_index_ptr_ = srcindex;
    svst1(svptrue_b8(), pcit.idx0_ptr_, svreinterpret_u8_s8(vsx0_idx));
    svst1(svptrue_b8(), pcit.idx1_ptr_, svreinterpret_u8_s8(vsx1_idx));

    // Extract the low halfwords containing the Q16 fractional coordinates,
    // then convert them to Q15. The bottom and top vectors hold the weights
    // for the even and odd byte lanes, respectively.
    svbool_t pg16 = svptrue_b16();
    svuint16_t vsxfrac_b =
        svlsr_n_u16_x(pg16,
                      svuzp1_u16(svreinterpret_u16_s32(vsx0b_coord),
                                 svreinterpret_u16_s32(vsx1b_coord)),
                      kFixpBits - kInterpolationBits);
    svuint16_t vsxfrac_t =
        svlsr_n_u16_x(pg16,
                      svuzp1_u16(svreinterpret_u16_s32(vsx0t_coord),
                                 svreinterpret_u16_s32(vsx1t_coord)),
                      kFixpBits - kInterpolationBits);
    svst1(svptrue_b16(), pcit.frac_ptr_, vsxfrac_b);
    svst1_vnum(svptrue_b16(), pcit.frac_ptr_, 1, vsxfrac_t);
  }

  const size_t src_width_;
  const size_t dst_width_;
  size_t n_iterations_;
  size_t n_iterations_2x_;
  const ptrdiff_t kStep_;
  std::unique_ptr<uint64_t, FreeDeleter> precalc_src_bases_;
  std::unique_ptr<uint8_t, FreeDeleter> precalc_idx_frac_;
};

// ratio: number of vectors to load and resize to 1 vector
// - supported ratios:
//   - kUpsize = false: 2, 3
//   - kUpsize = true: 1, 2
// - supported channels: 1, 2, 3, 4
// - kSetRightmostLanes: only with kUpsize=false, ratios=3, channels=3
template <int kRatio, int kChannels, bool kSetRightmostLanes, bool kUpsize>
class ResizeGenericU8Operation final {
 public:
  ResizeGenericU8Operation(const uint8_t *src, size_t src_stride,
                           size_t src_width, size_t src_height, size_t y_begin,
                           size_t y_end,
                           uint8_t *dst,  // NOLINT
                           size_t dst_stride, size_t dst_width,
                           size_t dst_height) KLEIDICV_STREAMING
      : src_rows_{src, src_stride, kChannels},
        dst_rows_{dst, dst_stride, kChannels},
        src_width_{src_width},
        src_height_{src_height},
        y_begin_{y_begin},
        y_end_{y_end},
        dst_width_{dst_width},
        dst_height_{dst_height},
        kStep_{static_cast<ptrdiff_t>(svcntb())},
        precalc_{src_width, dst_width, kStep_} {}

  kleidicv_error_t process_rows() KLEIDICV_STREAMING {
    bool precalc_success = false;
    if constexpr (kChannels == 3) {
      precalc_success =
          precalc_.precalculate_indices_fractions_srcindices_3ch();
    } else {
      precalc_success = precalc_.precalculate_indices_fractions_srcindices();
    }
    if (!precalc_success) {
      return KLEIDICV_ERROR_ALLOCATION;
    }

    for (uint64_t dst_y = y_begin_; dst_y < y_end_; ++dst_y) {
      process_row(dst_y);
    }

    return KLEIDICV_OK;
  }

 private:
  template <typename T = uint64_t>
  static T rounding_div(uint64_t nom, uint64_t denom) KLEIDICV_STREAMING {
    return static_cast<T>((nom + denom / 2) / denom);
  }

  // Scale coordinate using this formula, so the center is aligned:
  //   source_x = (destination_x + 0.5) / scale - 0.5;
  //   plus half a Q15 step for later rounding the fractional part
  static int64_t aligned_scale(size_t x, size_t nom,
                               size_t denom) KLEIDICV_STREAMING {
    return rounding_div<int64_t>(((x << kFixpBits) + kFixpHalf) * nom, denom) -
           kFixpHalf + (1 << (kFixpBits - kInterpolationBits - 1));
  }

  int64_t to_src_y(size_t dy) const KLEIDICV_STREAMING {
    return aligned_scale(dy, src_height_, dst_height_);
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

  static svint16x2_t lerp_delta(svuint8_t a, svuint8_t b,
                                int16_t weight) KLEIDICV_STREAMING {
    svint16_t delta_b = svreinterpret_s16_u16(svsublb(b, a));
    svint16_t delta_t = svreinterpret_s16_u16(svsublt(b, a));
    return svcreate2(svqrdmulh_n_s16(delta_b, weight),
                     svqrdmulh_n_s16(delta_t, weight));
  }

  static svint16x2_t lerp_delta(svuint8_t a, svuint8_t b, svuint16_t weight_b,
                                svuint16_t weight_t) KLEIDICV_STREAMING {
    svint16_t delta_b = svreinterpret_s16_u16(svsublb(b, a));
    svint16_t delta_t = svreinterpret_s16_u16(svsublt(b, a));
    return svcreate2(svqrdmulh_s16(delta_b, svreinterpret_s16_u16(weight_b)),
                     svqrdmulh_s16(delta_t, svreinterpret_s16_u16(weight_t)));
  }

  static svuint8_t lerp(svuint8_t a, svuint8_t b,
                        int16_t weight) KLEIDICV_STREAMING {
    svint16x2_t delta = lerp_delta(a, b, weight);

    // Interleave the low bytes of the even- and odd-lane signed deltas.
    svint8_t packed_delta = svtrn1_s8(svreinterpret_s8_s16(svget2(delta, 0)),
                                      svreinterpret_s8_s16(svget2(delta, 1)));
    return svadd_u8_x(svptrue_b8(), a, svreinterpret_u8_s8(packed_delta));
  }

  static svuint8_t lerp(svuint8_t a, svuint8_t b, svuint16_t weight_b,
                        svuint16_t weight_t) KLEIDICV_STREAMING {
    svint16x2_t delta = lerp_delta(a, b, weight_b, weight_t);

    // Interleave the low bytes of the even- and odd-lane signed deltas.
    svint8_t packed_delta = svtrn1_s8(svreinterpret_s8_s16(svget2(delta, 0)),
                                      svreinterpret_s8_s16(svget2(delta, 1)));
    return svadd_u8_x(svptrue_b8(), a, svreinterpret_u8_s8(packed_delta));
  }

  svuint8_t interpolate(const PrecalcIterator<kRatio> &pcit, uint16_t yfrac,
                        svuint8_t a, svuint8_t b, svuint8_t c,
                        svuint8_t d) const KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME2
    svuint16x2_t vsxfrac = svld1_x2(svptrue_c8(), pcit.frac_ptr_);
    svuint16_t vsxfrac_b = svget2(vsxfrac, 0);
    svuint16_t vsxfrac_t = svget2(vsxfrac, 1);
#else
    svuint16_t vsxfrac_b = svld1(svptrue_b16(), pcit.frac_ptr_);
    svuint16_t vsxfrac_t = svld1_vnum(svptrue_b16(), pcit.frac_ptr_, 1);
#endif
    svuint8_t left = lerp(a, c, static_cast<int16_t>(yfrac));
    svuint8_t right = lerp(b, d, static_cast<int16_t>(yfrac));
    return lerp(left, right, vsxfrac_b, vsxfrac_t);
  }

  svuint8_t common_vector_path_r1(
      const PrecalcIterator<kRatio> &pcit, uint16_t yfrac, svuint8_t topsrc,
      svuint8_t bottomsrc) const KLEIDICV_STREAMING {
    svuint8_t vsx0_idx = svld1(svptrue_b8(), pcit.idx0_ptr_);
    svuint8_t vsx1_idx = svld1(svptrue_b8(), pcit.idx1_ptr_);
    svuint8_t a = svtbl_u8(topsrc, vsx0_idx);
    svuint8_t b = svtbl_u8(topsrc, vsx1_idx);
    svuint8_t c = svtbl_u8(bottomsrc, vsx0_idx);
    svuint8_t d = svtbl_u8(bottomsrc, vsx1_idx);
    return interpolate(pcit, yfrac, a, b, c, d);
  }

  svuint8_t vector_path_r1(const PrecalcIterator<kRatio> &pcit, uint16_t yfrac,
                           const uint8_t *src_top,
                           const uint8_t *src_bottom) const KLEIDICV_STREAMING {
    uint64_t src_index = *pcit.src_index_ptr_;
    svuint8_t topsrc = svld1(svptrue_b8(), &src_top[src_index]);
    svuint8_t bottomsrc = svld1(svptrue_b8(), &src_bottom[src_index]);
    return common_vector_path_r1(pcit, yfrac, topsrc, bottomsrc);
  }

  svuint8_t remaining_path_r1(const PrecalcIterator<kRatio> &pcit,
                              uint16_t yfrac, const uint8_t *src_top,
                              const uint8_t *src_bottom) const
      KLEIDICV_STREAMING {
    uint64_t src_index = *pcit.src_index_ptr_;
    svbool_t pg = svwhilelt_b8(src_index, src_width_ * kChannels);
    svuint8_t topsrc = svld1(pg, &src_top[src_index]);
    svuint8_t bottomsrc = svld1(pg, &src_bottom[src_index]);
    return common_vector_path_r1(pcit, yfrac, topsrc, bottomsrc);
  }

  svuint8_t common_vector_path_r2(
      const PrecalcIterator<kRatio> &pcit, uint16_t yfrac, svuint8x2_t topsrc,
      svuint8x2_t bottomsrc) const KLEIDICV_STREAMING {
    svuint8_t vsx0_idx = svld1(svptrue_b8(), pcit.idx0_ptr_);
    svuint8_t vsx1_idx = svld1(svptrue_b8(), pcit.idx1_ptr_);
    svuint8_t a = svtbl2_u8(topsrc, vsx0_idx);
    svuint8_t b = svtbl2_u8(topsrc, vsx1_idx);
    svuint8_t c = svtbl2_u8(bottomsrc, vsx0_idx);
    svuint8_t d = svtbl2_u8(bottomsrc, vsx1_idx);
    return interpolate(pcit, yfrac, a, b, c, d);
  }

  svuint8_t vector_path_r2(const PrecalcIterator<kRatio> &pcit, uint16_t yfrac,
                           const uint8_t *src_top,
                           const uint8_t *src_bottom) const KLEIDICV_STREAMING {
    // Load 2*step elements, that's enough for 1/2 < scale < 1.0
    uint64_t src_index = *pcit.src_index_ptr_;
    svuint8x2_t topsrc = load8x2_u8(&src_top[src_index]);
    svuint8x2_t bottomsrc = load8x2_u8(&src_bottom[src_index]);
    return common_vector_path_r2(pcit, yfrac, topsrc, bottomsrc);
  }

  svuint8_t remaining_path_r2(const PrecalcIterator<kRatio> &pcit,
                              uint16_t yfrac, const uint8_t *src_top,
                              const uint8_t *src_bottom) const
      KLEIDICV_STREAMING {
    // Load 2*step elements, that's enough for 1/2 < scale < 1.0
    uint64_t src_index = *pcit.src_index_ptr_;
    svuint8x2_t topsrc = load8x2_while_u8(&src_top[src_index], src_index,
                                          src_width_ * kChannels);
    svuint8x2_t bottomsrc = load8x2_while_u8(&src_bottom[src_index], src_index,
                                             src_width_ * kChannels);
    return common_vector_path_r2(pcit, yfrac, topsrc, bottomsrc);
  }

  svuint8_t common_vector_path_r3(
      const PrecalcIterator<kRatio> &pcit, uint16_t yfrac, svuint8x3_t topsrc,
      svuint8x3_t bottomsrc, const uint8_t *src_top_ptr,
      const uint8_t *src_bottom_ptr) const KLEIDICV_STREAMING {
    svuint8_t vsx0_idx = svld1(svptrue_b8(), pcit.idx0_ptr_);
    svuint8_t vsx1_idx = svld1(svptrue_b8(), pcit.idx1_ptr_);
    if constexpr (kSetRightmostLanes) {
      // Make room for the last one, which is loaded separately and put together
      // using EXT, for better performance
      vsx1_idx = svinsr_n_u8(vsx1_idx, 0);
    }
    svuint8_t a =
        svtbl2_u8(svcreate2(svget3(topsrc, 0), svget3(topsrc, 1)), vsx0_idx);
    svuint8_t b =
        svtbl2_u8(svcreate2(svget3(topsrc, 0), svget3(topsrc, 1)), vsx1_idx);
    svuint8_t c = svtbl2_u8(
        svcreate2(svget3(bottomsrc, 0), svget3(bottomsrc, 1)), vsx0_idx);
    svuint8_t d = svtbl2_u8(
        svcreate2(svget3(bottomsrc, 0), svget3(bottomsrc, 1)), vsx1_idx);

    vsx0_idx =
        svsub_n_u8_x(svptrue_b8(), vsx0_idx, static_cast<uint8_t>(2 * kStep_));
    vsx1_idx =
        svsub_n_u8_x(svptrue_b8(), vsx1_idx, static_cast<uint8_t>(2 * kStep_));
    a = svtbx_u8(a, svget3(topsrc, 2), vsx0_idx);
    b = svtbx_u8(b, svget3(topsrc, 2), vsx1_idx);
    c = svtbx_u8(c, svget3(bottomsrc, 2), vsx0_idx);
    d = svtbx_u8(d, svget3(bottomsrc, 2), vsx1_idx);
    if constexpr (kSetRightmostLanes) {
      svbool_t pg = svptrue_pat_b8(SV_VL1);
      ptrdiff_t last_index =
          std::min(src_width_ * kChannels - 1,
                   static_cast<size_t>(pcit.idx1_ptr_[kStep_ - 1] +
                                       *pcit.src_index_ptr_));
      b = svext_u8(b, svld1_u8(pg, src_top_ptr + last_index), 1UL);
      d = svext_u8(d, svld1_u8(pg, src_bottom_ptr + last_index), 1UL);
    }
    return interpolate(pcit, yfrac, a, b, c, d);
  }

  svuint8_t vector_path_r3(const PrecalcIterator<kRatio> &pcit, uint16_t yfrac,
                           const uint8_t *src_top,
                           const uint8_t *src_bottom) const KLEIDICV_STREAMING {
    // Load 3*2*step elements, that's enough for 1/3 < scale < 1.0
    uint64_t src_index = *pcit.src_index_ptr_;
    svuint8x3_t topsrc = load8x3_u8(&src_top[src_index]);
    svuint8x3_t bottomsrc = load8x3_u8(&src_bottom[src_index]);
    return common_vector_path_r3(pcit, yfrac, topsrc, bottomsrc, src_top,
                                 src_bottom);
  }

  svuint8_t remaining_path_r3(const PrecalcIterator<kRatio> &pcit,
                              uint16_t yfrac, const uint8_t *src_top,
                              const uint8_t *src_bottom) const
      KLEIDICV_STREAMING {
    // Load 3*step elements, that's enough for 1/3 < scale < 1.0
    uint64_t src_index = *pcit.src_index_ptr_;
    svuint8x3_t topsrc = load8x3_while_u8(&src_top[src_index], src_index,
                                          src_width_ * kChannels);
    svuint8x3_t bottomsrc = load8x3_while_u8(&src_bottom[src_index], src_index,
                                             src_width_ * kChannels);
    return common_vector_path_r3(pcit, yfrac, topsrc, bottomsrc, src_top,
                                 src_bottom);
  }

  void process_row(uint64_t dy) const KLEIDICV_STREAMING {
    int64_t sy_fixp = to_src_y(dy);
    ptrdiff_t sy = static_cast<ptrdiff_t>(sy_fixp >> kFixpBits);
    const ptrdiff_t max_sy = static_cast<ptrdiff_t>(src_height_ - 1);
    ptrdiff_t sy_top = std::clamp(sy, ptrdiff_t{0}, max_sy);
    ptrdiff_t sy_bottom = std::clamp(sy + 1, ptrdiff_t{0}, max_sy);
    const uint8_t *src_top = &src_rows_.at(sy_top)[0];
    const uint8_t *src_bottom = &src_rows_.at(sy_bottom)[0];
    uint8_t *dst = &dst_rows_.at(static_cast<ptrdiff_t>(dy))[0];
    uint8_t *dst_end = dst + dst_width_ * kChannels;
    // Convert the fractional part to the Q15 interpolation weight.
    uint16_t yfrac = interpolation_fraction(sy_fixp);
    auto pcit = precalc_.begin();
    while (pcit.index_ + 1 < precalc_.n_iterations_2x()) {
      svuint8_t res0, res1;
      if constexpr (kRatio == 3) {
        res0 = vector_path_r3(pcit, yfrac, src_top, src_bottom);
        ++pcit;
        res1 = vector_path_r3(pcit, yfrac, src_top, src_bottom);
        ++pcit;
      } else if constexpr (kRatio == 2) {
        res0 = vector_path_r2(pcit, yfrac, src_top, src_bottom);
        ++pcit;
        res1 = vector_path_r2(pcit, yfrac, src_top, src_bottom);
        ++pcit;
      } else {
        static_assert(kRatio == 1);
        res0 = vector_path_r1(pcit, yfrac, src_top, src_bottom);
        ++pcit;
        res1 = vector_path_r1(pcit, yfrac, src_top, src_bottom);
        ++pcit;
      }
#if KLEIDICV_TARGET_SME2
      svst1(svptrue_c8(), dst, svcreate2(res0, res1));
#else
      svst1(svptrue_b8(), dst, res0);
      svst1_vnum(svptrue_b8(), dst, 1, res1);
#endif  // KLEIDICV_TARGET_SME2
      dst += 2 * kStep_;
    }

    // similar to above, but only a single vector path and with predicates
    while (pcit.index_ < precalc_.n_iterations()) {
      svbool_t pgdst = svwhilelt_b8_s64(0L, dst_end - dst);
      svuint8_t res;
      if constexpr (kRatio == 3) {
        res = remaining_path_r3(pcit, yfrac, src_top, src_bottom);
      } else if constexpr (kRatio == 2) {
        res = remaining_path_r2(pcit, yfrac, src_top, src_bottom);
      } else {
        static_assert(kRatio == 1);
        res = remaining_path_r1(pcit, yfrac, src_top, src_bottom);
      }
      svst1(pgdst, dst, res);
      ++pcit;
      dst += kStep_;
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
  const ptrdiff_t kStep_;
  PrecalcIndicesFractions<kRatio, kChannels, kUpsize> precalc_;
};

}  // namespace resize_generic_u8

// ratio: number of vectors to load and resize to 1 vector
// - supported ratios:
//   - kUpsize = false: 2, 3
//   - kUpsize = true: 1, 2
// - supported channels: 1, 2, 3, 4
template <int kRatio, int kChannels, bool kUpsize>
kleidicv_error_t kleidicv_resize_generic_stripe_u8_sc(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    size_t y_begin, size_t y_end,
    uint8_t *dst,  // NOLINT
    size_t dst_stride, size_t dst_width, size_t dst_height) KLEIDICV_STREAMING {
  if constexpr (kChannels == 3 && kRatio == 3 && !kUpsize) {
    double inverse_scale =
        static_cast<double>(src_width) / static_cast<double>(dst_width);
    if (inverse_scale >= 2.8) {
      // Rightmost lane(s) of b and d vectors need to be set separately, as
      // the loaded src registers don't have the last pixels
      resize_generic_u8::ResizeGenericU8Operation<kRatio, kChannels, true,
                                                  kUpsize>
          operation(src, src_stride, src_width, src_height, y_begin, y_end, dst,
                    dst_stride, dst_width, dst_height);
      return operation.process_rows();
    }
  }

  resize_generic_u8::ResizeGenericU8Operation<kRatio, kChannels, false, kUpsize>
      operation(src, src_stride, src_width, src_height, y_begin, y_end, dst,
                dst_stride, dst_width, dst_height);
  return operation.process_rows();
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_RESIZE_LINEAR_GENERIC_SC_H
