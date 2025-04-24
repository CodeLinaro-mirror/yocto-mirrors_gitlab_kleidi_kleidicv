// SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_SEPARABLE_FILTER_15X15_SC_H
#define KLEIDICV_SEPARABLE_FILTER_15X15_SC_H

#include <arm_sve.h>

#include <memory>

#include "kleidicv/sve2.h"
#include "kleidicv/workspace/border_15x15.h"

// It is used by SVE2 and SME2, the actual namespace will reflect it.
namespace KLEIDICV_TARGET_NAMESPACE {

// Template for drivers of separable NxM filters.
template <typename FilterType, const size_t S>
class SeparableFilter;

// Driver for a separable 15x15 filter.
template <typename FilterType>
class SeparableFilter<FilterType, 15UL> {
 public:
  using SourceType = typename FilterType::SourceType;
  using BufferType = typename FilterType::BufferType;
  using DestinationType = typename FilterType::DestinationType;
  using SourceVecTraits =
      typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using BufferVecTraits =
      typename ::KLEIDICV_TARGET_NAMESPACE::VecTraits<BufferType>;
  using BufferVectorType = typename BufferVecTraits::VectorType;
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo15x15<SourceType>;
  using BorderType = FixedBorderType;
  using BorderOffsets = typename BorderInfoType::Offsets;

  explicit SeparableFilter(FilterType filter, svuint32_t& t1, svuint32_t& t2,
                           svuint32_t& t3,
                           svuint32_t& t4) KLEIDICV_STREAMING_COMPATIBLE
      : filter_{filter},
        t1_{t1},
        t2_{t2},
        t3_{t3},
        t4_{t4} {
    uint32_t kTblPair[16] = {0, 16};
    uint32_t kTblPair2[16] = {0, 1, 16, 17};
    uint32_t kTblPair4[16] = {0, 1, 2, 3, 16, 17, 18, 19};
    uint32_t kTblPair7[16] = {0,  1,  2,  3,  4,  5,  6,  16,
                              17, 18, 19, 20, 21, 22, 23, 24};
    t1_ = svld1(svptrue_b32(), kTblPair);
    t2_ = svld1(svptrue_b32(), kTblPair2);
    t3_ = svld1(svptrue_b32(), kTblPair4);
    t4_ = svld1(svptrue_b32(), kTblPair7);
  }

  static constexpr size_t margin = 7UL;

  void process_vertical(
      size_t width, Rows<const SourceType> src_rows, Rows<BufferType> dst_rows,
      BorderOffsets border_offsets) const KLEIDICV_STREAMING_COMPATIBLE {
    LoopUnroll2 loop{width * src_rows.channels(), SourceVecTraits::num_lanes()};

    loop.unroll_once([&](size_t index) KLEIDICV_STREAMING_COMPATIBLE {
      svbool_t pg_all = SourceVecTraits::svptrue();
      vertical_vector_path(pg_all, src_rows, dst_rows, border_offsets, index);
    });

    loop.remaining(
        [&](size_t index, size_t length) KLEIDICV_STREAMING_COMPATIBLE {
          svbool_t pg = SourceVecTraits::svwhilelt(index, length);
          vertical_vector_path(pg, src_rows, dst_rows, border_offsets, index);
        });
  }

  void process_horizontal(size_t width, Rows<const BufferType> src_rows,
                          Rows<DestinationType> dst_rows,
                          BorderOffsets border_offsets) const
      KLEIDICV_STREAMING_COMPATIBLE {
    svbool_t pg_all = BufferVecTraits::svptrue();
    LoopUnroll2 loop{width * src_rows.channels(), BufferVecTraits::num_lanes()};

    loop.unroll_twice([&](size_t index) KLEIDICV_STREAMING_COMPATIBLE {
      horizontal_vector_path_2x(pg_all, src_rows, dst_rows, border_offsets,
                                index);
    });

    loop.unroll_once([&](size_t index) KLEIDICV_STREAMING_COMPATIBLE {
      horizontal_vector_path(pg_all, src_rows, dst_rows, border_offsets, index);
    });

    loop.remaining(
        [&](size_t index, size_t length) KLEIDICV_STREAMING_COMPATIBLE {
          svbool_t pg = BufferVecTraits::svwhilelt(index, length);
          horizontal_vector_path(pg, src_rows, dst_rows, border_offsets, index);
        });
  }

