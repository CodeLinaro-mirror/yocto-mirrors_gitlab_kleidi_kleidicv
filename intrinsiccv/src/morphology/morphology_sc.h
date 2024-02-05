// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_MORPHOLOGY_SC_H
#define INTRINSICCV_MORPHOLOGY_SC_H

#include <algorithm>
#include <limits>

#include "intrinsiccv.h"
#include "morphology/workspace.h"
#include "sve2.h"
#include "types.h"

namespace intrinsiccv::sve2 {

template <typename ScalarType, typename O>
class VerticalOp final {
 public:
  using VecTraits = sve2::VecTraits<ScalarType>;

  VerticalOp(Rectangle rect, Rectangle kernel) INTRINSICCV_STREAMING_COMPATIBLE
      : rect_(rect),
        kernel_(kernel) {}

  void process_rows(IndirectRows<ScalarType> src_rows,
                    Rows<ScalarType> dst_rows)
      INTRINSICCV_STREAMING_COMPATIBLE {
    if (INTRINSICCV_UNLIKELY(kernel_.height()) == 1) {
      CopyRows<ScalarType>::copy_rows(rect_, src_rows, dst_rows);
      return;
    }

    // Iterate across the rows from top to bottom. This implementation can
    // handle two rows at once.
    for (size_t height = 0; height < rect_.height(); height += 2) {
      // Iterate across the columns from left to right.
      LoopUnroll2 loop{rect_.width() * src_rows.channels(),
                       VecTraits::num_lanes()};
      // clang-format off
      loop.unroll_four_times([&](size_t index) INTRINSICCV_STREAMING_COMPATIBLE {
            vector_path_4x(src_rows, dst_rows, index, height);
          })
          .unroll_twice([&](size_t index) INTRINSICCV_STREAMING_COMPATIBLE {
            vector_path_2x(src_rows, dst_rows, index, height);
          })
          .remaining([&](size_t index, size_t length) INTRINSICCV_STREAMING_COMPATIBLE {
            svbool_t pg = VecTraits::svwhilelt(index, length);
            while (svptest_first(VecTraits::svptrue(), pg)) {
              vector_path(pg, src_rows, dst_rows, index, height);
              index += VecTraits::num_lanes();
              pg = VecTraits::svwhilelt(index, length);
            }
          });
      // clang-format on
      src_rows += 2;
      dst_rows += 2;
    }
  }

