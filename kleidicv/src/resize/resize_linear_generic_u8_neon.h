// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_RESIZE_RESIZE_LINEAR_GENERIC_U8_NEON_H
#define KLEIDICV_RESIZE_RESIZE_LINEAR_GENERIC_U8_NEON_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

#include "kleidicv/ctypes.h"
#include "kleidicv/neon.h"
#include "kleidicv/utils.h"

namespace kleidicv::neon::resize_linear_generic_u8 {

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

static constexpr ptrdiff_t kFixpBits = 16;
static constexpr ptrdiff_t kFixpHalf = (1UL << (kFixpBits - 1));
static constexpr ptrdiff_t kStep = kVectorLength / sizeof(uint8_t);
static constexpr ptrdiff_t kHalfStep = kStep / 2;

struct FullVectorInterpolationConstants {
  uint8_t idx0[kStep];
  uint8_t idx1[kStep];
  uint16_t xfrac[kStep];
  ptrdiff_t src_element_index;
};

struct HalfVectorInterpolationConstants {
  uint8_t idx0[kHalfStep];
  uint8_t idx1[kHalfStep];
  uint16_t xfrac[kHalfStep];
  ptrdiff_t src_element_index;
  ptrdiff_t dst_element_index;
};

struct VectorPathNums {
  size_t two_x;
  size_t half;

  explicit VectorPathNums(std::pair<size_t, size_t> sizes)
      : two_x{sizes.first}, half{sizes.second} {}
};

template <int kRatio>
using SrcVecType = std::conditional_t<
    kRatio == 1, uint8x16_t,
    std::conditional_t<kRatio == 2, uint8x16x2_t, uint8x16x3_t>>;

template <bool kLoadTwo>
using HalfSrcVecType = std::conditional_t<kLoadTwo, uint8x16x2_t, uint8x16_t>;

template <typename T = size_t>
static T rounding_div(size_t nom, size_t denom) {
  return static_cast<T>((nom + denom / 2) / denom);
}

// Scale coordinate using this formula, so the center is aligned:
//   source_x = (destination_x + 0.5) / scale - 0.5;
//   plus 1/256/2 for later rounding the fractional part to 8bits
// Note: return value is in fixed point format using kFixpBits for the
// fractional part
static inline int64_t aligned_scale(size_t x, size_t nom, size_t denom) {
  return rounding_div<int64_t>(((x << kFixpBits) + kFixpHalf) * nom, denom) -
         kFixpHalf + (1 << (kFixpBits - 9));
}

class RowInterpolationConstants {
 public:
  // Constructible only through create
  RowInterpolationConstants() = delete;

  static std::variant<RowInterpolationConstants, kleidicv_error_t> create(
      VectorPathNums num_of_vector_paths) {
    uint8_t *allocation = static_cast<uint8_t *>(malloc(
        num_of_vector_paths.two_x * 2 *
            sizeof(FullVectorInterpolationConstants) +
        num_of_vector_paths.half * sizeof(HalfVectorInterpolationConstants)));
    if (!allocation) {
      return KLEIDICV_ERROR_ALLOCATION;
    }

    return RowInterpolationConstants{num_of_vector_paths, allocation};
  }

  VectorPathNums num_of_vector_paths() const { return num_of_vector_paths_; }

  FullVectorInterpolationConstants *full_vector_constants_array() const {
    return full_vector_constants_array_;
  }

  HalfVectorInterpolationConstants *half_vector_constants_array() const {
    return half_vector_constants_array_;
  }

 private:
  RowInterpolationConstants(VectorPathNums num_of_vector_paths, uint8_t *buffer)
      : buffer_{buffer, &std::free},
        full_vector_constants_array_{
            reinterpret_cast<FullVectorInterpolationConstants *>(buffer)},
        half_vector_constants_array_{
            reinterpret_cast<HalfVectorInterpolationConstants *>(
                full_vector_constants_array_ +
                (num_of_vector_paths.two_x * 2))},
        num_of_vector_paths_{num_of_vector_paths} {}

  using FreeDeleter = decltype(&std::free);
  std::unique_ptr<uint8_t, FreeDeleter> buffer_;
  FullVectorInterpolationConstants *const full_vector_constants_array_;
  HalfVectorInterpolationConstants *const half_vector_constants_array_;
  const VectorPathNums num_of_vector_paths_;
};

template <int kRatio, int kChannels>
class RowInterpolationConstantsGeneratorBase {
 protected:
  RowInterpolationConstantsGeneratorBase(size_t src_width, size_t dst_width)
      : src_width_{src_width},
        dst_width_{dst_width},
        vsidx_tbl_{2, 6, 10, 14, 18, 22, 26, 30},
        vsfrac_tbl_{1,  255, 5,  255, 9,  255, 13, 255,
                    17, 255, 21, 255, 25, 255, 29, 255} {}

  std::pair<size_t, size_t> calculate_num_of_vector_paths() {
    size_t two_x = ((src_width_ * kChannels) >= sizeof(SrcVecType<kRatio>))
                       ? ((dst_width_ * kChannels) / (2 * kStep))
                       : 0;

    size_t remaining_dx_after_2x_cycle =
        (dst_width_ * kChannels) - (two_x * 2 * kStep);
    size_t half = align_up(remaining_dx_after_2x_cycle, kHalfStep) / kHalfStep;
    return {two_x, half};
  }

  // Scale destination x coordinate to source x coordinate, into fixed-point,
  // without center correction
  uint32_t scale_x(size_t dx) const {
    return rounding_div<uint32_t>(((dx * src_width_) << kFixpBits), dst_width_);
  }

  int64_t to_src_x(size_t dx) const {
    return aligned_scale(dx, src_width_, dst_width_);
  }

  uint64_t clamp_src_x(int64_t sx, int64_t max_sx) const {
    return static_cast<uint64_t>(std::clamp(sx, int64_t{0}, max_sx));
  }

