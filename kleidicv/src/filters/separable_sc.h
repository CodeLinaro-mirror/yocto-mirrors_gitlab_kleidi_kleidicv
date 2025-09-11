// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_SEPARABLE_SC_H
#define KLEIDICV_SEPARABLE_SC_H

#include "border_generic_sc.h"
#include "kleidicv/sve2.h"
#include "kleidicv/types.h"
#include "kleidicv/workspace/border_types.h"
#include "kleidicv/workspace/separable.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Processes rows vertically first along the full width
template <typename FilterType>
void process_using_bordermaker(
    SeparableFilterWorkspace *workspace, Rectangle rect, size_t y_begin,
    size_t y_end, Rows<const typename FilterType::SourceType> src_rows,
    Rows<typename FilterType::DestinationType> dst_rows, size_t channels,
    typename FilterType::BorderType border_type,
    FilterType filter) KLEIDICV_STREAMING {
  using IndexVectorType = typename VecTraits<
      std::make_unsigned_t<typename FilterType::BufferType>>::VectorType;
  if (border_type == FixedBorderType::REPLICATE) {
    if (channels == 3) {
      IndexVectorType sv0, sv1, sv2;
      KLEIDICV_TARGET_NAMESPACE::BorderMaker3ch<typename FilterType::BufferType,
                                                FixedBorderType::REPLICATE>
          border_maker(sv0, sv1, sv2);
      workspace->process_using_bordermaker(rect, y_begin, y_end, src_rows,
                                           dst_rows, channels, border_type,
                                           filter, border_maker);
    } else {
      IndexVectorType sv;
      KLEIDICV_TARGET_NAMESPACE::BorderMaker124ch<
          typename FilterType::BufferType, FixedBorderType::REPLICATE>
          border_maker(static_cast<ptrdiff_t>(channels), sv);
      workspace->process_using_bordermaker(rect, y_begin, y_end, src_rows,
                                           dst_rows, channels, border_type,
                                           filter, border_maker);
    }
  } else if (border_type == FixedBorderType::REVERSE) {
    IndexVectorType sv;
    svbool_t pg;
    KLEIDICV_TARGET_NAMESPACE::BorderMaker<typename FilterType::BufferType,
                                           FixedBorderType::REVERSE>
        border_maker(static_cast<ptrdiff_t>(channels), sv, pg);
    workspace->process_using_bordermaker(rect, y_begin, y_end, src_rows,
                                         dst_rows, channels, border_type,
                                         filter, border_maker);
  } else if (border_type == FixedBorderType::REFLECT) {
    IndexVectorType sv;
    svbool_t pg;
    KLEIDICV_TARGET_NAMESPACE::BorderMaker<typename FilterType::BufferType,
                                           FixedBorderType::REFLECT>
        border_maker(static_cast<ptrdiff_t>(channels), sv, pg);
    workspace->process_using_bordermaker(rect, y_begin, y_end, src_rows,
                                         dst_rows, channels, border_type,
                                         filter, border_maker);
  } else if (border_type == FixedBorderType::WRAP) {
    svbool_t pg;
    KLEIDICV_TARGET_NAMESPACE::BorderMaker<typename FilterType::BufferType,
                                           FixedBorderType::WRAP>
        border_maker(static_cast<ptrdiff_t>(channels), pg);
    workspace->process_using_bordermaker(rect, y_begin, y_end, src_rows,
                                         dst_rows, channels, border_type,
                                         filter, border_maker);
  }
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_SEPARABLE_SC_H
