// SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_SCALE_SC_H
#define KLEIDICV_SCALE_SC_H

#include <arm_sve.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>

#include "kleidicv/arithmetics/scale.h"
#include "kleidicv/sve2.h"

namespace KLEIDICV_TARGET_NAMESPACE {

class AddFloat final : public UnrollTwice {
 public:
  using ContextType = Context;
  using VecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<float>;
  using VectorType = typename VecTraits::VectorType;

  explicit AddFloat(const svfloat32_t &svshift) KLEIDICV_STREAMING
      : svshift_{svshift} {}

  // NOLINTBEGIN(readability-make-member-function-const)
  VectorType vector_path(ContextType ctx, VectorType src) KLEIDICV_STREAMING {
    return svadd_x(ctx.predicate(), src, svshift_);
  }
  // NOLINTEND(readability-make-member-function-const)

 private:
  const svfloat32_t &svshift_;
};  // end of class AddFloat

class ScaleFloat final : public UnrollTwice {
 public:
  using ContextType = Context;
  using VecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<float>;
  using VectorType = typename VecTraits::VectorType;

  ScaleFloat(const svfloat32_t &svscale,
             const svfloat32_t &svshift) KLEIDICV_STREAMING
      : svscale_{svscale},
        svshift_{svshift} {}

  // NOLINTBEGIN(readability-make-member-function-const)
  VectorType vector_path(ContextType ctx, VectorType src) KLEIDICV_STREAMING {
    return svmla_x(ctx.predicate(), svshift_, src, svscale_);
  }
  // NOLINTEND(readability-make-member-function-const)

 private:
  const svfloat32_t &svscale_, &svshift_;
};  // end of class ScaleFloat

template <typename T, typename U>
kleidicv_error_t scale_sc(const T *src, size_t src_stride, U *dst,
                          size_t dst_stride, size_t width, size_t height,
                          double scale, double shift) KLEIDICV_STREAMING;

// Specialization for float
template <>
kleidicv_error_t scale_sc(const float *src, size_t src_stride, float *dst,
                          size_t dst_stride, size_t width, size_t height,
                          double scale, double shift) KLEIDICV_STREAMING {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  Rectangle rect{width, height};
  Rows<const float> src_rows{src, src_stride};
  Rows<float> dst_rows{dst, dst_stride};
  svfloat32_t svscale = svdup_f32(static_cast<float>(scale));
  svfloat32_t svshift = svdup_f32(static_cast<float>(shift));
  if (scale == 1.0) {
    AddFloat operation(svshift);
    apply_operation_by_rows(operation, rect, src_rows, dst_rows);
  } else {
    ScaleFloat operation(svscale, svshift);
    apply_operation_by_rows(operation, rect, src_rows, dst_rows);
  }
  return KLEIDICV_OK;
}

// -----------------------------------------------------------------------
// Scale uint8 to float16
// -----------------------------------------------------------------------

class ScaleUint8ToFloat16Calc16 {
 public:
  using SrcType = uint8_t;
  using SrcVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<SrcType>;
  using SrcVectorType = typename SrcVecTraits::VectorType;
  using SrcVector2Type = typename SrcVecTraits::Vector2Type;
  using DstType = float16_t;
  using DstVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<DstType>;
  using DstVectorType = typename DstVecTraits::VectorType;
  using DstVector2Type = typename DstVecTraits::Vector2Type;
  using DstVector4Type = typename DstVecTraits::Vector4Type;

  ScaleUint8ToFloat16Calc16(double scale, double shift, svfloat16_t &svscale,
                            svfloat16_t &svshift) KLEIDICV_STREAMING
      : svscale_{svscale},
        svshift_{svshift} {
    svscale_ = svdup_n_f16(static_cast<float16_t>(scale));
    svshift_ = svdup_n_f16(static_cast<float16_t>(shift));
  }

