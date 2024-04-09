// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/types.h"

namespace intrinsiccv {

namespace neon {

template <typename InputType, typename OutputType>
intrinsiccv_error_t float_conversion(const InputType* src, size_t src_stride,
                                     OutputType* dst, size_t dst_stride,
                                     size_t width, size_t height);

}  // namespace neon

namespace sve2 {

template <typename InputType, typename OutputType>
intrinsiccv_error_t float_conversion(const InputType* src, size_t src_stride,
                                     OutputType* dst, size_t dst_stride,
                                     size_t width, size_t height);

}  // namespace sve2

namespace sme2 {

template <typename InputType, typename OutputType>
intrinsiccv_error_t float_conversion(const InputType* src, size_t src_stride,
                                     OutputType* dst, size_t dst_stride,
                                     size_t width, size_t height);

}  // namespace sme2

#ifdef INTRINSICCV_HAVE_SVE2
#define SVE2_FUNC_POINTER(name, itype, otype)                \
  [[maybe_unused]] static auto sve2_func_##itype##_##otype = \
      intrinsiccv::sve2::float_conversion<itype, otype>;
#else
#define SVE2_FUNC_POINTER(name, itype, otype)
#endif  // INTRINSICCV_HAVE_SVE2

#ifdef INTRINSICCV_HAVE_SME2
#define SME2_FUNC_POINTER(name, itype, otype) \
  static auto sme2_func_##itype##_##otype =   \
      intrinsiccv::sme2::float_conversion<itype, otype>;
#else
#define SME2_FUNC_POINTER(name, itype, otype)
#endif  // INTRINSICCV_HAVE_SME2

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
#define INTRINSICCV_DEFINE_C_API(name, itype, otype)         \
  static auto neon_func_##itype##_##otype =                  \
      intrinsiccv::neon::float_conversion<itype, otype>;     \
  SVE2_FUNC_POINTER(name, itype, otype);                     \
  SME2_FUNC_POINTER(name, itype, otype);                     \
  INTRINSICCV_MULTIVERSION_C_API(                            \
      name, neon_func_##itype##_##otype,                     \
      INTRINSICCV_SVE2_IMPL_IF(sve2_func_##itype##_##otype), \
      sme2_func_##itype##_##otype)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

INTRINSICCV_DEFINE_C_API(intrinsiccv_float_conversion_f32_s8, float, int8_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_float_conversion_f32_u8, float, uint8_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_float_conversion_s8_f32, int8_t, float);
INTRINSICCV_DEFINE_C_API(intrinsiccv_float_conversion_u8_f32, uint8_t, float);

}  // namespace intrinsiccv