 private:
  void vector_path_4x(IndirectRows<ScalarType> src_rows,
                      Rows<ScalarType> dst_rows, const size_t index,
                      const size_t height) INTRINSICCV_STREAMING_COMPATIBLE {
    const ScalarType *src_row = &src_rows[index];
    auto first_row0 = svld1(VecTraits::svptrue(), &src_row[0]);
    auto first_row1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);
    auto first_row2 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 2);
    auto first_row3 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 3);
    ++src_rows;

    src_row = &src_rows[index];
    auto acc0 = svld1(VecTraits::svptrue(), &src_row[0]);
    auto acc1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);
    auto acc2 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 2);
    auto acc3 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 3);
    ++src_rows;

    LoopUnroll loop{kernel_.height() - 2, 2};

    loop.unroll_once([&](size_t step) INTRINSICCV_STREAMING_COMPATIBLE {
      const ScalarType *src_row0 = &src_rows.at(0)[index];
      const ScalarType *src_row1 = &src_rows.at(1)[index];
      auto row00 = svld1(VecTraits::svptrue(), src_row0);
      auto row01 = svld1_vnum(VecTraits::svptrue(), src_row0, 1);
      auto row02 = svld1_vnum(VecTraits::svptrue(), src_row0, 2);
      auto row03 = svld1_vnum(VecTraits::svptrue(), src_row0, 3);
      auto row10 = svld1(VecTraits::svptrue(), src_row1);
      auto row11 = svld1_vnum(VecTraits::svptrue(), src_row1, 1);
      auto row12 = svld1_vnum(VecTraits::svptrue(), src_row1, 2);
      auto row13 = svld1_vnum(VecTraits::svptrue(), src_row1, 3);
      acc0 = O::operation(VecTraits::svptrue(), acc0,
                          O::operation(VecTraits::svptrue(), row00, row10));
      acc1 = O::operation(VecTraits::svptrue(), acc1,
                          O::operation(VecTraits::svptrue(), row01, row11));
      acc2 = O::operation(VecTraits::svptrue(), acc2,
                          O::operation(VecTraits::svptrue(), row02, row12));
      acc3 = O::operation(VecTraits::svptrue(), acc3,
                          O::operation(VecTraits::svptrue(), row03, row13));
      src_rows += step;
    });

    loop.tail([&](size_t /* index */)  // NOLINT(readability/casting)
              INTRINSICCV_STREAMING_COMPATIBLE {
                const ScalarType *src_row = &src_rows[index];
                auto row0 = svld1(VecTraits::svptrue(), &src_row[0]);
                auto row1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);
                auto row2 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 2);
                auto row3 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 3);
                acc0 = O::operation(VecTraits::svptrue(), acc0, row0);
                acc1 = O::operation(VecTraits::svptrue(), acc1, row1);
                acc2 = O::operation(VecTraits::svptrue(), acc2, row2);
                acc3 = O::operation(VecTraits::svptrue(), acc3, row3);
                ++src_rows;
              });

    // Save partial results which do not contain the first row.
    auto partial_acc0 = acc0;
    auto partial_acc1 = acc1;
    auto partial_acc2 = acc2;
    auto partial_acc3 = acc3;

    // Take the first row into account.
    acc0 = O::operation(VecTraits::svptrue(), acc0, first_row0);
    acc1 = O::operation(VecTraits::svptrue(), acc1, first_row1);
    acc2 = O::operation(VecTraits::svptrue(), acc2, first_row2);
    acc3 = O::operation(VecTraits::svptrue(), acc3, first_row3);

    // Store the results.
    ScalarType *dst_row = &dst_rows[index];
    svst1(VecTraits::svptrue(), &dst_row[0], acc0);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 1, acc1);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 2, acc2);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 3, acc3);

    // Try to process one more row, because it is relatively cheap to do so.
    if (INTRINSICCV_UNLIKELY((height + 1) >= rect_.height())) {
      return;
    }

    ++dst_rows;

    src_row = &src_rows[index];
    auto next_row0 = svld1(VecTraits::svptrue(), &src_row[0]);
    auto next_row1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);
    auto next_row2 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 2);
    auto next_row3 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 3);

    acc0 = O::operation(VecTraits::svptrue(), partial_acc0, next_row0);
    acc1 = O::operation(VecTraits::svptrue(), partial_acc1, next_row1);
    acc2 = O::operation(VecTraits::svptrue(), partial_acc2, next_row2);
    acc3 = O::operation(VecTraits::svptrue(), partial_acc3, next_row3);

    // Store the results.
    dst_row = &dst_rows[index];
    svst1(VecTraits::svptrue(), &dst_row[0], acc0);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 1, acc1);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 2, acc2);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 3, acc3);
  }

  void vector_path_2x(IndirectRows<ScalarType> src_rows,
                      Rows<ScalarType> dst_rows, const size_t index,
                      const size_t height) INTRINSICCV_STREAMING_COMPATIBLE {
    const ScalarType *src_row = &src_rows[index];
    auto first_row0 = svld1(VecTraits::svptrue(), &src_row[0]);
    auto first_row1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);
    ++src_rows;

    src_row = &src_rows[index];
    auto acc0 = svld1(VecTraits::svptrue(), &src_row[0]);
    auto acc1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);
    ++src_rows;

    LoopUnroll loop{kernel_.height() - 2, 2};

    loop.unroll_once([&](size_t step) INTRINSICCV_STREAMING_COMPATIBLE {
      const ScalarType *src_row0 = &src_rows.at(0)[index];
      const ScalarType *src_row1 = &src_rows.at(1)[index];
      auto row00 = svld1(VecTraits::svptrue(), src_row0);
      auto row01 = svld1_vnum(VecTraits::svptrue(), src_row0, 1);
      auto row10 = svld1(VecTraits::svptrue(), src_row1);
      auto row11 = svld1_vnum(VecTraits::svptrue(), src_row1, 1);
      acc0 = O::operation(VecTraits::svptrue(), acc0,
                          O::operation(VecTraits::svptrue(), row00, row10));
      acc1 = O::operation(VecTraits::svptrue(), acc1,
                          O::operation(VecTraits::svptrue(), row01, row11));
      src_rows += step;
    });

    loop.tail([&](size_t /* index */)  // NOLINT(readability/casting)
              INTRINSICCV_STREAMING_COMPATIBLE {
                const ScalarType *src_row = &src_rows[index];
                auto row0 = svld1(VecTraits::svptrue(), &src_row[0]);
                auto row1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);
                acc0 = O::operation(VecTraits::svptrue(), acc0, row0);
                acc1 = O::operation(VecTraits::svptrue(), acc1, row1);
                ++src_rows;
              });

    // Save partial results which do not contain the first row.
    auto partial_acc0 = acc0;
    auto partial_acc1 = acc1;

    // Take the first row into account.
    acc0 = O::operation(VecTraits::svptrue(), acc0, first_row0);
    acc1 = O::operation(VecTraits::svptrue(), acc1, first_row1);

    // Store the results.
    ScalarType *dst_row = &dst_rows[index];
    svst1(VecTraits::svptrue(), &dst_row[0], acc0);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 1, acc1);

    // Try to process one more row, because it is relatively cheap to do so.
    if (INTRINSICCV_UNLIKELY((height + 1) >= rect_.height())) {
      return;
    }

    ++dst_rows;

    src_row = &src_rows[index];
    auto next_row0 = svld1(VecTraits::svptrue(), &src_row[0]);
    auto next_row1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);

    acc0 = O::operation(VecTraits::svptrue(), partial_acc0, next_row0);
    acc1 = O::operation(VecTraits::svptrue(), partial_acc1, next_row1);

    dst_row = &dst_rows[index];
    svst1(VecTraits::svptrue(), &dst_row[0], acc0);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 1, acc1);
  }

  void vector_path(svbool_t pg, IndirectRows<ScalarType> src_rows,
                   Rows<ScalarType> dst_rows, const size_t index,
                   const size_t height) INTRINSICCV_STREAMING_COMPATIBLE {
    auto first_row = svld1(pg, &src_rows[index]);
    ++src_rows;

    auto acc = svld1(pg, &src_rows[index]);
    ++src_rows;

    LoopUnroll loop{kernel_.height() - 2, 2};

    loop.unroll_once([&](size_t step) INTRINSICCV_STREAMING_COMPATIBLE {
      auto row0 = svld1(pg, &src_rows.at(0)[index]);
      auto row1 = svld1(pg, &src_rows.at(1)[index]);
      acc = O::operation(pg, acc, O::operation(pg, row0, row1));
      src_rows += step;
    });

    loop.tail([&](size_t /* index */)  // NOLINT(readability/casting)
              INTRINSICCV_STREAMING_COMPATIBLE {
                auto row = svld1(pg, &src_rows[index]);
                acc = O::operation(pg, acc, row);
                ++src_rows;
              });

    // Save partial result which does not contain the first row.
    auto partial_acc = acc;

    // Take the first row into account.
    acc = O::operation(pg, acc, first_row);

    // Store the results.
    svst1(pg, &dst_rows[index], acc);

    // Try to process one more row, because it is relatively cheap to do so.
    if (INTRINSICCV_UNLIKELY((height + 1) >= rect_.height())) {
      return;
    }

    ++dst_rows;

    auto next_row = svld1(pg, &src_rows[index]);
    acc = O::operation(pg, partial_acc, next_row);
    svst1(pg, &dst_rows[index], acc);
  }

  Rectangle rect_;
  Rectangle kernel_;
};  // end of class VerticalOp<ScalarType, )>