  void process_row(size_t width, Columns<const SrcType> src,
                   Columns<DstType> dst) const KLEIDICV_STREAMING {
    svbool_t p8 = svptrue_b8();
    svbool_t p16 = svptrue_b16();
    svuint8_t svzero = svdup_n_u8(0);
#if KLEIDICV_TARGET_SME2
    svcount_t pc8 = SrcVecTraits::svptrue_c();
    svcount_t pc16 = DstVecTraits::svptrue_c();
#endif
    auto vector_path = [&](svuint16_t src) KLEIDICV_STREAMING {
      svfloat16_t fsrc = svcvt_f16_x(p16, src);
      return svmla_f16_x(p16, svshift_, fsrc, svscale_);
    };

    LoopUnroll{width, SrcVecTraits::num_lanes()}
        .unroll_twice([&](size_t step) KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME2
          SrcVector2Type src_2vec = svld1_x2(pc8, &src[0]);
          svuint8_t src0 = svget2(src_2vec, 0);
          svuint8_t src1 = svget2(src_2vec, 1);
#else
          svuint8_t src0 = svld1(p8, &src[0]);
          svuint8_t src1 = svld1_vnum(p8, &src[0], 1);
#endif  // KLEIDICV_TARGET_SME2
          DstVectorType dst00 =
              vector_path(svreinterpret_u16_u8(svzip1(src0, svzero)));
          DstVectorType dst01 =
              vector_path(svreinterpret_u16_u8(svzip2(src0, svzero)));
          DstVectorType dst02 =
              vector_path(svreinterpret_u16_u8(svzip1(src1, svzero)));
          DstVectorType dst03 =
              vector_path(svreinterpret_u16_u8(svzip2(src1, svzero)));
#if KLEIDICV_TARGET_SME2
          DstVector4Type dst4 = svcreate4(dst00, dst01, dst02, dst03);
          svst1(pc16, &dst[0], dst4);
#else
          svst1(p16, &dst[0], dst00);
          svst1_vnum(p16, &dst[0], 1, dst01);
          svst1_vnum(p16, &dst[0], 2, dst02);
          svst1_vnum(p16, &dst[0], 3, dst03);
#endif  // KLEIDICV_TARGET_SME2
          src += ptrdiff_t(step);
          dst += ptrdiff_t(step);
        })
        .unroll_once([&](size_t step) KLEIDICV_STREAMING {
          svuint8_t src0 = svld1(p8, &src[0]);
          DstVectorType dst00 =
              vector_path(svreinterpret_u16_u8(svzip1(src0, svzero)));
          DstVectorType dst01 =
              vector_path(svreinterpret_u16_u8(svzip2(src0, svzero)));
#if KLEIDICV_TARGET_SME2
          DstVector2Type dst2 = svcreate2(dst00, dst01);
          svst1(pc16, &dst[0], dst2);
#else
          svst1(p16, &dst[0], dst00);
          svst1_vnum(p16, &dst[0], 1, dst01);
#endif  // KLEIDICV_TARGET_SME2
          src += ptrdiff_t(step);
          dst += ptrdiff_t(step);
        })
        .remaining([&](size_t length, size_t) KLEIDICV_STREAMING {
          disable_loop_vectorization();
          size_t step = svcnth();
          while (length > 0) {
            svbool_t p16 = svwhilelt_b16(static_cast<size_t>(0), length);
            svuint16_t src0 = svld1ub_u16(p16, &src[0]);
            DstVectorType dst0 = vector_path(src0);
            svst1(p16, &dst[0], dst0);
            src += ptrdiff_t(step);
            dst += ptrdiff_t(step);
            length -= std::min<size_t>(length, step);
          }
        });
  }

  svfloat16_t &svscale_, &svshift_;
};  // end of class ScaleUint8ToFloat16Calc16

