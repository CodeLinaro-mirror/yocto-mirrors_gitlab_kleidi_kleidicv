#!/usr/bin/env bash
#
# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exu

# Ensure we're at the root of the repo.
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Ensure we're doing a clean build
rm -rf build/*opencv*

# Check building OpenCV with IntrinsicCV
OPENCV_PATCH_VER="4.9"
OPENCV_VER="${OPENCV_PATCH_VER}.0"
wget --no-verbose \
  https://github.com/opencv/opencv/archive/refs/tags/${OPENCV_VER}.tar.gz \
  -O build/opencv.tar.gz
tar xf build/opencv.tar.gz -C build
rm build/opencv.tar.gz
mv build/opencv-${OPENCV_VER} build/opencv
patch -d build/opencv -p1<adapters/opencv/opencv-${OPENCV_PATCH_VER}.patch
cmake \
  -S build/opencv \
  -B build/build-opencv \
  -G Ninja \
  -DWITH_INTRINSICCV=ON \
  -DINTRINSICCV_SOURCE_PATH="$(pwd)" \
  -DBUILD_LIST=core,imgproc
ninja -C build/build-opencv