template <typename ScalarType, typename O>
class HorizontalOp final {
 public:
  using VecTraits = sve2::VecTraits<ScalarType>;

  HorizontalOp(Rectangle rect,
               Rectangle kernel) INTRINSICCV_STREAMING_COMPATIBLE
      : rect_(rect),
        kernel_(kernel) {}

  void process_rows(Rows<const ScalarType> src_rows, Rows<ScalarType> dst_rows)
      INTRINSICCV_STREAMING_COMPATIBLE {
    // Iterate across the rows from top to bottom.
    for (size_t height = 0; height < rect_.height(); ++height) {
      // Iterate across the columns from left to right.
      LoopUnroll2 loop{rect_.width() * src_rows.channels(),
                       VecTraits::num_lanes()};
      // clang-format off
      loop.unroll_four_times([&](size_t index) INTRINSICCV_STREAMING_COMPATIBLE {
            vector_path_4x(src_rows, dst_rows, index);
          })
          .unroll_twice([&](size_t index) INTRINSICCV_STREAMING_COMPATIBLE {
            vector_path_2x(src_rows, dst_rows, index);
          })
          .remaining([&](size_t index, size_t length) INTRINSICCV_STREAMING_COMPATIBLE {
            svbool_t pg = VecTraits::svwhilelt(index, length);
            while (svptest_first(VecTraits::svptrue(), pg)) {
              vector_path(pg, src_rows, dst_rows, index);
              index += VecTraits::num_lanes();
              pg = VecTraits::svwhilelt(index, length);
            }
          });
      // clang-format on
      ++src_rows;
      ++dst_rows;
    }
  }