  const size_t src_width_;
  const size_t dst_width_;
  const uint8x8_t vsidx_tbl_;
  const uint8x16_t vsfrac_tbl_;
};

template <int kRatio, int kChannels, bool kUpsize>
class RowInterpolationConstantsGenerator final
    : RowInterpolationConstantsGeneratorBase<kRatio, kChannels> {
 public:
  static constexpr size_t kFullVectorSrcReadSize = sizeof(SrcVecType<kRatio>);
  static constexpr bool kLoadTwo = (kRatio == 3 || kChannels == 3) && !kUpsize;
  static constexpr size_t kHalfVectorSrcReadSize =
      sizeof(HalfSrcVecType<kLoadTwo>);
  using Base = RowInterpolationConstantsGeneratorBase<kRatio, kChannels>;
  RowInterpolationConstantsGenerator(size_t src_width, size_t dst_width)
      : Base{src_width, dst_width},
        // These starting values are not aligned to center. The center alignment
        // must be added only once. When added to a center-aligned source_x
        // value, the result will be center-aligned.
        vsx0_0_{Base::scale_x(0), Base::scale_x(1 / kChannels),
                Base::scale_x(2 / kChannels), Base::scale_x(3 / kChannels)},
        vsx0_1_{Base::scale_x(4 / kChannels), Base::scale_x(5 / kChannels),
                Base::scale_x(6 / kChannels), Base::scale_x(7 / kChannels)},
        vsx0_2_{Base::scale_x(8 / kChannels), Base::scale_x(9 / kChannels),
                Base::scale_x(10 / kChannels), Base::scale_x(11 / kChannels)},
        vsx0_3_{Base::scale_x(12 / kChannels), Base::scale_x(13 / kChannels),
                Base::scale_x(14 / kChannels), Base::scale_x(15 / kChannels)} {}

  std::variant<RowInterpolationConstants, kleidicv_error_t> operator()() {
    VectorPathNums v{Base::calculate_num_of_vector_paths()};
    auto row_interpolation_constants_variant =
        RowInterpolationConstants::create(v);
    if (std::holds_alternative<kleidicv_error_t>(
            row_interpolation_constants_variant)) {
      // Creation failed with some error, return with the variant as it is
      return row_interpolation_constants_variant;
    }
    auto &row_interpolation_constants = *std::get_if<RowInterpolationConstants>(
        &row_interpolation_constants_variant);

    size_t dx = 0;
    int64_t sx_fixp = 0;

    // Calculate constants for full vectors

    // Maximum source coordinate for full vector path
    const int64_t max_sx_fullvector = static_cast<int64_t>(
        (Base::src_width_ * kChannels - kFullVectorSrcReadSize) / kChannels);

    // Difference in source x coordinate for one vector path
    const int64_t sx_fixp_vector_step = rounding_div<int64_t>(
        (Base::src_width_ * kStep / kChannels) << kFixpBits, Base::dst_width_);

    for (size_t i = 0;
         i < row_interpolation_constants.num_of_vector_paths().two_x; ++i) {
      // Repeatedly adding sx_fixp_vector_step is faster than scaling dx to sx,
      // but it accumulates fixed-point error; periodic recalibration resets it.
      // The maximum per-addition error of sx_fixp_vector_step is 0.5 / (1 <<
      // 16). Only the upper 8 bits of the 16-bit fractional part are used for
      // interpolation, so once the accumulated error reaches 1 / (1 << 8), it
      // can affect later stages. This corresponds to 512 additions. Since two
      // additions are performed per cycle, we recalibrate every 256 cycles,
      // calculated by this mask.
      constexpr uint64_t kRecalibrateCycleMask = ((1 << 8) - 1);
      if ((i & kRecalibrateCycleMask) == 0) {
        sx_fixp = Base::to_src_x(dx);
      }

      // Pull back sx if it would overrun
      int64_t sx_candidate = sx_fixp >> kFixpBits;
      int64_t sx_base = Base::clamp_src_x(sx_candidate, max_sx_fullvector);
      calculate_indices_fractions_base_fullvector(
          row_interpolation_constants.full_vector_constants_array()[i * 2],
          sx_base, sx_fixp);
      sx_fixp += sx_fixp_vector_step;
      dx += kStep / kChannels;

      // Pull back sx if it would overrun
      sx_candidate = sx_fixp >> kFixpBits;
      sx_base = Base::clamp_src_x(sx_candidate, max_sx_fullvector);
      calculate_indices_fractions_base_fullvector(
          row_interpolation_constants
              .full_vector_constants_array()[(i * 2) + 1],
          sx_base, sx_fixp);
      sx_fixp += sx_fixp_vector_step;
      dx += kStep / kChannels;
    }

    // Calculate constants for half vectors

    sx_fixp = Base::to_src_x(dx);

    // Difference in source x coordinate for one destination pixel
    const uint64_t sx_fixp_one_dst_pixel =
        rounding_div(Base::src_width_ << kFixpBits, Base::dst_width_);
    // Maximum source coordinate for half vector path
    const uint64_t max_sx_half =
        (Base::src_width_ * kChannels - kHalfVectorSrcReadSize) / kChannels;
    // Maximum destination coordinate for half vector path
    const uint64_t max_dx_half = Base::dst_width_ - (kHalfStep / kChannels);
    // Difference in source x coordinate for the half vector path
    const uint64_t sx_fixp_half_step =
        rounding_div((Base::src_width_ * kHalfStep / kChannels) << kFixpBits,
                     Base::dst_width_);

    for (size_t i = 0;
         i < row_interpolation_constants.num_of_vector_paths().half; ++i) {
      // If (dx + half vector length) would overrun the buffer, pull it back
      size_t dx_pulled_back = std::min(dx, static_cast<size_t>(max_dx_half));
      // Pull back sx if dx was pulled back
      sx_fixp -=
          static_cast<int64_t>(dx - dx_pulled_back) * sx_fixp_one_dst_pixel;
      dx = dx_pulled_back;
      // If (sx_base + reading length) would overrun the buffer, pull sx back
      // again
      int64_t sx_candidate = sx_fixp >> kFixpBits;
      int64_t sx_base = Base::clamp_src_x(sx_candidate, max_sx_half);
      calculate_indices_fractions_base_halfvector(
          row_interpolation_constants.half_vector_constants_array()[i], sx_base,
          sx_fixp, dx);

      dx += kHalfStep / kChannels;
      sx_fixp += sx_fixp_half_step;
    }

    return row_interpolation_constants_variant;
  }

 private:
  void calculate_indices_fractions_base_fullvector(
      FullVectorInterpolationConstants &constants, int64_t sx_base,
      int64_t sx_fixp) {
    int32_t xfrac0 = static_cast<int32_t>(sx_fixp - (sx_base << kFixpBits));
    ptrdiff_t src_element_index = static_cast<ptrdiff_t>(sx_base * kChannels);
    int32x4_t vfrac = vdupq_n_s32(xfrac0);
    // Calculate x coordinate delta from sx_base, the integer part of source x
    int32x4x2_t vsx_delta_lo, vsx_delta_hi;
    vsx_delta_lo.val[0] = vaddq_s32(vsx0_0_, vfrac);
    vsx_delta_lo.val[1] = vaddq_s32(vsx0_1_, vfrac);
    vsx_delta_hi.val[0] = vaddq_s32(vsx0_2_, vfrac);
    vsx_delta_hi.val[1] = vaddq_s32(vsx0_3_, vfrac);
    int8x8_t idx_lo =
        vqtbl2_s8(vreinterpretq_s8_x2(vsx_delta_lo), Base::vsidx_tbl_);
    int8x8_t idx_hi =
        vqtbl2_s8(vreinterpretq_s8_x2(vsx_delta_hi), Base::vsidx_tbl_);
    int8x16_t vsx0_idx = vcombine_u8(idx_lo, idx_hi);
    int8x16_t vsx1_idx = vaddq_s8(vsx0_idx, vdupq_n_s8(1));

    if constexpr (kUpsize) {
      // clamp indices
      vsx0_idx = vmaxq_s8(vsx0_idx, vdupq_n_s8(0));
      vsx1_idx = vmaxq_s8(vsx1_idx, vdupq_n_s8(0));
      int8x16_t vmaxsx = vdupq_n_s8((kFullVectorSrcReadSize - 1) / kChannels);
      vsx0_idx = vminq_s8(vsx0_idx, vmaxsx);
      vsx1_idx = vminq_s8(vsx1_idx, vmaxsx);
    }

    if constexpr (kChannels > 1) {
      // Good for 2 and 4 channels, and does not add branches
      constexpr uint8_t kChannelShift = kChannels / 2;
      vsx0_idx = vshlq_n_u8(vsx0_idx, kChannelShift);
      vsx1_idx = vshlq_n_u8(vsx1_idx, kChannelShift);
      uint8x16_t v_channel_offsets = vreinterpretq_u8_u32(
          vdupq_n_u32(kChannels == 4 ? 0x03020100U : 0x01000100));
      vsx0_idx = vaddq_u8(vsx0_idx, v_channel_offsets);
      vsx1_idx = vaddq_u8(vsx1_idx, v_channel_offsets);
    }

    constants.src_element_index = src_element_index;
    vst1q(constants.idx0, vsx0_idx);
    vst1q(constants.idx1, vsx1_idx);
    uint16x8x2_t vsxfrac;
    // It is safe to reinterpret delta to unsigned, because it is only the
    // integer part that's affected by signedness, fraction is always between
    // [0, 1), so these values are between 0 and 255
    // e.g.  -0.3 = -1 (integer part) + 0.7 (fractional part)
    vsxfrac.val[0] = vreinterpretq_u16_u8(
        vqtbl2q_u8(vreinterpretq_u8_x2(vsx_delta_lo), Base::vsfrac_tbl_));
    vsxfrac.val[1] = vreinterpretq_u16_u8(
        vqtbl2q_u8(vreinterpretq_u8_x2(vsx_delta_hi), Base::vsfrac_tbl_));
    VecTraits<uint16_t>::store(vsxfrac, constants.xfrac);
  }

  void calculate_indices_fractions_base_halfvector(
      HalfVectorInterpolationConstants &constants, int64_t sx_base,
      int64_t sx_fixp, size_t dx) {
    int32_t xfrac0 = static_cast<int32_t>(sx_fixp - sx_base * (1 << kFixpBits));
    ptrdiff_t src_element_index = static_cast<ptrdiff_t>(sx_base * kChannels);
    int32x4_t vfrac = vdupq_n_s32(xfrac0);
    int32x4x2_t vsx_delta;
    vsx_delta.val[0] = vaddq_s32(vsx0_0_, vfrac);
    vsx_delta.val[1] = vaddq_s32(vsx0_1_, vfrac);
    int8x8_t vsx0_idx =
        vqtbl2_s8(vreinterpretq_s8_x2(vsx_delta), Base::vsidx_tbl_);
    int8x8_t vsx1_idx = vadd_s8(vsx0_idx, vdup_n_s8(1));

    if constexpr (kUpsize) {
      // clamp indices
      vsx0_idx = vmax_s8(vsx0_idx, vdup_n_s8(0));
      vsx1_idx = vmax_s8(vsx1_idx, vdup_n_s8(0));
      int8x8_t vmaxsx = vdup_n_s8((kHalfVectorSrcReadSize - 1) / kChannels);
      vsx0_idx = vmin_s8(vsx0_idx, vmaxsx);
      vsx1_idx = vmin_s8(vsx1_idx, vmaxsx);
    }

    if constexpr (kChannels > 1) {
      // Good for 2 and 4 channels, and does not add branches
      constexpr uint8_t kChannelShift = kChannels / 2;
      vsx0_idx = vshl_n_u8(vsx0_idx, kChannelShift);
      vsx1_idx = vshl_n_u8(vsx1_idx, kChannelShift);
      uint8x8_t v_channel_offsets = vreinterpret_u8_u32(
          vdup_n_u32(kChannels == 4 ? 0x03020100U : 0x01000100));
      vsx0_idx = vadd_u8(vsx0_idx, v_channel_offsets);
      vsx1_idx = vadd_u8(vsx1_idx, v_channel_offsets);
    }

    constants.src_element_index = src_element_index;
    vst1(constants.idx0, vsx0_idx);
    vst1(constants.idx1, vsx1_idx);
    uint16x8_t vsxfrac = vreinterpretq_u16_u8(
        vqtbl2q_u8(vreinterpretq_u8_x2(vsx_delta), Base::vsfrac_tbl_));
    VecTraits<uint16_t>::store(vsxfrac, constants.xfrac);
    constants.dst_element_index = static_cast<ptrdiff_t>(dx * kChannels);
  }

  const uint32x4_t vsx0_0_;
  const uint32x4_t vsx0_1_;
  const uint32x4_t vsx0_2_;
  const uint32x4_t vsx0_3_;
};

template <int kRatio, bool kUpsize>
class RowInterpolationConstantsGenerator<kRatio, 3, kUpsize> final
    : RowInterpolationConstantsGeneratorBase<kRatio, 3> {
 public:
  using Base = RowInterpolationConstantsGeneratorBase<kRatio, 3>;
  RowInterpolationConstantsGenerator(size_t src_width, size_t dst_width)
      : Base{src_width, dst_width},
        vsx_r_{vreinterpretq_s32_x4(
            uint32x4x4_t{Base::scale_x(0), Base::scale_x(0), Base::scale_x(0),
                         Base::scale_x(1), Base::scale_x(1), Base::scale_x(1),
                         Base::scale_x(2), Base::scale_x(2), Base::scale_x(2),
                         Base::scale_x(3), Base::scale_x(3), Base::scale_x(3),
                         Base::scale_x(4), Base::scale_x(4), Base::scale_x(4),
                         Base::scale_x(5)})},
        vsx_channel_offsets_r_{0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0},
        vsx_g_{vreinterpretq_s32_x4(
            uint32x4x4_t{Base::scale_x(5), Base::scale_x(5), Base::scale_x(6),
                         Base::scale_x(6), Base::scale_x(6), Base::scale_x(7),
                         Base::scale_x(7), Base::scale_x(7), Base::scale_x(8),
                         Base::scale_x(8), Base::scale_x(8), Base::scale_x(9),
                         Base::scale_x(9), Base::scale_x(9), Base::scale_x(10),
                         Base::scale_x(10)})},
        vsx_channel_offsets_g_{1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1},
        vsx_b_{vreinterpretq_s32_x4(uint32x4x4_t{
            Base::scale_x(10), Base::scale_x(11), Base::scale_x(11),
            Base::scale_x(11), Base::scale_x(12), Base::scale_x(12),
            Base::scale_x(12), Base::scale_x(13), Base::scale_x(13),
            Base::scale_x(13), Base::scale_x(14), Base::scale_x(14),
            Base::scale_x(14), Base::scale_x(15), Base::scale_x(15),
            Base::scale_x(15)})},
        vsx_channel_offsets_b_{2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2},
        sx_fixp_one_dst_pixel_{static_cast<int64_t>(
            rounding_div(src_width << kFixpBits, dst_width))} {}

  std::variant<RowInterpolationConstants, kleidicv_error_t> operator()() {
    static constexpr size_t kFullVectorSrcReadSize = sizeof(SrcVecType<kRatio>);
    static constexpr size_t kHalfVectorSrcReadSize =
        sizeof(HalfSrcVecType<!kUpsize>);
    VectorPathNums v{Base::calculate_num_of_vector_paths()};
    auto row_interpolation_constants_variant =
        RowInterpolationConstants::create(v);
    if (std::holds_alternative<kleidicv_error_t>(
            row_interpolation_constants_variant)) {
      // Creation failed with some error, return with the variant as it is
      return row_interpolation_constants_variant;
    }
    auto &row_interpolation_constants = *std::get_if<RowInterpolationConstants>(
        &row_interpolation_constants_variant);

    size_t dst_element_index = 0;
    int64_t sx_fixp{};

    // Calculate constants for full vectors

    size_t num_of_full_vector_constants =
        row_interpolation_constants.num_of_vector_paths().two_x * 2;
    if (num_of_full_vector_constants > 0) {
      size_t handled_full_vector_paths = 0;

      // Maximum source coordinate for full vector path
      const int64_t max_src_base_index = static_cast<int64_t>(
          saturating_sub(Base::src_width_ * kChannels, kFullVectorSrcReadSize));

      const int64_t sx_fixp_triplet_step = rounding_div<int64_t>(
          Base::src_width_ << (kFixpBits + 4), Base::dst_width_);

      sx_fixp = Base::to_src_x(0);
      unsigned recalibrate_cnt = 0;
      while (handled_full_vector_paths < num_of_full_vector_constants) {
        // Repeatedly adding the 16-destination-pixel step is faster than
        // scaling dx to sx, but it accumulates fixed-point error; periodic
        // recalibration resets it. The maximum per-addition error is
        // 0.5 / (1 << 16). Only the upper 8 bits of the 16-bit fractional
        // part are used for interpolation, so once the accumulated error
        // reaches 1 / (1 << 8), it can affect later stages. This corresponds
        // to 512 additions.
        if (recalibrate_cnt == 512) {
          sx_fixp = Base::to_src_x(dst_element_index / 3);
          recalibrate_cnt = 0;
        } else {
          recalibrate_cnt++;
        }

        fill_full_constants_vectorially(
            row_interpolation_constants
                .full_vector_constants_array()[handled_full_vector_paths],
            vsx_r_, vsx_channel_offsets_r_, sx_fixp, max_src_base_index);
        handled_full_vector_paths++;
        dst_element_index += kStep;
        if (handled_full_vector_paths == num_of_full_vector_constants) {
          break;
        }

        fill_full_constants_vectorially(
            row_interpolation_constants
                .full_vector_constants_array()[handled_full_vector_paths],
            vsx_g_, vsx_channel_offsets_g_, sx_fixp, max_src_base_index);
        handled_full_vector_paths++;
        dst_element_index += kStep;
        if (handled_full_vector_paths == num_of_full_vector_constants) {
          break;
        }

        fill_full_constants_vectorially(
            row_interpolation_constants
                .full_vector_constants_array()[handled_full_vector_paths],
            vsx_b_, vsx_channel_offsets_b_, sx_fixp, max_src_base_index);
        handled_full_vector_paths++;
        dst_element_index += kStep;

        sx_fixp += sx_fixp_triplet_step;
      }
    }

    // Calculate constants for half vectors

    // Maximum source coordinate for half vector path
    const int64_t max_src_base_index = static_cast<int64_t>(
        saturating_sub(Base::src_width_ * kChannels, kHalfVectorSrcReadSize));
    // Maximum destination coordinate for half vector path
    const uint64_t max_dst_index_half =
        (Base::dst_width_ * kChannels) - kHalfStep;

    for (size_t i = 0;
         i < row_interpolation_constants.num_of_vector_paths().half; ++i) {
      auto &constants =
          row_interpolation_constants.half_vector_constants_array()[i];

      // If (dst index + half vector length) would overrun the buffer, pull it
      // back
      dst_element_index =
          std::min(dst_element_index, static_cast<size_t>(max_dst_index_half));

      size_t dx = dst_element_index / kChannels;
      unsigned in_pixel_index = dst_element_index % kChannels;
      int64_t sx_fixp_pixel0 = Base::to_src_x(dx);
      int64_t sx_fixp_pixel1 = Base::to_src_x(dx + 1);
      int64_t src_element_index_pixel0 =
          ((sx_fixp_pixel0 >> kFixpBits) * kChannels) + in_pixel_index;
      int64_t src_element_index_pixel1 =
          (sx_fixp_pixel1 >> kFixpBits) * kChannels;

      // Pull back src if it would overrun
      int64_t src_element_base = std::clamp(
          std::min(src_element_index_pixel0, src_element_index_pixel1),
          int64_t{0}, max_src_base_index);

      fill_half_constants_scalarly(constants, dst_element_index, in_pixel_index,
                                   src_element_index_pixel0, src_element_base,
                                   sx_fixp_pixel0);

      dst_element_index += kHalfStep;
    }

    return row_interpolation_constants_variant;
  }

 private:
  void fill_full_constants_vectorially(
      FullVectorInterpolationConstants &constants, const int32x4x4_t &vsx,
      const int8x16_t &vsx_channel_offsets, int64_t sx_fixp,
      int64_t max_src_base_index) {
    int64_t sx_base = sx_fixp >> kFixpBits;
    int64_t src_element_base = sx_base * static_cast<int64_t>(kChannels);
    // Create x coordinate for all lanes.
    int32_t xfrac0 = static_cast<int32_t>(sx_fixp & ((1 << kFixpBits) - 1));
    int32x4_t vfrac = vdupq_n_s32(xfrac0);
    int32x4x2_t vsx_delta_lo, vsx_delta_hi;
    vsx_delta_lo.val[0] = vaddq_s32(vsx.val[0], vfrac);
    vsx_delta_lo.val[1] = vaddq_s32(vsx.val[1], vfrac);
    vsx_delta_hi.val[0] = vaddq_s32(vsx.val[2], vfrac);
    vsx_delta_hi.val[1] = vaddq_s32(vsx.val[3], vfrac);
    // The integer part makes the index
    int8x8_t idx_lo =
        vqtbl2_s8(vreinterpretq_s8_x2(vsx_delta_lo), Base::vsidx_tbl_);
    int8x8_t idx_hi =
        vqtbl2_s8(vreinterpretq_s8_x2(vsx_delta_hi), Base::vsidx_tbl_);
    int8x16_t vsx0_idx = vcombine_u8(idx_lo, idx_hi);
    int8x16_t vsx1_idx = vaddq_s8(vsx0_idx, vdupq_n_s8(1));

    if constexpr (kUpsize) {
      // Clamp coordinates
      // At this point the lanes contain x-coordinates based on sx_base.
      int8x16_t vminidx =
          vdupq_n_s8(saturating_cast<int64_t, int8_t>(-sx_base));
      vsx0_idx = vmaxq_s8(vsx0_idx, vminidx);
      vsx1_idx = vmaxq_s8(vsx1_idx, vminidx);
      int8x16_t vmaxidx = vdupq_n_s8(
          saturating_cast<size_t, int8_t>(Base::src_width_ - 1 - sx_base));
      vsx0_idx = vminq_s8(vsx0_idx, vmaxidx);
      vsx1_idx = vminq_s8(vsx1_idx, vmaxidx);
    }

    // One step in x means 3 steps in elements.
    vsx0_idx = vmulq_s8(vsx0_idx, vdupq_n_s8(kChannels));
    vsx1_idx = vmulq_s8(vsx1_idx, vdupq_n_s8(kChannels));

    // The channel offsets are relative to src_element_index_base.
    vsx0_idx = vaddq_s8(vsx0_idx, vsx_channel_offsets);
    vsx1_idx = vaddq_s8(vsx1_idx, vsx_channel_offsets);

    // Adjust the load address and the indices as well.
    int8_t idx0 = vgetq_lane_s8(vsx0_idx, 0);
    int8_t idx1 = vgetq_lane_s8(vsx0_idx, 1);
    int8_t idx2 = vgetq_lane_s8(vsx0_idx, 2);
    int8_t min_idx = std::min(std::min(idx0, idx1), idx2);
    ptrdiff_t final_base =
        std::clamp(src_element_base + min_idx, int64_t{0}, max_src_base_index);
    constants.src_element_index = final_base;
    int8_t adjustment = static_cast<int8_t>(final_base - src_element_base);
    vsx0_idx = vsubq_u8(vsx0_idx, vdupq_n_s8(adjustment));
    vsx1_idx = vsubq_u8(vsx1_idx, vdupq_n_s8(adjustment));
    vst1q(constants.idx0, vsx0_idx);
    vst1q(constants.idx1, vsx1_idx);

    // Get fraction from coordinate
    uint16x8x2_t vsxfrac;
    vsxfrac.val[0] = vreinterpretq_u16_u8(
        vqtbl2q_u8(vreinterpretq_u8_x2(vsx_delta_lo), Base::vsfrac_tbl_));
    vsxfrac.val[1] = vreinterpretq_u16_u8(
        vqtbl2q_u8(vreinterpretq_u8_x2(vsx_delta_hi), Base::vsfrac_tbl_));
    VecTraits<uint16_t>::store(vsxfrac, constants.xfrac);
  }

  void fill_half_constants_scalarly(HalfVectorInterpolationConstants &constants,
                                    size_t dst_element_index,
                                    unsigned in_pixel_index,
                                    int64_t src_element_index,
                                    int64_t src_element_base, int64_t sx_fixp) {
    constants.dst_element_index = static_cast<ptrdiff_t>(dst_element_index);
    constants.src_element_index = static_cast<ptrdiff_t>(src_element_base);

    // For indexing inside idx and xfrac arrays of
    // the interpolation constants
    auto sx_indices = [&](int64_t current_sx_fixp) {
      int64_t sx0 = current_sx_fixp >> kFixpBits;
      int64_t sx1 = sx0 + 1;
      if constexpr (kUpsize) {
        sx0 = std::clamp(sx0, int64_t{0},
                         static_cast<int64_t>(Base::src_width_ - 1));
        sx1 = std::clamp(sx1, int64_t{0},
                         static_cast<int64_t>(Base::src_width_ - 1));
      }
      return std::pair<int64_t, int64_t>{sx0 * kChannels, sx1 * kChannels};
    };

    unsigned j = 0;
    auto [src0, src1] = sx_indices(sx_fixp);
    uint16_t xfrac =
        (src_element_index < src_element_base)
            ? 0
            : (sx_fixp & ((1 << kFixpBits) - 1)) >> (kFixpBits / 2);

    for (; j < (kChannels - in_pixel_index); ++j) {
      constants.idx0[j] =
          static_cast<uint8_t>(src0 + in_pixel_index + j - src_element_base);
      constants.idx1[j] =
          static_cast<uint8_t>(src1 + in_pixel_index + j - src_element_base);
      constants.xfrac[j] = xfrac;
    }

    sx_fixp += sx_fixp_one_dst_pixel_;
    src_element_index =
        static_cast<int64_t>((sx_fixp >> kFixpBits) * kChannels);
    auto src_indices = sx_indices(sx_fixp);
    src0 = src_indices.first;
    src1 = src_indices.second;
    xfrac = (sx_fixp & ((1 << kFixpBits) - 1)) >> (kFixpBits / 2);

    constexpr size_t idx_frac_elem_num =
        sizeof(constants.xfrac) / sizeof(constants.xfrac[0]);

    while (j < idx_frac_elem_num) {
      // k is the index for the elements in one pixel
      for (unsigned k = 0; (j < idx_frac_elem_num) && (k < kChannels);
           ++j, ++k) {
        constants.idx0[j] = static_cast<uint8_t>(src0 + k - src_element_base);
        constants.idx1[j] = static_cast<uint8_t>(src1 + k - src_element_base);
        constants.xfrac[j] = xfrac;
      }
      sx_fixp += sx_fixp_one_dst_pixel_;
      src_element_index = (sx_fixp >> kFixpBits) * kChannels;
      src_indices = sx_indices(sx_fixp);
      src0 = src_indices.first;
      src1 = src_indices.second;
      xfrac = (src_element_index < src_element_base)
                  ? 0
                  : (sx_fixp & ((1 << kFixpBits) - 1)) >> (kFixpBits / 2);
    }
  }

  static constexpr unsigned kChannels = 3;
  const int32x4x4_t vsx_r_;
  const int8x16_t vsx_channel_offsets_r_;
  const int32x4x4_t vsx_g_;
  const int8x16_t vsx_channel_offsets_g_;
  const int32x4x4_t vsx_b_;
  const int8x16_t vsx_channel_offsets_b_;
  // Difference in source x coordinate for one destination pixel
  const int64_t sx_fixp_one_dst_pixel_;
};

template <int kRatio, int kChannels, bool kSetRightmostLanes = false,
          bool kUpsize = false>
class ResizeGenericU8Operation final {
 public:
  ResizeGenericU8Operation(const uint8_t *src, size_t src_stride,
                           size_t src_height, size_t y_begin, size_t y_end,
                           uint8_t *dst, size_t dst_stride, size_t dst_width,
                           size_t dst_height)
      : src_rows_{src, src_stride, kChannels},
        dst_rows_{dst, dst_stride, kChannels},
        src_height_{src_height},
        y_begin_{y_begin},
        y_end_{y_end},
        dst_length_{static_cast<ptrdiff_t>(dst_width * kChannels)},
        dst_height_{dst_height},
        hbuffer_{nullptr, std::free},
        hbuffer1_{nullptr},
        hbuffer2_{nullptr},
        hbuffer_sy_{-2} {}

