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
    RemapS16Point5ConstantBorder<T> operation{
        src_rows,      src_width, src_height, border_value,
        sv_src_stride, sv_width,  sv_height,  sv_border};
    zip_rows(operation, rect, mapxy_rows, mapfrac_rows, dst_rows);
  } else {
    assert(border_type == KLEIDICV_BORDER_TYPE_REPLICATE);
    svint16_t sv_xmax, sv_ymax;
    RemapS16Point5Replicate<T> operation{src_rows,      src_width, src_height,
                                         sv_src_stride, sv_xmax,   sv_ymax};
    zip_rows(operation, rect, mapxy_rows, mapfrac_rows, dst_rows);
  }
  return KLEIDICV_OK;
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
  svuint32_t sv_border;
  // sv_border is only used if the border type is constant.
  // If the border type is not constant then border_value is permitted to be
  // null and must not be read.
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

  auto process_row = [&]() {
    Columns<ScalarType> dst = dst_rows.as_columns();
    LoopUnroll2 loop{dst_width, kStep};
    // GCOVR_EXCL_START
    if constexpr (Inter == KLEIDICV_INTERPOLATION_NEAREST) {
      assert(!"INTER_NEAREST not implemented for RemapF32");
      // GCOVR_EXCL_STOP
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
  };

  for (size_t y = y_begin; y < y_end; ++y) {
    process_row();
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
