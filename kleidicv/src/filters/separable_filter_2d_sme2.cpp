// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/filters/separable_filter_2d.h"

namespace kleidicv::sme2 {

KLEIDICV_LOCALLY_STREAMING KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t
separable_filter_2d_u8(const uint8_t *, size_t, uint8_t *, size_t, size_t,
                       size_t, size_t, const uint8_t *, size_t, const uint8_t *,
                       size_t, kleidicv_border_type_t,
                       kleidicv_filter_context_t *) {
  return KLEIDICV_ERROR_NOT_IMPLEMENTED;
}

}  // namespace kleidicv::sme2