 private:
  void vector_path_4x(Rows<const ScalarType> src_rows,
                      Rows<ScalarType> dst_rows,
                      const size_t index) INTRINSICCV_STREAMING_COMPATIBLE {
    const auto *src_row = &src_rows[index];
    auto acc0 = svld1(VecTraits::svptrue(), &src_row[0]);
    auto acc1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);
    auto acc2 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 2);
    auto acc3 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 3);

    for (size_t width = 1; width < kernel_.width(); ++width) {
      src_row = &src_rows[index + width * src_rows.channels()];
      auto row0 = svld1(VecTraits::svptrue(), &src_row[0]);
      auto row1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);
      auto row2 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 2);
      auto row3 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 3);
      acc0 = O::operation(VecTraits::svptrue(), acc0, row0);
      acc1 = O::operation(VecTraits::svptrue(), acc1, row1);
      acc2 = O::operation(VecTraits::svptrue(), acc2, row2);
      acc3 = O::operation(VecTraits::svptrue(), acc3, row3);
    }

    auto dst_row = &dst_rows[index];
    svst1(VecTraits::svptrue(), &dst_row[0], acc0);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 1, acc1);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 2, acc2);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 3, acc3);
  }

  void vector_path_2x(Rows<const ScalarType> src_rows,
                      Rows<ScalarType> dst_rows,
                      const size_t index) INTRINSICCV_STREAMING_COMPATIBLE {
    const auto *src_row = &src_rows[index];
    auto acc0 = svld1(VecTraits::svptrue(), &src_row[0]);
    auto acc1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);

    for (size_t width = 1; width < kernel_.width(); ++width) {
      src_row = &src_rows[index + width * src_rows.channels()];
      auto row0 = svld1(VecTraits::svptrue(), &src_row[0]);
      auto row1 = svld1_vnum(VecTraits::svptrue(), &src_row[0], 1);
      acc0 = O::operation(VecTraits::svptrue(), acc0, row0);
      acc1 = O::operation(VecTraits::svptrue(), acc1, row1);
    }

    auto dst_row = &dst_rows[index];
    svst1(VecTraits::svptrue(), &dst_row[0], acc0);
    svst1_vnum(VecTraits::svptrue(), &dst_row[0], 1, acc1);
  }

  void vector_path(svbool_t pg, Rows<const ScalarType> src_rows,
                   Rows<ScalarType> dst_rows,
                   const size_t index) INTRINSICCV_STREAMING_COMPATIBLE {
    auto acc = svld1(pg, &src_rows[index]);

    for (size_t width = 1; width < kernel_.width(); ++width) {
      const auto *src_row = &src_rows[index + width * src_rows.channels()];
      acc = O::operation(pg, acc, svld1(pg, &src_row[0]));
    }

    svst1(pg, &dst_rows[index], acc);
  }

  Rectangle rect_;
  Rectangle kernel_;
};  // end of class HorizontalOp<ScalarType>

template <typename ScalarType>
class Min final {
 public:
  using VecTraits = sve2::VecTraits<ScalarType>;
  using VectorType = typename VecTraits::VectorType;

  static VectorType operation(svbool_t pg, VectorType lhs,
                              VectorType rhs) INTRINSICCV_STREAMING_COMPATIBLE {
    return svmin_x(pg, lhs, rhs);
  }
};  // end of class Min<ScalarType>

template <typename ScalarType>
class Max final {
 public:
  using VecTraits = sve2::VecTraits<ScalarType>;
  using VectorType = typename VecTraits::VectorType;

  static VectorType operation(svbool_t pg, VectorType lhs,
                              VectorType rhs) INTRINSICCV_STREAMING_COMPATIBLE {
    return svmax_x(pg, lhs, rhs);
  }
};  // end of class Max<ScalarType>

template <typename T>
using VerticalMin = VerticalOp<T, Min<T>>;
template <typename T>
using VerticalMax = VerticalOp<T, Max<T>>;

template <typename T>
using HorizontalMin = HorizontalOp<T, Min<T>>;
template <typename T>
using HorizontalMax = HorizontalOp<T, Max<T>>;

template <typename ScalarType>
class DilateOperation final {
 public:
  using SourceType = ScalarType;
  using BufferType = ScalarType;
  using DestinationType = ScalarType;

  explicit DilateOperation(Rectangle kernel) INTRINSICCV_STREAMING_COMPATIBLE
      : kernel_{kernel} {}

