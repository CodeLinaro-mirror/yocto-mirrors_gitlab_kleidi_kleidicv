// SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_REMAP_SC_H
#define KLEIDICV_REMAP_SC_H

#include <arm_sve.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "common_sc.h"
#include "kleidicv/sve2.h"
#include "kleidicv/transform/remap.h"

namespace KLEIDICV_TARGET_NAMESPACE {

template <typename ScalarType>
class RemapS16Replicate {
 public:
  using MapVecTraits = VecTraits<int16_t>;
  using MapVectorType = typename MapVecTraits::VectorType;
  using MapVector2Type = typename MapVecTraits::Vector2Type;

  RemapS16Replicate(Rows<const ScalarType> src_rows, size_t src_width,
                    size_t src_height, svuint16_t& v_src_element_stride,
                    MapVectorType& v_x_max, MapVectorType& v_y_max)
      : src_rows_{src_rows},
        v_src_element_stride_{v_src_element_stride},
        v_xmax_{v_x_max},
        v_ymax_{v_y_max} {
    v_src_element_stride_ = svdup_u16(src_rows.stride() / sizeof(ScalarType));
    v_xmax_ = svdup_s16(static_cast<int16_t>(src_width - 1));
    v_ymax_ = svdup_s16(static_cast<int16_t>(src_height - 1));
  }

  void transform_pixels(svbool_t pg, svuint32_t offsets_b, svbool_t pg_b,
                        svuint32_t offsets_t, svbool_t pg_t,
                        Columns<ScalarType> dst);

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<ScalarType> dst) {
    svuint32_t offsets_b, offsets_t;
    svint16_t svzero = svdup_n_s16(0);
    auto load_offsets = [&](svbool_t pg) {
      MapVector2Type xy = svld2_s16(pg, &mapxy[0]);
      // Clamp coordinates to within the dimensions of the source image
      svuint16_t x = svreinterpret_u16_s16(
          svmax_x(pg, svzero, svmin_x(pg, svget2(xy, 0), v_xmax_)));
      svuint16_t y = svreinterpret_u16_s16(
          svmax_x(pg, svzero, svmin_x(pg, svget2(xy, 1), v_ymax_)));
      // Calculate offsets from coordinates (y * stride/sizeof(ScalarType) + x)
      offsets_b = svmlalb_u32(svmovlb_u32(x), y, v_src_element_stride_);
      offsets_t = svmlalt_u32(svmovlt_u32(x), y, v_src_element_stride_);
    };

    svbool_t pg_all16 = MapVecTraits::svptrue();
    svbool_t pg_all32 = svptrue_b32();

    auto gather_load_generic_vector_path = [&](svbool_t pg, ptrdiff_t step) {
      load_offsets(pg);
      svbool_t pg_b = svwhilelt_b32(int64_t{0}, (step + 1) / 2);
      svbool_t pg_t = svwhilelt_b32(int64_t{0}, step / 2);
      transform_pixels(pg, offsets_b, pg_b, offsets_t, pg_t, dst);
      mapxy += step;
      dst += step;
    };

    // NOTE: gather load is not available in streaming mode
    auto gather_load_full_vector_path = [&](ptrdiff_t step) {
      load_offsets(pg_all16);
      transform_pixels(pg_all16, offsets_b, pg_all32, offsets_t, pg_all32, dst);
      mapxy += step;
      dst += step;
    };

    LoopUnroll loop{width, MapVecTraits::num_lanes()};
    loop.unroll_once([&](size_t step) {
      gather_load_full_vector_path(static_cast<ptrdiff_t>(step));
    });
    loop.remaining([&](size_t length, size_t step) {
      svbool_t pg = MapVecTraits::svwhilelt(step - length, step);
      gather_load_generic_vector_path(pg, static_cast<ptrdiff_t>(length));
    });
  }

 private:
  Rows<const ScalarType> src_rows_;
  svuint16_t& v_src_element_stride_;
  MapVectorType& v_xmax_;
  MapVectorType& v_ymax_;
};  // end of class RemapS16Replicate<ScalarType>

template <>
void RemapS16Replicate<uint8_t>::transform_pixels(
    svbool_t pg, svuint32_t offsets_b, svbool_t pg_b, svuint32_t offsets_t,
    svbool_t pg_t, Columns<uint8_t> dst) {
  // Copy pixels from source
  svuint32_t result_b =
      svld1ub_gather_u32offset_u32(pg_b, &src_rows_[0], offsets_b);
  svuint32_t result_t =
      svld1ub_gather_u32offset_u32(pg_t, &src_rows_[0], offsets_t);
  svuint16_t result = svtrn1_u16(svreinterpret_u16_u32(result_b),
                                 svreinterpret_u16_u32(result_t));

  svst1b_u16(pg, &dst[0], result);
}

template <>
void RemapS16Replicate<uint16_t>::transform_pixels(
    svbool_t pg, svuint32_t offsets_b, svbool_t pg_b, svuint32_t offsets_t,
    svbool_t pg_t, Columns<uint16_t> dst) {
  // Account for the size of the source type when calculating offset
  offsets_b = svlsl_n_u32_x(pg, offsets_b, 1);
  offsets_t = svlsl_n_u32_x(pg, offsets_t, 1);

  // Copy pixels from source
  svuint32_t result_b =
      svld1uh_gather_u32offset_u32(pg_b, &src_rows_[0], offsets_b);
  svuint32_t result_t =
      svld1uh_gather_u32offset_u32(pg_t, &src_rows_[0], offsets_t);
  svuint16_t result = svtrn1_u16(svreinterpret_u16_u32(result_b),
                                 svreinterpret_u16_u32(result_t));

  svst1_u16(pg, &dst[0], result);
}

template <typename ScalarType>
class RemapS16ConstantBorder {
 public:
  RemapS16ConstantBorder(Rows<const ScalarType> src_rows, size_t src_width,
                         size_t src_height, const ScalarType* border_value,
                         svuint16_t& v_src_element_stride, svuint16_t& v_width,
                         svuint16_t& v_height, svuint16_t& v_border)
      : src_rows_{src_rows},
        v_src_element_stride_{v_src_element_stride},
        v_width_{v_width},
        v_height_{v_height},
        v_border_{v_border} {
    v_src_element_stride_ = svdup_u16(src_rows.stride() / sizeof(ScalarType));
    v_width_ = svdup_u16(static_cast<uint16_t>(src_width));
    v_height_ = svdup_u16(static_cast<uint16_t>(src_height));
    v_border_ = svdup_u16(*border_value);
  }

  void transform_pixels(svbool_t pg, svuint32_t offsets_b, svbool_t pg_b,
                        svuint32_t offsets_t, svbool_t pg_t, ScalarType* dst);

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<ScalarType> dst) {
    for (size_t i = 0; i < width; i += svcnth()) {
      svbool_t pg = svwhilelt_b16(i, width);

      svint16x2_t xy = svld2_s16(pg, &mapxy[static_cast<ptrdiff_t>(i * 2)]);
      svuint16_t x = svreinterpret_u16_s16(svget2(xy, 0));
      svuint16_t y = svreinterpret_u16_s16(svget2(xy, 1));

      // Find whether coordinates are within the image dimensions.
      svbool_t in_range = svand_b_z(pg, svcmplt_u16(pg, x, v_width_),
                                    svcmplt_u16(pg, y, v_height_));
      svbool_t pg_b = in_range;
      svbool_t pg_t = svtrn2_b16(in_range, svpfalse());

      // Calculate offsets from coordinates (y * stride/sizeof(ScalarType) + x)
      svuint32_t offsets_b =
          svmlalb_u32(svmovlb_u32(x), y, v_src_element_stride_);
      svuint32_t offsets_t =
          svmlalt_u32(svmovlt_u32(x), y, v_src_element_stride_);

      transform_pixels(pg, offsets_b, pg_b, offsets_t, pg_t,
                       &dst[static_cast<ptrdiff_t>(i)]);
    }
  }

 private:
  Rows<const ScalarType> src_rows_;
  svuint16_t& v_src_element_stride_;
  svuint16_t& v_width_;
  svuint16_t& v_height_;
  svuint16_t& v_border_;
};  // end of class RemapS16ConstantBorder<ScalarType>

template <>
void RemapS16ConstantBorder<uint8_t>::transform_pixels(
    svbool_t pg, svuint32_t offsets_b, svbool_t pg_b, svuint32_t offsets_t,
    svbool_t pg_t, uint8_t* dst) {
  // Copy pixels from source
  svuint32_t result_b =
      svld1ub_gather_u32offset_u32(pg_b, &src_rows_[0], offsets_b);
  svuint32_t result_t =
      svld1ub_gather_u32offset_u32(pg_t, &src_rows_[0], offsets_t);

  svuint16_t result = svtrn1_u16(svreinterpret_u16_u32(result_b),
                                 svreinterpret_u16_u32(result_t));

  svuint16_t result_selected = svsel(pg_b, result, v_border_);
  svst1b_u16(pg, dst, result_selected);
}

template <>
void RemapS16ConstantBorder<uint16_t>::transform_pixels(
    svbool_t pg, svuint32_t offsets_b, svbool_t pg_b, svuint32_t offsets_t,
    svbool_t pg_t, uint16_t* dst) {
  // Account for the size of the source type when calculating offset
  offsets_b = svlsl_n_u32_x(pg, offsets_b, 1);
  offsets_t = svlsl_n_u32_x(pg, offsets_t, 1);

  // Copy pixels from source
  svuint32_t result_b =
      svld1uh_gather_u32offset_u32(pg_b, &src_rows_[0], offsets_b);
  svuint32_t result_t =
      svld1uh_gather_u32offset_u32(pg_t, &src_rows_[0], offsets_t);

  svuint16_t result = svtrn1_u16(svreinterpret_u16_u32(result_b),
                                 svreinterpret_u16_u32(result_t));

  svuint16_t result_selected = svsel(pg_b, result, v_border_);
  svst1_u16(pg, dst, result_selected);
}