class ScaleUint8ToFloat16Calc32 {
 public:
  using SrcType = uint8_t;
  using SrcVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<SrcType>;
  using SrcVectorType = typename SrcVecTraits::VectorType;
  using SrcVector2Type = typename SrcVecTraits::Vector2Type;
  using DstType = float16_t;
  using DstVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<DstType>;
  using DstVectorType = typename DstVecTraits::VectorType;
  using DstVector2Type = typename DstVecTraits::Vector2Type;
  using DstVector4Type = typename DstVecTraits::Vector4Type;

  ScaleUint8ToFloat16Calc32(double scale, double shift, svfloat32_t &svscale,
                            svfloat32_t &svshift) KLEIDICV_STREAMING
      : svscale_{svscale},
        svshift_{svshift} {
    svscale_ = svdup_n_f32(static_cast<float>(scale));
    svshift_ = svdup_n_f32(static_cast<float>(shift));
  }

  void process_row(size_t width, Columns<const SrcType> src,
                   Columns<DstType> dst) const KLEIDICV_STREAMING {
    svbool_t p8 = svptrue_b8();
    svbool_t p16 = svptrue_b16();
    svbool_t p32 = svptrue_b32();
    svuint8_t svzero = svdup_n_u8(0);
#if KLEIDICV_TARGET_SME2
    svcount_t pc8 = SrcVecTraits::svptrue_c();
    svcount_t pc16 = DstVecTraits::svptrue_c();
#endif
    auto vector_path = [&](svuint8_t src) KLEIDICV_STREAMING {
      svuint8_t src_b = svtrn1(src, svzero);
      svuint8_t src_t = svtrn2(src, svzero);
      svuint8_t src_b0 = svzip1(src_b, svzero);
      svuint8_t src_b1 = svzip2(src_b, svzero);
      svuint8_t src_t0 = svzip1(src_t, svzero);
      svuint8_t src_t1 = svzip2(src_t, svzero);
      svfloat32_t fsrc_b0 = svcvt_f32_x(p32, svreinterpret_u32_u8(src_b0));
      svfloat32_t fsrc_b1 = svcvt_f32_x(p32, svreinterpret_u32_u8(src_b1));
      svfloat32_t fsrc_t0 = svcvt_f32_x(p32, svreinterpret_u32_u8(src_t0));
      svfloat32_t fsrc_t1 = svcvt_f32_x(p32, svreinterpret_u32_u8(src_t1));
      svfloat32_t res_b0 = svmla_f32_x(p32, svshift_, fsrc_b0, svscale_);
      svfloat32_t res_b1 = svmla_f32_x(p32, svshift_, fsrc_b1, svscale_);
      svfloat32_t res_t0 = svmla_f32_x(p32, svshift_, fsrc_t0, svscale_);
      svfloat32_t res_t1 = svmla_f32_x(p32, svshift_, fsrc_t1, svscale_);
      svfloat16_t res0 = svcvtnt_f16_x(svcvt_f16_x(p16, res_b0), p16, res_t0);
      svfloat16_t res1 = svcvtnt_f16_x(svcvt_f16_x(p16, res_b1), p16, res_t1);
      return svcreate2(res0, res1);
    };

    LoopUnroll{width, SrcVecTraits::num_lanes()}
        .unroll_twice([&](size_t step) KLEIDICV_STREAMING {
#if KLEIDICV_TARGET_SME2
          SrcVector2Type src_2vec = svld1_x2(pc8, &src[0]);
          svuint8_t src0 = svget2(src_2vec, 0);
          svuint8_t src1 = svget2(src_2vec, 1);
#else
          svuint8_t src0 = svld1(p8, &src[0]);
          svuint8_t src1 = svld1_vnum(p8, &src[0], 1);
#endif  // KLEIDICV_TARGET_SME2
          DstVector2Type dst0 = vector_path(src0);
          DstVector2Type dst1 = vector_path(src1);
#if KLEIDICV_TARGET_SME2
          DstVector4Type dst4 = svcreate4(svget2(dst0, 0), svget2(dst0, 1),
                                          svget2(dst1, 0), svget2(dst1, 1));
          svst1(pc16, &dst[0], dst4);
#else
          svst1(p16, &dst[0], svget2(dst0, 0));
          svst1_vnum(p16, &dst[0], 1, svget2(dst0, 1));
          svst1_vnum(p16, &dst[0], 2, svget2(dst1, 0));
          svst1_vnum(p16, &dst[0], 3, svget2(dst1, 1));
#endif  // KLEIDICV_TARGET_SME2
          src += ptrdiff_t(step);
          dst += ptrdiff_t(step);
        })
        .unroll_once([&](size_t step) KLEIDICV_STREAMING {
          svuint8_t src0 = svld1(p8, &src[0]);
          DstVector2Type dst0 = vector_path(src0);
#if KLEIDICV_TARGET_SME2
          svst1(pc16, &dst[0], dst0);
#else
          svst1(p16, &dst[0], svget2(dst0, 0));
          svst1_vnum(p16, &dst[0], 1, svget2(dst0, 1));
#endif  // KLEIDICV_TARGET_SME2
          src += ptrdiff_t(step);
          dst += ptrdiff_t(step);
        })
        .remaining([&](size_t length, size_t) KLEIDICV_STREAMING {
          size_t step = svcnth();
          while (length > 0) {
            disable_loop_vectorization();
            svbool_t p16 = svwhilelt_b16(static_cast<size_t>(0), length);
            svuint16_t src0 = svld1ub_u16(p16, &src[0]);
            svuint16_t src_b = svtrn1(src0, svreinterpret_u16_u8(svzero));
            svuint16_t src_t = svtrn2(src0, svreinterpret_u16_u8(svzero));
            svfloat32_t fsrc_b = svcvt_f32_x(p32, svreinterpret_u32_u16(src_b));
            svfloat32_t fsrc_t = svcvt_f32_x(p32, svreinterpret_u32_u16(src_t));
            svfloat32_t res_b = svmla_f32_x(p32, svshift_, fsrc_b, svscale_);
            svfloat32_t res_t = svmla_f32_x(p32, svshift_, fsrc_t, svscale_);
            svfloat16_t res =
                svcvtnt_f16_x(svcvt_f16_x(p16, res_b), p16, res_t);
            svst1(p16, &dst[0], res);
            src += ptrdiff_t(step);
            dst += ptrdiff_t(step);
            length -= std::min<size_t>(length, step);
          }
        });
  }

