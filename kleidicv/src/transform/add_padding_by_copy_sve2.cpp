// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "add_padding_by_copy_sc.h"
#include "kleidicv/transform/add_padding_by_copy.h"

namespace kleidicv::sve2 {

KLEIDICV_TARGET_FN_ATTRS AddPaddingByCopyOpPointer
create_add_padding_by_copy_operation(AddPaddingByCopyParameters parameters,
                                     AddPaddingByCopyBorderStrategy strategy) {
  return create_add_padding_by_copy_operation_sc(parameters, strategy);
}

}  // namespace kleidicv::sve2