// Most of the complexity comes from parameter checking.
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <typename T>
kleidicv_error_t remap_s16_sc(const T* src, size_t src_stride, size_t src_width,
                              size_t src_height, T* dst, size_t dst_stride,
                              size_t dst_width, size_t dst_height,
                              size_t channels, const int16_t* mapxy,
                              size_t mapxy_stride,
                              kleidicv_border_type_t border_type,
                              const T* border_value) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_POINTER_AND_STRIDE(mapxy, mapxy_stride, dst_height);
  CHECK_IMAGE_SIZE(src_width, src_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);
  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT && nullptr == border_value) {
    return KLEIDICV_ERROR_NULL_POINTER;
  }

  if (!remap_s16_is_implemented<T>(src_stride, src_width, src_height, dst_width,
                                   border_type, channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  Rows<const T> src_rows{src, src_stride, channels};
  Rows<const int16_t> mapxy_rows{mapxy, mapxy_stride, 2};
  Rows<T> dst_rows{dst, dst_stride, channels};
  svuint16_t sv_src_element_stride;
  Rectangle rect{dst_width, dst_height};
  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT) {
    svuint16_t sv_width, sv_height, sv_border;
    RemapS16ConstantBorder<T> operation{
        src_rows, src_width, src_height, border_value, sv_src_element_stride,
        sv_width, sv_height, sv_border};
    zip_rows(operation, rect, mapxy_rows, dst_rows);
  } else {
    assert(border_type == KLEIDICV_BORDER_TYPE_REPLICATE);
    svint16_t sv_xmax, sv_ymax;
    RemapS16Replicate<T> operation{src_rows,   src_width,
                                   src_height, sv_src_element_stride,
                                   sv_xmax,    sv_ymax};
    zip_rows(operation, rect, mapxy_rows, dst_rows);
  }
  return KLEIDICV_OK;
}
// NOLINTEND(readability-function-cognitive-complexity)

template <typename ScalarType>
inline svuint16_t interpolate_16point5(svbool_t pg, svuint16_t frac,
                                       svuint16_t src_a, svuint16_t src_b,
                                       svuint16_t src_c, svuint16_t src_d,
                                       svuint32_t bias);

template <>
inline svuint16_t interpolate_16point5<uint8_t>(
    svbool_t pg, svuint16_t frac, svuint16_t src_a, svuint16_t src_b,
    svuint16_t src_c, svuint16_t src_d, svuint32_t bias) {
  svuint16_t xfrac = svand_x(pg, frac, svdup_n_u16(REMAP16POINT5_FRAC_MAX - 1));
  svuint16_t yfrac =
      svand_x(pg, svlsr_n_u16_x(pg, frac, REMAP16POINT5_FRAC_BITS),
              svdup_n_u16(REMAP16POINT5_FRAC_MAX - 1));
  svuint16_t nxfrac =
      svsub_u16_x(pg, svdup_n_u16(REMAP16POINT5_FRAC_MAX), xfrac);
  svuint16_t nyfrac =
      svsub_u16_x(pg, svdup_n_u16(REMAP16POINT5_FRAC_MAX), yfrac);
  svuint16_t line0 = svmla_x(pg, svmul_x(pg, xfrac, src_b), nxfrac, src_a);
  svuint16_t line1 = svmla_x(pg, svmul_x(pg, xfrac, src_d), nxfrac, src_c);

  svuint32_t acc_b = svmlalb_u32(bias, line0, nyfrac);
  svuint32_t acc_t = svmlalt_u32(bias, line0, nyfrac);
  acc_b = svmlalb_u32(acc_b, line1, yfrac);
  acc_t = svmlalt_u32(acc_t, line1, yfrac);

  return svshrnt(svshrnb(acc_b, 2ULL * REMAP16POINT5_FRAC_BITS), acc_t,
                 2ULL * REMAP16POINT5_FRAC_BITS);
}

template <>
inline svuint16_t interpolate_16point5<uint16_t>(
    svbool_t pg, svuint16_t frac, svuint16_t src_a, svuint16_t src_b,
    svuint16_t src_c, svuint16_t src_d, svuint32_t bias) {
  svuint16_t xfrac = svand_x(pg, frac, svdup_n_u16(REMAP16POINT5_FRAC_MAX - 1));
  svuint16_t yfrac =
      svand_x(pg, svlsr_n_u16_x(pg, frac, REMAP16POINT5_FRAC_BITS),
              svdup_n_u16(REMAP16POINT5_FRAC_MAX - 1));
  svuint16_t nxfrac =
      svsub_u16_x(pg, svdup_n_u16(REMAP16POINT5_FRAC_MAX), xfrac);
  svuint16_t nyfrac =
      svsub_u16_x(pg, svdup_n_u16(REMAP16POINT5_FRAC_MAX), yfrac);
  svuint32_t line0_b = svmla_x(pg, svmullb(xfrac, src_b), svmovlb_u32(nxfrac),
                               svmovlb_u32(src_a));
  svuint32_t line0_t = svmla_x(pg, svmullt(xfrac, src_b), svmovlt_u32(nxfrac),
                               svmovlt_u32(src_a));
  svuint32_t line1_b = svmla_x(pg, svmullb(xfrac, src_d), svmovlb_u32(nxfrac),
                               svmovlb_u32(src_c));
  svuint32_t line1_t = svmla_x(pg, svmullt(xfrac, src_d), svmovlt_u32(nxfrac),
                               svmovlt_u32(src_c));

  svuint32_t acc_b =
      svmla_u32_x(pg, svmla_u32_x(pg, bias, line0_b, svmovlb_u32(nyfrac)),
                  line1_b, svmovlb_u32(yfrac));
  svuint32_t acc_t =
      svmla_u32_x(pg, svmla_u32_x(pg, bias, line0_t, svmovlt_u32(nyfrac)),
                  line1_t, svmovlt_u32(yfrac));

  return svshrnt(svshrnb(acc_b, 2ULL * REMAP16POINT5_FRAC_BITS), acc_t,
                 2ULL * REMAP16POINT5_FRAC_BITS);
}

template <typename ScalarType>
class RemapS16Point5Replicate;

template <>
class RemapS16Point5Replicate<uint8_t> {
 public:
  using ScalarType = uint8_t;
  using MapVecTraits = VecTraits<int16_t>;
  using MapVectorType = typename MapVecTraits::VectorType;
  using MapVector2Type = typename MapVecTraits::Vector2Type;
  using FracVecTraits = VecTraits<uint16_t>;
  using FracVectorType = typename FracVecTraits::VectorType;

