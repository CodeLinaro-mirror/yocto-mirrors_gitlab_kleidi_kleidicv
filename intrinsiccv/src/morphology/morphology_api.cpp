// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "dispatch.h"
#include "intrinsiccv.h"
#include "morphology/workspace.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
void dilate(const T *src, size_t src_stride, T *dst, size_t dst_stride,
            size_t width, size_t height,
            const intrinsiccv_morphology_params_t *params);

template <typename T>
void erode(const T *src, size_t src_stride, T *dst, size_t dst_stride,
           size_t width, size_t height,
           const intrinsiccv_morphology_params_t *params);

}  // namespace neon

namespace sve2 {

template <typename T>
void dilate(const T *src, size_t src_stride, T *dst, size_t dst_stride,
            size_t width, size_t height,
            const intrinsiccv_morphology_params_t *params);

template <typename T>
void erode(const T *src, size_t src_stride, T *dst, size_t dst_stride,
           size_t width, size_t height,
           const intrinsiccv_morphology_params_t *params);

}  // namespace sve2

namespace sme2 {

template <typename T>
void dilate(const T *src, size_t src_stride, T *dst, size_t dst_stride,
            size_t width, size_t height,
            const intrinsiccv_morphology_params_t *params);

template <typename T>
void erode(const T *src, size_t src_stride, T *dst, size_t dst_stride,
           size_t width, size_t height,
           const intrinsiccv_morphology_params_t *params);

}  // namespace sme2

extern "C" {

intrinsiccv_morphology_params_t *INTRINSICCV_C_API(morphology_create)(
    intrinsiccv_morphology_params_t *params, intrinsiccv_rectangle_t image) {
  auto workspace =
      MorphologyWorkspace::create(Rectangle{image}, Rectangle{params->kernel},
                                  Margin{params->kernel, params->anchor},
                                  params->channels, params->type_size);
  if (!workspace) {
    return nullptr;
  }

  params->data = reinterpret_cast<void *>(workspace.release());
  return params;
}

void INTRINSICCV_C_API(morphology_release)(
    intrinsiccv_morphology_params_t *params) {
  if (!params->data) {
    return;
  }

  // Deliberately create and immediately destroy a unique_ptr to delete the
  // workspace.
  // NOLINTBEGIN(bugprone-unused-raii)
  MorphologyWorkspace::Pointer{
      reinterpret_cast<MorphologyWorkspace *>(params->data)};
  // NOLINTEND(bugprone-unused-raii)
  params->data = nullptr;
}

}  // extern "C"

#define INTRINSICCV_DEFINE_C_API(name, tname, type)                         \
  static IFuncImpls name##_impls_builder(void) {                            \
    IFuncImpls impls;                                                       \
    INTRINSICCV_ADD_NEON_IMPL(intrinsiccv::neon::tname<type>);              \
    INTRINSICCV_ADD_SVE2_IMPL_IF(intrinsiccv::sve2::tname<type>);           \
    INTRINSICCV_ADD_SME2_IMPL(intrinsiccv::sme2::tname<type>);              \
    return impls;                                                           \
  }                                                                         \
  INTRINSICCV_MULTIVERSION_C_API(                                           \
      name, name##_impls_builder, void, const type *src, size_t src_stride, \
      type *dst, size_t dst_stride, size_t width, size_t height,            \
      const intrinsiccv_morphology_params_t *)

INTRINSICCV_DEFINE_C_API(dilate_u8, dilate, uint8_t);
INTRINSICCV_DEFINE_C_API(erode_u8, erode, uint8_t);

}  // namespace intrinsiccv