  size_t process_left_border(Rows<const BufferType> src_rows,
                             Rows<DestinationType> dst_rows,
                             BorderInfoType horizontal_border,
                             size_t width) const KLEIDICV_STREAMING_COMPATIBLE {
    switch (src_rows.channels()) {
      case 1:
        return process_left_border<1UL>(
            src_rows, dst_rows, horizontal_border, width,
            BufferVecTraits::template svptrue_pat<SV_VL1>());
        break;
      case 2:
        return process_left_border<2UL>(
            src_rows, dst_rows, horizontal_border, width,
            BufferVecTraits::template svptrue_pat<SV_VL2>());
        break;
      case 3:
        return process_left_border<3UL>(
            src_rows, dst_rows, horizontal_border, width,
            BufferVecTraits::template svptrue_pat<SV_VL3>());
        break;
      case 4:
        return process_left_border<4UL>(
            src_rows, dst_rows, horizontal_border, width,
            BufferVecTraits::template svptrue_pat<SV_VL4>());
        break;
      default:
        break;
    }
    return 0;
  }

  template <size_t Channels>
  size_t process_left_border(
      Rows<const BufferType> src_rows, Rows<DestinationType> dst_rows,
      BorderInfoType horizontal_border, size_t width,
      svbool_t pg_ch) const KLEIDICV_STREAMING_COMPATIBLE {
    // Process <number of channels> vectors, as many times as needed to pass
    // all the borders. Plus, because of the horizontal path, the algorithm
    // needs additional <ksize - 1> pixels.
    const size_t block_len = Channels * BufferVecTraits::num_lanes();
    const size_t border_len = Channels * margin;
    const size_t process_len =
        ((border_len + block_len - 1) / block_len) * block_len;
    const size_t buffer_len = process_len + Channels * (15 - 1);
    if (buffer_len - margin >= width) {  // would it be too long?
      return 0;
    }
    // PROTO: now this is pretty much fixed
    // only implemented for 512-bit vector length
    if (svcntw() != 16) {
      return 0;
    }
    // With 4 channels, buffer_len is 14*4 + 16 = 72, that needs 5 vectors (80)
    BufferVectorType vbuf0, vbuf1;  //, vbuf2, vbuf3, vbuf4;
    // BufferVectorType* pvbuf[5] = {&vbuf0, &vbuf1, &vbuf2, &vbuf3, &vbuf4};

    BorderOffsets offsets = horizontal_border.offsets_with_left_border(0);

    Columns<const BufferType> src_cols = src_rows.as_columns();

    // Copy the 7 border pixels (=margin) into the buffer
    {
      BufferVectorType pixel0 = svld1(pg_ch, src_cols.ptr_at(offsets.c0()));
      BufferVectorType pixel1 = svld1(pg_ch, src_cols.ptr_at(offsets.c1()));
      BufferVectorType pixel2 = svld1(pg_ch, src_cols.ptr_at(offsets.c2()));
      BufferVectorType pixel3 = svld1(pg_ch, src_cols.ptr_at(offsets.c3()));
      BufferVectorType pixel4 = svld1(pg_ch, src_cols.ptr_at(offsets.c4()));
      BufferVectorType pixel5 = svld1(pg_ch, src_cols.ptr_at(offsets.c5()));
      BufferVectorType pixel6 = svld1(pg_ch, src_cols.ptr_at(offsets.c6()));
      if constexpr (Channels == 1) {
        // need to load 14 + 16 = 30 elements, that's two vectors
        BufferVectorType px01 = svtbl2_u32(svcreate2(pixel0, pixel1), t1_);
        BufferVectorType px23 = svtbl2_u32(svcreate2(pixel2, pixel3), t1_);
        BufferVectorType px45 = svtbl2_u32(svcreate2(pixel4, pixel5), t1_);
        BufferVectorType px0123 = svtbl2_u32(svcreate2(px01, px23), t2_);
        BufferVectorType px456 = svtbl2_u32(svcreate2(px45, pixel6), t2_);
        BufferVectorType px0to6 = svtbl2_u32(svcreate2(px0123, px456), t3_);
        BufferVectorType image0 = svld1(svwhilelt_b32(7, 16), &src_cols[0]);
        vbuf1 = svld1(svwhilelt_b32(0, 14), &src_cols[16 - 7]);
        vbuf0 = svtbl2_u32(svcreate2(px0to6, image0), t4_);
      } else {
        vbuf0 = svld1(svptrue_b32(), &src_cols[0]);
      }
    }
    // Do the gaussian blur

    if constexpr (Channels == 1) {
      BufferVectorType src_0 = vbuf0;
      BufferVectorType src_1 = svext(vbuf0, vbuf1, 1);
      BufferVectorType src_2 = svext(vbuf0, vbuf1, 2);
      BufferVectorType src_3 = svext(vbuf0, vbuf1, 3);
      BufferVectorType src_4 = svext(vbuf0, vbuf1, 4);
      BufferVectorType src_5 = svext(vbuf0, vbuf1, 5);
      BufferVectorType src_6 = svext(vbuf0, vbuf1, 6);
      BufferVectorType src_7 = svext(vbuf0, vbuf1, 7);
      BufferVectorType src_8 = svext(vbuf0, vbuf1, 8);
      BufferVectorType src_9 = svext(vbuf0, vbuf1, 9);
      BufferVectorType src_10 = svext(vbuf0, vbuf1, 10);
      BufferVectorType src_11 = svext(vbuf0, vbuf1, 11);
      BufferVectorType src_12 = svext(vbuf0, vbuf1, 12);
      BufferVectorType src_13 = svext(vbuf0, vbuf1, 13);
      BufferVectorType src_14 = svext(vbuf0, vbuf1, 14);
      filter_.horizontal_vector_path(svptrue_b32(), src_0, src_1, src_2, src_3,
                                     src_4, src_5, src_6, src_7, src_8, src_9,
                                     src_10, src_11, src_12, src_13, src_14,
                                     &dst_rows.as_columns()[0]);
    }

    return process_len;
  }

