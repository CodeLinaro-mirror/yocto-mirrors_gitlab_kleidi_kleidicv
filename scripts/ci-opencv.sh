#!/usr/bin/env bash

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
  -DBUILD_TESTS=ON \
  -DBUILD_LIST=core,imgproc,ts
ninja -C build/build-opencv

# Run a subset of the OpenCV test suite, requres opencv_extra for the test images
wget --no-verbose \
  https://github.com/opencv/opencv_extra/archive/refs/tags/${OPENCV_VER}.tar.gz \
  -O build/opencv_extra.tar.gz
tar xf build/opencv_extra.tar.gz -C build
rm build/opencv_extra.tar.gz
mv build/opencv_extra-${OPENCV_VER} build/opencv_extra

pushd build/opencv_extra/testdata/cv

join_strings_with_colon() {
    local array="${1}"
    # shellcheck disable=SC2068
    # Here array should be re-splitted.
    array="$(printf ":%s" ${array[@]})"
    echo "${array:1}"
}

IMGPROC_TEST_PATTERNS=(
    '*Imgproc_ColorGray*'
    '*Imgproc_ColorRGB*'
    '*Imgproc_ColorYUV*'
    '*Imgproc_cvtColor_BE.COLOR_YUV*'
    '*Imgproc_Threshold*'
    '*Imgproc_Morphology*'
    '*Imgproc_GaussianBlur*'
    '*Imgproc_Sobel*'
    '*Imgproc_Canny*'
)
IMGPROC_TEST_PATTERNS_STR="$(join_strings_with_colon "${IMGPROC_TEST_PATTERNS[*]}")"
../../../build-opencv/bin/opencv_test_imgproc --gtest_filter="${IMGPROC_TEST_PATTERNS_STR}"

CORE_TEST_PATTERNS=(
    '*Core_Transpose*'
    '*Core_MinMaxLoc*'
    '*Core_ConvertScale*'
)
CORE_TEST_PATTERNS_STR="$(join_strings_with_colon "${CORE_TEST_PATTERNS[*]}")"
../../../build-opencv/bin/opencv_test_core --gtest_filter="${CORE_TEST_PATTERNS_STR}"

popd
