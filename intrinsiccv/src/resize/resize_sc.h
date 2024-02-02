// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_RESIZE_SC_H
#define INTRINSICCV_RESIZE_SC_H

#include "intrinsiccv.h"
#include "sve2.h"

namespace intrinsiccv::sve2 {

static inline svuint8_t resize_parallel_vectors(svbool_t pg, svuint8_t top_row,
                                                svuint8_t bottom_row)
    INTRINSICCV_STREAMING_COMPATIBLE {
  svuint16_t result_before_averaging_b = svaddlb(top_row, bottom_row);
  svuint16_t result_before_averaging_t = svaddlt(top_row, bottom_row);
  svuint16_t result_before_averaging =
      svadd_x(pg, result_before_averaging_b, result_before_averaging_t);
  return svrshrnb(result_before_averaging, 2);
}

static inline void parallel_rows_vectors_path_2x(
    svbool_t pg, Rows<const uint8_t> src_rows,
    Rows<uint8_t> dst_rows) INTRINSICCV_STREAMING_COMPATIBLE {
  svuint8_t top_line0 = svld1(pg, &src_rows.at(0)[0]);
  svuint8_t bottom_line0 = svld1(pg, &src_rows.at(1)[0]);
  svuint8_t top_line1 = svld1_vnum(pg, &src_rows.at(0)[0], 1);
  svuint8_t bottom_line1 = svld1_vnum(pg, &src_rows.at(1)[0], 1);
  svuint8_t result0 = resize_parallel_vectors(pg, top_line0, bottom_line0);
  svuint8_t result1 = resize_parallel_vectors(pg, top_line1, bottom_line1);
  svst1b(pg, &dst_rows[0], svreinterpret_u16_u8(result0));
  svst1b_vnum(pg, &dst_rows[0], 1, svreinterpret_u16_u8(result1));
}

static inline void parallel_rows_vectors_path(
    svbool_t pg, Rows<const uint8_t> src_rows,
    Rows<uint8_t> dst_rows) INTRINSICCV_STREAMING_COMPATIBLE {
  svuint8_t top_line = svld1(pg, &src_rows.at(0)[0]);
  svuint8_t bottom_line = svld1(pg, &src_rows.at(1)[0]);
  svuint8_t result = resize_parallel_vectors(pg, top_line, bottom_line);
  svst1b(pg, &dst_rows[0], svreinterpret_u16_u8(result));
}

template <typename ScalarType>
static inline void process_parallel_rows(
    Rows<const ScalarType> src_rows, size_t src_width,
    Rows<ScalarType> dst_rows,
    size_t dst_width) INTRINSICCV_STREAMING_COMPATIBLE {
  using VecTraits = sve2::VecTraits<ScalarType>;
  const size_t size_mask = ~static_cast<size_t>(1U);

  // Process rows up to the last even pixel index.
  LoopUnroll2{src_width & size_mask, VecTraits::num_lanes()}
      // Process double vector chunks.
      .unroll_twice([&](size_t index) INTRINSICCV_STREAMING_COMPATIBLE {
        auto pg = VecTraits::svptrue();
        parallel_rows_vectors_path_2x(pg, src_rows.at(0, index),
                                      dst_rows.at(0, index / 2));
      })
      .unroll_once([&](size_t index) INTRINSICCV_STREAMING_COMPATIBLE {
        auto pg = VecTraits::svptrue();
        parallel_rows_vectors_path(pg, src_rows.at(0, index),
                                   dst_rows.at(0, index / 2));
      })
      // Process the remaining chunk of the row.
      .remaining([&](size_t index, size_t length)
                     INTRINSICCV_STREAMING_COMPATIBLE {
                       auto pg = VecTraits::svwhilelt(index, length);
                       parallel_rows_vectors_path(pg, src_rows.at(0, index),
                                                  dst_rows.at(0, index / 2));
                     });

  // Handle the last odd column, if any.
  if (dst_width > (src_width / 2)) {
    dst_rows[dst_width - 1] = rounding_shift_right<uint16_t>(
        static_cast<const uint16_t>(src_rows.at(0, src_width - 1)[0]) +
            src_rows.at(1, src_width - 1)[0],
        1);
  }
}

static inline svuint8_t resize_single_row(svbool_t pg, svuint8_t row)
    INTRINSICCV_STREAMING_COMPATIBLE {
  return svrshrnb(svadalp_x(pg, svdup_u16(0), row), 1);
}

static inline void single_row_vector_path_2x(
    svbool_t pg, Rows<const uint8_t> src_rows,
    Rows<uint8_t> dst_rows) INTRINSICCV_STREAMING_COMPATIBLE {
  svuint8_t line0 = svld1(pg, &src_rows[0]);
  svuint8_t line1 = svld1_vnum(pg, &src_rows[0], 1);
  svuint8_t result0 = svrshrnb(svadalp_x(pg, svdup_u16(0), line0), 1);
  svuint8_t result1 = svrshrnb(svadalp_x(pg, svdup_u16(0), line1), 1);
  svst1b(pg, &dst_rows[0], svreinterpret_u16_u8(result0));
  svst1b_vnum(pg, &dst_rows[0], 1, svreinterpret_u16_u8(result1));
}

static inline void single_row_vector_path(
    svbool_t pg, Rows<const uint8_t> src_rows,
    Rows<uint8_t> dst_rows) INTRINSICCV_STREAMING_COMPATIBLE {
  svuint8_t line = svld1(pg, &src_rows.at(0)[0]);
  svuint8_t result = svrshrnb(svadalp_x(pg, svdup_u16(0), line), 1);
  svst1b(pg, &dst_rows[0], svreinterpret_u16_u8(result));
}

template <typename ScalarType>
static inline void process_single_row(
    Rows<const ScalarType> src_rows, size_t src_width,
    Rows<ScalarType> dst_rows,
    size_t dst_width) INTRINSICCV_STREAMING_COMPATIBLE {
  using VecTraits = sve2::VecTraits<ScalarType>;
  const size_t size_mask = ~static_cast<size_t>(1U);

  // Process rows up to the last even pixel index.
  LoopUnroll2{src_width & size_mask, VecTraits::num_lanes()}
      // Process full vector chunks.
      .unroll_twice([&](size_t index) INTRINSICCV_STREAMING_COMPATIBLE {
        auto pg = VecTraits::svptrue();
        single_row_vector_path_2x(pg, src_rows.at(0, index),
                                  dst_rows.at(0, index / 2));
      })
      .unroll_once([&](size_t index) INTRINSICCV_STREAMING_COMPATIBLE {
        auto pg = VecTraits::svptrue();
        single_row_vector_path(pg, src_rows.at(0, index),
                               dst_rows.at(0, index / 2));
      })
      // Process the remaining chunk of the row.
      .remaining([&](size_t index, size_t length)
                     INTRINSICCV_STREAMING_COMPATIBLE {
                       auto pg = VecTraits::svwhilelt(index, length);
                       single_row_vector_path(pg, src_rows.at(0, index),
                                              dst_rows.at(0, index / 2));
                     });

  // Handle the last odd column, if any.
  if (dst_width > (src_width / 2)) {
    dst_rows[dst_width - 1] = src_rows[src_width - 1];
  }
}

INTRINSICCV_TARGET_FN_ATTRS static intrinsiccv_error_t resize_to_quarter_u8_sc(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width,
    size_t dst_height) INTRINSICCV_STREAMING_COMPATIBLE {
  CHECK_POINTERS(src, dst);

  Rows<const uint8_t> src_rows{src, src_stride, /* channels*/ 1};
  Rows<uint8_t> dst_rows{dst, dst_stride, /* channels*/ 1};
  LoopUnroll2 loop{src_height, /* Process two rows */ 2};

  // Process two rows at once.
  loop.unroll_once([&](size_t)  // NOLINT(readability/casting)
                   INTRINSICCV_STREAMING_COMPATIBLE {
                     process_parallel_rows(src_rows, src_width, dst_rows,
                                           dst_width);
                     src_rows += 2;
                     ++dst_rows;
                   });

  // Handle an odd row, if any.
  if (dst_height > (src_height / 2)) {
    loop.remaining([&](size_t, size_t) INTRINSICCV_STREAMING_COMPATIBLE {
      process_single_row(src_rows, src_width, dst_rows, dst_width);
    });
  }
  return INTRINSICCV_OK;
}

}  // namespace intrinsiccv::sve2

#endif  // INTRINSICCV_RESIZE_SC_H
