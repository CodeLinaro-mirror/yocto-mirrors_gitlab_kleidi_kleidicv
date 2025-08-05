// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_WORKSPACE_BORDER_GENERIC_SC_H
#define KLEIDICV_WORKSPACE_BORDER_GENERIC_SC_H

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "kleidicv/sve2.h"
#include "kleidicv/types.h"
#include "kleidicv/workspace/border_types.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Border offsets for generic filters.
template <kleidicv::FixedBorderType BorderType>
class GenericBorder final {
 public:
  explicit GenericBorder(size_t width, size_t channels, svuint16_t& left1,
                         svuint16_t& left2, svuint16_t& right1,
                         svuint16_t& right2) KLEIDICV_STREAMING_COMPATIBLE
      : width_(static_cast<ptrdiff_t>(width)),
        channels_{static_cast<ptrdiff_t>(channels)},
        total_width_(width_* channels_),
        border_indices_left_(left1),
        border_indices_left_ext_(left2),
        border_indices_right_(right1),
        border_indices_right_ext_(right2) {
    // The result will take some elements from the image (data), and the
    // remaining parts from the border.
    // An index vector is prepared here to help the process, e.g. for
    // replicated borders and 3 channels, the constructed index vector will
    // look like this:
    //               [1, 2, 0, 1, 2, 3, 4, 5]
    // (0,1,2 is repeated until index 0 is reached, when the image data
    // begins.) Right side is similar, but it is the [5,6,7] that repeats
    // after.
    uint16_t left[128 + 4], right[128 + 4];
    // This is to ensure the last element be (channels_ - 1)
    uint16_t bias = channels_ - 1 - ((svcnth() - 1) % channels_);
    for (size_t i = 0; i < svcnth() + 4; ++i) {
      left[i] = (i + bias) % channels_;
      right[i] = (i % channels_) + svcnth() - channels_;
    }
    // Analyser thinks left[0] is garbage, but it is not.
    // NOLINTBEGIN(clang-analyzer-core.UndefinedBinaryOperatorResult)
    border_indices_left_ = svld1_u16(svptrue_b16(), left);
    border_indices_left_ext_ =
        svld1_u16(svptrue_b16(), left + channels_ - left[0]);
    border_indices_right_ = svld1_u16(svptrue_b16(), right);
    border_indices_right_ext_ = svld1_u16(svptrue_b16(), right + left[0]);
    // NOLINTEND(clang-analyzer-core.UndefinedBinaryOperatorResult)
  }

  // Raw column can be bigger than width-1 or less than 0
  ptrdiff_t get_column(ptrdiff_t raw_column) const {
    // TODO more border types, this is only the Replicated
    return std::max<ptrdiff_t>(std::min<ptrdiff_t>(raw_column, width_ - 1),
                               ptrdiff_t{0});
  }

  // Assuming that start_offset is <= 0
  svuint16_t load_left(Rows<const uint8_t> src_rows, ptrdiff_t start_offset)
      const KLEIDICV_STREAMING_COMPATIBLE {
    if constexpr (BorderType == FixedBorderType::REPLICATE) {
      svuint16_t data = svld1ub_u16(svptrue_b16(), &src_rows[0]);
      svuint16_t indices{};
      svuint16_t increasing = svindex_u16(0, 1);
      if (-start_offset < static_cast<ptrdiff_t>(svcnth())) {
        // '-start_offset' elements from the border, the others from the data
        svbool_t pg =
            svcmpge_n_u16(svptrue_b16(), increasing,
                          static_cast<uint16_t>(svcnth() + start_offset));
        indices = svsplice_u16(pg, border_indices_left_, increasing);
      } else {
        // 'shift' elements need to be shifted out
        ptrdiff_t shift = channels_ - (-start_offset - svcnth()) % channels_;
        svbool_t pg = svcmpge_n_u16(svptrue_b16(), increasing, shift);
        indices =
            svsplice_u16(pg, border_indices_left_, border_indices_left_ext_);
      }
      return svtbl_u16(data, indices);
    }
  }