  svfloat32_t &svscale_, &svshift_;
};  // end of class ScaleUint8ToFloat16Calc32

template <size_t SvCntH>
class ScaleUint8ToFloat16Table {
 public:
  using SrcType = uint8_t;
  using SrcVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<SrcType>;
  using SrcVectorType = typename SrcVecTraits::VectorType;
  using SrcVector2Type = typename SrcVecTraits::Vector2Type;
  using DstType = float16_t;
  using DstVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<DstType>;
  using DstVectorType = typename DstVecTraits::VectorType;
  using DstVector2Type = typename DstVecTraits::Vector2Type;
  using DstVector4Type = typename DstVecTraits::Vector4Type;

  static constexpr size_t NTblVectors = (256 + SvCntH - 1) / SvCntH;
  ScaleUint8ToFloat16Table(const float16_t precalc[256],
                           std::reference_wrapper<svfloat16_t> svt[NTblVectors])
      KLEIDICV_STREAMING : st_{svt} {
    size_t c = 0;
    KLEIDICV_FORCE_LOOP_UNROLL
    for (uint64_t i = 0; i < 256; i += SvCntH) {
      svbool_t pg = svwhilelt_b16(i, 256ULL);
      st_[c++].get() = svld1(pg, precalc + i);
    }
  }