  // Processing of horizontal borders is always scalar because border offsets
  // change for each and every element in the border.
  void process_horizontal_borders(
      Rows<const BufferType> src_rows, Rows<DestinationType> dst_rows,
      BorderOffsets border_offsets) const KLEIDICV_STREAMING_COMPATIBLE {
    for (size_t index = 0; index < src_rows.channels(); ++index) {
      disable_loop_vectorization();
      process_horizontal_border(src_rows, dst_rows, border_offsets, index);
    }
  }

 private:
  void vertical_vector_path(svbool_t pg, Rows<const SourceType> src_rows,
                            Rows<BufferType> dst_rows,
                            BorderOffsets border_offsets,
                            size_t index) const KLEIDICV_STREAMING_COMPATIBLE {
    SourceVectorType src_0 =
        svld1(pg, &src_rows.at(border_offsets.c0())[index]);
    SourceVectorType src_1 =
        svld1(pg, &src_rows.at(border_offsets.c1())[index]);
    SourceVectorType src_2 =
        svld1(pg, &src_rows.at(border_offsets.c2())[index]);
    SourceVectorType src_3 =
        svld1(pg, &src_rows.at(border_offsets.c3())[index]);
    SourceVectorType src_4 =
        svld1(pg, &src_rows.at(border_offsets.c4())[index]);
    SourceVectorType src_5 =
        svld1(pg, &src_rows.at(border_offsets.c5())[index]);
    SourceVectorType src_6 =
        svld1(pg, &src_rows.at(border_offsets.c6())[index]);
    SourceVectorType src_7 =
        svld1(pg, &src_rows.at(border_offsets.c7())[index]);
    SourceVectorType src_8 =
        svld1(pg, &src_rows.at(border_offsets.c8())[index]);
    SourceVectorType src_9 =
        svld1(pg, &src_rows.at(border_offsets.c9())[index]);
    SourceVectorType src_10 =
        svld1(pg, &src_rows.at(border_offsets.c10())[index]);
    SourceVectorType src_11 =
        svld1(pg, &src_rows.at(border_offsets.c11())[index]);
    SourceVectorType src_12 =
        svld1(pg, &src_rows.at(border_offsets.c12())[index]);
    SourceVectorType src_13 =
        svld1(pg, &src_rows.at(border_offsets.c13())[index]);
    SourceVectorType src_14 =
        svld1(pg, &src_rows.at(border_offsets.c14())[index]);
    filter_.vertical_vector_path(pg, src_0, src_1, src_2, src_3, src_4, src_5,
                                 src_6, src_7, src_8, src_9, src_10, src_11,
                                 src_12, src_13, src_14, &dst_rows[index]);
  }

