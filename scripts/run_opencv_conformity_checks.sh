#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exu

: "${CLEAN:=OFF}"
: "${OPENCV_VERSION:=}"
: "${OPENCV_URL:=}"

SCRIPT_PATH="$(realpath "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")"

SOURCE_PATH="${SCRIPT_PATH}/../conformity/opencv"
INTRINSICCV_SOURCE_PATH="${SCRIPT_PATH}/.."
BUILD_PATH="${SCRIPT_PATH}/../build/conformity"
OPENCV_DEFAULT_PATH="${BUILD_PATH}/opencv_default"
OPENCV_INTRINSICCV_PATH="${BUILD_PATH}/opencv_intrinsiccv"

if [[ "${CLEAN}" == "ON" ]]; then
    rm -rf "${BUILD_PATH}"
fi

common_cmake_args=(
    -Wno-dev
    -S "${SOURCE_PATH}"
    -G Ninja
)

if [[ -n "${OPENCV_VERSION}" ]]; then
    common_cmake_args+=(
        "-DOPENCV_VERSION=${OPENCV_VERSION}"
    )
fi

if [[ -n "${OPENCV_URL}" ]]; then
    common_cmake_args+=(
        "-DOPENCV_URL=${OPENCV_URL}"
    )
fi

cmake "${common_cmake_args[@]}" \
      -B "${OPENCV_DEFAULT_PATH}" \
      -DWITH_INTRINSICCV=OFF
ninja -C "${OPENCV_DEFAULT_PATH}" subordinate

cmake "${common_cmake_args[@]}" \
      -B "${OPENCV_INTRINSICCV_PATH}" \
      -DWITH_INTRINSICCV=ON \
      -DINTRINSICCV_SOURCE_PATH="${INTRINSICCV_SOURCE_PATH}" \
      -DINTRINSICCV_ENABLE_SVE2=ON \
      -DINTRINSICCV_ENABLE_SVE2_SELECTIVELY=OFF
ninja -C "${OPENCV_INTRINSICCV_PATH}" manager

qemu-aarch64 -cpu cortex-a35 "${OPENCV_INTRINSICCV_PATH}/bin/manager" "${OPENCV_DEFAULT_PATH}/bin/subordinate"
qemu-aarch64 -cpu max,sve128=on,sme=off "${OPENCV_INTRINSICCV_PATH}/bin/manager" "${OPENCV_DEFAULT_PATH}/bin/subordinate"
qemu-aarch64 -cpu max,sve128=on,sme512=on "${OPENCV_INTRINSICCV_PATH}/bin/manager" "${OPENCV_DEFAULT_PATH}/bin/subordinate"
