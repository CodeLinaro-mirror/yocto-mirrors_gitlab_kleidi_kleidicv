#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exu

SCRIPT_PATH="$(realpath "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")"

: "${BUILD_PATH:=${SCRIPT_PATH}/../build/conformity}"

OPENCV_DEFAULT_PATH="${BUILD_PATH}/opencv_default"
OPENCV_KLEIDICV_PATH="${BUILD_PATH}/opencv_kleidicv"

TESTRESULT=0
qemu-aarch64 -cpu cortex-a35 "${OPENCV_KLEIDICV_PATH}/bin/manager" "${OPENCV_DEFAULT_PATH}/bin/subordinate" || TESTRESULT=1
qemu-aarch64 -cpu max,sve128=on,sme=off "${OPENCV_KLEIDICV_PATH}/bin/manager" "${OPENCV_DEFAULT_PATH}/bin/subordinate" || TESTRESULT=1
qemu-aarch64 -cpu max,sve128=on,sme512=on "${OPENCV_KLEIDICV_PATH}/bin/manager" "${OPENCV_DEFAULT_PATH}/bin/subordinate" || TESTRESULT=1

exit $TESTRESULT