  void horizontal_vector_path_2x(
      svbool_t pg, Rows<const BufferType> src_rows,
      Rows<DestinationType> dst_rows, BorderOffsets border_offsets,
      size_t index) const KLEIDICV_STREAMING_COMPATIBLE {
    auto src_0 = &src_rows.at(0, border_offsets.c0())[index];
    auto src_1 = &src_rows.at(0, border_offsets.c1())[index];
    auto src_2 = &src_rows.at(0, border_offsets.c2())[index];
    auto src_3 = &src_rows.at(0, border_offsets.c3())[index];
    auto src_4 = &src_rows.at(0, border_offsets.c4())[index];
    auto src_5 = &src_rows.at(0, border_offsets.c5())[index];
    auto src_6 = &src_rows.at(0, border_offsets.c6())[index];
    auto src_7 = &src_rows.at(0, border_offsets.c7())[index];
    auto src_8 = &src_rows.at(0, border_offsets.c8())[index];
    auto src_9 = &src_rows.at(0, border_offsets.c9())[index];
    auto src_10 = &src_rows.at(0, border_offsets.c10())[index];
    auto src_11 = &src_rows.at(0, border_offsets.c11())[index];
    auto src_12 = &src_rows.at(0, border_offsets.c12())[index];
    auto src_13 = &src_rows.at(0, border_offsets.c13())[index];
    auto src_14 = &src_rows.at(0, border_offsets.c14())[index];

    BufferVectorType src_0_0 = svld1(pg, &src_0[0]);
    BufferVectorType src_1_0 = svld1_vnum(pg, &src_0[0], 1);
    BufferVectorType src_0_1 = svld1(pg, &src_1[0]);
    BufferVectorType src_1_1 = svld1_vnum(pg, &src_1[0], 1);
    BufferVectorType src_0_2 = svld1(pg, &src_2[0]);
    BufferVectorType src_1_2 = svld1_vnum(pg, &src_2[0], 1);
    BufferVectorType src_0_3 = svld1(pg, &src_3[0]);
    BufferVectorType src_1_3 = svld1_vnum(pg, &src_3[0], 1);
    BufferVectorType src_0_4 = svld1(pg, &src_4[0]);
    BufferVectorType src_1_4 = svld1_vnum(pg, &src_4[0], 1);
    BufferVectorType src_0_5 = svld1(pg, &src_5[0]);
    BufferVectorType src_1_5 = svld1_vnum(pg, &src_5[0], 1);
    BufferVectorType src_0_6 = svld1(pg, &src_6[0]);
    BufferVectorType src_1_6 = svld1_vnum(pg, &src_6[0], 1);
    BufferVectorType src_0_7 = svld1(pg, &src_7[0]);
    BufferVectorType src_1_7 = svld1_vnum(pg, &src_7[0], 1);
    BufferVectorType src_0_8 = svld1(pg, &src_8[0]);
    BufferVectorType src_1_8 = svld1_vnum(pg, &src_8[0], 1);
    BufferVectorType src_0_9 = svld1(pg, &src_9[0]);
    BufferVectorType src_1_9 = svld1_vnum(pg, &src_9[0], 1);
    BufferVectorType src_0_10 = svld1(pg, &src_10[0]);
    BufferVectorType src_1_10 = svld1_vnum(pg, &src_10[0], 1);
    BufferVectorType src_0_11 = svld1(pg, &src_11[0]);
    BufferVectorType src_1_11 = svld1_vnum(pg, &src_11[0], 1);
    BufferVectorType src_0_12 = svld1(pg, &src_12[0]);
    BufferVectorType src_1_12 = svld1_vnum(pg, &src_12[0], 1);
    BufferVectorType src_0_13 = svld1(pg, &src_13[0]);
    BufferVectorType src_1_13 = svld1_vnum(pg, &src_13[0], 1);
    BufferVectorType src_0_14 = svld1(pg, &src_14[0]);
    BufferVectorType src_1_14 = svld1_vnum(pg, &src_14[0], 1);

    filter_.horizontal_vector_path(pg, src_0_0, src_0_1, src_0_2, src_0_3,
                                   src_0_4, src_0_5, src_0_6, src_0_7, src_0_8,
                                   src_0_9, src_0_10, src_0_11, src_0_12,
                                   src_0_13, src_0_14, &dst_rows[index]);
    filter_.horizontal_vector_path(
        pg, src_1_0, src_1_1, src_1_2, src_1_3, src_1_4, src_1_5, src_1_6,
        src_1_7, src_1_8, src_1_9, src_1_10, src_1_11, src_1_12, src_1_13,
        src_1_14, &dst_rows[index + BufferVecTraits::num_lanes()]);
  }

