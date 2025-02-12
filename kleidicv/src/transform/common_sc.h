// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <arm_sve.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/sve2.h"
#include "kleidicv/traits.h"
#include "kleidicv/types.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Convert border_type to a template argument.
template <typename ScalarType, bool IsLarge,
          kleidicv_interpolation_type_t Inter, typename... Args>
void remap32f_process_rows(kleidicv_border_type_t border_type, Args &&...args) {
  if (border_type == KLEIDICV_BORDER_TYPE_REPLICATE) {
    remap32f_process_rows<ScalarType, IsLarge, Inter,
                          KLEIDICV_BORDER_TYPE_REPLICATE>(
        std::forward<Args>(args)...);
  } else {
    remap32f_process_rows<ScalarType, IsLarge, Inter,
                          KLEIDICV_BORDER_TYPE_CONSTANT>(
        std::forward<Args>(args)...);
  }
}

// Convert interpolation_type to a template argument.
template <typename ScalarType, bool IsLarge, typename... Args>
void remap32f_process_rows(kleidicv_interpolation_type_t interpolation_type,
                           Args &&...args) {
  if (interpolation_type == KLEIDICV_INTERPOLATION_NEAREST) {
    remap32f_process_rows<ScalarType, IsLarge, KLEIDICV_INTERPOLATION_NEAREST>(
        std::forward<Args>(args)...);
  } else {
    remap32f_process_rows<ScalarType, IsLarge, KLEIDICV_INTERPOLATION_LINEAR>(
        std::forward<Args>(args)...);
  }
}

template <typename T>
bool remap_image_is_large(const Rows<T> &rows, size_t height) {
  return rows.stride() * height >= 1ULL << 32;
}

// Convert is_large to a template argument.
template <typename ScalarType, typename... Args>
void remap32f_process_rows(bool is_large, Args &&...args) {
  if (KLEIDICV_UNLIKELY(is_large)) {
    remap32f_process_rows<ScalarType, true>(std::forward<Args>(args)...);
  } else {
    remap32f_process_rows<ScalarType, false>(std::forward<Args>(args)...);
  }
}

template <typename ScalarType, bool IsLarge>
svuint32_t inline load_common(svbool_t pg, svuint32_t x, svuint32_t y,
                              svuint32_t sv_src_stride,
                              Rows<const ScalarType> &src_rows) {
  if constexpr (IsLarge) {
    svbool_t pg_b = pg;
    svbool_t pg_t = svtrn2_b32(pg, svpfalse());

    // Calculate offsets from coordinates (y * stride + x)
    // To avoid losing precision, the final offsets should be in 64 bits
    svuint64_t offsets_b = svmlalb(svmovlb(x), y, sv_src_stride);
    svuint64_t offsets_t = svmlalt(svmovlt(x), y, sv_src_stride);
    // Copy pixels from source
    svuint64_t result_b =
        svld1ub_gather_offset_u64(pg_b, &src_rows[0], offsets_b);
    svuint64_t result_t =
        svld1ub_gather_offset_u64(pg_t, &src_rows[0], offsets_t);
    return svtrn1_u32(svreinterpret_u32_u64(result_b),
                      svreinterpret_u32_u64(result_t));
  } else {
    svuint32_t offsets = svmla_x(pg, x, y, sv_src_stride);
    return svld1ub_gather_offset_u32(pg, &src_rows[0], offsets);
  }
}

template <typename ScalarType, bool IsLarge>
svuint32_t inline calculate_linear_replicated_border(
    svbool_t pg, svfloat32x2_t coords, svfloat32_t xmaxf, svfloat32_t ymaxf,
    svuint32_t sv_src_stride, Rows<const ScalarType> &src_rows) {
  auto load_source = [&](svuint32_t x, svuint32_t y) {
    return load_common<ScalarType, IsLarge>(pg, x, y, sv_src_stride, src_rows);
  };
  svbool_t pg_all32 = svptrue_b32();
  svfloat32_t xf = svget2(coords, 0);
  svfloat32_t yf = svget2(coords, 1);
  // Take the integer part, clamp it to within the dimensions of the
  // source image (negative values are already saturated to 0)
  svuint32_t x0 = svcvt_u32_f32_x(pg_all32, svmin_x(pg_all32, xf, xmaxf));
  svuint32_t y0 = svcvt_u32_f32_x(pg_all32, svmin_x(pg_all32, yf, ymaxf));

  // Get fractional part, or 0 if out of range
  svbool_t x_in_range = svand_z(pg_all32, svcmpge_n_f32(pg_all32, xf, 0.F),
                                svcmplt_f32(pg_all32, xf, xmaxf));
  svbool_t y_in_range = svand_z(pg_all32, svcmpge_n_f32(pg_all32, yf, 0.F),
                                svcmplt_f32(pg_all32, yf, ymaxf));
  svfloat32_t xfrac =
      svsel_f32(x_in_range, svsub_f32_x(pg_all32, xf, svrintm_x(pg_all32, xf)),
                svdup_n_f32(0.F));
  svfloat32_t yfrac =
      svsel_f32(y_in_range, svsub_f32_x(pg_all32, yf, svrintm_x(pg_all32, yf)),
                svdup_n_f32(0.F));

  // x1 = x0 + 1, except if it's already xmax or out of range
  svuint32_t x1 = svsel_u32(x_in_range, svadd_n_u32_x(pg_all32, x0, 1), x0);
  svuint32_t y1 = svsel_u32(y_in_range, svadd_n_u32_x(pg_all32, y0, 1), y0);

  // Calculate offsets from coordinates (y * stride + x)
  // a: top left, b: top right, c: bottom left, d: bottom right
  svfloat32_t a = svcvt_f32_u32_x(pg_all32, load_source(x0, y0));
  svfloat32_t b = svcvt_f32_u32_x(pg_all32, load_source(x1, y0));
  svfloat32_t line0 =
      svmla_f32_x(pg_all32, a, svsub_f32_x(pg_all32, b, a), xfrac);
  svfloat32_t c = svcvt_f32_u32_x(pg_all32, load_source(x0, y1));
  svfloat32_t d = svcvt_f32_u32_x(pg_all32, load_source(x1, y1));
  svfloat32_t line1 =
      svmla_f32_x(pg_all32, c, svsub_f32_x(pg_all32, d, c), xfrac);
  svfloat32_t result =
      svmla_f32_x(pg_all32, line0, svsub_f32_x(pg_all32, line1, line0), yfrac);
  return svcvt_u32_f32_x(pg_all32, svadd_n_f32_x(pg_all32, result, 0.5F));
}