  kleidicv_error_t process_rows_separated(
      RowInterpolationConstants &row_interpolation_constants) {
    uint8_t *allocation = static_cast<uint8_t *>(malloc(dst_length_ * 2));
    if (!allocation) {
      return KLEIDICV_ERROR_ALLOCATION;
    }
    hbuffer_.reset(allocation);
    hbuffer1_ = allocation;
    hbuffer2_ = allocation + dst_length_;
    // invalid value to force buffers processed first
    hbuffer_sy_ = -2;

    for (size_t dst_y = y_begin_; dst_y < y_end_; ++dst_y) {
      process_row_separated(dst_y, row_interpolation_constants);
    }
    return KLEIDICV_OK;
  }

  kleidicv_error_t process_rows(
      RowInterpolationConstants &row_interpolation_constants) {
    for (size_t dst_y = y_begin_; dst_y < y_end_; ++dst_y) {
      process_row(dst_y, row_interpolation_constants);
    }
    return KLEIDICV_OK;
  }

 private:
  int64_t to_src_y(size_t dy) const {
    return aligned_scale(dy, src_height_, dst_height_);
  }

  void process_row(size_t dy,
                   RowInterpolationConstants &row_interpolation_constants) {
    VectorPathNums num_of_vector_paths =
        row_interpolation_constants.num_of_vector_paths();
    auto *full_array =
        row_interpolation_constants.full_vector_constants_array();
    auto *half_array =
        row_interpolation_constants.half_vector_constants_array();

    int64_t sy_fixp = to_src_y(dy);
    ptrdiff_t sy = static_cast<ptrdiff_t>(sy_fixp >> kFixpBits);
    const ptrdiff_t max_sy = static_cast<ptrdiff_t>(src_height_ - 1);
    ptrdiff_t sy_top = std::clamp(sy, ptrdiff_t{0}, max_sy);
    ptrdiff_t sy_bottom = std::clamp(sy + 1, ptrdiff_t{0}, max_sy);
    const uint8_t *src_top = &src_rows_.at(sy_top)[0];
    const uint8_t *src_bottom = &src_rows_.at(sy_bottom)[0];
    uint8_t *dst = &dst_rows_.at(static_cast<ptrdiff_t>(dy))[0];
    // Get the highest 8 bits of the fractional part
    // This is a good compromise between accuracy and performance
    // Because the result is 8bits, the error only affects the least
    // significant 1-2 bits, see the accuracy calculation in kleidicv.h
    const int64_t sy_fixp_base = sy * (int64_t{1} << kFixpBits);
    uint16_t yfrac =
        static_cast<uint16_t>((sy_fixp - sy_fixp_base) >> (kFixpBits - 8));

    ptrdiff_t dst_element_index = 0;

    for (size_t i = 0; i < num_of_vector_paths.two_x; i += 1) {
      uint8x16x2_t res{};
      res.val[0] = vector_path(full_array[i * 2], src_top, src_bottom, yfrac);
      res.val[1] =
          vector_path(full_array[(i * 2) + 1], src_top, src_bottom, yfrac);
      VecTraits<uint8_t>::store(res, &dst[dst_element_index]);
      dst_element_index += kStep * 2;
    }

    for (size_t i = 0; i < num_of_vector_paths.half; i += 1) {
      auto res = vector_path_half(half_array[i], yfrac, src_top, src_bottom);
      vst1(&dst[half_array[i].dst_element_index], res);
    }
  }