  // Assuming that start_offset is >= width - svcnth()
  svuint16_t load_right(Rows<const uint8_t> src_rows, ptrdiff_t start_offset)
      const KLEIDICV_STREAMING_COMPATIBLE {
    if constexpr (BorderType == FixedBorderType::REPLICATE) {
      svuint16_t data =
          svld1ub_u16(svptrue_b16(), &src_rows[total_width_ - svcnth()]);
      svuint16_t indices{};
      svuint16_t increasing = svindex_u16(0, 1);
      if (start_offset <= width_ * channels_) {
        svbool_t pg = svcmpge_n_u16(
            svptrue_b16(), increasing,
            static_cast<uint16_t>(
                start_offset -
                (total_width_ - static_cast<ptrdiff_t>(svcnth()))));
        indices = svsplice_u16(pg, increasing, border_indices_right_);
      } else {
        ptrdiff_t shift =
            svcnth() -
            (channels_ - (start_offset - width_ * channels_) % channels_);
        svbool_t pg = svcmpge_n_u16(svptrue_b16(), increasing, shift);
        indices =
            svsplice_u16(pg, border_indices_right_ext_, border_indices_right_);
      }

      return svtbl_u16(data, indices);
    }
  }

 private:
  ptrdiff_t width_, channels_, total_width_;
  svuint16_t &border_indices_left_, &border_indices_left_ext_,
      &border_indices_right_, &border_indices_right_ext_;
};  // end of class GenericBorder<BorderType>

// Dummy
template <typename T>
class DummyBorderMaker {
 public:
  void decorate(Rows<T>, size_t, size_t) KLEIDICV_STREAMING_COMPATIBLE {}
  void decorate_from_left(Rows<T>, size_t,
                          size_t) KLEIDICV_STREAMING_COMPATIBLE {}
  void decorate_from_right(Rows<T>, size_t,
                           size_t) KLEIDICV_STREAMING_COMPATIBLE {}
};

template <typename ScalarType>
class BorderMakerArbitrary {
  using VecTraits = typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<ScalarType>;
  using VectorType = typename VecTraits::VectorType;

