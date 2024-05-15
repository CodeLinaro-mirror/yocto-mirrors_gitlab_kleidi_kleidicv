// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_WORKSPACE_BORDER_TYPES_H
#define KLEIDICV_WORKSPACE_BORDER_TYPES_H

#include <optional>

#include "kleidicv/kleidicv.h"

namespace KLEIDICV_TARGET_NAMESPACE {

enum class FixedBorderType {
  REPLICATE,
  REFLECT,
  WRAP,
  REVERSE,
};

inline std::optional<FixedBorderType> get_fixed_border_type(
    kleidicv_border_type_t border_type) KLEIDICV_STREAMING_COMPATIBLE {
  switch (border_type) {
    case KLEIDICV_BORDER_TYPE_REPLICATE:
      return FixedBorderType::REPLICATE;
    case KLEIDICV_BORDER_TYPE_REFLECT:
      return FixedBorderType::REFLECT;
    case KLEIDICV_BORDER_TYPE_WRAP:
      return FixedBorderType::WRAP;
    case KLEIDICV_BORDER_TYPE_REVERSE:
      return FixedBorderType::REVERSE;
    default:
      return std::optional<FixedBorderType>();
  }
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_WORKSPACE_BORDER_TYPES_H