  void process_row_separated(
      size_t dy, RowInterpolationConstants &row_interpolation_constants) {
    int64_t sy_fixp = to_src_y(dy);
    ptrdiff_t sy = static_cast<ptrdiff_t>(sy_fixp >> kFixpBits);
    const ptrdiff_t max_sy = static_cast<ptrdiff_t>(src_height_ - 1);
    ptrdiff_t sy_top = std::clamp(sy, ptrdiff_t{0}, max_sy);
    ptrdiff_t sy_bottom = std::clamp(sy + 1, ptrdiff_t{0}, max_sy);
    if (sy_top != hbuffer_sy_) {
      if (sy_top == hbuffer_sy_ + 1) {
        std::swap(hbuffer1_, hbuffer2_);
      } else {
        process_row_horizontal(&src_rows_.at(sy_top)[0], hbuffer1_,
                               row_interpolation_constants);
      }
      hbuffer_sy_ = sy_top;
      if (sy_top < max_sy) {
        process_row_horizontal(&src_rows_.at(sy_top + 1)[0], hbuffer2_,
                               row_interpolation_constants);
      }
    }

    const uint8_t *inter_top = hbuffer1_;
    const uint8_t *inter_bottom =
        sy_bottom == hbuffer_sy_ ? hbuffer1_ : hbuffer2_;
    uint8_t *dst = &dst_rows_.at(static_cast<ptrdiff_t>(dy))[0];
    // Get the highest 8 bits of the fractional part
    // This is a good compromise between accuracy and performance
    // Because the result is 8bits, the error only affects the least
    // significant 1-2 bits, see the accuracy calculation in kleidicv.h
    const int64_t sy_fixp_base = sy * (int64_t{1} << kFixpBits);
    uint16_t yfrac =
        static_cast<uint16_t>((sy_fixp - sy_fixp_base) >> (kFixpBits - 8));
    process_row_vertical(inter_top, inter_bottom, dst, dst_length_, yfrac);
    // hbuffer_ owns the allocation from process_rows(); clang-analyzer loses
    // that member ownership on this template path.
  }  // NOLINT(clang-analyzer-unix.Malloc)

