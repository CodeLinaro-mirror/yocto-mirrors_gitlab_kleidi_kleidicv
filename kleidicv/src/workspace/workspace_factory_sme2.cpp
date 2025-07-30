// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/filters/matmul_filter_checks.h"
#include "kleidicv/workspace/matmul.h"
#include "kleidicv/workspace/separable.h"

namespace kleidicv::sme2 {

KLEIDICV_TARGET_FN_ATTRS void *
create_separable_filter_workspace(size_t max_image_width,
                                  size_t max_image_height,
                                  size_t max_kernel_width,
                                  size_t max_kernel_height, size_t max_channels,
                                  size_t intermediate_size) {
  Rectangle max_rect(max_image_width, max_image_height);
  Rectangle max_kernel(max_kernel_width, max_kernel_height);
  SeparableFilterWorkspace::Pointer workspace;
  if (gaussian_blur_sme2_implementation_checks(max_kernel_width,
                                               max_kernel_height)) {
    MatmulBufferSizesPolicy policy(max_rect, max_kernel, max_channels);
    workspace = SeparableFilterWorkspace::create(max_rect, max_channels,
                                                 intermediate_size, policy);
  } else {
    workspace = SeparableFilterWorkspace::create(max_rect, max_channels,
                                                 intermediate_size);
  }

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

}  // namespace kleidicv::sme2
