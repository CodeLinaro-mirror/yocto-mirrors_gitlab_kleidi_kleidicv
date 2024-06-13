#!/bin/sh

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -eu

DEV_DIR=/data/local/tmp
CPU=7
THERMAL=0
CUSTOM_BUILD_SUFFIX="${CUSTOM_BUILD_SUFFIX:-custom}"

RES=$(printf "Operation\tOpenCV\tstd\tKleidiCV\tstd\tKleidiCV_$CUSTOM_BUILD_SUFFIX\tstd")

RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL GRAY2BGR opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, COLOR_GRAY2BGR)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL GRAY2BGRA opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, COLOR_GRAY2BGRA)')")

RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL BGR2RGB opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, COLOR_BGR2RGB)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL BGRA2RGBA opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, COLOR_BGRA2RGBA)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL BGR2RGBA opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, COLOR_BGR2RGBA)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL BGR2BGRA opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, COLOR_BGR2BGRA)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL RGBA2BGR opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, COLOR_RGBA2BGR)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL BGRA2BGR opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, COLOR_BGRA2BGR)')")

RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL YUVSP2BGR opencv_perf_imgproc '*CvtMode2*' '(3840x2160, COLOR_YUV2BGR_NV12)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL YUVSP2BGRA opencv_perf_imgproc '*CvtMode2*' '(3840x2160, COLOR_YUV2BGRA_NV12)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL YUVSP2RGB opencv_perf_imgproc '*CvtMode2*' '(3840x2160, COLOR_YUV2RGB_NV12)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL YUVSP2RGBA opencv_perf_imgproc '*CvtMode2*' '(3840x2160, COLOR_YUV2RGBA_NV12)')")

RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL RGB2YUV opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, COLOR_RGB2YUV)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL BGR2YUV opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, COLOR_BGR2YUV)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL RGBA2YUV opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, CX_RGBA2YUV)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL BGRA2YUV opencv_perf_imgproc '*cvtColor8u*' '(3840x2160, CX_BGRA2YUV)')")

RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL BinaryThreshold opencv_perf_imgproc '*ThreshFixture_Threshold*' '(3840x2160, 8UC1, THRESH_BINARY)')")

RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL GaussianBlur3x3 opencv_perf_imgproc '*gaussianBlur3x3*' '(3840x2160, 8UC1, BORDER_REPLICATE)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL GaussianBlur5x5 opencv_perf_imgproc '*gaussianBlur5x5*' '(3840x2160, 8UC1, BORDER_REPLICATE)')")

RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Sobel_Gx opencv_perf_imgproc '*Border3x3_sobelFilter*' '(3840x2160, 16SC1, (1, 0), BORDER_REPLICATE)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Sobel_Gy opencv_perf_imgproc '*Border3x3_sobelFilter*' '(3840x2160, 16SC1, (0, 1), BORDER_REPLICATE)')")

RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Dilate3x3 opencv_perf_imgproc '*Dilate_big*' '(3840x2160, 8UC1, 3)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Dilate5x5 opencv_perf_imgproc '*Dilate_big*' '(3840x2160, 8UC1, 5)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Dilate17x17 opencv_perf_imgproc '*Dilate_big*' '(3840x2160, 8UC1, 17)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Erode3x3 opencv_perf_imgproc '*Erode_big*' '(3840x2160, 8UC1, 3)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Erode5x5 opencv_perf_imgproc '*Erode_big*' '(3840x2160, 8UC1, 5)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Erode17x17 opencv_perf_imgproc '*Erode_big*' '(3840x2160, 8UC1, 17)')")

RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Resize2x2_8b opencv_perf_imgproc '*resizeUpLinearNonExact*' '(8UC1, (1920x1080, 3840x2160))')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Resize2x2_float opencv_perf_imgproc '*resizeUpLinearNonExact*' '(32FC1, (1920x1080, 3840x2160))')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Resize4x4_8b opencv_perf_imgproc '*resizeUpLinearNonExact*' '(8UC1, (960x540, 3840x2160))')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Resize4x4_float opencv_perf_imgproc '*resizeUpLinearNonExact*' '(32FC1, (960x540, 3840x2160))')")

RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Scale opencv_perf_core '*convertTo*' '(3840x2160, 8UC1, 8UC1, 1, 1.234, 4.567)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Scale_float_1.0 opencv_perf_core '*convertTo*' '(3840x2160, 32FC1, 32FC1, 1, 1, 4.567)')")
RES+=$(printf "\n$(${DEV_DIR}/perf_test_op.sh $CUSTOM_BUILD_SUFFIX $CPU $THERMAL Scale_float opencv_perf_core '*convertTo*' '(3840x2160, 32FC1, 32FC1, 1, 1.234, 4.567)')")

RES=$(printf "${RES}\n$(${DEV_DIR}/perf_test_op.sh $CPU $THERMAL MinMax_S8_4K opencv_perf_core '*minMaxVals*' '(3840x2160, 8SC1)')")
RES=$(printf "${RES}\n$(${DEV_DIR}/perf_test_op.sh $CPU $THERMAL MinMax_U8_4K opencv_perf_core '*minMaxVals*' '(3840x2160, 8UC1)')")
RES=$(printf "${RES}\n$(${DEV_DIR}/perf_test_op.sh $CPU $THERMAL MinMax_S16_4K opencv_perf_core '*minMaxVals*' '(3840x2160, 16SC1)')")
RES=$(printf "${RES}\n$(${DEV_DIR}/perf_test_op.sh $CPU $THERMAL MinMax_U16_4K opencv_perf_core '*minMaxVals*' '(3840x2160, 16UC1)')")
RES=$(printf "${RES}\n$(${DEV_DIR}/perf_test_op.sh $CPU $THERMAL MinMax_S32_4K opencv_perf_core '*minMaxVals*' '(3840x2160, 32SC1)')")
RES=$(printf "${RES}\n$(${DEV_DIR}/perf_test_op.sh $CPU $THERMAL MinMax_F32_4K opencv_perf_core '*minMaxVals*' '(3840x2160, 32FC1)')")

echo "$RES"