  void process_row_horizontal(
      const uint8_t *src, uint8_t *dst,
      RowInterpolationConstants &row_interpolation_constants) {
    VectorPathNums num_of_vector_paths =
        row_interpolation_constants.num_of_vector_paths();
    auto *full_array =
        row_interpolation_constants.full_vector_constants_array();
    auto *half_array =
        row_interpolation_constants.half_vector_constants_array();
    ptrdiff_t dst_element_index = 0;

    for (size_t i = 0; i < num_of_vector_paths.two_x; i += 1) {
      uint8x16x2_t res{};
      res.val[0] = horizontal_vector_path(full_array[i * 2], src);
      res.val[1] = horizontal_vector_path(full_array[(i * 2) + 1], src);
      VecTraits<uint8_t>::store(res, &dst[dst_element_index]);
      dst_element_index += kStep * 2;
    }

    for (size_t i = 0; i < num_of_vector_paths.half; i += 1) {
      auto res = horizontal_vector_path_half(half_array[i], src);
      vst1(&dst[half_array[i].dst_element_index], res);
    }
  }

  void process_row_vertical(const uint8_t *inter_top,
                            const uint8_t *inter_bottom, uint8_t *dst,
                            ptrdiff_t dst_len, uint16_t yfrac) {
    ptrdiff_t dst_element_index = 0, max2x = dst_len - 2 * kStep;

    for (; dst_element_index <= max2x; dst_element_index += kStep * 2) {
      uint8x16x2_t top, bottom;
      VecTraits<uint8_t>::load(&inter_top[dst_element_index], top);
      VecTraits<uint8_t>::load(&inter_bottom[dst_element_index], bottom);

      uint8x16x2_t res = lerp1d(top, bottom, yfrac);
      VecTraits<uint8_t>::store(res, &dst[dst_element_index]);
    }

    for (; dst_element_index < dst_len; dst_element_index += kHalfStep) {
      dst_element_index = std::min(dst_element_index, dst_len - kHalfStep);
      uint8x8_t top = vld1_u8(&inter_top[dst_element_index]);
      uint8x8_t bottom = vld1_u8(&inter_bottom[dst_element_index]);
      uint8x8_t res = lerp1d(top, bottom, yfrac);
      vst1(&dst[dst_element_index], res);
    }
  }

