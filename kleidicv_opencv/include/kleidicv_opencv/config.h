// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_OPENCV_CONFIG_H
#define KLEIDICV_OPENCV_CONFIG_H

#if KLEIDICV_TARGET_NEON
#define KLEIDICV_OPENCV_TARGET_NAMESPACE kleidicv_opencv::neon
#elif KLEIDICV_TARGET_SVE2
#define KLEIDICV_OPENCV_TARGET_NAMESPACE kleidicv_opencv::sve2
#elif KLEIDICV_TARGET_SME2
#define KLEIDICV_OPENCV_TARGET_NAMESPACE kleidicv_opencv::sme2
#endif

#endif  // KLEIDICV_OPENCV_CONFIG_H