 public:
  // OK this is specialized for uint8_t :o
  BorderMakerArbitrary(ptrdiff_t channels, svuint8_t& sv0, svuint8_t& sv1,
                       svuint8_t& sv2) KLEIDICV_STREAMING_COMPATIBLE
      : channels_(channels),
        indices0_(sv0),
        indices1_(sv1),
        indices2_(sv2) {
    if (channels_ == 3) {
      size_t kVL = VecTraits::num_lanes();
      indices0_ = svindex_u8(0, 1);
      indices1_ = svindex_u8(kVL % 3, 1);
      indices2_ = svindex_u8((kVL + kVL) % 3, 1);
      // Decrease by 3 while they are >= 3 --> so we get the modulo
      size_t steps = (kVL - 1) / 3;
      for (size_t i = 0; i < steps; ++i) {
        indices0_ = svsub_n_u8_m(svcmpge_n_u8(svptrue_b8(), indices0_, 3),
                                 indices0_, 3);
        indices1_ = svsub_n_u8_m(svcmpge_n_u8(svptrue_b8(), indices1_, 3),
                                 indices1_, 3);
        indices2_ = svsub_n_u8_m(svcmpge_n_u8(svptrue_b8(), indices2_, 3),
                                 indices2_, 3);
      }
    } else {
      indices0_ = svindex_u8(0, 1);
      // It does the same as the modulo for 1,2 and 4
      indices0_ = svand_n_u8_x(svptrue_b8(), indices0_, channels_ - 1);
    }
  }
  // Replicate only
  void decorate(Rows<ScalarType> rows, ptrdiff_t margin,
                ptrdiff_t width) KLEIDICV_STREAMING_COMPATIBLE {
    const size_t kVL = VecTraits::num_lanes();
    svbool_t pg_ch = VecTraits::svwhilelt(0UL, rows.channels());

    // right border
    svuint8_t data = svld1_u8(pg_ch, &rows[(width - 1) * rows.channels()]);
    if (rows.channels() == 3) {
      svuint8_t data0 = svtbl_u8(data, indices0_);
      svuint8_t data1 = svtbl_u8(data, indices1_);
      svuint8_t data2 = svtbl_u8(data, indices2_);
      ptrdiff_t width_plus = (width + margin) * 3;
      for (ptrdiff_t x = width * 3; x < width_plus;) {
        svbool_t pg = VecTraits::svwhilelt(x, width_plus);
        svst1(pg, &rows[x], data0);
        x += kVL;
        pg = VecTraits::svwhilelt(x, width_plus);
        svst1(pg, &rows[x], data1);
        x += kVL;
        pg = VecTraits::svwhilelt(x, width_plus);
        svst1(pg, &rows[x], data2);
        x += kVL;
      }
    } else {
      data = svtbl_u8(data, indices0_);
      ptrdiff_t width_plus = (width + margin) * rows.channels();
      for (ptrdiff_t x = width * rows.channels(); x < width_plus;) {
        svbool_t pg = VecTraits::svwhilelt(x, width_plus);
        svst1(pg, &rows[x], data);
        x += kVL;
      }
    }

    // left border
    data = svld1_u8(pg_ch, &rows[0]);
    if (rows.channels() == 3) {
      svuint8_t data0 = svtbl_u8(data, indices0_);
      svuint8_t data1 = svtbl_u8(data, indices1_);
      svuint8_t data2 = svtbl_u8(data, indices2_);
      ptrdiff_t mwidth = margin * 3;
      for (ptrdiff_t x = 0; x < mwidth;) {
        svbool_t pg = VecTraits::svwhilelt(x, mwidth);
        svst1(pg, &rows[x - mwidth], data0);
        x += kVL;
        pg = VecTraits::svwhilelt(x, mwidth);
        svst1(pg, &rows[x - mwidth], data1);
        x += kVL;
        pg = VecTraits::svwhilelt(x, mwidth);
        svst1(pg, &rows[x - mwidth], data2);
        x += kVL;
      }
    } else {
      data = svtbl_u8(data, indices0_);
      ptrdiff_t mwidth = margin * rows.channels();
      for (ptrdiff_t x = 0; x < mwidth;) {
        svbool_t pg = VecTraits::svwhilelt(x, mwidth);
        svst1(pg, &rows[x - mwidth], data);
        x += kVL;
      }
    }
  }

 private:
  ptrdiff_t channels_;
  svuint8_t &indices0_, &indices1_, &indices2_;
};

template <typename ScalarType, size_t NVectors>
class BorderMakerFixed3ch {
  using VecTraits = typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<ScalarType>;
  using VectorType = typename VecTraits::VectorType;

 public:
  // OK this is specialized for uint8_t :o
  BorderMakerFixed3ch(svuint8_t& sv0, svuint8_t& sv1, svuint8_t& sv2,
                      svbool_t& pg_last,
                      size_t n_elements) KLEIDICV_STREAMING_COMPATIBLE
      : indices0_(sv0),
        indices1_(sv1),
        indices2_(sv2),
        pg_last_(pg_last) {
    size_t kVL = VecTraits::num_lanes() / sizeof(ScalarType);
    indices0_ = svindex_u8(0, 1);
    indices1_ = svindex_u8(kVL % 3, 1);
    indices2_ = svindex_u8((kVL + kVL) % 3, 1);
    pg_last_ = VecTraits::svwhilelt(0UL, ((n_elements - 1) % kVL) + 1);
    // Decrease by 3 while they are >= 3 --> so we get the modulo
    size_t steps = (kVL - 1) / 3;
    for (size_t i = 0; i < steps; ++i) {
      indices0_ =
          svsub_n_u8_m(svcmpge_n_u8(svptrue_b8(), indices0_, 3), indices0_, 3);
      indices1_ =
          svsub_n_u8_m(svcmpge_n_u8(svptrue_b8(), indices1_, 3), indices1_, 3);
      indices2_ =
          svsub_n_u8_m(svcmpge_n_u8(svptrue_b8(), indices2_, 3), indices2_, 3);
    }
  }