  RemapS16Point5Replicate(Rows<const ScalarType> src_rows, size_t src_width,
                          size_t src_height, svuint16_t& v_src_stride,
                          MapVectorType& v_x_max, MapVectorType& v_y_max)
      : src_rows_{src_rows},
        v_src_stride_{v_src_stride},
        v_xmax_{v_x_max},
        v_ymax_{v_y_max} {
    v_src_stride_ = svdup_u16(src_rows.stride());
    v_xmax_ = svdup_s16(static_cast<int16_t>(src_width - 1));
    v_ymax_ = svdup_s16(static_cast<int16_t>(src_height - 1));
  }

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<const uint16_t> mapfrac, Columns<ScalarType> dst) {
    svuint16_t src_a, src_b, src_c, src_d;

    svuint32_t bias = svdup_n_u32(REMAP16POINT5_FRAC_MAX_SQUARE / 2);
    auto vector_path = [&](svbool_t pg, ptrdiff_t step) {
      load_source(pg, step, mapxy, src_a, src_b, src_c, src_d);
      interpolate_and_store(pg, step, mapfrac, dst, src_a, src_b, src_c, src_d,
                            bias);
    };

    LoopUnroll loop{width, MapVecTraits::num_lanes()};
    loop.unroll_once([&](size_t step) {
      svbool_t pg = MapVecTraits::svptrue();
      vector_path(pg, static_cast<ptrdiff_t>(step));
    });
    loop.remaining([&](size_t length, size_t step) {
      svbool_t pg = MapVecTraits::svwhilelt(step - length, step);
      vector_path(pg, static_cast<ptrdiff_t>(length));
    });
  }

 protected:
  svuint16_t gather_load_src(svbool_t pg_b, svuint32_t offsets_b, svbool_t pg_t,
                             svuint32_t offsets_t) {
    svuint32_t src_b =
        svldnt1ub_gather_u32offset_u32(pg_b, &src_rows_[0], offsets_b);
    svuint32_t src_t =
        svldnt1ub_gather_u32offset_u32(pg_t, &src_rows_[0], offsets_t);
    return svtrn1_u16(svreinterpret_u16_u32(src_b),
                      svreinterpret_u16_u32(src_t));
  }

  void load_source(svbool_t pg, ptrdiff_t step, Columns<const int16_t>& mapxy,
                   svuint16_t& src_a, svuint16_t& src_b, svuint16_t& src_c,
                   svuint16_t& src_d) {
    MapVector2Type xy = svld2_s16(pg, &mapxy[0]);

    // Clamp coordinates to within the dimensions of the source image
    svuint16_t x0 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0), svmin_x(pg, svget2(xy, 0), v_xmax_)));
    svuint16_t y0 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0), svmin_x(pg, svget2(xy, 1), v_ymax_)));

    // x1 = x0 + 1, and clamp it too
    svuint16_t x1 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0),
                svmin_x(pg, svqadd_n_s16_x(pg, svget2(xy, 0), 1), v_xmax_)));

    svuint16_t y1 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0),
                svmin_x(pg, svqadd_n_s16_x(pg, svget2(xy, 1), 1), v_ymax_)));
    svbool_t pg_b = svwhilelt_b32(int64_t{0}, (step + 1) / 2);
    svbool_t pg_t = svwhilelt_b32(int64_t{0}, step / 2);

    // Calculate offsets from coordinates (y * stride + x)
    svuint32_t offsets_a_b = svmlalb_u32(svmovlb_u32(x0), y0, v_src_stride_);
    svuint32_t offsets_a_t = svmlalt_u32(svmovlt_u32(x0), y0, v_src_stride_);
    svuint32_t offsets_b_b = svmlalb_u32(svmovlb_u32(x1), y0, v_src_stride_);
    svuint32_t offsets_b_t = svmlalt_u32(svmovlt_u32(x1), y0, v_src_stride_);
    svuint32_t offsets_c_b = svmlalb_u32(svmovlb_u32(x0), y1, v_src_stride_);
    svuint32_t offsets_c_t = svmlalt_u32(svmovlt_u32(x0), y1, v_src_stride_);
    svuint32_t offsets_d_b = svmlalb_u32(svmovlb_u32(x1), y1, v_src_stride_);
    svuint32_t offsets_d_t = svmlalt_u32(svmovlt_u32(x1), y1, v_src_stride_);

    // Load pixels from source
    src_a = gather_load_src(pg_b, offsets_a_b, pg_t, offsets_a_t);
    src_b = gather_load_src(pg_b, offsets_b_b, pg_t, offsets_b_t);
    src_c = gather_load_src(pg_b, offsets_c_b, pg_t, offsets_c_t);
    src_d = gather_load_src(pg_b, offsets_d_b, pg_t, offsets_d_t);
    mapxy += step;
  }

  void interpolate_and_store(svbool_t pg, ptrdiff_t step,
                             Columns<const uint16_t>& mapfrac,
                             Columns<ScalarType>& dst, svuint16_t src_a,
                             svuint16_t src_b, svuint16_t src_c,
                             svuint16_t src_d, svuint32_t bias) {
    FracVectorType frac = svld1_u16(pg, &mapfrac[0]);
    svuint16_t result = interpolate_16point5<uint8_t>(pg, frac, src_a, src_b,
                                                      src_c, src_d, bias);
    svst1b_u16(pg, &dst[0], result);
    mapfrac += step;
    dst += step;
  }

  Rows<const ScalarType> src_rows_;

 private:
  svuint16_t& v_src_stride_;
  MapVectorType& v_xmax_;
  MapVectorType& v_ymax_;
};  // end of class RemapS16Point5Replicate<uint8_t>

template <>
class RemapS16Point5Replicate<uint16_t> {
 public:
  using ScalarType = uint16_t;
  using MapVecTraits = VecTraits<int16_t>;
  using MapVectorType = typename MapVecTraits::VectorType;
  using MapVector2Type = typename MapVecTraits::Vector2Type;
  using FracVecTraits = VecTraits<uint16_t>;
  using FracVectorType = typename FracVecTraits::VectorType;

  RemapS16Point5Replicate(Rows<const ScalarType> src_rows, size_t src_width,
                          size_t src_height, svuint16_t& v_src_stride,
                          MapVectorType& v_x_max, MapVectorType& v_y_max)
      : src_rows_{src_rows},
        v_src_element_stride_{v_src_stride},
        v_xmax_{v_x_max},
        v_ymax_{v_y_max} {
    v_src_element_stride_ = svdup_u16(src_rows.stride() / sizeof(ScalarType));
    v_xmax_ = svdup_s16(static_cast<int16_t>(src_width - 1));
    v_ymax_ = svdup_s16(static_cast<int16_t>(src_height - 1));
  }

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<const uint16_t> mapfrac, Columns<ScalarType> dst) {
    svuint16_t src_a, src_b, src_c, src_d;

    svuint32_t bias = svdup_n_u32(REMAP16POINT5_FRAC_MAX_SQUARE / 2);
    auto vector_path = [&](svbool_t pg, ptrdiff_t step) {
      load_source(pg, step, mapxy, src_a, src_b, src_c, src_d);
      interpolate_and_store(pg, step, mapfrac, dst, src_a, src_b, src_c, src_d,
                            bias);
    };

    LoopUnroll loop{width, MapVecTraits::num_lanes()};
    loop.unroll_once([&](size_t step) {
      svbool_t pg = MapVecTraits::svptrue();
      vector_path(pg, static_cast<ptrdiff_t>(step));
    });
    loop.remaining([&](size_t length, size_t step) {
      svbool_t pg = MapVecTraits::svwhilelt(step - length, step);
      vector_path(pg, static_cast<ptrdiff_t>(length));
    });
  }

 protected:
  svuint16_t gather_load_src(svbool_t pg_b, svuint32_t offsets_b, svbool_t pg_t,
                             svuint32_t offsets_t) {
    // Account for the size of the source type when calculating offset
    offsets_b = svlsl_n_u32_x(pg_b, offsets_b, 1);
    offsets_t = svlsl_n_u32_x(pg_t, offsets_t, 1);

    svuint32_t src_b =
        svldnt1uh_gather_u32offset_u32(pg_b, &src_rows_[0], offsets_b);
    svuint32_t src_t =
        svldnt1uh_gather_u32offset_u32(pg_t, &src_rows_[0], offsets_t);
    return svtrn1_u16(svreinterpret_u16_u32(src_b),
                      svreinterpret_u16_u32(src_t));
  }

  void load_source(svbool_t pg, ptrdiff_t step, Columns<const int16_t>& mapxy,
                   svuint16_t& src_a, svuint16_t& src_b, svuint16_t& src_c,
                   svuint16_t& src_d) {
    MapVector2Type xy = svld2_s16(pg, &mapxy[0]);

    // Clamp coordinates to within the dimensions of the source image
    svuint16_t x0 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0), svmin_x(pg, svget2(xy, 0), v_xmax_)));
    svuint16_t y0 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0), svmin_x(pg, svget2(xy, 1), v_ymax_)));

    // x1 = x0 + 1, and clamp it too
    svuint16_t x1 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0),
                svmin_x(pg, svqadd_n_s16_x(pg, svget2(xy, 0), 1), v_xmax_)));

    svuint16_t y1 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0),
                svmin_x(pg, svqadd_n_s16_x(pg, svget2(xy, 1), 1), v_ymax_)));
    svbool_t pg_b = svwhilelt_b32(int64_t{0}, (step + 1) / 2);
    svbool_t pg_t = svwhilelt_b32(int64_t{0}, step / 2);

    // Calculate offsets from coordinates (y * stride/sizeof(ScalarType) + x)
    svuint32_t offsets_a_b =
        svmlalb_u32(svmovlb_u32(x0), y0, v_src_element_stride_);
    svuint32_t offsets_a_t =
        svmlalt_u32(svmovlt_u32(x0), y0, v_src_element_stride_);
    svuint32_t offsets_b_b =
        svmlalb_u32(svmovlb_u32(x1), y0, v_src_element_stride_);
    svuint32_t offsets_b_t =
        svmlalt_u32(svmovlt_u32(x1), y0, v_src_element_stride_);
    svuint32_t offsets_c_b =
        svmlalb_u32(svmovlb_u32(x0), y1, v_src_element_stride_);
    svuint32_t offsets_c_t =
        svmlalt_u32(svmovlt_u32(x0), y1, v_src_element_stride_);
    svuint32_t offsets_d_b =
        svmlalb_u32(svmovlb_u32(x1), y1, v_src_element_stride_);
    svuint32_t offsets_d_t =
        svmlalt_u32(svmovlt_u32(x1), y1, v_src_element_stride_);

    // Load pixels from source
    src_a = gather_load_src(pg_b, offsets_a_b, pg_t, offsets_a_t);
    src_b = gather_load_src(pg_b, offsets_b_b, pg_t, offsets_b_t);
    src_c = gather_load_src(pg_b, offsets_c_b, pg_t, offsets_c_t);
    src_d = gather_load_src(pg_b, offsets_d_b, pg_t, offsets_d_t);
    mapxy += step;
  }

  void interpolate_and_store(svbool_t pg, ptrdiff_t step,
                             Columns<const uint16_t>& mapfrac,
                             Columns<ScalarType>& dst, svuint16_t src_a,
                             svuint16_t src_b, svuint16_t src_c,
                             svuint16_t src_d, svuint32_t bias) {
    FracVectorType frac = svld1_u16(pg, &mapfrac[0]);
    svuint16_t result = interpolate_16point5<uint16_t>(pg, frac, src_a, src_b,
                                                       src_c, src_d, bias);
    svst1_u16(pg, &dst[0], result);
    mapfrac += step;
    dst += step;
  }

  Rows<const ScalarType> src_rows_;

 private:
  svuint16_t& v_src_element_stride_;
  MapVectorType& v_xmax_;
  MapVectorType& v_ymax_;
};  // end of class RemapS16Point5Replicate<uint16_t>

template <typename ScalarType>
class RemapS16Point5ReplicateFourChannels;