  void process_row(size_t width, Columns<const SrcType> src,
                   Columns<DstType> dst) const KLEIDICV_STREAMING {
    svbool_t p8 = svptrue_b8();
    svbool_t p16 = svptrue_b16();
    svuint8_t svzero = svdup_n_u8(0);
#if KLEIDICV_TARGET_SME2
    svcount_t pc16 = DstVecTraits::svptrue_c();
#endif
    auto lookup = [&](svuint16_t src) KLEIDICV_STREAMING {
      svfloat16_t res = svtbl2(svcreate2(st_[0], st_[1]), src);
      constexpr uint16_t step = SvCntH;
      src = svsub_n_u16_x(p16, src, 2 * step);
      size_t c = 2;
      uint64_t i = 2ULL * step;
      KLEIDICV_FORCE_LOOP_UNROLL
      for (; i < 256; i += step) {
        res = svtbx(res, st_[c++].get(), src);
        src = svsub_n_u16_x(p16, src, step);
      }
      return res;
    };

    LoopUnroll{width, SrcVecTraits::num_lanes()}
        .unroll_once([&](size_t step) KLEIDICV_STREAMING {
          svuint8_t src0 = svld1(p8, &src[0]);
          DstVectorType dst0 =
              lookup(svreinterpret_u16_u8(svzip1(src0, svzero)));
          DstVectorType dst1 =
              lookup(svreinterpret_u16_u8(svzip2(src0, svzero)));
#if KLEIDICV_TARGET_SME2
          svst1(pc16, &dst[0], svcreate2(dst0, dst1));
#else
          svst1(p16, &dst[0], dst0);
          svst1_vnum(p16, &dst[0], 1, dst1);
#endif  // KLEIDICV_TARGET_SME2
          src += ptrdiff_t(step);
          dst += ptrdiff_t(step);
        })
        .remaining([&](size_t length, size_t) KLEIDICV_STREAMING {
          size_t step = SvCntH;
          while (length > 0) {
            disable_loop_vectorization();
            svbool_t p16 = svwhilelt_b16(static_cast<size_t>(0), length);
            svuint16_t src0 = svld1ub_u16(p16, &src[0]);
            svfloat16_t res = lookup(src0);
            svst1(p16, &dst[0], res);
            src += ptrdiff_t(step);
            dst += ptrdiff_t(step);
            length -= std::min<size_t>(length, step);
          }
        });
  }

 private:
  std::reference_wrapper<svfloat16_t> *st_;
};  // end of class ScaleUint8ToFloat16Table

#if !KLEIDICV_TARGET_SME && !KLEIDICV_TARGET_SME2
class ScaleUint8ToFloat16Gather {
 public:
  using SrcType = uint8_t;
  using SrcVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<SrcType>;
  using SrcVectorType = typename SrcVecTraits::VectorType;
  using SrcVector2Type = typename SrcVecTraits::Vector2Type;
  using DstType = float16_t;
  using DstVecTraits = KLEIDICV_TARGET_NAMESPACE::VecTraits<DstType>;
  using DstVectorType = typename DstVecTraits::VectorType;
  using DstVector2Type = typename DstVecTraits::Vector2Type;
  using DstVector4Type = typename DstVecTraits::Vector4Type;

  explicit ScaleUint8ToFloat16Gather(const float16_t precalc[256])
      KLEIDICV_STREAMING : sct_{precalc} {}