  void decorate_one_side(Rows<ScalarType> rows, svuint8_t data,
                         ptrdiff_t offset, ptrdiff_t kVL,
                         svbool_t pgtrue) KLEIDICV_STREAMING_COMPATIBLE {
    svuint8_t data0 = svtbl_u8(data, indices0_);
    svuint8_t data1 = svtbl_u8(data, indices1_);
    svuint8_t data2 = svtbl_u8(data, indices2_);
    ptrdiff_t x = offset;
    if constexpr (NVectors == 1) {
      svst1(pg_last_, &rows[x], data0);
    } else {
      svst1(pgtrue, &rows[x], data0);
      x += kVL;
      if constexpr (NVectors == 2) {
        svst1(pg_last_, &rows[x], data1);
      } else {
        svst1(pgtrue, &rows[x], data1);
        x += kVL;
        if constexpr (NVectors == 3) {
          svst1(pg_last_, &rows[x], data2);
        } else {
          svst1(pgtrue, &rows[x], data2);
          x += kVL;
          if constexpr (NVectors == 4) {
            svst1(pg_last_, &rows[x], data0);
          } else {
            svst1(pgtrue, &rows[x], data0);
            x += kVL;
            if constexpr (NVectors == 5) {
              svst1(pg_last_, &rows[x], data1);
            } else {
              static_assert(false, "NVectors cannot be more than 5!");
            }
          }
        }
      }
    }
  }

  // Replicate only
  void decorate(Rows<ScalarType> rows, ptrdiff_t margin,
                ptrdiff_t width) KLEIDICV_STREAMING_COMPATIBLE {
    const ptrdiff_t kVL = static_cast<ptrdiff_t>(VecTraits::num_lanes());
    svbool_t pg_ch = svptrue_pat_b8(SV_VL3);
    svbool_t pgtrue = svptrue_b8();

    // left border
    svuint8_t data = svld1_u8(pg_ch, &rows[0]);
    decorate_one_side(rows, data, -margin * 3, kVL, pgtrue);

    // right border
    data = svld1_u8(pg_ch, &rows[(width - 1) * 3]);
    decorate_one_side(rows, data, width * 3, kVL, pgtrue);
  }

  void decorate_from_left(Rows<ScalarType> rows, ptrdiff_t margin,
                          ptrdiff_t) KLEIDICV_STREAMING_COMPATIBLE {
    const ptrdiff_t kVL = static_cast<ptrdiff_t>(VecTraits::num_lanes());
    svbool_t pg_ch = svptrue_pat_b8(SV_VL3);
    svbool_t pgtrue = svptrue_b8();

    // from pixels on the left border
    svuint8_t data = svld1_u8(pg_ch, &rows[0]);
    decorate_one_side(rows, data, -margin * 3, kVL, pgtrue);
  }

  void decorate_from_right(Rows<ScalarType> rows, ptrdiff_t,
                           ptrdiff_t width) KLEIDICV_STREAMING_COMPATIBLE {
    const ptrdiff_t kVL = static_cast<ptrdiff_t>(VecTraits::num_lanes());
    svbool_t pg_ch = svptrue_pat_b8(SV_VL3);
    svbool_t pgtrue = svptrue_b8();

    // from pixels on the right border
    svuint8_t data = svld1_u8(pg_ch, &rows[(width - 1) * 3]);
    decorate_one_side(rows, data, width * 3, kVL, pgtrue);
  }

 private:
  svuint8_t &indices0_, &indices1_, &indices2_;
  svbool_t& pg_last_;
};

template <typename ScalarType, size_t NVectors>
class BorderMakerFixed124ch {
  using VecTraits = typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<ScalarType>;
  using VectorType = typename VecTraits::VectorType;