template <typename ScalarType, bool IsLarge>
svuint32_t get_pixels_or_border(svbool_t pg, svuint32_t x, svuint32_t y,
                                svuint32_t sv_border, svuint32_t sv_xmax,
                                svuint32_t sv_ymax, svuint32_t sv_src_stride,
                                Rows<const ScalarType> &src_rows) {
  svbool_t in_range =
      svand_b_z(pg, svcmple_u32(pg, x, sv_xmax), svcmple_u32(pg, y, sv_ymax));
  svuint32_t result =
      load_common<ScalarType, IsLarge>(in_range, x, y, sv_src_stride, src_rows);
  // Select between source pixels and border colour
  return svsel_u32(in_range, result, sv_border);
}

template <typename ScalarType, bool IsLarge>
svuint32_t inline calculate_linear_constant_border(
    svbool_t pg, svfloat32x2_t coords, svuint32_t sv_border, svuint32_t sv_xmax,
    svuint32_t sv_ymax, svuint32_t sv_src_stride,
    Rows<const ScalarType> &src_rows) {
  svfloat32_t xf = svget2(coords, 0);
  svfloat32_t yf = svget2(coords, 1);

  // Convert obviously out-of-range coordinates to values that are just beyond
  // the largest permitted image width & height. This avoids the need for
  // special case handling elsewhere.
  svfloat32_t big = svdup_n_f32(1 << 24);
  xf = svsel_f32(svcmple_f32(pg, svabs_f32_x(pg, xf), big), xf, big);
  yf = svsel_f32(svcmple_f32(pg, svabs_f32_x(pg, yf), big), yf, big);

  svfloat32_t xf0 = svrintm_f32_x(pg, xf);
  svfloat32_t yf0 = svrintm_f32_x(pg, yf);

  svint32_t x0 = svcvt_s32_x(pg, xf0);
  svint32_t y0 = svcvt_s32_x(pg, yf0);
  svint32_t x1 = svadd_s32_x(pg, x0, svdup_n_s32(1));
  svint32_t y1 = svadd_s32_x(pg, y0, svdup_n_s32(1));

  svfloat32_t xfrac = svsub_f32_x(pg, xf, xf0);
  svfloat32_t yfrac = svsub_f32_x(pg, yf, yf0);

  svfloat32_t a = svcvt_f32_u32_x(
      pg, get_pixels_or_border<ScalarType, IsLarge>(
              pg, svreinterpret_u32_s32(x0), svreinterpret_u32_s32(y0),
              sv_border, sv_xmax, sv_ymax, sv_src_stride, src_rows));
  svfloat32_t b = svcvt_f32_u32_x(
      pg, get_pixels_or_border<ScalarType, IsLarge>(
              pg, svreinterpret_u32_s32(x1), svreinterpret_u32_s32(y0),
              sv_border, sv_xmax, sv_ymax, sv_src_stride, src_rows));
  svfloat32_t line0 = svmla_f32_x(pg, a, svsub_f32_x(pg, b, a), xfrac);
  svfloat32_t c = svcvt_f32_u32_x(
      pg, get_pixels_or_border<ScalarType, IsLarge>(
              pg, svreinterpret_u32_s32(x0), svreinterpret_u32_s32(y1),
              sv_border, sv_xmax, sv_ymax, sv_src_stride, src_rows));
  svfloat32_t d = svcvt_f32_u32_x(
      pg, get_pixels_or_border<ScalarType, IsLarge>(
              pg, svreinterpret_u32_s32(x1), svreinterpret_u32_s32(y1),
              sv_border, sv_xmax, sv_ymax, sv_src_stride, src_rows));
  svfloat32_t line1 = svmla_f32_x(pg, c, svsub_f32_x(pg, d, c), xfrac);
  svfloat32_t result =
      svmla_f32_x(pg, line0, svsub_f32_x(pg, line1, line0), yfrac);
  return svcvt_u32_f32_x(pg, svrinta_f32_x(pg, result));
}

}  // namespace KLEIDICV_TARGET_NAMESPACE
