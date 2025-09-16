// SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_WORKSPACE_WORKSPACE_FACTORY_H
#define KLEIDICV_WORKSPACE_WORKSPACE_FACTORY_H

namespace kleidicv {

namespace neon {
void *create_separable_filter_workspace(
    size_t max_image_width, size_t max_image_height, size_t max_kernel_width,
    size_t max_kernel_height, size_t max_channels, size_t intermediate_size);

void release_separable_filter_workspace(void *workspace);
}  // namespace neon

namespace sve2 {
void *create_separable_filter_workspace(
    size_t max_image_width, size_t max_image_height, size_t max_kernel_width,
    size_t max_kernel_height, size_t max_channels, size_t intermediate_size);
void release_separable_filter_workspace(void *workspace);

}  // namespace sve2

namespace sme {
void *create_separable_filter_workspace(
    size_t max_image_width, size_t max_image_height, size_t max_kernel_width,
    size_t max_kernel_height, size_t max_channels, size_t intermediate_size);
void release_separable_filter_workspace(void *workspace);

}  // namespace sme

namespace sme2 {
void *create_separable_filter_workspace(
    size_t max_image_width, size_t max_image_height, size_t max_kernel_width,
    size_t max_kernel_height, size_t max_channels, size_t intermediate_size);
void release_separable_filter_workspace(void *workspace);

}  // namespace sme2

}  // namespace kleidicv

#endif