  uint8x16_t horizontal_vector_path(
      const FullVectorInterpolationConstants &constants,
      const uint8_t *src) const {
    uint8x16_t vsx0_idx = vld1q(constants.idx0);
    uint8x16_t vsx1_idx = vld1q(constants.idx1);
    uint16x8x2_t vsxfrac2;
    VecTraits<uint16_t>::load(constants.xfrac, vsxfrac2);
    ptrdiff_t src_element_index = constants.src_element_index;
    SrcVecType<kRatio> src_data;
    VecTraits<uint8_t>::load(&src[src_element_index], src_data);
    uint8x16_t a, b;
    if constexpr (kRatio == 1) {
      a = vqtbl1q_u8(src_data, vsx0_idx);
      b = vqtbl1q_u8(src_data, vsx1_idx);
      static_assert(!kSetRightmostLanes);
    } else if constexpr (kRatio == 2) {
      a = vqtbl2q_u8(src_data, vsx0_idx);
      b = vqtbl2q_u8(src_data, vsx1_idx);
      static_assert(!kSetRightmostLanes);
    } else if constexpr (kRatio == 3) {
      a = vqtbl3q_u8(src_data, vsx0_idx);
      b = vqtbl3q_u8(src_data, vsx1_idx);
      // table lookup would overindex src_data
      if constexpr (kSetRightmostLanes) {
        ptrdiff_t last_right_elem_idx = src_element_index + constants.idx1[15];
        b = vsetq_lane_u8(src[last_right_elem_idx], b, 15);
      }
    }
    return lerp1d(a, b, vsxfrac2);
  }