  void process_row(size_t width, Columns<const SrcType> src,
                   Columns<DstType> dst) const KLEIDICV_STREAMING {
    svbool_t p8 = svptrue_b8();
    svbool_t p16 = svptrue_b16();
    svbool_t p32 = svptrue_b32();
    svuint8_t svzero = svdup_n_u8(0);
#if KLEIDICV_TARGET_SME2
    svcount_t pc16 = DstVecTraits::svptrue_c();
#endif
    auto lookup = [&](svuint16_t src) KLEIDICV_STREAMING {
      svuint32_t src0 =
          svreinterpret_u32_u16(svzip1(src, svreinterpret_u16_u8(svzero)));
      svuint32_t src1 =
          svreinterpret_u32_u16(svzip2(src, svreinterpret_u16_u8(svzero)));

      svuint32_t res0 = svld1uh_gather_index_u32(
          p32, reinterpret_cast<const uint16_t *>(sct_), src0);
      svuint32_t res1 = svld1uh_gather_index_u32(
          p32, reinterpret_cast<const uint16_t *>(sct_), src1);

      return svreinterpret_f16_u16(
          svuzp1_u16(svreinterpret_u16_u32(res0), svreinterpret_u16_u32(res1)));
    };

    LoopUnroll{width, SrcVecTraits::num_lanes()}
        .unroll_once([&](size_t step) KLEIDICV_STREAMING {
          svuint8_t src0 = svld1(p8, &src[0]);
          DstVectorType dst0 =
              lookup(svreinterpret_u16_u8(svzip1(src0, svzero)));
          DstVectorType dst1 =
              lookup(svreinterpret_u16_u8(svzip2(src0, svzero)));
#if KLEIDICV_TARGET_SME2
          svst1(pc16, &dst[0], svcreate2(dst0, dst1));
#else
          svst1(p16, &dst[0], dst0);
          svst1_vnum(p16, &dst[0], 1, dst1);
#endif  // KLEIDICV_TARGET_SME2
          src += ptrdiff_t(step);
          dst += ptrdiff_t(step);
        })
        .remaining([&](size_t length, size_t) KLEIDICV_STREAMING {
          size_t step = svcnth();
          while (length > 0) {
            disable_loop_vectorization();
            svbool_t p16 = svwhilelt_b16(static_cast<size_t>(0), length);
            svuint16_t src0 = svld1ub_u16(p16, &src[0]);
            svfloat16_t res = lookup(src0);
            svst1(p16, &dst[0], res);
            src += ptrdiff_t(step);
            dst += ptrdiff_t(step);
            length -= std::min<size_t>(length, step);
          }
        });
  }

 private:
  const float16_t *sct_;  /// SCT = SCale Table
};  // end of class ScaleUint8ToFloat16Gather
#endif