template <>
class RemapS16Point5ReplicateFourChannels<uint8_t> {
 public:
  using ScalarType = uint8_t;
  using MapVecTraits = VecTraits<int16_t>;
  using MapVectorType = typename MapVecTraits::VectorType;
  using MapVector2Type = typename MapVecTraits::Vector2Type;
  using FracVecTraits = VecTraits<uint16_t>;
  using FracVectorType = typename FracVecTraits::VectorType;

  RemapS16Point5ReplicateFourChannels(Rows<const ScalarType> src_rows,
                                      size_t src_width, size_t src_height,
                                      svuint16_t& v_src_stride,
                                      MapVectorType& v_x_max,
                                      MapVectorType& v_y_max)
      : src_rows_{src_rows},
        v_src_stride_{v_src_stride},
        v_xmax_{v_x_max},
        v_ymax_{v_y_max} {
    v_src_stride_ = svdup_u16(src_rows.stride());
    v_xmax_ = svdup_s16(static_cast<int16_t>(src_width - 1));
    v_ymax_ = svdup_s16(static_cast<int16_t>(src_height - 1));
  }

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<const uint16_t> mapfrac, Columns<ScalarType> dst) {
    LoopUnroll loop{width, MapVecTraits::num_lanes()};
    loop.unroll_once([&](size_t step) {
      svbool_t pg = MapVecTraits::svptrue();
      vector_path(pg, mapxy, mapfrac, dst, static_cast<ptrdiff_t>(step));
    });
    loop.remaining([&](size_t length, size_t step) {
      svbool_t pg = MapVecTraits::svwhilelt(step - length, step);
      vector_path(pg, mapxy, mapfrac, dst, static_cast<ptrdiff_t>(length));
    });
  }

  void vector_path(svbool_t pg, Columns<const int16_t>& mapxy,
                   Columns<const uint16_t>& mapfrac, Columns<ScalarType>& dst,
                   ptrdiff_t step) {
    MapVector2Type xy = svld2_s16(pg, &mapxy[0]);
    svuint32_t bias = svdup_n_u32(REMAP16POINT5_FRAC_MAX_SQUARE / 2);

    // Clamp coordinates to within the dimensions of the source image
    svuint16_t x0 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0), svmin_x(pg, svget2(xy, 0), v_xmax_)));
    svuint16_t y0 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0), svmin_x(pg, svget2(xy, 1), v_ymax_)));

    // x1 = x0 + 1, and clamp it too
    svuint16_t x1 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0),
                svmin_x(pg, svqadd_n_s16_x(pg, svget2(xy, 0), 1), v_xmax_)));

    svuint16_t y1 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0),
                svmin_x(pg, svqadd_n_s16_x(pg, svget2(xy, 1), 1), v_ymax_)));
    svbool_t pg_b = svwhilelt_b32(int64_t{0}, (step + 1) / 2);
    svbool_t pg_t = svwhilelt_b32(int64_t{0}, step / 2);

    //// NEW PART
    // Calculate offsets from coordinates (y * stride + x), x multiplied by 4
    // channels
    auto load_4ch_b = [&](svuint16_t x, svuint16_t y) {
      return svreinterpret_u8_u32(svld1_gather_u32offset_u32(
          pg_b, reinterpret_cast<const uint32_t*>(&src_rows_[0]),
          svmlalb_u32(svshllb_n_u32(x, 2), y, v_src_stride_)));
    };
    auto load_4ch_t = [&](svuint16_t x, svuint16_t y) {
      return svreinterpret_u8_u32(svld1_gather_u32offset_u32(
          pg_t, reinterpret_cast<const uint32_t*>(&src_rows_[0]),
          svmlalt_u32(svshllt_n_u32(x, 2), y, v_src_stride_)));
    };

    FracVectorType frac = svld1_u16(pg, &mapfrac[0]);
    svuint16_t xfrac =
        svand_x(pg, frac, svdup_n_u16(REMAP16POINT5_FRAC_MAX - 1));
    svuint16_t yfrac =
        svand_x(pg, svlsr_n_u16_x(pg, frac, REMAP16POINT5_FRAC_BITS),
                svdup_n_u16(REMAP16POINT5_FRAC_MAX - 1));

    auto lerp2d = [&](svuint16_t xfrac, svuint16_t yfrac, svuint16_t nxfrac,
                      svuint16_t nyfrac, svuint16_t src_a, svuint16_t src_b,
                      svuint16_t src_c, svuint16_t src_d, svuint32_t bias) {
      svuint16_t line0 = svmla_x(
          svptrue_b16(), svmul_x(svptrue_b16(), xfrac, src_b), nxfrac, src_a);
      svuint16_t line1 = svmla_x(
          svptrue_b16(), svmul_x(svptrue_b16(), xfrac, src_d), nxfrac, src_c);

      svuint32_t acc_b = svmlalb_u32(bias, line0, nyfrac);
      svuint32_t acc_t = svmlalt_u32(bias, line0, nyfrac);
      acc_b = svmlalb_u32(acc_b, line1, yfrac);
      acc_t = svmlalt_u32(acc_t, line1, yfrac);

      return svshrnt(svshrnb(acc_b, 2ULL * REMAP16POINT5_FRAC_BITS), acc_t,
                     2ULL * REMAP16POINT5_FRAC_BITS);
    };

    // bottom part
    svuint8_t a = load_4ch_b(x0, y0);
    svuint8_t b = load_4ch_b(x1, y0);
    svuint8_t c = load_4ch_b(x0, y1);
    svuint8_t d = load_4ch_b(x1, y1);
    // from xfrac, we need the bottom part twice
    svuint16_t xfrac2b = svtrn1_u16(xfrac, xfrac);
    svuint16_t nxfrac2b = svsub_u16_x(
        svptrue_b16(), svdup_n_u16(REMAP16POINT5_FRAC_MAX), xfrac2b);
    svuint16_t yfrac2b = svtrn1_u16(yfrac, yfrac);
    svuint16_t nyfrac2b = svsub_u16_x(
        svptrue_b16(), svdup_n_u16(REMAP16POINT5_FRAC_MAX), yfrac2b);

    // a,b,c,d looks like 12341234...(four channels)
    // bottom is 1313...
    svuint16_t res_bb =
        lerp2d(xfrac2b, yfrac2b, nxfrac2b, nyfrac2b, svmovlb_u16(a),
               svmovlb_u16(b), svmovlb_u16(c), svmovlb_u16(d), bias);
    // top is 2424...
    svuint16_t res_bt =
        lerp2d(xfrac2b, yfrac2b, nxfrac2b, nyfrac2b, svmovlt_u16(a),
               svmovlt_u16(b), svmovlt_u16(c), svmovlt_u16(d), bias);
    svuint8_t res_b =
        svtrn1_u8(svreinterpret_u8_u16(res_bb), svreinterpret_u8_u16(res_bt));

    // top part
    a = load_4ch_t(x0, y0);
    b = load_4ch_t(x1, y0);
    c = load_4ch_t(x0, y1);
    d = load_4ch_t(x1, y1);
    // from xfrac, we need the top part twice
    svuint16_t xfrac2t = svtrn2_u16(xfrac, xfrac);
    svuint16_t nxfrac2t = svsub_u16_x(
        svptrue_b16(), svdup_n_u16(REMAP16POINT5_FRAC_MAX), xfrac2t);
    svuint16_t yfrac2t = svtrn2_u16(yfrac, yfrac);
    svuint16_t nyfrac2t = svsub_u16_x(
        svptrue_b16(), svdup_n_u16(REMAP16POINT5_FRAC_MAX), yfrac2t);

    // a,b,c,d looks like 12341234...(four channels)
    // bottom is 1313...
    svuint16_t res_tb =
        lerp2d(xfrac2t, yfrac2t, nxfrac2t, nyfrac2t, svmovlb_u16(a),
               svmovlb_u16(b), svmovlb_u16(c), svmovlb_u16(d), bias);
    // top is 2424...
    svuint16_t res_tt =
        lerp2d(xfrac2t, yfrac2t, nxfrac2t, nyfrac2t, svmovlt_u16(a),
               svmovlt_u16(b), svmovlt_u16(c), svmovlt_u16(d), bias);
    svuint8_t res_t =
        svtrn1_u8(svreinterpret_u8_u16(res_tb), svreinterpret_u8_u16(res_tt));

    svbool_t pg_low = svwhilelt_b32(0L, step);
    svbool_t pg_high = svwhilelt_b32(svcntw(), static_cast<size_t>(step));
    svuint32_t res_low =
        svzip1_u32(svreinterpret_u32_u8(res_b), svreinterpret_u32_u8(res_t));
    svuint32_t res_high =
        svzip2_u32(svreinterpret_u32_u8(res_b), svreinterpret_u32_u8(res_t));
    mapxy += step;
    svst1_u32(pg_low, reinterpret_cast<uint32_t*>(&dst[0]), res_low);
    svst1_u32(pg_high, reinterpret_cast<uint32_t*>(&dst[0]) + svcntw(),
              res_high);
    mapfrac += step;
    dst += step;
  }

  Rows<const ScalarType> src_rows_;

 private:
  svuint16_t& v_src_stride_;
  MapVectorType& v_xmax_;
  MapVectorType& v_ymax_;
};  // end of class RemapS16Point5ReplicateFourChannels<uint8_t>

template <>
class RemapS16Point5ReplicateFourChannels<uint16_t> {
 public:
  using ScalarType = uint16_t;
  using MapVecTraits = VecTraits<int16_t>;
  using MapVectorType = typename MapVecTraits::VectorType;
  using MapVector2Type = typename MapVecTraits::Vector2Type;
  using FracVecTraits = VecTraits<uint16_t>;
  using FracVectorType = typename FracVecTraits::VectorType;