  uint8x8_t horizontal_vector_path_half(
      const HalfVectorInterpolationConstants &constants,
      const uint8_t *src) const {
    uint8x8_t vsx0_idx = vld1_u8(constants.idx0);
    uint8x8_t vsx1_idx = vld1_u8(constants.idx1);
    ptrdiff_t src_element_index = constants.src_element_index;
    uint16x8_t vsxfrac;
    VecTraits<uint16_t>::load(constants.xfrac, vsxfrac);

    constexpr bool kLoadTwo =
        (!kUpsize && kChannels == 3) || kRatio == 3;  // GCOVR_EXCL_LINE
    HalfSrcVecType<kLoadTwo> src_data;
    VecTraits<uint8_t>::load(&src[src_element_index], src_data);

    uint8x8_t a, b;
    if constexpr (!kLoadTwo) {
      a = vqtbl1_u8(src_data, vsx0_idx);
      b = vqtbl1_u8(src_data, vsx1_idx);
    } else if constexpr (kLoadTwo) {
      a = vqtbl2_u8(src_data, vsx0_idx);
      b = vqtbl2_u8(src_data, vsx1_idx);
    }
    return lerp1d(a, b, vsxfrac);
  }

  uint8x16_t vector_path(const FullVectorInterpolationConstants &constants,
                         const uint8_t *src_top, const uint8_t *src_bottom,
                         uint16_t yfrac) const {
    uint8x16_t vsx0_idx = vld1q(constants.idx0);
    uint8x16_t vsx1_idx = vld1q(constants.idx1);
    uint16x8x2_t vsxfrac2;
    VecTraits<uint16_t>::load(constants.xfrac, vsxfrac2);
    ptrdiff_t src_element_index = constants.src_element_index;

    SrcVecType<kRatio> topsrc, bottomsrc;
    VecTraits<uint8_t>::load(&src_top[src_element_index], topsrc);
    VecTraits<uint8_t>::load(&src_bottom[src_element_index], bottomsrc);
    uint8x16_t a, b, c, d;
    if constexpr (kRatio == 1) {
      a = vqtbl1q_u8(topsrc, vsx0_idx);
      b = vqtbl1q_u8(topsrc, vsx1_idx);
      c = vqtbl1q_u8(bottomsrc, vsx0_idx);
      d = vqtbl1q_u8(bottomsrc, vsx1_idx);
      static_assert(!kSetRightmostLanes);
    } else if constexpr (kRatio == 2) {
      a = vqtbl2q_u8(topsrc, vsx0_idx);
      b = vqtbl2q_u8(topsrc, vsx1_idx);
      c = vqtbl2q_u8(bottomsrc, vsx0_idx);
      d = vqtbl2q_u8(bottomsrc, vsx1_idx);
      static_assert(!kSetRightmostLanes);
    } else if constexpr (kRatio == 3) {
      a = vqtbl3q_u8(topsrc, vsx0_idx);
      b = vqtbl3q_u8(topsrc, vsx1_idx);
      c = vqtbl3q_u8(bottomsrc, vsx0_idx);
      d = vqtbl3q_u8(bottomsrc, vsx1_idx);
      // table lookup would overindex topsrc and bottomsrc
      if constexpr (kSetRightmostLanes) {
        ptrdiff_t last_right_elem_idx = src_element_index + constants.idx1[15];
        b = vsetq_lane_u8(src_top[last_right_elem_idx], b, 15);
        d = vsetq_lane_u8(src_bottom[last_right_elem_idx], d, 15);
      }
    }
    uint8x16_t top = lerp1d(a, b, vsxfrac2);
    uint8x16_t bottom = lerp1d(c, d, vsxfrac2);
    return lerp1d(top, bottom, yfrac);
  }