kleidicv_error_t scale_with_precalculated_table_u8_f16(
    const uint8_t *src, size_t src_stride, float16_t *dst, size_t dst_stride,
    size_t width, size_t height,
    const std::array<float16_t, 256> &precalculated_table) KLEIDICV_STREAMING {
  Rectangle rect{width, height};
  Rows<const uint8_t> src_rows{src, src_stride};
  Rows<float16_t> dst_rows{dst, dst_stride};
  if (svcnth() == 8) {
    svfloat16_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14,
        s15, s16, s17, s18, s19, s20, s21, s22, s23, s24, s25, s26, s27, s28,
        s29, s30, s31;
    std::reference_wrapper<svfloat16_t> st[32] = {
        s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7,  s8,  s9,  s10,
        s11, s12, s13, s14, s15, s16, s17, s18, s19, s20, s21,
        s22, s23, s24, s25, s26, s27, s28, s29, s30, s31};
    ScaleUint8ToFloat16Table<8> operation(precalculated_table.data(), st);
    zip_rows(operation, rect, src_rows, dst_rows);
  } else if (svcnth() == 16) {
    svfloat16_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14,
        s15;
    std::reference_wrapper<svfloat16_t> st[16] = {
        s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15};
    ScaleUint8ToFloat16Table<16> operation(precalculated_table.data(), st);
    zip_rows(operation, rect, src_rows, dst_rows);
  } else if (svcnth() == 24) {
    svfloat16_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    std::reference_wrapper<svfloat16_t> st[11] = {s0, s1, s2, s3, s4, s5,
                                                  s6, s7, s8, s9, s10};
    ScaleUint8ToFloat16Table<24> operation(precalculated_table.data(), st);
    zip_rows(operation, rect, src_rows, dst_rows);
  } else if (svcnth() == 32) {
    svfloat16_t s0, s1, s2, s3, s4, s5, s6, s7;
    std::reference_wrapper<svfloat16_t> st[8] = {s0, s1, s2, s3,
                                                 s4, s5, s6, s7};
    ScaleUint8ToFloat16Table<32> operation(precalculated_table.data(), st);
    zip_rows(operation, rect, src_rows, dst_rows);
    /*  } else if (svcnth() == 40) {
        svfloat16_t s0, s1, s2, s3, s4, s5, s6;
        std::reference_wrapper<svfloat16_t> st[7] = {s0, s1, s2, s3, s4, s5,
      s6}; ScaleUint8ToFloat16Table<40> operation(precalculated_table.data(),
      st); zip_rows(operation, rect, src_rows, dst_rows); } else if (svcnth() ==
      48) { svfloat16_t s0, s1, s2, s3, s4, s5;
        std::reference_wrapper<svfloat16_t> st[6] = {s0, s1, s2, s3, s4, s5};
        ScaleUint8ToFloat16Table<48> operation(precalculated_table.data(), st);
        zip_rows(operation, rect, src_rows, dst_rows);
      } else if (svcnth() == 56) {
        svfloat16_t s0, s1, s2, s3, s4;
        std::reference_wrapper<svfloat16_t> st[5] = {s0, s1, s2, s3, s4};
        ScaleUint8ToFloat16Table<56> operation(precalculated_table.data(), st);
        zip_rows(operation, rect, src_rows, dst_rows);
      } else if (svcnth() == 64) {
        svfloat16_t s0, s1, s2, s3;
        std::reference_wrapper<svfloat16_t> st[4] = {s0, s1, s2, s3};
        ScaleUint8ToFloat16Table<64> operation(precalculated_table.data(), st);
        zip_rows(operation, rect, src_rows, dst_rows);
      } else if (svcnth() == 72) {
        svfloat16_t s0, s1, s2, s3;
        std::reference_wrapper<svfloat16_t> st[4] = {s0, s1, s2, s3};
        ScaleUint8ToFloat16Table<72> operation(precalculated_table.data(), st);
        zip_rows(operation, rect, src_rows, dst_rows);
      } else if (svcnth() == 80) {
        svfloat16_t s0, s1, s2, s3;
        std::reference_wrapper<svfloat16_t> st[4] = {s0, s1, s2, s3};
        ScaleUint8ToFloat16Table<80> operation(precalculated_table.data(), st);
        zip_rows(operation, rect, src_rows, dst_rows);
      } else if (svcnth() == 88) {
        svfloat16_t s0, s1, s2;
        std::reference_wrapper<svfloat16_t> st[3] = {s0, s1, s2};
        ScaleUint8ToFloat16Table<88> operation(precalculated_table.data(), st);
        zip_rows(operation, rect, src_rows, dst_rows);
      } else if (svcnth() == 96) {
        svfloat16_t s0, s1, s2;
        std::reference_wrapper<svfloat16_t> st[3] = {s0, s1, s2};
        ScaleUint8ToFloat16Table<96> operation(precalculated_table.data(), st);
        zip_rows(operation, rect, src_rows, dst_rows);
      } else if (svcnth() == 104) {
        svfloat16_t s0, s1, s2;
        std::reference_wrapper<svfloat16_t> st[3] = {s0, s1, s2};
        ScaleUint8ToFloat16Table<104> operation(precalculated_table.data(), st);
        zip_rows(operation, rect, src_rows, dst_rows);
      } else if (svcnth() == 112) {
        svfloat16_t s0, s1, s2;
        std::reference_wrapper<svfloat16_t> st[3] = {s0, s1, s2};
        ScaleUint8ToFloat16Table<112> operation(precalculated_table.data(), st);
        zip_rows(operation, rect, src_rows, dst_rows);
      } else if (svcnth() == 120) {
        svfloat16_t s0, s1, s2;
        std::reference_wrapper<svfloat16_t> st[3] = {s0, s1, s2};
        ScaleUint8ToFloat16Table<120> operation(precalculated_table.data(), st);
        zip_rows(operation, rect, src_rows, dst_rows);
    */
  } else if (svcnth() == 128) {
    svfloat16_t s0, s1;
    std::reference_wrapper<svfloat16_t> st[2] = {s0, s1};
    ScaleUint8ToFloat16Table<128> operation(precalculated_table.data(), st);
    zip_rows(operation, rect, src_rows, dst_rows);
  } else {
    assert(false);
  }
  return KLEIDICV_OK;
}

