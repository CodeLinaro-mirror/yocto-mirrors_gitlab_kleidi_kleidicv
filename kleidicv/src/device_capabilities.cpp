// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/device_capabilities.h"

#include "kleidicv/config.h"

#if KLEIDICV_ENABLE_SME2 || KLEIDICV_ENABLE_SME || KLEIDICV_ENABLE_SVE2

#include <sys/auxv.h>

#include <cstdint>

namespace kleidicv {

using HwCapTy = uint64_t;

struct HwCaps final {
  HwCapTy hwcap1;
  HwCapTy hwcap2;
};

static HwCaps get_hwcaps() {
  return HwCaps{getauxval(AT_HWCAP), getauxval(AT_HWCAP2)};
}
static const HwCaps hwcaps = get_hwcaps();

#if KLEIDICV_ENABLE_SVE2
static bool hwcaps_has_sve2(HwCaps hwcaps) { return hwcaps.hwcap2 & (1 << 1); }
const bool DeviceCapabilities::has_sve2 = hwcaps_has_sve2(hwcaps);
#endif

#if KLEIDICV_ENABLE_SME
static bool hwcaps_has_sme(HwCaps hwcaps) {
  const int kSMEBit = 23;
  return hwcaps.hwcap2 & (1UL << kSMEBit);
}
const bool DeviceCapabilities::has_sme = hwcaps_has_sme(hwcaps);
#endif  // KLEIDICV_ENABLE_SME

#if KLEIDICV_ENABLE_SME2
static bool hwcaps_has_sme2(HwCaps hwcaps) {
  const int kSME2Bit = 37;
  return hwcaps.hwcap2 & (1UL << kSME2Bit);
}
const bool DeviceCapabilities::has_sme2 = hwcaps_has_sme2(hwcaps);
#endif  // KLEIDICV_ENABLE_SME2

}  // namespace kleidicv

#endif  // KLEIDICV_ENABLE_SME2 || KLEIDICV_ENABLE_SME ||  KLEIDICV_ENABLE_SVE2
