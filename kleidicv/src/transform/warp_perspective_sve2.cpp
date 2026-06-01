// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <cmath>
#include <type_traits>

#include "kleidicv/ctypes.h"
#include "kleidicv/sve2.h"
#include "kleidicv/types.h"
#include "transform_sve2.h"

namespace kleidicv::sve2 {

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

template <kleidicv_border_type_t Border>
svuint32x2_t calculate_nearest_coordinates(svfloat32x2_t coords,
                                           svuint32_t sv_xmax,
                                           svuint32_t sv_ymax) {
  svbool_t pg_all32 = svptrue_b32();
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
}

template <typename ScalarType, bool IsLarge,
          kleidicv_interpolation_type_t Inter, kleidicv_border_type_t Border,
          size_t Channels>
void transform_operation(Rows<const ScalarType> src_rows, size_t src_width,
                         size_t src_height, const ScalarType *border_value,
                         Rows<ScalarType> dst_rows, size_t dst_width,
                         size_t y_begin, size_t y_end,
                         const float transform[9]) {
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

  svfloat32_t sv_0123 = svcvt_f32_u32_z(pg_all32, svindex_u32(0, 1));
  svfloat32_t T0 = svdup_n_f32(transform[0]);
  svfloat32_t T3 = svdup_n_f32(transform[3]);
  svfloat32_t T6 = svdup_n_f32(transform[6]);
  svfloat32_t tx0, ty0, tw0;

  auto process_rows = [&](auto check_zero_divisor) {
    using CheckZeroDivisor = decltype(check_zero_divisor);

    auto calc_coords = [&](svbool_t, size_t x) {
      svfloat32_t vx = svadd_n_f32_x(pg_all32, sv_0123, static_cast<float>(x));
      // Calculate half-transformed values from the first few pixel values,
      // plus Tn*x, similarly to the one above
      // Calculate inverse weight because division is expensive
      svfloat32_t tw = svmla_x(pg_all32, tw0, vx, T6);
      svfloat32_t iw;
      if constexpr (CheckZeroDivisor::value) {
        svbool_t zero_tw = svcmpeq_n_f32(pg_all32, tw, 0.F);
        iw = svsel_f32(zero_tw, svdup_n_f32(0.F),
                       svdiv_f32_x(pg_all32, svdup_n_f32(1.F), tw));
      } else {
        iw = svdiv_f32_x(pg_all32, svdup_n_f32(1.F), tw);
      }
      svfloat32_t tx = svmla_x(pg_all32, tx0, vx, T0);
      svfloat32_t ty = svmla_x(pg_all32, ty0, vx, T3);

      // Calculate coordinates into the source image
      return svcreate2(svmul_f32_x(pg_all32, tx, iw),
                       svmul_f32_x(pg_all32, ty, iw));
    };

    auto load_nearest_source = [&](svbool_t pg, svuint32x2_t coords) {
      svuint32_t x = svget2(coords, 0);
      svuint32_t y = svget2(coords, 1);
      if constexpr (Border == KLEIDICV_BORDER_TYPE_CONSTANT) {
        return get_pixels_or_border<ScalarType, IsLarge>(
            pg, x, y, sv_border, sv_xmax, sv_ymax, sv_src_stride, src_rows);
      } else {
        return load_xy<ScalarType, IsLarge>(pg, x, y, sv_src_stride, src_rows);
      }
    };

    auto vector_path_nearest_4x = [&](size_t x, Columns<ScalarType> dst) {
      ScalarType *p_dst = &dst[static_cast<ptrdiff_t>(x)];
      svuint32_t res32_0 = load_nearest_source(
          pg_all32, calculate_nearest_coordinates<Border>(
                        calc_coords(pg_all32, x), sv_xmax, sv_ymax));
      x += kStep;
      svuint32_t res32_1 = load_nearest_source(
          pg_all32, calculate_nearest_coordinates<Border>(
                        calc_coords(pg_all32, x), sv_xmax, sv_ymax));
      svuint16_t result0 = svuzp1_u16(svreinterpret_u16_u32(res32_0),
                                      svreinterpret_u16_u32(res32_1));
      x += kStep;
      res32_0 = load_nearest_source(
          pg_all32, calculate_nearest_coordinates<Border>(
                        calc_coords(pg_all32, x), sv_xmax, sv_ymax));
      x += kStep;
      res32_1 = load_nearest_source(
          pg_all32, calculate_nearest_coordinates<Border>(
                        calc_coords(pg_all32, x), sv_xmax, sv_ymax));
      svuint16_t result1 = svuzp1_u16(svreinterpret_u16_u32(res32_0),
                                      svreinterpret_u16_u32(res32_1));
      svuint8_t result = svuzp1_u8(svreinterpret_u8_u16(result0),
                                   svreinterpret_u8_u16(result1));
      svst1(svptrue_b8(), p_dst, result);
    };

    auto vector_path_nearest_tail = [&](size_t x, size_t x_max,
                                        Columns<ScalarType> dst) {
      size_t length = x_max - x;
      svbool_t pg32 = svwhilelt_b32(0ULL, length);

      svuint32x2_t coords = calculate_nearest_coordinates<Border>(
          calc_coords(pg32, x), sv_xmax, sv_ymax);
      svuint32_t result = load_nearest_source(pg32, coords);
      svst1b_u32(pg32, &dst[static_cast<ptrdiff_t>(x)], result);
    };

    // WarpPerspective does not implement 2 channels, so this is dummy
    svuint8_t dummy_load_table_2ch{};

    auto calculate_linear = [&](svbool_t pg, uint32_t x) {
      svfloat32x2_t coords = calc_coords(pg, x);
      if constexpr (Border == KLEIDICV_BORDER_TYPE_REPLICATE) {
        return calculate_linear_replicated_border<ScalarType, IsLarge, 1>(
            pg, coords, xmaxf, ymaxf, sv_src_stride, src_rows,
            dummy_load_table_2ch);
      } else {
        static_assert(Border == KLEIDICV_BORDER_TYPE_CONSTANT);
        return calculate_linear_constant_border<ScalarType, IsLarge, 1>(
            pg, coords, sv_border, sv_xmax, sv_ymax, sv_src_stride, src_rows,
            dummy_load_table_2ch);
      }
    };

    for (size_t y = y_begin; y < y_end; ++y) {
      float fy = static_cast<float>(y);
      // Calculate half-transformed values at the first pixel (nominators)
      // tw =  T6*x + T7*y + T8
      // tx = (T0*x + T1*y + T2) / tw
      // ty = (T3*x + T4*y + T5) / tw
      tx0 = svdup_n_f32(fmaf(transform[1], fy, transform[2]));
      ty0 = svdup_n_f32(fmaf(transform[4], fy, transform[5]));
      tw0 = svdup_n_f32(fmaf(transform[7], fy, transform[8]));

      Columns<ScalarType> dst = dst_rows.as_columns();
      LoopUnroll2 loop{dst_width, kStep};
      if constexpr (Inter == KLEIDICV_INTERPOLATION_NEAREST) {
        loop.unroll_four_times(
            [&](size_t x) { vector_path_nearest_4x(x, dst); });
        loop.unroll_once(
            [&](size_t x) { vector_path_nearest_tail(x, x + kStep, dst); });
        loop.remaining([&](size_t x, size_t length) {
          vector_path_nearest_tail(x, length, dst);
        });
      } else {
        static_assert(Inter == KLEIDICV_INTERPOLATION_LINEAR,
                      ": Unknown interpolation type!");
        loop.unroll_four_times([&](size_t x) {
          ScalarType *p_dst = &dst[static_cast<ptrdiff_t>(x)];
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
        loop.unroll_once([&](size_t x) {
          ScalarType *p_dst = &dst[static_cast<ptrdiff_t>(x)];
          svuint32_t result = calculate_linear(pg_all32, x);
          svst1b_u32(pg_all32, p_dst, result);
        });
        loop.remaining([&](size_t x, size_t x_max) {
          ScalarType *p_dst = &dst[static_cast<ptrdiff_t>(x)];
          svbool_t pg32 = svwhilelt_b32_u64(x, x_max);
          svuint32_t result = calculate_linear(pg32, x);
          svst1b_u32(pg32, p_dst, result);
        });
      }
      ++dst_rows;
    }
  };

  if (weight_may_be_zero(transform, dst_width, y_begin, y_end)) {
    process_rows(std::true_type{});
  } else {
    process_rows(std::false_type{});
  }
}

template <typename T>
KLEIDICV_LOCALLY_STREAMING kleidicv_error_t warp_perspective_stripe(
    const T *src, size_t src_stride, size_t src_width, size_t src_height,
    T *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t y_begin, size_t y_end, const float transform[9], size_t channels,
    kleidicv_interpolation_type_t interpolation,
    kleidicv_border_type_t border_type, const T *border_value) {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_POINTERS(transform);
  CHECK_IMAGE_SIZE(src_width, src_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);
  if (border_type == KLEIDICV_BORDER_TYPE_CONSTANT && nullptr == border_value) {
    return KLEIDICV_ERROR_NULL_POINTER;
  }

  // Calculating in float32_t will only be precise until 24 bits, and
  // multiplication can only be done with 32x32 bits
  // Empty source image is not supported
  if (src_width >= (1ULL << 24) || src_height >= (1ULL << 24) ||
      dst_width >= (1ULL << 24) || dst_height >= (1ULL << 24) ||
      src_stride >= (1ULL << 32) || src_width == 0 || src_height == 0) {
    return KLEIDICV_ERROR_RANGE;
  }

  Rows<const T> src_rows{src, src_stride, channels};
  Rows<T> dst_rows{dst, dst_stride, channels};
  Rectangle rect{dst_width, dst_height};

  dst_rows += y_begin;

  transform_operation<T>(is_image_large(src_rows, src_height), interpolation,
                         border_type, channels, src_rows, src_width, src_height,
                         border_value, dst_rows, dst_width, y_begin, y_end,
                         transform);

  return KLEIDICV_OK;
}

#define KLEIDICV_INSTANTIATE_WARP_PERSPECTIVE(type)                            \
  template KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t                           \
  warp_perspective_stripe<type>(                                               \
      const type *src, size_t src_stride, size_t src_width, size_t src_height, \
      type *dst, size_t dst_stride, size_t dst_width, size_t dst_height,       \
      size_t y_begin, size_t y_end, const float transformation[9],             \
      size_t channels, kleidicv_interpolation_type_t interpolation,            \
      kleidicv_border_type_t border_type, const type *border_value)

KLEIDICV_INSTANTIATE_WARP_PERSPECTIVE(uint8_t);

}  // namespace kleidicv::sve2
