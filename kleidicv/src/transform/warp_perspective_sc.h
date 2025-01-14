// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <arm_sve.h>

#include <cassert>
#include <cmath>
#include <utility>

#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/sve2.h"
#include "kleidicv/traits.h"
#include "kleidicv/transform/warp_perspective.h"
#include "kleidicv/types.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Gather load is not available in streaming mode, and in general random access
// is not recommended for SME.
#if !KLEIDICV_TARGET_SME2

// Template for WarpPerspective transformation.
// Destination pixels are filled from the source, by taking pixels using the
// transformed coordinates that are calculated as follows:
//
//                    [ T0, T1, T2 ]   [ x ]
//      (x',y',w') =  [ T3, T4, T5 ] * [ y ]
//                    [ T6, T7, T8 ]   [ 1 ]
//  then
//
//      xt = x' / w'
//      yt = y' / w'
//
//  or putting it together:
//
//      xt = (T0*x + T1*y + T2) / (T6*x + T7*y + T8)
//      yt = (T3*x + T4*y + T5) / (T6*x + T7*y + T8)
//

template <typename ScalarType, bool IsLarge,
          kleidicv_interpolation_type_t Inter, kleidicv_border_type_t Border>
void warp_perspective_operation(Rows<const ScalarType> src_rows,
                                size_t src_width, size_t src_height,
                                const float transform[9],
                                const ScalarType *border_value,
                                Rows<ScalarType> dst_rows, size_t dst_width,
                                size_t y_begin, size_t y_end) {
  svbool_t pg_all64 = svptrue_b64();
  svbool_t pg_all32 = svptrue_b32();
  svfloat32_t sv_0123 = svcvt_f32_u32_z(pg_all32, svindex_u32(0, 1));
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

  svfloat32_t T0 = svdup_n_f32(transform[0]);
  svfloat32_t T3 = svdup_n_f32(transform[3]);
  svfloat32_t T6 = svdup_n_f32(transform[6]);
  svfloat32_t tx0, ty0, tw0;
  svfloat32_t xf, yf;
  svfloat32_t xmaxf = svdup_n_f32(static_cast<float>(src_width - 1));
  svfloat32_t ymaxf = svdup_n_f32(static_cast<float>(src_height - 1));
  svuint32_t xi, yi;

  svbool_t pg32 = pg_all32;
  svbool_t pg64_b = pg_all64, pg64_t = pg_all64;

  const size_t kStep = VecTraits<float>::num_lanes();

  auto calculate_coordinates = [&](size_t x) {
    svfloat32_t vx = svadd_n_f32_x(pg_all32, sv_0123, static_cast<float>(x));
    // Calculate half-transformed values from the first few pixel values,
    // plus Tn*x, similarly to the one above
    // Calculate inverse weight because division is expensive
    svfloat32_t iw =
        svdiv_f32_x(pg_all32, svdup_n_f32(1.F), svmla_x(pg_all32, tw0, vx, T6));
    svfloat32_t tx = svmla_x(pg_all32, tx0, vx, T0);
    svfloat32_t ty = svmla_x(pg_all32, ty0, vx, T3);

    // Calculate coordinates into the source image
    xf = svmul_f32_x(pg_all32, tx, iw);
    yf = svmul_f32_x(pg_all32, ty, iw);
  };

  auto calculate_nearest_coordinates = [&](size_t x) {
    calculate_coordinates(x);

    if constexpr (Border == KLEIDICV_BORDER_TYPE_CONSTANT) {
      // Round to the nearest integer
      xi = svreinterpret_u32_s32(
          svcvt_s32_f32_x(pg_all32, svrinta_f32_x(pg_all32, xf)));
      yi = svreinterpret_u32_s32(
          svcvt_s32_f32_x(pg_all32, svrinta_f32_x(pg_all32, yf)));
    } else {
      // Round to the nearest integer, clamp it to within the dimensions of the
      // source image (negative values are already saturated to 0)
      xi = svmin_x(pg_all32,
                   svcvt_u32_f32_x(pg_all32, svadd_n_f32_x(pg_all32, xf, 0.5F)),
                   sv_xmax);
      yi = svmin_x(pg_all32,
                   svcvt_u32_f32_x(pg_all32, svadd_n_f32_x(pg_all32, yf, 0.5F)),
                   sv_ymax);
    }
  };

  auto load_small = [&](svbool_t pg, svuint32_t x, svuint32_t y) {
    svuint32_t offsets = svmla_x(pg, x, y, sv_src_stride);
    return svld1ub_gather_offset_u32(pg, &src_rows[0], offsets);
  };

  auto load_large = [&](svbool_t pg_b, svbool_t pg_t, svuint32_t x,
                        svuint32_t y) {
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
  };

  auto get_pixels_or_border = [&](svuint32_t x, svuint32_t y) {
    svbool_t in_range = svand_b_z(pg32, svcmple_u32(pg32, x, sv_xmax),
                                  svcmple_u32(pg32, y, sv_ymax));

    svuint32_t result;
    if constexpr (IsLarge) {
      svbool_t pg_b = in_range;
      svbool_t pg_t = svtrn2_b32(in_range, svpfalse());
      result = load_large(pg_b, pg_t, x, y);
    } else {
      result = load_small(in_range, x, y);
    }

    // Select between source pixels and border colour
    return svsel_u32(in_range, result, sv_border);
  };

  auto vector_path_nearest_4x = [&](size_t x, Columns<ScalarType> dst) {
    auto load_source = [&](svuint32_t x, svuint32_t y) {
      if constexpr (Border == KLEIDICV_BORDER_TYPE_CONSTANT) {
        return get_pixels_or_border(x, y);
      } else if constexpr (IsLarge) {
        return load_large(pg_all64, pg_all64, x, y);
      } else {
        return load_small(pg_all32, x, y);
      }
    };
    ScalarType *p_dst = &dst[static_cast<ptrdiff_t>(x)];
    calculate_nearest_coordinates(x);
    svuint32_t res32_0 = load_source(xi, yi);
    x += kStep;
    calculate_nearest_coordinates(x);
    svuint32_t res32_1 = load_source(xi, yi);
    svuint16_t result0 = svuzp1_u16(svreinterpret_u16_u32(res32_0),
                                    svreinterpret_u16_u32(res32_1));
    x += kStep;
    calculate_nearest_coordinates(x);
    res32_0 = load_source(xi, yi);
    x += kStep;
    calculate_nearest_coordinates(x);
    res32_1 = load_source(xi, yi);
    svuint16_t result1 = svuzp1_u16(svreinterpret_u16_u32(res32_0),
                                    svreinterpret_u16_u32(res32_1));
    svuint8_t result =
        svuzp1_u8(svreinterpret_u8_u16(result0), svreinterpret_u8_u16(result1));
    svst1(svptrue_b8(), p_dst, result);
  };

  auto vector_path_nearest_tail = [&](size_t x, size_t x_max,
                                      Columns<ScalarType> dst) {
    size_t length = x_max - x;
    pg64_b = svwhilelt_b64(0ULL, (length + 1) / 2);
    pg64_t = svwhilelt_b64(0ULL, length / 2);
    pg32 = svwhilelt_b32(0ULL, length);
    calculate_nearest_coordinates(x);
    svuint32_t result;

    if constexpr (Border == KLEIDICV_BORDER_TYPE_CONSTANT) {
      result = get_pixels_or_border(xi, yi);
    } else {
      // To avoid losing precision, the final indices use 64 bits
      result = load_large(pg64_b, pg64_t, xi, yi);
    }
    svst1b_u32(pg32, &dst[static_cast<ptrdiff_t>(x)], result);
  };

  auto calculate_linear_replicate = [&](uint32_t x) {
    auto load_source = [&](svuint32_t x, svuint32_t y) {
      if constexpr (IsLarge) {
        return load_large(pg64_b, pg64_t, x, y);
      } else {
        return load_small(pg32, x, y);
      }
    };

    calculate_coordinates(x);
    // Take the integer part, clamp it to within the dimensions of the
    // source image (negative values are already saturated to 0)
    svuint32_t x0 = svcvt_u32_f32_x(pg_all32, svmin_x(pg_all32, xf, xmaxf));
    svuint32_t y0 = svcvt_u32_f32_x(pg_all32, svmin_x(pg_all32, yf, ymaxf));

    // Get fractional part, or 0 if out of range
    svbool_t x_in_range = svand_z(pg_all32, svcmpge_n_f32(pg_all32, xf, 0.F),
                                  svcmplt_f32(pg_all32, xf, xmaxf));
    svbool_t y_in_range = svand_z(pg_all32, svcmpge_n_f32(pg_all32, yf, 0.F),
                                  svcmplt_f32(pg_all32, yf, ymaxf));
    svfloat32_t xfrac = svsel_f32(
        x_in_range, svsub_f32_x(pg_all32, xf, svrintm_x(pg_all32, xf)),
        svdup_n_f32(0.F));
    svfloat32_t yfrac = svsel_f32(
        y_in_range, svsub_f32_x(pg_all32, yf, svrintm_x(pg_all32, yf)),
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
    svfloat32_t result = svmla_f32_x(
        pg_all32, line0, svsub_f32_x(pg_all32, line1, line0), yfrac);
    return svmin_u32_x(
        pg_all32, svdup_n_u32(0xFF),
        svcvt_u32_f32_x(pg_all32, svadd_n_f32_x(pg_all32, result, 0.5F)));
  };

  auto calculate_linear_constant_border = [&](uint32_t x) {
    calculate_coordinates(x);

    // Convert obviously out-of-range coordinates to values that are just beyond
    // the largest permitted image width & height. This avoids the need for
    // special case handling elsewhere.
    svfloat32_t big = svdup_n_f32(1 << 24);
    xf = svsel_f32(svcmple_f32(pg_all32, svabs_f32_x(pg_all32, xf), big), xf,
                   big);
    yf = svsel_f32(svcmple_f32(pg_all32, svabs_f32_x(pg_all32, yf), big), yf,
                   big);

    svfloat32_t xf0 = svrintm_f32_x(pg_all32, xf);
    svfloat32_t yf0 = svrintm_f32_x(pg_all32, yf);

    svint32_t x0 = svcvt_s32_x(pg_all32, xf0);
    svint32_t y0 = svcvt_s32_x(pg_all32, yf0);
    svint32_t x1 = svadd_s32_x(pg_all32, x0, svdup_n_s32(1));
    svint32_t y1 = svadd_s32_x(pg_all32, y0, svdup_n_s32(1));

    svfloat32_t xfrac = svsub_f32_x(pg_all32, xf, xf0);
    svfloat32_t yfrac = svsub_f32_x(pg_all32, yf, yf0);

    svfloat32_t a = svcvt_f32_u32_x(
        pg_all32, get_pixels_or_border(svreinterpret_u32_s32(x0),
                                       svreinterpret_u32_s32(y0)));
    svfloat32_t b = svcvt_f32_u32_x(
        pg_all32, get_pixels_or_border(svreinterpret_u32_s32(x1),
                                       svreinterpret_u32_s32(y0)));
    svfloat32_t line0 =
        svmla_f32_x(pg_all32, a, svsub_f32_x(pg_all32, b, a), xfrac);
    svfloat32_t c = svcvt_f32_u32_x(
        pg_all32, get_pixels_or_border(svreinterpret_u32_s32(x0),
                                       svreinterpret_u32_s32(y1)));
    svfloat32_t d = svcvt_f32_u32_x(
        pg_all32, get_pixels_or_border(svreinterpret_u32_s32(x1),
                                       svreinterpret_u32_s32(y1)));
    svfloat32_t line1 =
        svmla_f32_x(pg_all32, c, svsub_f32_x(pg_all32, d, c), xfrac);
    svfloat32_t result = svmla_f32_x(
        pg_all32, line0, svsub_f32_x(pg_all32, line1, line0), yfrac);
    return svcvt_u32_f32_x(pg_all32, svrinta_f32_x(pg_all32, result));
  };

  auto calculate_linear = [&](uint32_t x) {
    if constexpr (Border == KLEIDICV_BORDER_TYPE_REPLICATE) {
      return calculate_linear_replicate(x);
    } else {
      static_assert(Border == KLEIDICV_BORDER_TYPE_CONSTANT);
      return calculate_linear_constant_border(x);
    }
  };

  auto process_row = [&](size_t y, Columns<ScalarType> dst) {
    float fy = static_cast<float>(y);
    // Calculate half-transformed values at the first pixel (nominators)
    // tw =  T6*x + T7*y + T8
    // tx = (T0*x + T1*y + T2) / tw
    // ty = (T3*x + T4*y + T5) / tw
    tx0 = svdup_n_f32(fmaf(transform[1], fy, transform[2]));
    ty0 = svdup_n_f32(fmaf(transform[4], fy, transform[5]));
    tw0 = svdup_n_f32(fmaf(transform[7], fy, transform[8]));

    pg32 = pg_all32;
    pg64_b = pg64_t = pg_all64;

    LoopUnroll2 loop{dst_width, kStep};
    if constexpr (Inter == KLEIDICV_INTERPOLATION_NEAREST) {
      loop.unroll_four_times([&](size_t x) { vector_path_nearest_4x(x, dst); });
      loop.unroll_once(
          [&](size_t x) { vector_path_nearest_tail(x, x + kStep, dst); });
      loop.remaining([&](size_t x, size_t length) {
        vector_path_nearest_tail(x, length, dst);
      });
    } else if constexpr (Inter == KLEIDICV_INTERPOLATION_LINEAR) {
      loop.unroll_four_times([&](size_t x) {
        ScalarType *p_dst = &dst[static_cast<ptrdiff_t>(x)];
        svuint32_t res0 = calculate_linear(x);
        x += kStep;
        svuint32_t res1 = calculate_linear(x);
        svuint16_t result16_0 = svuzp1_u16(svreinterpret_u16_u32(res0),
                                           svreinterpret_u16_u32(res1));
        x += kStep;
        res0 = calculate_linear(x);
        x += kStep;
        res1 = calculate_linear(x);
        svuint16_t result16_1 = svuzp1_u16(svreinterpret_u16_u32(res0),
                                           svreinterpret_u16_u32(res1));
        svst1_u8(svptrue_b8(), p_dst,
                 svuzp1_u8(svreinterpret_u8_u16(result16_0),
                           svreinterpret_u8_u16(result16_1)));
      });
      loop.unroll_once([&](size_t x) {
        ScalarType *p_dst = &dst[static_cast<ptrdiff_t>(x)];
        svuint32_t result = calculate_linear(x);
        svst1b_u32(pg_all32, p_dst, result);
      });
      loop.remaining([&](size_t x, size_t x_max) {
        ScalarType *p_dst = &dst[static_cast<ptrdiff_t>(x)];
        size_t length = x_max - x;
        pg32 = svwhilelt_b32(0ULL, length);
        if constexpr (IsLarge) {
          pg64_b = svwhilelt_b64(0ULL, (length + 1) / 2);
          pg64_t = svwhilelt_b64(0ULL, length / 2);
        }
        svuint32_t result = calculate_linear(x);
        svst1b_u32(pg32, p_dst, result);
      });
    } else {
      static_assert(Inter == KLEIDICV_INTERPOLATION_NEAREST ||
                        Inter == KLEIDICV_INTERPOLATION_LINEAR,
                    ": Unknown interpolation type!");
    }
  };

  for (size_t y = y_begin; y < y_end; ++y) {
    process_row(y, dst_rows.as_columns());
    ++dst_rows;
  }
}

// Convert border_type to a template argument.
template <typename ScalarType, bool IsLarge,
          kleidicv_interpolation_type_t Inter, typename... Args>
void warp_perspective_operation(kleidicv_border_type_t border_type,
                                Args &&...args) {
  if (border_type == KLEIDICV_BORDER_TYPE_REPLICATE) {
    warp_perspective_operation<ScalarType, IsLarge, Inter,
                               KLEIDICV_BORDER_TYPE_REPLICATE>(
        std::forward<Args>(args)...);
  } else {
    warp_perspective_operation<ScalarType, IsLarge, Inter,
                               KLEIDICV_BORDER_TYPE_CONSTANT>(
        std::forward<Args>(args)...);
  }
}

// Convert interpolation_type to a template argument.
template <typename ScalarType, bool IsLarge, typename... Args>
void warp_perspective_operation(
    kleidicv_interpolation_type_t interpolation_type, Args &&...args) {
  if (interpolation_type == KLEIDICV_INTERPOLATION_NEAREST) {
    warp_perspective_operation<ScalarType, IsLarge,
                               KLEIDICV_INTERPOLATION_NEAREST>(
        std::forward<Args>(args)...);
  } else {
    warp_perspective_operation<ScalarType, IsLarge,
                               KLEIDICV_INTERPOLATION_LINEAR>(
        std::forward<Args>(args)...);
  }
}

// Most of the complexity comes from parameter checking.
// NOLINTBEGIN(readability-function-cognitive-complexity)
template <typename T>
KLEIDICV_LOCALLY_STREAMING kleidicv_error_t warp_perspective_stripe_sc(
    const T *src, size_t src_stride, size_t src_width, size_t src_height,
    T *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t y_begin, size_t y_end, const float transformation[9],
    size_t channels, kleidicv_interpolation_type_t interpolation,
    kleidicv_border_type_t border_type, const T *border_value) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_POINTERS(transformation);
  CHECK_IMAGE_SIZE(src_width, src_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);
  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT && nullptr == border_value) {
    return KLEIDICV_ERROR_NULL_POINTER;
  }

  // Calculating in float32_t will only be precise until 24 bits, and
  // multiplication can only be done with 32x32 bits
  if (src_width >= (1ULL << 24) || src_height >= (1ULL << 24) ||
      dst_width >= (1ULL << 24) || dst_height >= (1ULL << 24) ||
      src_stride >= (1ULL << 32)) {
    return KLEIDICV_ERROR_RANGE;
  }

  Rows<const T> src_rows{src, src_stride, channels};
  Rows<T> dst_rows{dst, dst_stride, channels};
  Rectangle rect{dst_width, dst_height};

  dst_rows += y_begin;

  if (KLEIDICV_UNLIKELY(src_rows.stride() * src_height >= (1ULL << 32))) {
    warp_perspective_operation<T, true>(
        interpolation, border_type, src_rows, src_width, src_height,
        transformation, border_value, dst_rows, dst_width, y_begin, y_end);
  } else {
    warp_perspective_operation<T, false>(
        interpolation, border_type, src_rows, src_width, src_height,
        transformation, border_value, dst_rows, dst_width, y_begin, y_end);
  }

  return KLEIDICV_OK;
}
// NOLINTEND(readability-function-cognitive-complexity)

#define KLEIDICV_INSTANTIATE_WARP_PERSPECTIVE_SC(type)                         \
  template KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t                           \
  warp_perspective_stripe_sc<type>(                                            \
      const type *src, size_t src_stride, size_t src_width, size_t src_height, \
      type *dst, size_t dst_stride, size_t dst_width, size_t dst_height,       \
      size_t y_begin, size_t y_end, const float transformation[9],             \
      size_t channels, kleidicv_interpolation_type_t interpolation,            \
      kleidicv_border_type_t border_type, const type *border_value)

KLEIDICV_INSTANTIATE_WARP_PERSPECTIVE_SC(uint8_t);

#endif

}  // namespace KLEIDICV_TARGET_NAMESPACE