  RemapS16Point5ReplicateFourChannels(Rows<const ScalarType> src_rows,
                                      size_t src_width, size_t src_height,
                                      svuint16_t& v_src_stride,
                                      MapVectorType& v_x_max,
                                      MapVectorType& v_y_max)
      : src_rows_{src_rows},
        v_src_element_stride_{v_src_stride},
        v_xmax_{v_x_max},
        v_ymax_{v_y_max} {
    v_src_element_stride_ = svdup_u16(src_rows.stride() / sizeof(ScalarType));
    v_xmax_ = svdup_s16(static_cast<int16_t>(src_width - 1));
    v_ymax_ = svdup_s16(static_cast<int16_t>(src_height - 1));
  }

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<const uint16_t> mapfrac, Columns<ScalarType> dst) {
    LoopUnroll loop{width, MapVecTraits::num_lanes()};
    loop.unroll_once([&](size_t step) {
      svbool_t pg = MapVecTraits::svptrue();
      vector_path(pg, mapxy, mapfrac, dst, static_cast<ptrdiff_t>(step));
    });
    loop.remaining([&](size_t length, size_t step) {
      svbool_t pg = MapVecTraits::svwhilelt(step - length, step);
      vector_path(pg, mapxy, mapfrac, dst, static_cast<ptrdiff_t>(length));
    });
  }

  void vector_path(svbool_t pg, Columns<const int16_t>& mapxy,
                   Columns<const uint16_t>& mapfrac, Columns<ScalarType>& dst,
                   ptrdiff_t step) {
    MapVector2Type xy = svld2_s16(pg, &mapxy[0]);
    svuint32_t bias = svdup_n_u32(REMAP16POINT5_FRAC_MAX_SQUARE / 2);

    // Clamp coordinates to within the dimensions of the source image
    svuint16_t x0 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0), svmin_x(pg, svget2(xy, 0), v_xmax_)));
    svuint16_t y0 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0), svmin_x(pg, svget2(xy, 1), v_ymax_)));

    // x1 = x0 + 1, and clamp it too
    svuint16_t x1 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0),
                svmin_x(pg, svqadd_n_s16_x(pg, svget2(xy, 0), 1), v_xmax_)));

    svuint16_t y1 = svreinterpret_u16_s16(
        svmax_x(pg, svdup_n_s16(0),
                svmin_x(pg, svqadd_n_s16_x(pg, svget2(xy, 1), 1), v_ymax_)));

    auto load_4ch_b = [&](svbool_t pg, svuint32_t offsets) {
      return svreinterpret_u16_u64(svld1_gather_u64offset_u64(
          pg, reinterpret_cast<const uint64_t*>(&src_rows_[0]),
          svshllb_n_u64(offsets, 1)));
    };
    auto load_4ch_t = [&](svbool_t pg, svuint32_t offsets) {
      return svreinterpret_u16_u64(svld1_gather_u64offset_u64(
          pg, reinterpret_cast<const uint64_t*>(&src_rows_[0]),
          svshllt_n_u64(offsets, 1)));
    };

    FracVectorType frac = svld1_u16(pg, &mapfrac[0]);
    svuint16_t xfrac =
        svand_x(pg, frac, svdup_n_u16(REMAP16POINT5_FRAC_MAX - 1));
    svuint16_t yfrac =
        svand_x(pg, svlsr_n_u16_x(pg, frac, REMAP16POINT5_FRAC_BITS),
                svdup_n_u16(REMAP16POINT5_FRAC_MAX - 1));
    svuint16_t nxfrac =
        svsub_u16_x(pg, svdup_n_u16(REMAP16POINT5_FRAC_MAX), xfrac);
    svuint16_t nyfrac =
        svsub_u16_x(pg, svdup_n_u16(REMAP16POINT5_FRAC_MAX), yfrac);

    auto lerp2d = [&](svuint16_t xfrac, svuint16_t yfrac, svuint16_t nxfrac,
                      svuint16_t nyfrac, svuint16_t src_a, svuint16_t src_b,
                      svuint16_t src_c, svuint16_t src_d, svuint32_t bias) {
      svuint32_t line0_b = svmlalb(svmullb(xfrac, src_b), nxfrac, src_a);
      svuint32_t line0_t = svmlalt(svmullt(xfrac, src_b), nxfrac, src_a);
      svuint32_t line1_b = svmlalb(svmullb(xfrac, src_d), nxfrac, src_c);
      svuint32_t line1_t = svmlalt(svmullt(xfrac, src_d), nxfrac, src_c);

      svuint32_t acc_b =
          svmla_u32_x(svptrue_b32(), bias, line0_b, svmovlb_u32(nyfrac));
      svuint32_t acc_t =
          svmla_u32_x(svptrue_b32(), bias, line0_t, svmovlt_u32(nyfrac));
      acc_b = svmla_u32_x(svptrue_b32(), acc_b, line1_b, svmovlb_u32(yfrac));
      acc_t = svmla_u32_x(svptrue_b32(), acc_t, line1_t, svmovlt_u32(yfrac));

      return svshrnt(svshrnb(acc_b, 2ULL * REMAP16POINT5_FRAC_BITS), acc_t,
                     2ULL * REMAP16POINT5_FRAC_BITS);
    };

    // There are 4 channels: data is 4 times wider than the maps (4x16 bits vs
    // 16-bit coordinates) So calculation is done in 4 parts:
    // - 0,4,8,... (bottom-bottom)
    // - 2,6,10,... (bottom-top)
    // - 1,5,9,... (top-bottom)
    // - 3,7,11,... (top-top)
    svuint16_t res_bb, res_bt, res_tb, res_tt;

    svuint16_t fractbl_bb =
        svreinterpret_u16_u64(svindex_u64(0, 0x0004000400040004UL));
    svuint16_t fractbl_bt = svreinterpret_u16_u64(
        svadd_n_u64_x(svptrue_b64(), svreinterpret_u64_u16(fractbl_bb),
                      0x0002000200020002UL));
    svuint16_t fractbl_tb = svreinterpret_u16_u64(
        svadd_n_u64_x(svptrue_b64(), svreinterpret_u64_u16(fractbl_bb),
                      0x0001000100010001UL));
    svuint16_t fractbl_tt = svreinterpret_u16_u64(
        svadd_n_u64_x(svptrue_b64(), svreinterpret_u64_u16(fractbl_bb),
                      0x0003000300030003UL));

    {  // bottom
      svbool_t pg_bb = svwhilelt_b64(int64_t{0}, (step + 3) / 4);
      svbool_t pg_bt = svwhilelt_b64(int64_t{0}, (step + 2) / 4);

      svuint32_t offsets_a_b =
          svmlalb_u32(svshllb_n_u32(x0, 2), y0, v_src_element_stride_);
      svuint32_t offsets_b_b =
          svmlalb_u32(svshllb_n_u32(x1, 2), y0, v_src_element_stride_);
      svuint32_t offsets_c_b =
          svmlalb_u32(svshllb_n_u32(x0, 2), y1, v_src_element_stride_);
      svuint32_t offsets_d_b =
          svmlalb_u32(svshllb_n_u32(x1, 2), y1, v_src_element_stride_);

      {  // bottom-bottom
        svuint16_t a = load_4ch_b(pg_bb, offsets_a_b);
        svuint16_t b = load_4ch_b(pg_bb, offsets_b_b);
        svuint16_t c = load_4ch_b(pg_bb, offsets_c_b);
        svuint16_t d = load_4ch_b(pg_bb, offsets_d_b);

        svuint16_t xfr = svtbl_u16(xfrac, fractbl_bb);
        svuint16_t nxfr = svtbl_u16(nxfrac, fractbl_bb);
        svuint16_t yfr = svtbl_u16(yfrac, fractbl_bb);
        svuint16_t nyfr = svtbl_u16(nyfrac, fractbl_bb);

        res_bb = lerp2d(xfr, yfr, nxfr, nyfr, a, b, c, d, bias);
      }

      {  // bottom-top
        svuint16_t a = load_4ch_t(pg_bt, offsets_a_b);
        svuint16_t b = load_4ch_t(pg_bt, offsets_b_b);
        svuint16_t c = load_4ch_t(pg_bt, offsets_c_b);
        svuint16_t d = load_4ch_t(pg_bt, offsets_d_b);

        svuint16_t xfr = svtbl_u16(xfrac, fractbl_bt);
        svuint16_t nxfr = svtbl_u16(nxfrac, fractbl_bt);
        svuint16_t yfr = svtbl_u16(yfrac, fractbl_bt);
        svuint16_t nyfr = svtbl_u16(nyfrac, fractbl_bt);

        res_bt = lerp2d(xfr, yfr, nxfr, nyfr, a, b, c, d, bias);
      }
    }

    {  // top
      svbool_t pg_tb = svwhilelt_b64(int64_t{0}, (step + 1) / 4);
      svbool_t pg_tt = svwhilelt_b64(int64_t{0}, step / 4);

      svuint32_t offsets_a_t =
          svmlalt_u32(svshllt_n_u32(x0, 2), y0, v_src_element_stride_);
      svuint32_t offsets_b_t =
          svmlalt_u32(svshllt_n_u32(x1, 2), y0, v_src_element_stride_);
      svuint32_t offsets_c_t =
          svmlalt_u32(svshllt_n_u32(x0, 2), y1, v_src_element_stride_);
      svuint32_t offsets_d_t =
          svmlalt_u32(svshllt_n_u32(x1, 2), y1, v_src_element_stride_);

      {  // top-bottom
        svuint16_t a = load_4ch_b(pg_tb, offsets_a_t);
        svuint16_t b = load_4ch_b(pg_tb, offsets_b_t);
        svuint16_t c = load_4ch_b(pg_tb, offsets_c_t);
        svuint16_t d = load_4ch_b(pg_tb, offsets_d_t);

        svuint16_t xfr = svtbl_u16(xfrac, fractbl_tb);
        svuint16_t nxfr = svtbl_u16(nxfrac, fractbl_tb);
        svuint16_t yfr = svtbl_u16(yfrac, fractbl_tb);
        svuint16_t nyfr = svtbl_u16(nyfrac, fractbl_tb);

        res_tb = lerp2d(xfr, yfr, nxfr, nyfr, a, b, c, d, bias);
      }

      {  // top-top
        svuint16_t a = load_4ch_t(pg_tt, offsets_a_t);
        svuint16_t b = load_4ch_t(pg_tt, offsets_b_t);
        svuint16_t c = load_4ch_t(pg_tt, offsets_c_t);
        svuint16_t d = load_4ch_t(pg_tt, offsets_d_t);

        svuint16_t xfr = svtbl_u16(xfrac, fractbl_tt);
        svuint16_t nxfr = svtbl_u16(nxfrac, fractbl_tt);
        svuint16_t yfr = svtbl_u16(yfrac, fractbl_tt);
        svuint16_t nyfr = svtbl_u16(nyfrac, fractbl_tt);

        res_tt = lerp2d(xfr, yfr, nxfr, nyfr, a, b, c, d, bias);
      }
    }

    svbool_t pg_00 = svwhilelt_b64(0L, step);
    svbool_t pg_01 = svwhilelt_b64(svcntd(), static_cast<size_t>(step));
    svbool_t pg_02 = svwhilelt_b64(2 * svcntd(), static_cast<size_t>(step));
    svbool_t pg_03 = svwhilelt_b64(3 * svcntd(), static_cast<size_t>(step));

    // Back-transforming is needed, svst4 cannot be used because the
    // interleaving would need equal number of elements in the 4 vectors the
    // results are now these:
    // - 0,4,8,... (bottom-bottom)
    // - 2,6,10,... (bottom-top)
    // - 1,5,9,... (top-bottom)
    // - 3,7,11,... (top-top)
    // first pass will result in:
    // - 0,2,4,... (lower and higher ones)
    // - 1,3,5,... (lower and higher ones)
    // second pass gives back 0,1,2,3,....

    svuint64_t res_even_low = svzip1_u64(svreinterpret_u64_u16(res_bb),
                                         svreinterpret_u64_u16(res_bt));
    svuint64_t res_even_high = svzip2_u64(svreinterpret_u64_u16(res_bb),
                                          svreinterpret_u64_u16(res_bt));
    svuint64_t res_odd_low = svzip1_u64(svreinterpret_u64_u16(res_tb),
                                        svreinterpret_u64_u16(res_tt));
    svuint64_t res_odd_high = svzip2_u64(svreinterpret_u64_u16(res_tb),
                                         svreinterpret_u64_u16(res_tt));

    svuint64_t res_00 = svzip1_u64(res_even_low, res_odd_low);
    svuint64_t res_01 = svzip2_u64(res_even_low, res_odd_low);
    svuint64_t res_02 = svzip1_u64(res_even_high, res_odd_high);
    svuint64_t res_03 = svzip2_u64(res_even_high, res_odd_high);

    svst1_u64(pg_00, reinterpret_cast<uint64_t*>(&dst[0]), res_00);
    svst1_u64(pg_01, reinterpret_cast<uint64_t*>(&dst[0]) + svcntd(), res_01);
    svst1_u64(pg_02, reinterpret_cast<uint64_t*>(&dst[0]) + 2 * svcntd(),
              res_02);
    svst1_u64(pg_03, reinterpret_cast<uint64_t*>(&dst[0]) + 3 * svcntd(),
              res_03);

    mapxy += step;
    mapfrac += step;
    dst += step;
  }

  Rows<const ScalarType> src_rows_;

 private:
  svuint16_t& v_src_element_stride_;
  MapVectorType& v_xmax_;
  MapVectorType& v_ymax_;
};  // end of class RemapS16Point5ReplicateFourChannels<uint16_t>

