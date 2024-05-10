#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

# Environment variables used:
#   ADB:                adb executable. Must be set.
#   ANDROID_SERIAL:     Passed to adb it selects a device by its serial number. This is optional.
#   DEVICE_HOST:        Passed to adb it sets the host where the device is found. This is optional.
#   DEVICE_PORT:        Passed to adb it sets the port where the device is found. This is optional.

set -exu

run_adb_command() {
    local args=()
    if [[ -n "${DEVICE_HOST+x}" ]]; then
        args+=( "-H" "${DEVICE_HOST}" )
    fi
    if [[ -n "${DEVICE_PORT+x}" ]]; then
        args+=( "-P" "${DEVICE_PORT}" )
    fi
    if [[ -n "${ANDROID_SERIAL+x}" ]]; then
        args+=( "-s" "${ANDROID_SERIAL}" )
    fi

    "${ADB}" "${args[@]}" "$@"
}

if [[ -z "${ADB}" ]]; then
    echo "Required variable 'ADB' is not set. Please set it to point to adb command."
    exit 1
fi

SCRIPT_PATH="$(realpath "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")"

DEV_DIR=/data/local/tmp

run_adb_command push build/vanilla/bin/opencv_perf_core ${DEV_DIR}/opencv_perf_core_vanilla
run_adb_command push build/vanilla/bin/opencv_perf_imgproc ${DEV_DIR}/opencv_perf_imgproc_vanilla

run_adb_command push build/kleidicv/bin/opencv_perf_core ${DEV_DIR}/opencv_perf_core_kleidicv
run_adb_command push build/kleidicv/bin/opencv_perf_imgproc ${DEV_DIR}/opencv_perf_imgproc_kleidicv

if [ -f build/kleidicv_custom/bin/opencv_perf_core ] && [ -f build/kleidicv_custom/bin/opencv_perf_imgproc ]; then
    run_adb_command push build/kleidicv_custom/bin/opencv_perf_core ${DEV_DIR}/opencv_perf_core_custom
    run_adb_command push build/kleidicv_custom/bin/opencv_perf_imgproc ${DEV_DIR}/opencv_perf_imgproc_custom
fi

run_adb_command push ${SCRIPT_PATH}/perf_test_op.sh ${DEV_DIR}/
run_adb_command push ${SCRIPT_PATH}/run_benchmarks_FHD.sh ${DEV_DIR}/
run_adb_command push ${SCRIPT_PATH}/run_benchmarks_4K.sh ${DEV_DIR}/
