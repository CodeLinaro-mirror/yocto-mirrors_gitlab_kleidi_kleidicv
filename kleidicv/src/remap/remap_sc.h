// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_REMAP_SC_H
#define KLEIDICV_REMAP_SC_H

#include <arm_sve.h>

#include <algorithm>
#include <cstddef>
#include <limits>

#include "kleidicv/kleidicv.h"
#include "kleidicv/remap/remap.h"
#include "kleidicv/sve2.h"

namespace KLEIDICV_TARGET_NAMESPACE {

template <typename ScalarType>
class RemapS16;

template <>
class RemapS16<uint8_t> {
 public:
  using ScalarType = uint8_t;
  using MapVecTraits = VecTraits<int16_t>;
  using MapVectorType = typename MapVecTraits::VectorType;
  using MapVector2Type = typename MapVecTraits::Vector2Type;

  RemapS16(Rows<const ScalarType> src_rows, svuint16_t& v_src_stride,
           MapVectorType& v_x_max,
           MapVectorType& v_y_max) KLEIDICV_STREAMING_COMPATIBLE
      : src_rows_{src_rows},
        v_src_stride_{v_src_stride},
        v_xmax_{v_x_max},
        v_ymax_{v_y_max} {}

  void process_row(size_t width, Columns<const int16_t> mapxy,
                   Columns<ScalarType> dst) KLEIDICV_STREAMING_COMPATIBLE {
    svuint32_t offsets_b, offsets_t;
    svint16_t svzero = svdup_n_s16(0);
    auto load_offsets = [&](svbool_t pg) KLEIDICV_STREAMING_COMPATIBLE {
      MapVector2Type xy = svld2_s16(pg, &mapxy[0]);
      // Clamp coordinates to within the dimensions of the source image
      svuint16_t x = svreinterpret_u16_s16(
          svmax_x(pg, svzero, svmin_x(pg, svget2(xy, 0), v_xmax_)));
      svuint16_t y = svreinterpret_u16_s16(
          svmax_x(pg, svzero, svmin_x(pg, svget2(xy, 1), v_ymax_)));
      // Calculate offsets from coordinates (y * stride + x)
      offsets_b = svmlalb_u32(svmovlb_u32(x), y, v_src_stride_);
      offsets_t = svmlalt_u32(svmovlt_u32(x), y, v_src_stride_);
    };

    svbool_t pg_all = MapVecTraits::svptrue();
    // always get element #0
    svbool_t pg_0 = svpfalse();

    auto generic_vector_path =
        [&](svbool_t pg, ptrdiff_t step) KLEIDICV_STREAMING_COMPATIBLE {
          load_offsets(pg);
          // Copy pixels from source
          for (ptrdiff_t i = 0; i < step / 2; ++i) {
            dst[i * 2] = src_rows_[svlasta_u32(pg_0, offsets_b)];
            offsets_b = svext(offsets_b, offsets_b, 1);
            dst[i * 2 + 1] = src_rows_[svlasta_u32(pg_0, offsets_t)];
            offsets_t = svext(offsets_t, offsets_t, 1);
          }
          if (step % 2) {
            dst[step - 1] = src_rows_[svlasta_u32(pg_0, offsets_b)];
          }
          mapxy += step;
          dst += step;
        };

    // NOTE: gather load is not available in streaming mode
    auto gather_load_full_vector_path =
        [&](svbool_t pg, ptrdiff_t step) KLEIDICV_STREAMING_COMPATIBLE {
          load_offsets(pg);
          // Copy pixels from source
          svuint32_t result_b =
              svldnt1ub_gather_u32offset_u32(pg, &src_rows_[0], offsets_b);
          svuint32_t result_t =
              svldnt1ub_gather_u32offset_u32(pg, &src_rows_[0], offsets_t);
          svuint16_t result = svtrn1_u16(svreinterpret_u16_u32(result_b),
                                         svreinterpret_u16_u32(result_t));
          svst1b_u16(pg, &dst[0], result);
          mapxy += step;
          dst += step;
        };

    LoopUnroll loop{width, MapVecTraits::num_lanes()};
    loop.unroll_once([&](size_t step) KLEIDICV_STREAMING_COMPATIBLE {
      gather_load_full_vector_path(pg_all, static_cast<ptrdiff_t>(step));
    });
    loop.remaining(
        [&](size_t length, size_t step) KLEIDICV_STREAMING_COMPATIBLE {
          svbool_t pg = MapVecTraits::svwhilelt(step - length, step);
          generic_vector_path(pg, static_cast<ptrdiff_t>(length));
        });
  }

 private:
  Rows<const ScalarType> src_rows_;
  svuint16_t& v_src_stride_;
  MapVectorType& v_xmax_;
  MapVectorType& v_ymax_;
};  // end of class RemapS16<uint8_t>

template <typename T>
kleidicv_error_t remap_s16_sc(
    const T* src, size_t src_stride, size_t src_width, size_t src_height,
    T* dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t* mapxy, size_t mapxy_stride,
    kleidicv_border_type_t border_type,
    kleidicv_border_values_t) KLEIDICV_STREAMING_COMPATIBLE {
  CHECK_POINTER_AND_STRIDE(src, src_stride, src_height);
  CHECK_POINTER_AND_STRIDE(dst, dst_stride, dst_height);
  CHECK_POINTER_AND_STRIDE(mapxy, mapxy_stride, dst_height);
  CHECK_IMAGE_SIZE(src_width, src_height);
  CHECK_IMAGE_SIZE(dst_width, dst_height);

  if (!remap_s16_is_implemented<T>(dst_width, border_type, channels)) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  }

  Rows<const T> src_rows{src, src_stride, channels};
  Rows<const int16_t> mapxy_rows{mapxy, mapxy_stride, 2};
  Rows<T> dst_rows{dst, dst_stride, channels};
  svuint16_t sv_src_stride = svdup_u16(src_rows.stride());
  svint16_t sv_xmax = svdup_s16(static_cast<int16_t>(
      std::min<size_t>(std::numeric_limits<int16_t>::max(), src_width - 1)));
  svint16_t sv_ymax = svdup_s16(static_cast<int16_t>(
      std::min<size_t>(std::numeric_limits<int16_t>::max(), src_height - 1)));
  RemapS16<T> operation{src_rows, sv_src_stride, sv_xmax, sv_ymax};
  Rectangle rect{dst_width, dst_height};
  zip_rows(operation, rect, mapxy_rows, dst_rows);
  return KLEIDICV_OK;
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_REMAP_SC_H
