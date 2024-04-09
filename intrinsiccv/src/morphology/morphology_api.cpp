// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/morphology/workspace.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
intrinsiccv_error_t dilate(const T *src, size_t src_stride, T *dst,
                           size_t dst_stride, size_t width, size_t height,
                           intrinsiccv_morphology_context_t *context);

template <typename T>
intrinsiccv_error_t erode(const T *src, size_t src_stride, T *dst,
                          size_t dst_stride, size_t width, size_t height,
                          intrinsiccv_morphology_context_t *context);

}  // namespace neon

namespace sve2 {

template <typename T>
intrinsiccv_error_t dilate(const T *src, size_t src_stride, T *dst,
                           size_t dst_stride, size_t width, size_t height,
                           intrinsiccv_morphology_context_t *context);

template <typename T>
intrinsiccv_error_t erode(const T *src, size_t src_stride, T *dst,
                          size_t dst_stride, size_t width, size_t height,
                          intrinsiccv_morphology_context_t *context);

}  // namespace sve2

namespace sme2 {

template <typename T>
intrinsiccv_error_t dilate(const T *src, size_t src_stride, T *dst,
                           size_t dst_stride, size_t width, size_t height,
                           intrinsiccv_morphology_context_t *context);

template <typename T>
intrinsiccv_error_t erode(const T *src, size_t src_stride, T *dst,
                          size_t dst_stride, size_t width, size_t height,
                          intrinsiccv_morphology_context_t *context);

}  // namespace sme2

}  // namespace intrinsiccv

extern "C" {

using INTRINSICCV_TARGET_NAMESPACE::MorphologyWorkspace;

intrinsiccv_error_t intrinsiccv_morphology_create(
    intrinsiccv_morphology_context_t **context, intrinsiccv_rectangle_t kernel,
    intrinsiccv_point_t anchor, intrinsiccv_border_type_t border_type,
    intrinsiccv_border_values_t border_values, size_t channels,
    size_t iterations, size_t type_size, intrinsiccv_rectangle_t image) {
  CHECK_POINTERS(context);
  *context = nullptr;
  CHECK_RECTANGLE_SIZE(kernel);
  CHECK_RECTANGLE_SIZE(image);

  if (type_size > INTRINSICCV_MAXIMUM_TYPE_SIZE) {
    return INTRINSICCV_ERROR_RANGE;
  }

  if (channels > INTRINSICCV_MAXIMUM_CHANNEL_COUNT) {
    return INTRINSICCV_ERROR_RANGE;
  }

  auto morphology_border_type =
      MorphologyWorkspace::get_border_type(border_type);

  if (!morphology_border_type) {
    return INTRINSICCV_ERROR_NOT_IMPLEMENTED;
  }

  MorphologyWorkspace::Pointer workspace;
  if (intrinsiccv_error_t error = MorphologyWorkspace::create(
          workspace, kernel, anchor, *morphology_border_type, border_values,
          channels, iterations, type_size, image)) {
    return error;
  }

  *context =
      reinterpret_cast<intrinsiccv_morphology_context_t *>(workspace.release());
  return INTRINSICCV_OK;
}

intrinsiccv_error_t intrinsiccv_morphology_release(
    intrinsiccv_morphology_context_t *context) {
  CHECK_POINTERS(context);

  // Deliberately create and immediately destroy a unique_ptr to delete the
  // workspace.
  // NOLINTBEGIN(bugprone-unused-raii)
  MorphologyWorkspace::Pointer{
      reinterpret_cast<MorphologyWorkspace *>(context)};
  // NOLINTEND(bugprone-unused-raii)
  return INTRINSICCV_OK;
}

}  // extern "C"

#define INTRINSICCV_DEFINE_C_API(name, tname, type)              \
  INTRINSICCV_MULTIVERSION_C_API(                                \
      name, &intrinsiccv::neon::tname<type>,                     \
      INTRINSICCV_SVE2_IMPL_IF(&intrinsiccv::sve2::tname<type>), \
      &intrinsiccv::sme2::tname<type>)

INTRINSICCV_DEFINE_C_API(intrinsiccv_dilate_u8, dilate, uint8_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_erode_u8, erode, uint8_t);
