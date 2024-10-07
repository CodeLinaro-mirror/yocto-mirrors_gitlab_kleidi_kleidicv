#!/bin/sh

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -eu

: "${DEV_DIR:=/data/local/tmp}"
CPU=7
THERMAL=0
CUSTOM_BUILD_SUFFIX="${CUSTOM_BUILD_SUFFIX:-custom}"

echo "Operation\tOpenCV\tstd\tKleidiCV\tstd\tKleidiCV_$CUSTOM_BUILD_SUFFIX\tstd"

benchmarks=(
    "GRAY2BGR:   opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_GRAY2BGR)'"
    "GRAY2BGRA:  opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_GRAY2BGRA)'"

    "BGR2RGB:    opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_BGR2RGB)'"
    "BGRA2RGBA:  opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_BGRA2RGBA)'"
    "BGR2RGBA:   opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_BGR2RGBA)'"
    "BGR2BGRA:   opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_BGR2BGRA)'"
    "RGBA2BGR:   opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_RGBA2BGR)'"
    "BGRA2BGR:   opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_BGRA2BGR)'"

    "YUVSP2BGR:  opencv_perf_imgproc '*cvtColorYUV420/*' '(1920x1080, COLOR_YUV2BGR_NV12)'"
    "YUVSP2BGRA: opencv_perf_imgproc '*cvtColorYUV420/*' '(1920x1080, COLOR_YUV2BGRA_NV12)'"
    "YUVSP2RGB:  opencv_perf_imgproc '*cvtColorYUV420/*' '(1920x1080, COLOR_YUV2RGB_NV12)'"
    "YUVSP2RGBA: opencv_perf_imgproc '*cvtColorYUV420/*' '(1920x1080, COLOR_YUV2RGBA_NV12)'"

    "RGB2YUV:    opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_RGB2YUV)'"
    "BGR2YUV:    opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_BGR2YUV)'"
    "RGBA2YUV:   opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, CX_RGBA2YUV)'"
    "BGRA2YUV:   opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, CX_BGRA2YUV)'"

    "YUV2RGB:    opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_YUV2RGB)'"
    "YUV2BGR:    opencv_perf_imgproc '*cvtColor8u/*'     '(1920x1080, COLOR_YUV2BGR)'"

    "BinaryThreshold: opencv_perf_imgproc '*ThreshFixture_Threshold.Threshold/*' '(1920x1080, 8UC1, THRESH_BINARY)'"

    "SepFilter2D_5x5_U8:  opencv_perf_imgproc '*KleidiCV_SepFilter2D.SepFilter2D/*' '(1920x1080, 8UC1, 5, BORDER_REPLICATE)'"
    "SepFilter2D_5x5_U16: opencv_perf_imgproc '*KleidiCV_SepFilter2D.SepFilter2D/*' '(1920x1080, 16UC1, 5, BORDER_REPLICATE)'"
    "SepFilter2D_5x5_S16: opencv_perf_imgproc '*KleidiCV_SepFilter2D.SepFilter2D/*' '(1920x1080, 16SC1, 5, BORDER_REPLICATE)'"

    "GaussianBlur3x3:   opencv_perf_imgproc '*gaussianBlur3x3/*'   '(1920x1080, 8UC1, BORDER_REPLICATE)'"
    "GaussianBlur5x5:   opencv_perf_imgproc '*gaussianBlur5x5/*'   '(1920x1080, 8UC1, BORDER_REPLICATE)'"
    "GaussianBlur7x7:   opencv_perf_imgproc '*gaussianBlur7x7/*'   '(1920x1080, 8UC1, BORDER_REPLICATE)'"
    "GaussianBlur15x15: opencv_perf_imgproc '*gaussianBlur15x15/*' '(1920x1080, 8UC1, BORDER_REPLICATE)'"

    "GaussianBlur3x3_CustomSigma:   opencv_perf_imgproc '*gaussianBlur3x3_CustomSigma/*'   '(1920x1080, 8UC1, BORDER_REPLICATE)'"
    "GaussianBlur5x5_CustomSigma:   opencv_perf_imgproc '*gaussianBlur5x5_CustomSigma/*'   '(1920x1080, 8UC1, BORDER_REPLICATE)'"
    "GaussianBlur7x7_CustomSigma:   opencv_perf_imgproc '*gaussianBlur7x7_CustomSigma/*'   '(1920x1080, 8UC1, BORDER_REPLICATE)'"
    "GaussianBlur15x15_CustomSigma: opencv_perf_imgproc '*gaussianBlur15x15_CustomSigma/*' '(1920x1080, 8UC1, BORDER_REPLICATE)'"

    "Sobel_Gx: opencv_perf_imgproc '*Border3x3_sobelFilter.sobelFilter/*' '(1920x1080, 16SC1, (1, 0), BORDER_REPLICATE)'"
    "Sobel_Gy: opencv_perf_imgproc '*Border3x3_sobelFilter.sobelFilter/*' '(1920x1080, 16SC1, (0, 1), BORDER_REPLICATE)'"

    "Dilate3x3:   opencv_perf_imgproc '*Dilate_big.big/*'  '(1920x1080, 8UC1, 3)'"
    "Dilate5x5:   opencv_perf_imgproc '*Dilate_big.big/*'  '(1920x1080, 8UC1, 5)'"
    "Dilate17x17: opencv_perf_imgproc '*Dilate_big.big/*'  '(1920x1080, 8UC1, 17)'"
    "Erode3x3:    opencv_perf_imgproc '*Erode_big.big/*'   '(1920x1080, 8UC1, 3)'"
    "Erode5x5:    opencv_perf_imgproc '*Erode_big.big/*'   '(1920x1080, 8UC1, 5)'"
    "Erode17x17:  opencv_perf_imgproc '*Erode_big.big/*'   '(1920x1080, 8UC1, 17)'"

    "Resize_0.5_8b:   opencv_perf_imgproc '*ResizeAreaFast/*' '(8UC1, 1920x1080, 2)'"
    "Resize2x2_8b:    opencv_perf_imgproc '*resizeUpLinearNonExact/*' '(8UC1,  (960x540, 1920x1080))'"
    "Resize2x2_float: opencv_perf_imgproc '*resizeUpLinearNonExact/*' '(32FC1, (960x540, 1920x1080))'"
    "Resize4x4_8b:    opencv_perf_imgproc '*resizeUpLinearNonExact/*' '(8UC1,  (480x270, 1920x1080))'"
    "Resize4x4_float: opencv_perf_imgproc '*resizeUpLinearNonExact/*' '(32FC1, (480x270, 1920x1080))'"
    "Resize8x8_float: opencv_perf_imgproc '*resizeUpLinearNonExact/*' '(32FC1, (240x135, 1920x1080))'"

    "Scale:           opencv_perf_core '*convertTo/*' '(1920x1080, 8UC1,  8UC1,  1, 1.234, 4.567)'"
    "Scale_float_1.0: opencv_perf_core '*convertTo/*' '(1920x1080, 32FC1, 32FC1, 1, 1,     4.567)'"
    "Scale_float:     opencv_perf_core '*convertTo/*' '(1920x1080, 32FC1, 32FC1, 1, 1.234, 4.567)'"

    "MinMax_S8:  opencv_perf_core '*minMaxVals/*' '(1920x1080, 8SC1)'"
    "MinMax_U8:  opencv_perf_core '*minMaxVals/*' '(1920x1080, 8UC1)'"
    "MinMax_S16: opencv_perf_core '*minMaxVals/*' '(1920x1080, 16SC1)'"
    "MinMax_U16: opencv_perf_core '*minMaxVals/*' '(1920x1080, 16UC1)'"
    "MinMax_S32: opencv_perf_core '*minMaxVals/*' '(1920x1080, 32SC1)'"
    "MinMax_F32: opencv_perf_core '*minMaxVals/*' '(1920x1080, 32FC1)'"

    "MinMaxLoc_U8: opencv_perf_core '*minMaxLoc/*' '(1920x1080, 8UC1)'"

    "FloatToInt:  opencv_perf_core '*convertTo/*' '(1920x1080, 32FC1, 8SC1,  1, 1, 0)'"
    "FloatToUint: opencv_perf_core '*convertTo/*' '(1920x1080, 32FC1, 8UC1,  1, 1, 0)'"
    "IntToFloat:  opencv_perf_core '*convertTo/*' '(1920x1080, 8SC1,  32FC1, 1, 1, 0)'"
    "UintToFloat: opencv_perf_core '*convertTo/*' '(1920x1080, 8UC1,  32FC1, 1, 1, 0)'"

    "CompareGt: opencv_perf_core '*compare/*' '(1920x1080, 8UC1, CMP_GT)'"

    "InRange_U8:  opencv_perf_core '*inRangeScalar/*' '(1920x1080,  8UC1, 1, 2)'"
    "InRange_F32: opencv_perf_core '*inRangeScalar/*' '(1920x1080, 32FC1, 1, 2)'"

    "Remap_S16_U8: opencv_perf_imgproc '*Remap/*' '(1920x1080, 8UC1, 16SC2, INTER_NEAREST, BORDER_REPLICATE)'"
    "Remap_S16Point5_U8: opencv_perf_imgproc '*Remap/*' '(1920x1080, 8UC1, 16SC2, INTER_LINEAR, BORDER_REPLICATE)'"

    "BlurAndDownsample: opencv_perf_imgproc '*pyrDown/*' '(1920x1080, 8UC1)'"
)

for idx in "${!benchmarks[@]}"; do
    benchmark="${benchmarks[$idx]}"
    >&2 echo
    >&2 echo "RUNNING [$((${idx} + 1))/${#benchmarks[@]}]:" ${benchmark%%:*} ${benchmark#*:}
    eval "${DEV_DIR}/perf_test_op.sh" "${CUSTOM_BUILD_SUFFIX}" "${CPU}" "${THERMAL}" ${benchmark%%:*} ${benchmark#*:}
done