  uint8x8_t vector_path_half(const HalfVectorInterpolationConstants &constants,
                             uint16_t yfrac, const uint8_t *src_top,
                             const uint8_t *src_bottom) const {
    uint8x8_t vsx0_idx = vld1_u8(constants.idx0);
    uint8x8_t vsx1_idx = vld1_u8(constants.idx1);
    ptrdiff_t src_element_index = constants.src_element_index;
    uint16x8_t vsxfrac;
    VecTraits<uint16_t>::load(constants.xfrac, vsxfrac);

    constexpr bool kLoadTwo =
        (!kUpsize && kChannels == 3) || kRatio == 3;  // GCOVR_EXCL_LINE
    HalfSrcVecType<kLoadTwo> topsrc, bottomsrc;
    VecTraits<uint8_t>::load(&src_top[src_element_index], topsrc);
    VecTraits<uint8_t>::load(&src_bottom[src_element_index], bottomsrc);

    uint8x8_t a, b, c, d;
    if constexpr (!kLoadTwo) {
      a = vqtbl1_u8(topsrc, vsx0_idx);
      b = vqtbl1_u8(topsrc, vsx1_idx);
      c = vqtbl1_u8(bottomsrc, vsx0_idx);
      d = vqtbl1_u8(bottomsrc, vsx1_idx);
    } else if constexpr (kLoadTwo) {
      a = vqtbl2_u8(topsrc, vsx0_idx);
      b = vqtbl2_u8(topsrc, vsx1_idx);
      c = vqtbl2_u8(bottomsrc, vsx0_idx);
      d = vqtbl2_u8(bottomsrc, vsx1_idx);
    }
    uint8x8_t top = lerp1d(a, b, vsxfrac);
    uint8x8_t bottom = lerp1d(c, d, vsxfrac);
    return lerp1d(top, bottom, yfrac);
  }

  static uint8x8_t lerp1d(uint8x8_t a, uint8x8_t b, uint16_t w) {
    return vraddhn_u16(vshll_n_u8(a, 8), vmulq_n_u16(vsubl_u8(b, a), w));
  }

  static uint8x8_t lerp1d(uint8x8_t a, uint8x8_t b, uint16x8_t w) {
    return vraddhn_u16(vshll_n_u8(a, 8), vmulq_u16(vsubl_u8(b, a), w));
  }

  static uint8x16_t lerp1d(uint8x16_t a, uint8x16_t b, uint16_t w) {
    uint8x8_t lo = lerp1d(vget_low_u8(a), vget_low_u8(b), w);
    uint8x8_t hi = lerp1d(vget_high_u8(a), vget_high_u8(b), w);
    return vcombine_u8(lo, hi);
  }

  static uint8x16x2_t lerp1d(uint8x16x2_t a, uint8x16x2_t b, uint16_t w) {
    uint8x8_t res0_lo = lerp1d(vget_low_u8(a.val[0]), vget_low_u8(b.val[0]), w);
    uint8x8_t res0_hi =
        lerp1d(vget_high_u8(a.val[0]), vget_high_u8(b.val[0]), w);
    uint8x8_t res1_lo = lerp1d(vget_low_u8(a.val[1]), vget_low_u8(b.val[1]), w);
    uint8x8_t res1_hi =
        lerp1d(vget_high_u8(a.val[1]), vget_high_u8(b.val[1]), w);
    return uint8x16x2_t{vcombine_u8(res0_lo, res0_hi),
                        vcombine_u8(res1_lo, res1_hi)};
  }

  static uint8x16_t lerp1d(uint8x16_t a, uint8x16_t b, uint16x8x2_t w) {
    uint8x8_t lo = lerp1d(vget_low_u8(a), vget_low_u8(b), w.val[0]);
    uint8x8_t hi = lerp1d(vget_high_u8(a), vget_high_u8(b), w.val[1]);
    return vcombine_u8(lo, hi);
  }

  const Rows<const uint8_t> src_rows_;
  const Rows<uint8_t> dst_rows_;
  const size_t src_height_;
  const size_t y_begin_;
  const size_t y_end_;
  // number of elements in a row
  const ptrdiff_t dst_length_;
  const size_t dst_height_;
  using FreeDeleter = decltype(&std::free);
  std::unique_ptr<uint8_t, FreeDeleter> hbuffer_;
  uint8_t *hbuffer1_, *hbuffer2_;
  // index of the top row stored at hbuffer1_; hbuffer2_ is
  // always the next row
  ptrdiff_t hbuffer_sy_;
};  // template class ResizeGenericU8Operation

}  // namespace kleidicv::neon::resize_linear_generic_u8

#endif  // KLEIDICV_RESIZE_RESIZE_LINEAR_GENERIC_U8_NEON_H