  void process_horizontal(Rectangle rect, Rows<const SourceType> src_rows,
                          Rows<BufferType> dst_rows)
      INTRINSICCV_STREAMING_COMPATIBLE {
    sve2::HorizontalMax<ScalarType>{rect, kernel_}.process_rows(src_rows,
                                                                dst_rows);
  }

  void process_vertical(Rectangle rect, IndirectRows<BufferType> src_rows,
                        Rows<DestinationType> dst_rows)
      INTRINSICCV_STREAMING_COMPATIBLE {
    sve2::VerticalMax<ScalarType>{rect, kernel_}.process_rows(src_rows,
                                                              dst_rows);
  }

 private:
  Rectangle kernel_;
};  // end of class DilateOperation<ScalarType>

template <typename T>
static intrinsiccv_error_t dilate_sc(const T *src, size_t src_stride, T *dst,
                                     size_t dst_stride, size_t width,
                                     size_t height,
                                     intrinsiccv_morphology_context_t *context)
    INTRINSICCV_STREAMING_COMPATIBLE {
  CHECK_POINTERS(src, dst, context);

  auto *workspace = reinterpret_cast<MorphologyWorkspace *>(context);

  Rectangle rect{width, height};
  Rectangle kernel{workspace->kernel()};
  Rows<const T> src_rows{src, src_stride, workspace->channels()};
  Rows<T> dst_rows{dst, dst_stride, workspace->channels()};
  Margin margin{workspace->kernel(), workspace->anchor()};
  Border<T> border{workspace->border_values()};

  Rows<const T> current_src_rows = src_rows;
  Rows<T> current_dst_rows = dst_rows;
  for (size_t iteration = 0; iteration < workspace->iterations(); ++iteration) {
    DilateOperation<T> operation{kernel};
    workspace->process(rect, current_src_rows, current_dst_rows, margin, border,
                       workspace->border_type(), operation);
    // Update source for the next iteration.
    current_src_rows = dst_rows;
  }
  return INTRINSICCV_OK;
}

// Helper structure for erode.
template <typename ScalarType>
class ErodeOperation final {
 public:
  using SourceType = ScalarType;
  using BufferType = ScalarType;
  using DestinationType = ScalarType;

  explicit ErodeOperation(Rectangle kernel) INTRINSICCV_STREAMING_COMPATIBLE
      : kernel_{kernel} {}

  void process_horizontal(Rectangle rect, Rows<const SourceType> src_rows,
                          Rows<BufferType> dst_rows)
      INTRINSICCV_STREAMING_COMPATIBLE {
    sve2::HorizontalMin<ScalarType>{rect, kernel_}.process_rows(src_rows,
                                                                dst_rows);
  }

  void process_vertical(Rectangle rect, IndirectRows<BufferType> src_rows,
                        Rows<DestinationType> dst_rows)
      INTRINSICCV_STREAMING_COMPATIBLE {
    sve2::VerticalMin<ScalarType>{rect, kernel_}.process_rows(src_rows,
                                                              dst_rows);
  }

 private:
  Rectangle kernel_;
};  // end of class ErodeOperation<ScalarType>

template <typename T>
static intrinsiccv_error_t erode_sc(const T *src, size_t src_stride, T *dst,
                                    size_t dst_stride, size_t width,
                                    size_t height,
                                    intrinsiccv_morphology_context_t *context)
    INTRINSICCV_STREAMING_COMPATIBLE {
  CHECK_POINTERS(src, dst, context);

  auto *workspace = reinterpret_cast<MorphologyWorkspace *>(context);

  Rectangle rect{width, height};
  Rectangle kernel{workspace->kernel()};
  Rows<const T> src_rows{src, src_stride, workspace->channels()};
  Rows<T> dst_rows{dst, dst_stride, workspace->channels()};
  Margin margin{workspace->kernel(), workspace->anchor()};
  Border<T> border{workspace->border_values()};

  Rows<const T> current_src_rows = src_rows;
  Rows<T> current_dst_rows = dst_rows;
  for (size_t iteration = 0; iteration < workspace->iterations(); ++iteration) {
    ErodeOperation<T> operation{kernel};
    workspace->process(rect, current_src_rows, current_dst_rows, margin, border,
                       workspace->border_type(), operation);
    // Update source for the next iteration.
    current_src_rows = dst_rows;
  }
  return INTRINSICCV_OK;
}

}  // namespace intrinsiccv::sve2

#endif  // INTRINSICCV_MORPHOLOGY_SC_H
