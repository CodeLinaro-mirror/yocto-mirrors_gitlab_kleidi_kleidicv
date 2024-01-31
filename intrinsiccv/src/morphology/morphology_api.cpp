// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "dispatch.h"
#include "intrinsiccv.h"
#include "morphology/workspace.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
intrinsiccv_error_t dilate(const T *src, size_t src_stride, T *dst,
                           size_t dst_stride, size_t width, size_t height,
                           const intrinsiccv_morphology_params_t *params);

template <typename T>
intrinsiccv_error_t erode(const T *src, size_t src_stride, T *dst,
                          size_t dst_stride, size_t width, size_t height,
                          const intrinsiccv_morphology_params_t *params);

}  // namespace neon

namespace sve2 {

template <typename T>
intrinsiccv_error_t dilate(const T *src, size_t src_stride, T *dst,
                           size_t dst_stride, size_t width, size_t height,
                           const intrinsiccv_morphology_params_t *params);

template <typename T>
intrinsiccv_error_t erode(const T *src, size_t src_stride, T *dst,
                          size_t dst_stride, size_t width, size_t height,
                          const intrinsiccv_morphology_params_t *params);

}  // namespace sve2

namespace sme2 {

template <typename T>
intrinsiccv_error_t dilate(const T *src, size_t src_stride, T *dst,
                           size_t dst_stride, size_t width, size_t height,
                           const intrinsiccv_morphology_params_t *params);

template <typename T>
intrinsiccv_error_t erode(const T *src, size_t src_stride, T *dst,
                          size_t dst_stride, size_t width, size_t height,
                          const intrinsiccv_morphology_params_t *params);

}  // namespace sme2

extern "C" {

intrinsiccv_error_t intrinsiccv_morphology_create(
    intrinsiccv_morphology_params_t *params, intrinsiccv_rectangle_t image) {
  auto workspace =
      MorphologyWorkspace::create(Rectangle{image}, Rectangle{params->kernel},
                                  Margin{params->kernel, params->anchor},
                                  params->channels, params->type_size);
  if (!workspace) {
    return INTRINSICCV_ERROR_ALLOCATION;
  }

  params->data = reinterpret_cast<void *>(workspace.release());
  return INTRINSICCV_OK;
}

intrinsiccv_error_t intrinsiccv_morphology_release(
    intrinsiccv_morphology_params_t *params) {
  if (!params->data) {
    return INTRINSICCV_ERROR_NULL_POINTER;
  }

  // Deliberately create and immediately destroy a unique_ptr to delete the
  // workspace.
  // NOLINTBEGIN(bugprone-unused-raii)
  MorphologyWorkspace::Pointer{
      reinterpret_cast<MorphologyWorkspace *>(params->data)};
  // NOLINTEND(bugprone-unused-raii)
  params->data = nullptr;
  return INTRINSICCV_OK;
}

}  // extern "C"

#define INTRINSICCV_DEFINE_C_API(name, tname, type)                       \
  INTRINSICCV_MULTIVERSION_C_API(                                         \
      name, intrinsiccv::neon::tname<type>,                               \
      INTRINSICCV_SVE2_IMPL_IF(intrinsiccv::sve2::tname<type>),           \
      intrinsiccv::sme2::tname<type>, const type *src, size_t src_stride, \
      type *dst, size_t dst_stride, size_t width, size_t height,          \
      const intrinsiccv_morphology_params_t *)

INTRINSICCV_DEFINE_C_API(intrinsiccv_dilate_u8, dilate, uint8_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_erode_u8, erode, uint8_t);

}  // namespace intrinsiccv