std::array<float16_t, 256> precalculate_scale_table_u8_f16(
    double dscale, double dshift) KLEIDICV_STREAMING {
  static constexpr size_t TableLength = 256;
  std::array<float16_t, TableLength> precalculated_table{};
  svuint16_t counter = svindex_u16(0, 1);
  svuint16_t svstep = svdup_n_u16(svcnth());
  svbool_t p32 = svptrue_b32();
  svbool_t p16 = svptrue_b16();
  size_t step = svcnth();
  svfloat32_t svscale = svdup_n_f32(static_cast<float>(dscale));
  svfloat32_t svshift = svdup_n_f32(static_cast<float>(dshift));
  svuint16_t svzero = svdup_n_u16(0);
  for (size_t i = 0; i < TableLength; i += step) {
    svuint16_t cnt_b = svtrn1(counter, svzero);
    svuint16_t cnt_t = svtrn2(counter, svzero);
    counter = svadd_x(p16, counter, svstep);
    svfloat32_t fcnt_b = svcvt_f32_x(p32, svreinterpret_u32_u16(cnt_b));
    svfloat32_t fcnt_t = svcvt_f32_x(p32, svreinterpret_u32_u16(cnt_t));
    svfloat32_t res_b = svmla_f32_x(p32, svshift, fcnt_b, svscale);
    svfloat32_t res_t = svmla_f32_x(p32, svshift, fcnt_t, svscale);
    svfloat16_t res = svcvtnt_f16_x(svcvt_f16_x(p16, res_b), p16, res_t);
    svst1(p16, &precalculated_table[i], res);
  }
  return precalculated_table;
}

// Specialization for uint8_t to float16_t
template <>
kleidicv_error_t scale_sc(const uint8_t *src, size_t src_stride, float16_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          double scale, double shift) KLEIDICV_STREAMING {
  CHECK_POINTER_AND_STRIDE(src, src_stride, height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, height);
  CHECK_IMAGE_SIZE(width, height);

  // For smaller inputs, the full calculation is the faster
  if (width * height < 60000) {  // empirical value
    Rectangle rect{width, height};
    Rows<const uint8_t> src_rows{src, src_stride};
    Rows<float16_t> dst_rows{dst, dst_stride};
    if (static_cast<double>(static_cast<float16_t>(scale)) == scale &&
        static_cast<double>(static_cast<float16_t>(shift)) == shift) {
      svfloat16_t s0, s1;
      ScaleUint8ToFloat16Calc16 operation(scale, shift, s0, s1);
      zip_rows(operation, rect, src_rows, dst_rows);
    } else {
      svfloat32_t s0, s1;
      ScaleUint8ToFloat16Calc32 operation(scale, shift, s0, s1);
      zip_rows(operation, rect, src_rows, dst_rows);
    }
  } else {
    // For bigger inputs, it's faster to pre-calculate the table
    // and map those values during the run
    auto precalculated_table = precalculate_scale_table_u8_f16(scale, shift);
#if KLEIDICV_TARGET_SME || KLEIDICV_TARGET_SME2
    return scale_with_precalculated_table_u8_f16(
        src, src_stride, dst, dst_stride, width, height, precalculated_table);
#else
    Rectangle rect{width, height};
    Rows<const uint8_t> src_rows{src, src_stride};
    Rows<float16_t> dst_rows{dst, dst_stride};
    ScaleUint8ToFloat16Gather operation(precalculated_table.data());
    zip_rows(operation, rect, src_rows, dst_rows);
#endif
  }

  return KLEIDICV_OK;
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_SCALE_SC_H
