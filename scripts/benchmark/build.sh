#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exu

if [[ -z "${KLEIDICV_PATH}" ]]; then
  echo "Please specify the local path to a checked out (cloned) KleidiCV repo in the KLEIDICV_PATH env variable"
  exit 1
fi

if [[ -z "${OPENCV_PATH}" ]]; then
  echo "Please specify the local path to a checked out (cloned) OpenCV repo in the OPENCV_PATH env variable"
  exit 1
fi

if [ ! -f "${NDK_TOOLCHAIN_FILE:-}" ]; then
  echo "Please specify the path of the Android NDK toolchain file (e.g. android-ndk-r26d/build/cmake/android.toolchain.cmake) in the NDK_TOOLCHAIN_FILE env variable"
  exit 1
fi

OPENCV_PATCH=$(realpath "${KLEIDICV_PATH}")/adapters/opencv/opencv-4.9.patch
OPENCV_BENCHMARK_PATCH=$(realpath "${KLEIDICV_PATH}")/adapters/opencv/extra_benchmarks/opencv-4.9.patch

pushd ${OPENCV_PATH}
if [ "patch --forward -p1<${OPENCV_PATCH}" -gt 0 ]; then
  echo patch failed!
  exit 2
fi

if [ "patch --forward -p1<${OPENCV_BENCHMARK_PATCH}" -gt 0 ]; then
  echo patch failed!
  exit 2
fi
popd

cmake -S ${OPENCV_PATH} \
  -B build/vanilla \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=14 \
  -DBUILD_ANDROID_EXAMPLES=OFF \
  -DBUILD_TESTS=OFF \
  -DBUILD_PERF_TESTS=ON \
  -DOPENCV_DISABLE_THREAD_SUPPORT=ON \
  -DCMAKE_TOOLCHAIN_FILE=$(readlink -f "${NDK_TOOLCHAIN_FILE}") \
  -DANDROID_ABI=arm64-v8a \
  -DBENCHMARK_DOWNLOAD_DEPENDENCIES=ON \
  -DWITH_KLEIDICV=OFF

ninja -C build/vanilla opencv_perf_imgproc opencv_perf_core

cmake -S ${OPENCV_PATH} \
  -B build/kleidicv \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=14 \
  -DBUILD_ANDROID_EXAMPLES=OFF \
  -DBUILD_TESTS=OFF \
  -DBUILD_PERF_TESTS=ON \
  -DOPENCV_DISABLE_THREAD_SUPPORT=ON \
  -DCMAKE_TOOLCHAIN_FILE=$(readlink -f "${NDK_TOOLCHAIN_FILE}") \
  -DANDROID_ABI=arm64-v8a \
  -DBENCHMARK_DOWNLOAD_DEPENDENCIES=ON \
  -DWITH_KLEIDICV=ON \
  -DKLEIDICV_SOURCE_PATH=$(realpath "${KLEIDICV_PATH}")

ninja -C build/kleidicv opencv_perf_imgproc opencv_perf_core

if [[ -z "${CUSTOM_CMAKE_OPTIONS}" ]]; then
  exit 0;
fi

cmake -S ${OPENCV_PATH} \
  -B build/kleidicv_custom \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=14 \
  -DBUILD_ANDROID_EXAMPLES=OFF \
  -DBUILD_TESTS=OFF \
  -DBUILD_PERF_TESTS=ON \
  -DOPENCV_DISABLE_THREAD_SUPPORT=ON \
  -DCMAKE_TOOLCHAIN_FILE=$(readlink -f "${NDK_TOOLCHAIN_FILE}") \
  -DANDROID_ABI=arm64-v8a \
  -DBENCHMARK_DOWNLOAD_DEPENDENCIES=ON \
  -DWITH_KLEIDICV=ON \
  -DKLEIDICV_SOURCE_PATH=$(realpath "${KLEIDICV_PATH}") \
  ${CUSTOM_CMAKE_OPTIONS}

ninja -C build/kleidicv_custom opencv_perf_imgproc opencv_perf_core
