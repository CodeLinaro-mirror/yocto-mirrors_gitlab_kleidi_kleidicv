// SPDX-FileCopyrightText: 2025 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_MEDIAN_BLUR_BORDER_HANDLING_H
#define KLEIDICV_MEDIAN_BLUR_BORDER_HANDLING_H

#include <algorithm>

#include "kleidicv/kleidicv.h"

namespace kleidicv::neon {

static ptrdiff_t get_physical_index(ptrdiff_t index, size_t limit,
                                    FixedBorderType border_type) {
  ptrdiff_t result = 0;
  ptrdiff_t signed_index = index;
  ptrdiff_t signed_limit = static_cast<ptrdiff_t>(limit);

  if (signed_index >= 0 && signed_index < signed_limit) {
    return index;
  }
  switch (border_type) {
    case FixedBorderType::REFLECT: {
      if (signed_index < 0) {
        result = -signed_index - 1;
      } else {
        result = 2 * signed_limit - signed_index - 1;
      }
      break;
    }

    case FixedBorderType::WRAP: {
      if (signed_index < 0) {
        result = signed_limit + signed_index;
      } else {
        result = signed_index - signed_limit;
      }
      break;
    }

    case FixedBorderType::REVERSE: {
      if (signed_index < 0) {
        result = std::min(-signed_index, signed_limit - 1);
      } else {
        result = 2 * signed_limit - signed_index - 2;
      }
      break;
    }
    default: /* FixedBorderType::REPLICATE */ {
      result = std::clamp(signed_index, ptrdiff_t{0}, signed_limit - 1);
      break;
    }
  }

  return result;
}

}  // namespace kleidicv::neon
#endif  // KLEIDICV_MEDIAN_BLUR_BORDER_HANDLING_H