template <typename ScalarType>
class RemapS16Point5ConstantBorder;

template <>
class RemapS16Point5ConstantBorder<uint8_t> {
 public:
  using ScalarType = uint8_t;

  RemapS16Point5ConstantBorder(Rows<const ScalarType> src_rows,
                               size_t src_width, size_t src_height,
                               const ScalarType* border_value,
                               svuint16_t& v_src_stride, svuint16_t& v_width,
                               svuint16_t& v_height, svuint16_t& v_border)
      : src_rows_{src_rows},
        v_src_stride_{v_src_stride},
        v_width_{v_width},
        v_height_{v_height},
        v_border_{v_border} {
    v_src_stride_ = svdup_u16(src_rows.stride());
    v_width_ = svdup_u16(static_cast<uint16_t>(src_width));
    v_height_ = svdup_u16(static_cast<uint16_t>(src_height));
    v_border_ = svdup_u16(*border_value);
  }

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<const uint16_t> mapfrac, Columns<ScalarType> dst) {
    svuint16_t one = svdup_n_u16(1);
    svuint32_t bias = svdup_n_u32(REMAP16POINT5_FRAC_MAX_SQUARE / 2);
    for (size_t i = 0; i < width; i += svcnth()) {
      svbool_t pg = svwhilelt_b16(i, width);

      svuint16x2_t xy =
          svld2_u16(pg, reinterpret_cast<const uint16_t*>(
                            &mapxy[static_cast<ptrdiff_t>(i * 2)]));

      svuint16_t x0 = svget2(xy, 0);
      svuint16_t y0 = svget2(xy, 1);
      svuint16_t x1 = svadd_x(pg, x0, one);
      svuint16_t y1 = svadd_x(pg, y0, one);

      svuint16_t v00 = load_pixels_or_constant_border(
          src_rows_, v_src_stride_, v_width_, v_height_, v_border_, pg, x0, y0);
      svuint16_t v01 = load_pixels_or_constant_border(
          src_rows_, v_src_stride_, v_width_, v_height_, v_border_, pg, x0, y1);
      svuint16_t v10 = load_pixels_or_constant_border(
          src_rows_, v_src_stride_, v_width_, v_height_, v_border_, pg, x1, y0);
      svuint16_t v11 = load_pixels_or_constant_border(
          src_rows_, v_src_stride_, v_width_, v_height_, v_border_, pg, x1, y1);

      svuint16_t frac = svld1_u16(pg, &mapfrac[static_cast<ptrdiff_t>(i)]);
      svuint16_t result =
          interpolate_16point5<uint8_t>(pg, frac, v00, v10, v01, v11, bias);

      svst1b_u16(pg, &dst[static_cast<ptrdiff_t>(i)], result);
    }
  }

 private:
  svuint16_t load_pixels_or_constant_border(Rows<const ScalarType> src_rows_,
                                            svuint16_t& v_src_stride_,
                                            svuint16_t& v_width_,
                                            svuint16_t& v_height_,
                                            svuint16_t& v_border_, svbool_t pg,
                                            svuint16_t x, svuint16_t y) {
    // Find whether coordinates are within the image dimensions.
    svbool_t in_range = svand_b_z(pg, svcmplt_u16(pg, x, v_width_),
                                  svcmplt_u16(pg, y, v_height_));

    // Calculate offsets from coordinates (y * stride + x)
    svuint32_t offsets_b = svmlalb_u32(svmovlb_u32(x), y, v_src_stride_);
    svuint32_t offsets_t = svmlalt_u32(svmovlt_u32(x), y, v_src_stride_);

    svbool_t pg_b = in_range;
    svbool_t pg_t = svtrn2_b16(in_range, svpfalse());

    // Copy pixels from source
    svuint32_t result_b =
        svld1ub_gather_u32offset_u32(pg_b, &src_rows_[0], offsets_b);
    svuint32_t result_t =
        svld1ub_gather_u32offset_u32(pg_t, &src_rows_[0], offsets_t);

    svuint16_t result = svtrn1_u16(svreinterpret_u16_u32(result_b),
                                   svreinterpret_u16_u32(result_t));

    return svsel(in_range, result, v_border_);
  }

  Rows<const ScalarType> src_rows_;
  svuint16_t& v_src_stride_;
  svuint16_t& v_width_;
  svuint16_t& v_height_;
  svuint16_t& v_border_;
};  // end of class RemapS16Point5ConstantBorder<uint8_t>

template <>
class RemapS16Point5ConstantBorder<uint16_t> {
 public:
  using ScalarType = uint16_t;

