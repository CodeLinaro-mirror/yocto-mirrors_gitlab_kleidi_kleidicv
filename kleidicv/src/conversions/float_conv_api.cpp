// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/conversions/float_conversion.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"

namespace kleidicv {

namespace sve2 {

template <typename InputType, typename OutputType>
kleidicv_error_t float_conversion(const InputType* src, size_t src_stride,
                                  OutputType* dst, size_t dst_stride,
                                  size_t width, size_t height);

}  // namespace sve2

namespace sme2 {

template <typename InputType, typename OutputType>
kleidicv_error_t float_conversion(const InputType* src, size_t src_stride,
                                  OutputType* dst, size_t dst_stride,
                                  size_t width, size_t height);

}  // namespace sme2

#ifdef KLEIDICV_HAVE_SVE2
#define SVE2_FUNC_POINTER(itype, otype)                      \
  [[maybe_unused]] static auto sve2_func_##itype##_##otype = \
      kleidicv::sve2::float_conversion<itype, otype>;
#else
#define SVE2_FUNC_POINTER(itype, otype)
#endif  // KLEIDICV_HAVE_SVE2

#ifdef KLEIDICV_HAVE_SME2
#define SME2_FUNC_POINTER(itype, otype)     \
  static auto sme2_func_##itype##_##otype = \
      kleidicv::sme2::float_conversion<itype, otype>;
#else
#define SME2_FUNC_POINTER(itype, otype)
#endif  // KLEIDICV_HAVE_SME2

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
#define KLEIDICV_DEFINE_C_API(partialname, itype, otype)    \
  SVE2_FUNC_POINTER(itype, otype);                          \
  SME2_FUNC_POINTER(itype, otype);                          \
  KLEIDICV_MULTIVERSION_C_API(                              \
      kleidicv_##partialname, &kleidicv::neon::partialname, \
      KLEIDICV_SVE2_IMPL_IF(sve2_func_##itype##_##otype),   \
      sme2_func_##itype##_##otype)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

KLEIDICV_DEFINE_C_API(float_conversion_f32_s8, float, int8_t);
KLEIDICV_DEFINE_C_API(float_conversion_f32_u8, float, uint8_t);
KLEIDICV_DEFINE_C_API(float_conversion_s8_f32, int8_t, float);
KLEIDICV_DEFINE_C_API(float_conversion_u8_f32, uint8_t, float);

}  // namespace kleidicv