 public:
  // OK this is specialized for uint8_t :o
  BorderMakerFixed124ch(ptrdiff_t channels, ptrdiff_t width, ptrdiff_t margin,
                        svuint8_t& sv, svbool_t& pg_last,
                        svbool_t& pg_ch) KLEIDICV_STREAMING_COMPATIBLE
      : indices_(sv),
        pg_last_(pg_last),
        pg_ch_(pg_ch),
        left_margin_start_{-margin * channels},
        right_margin_start_{width * channels},
        last_column_{(width - 1) * channels} {
    const ptrdiff_t kNElements = margin * channels;
    indices_ = svindex_u8(0, 1);
    // It does the same as the modulo for 1,2 and 4
    indices_ = svand_n_u8_x(svptrue_b8(), indices_, channels - 1);
    pg_ch_ = VecTraits::svwhilelt(0L, channels);
    ptrdiff_t kVL = VecTraits::num_lanes() / sizeof(ScalarType);
    pg_last_ = VecTraits::svwhilelt(
        0L, ((static_cast<ptrdiff_t>(kNElements) - 1) % kVL) + 1);
  }

  void decorate_one_side(Rows<ScalarType> rows, svuint8_t data,
                         ptrdiff_t offset, ptrdiff_t kVL,
                         svbool_t pgtrue) KLEIDICV_STREAMING_COMPATIBLE {
    svuint8_t filled_data = svtbl_u8(data, indices_);
    ptrdiff_t x = offset;
    if constexpr (NVectors == 1) {
      svst1(pg_last_, &rows[x], filled_data);
    } else {
      svst1(pgtrue, &rows[x], filled_data);
      x += kVL;
      if constexpr (NVectors == 2) {
        svst1(pg_last_, &rows[x], filled_data);
      } else {
        svst1(pgtrue, &rows[x], filled_data);
        x += kVL;
        if constexpr (NVectors == 3) {
          svst1(pg_last_, &rows[x], filled_data);
        } else {
          svst1(pgtrue, &rows[x], filled_data);
          x += kVL;
          if constexpr (NVectors == 4) {
            svst1(pg_last_, &rows[x], filled_data);
          } else {
            svst1(pgtrue, &rows[x], filled_data);
            x += kVL;
            if constexpr (NVectors == 5) {
              svst1(pg_last_, &rows[x], filled_data);
            } else {
              static_assert(false, "NVectors cannot be more than 5!");
            }
          }
        }
      }
    }
  }

  // Replicate only
  void decorate(Rows<ScalarType> rows, ptrdiff_t = 0,
                ptrdiff_t = 0) KLEIDICV_STREAMING_COMPATIBLE {
    const size_t kVL = VecTraits::num_lanes();
    svbool_t pgtrue = svptrue_b8();

    // left border
    svuint8_t data = svld1_u8(pg_ch_, &rows[0]);
    decorate_one_side(rows, data, left_margin_start_, kVL, pgtrue);

    // right border
    data = svld1_u8(pg_ch_, &rows[last_column_]);
    decorate_one_side(rows, data, right_margin_start_, kVL, pgtrue);
  }

  void decorate_from_left(Rows<ScalarType> rows, ptrdiff_t,
                          ptrdiff_t) KLEIDICV_STREAMING_COMPATIBLE {
    const size_t kVL = VecTraits::num_lanes();
    svbool_t pgtrue = svptrue_b8();

    // from pixels on the left border
    svuint8_t data = svld1_u8(pg_ch_, &rows[0]);
    decorate_one_side(rows, data, left_margin_start_, kVL, pgtrue);
  }

  void decorate_from_right(Rows<ScalarType> rows, ptrdiff_t,
                           ptrdiff_t) KLEIDICV_STREAMING_COMPATIBLE {
    const size_t kVL = VecTraits::num_lanes();
    svbool_t pgtrue = svptrue_b8();

    // from pixels on the right border
    svuint8_t data = svld1_u8(pg_ch_, &rows[last_column_]);
    decorate_one_side(rows, data, right_margin_start_, kVL, pgtrue);
  }

 private:
  svuint8_t& indices_;
  svbool_t &pg_last_, &pg_ch_;
  ptrdiff_t left_margin_start_, right_margin_start_, last_column_;
};

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_WORKSPACE_BORDER_GENERIC_NEON_H
