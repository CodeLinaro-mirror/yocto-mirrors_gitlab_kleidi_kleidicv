// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_DEVICE_CAPABILITIES_H
#define KLEIDICV_DEVICE_CAPABILITIES_H

#include "kleidicv/config.h"

#if KLEIDICV_ENABLE_SME2 || KLEIDICV_ENABLE_SME || KLEIDICV_ENABLE_SVE2

namespace kleidicv {

class DeviceCapabilities {
 public:
#if KLEIDICV_ENABLE_SVE2
  static const bool has_sve2;
#endif
#if KLEIDICV_ENABLE_SME
  static const bool has_sme;
#endif
#if KLEIDICV_ENABLE_SME2
  static const bool has_sme2;
#endif
};

}  // namespace kleidicv

#endif  // KLEIDICV_ENABLE_SME2 || KLEIDICV_ENABLE_SME ||  KLEIDICV_ENABLE_SVE2

#endif  // KLEIDICV_DEVICE_CAPABILITIES_H
