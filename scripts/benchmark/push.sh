#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

# Environment variables used:
#   ADB:                adb executable. Must be set.
# Note:
#   Use standard ADB env vars (like ANDROID_SERIAL, ANDROID_ADB_SERVER_ADDRESS and
#   ANDROID_ADB_SERVER_PORT) to customize ADB calls.

set -exu

if [[ -z "${ADB:-}" ]]; then
  echo "Required variable 'ADB' is not set. Please set it to point to adb command."
  exit 1
fi

SCRIPT_PATH="$(realpath "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")"

DEV_DIR=/data/local/tmp

"${ADB}" push "${SCRIPT_PATH}"/build/vanilla/bin/opencv_perf_core ${DEV_DIR}/opencv_perf_core_vanilla
"${ADB}" push "${SCRIPT_PATH}"/build/vanilla/bin/opencv_perf_imgproc ${DEV_DIR}/opencv_perf_imgproc_vanilla

"${ADB}" push "${SCRIPT_PATH}"/build/kleidicv/bin/opencv_perf_core ${DEV_DIR}/opencv_perf_core_kleidicv
"${ADB}" push "${SCRIPT_PATH}"/build/kleidicv/bin/opencv_perf_imgproc ${DEV_DIR}/opencv_perf_imgproc_kleidicv

if [[ -f "${SCRIPT_PATH}"/build/kleidicv_custom/bin/opencv_perf_core && -f "${SCRIPT_PATH}"/build/kleidicv_custom/bin/opencv_perf_imgproc ]]; then
  "${ADB}" push "${SCRIPT_PATH}"/build/kleidicv_custom/bin/opencv_perf_core ${DEV_DIR}/opencv_perf_core_custom
  "${ADB}" push "${SCRIPT_PATH}"/build/kleidicv_custom/bin/opencv_perf_imgproc ${DEV_DIR}/opencv_perf_imgproc_custom
fi

"${ADB}" push "${SCRIPT_PATH}"/perf_test_op.sh ${DEV_DIR}/
"${ADB}" push "${SCRIPT_PATH}"/run_benchmarks_FHD.sh ${DEV_DIR}/
"${ADB}" push "${SCRIPT_PATH}"/run_benchmarks_4K.sh ${DEV_DIR}/