  RemapS16Point5ConstantBorder(Rows<const ScalarType> src_rows,
                               size_t src_width, size_t src_height,
                               const ScalarType* border_value,
                               svuint16_t& v_src_stride, svuint16_t& v_width,
                               svuint16_t& v_height, svuint16_t& v_border)
      : src_rows_{src_rows},
        v_src_element_stride_{v_src_stride},
        v_width_{v_width},
        v_height_{v_height},
        v_border_{v_border} {
    v_src_element_stride_ = svdup_u16(src_rows.stride() / sizeof(ScalarType));
    v_width_ = svdup_u16(static_cast<uint16_t>(src_width));
    v_height_ = svdup_u16(static_cast<uint16_t>(src_height));
    v_border_ = svdup_u16(*border_value);
  }

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<const uint16_t> mapfrac, Columns<ScalarType> dst) {
    svuint16_t one = svdup_n_u16(1);
    svuint32_t bias = svdup_n_u32(REMAP16POINT5_FRAC_MAX_SQUARE / 2);
    for (size_t i = 0; i < width; i += svcnth()) {
      svbool_t pg = svwhilelt_b16(i, width);

      svuint16x2_t xy =
          svld2_u16(pg, reinterpret_cast<const uint16_t*>(
                            &mapxy[static_cast<ptrdiff_t>(i * 2)]));

      svuint16_t x0 = svget2(xy, 0);
      svuint16_t y0 = svget2(xy, 1);
      svuint16_t x1 = svadd_x(pg, x0, one);
      svuint16_t y1 = svadd_x(pg, y0, one);

      svuint16_t v00 = load_pixels_or_constant_border(
          src_rows_, v_src_element_stride_, v_width_, v_height_, v_border_, pg,
          x0, y0);
      svuint16_t v01 = load_pixels_or_constant_border(
          src_rows_, v_src_element_stride_, v_width_, v_height_, v_border_, pg,
          x0, y1);
      svuint16_t v10 = load_pixels_or_constant_border(
          src_rows_, v_src_element_stride_, v_width_, v_height_, v_border_, pg,
          x1, y0);
      svuint16_t v11 = load_pixels_or_constant_border(
          src_rows_, v_src_element_stride_, v_width_, v_height_, v_border_, pg,
          x1, y1);

      svuint16_t frac = svld1_u16(pg, &mapfrac[static_cast<ptrdiff_t>(i)]);
      svuint16_t result =
          interpolate_16point5<uint16_t>(pg, frac, v00, v10, v01, v11, bias);

      svst1_u16(pg, &dst[static_cast<ptrdiff_t>(i)], result);
    }
  }

 private:
  svuint16_t load_pixels_or_constant_border(Rows<const ScalarType> src_rows_,
                                            svuint16_t& v_src_element_stride_,
                                            svuint16_t& v_width_,
                                            svuint16_t& v_height_,
                                            svuint16_t& v_border_, svbool_t pg,
                                            svuint16_t x, svuint16_t y) {
    // Find whether coordinates are within the image dimensions.
    svbool_t in_range = svand_b_z(pg, svcmplt_u16(pg, x, v_width_),
                                  svcmplt_u16(pg, y, v_height_));

    // Calculate offsets from coordinates (y * stride/sizeof(ScalarType) + x)
    svuint32_t offsets_b =
        svmlalb_u32(svmovlb_u32(x), y, v_src_element_stride_);
    svuint32_t offsets_t =
        svmlalt_u32(svmovlt_u32(x), y, v_src_element_stride_);

    svbool_t pg_b = in_range;
    svbool_t pg_t = svtrn2_b16(in_range, svpfalse());

    // Account for the size of the source type when calculating offset
    offsets_b = svlsl_n_u32_x(pg_b, offsets_b, 1);
    offsets_t = svlsl_n_u32_x(pg_t, offsets_t, 1);

    // Copy pixels from source
    svuint32_t result_b =
        svld1uh_gather_u32offset_u32(pg_b, &src_rows_[0], offsets_b);
    svuint32_t result_t =
        svld1uh_gather_u32offset_u32(pg_t, &src_rows_[0], offsets_t);

    svuint16_t result = svtrn1_u16(svreinterpret_u16_u32(result_b),
                                   svreinterpret_u16_u32(result_t));

    return svsel(in_range, result, v_border_);
  }

  Rows<const ScalarType> src_rows_;
  svuint16_t& v_src_element_stride_;
  svuint16_t& v_width_;
  svuint16_t& v_height_;
  svuint16_t& v_border_;
};  // end of class RemapS16Point5ConstantBorder<uint16_t>

// Most of the complexity comes from parameter checking.
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <typename T>
kleidicv_error_t remap_s16point5_sc(
    const T* src, size_t src_stride, size_t src_width, size_t src_height,
    T* dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t* mapxy, size_t mapxy_stride,
    const uint16_t* mapfrac, size_t mapfrac_stride,
    kleidicv_border_type_t border_type, const T* border_value) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_POINTER_AND_STRIDE(mapxy, mapxy_stride, dst_height);
  CHECK_POINTER_AND_STRIDE(mapfrac, mapfrac_stride, dst_height);
  CHECK_IMAGE_SIZE(src_width, src_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);
  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT && nullptr == border_value) {
    return KLEIDICV_ERROR_NULL_POINTER;
  }

  if (!remap_s16point5_is_implemented<T>(src_stride, src_width, src_height,
                                         dst_width, border_type, channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  Rows<const T> src_rows{src, src_stride, channels};
  Rows<const int16_t> mapxy_rows{mapxy, mapxy_stride, 2};
  Rows<const uint16_t> mapfrac_rows{mapfrac, mapfrac_stride, 1};
  Rows<T> dst_rows{dst, dst_stride, channels};
  svuint16_t sv_src_stride;
  Rectangle rect{dst_width, dst_height};

  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT) {
    svuint16_t sv_width, sv_height, sv_border;
    if (channels == 1) {
      RemapS16Point5ConstantBorder<T> operation{
          src_rows,      src_width, src_height, border_value,
          sv_src_stride, sv_width,  sv_height,  sv_border};
      zip_rows(operation, rect, mapxy_rows, mapfrac_rows, dst_rows);
    } else {
      assert(channels == 4);
      // TODO
      return KLEIDICV_ERROR_NOT_IMPLEMENTED;
    }
  } else {
    assert(border_type == KLEIDICV_BORDER_TYPE_REPLICATE);
    svint16_t sv_xmax, sv_ymax;
    if (channels == 1) {
      RemapS16Point5Replicate<T> operation{src_rows,      src_width, src_height,
                                           sv_src_stride, sv_xmax,   sv_ymax};
      zip_rows(operation, rect, mapxy_rows, mapfrac_rows, dst_rows);
    } else {
      assert(channels == 4);
      RemapS16Point5ReplicateFourChannels<T> operation{
          src_rows, src_width, src_height, sv_src_stride, sv_xmax, sv_ymax};
      zip_rows(operation, rect, mapxy_rows, mapfrac_rows, dst_rows);
    }
  }
  return KLEIDICV_OK;
}

template <typename ScalarType, bool IsLarge, kleidicv_border_type_t Border>
void remap32f_nearest(svuint32_t sv_xmax, svuint32_t sv_ymax,
                      svuint32_t sv_src_stride, Rows<const ScalarType> src_rows,
                      svuint32_t sv_border, Columns<ScalarType> dst,
                      size_t kStep, size_t dst_width,
                      Rows<const float> mapx_rows,
                      Rows<const float> mapy_rows) {
  svbool_t pg_all32 = svptrue_b32();
  auto load_coords = [&](svbool_t pg, size_t xs) {
    auto x = static_cast<ptrdiff_t>(xs);
    return svcreate2(svld1_f32(pg, &mapx_rows.as_columns()[x]),
                     svld1_f32(pg, &mapy_rows.as_columns()[x]));
  };

  auto get_pixels = [&](svbool_t pg, svuint32x2_t coords) {
    svuint32_t x = svget2(coords, 0);
    svuint32_t y = svget2(coords, 1);
    if constexpr (Border == KLEIDICV_BORDER_TYPE_CONSTANT) {
      svbool_t in_range = svand_b_z(pg, svcmple_u32(pg, x, sv_xmax),
                                    svcmple_u32(pg, y, sv_ymax));
      svuint32_t result = load_common<ScalarType, IsLarge>(
          in_range, x, y, sv_src_stride, src_rows);
      // Select between source pixels and border colour
      return svsel_u32(in_range, result, sv_border);
    } else {
      static_assert(Border == KLEIDICV_BORDER_TYPE_REPLICATE);
      return load_common<ScalarType, IsLarge>(pg, x, y, sv_src_stride,
                                              src_rows);
    }
  };

  auto calculate_nearest_coordinates = [&](svbool_t pg32, size_t x) {
    svfloat32x2_t coords = load_coords(pg32, x);
    svfloat32_t xf = svget2(coords, 0);
    svfloat32_t yf = svget2(coords, 1);

    svuint32_t xi, yi;
    if constexpr (Border == KLEIDICV_BORDER_TYPE_CONSTANT) {
      // Round to the nearest integer
      xi = svreinterpret_u32_s32(
          svcvt_s32_f32_x(pg_all32, svrinta_f32_x(pg_all32, xf)));
      yi = svreinterpret_u32_s32(
          svcvt_s32_f32_x(pg_all32, svrinta_f32_x(pg_all32, yf)));
    } else {
      // Round to the nearest integer, clamp it to within the dimensions of
      // the source image (negative values are already saturated to 0)
      xi = svmin_x(pg_all32,
                   svcvt_u32_f32_x(pg_all32, svadd_n_f32_x(pg_all32, xf, 0.5F)),
                   sv_xmax);
      yi = svmin_x(pg_all32,
                   svcvt_u32_f32_x(pg_all32, svadd_n_f32_x(pg_all32, yf, 0.5F)),
                   sv_ymax);
    }
    return svcreate2(xi, yi);
  };

  LoopUnroll2 loop{dst_width, kStep};

  if constexpr (std::is_same<ScalarType, uint8_t>::value) {
    auto vector_path_generic = [&](size_t x, size_t x_max,
                                   Columns<ScalarType> dst) {
      size_t length = x_max - x;
      svbool_t pg32 = svwhilelt_b32(0ULL, length);
      svuint32_t result =
          get_pixels(pg32, calculate_nearest_coordinates(pg32, x));
      svst1b_u32(pg32, &dst[static_cast<ptrdiff_t>(x)], result);
    };

    loop.unroll_four_times([&](size_t x) {
      ScalarType* p_dst = &dst[static_cast<ptrdiff_t>(x)];
      svuint32_t res32_0 =
          get_pixels(pg_all32, calculate_nearest_coordinates(pg_all32, x));
      x += kStep;
      svuint32_t res32_1 =
          get_pixels(pg_all32, calculate_nearest_coordinates(pg_all32, x));
      svuint16_t result0 = svuzp1_u16(svreinterpret_u16_u32(res32_0),
                                      svreinterpret_u16_u32(res32_1));
      x += kStep;
      res32_0 =
          get_pixels(pg_all32, calculate_nearest_coordinates(pg_all32, x));
      x += kStep;
      res32_1 =
          get_pixels(pg_all32, calculate_nearest_coordinates(pg_all32, x));
      svuint16_t result1 = svuzp1_u16(svreinterpret_u16_u32(res32_0),
                                      svreinterpret_u16_u32(res32_1));
      svuint8_t result = svuzp1_u8(svreinterpret_u8_u16(result0),
                                   svreinterpret_u8_u16(result1));
      svst1(svptrue_b8(), p_dst, result);
    });
    loop.unroll_once([&](size_t x) { vector_path_generic(x, x + kStep, dst); });
    loop.remaining(
        [&](size_t x, size_t length) { vector_path_generic(x, length, dst); });
  }

  if constexpr (std::is_same<ScalarType, uint16_t>::value) {
    auto vector_path_generic = [&](size_t x, size_t x_max,
                                   Columns<ScalarType> dst) {
      size_t length = x_max - x;
      svbool_t pg32 = svwhilelt_b32(0ULL, length);
      svuint32_t result =
          get_pixels(pg32, calculate_nearest_coordinates(pg32, x));
      svst1h_u32(pg32, &dst[static_cast<ptrdiff_t>(x)], result);
    };

    loop.unroll_twice([&](size_t x) {
      ScalarType* p_dst = &dst[static_cast<ptrdiff_t>(x)];
      svuint32_t res32_0 =
          get_pixels(pg_all32, calculate_nearest_coordinates(pg_all32, x));
      x += kStep;
      svuint32_t res32_1 =
          get_pixels(pg_all32, calculate_nearest_coordinates(pg_all32, x));
      svuint16_t result = svuzp1_u16(svreinterpret_u16_u32(res32_0),
                                     svreinterpret_u16_u32(res32_1));
      svst1(svptrue_b16(), p_dst, result);
    });
    loop.unroll_once([&](size_t x) { vector_path_generic(x, x + kStep, dst); });
    loop.remaining(
        [&](size_t x, size_t length) { vector_path_generic(x, length, dst); });
  }
}