  void horizontal_vector_path(svbool_t pg, Rows<const BufferType> src_rows,
                              Rows<DestinationType> dst_rows,
                              BorderOffsets border_offsets, size_t index) const
      KLEIDICV_STREAMING_COMPATIBLE {
    BufferVectorType src_0 =
        svld1(pg, &src_rows.at(0, border_offsets.c0())[index]);
    BufferVectorType src_1 =
        svld1(pg, &src_rows.at(0, border_offsets.c1())[index]);
    BufferVectorType src_2 =
        svld1(pg, &src_rows.at(0, border_offsets.c2())[index]);
    BufferVectorType src_3 =
        svld1(pg, &src_rows.at(0, border_offsets.c3())[index]);
    BufferVectorType src_4 =
        svld1(pg, &src_rows.at(0, border_offsets.c4())[index]);
    BufferVectorType src_5 =
        svld1(pg, &src_rows.at(0, border_offsets.c5())[index]);
    BufferVectorType src_6 =
        svld1(pg, &src_rows.at(0, border_offsets.c6())[index]);
    BufferVectorType src_7 =
        svld1(pg, &src_rows.at(0, border_offsets.c7())[index]);
    BufferVectorType src_8 =
        svld1(pg, &src_rows.at(0, border_offsets.c8())[index]);
    BufferVectorType src_9 =
        svld1(pg, &src_rows.at(0, border_offsets.c9())[index]);
    BufferVectorType src_10 =
        svld1(pg, &src_rows.at(0, border_offsets.c10())[index]);
    BufferVectorType src_11 =
        svld1(pg, &src_rows.at(0, border_offsets.c11())[index]);
    BufferVectorType src_12 =
        svld1(pg, &src_rows.at(0, border_offsets.c12())[index]);
    BufferVectorType src_13 =
        svld1(pg, &src_rows.at(0, border_offsets.c13())[index]);
    BufferVectorType src_14 =
        svld1(pg, &src_rows.at(0, border_offsets.c14())[index]);
    filter_.horizontal_vector_path(pg, src_0, src_1, src_2, src_3, src_4, src_5,
                                   src_6, src_7, src_8, src_9, src_10, src_11,
                                   src_12, src_13, src_14, &dst_rows[index]);
  }

  void process_horizontal_border(
      Rows<const BufferType> src_rows, Rows<DestinationType> dst_rows,
      BorderOffsets border_offsets,
      size_t index) const KLEIDICV_STREAMING_COMPATIBLE {
    svbool_t pg_1 = BufferVecTraits::template svptrue_pat<SV_VL1>();
    horizontal_vector_path(pg_1, src_rows, dst_rows, border_offsets, index);
  }

  FilterType filter_;
  svuint32_t &t1_, &t2_, &t3_, &t4_;
};  // end of class SeparableFilter<FilterType, 15UL>

// Shorthand for 15x15 separable filters driver type.
template <class FilterType>
using SeparableFilter15x15 = SeparableFilter<FilterType, 15UL>;

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_SEPARABLE_FILTER_15X15_SC_H
