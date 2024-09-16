#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exu

# Ensure we're at the root of the repo.
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Run OpenCV conformity checks
TESTRESULT=0
export OPENCV_VERSION="4.10.0"

CLEAN="ON" \
  OPENCV_URL="/opt/opencv-${OPENCV_VERSION}.tar.gz" \
  LDFLAGS="--rtlib=compiler-rt -fuse-ld=lld" \
  ./scripts/run_opencv_conformity_checks.sh || TESTRESULT=1

# Build OpenCV test executables from already configured conformity check project
ninja -C build/conformity/opencv_kleidicv opencv_test_imgproc opencv_test_core

# Run a subset of the OpenCV test suite, requres opencv_extra for the test images
tar xf /opt/opencv-extra-${OPENCV_VERSION}.tar.gz -C build
mv build/opencv_extra-${OPENCV_VERSION} build/opencv_extra

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
  '*Imgproc_cvtColor_BE.COLOR_RGB2YUV'
  '*Imgproc_Threshold*'
  '*Imgproc_Morphology*'
  '*Imgproc_GaussianBlur*'
  '*Imgproc_Sobel*'
  '*Imgproc_Canny*'
  '*Imgproc_Resize*'
  '*Imgproc_Dilate*'
  '*Imgproc_Erode*'
  '*Imgproc_PyramidDown*'
)
IMGPROC_TEST_PATTERNS_STR="$(join_strings_with_colon "${IMGPROC_TEST_PATTERNS[*]}")"
../../../conformity/opencv_kleidicv/bin/opencv_test_imgproc \
  --gtest_filter="${IMGPROC_TEST_PATTERNS_STR}" || TESTRESULT=1

CORE_TEST_PATTERNS=(
  '*Core_AbsDiff*'
  '*Core_Add*'
  '*Core_And*'
  '*Core_Mul*'
  '*Core_Sub*'
  '*Core_Transpose*'
  '*Core_MinMaxLoc*'
  '*MinMaxLoc*'
  '*Core_ConvertScale*'
  '*Core_Exp*'
  '*Core_MinMaxIdx*'
  '*Core_minMaxIdx*'
  '*Core_Array*'
  'Compare*'
  '*Core_InRange/*'
)
CORE_TEST_PATTERNS_STR="$(join_strings_with_colon "${CORE_TEST_PATTERNS[*]}")"
../../../conformity/opencv_kleidicv/bin/opencv_test_core \
  --gtest_filter="${CORE_TEST_PATTERNS_STR}" || TESTRESULT=1

popd

exit $TESTRESULT
