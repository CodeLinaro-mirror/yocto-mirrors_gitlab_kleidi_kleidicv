// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/workspace/separable.h"
#include "kleidicv/workspace/workspace_factory.h"

namespace kleidicv::neon {

KLEIDICV_TARGET_FN_ATTRS void *create_separable_filter_workspace(
    size_t max_image_width, size_t max_image_height, size_t, size_t,
    size_t max_channels, size_t intermediate_size) {
  Rectangle max_rect(max_image_width, max_image_height);
  auto workspace = SeparableFilterWorkspace::create(max_rect, max_channels,
                                                    intermediate_size);

  return workspace.release();
}

KLEIDICV_TARGET_FN_ATTRS void release_separable_filter_workspace(
    void *workspace) {
  // Deliberately create and immediately destroy a unique_ptr to delete the
  // workspace.
  // NOLINTBEGIN(bugprone-unused-raii)
  auto ptr = SeparableFilterWorkspace::Pointer{
      reinterpret_cast<SeparableFilterWorkspace *>(workspace)};
  // NOLINTEND(bugprone-unused-raii)
}

}  // namespace kleidicv::neon