// TODO reduce functional complexity
template <typename ScalarType, bool IsLarge,
          kleidicv_interpolation_type_t Inter, kleidicv_border_type_t Border>
void remap32f_process_rows(Rows<const ScalarType> src_rows, size_t src_width,
                           size_t src_height, const ScalarType* border_value,
                           Rows<ScalarType> dst_rows, size_t dst_width,
                           size_t y_begin, size_t y_end,
                           Rows<const float> mapx_rows,
                           Rows<const float> mapy_rows) {
  svbool_t pg_all32 = svptrue_b32();
  svuint32_t sv_xmax = svdup_n_u32(src_width - 1);
  svuint32_t sv_ymax = svdup_n_u32(src_height - 1);
  svuint32_t sv_src_stride = svdup_n_u32(src_rows.stride());
  svuint32_t sv_border = svdup_n_u32(0);

  if constexpr (Border == KLEIDICV_BORDER_TYPE_CONSTANT) {
    sv_border = svdup_n_u32(border_value[0]);
  }

  svfloat32_t xmaxf = svdup_n_f32(static_cast<float>(src_width - 1));
  svfloat32_t ymaxf = svdup_n_f32(static_cast<float>(src_height - 1));

  const size_t kStep = VecTraits<float>::num_lanes();

  // auto get_coordinates = [&](svbool_t pg, size_t xs) {
  auto coordinate_getter = [&](svbool_t pg, size_t xs) {
    auto x = static_cast<ptrdiff_t>(xs);
    return svcreate2(svld1_f32(pg, &mapx_rows.as_columns()[x]),
                     svld1_f32(pg, &mapy_rows.as_columns()[x]));
  };

  auto calculate_linear = [&](svbool_t pg, uint32_t x) {
    if constexpr (Border == KLEIDICV_BORDER_TYPE_REPLICATE) {
      svfloat32x2_t coords = coordinate_getter(pg, x);
      return calculate_linear_replicated_border<ScalarType, IsLarge>(
          pg, coords, xmaxf, ymaxf, sv_src_stride, src_rows);
    } else {
      static_assert(Border == KLEIDICV_BORDER_TYPE_CONSTANT);
      svfloat32x2_t coords = coordinate_getter(pg, x);
      return calculate_linear_constant_border<ScalarType, IsLarge>(
          pg, coords, sv_border, sv_xmax, sv_ymax, sv_src_stride, src_rows);
    }
  };

  for (size_t y = y_begin; y < y_end; ++y) {
    Columns<ScalarType> dst = dst_rows.as_columns();
    LoopUnroll2 loop{dst_width, kStep};
    if constexpr (Inter == KLEIDICV_INTERPOLATION_NEAREST) {
      remap32f_nearest<ScalarType, IsLarge, Border>(
          sv_xmax, sv_ymax, sv_src_stride, src_rows, sv_border, dst, kStep,
          dst_width, mapx_rows, mapy_rows);
    } else if constexpr (Inter == KLEIDICV_INTERPOLATION_LINEAR) {
      if constexpr (std::is_same<ScalarType, uint8_t>::value) {
        loop.unroll_four_times([&](size_t x) {
          ScalarType* p_dst = &dst[static_cast<ptrdiff_t>(x)];
          svuint32_t res0 = calculate_linear(pg_all32, x);
          x += kStep;
          svuint32_t res1 = calculate_linear(pg_all32, x);
          svuint16_t result16_0 = svuzp1_u16(svreinterpret_u16_u32(res0),
                                             svreinterpret_u16_u32(res1));
          x += kStep;
          res0 = calculate_linear(pg_all32, x);
          x += kStep;
          res1 = calculate_linear(pg_all32, x);
          svuint16_t result16_1 = svuzp1_u16(svreinterpret_u16_u32(res0),
                                             svreinterpret_u16_u32(res1));
          svst1_u8(svptrue_b8(), p_dst,
                   svuzp1_u8(svreinterpret_u8_u16(result16_0),
                             svreinterpret_u8_u16(result16_1)));
        });
      } else if constexpr (std::is_same<ScalarType, uint16_t>::value) {
        loop.unroll_twice([&](size_t x) {
          ScalarType* p_dst = &dst[static_cast<ptrdiff_t>(x)];
          svuint32_t res0 = calculate_linear(pg_all32, x);
          x += kStep;
          svuint32_t res1 = calculate_linear(pg_all32, x);
          svuint16_t result16 = svuzp1_u16(svreinterpret_u16_u32(res0),
                                           svreinterpret_u16_u32(res1));
          svst1_u16(svptrue_b16(), p_dst, result16);
        });
      }
      loop.unroll_once([&](size_t x) {
        ScalarType* p_dst = &dst[static_cast<ptrdiff_t>(x)];
        svuint32_t result = calculate_linear(pg_all32, x);
        if constexpr (std::is_same<ScalarType, uint8_t>::value) {
          svst1b_u32(pg_all32, p_dst, result);
        }
        if constexpr (std::is_same<ScalarType, uint16_t>::value) {
          svst1h_u32(pg_all32, p_dst, result);
        }
      });
      loop.remaining([&](size_t x, size_t x_max) {
        ScalarType* p_dst = &dst[static_cast<ptrdiff_t>(x)];
        svbool_t pg32 = svwhilelt_b32(x, x_max);
        svuint32_t result = calculate_linear(pg32, x);
        if constexpr (std::is_same<ScalarType, uint8_t>::value) {
          svst1b_u32(pg32, p_dst, result);
        }
        if constexpr (std::is_same<ScalarType, uint16_t>::value) {
          svst1h_u32(pg32, p_dst, result);
        }
      });
    } else {
      static_assert(Inter == KLEIDICV_INTERPOLATION_NEAREST ||
                        Inter == KLEIDICV_INTERPOLATION_LINEAR,
                    ": Unknown interpolation type!");
    }
    ++mapx_rows;
    ++mapy_rows;
    ++dst_rows;
  }
}
// NOLINTEND(readability-function-cognitive-complexity)

// Most of the complexity comes from parameter checking.
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <typename T>
kleidicv_error_t remap_f32_sc(const T* src, size_t src_stride, size_t src_width,
                              size_t src_height, T* dst, size_t dst_stride,
                              size_t dst_width, size_t dst_height,
                              size_t channels, const float* mapx,
                              size_t mapx_stride, const float* mapy,
                              size_t mapy_stride,
                              kleidicv_interpolation_type_t interpolation,
                              kleidicv_border_type_t border_type,
                              [[maybe_unused]] const T* border_value) {
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

  remap32f_process_rows<T>(remap_image_is_large(src_rows, src_height),
                           interpolation, border_type, src_rows, src_width,
                           src_height, border_value, dst_rows, dst_width, 0,
                           dst_height, mapx_rows, mapy_rows);

  return KLEIDICV_OK;
}
// NOLINTEND(readability-function-cognitive-complexity)

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_REMAP_SC_H
