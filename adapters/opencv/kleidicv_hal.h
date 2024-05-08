// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_OPENCV_HAL_H
#define KLEIDICV_OPENCV_HAL_H

#include <cmath>
#include <type_traits>

#include "kleidicv/kleidicv.h"
#include "opencv2/core/hal/interface.h"

// Forward declarations of OpenCV internals.
struct cvhalFilter2D;

// Implemented HAL interfaces.
namespace kleidicv {
namespace hal {

// Macros to shorten repeated code.
#define KLEIDICV_HAL_API(api) (kleidicv::hal::api)

int gray_to_bgr(const uchar *src_data, size_t src_step, uchar *dst_data,
                size_t dst_step, int width, int height, int depth, int dcn);

int bgr_to_bgr(const uchar *src_data, size_t src_step, uchar *dst_data,
               size_t dst_step, int width, int height, int depth, int scn,
               int dcn, bool swapBlue);

int yuv_to_bgr(const uchar *src_data, size_t src_step, uchar *dst_data,
               size_t dst_step, int dst_width, int dst_height, int dcn,
               bool swapBlue, int uIdx);

int yuv_to_bgr_ex(const uchar *y_data, size_t y_step, const uchar *uv_data,
                  size_t uv_step, uchar *dst_data, size_t dst_step,
                  int dst_width, int dst_height, int dcn, bool swapBlue,
                  int uIdx);

int bgr_to_yuv(const uchar *src_data, size_t src_step, uchar *dst_data,
               size_t dst_step, int width, int height, int depth, int scn,
               bool swapBlue, bool isCbCr);

int threshold(const uchar *src_data, size_t src_step, uchar *dst_data,
              size_t dst_step, int width, int height, int depth, int cn,
              double thresh, double maxValue, int thresholdType);

int gaussian_blur_binomial(const uchar *src_data, size_t src_step,
                           uchar *dst_data, size_t dst_step, int width,
                           int height, int depth, int cn, size_t margin_left,
                           size_t margin_top, size_t margin_right,
                           size_t margin_bottom, size_t kernel_size,
                           int border_type);

int morphology_init(cvhalFilter2D **context, int operation, int src_type,
                    int dst_type, int max_width, int max_height,
                    int kernel_type, uchar *kernel_data, size_t kernel_step,
                    int kernel_width, int kernel_height, int anchor_x,
                    int anchor_y, int border_type,
                    const double border_values[4], int iterations,
                    bool allow_submatrix, bool allow_in_place);

int morphology_operation(cvhalFilter2D *context, uchar *src_data,
                         size_t src_step, uchar *dst_data, size_t dst_step,
                         int width, int height, int src_full_width,
                         int src_full_height, int src_roi_x, int src_roi_y,
                         int dst_full_width, int dst_full_height, int dst_roi_x,
                         int dst_roi_y);

int morphology_free(cvhalFilter2D *context);

int resize(int src_type, const uchar *src_data, size_t src_step, int src_width,
           int src_height, uchar *dst_data, size_t dst_step, int dst_width,
           int dst_height, double inv_scale_x, double inv_scale_y,
           int interpolation);

int sobel(const uchar *src_data, size_t src_step, uchar *dst_data,
          size_t dst_step, int width, int height, int src_depth, int dst_depth,
          int cn, int margin_left, int margin_top, int margin_right,
          int margin_bottom, int dx, int dy, int ksize, double scale,
          double delta, int border_type);

#if KLEIDICV_EXPERIMENTAL_FEATURE_CANNY
int canny(const uchar *src_data, size_t src_step, uchar *dst_data,
          size_t dst_step, int width, int height, int cn, double lowThreshold,
          double highThreshold, int ksize, bool L2gradient);
#endif  // KLEIDICV_EXPERIMENTAL_FEATURE_CANNY

int transpose(const uchar *src_data, size_t src_step, uchar *dst_data,
              size_t dst_step, int src_width, int src_height, int element_size);

int min_max_idx(const uchar *src_data, size_t src_stride, int width, int height,
                int depth, double *min_value, double *max_value, int *min_index,
                int *max_index, uchar *mask);

int convertTo(const uchar *src_data, size_t src_step, int src_depth,
              uchar *dst_data, size_t dst_step, int dst_depth, int width,
              int height, double scale, double shift);

}  // namespace hal
}  // namespace kleidicv

// Other HAL implementations might require the cv namespace
namespace cv {

#define KLEIDICV_HAL_FALLBACK_FORWARD(kleidicv_impl, fallback_hal_impl, ...) \
  (KLEIDICV_HAL_API(kleidicv_impl)(__VA_ARGS__) == CV_HAL_ERROR_OK           \
       ? CV_HAL_ERROR_OK                                                     \
       : fallback_hal_impl(__VA_ARGS__))

#ifdef OPENCV_IMGPROC_HAL_REPLACEMENT_HPP

// gray_to_bgr
static inline int kleidicv_gray_to_bgr_with_fallback(
    const uchar *src_data, size_t src_step, uchar *dst_data, size_t dst_step,
    int width, int height, int depth, int dcn) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(gray_to_bgr, cv_hal_cvtGraytoBGR,
                                       src_data, src_step, dst_data, dst_step,
                                       width, height, depth, dcn);
}
#undef cv_hal_cvtGraytoBGR
#define cv_hal_cvtGraytoBGR kleidicv_gray_to_bgr_with_fallback

// bgr_to_bgr
static inline int kleidicv_bgr_to_bgr_with_fallback(
    const uchar *src_data, size_t src_step, uchar *dst_data, size_t dst_step,
    int width, int height, int depth, int scn, int dcn, bool swapBlue) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(bgr_to_bgr, cv_hal_cvtBGRtoBGR, src_data,
                                       src_step, dst_data, dst_step, width,
                                       height, depth, scn, dcn, swapBlue);
}
#undef cv_hal_cvtBGRtoBGR
#define cv_hal_cvtBGRtoBGR kleidicv_bgr_to_bgr_with_fallback

// yuv_to_bgr
static inline int kleidicv_yuv_to_bgr_with_fallback(
    const uchar *src_data, size_t src_step, uchar *dst_data, size_t dst_step,
    int dst_width, int dst_height, int dcn, bool swapBlue, int uIdx) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(
      yuv_to_bgr, cv_hal_cvtTwoPlaneYUVtoBGR, src_data, src_step, dst_data,
      dst_step, dst_width, dst_height, dcn, swapBlue, uIdx);
}
#undef cv_hal_cvtTwoPlaneYUVtoBGR
#define cv_hal_cvtTwoPlaneYUVtoBGR kleidicv_yuv_to_bgr_with_fallback

// yuv_to_bgr_ex
static inline int kleidicv_yuv_to_bgr_ex_with_fallback(
    const uchar *y_data, size_t y_step, const uchar *uv_data, size_t uv_step,
    uchar *dst_data, size_t dst_step, int dst_width, int dst_height, int dcn,
    bool swapBlue, int uIdx) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(
      yuv_to_bgr_ex, cv_hal_cvtTwoPlaneYUVtoBGREx, y_data, y_step, uv_data,
      uv_step, dst_data, dst_step, dst_width, dst_height, dcn, swapBlue, uIdx);
}
#undef cv_hal_cvtTwoPlaneYUVtoBGREx
#define cv_hal_cvtTwoPlaneYUVtoBGREx kleidicv_yuv_to_bgr_ex_with_fallback

// bgr_to_yuv
static inline int kleidicv_bgr_to_yuv_with_fallback(
    const uchar *src_data, size_t src_step, uchar *dst_data, size_t dst_step,
    int width, int height, int depth, int scn, bool swapBlue, bool isCbCr) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(bgr_to_yuv, cv_hal_cvtBGRtoYUV, src_data,
                                       src_step, dst_data, dst_step, width,
                                       height, depth, scn, swapBlue, isCbCr);
}
#undef cv_hal_cvtBGRtoYUV
#define cv_hal_cvtBGRtoYUV kleidicv_bgr_to_yuv_with_fallback

// threshold
static inline int kleidicv_threshold_with_fallback(
    const uchar *src_data, size_t src_step, uchar *dst_data, size_t dst_step,
    int width, int height, int depth, int cn, double thresh, double maxValue,
    int thresholdType) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(
      threshold, cv_hal_threshold, src_data, src_step, dst_data, dst_step,
      width, height, depth, cn, thresh, maxValue, thresholdType);
}
#undef cv_hal_threshold
#define cv_hal_threshold kleidicv_threshold_with_fallback

// gaussian_blur_binomial
static inline int kleidicv_gaussian_blur_binomial_with_fallback(
    const uchar *src_data, size_t src_step, uchar *dst_data, size_t dst_step,
    int width, int height, int depth, int cn, size_t margin_left,
    size_t margin_top, size_t margin_right, size_t margin_bottom,
    size_t kernel_size, int border_type) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(
      gaussian_blur_binomial, cv_hal_gaussianBlurBinomial, src_data, src_step,
      dst_data, dst_step, width, height, depth, cn, margin_left, margin_top,
      margin_right, margin_bottom, kernel_size, border_type);
}
#undef cv_hal_gaussianBlurBinomial
#define cv_hal_gaussianBlurBinomial \
  kleidicv_gaussian_blur_binomial_with_fallback

// morphology_init
static inline int kleidicv_morphology_init_with_fallback(
    cvhalFilter2D **context, int operation, int src_type, int dst_type,
    int max_width, int max_height, int kernel_type, uchar *kernel_data,
    size_t kernel_step, int kernel_width, int kernel_height, int anchor_x,
    int anchor_y, int border_type, const double border_values[4],
    int iterations, bool allow_submatrix, bool allow_in_place) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(
      morphology_init, cv_hal_morphInit, context, operation, src_type, dst_type,
      max_width, max_height, kernel_type, kernel_data, kernel_step,
      kernel_width, kernel_height, anchor_x, anchor_y, border_type,
      border_values, iterations, allow_submatrix, allow_in_place);
}
#undef cv_hal_morphInit
#define cv_hal_morphInit kleidicv_morphology_init_with_fallback

// morphology_operation
static inline int kleidicv_morphology_operation_with_fallback(
    cvhalFilter2D *context, uchar *src_data, size_t src_step, uchar *dst_data,
    size_t dst_step, int width, int height, int src_full_width,
    int src_full_height, int src_roi_x, int src_roi_y, int dst_full_width,
    int dst_full_height, int dst_roi_x, int dst_roi_y) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(
      morphology_operation, cv_hal_morph, context, src_data, src_step, dst_data,
      dst_step, width, height, src_full_width, src_full_height, src_roi_x,
      src_roi_y, dst_full_width, dst_full_height, dst_roi_x, dst_roi_y);
}
#undef cv_hal_morph
#define cv_hal_morph kleidicv_morphology_operation_with_fallback

// morphology_free
static inline int kleidicv_morphology_free_with_fallback(
    cvhalFilter2D *context) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(morphology_free, cv_hal_morphFree,
                                       context);
}
#undef cv_hal_morphFree
#define cv_hal_morphFree kleidicv_morphology_free_with_fallback

// resize
static inline int kleidicv_resize_with_fallback(
    int src_type, const uchar *src_data, size_t src_step, int src_width,
    int src_height, uchar *dst_data, size_t dst_step, int dst_width,
    int dst_height, double inv_scale_x, double inv_scale_y, int interpolation) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(
      resize, cv_hal_resize, src_type, src_data, src_step, src_width,
      src_height, dst_data, dst_step, dst_width, dst_height, inv_scale_x,
      inv_scale_y, interpolation);
}
#undef cv_hal_resize
#define cv_hal_resize kleidicv_resize_with_fallback

// sobel
static inline int kleidicv_sobel_with_fallback(
    const uchar *src_data, size_t src_step, uchar *dst_data, size_t dst_step,
    int width, int height, int src_depth, int dst_depth, int cn,
    int margin_left, int margin_top, int margin_right, int margin_bottom,
    int dx, int dy, int ksize, double scale, double delta, int border_type) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(
      sobel, cv_hal_sobel, src_data, src_step, dst_data, dst_step, width,
      height, src_depth, dst_depth, cn, margin_left, margin_top, margin_right,
      margin_bottom, dx, dy, ksize, scale, delta, border_type);
}
#undef cv_hal_sobel
#define cv_hal_sobel kleidicv_sobel_with_fallback

#if KLEIDICV_EXPERIMENTAL_FEATURE_CANNY
// canny
static inline int kleidicv_canny_with_fallback(
    const uchar *src_data, size_t src_step, uchar *dst_data, size_t dst_step,
    int width, int height, int cn, double lowThreshold, double highThreshold,
    int ksize, bool L2gradient) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(
      canny, cv_hal_canny, src_data, src_step, dst_data, dst_step, width,
      height, cn, lowThreshold, highThreshold, ksize, L2gradient);
}
#undef cv_hal_canny
#define cv_hal_canny kleidicv_canny_with_fallback
#endif  // KLEIDICV_EXPERIMENTAL_FEATURE_CANNY

#endif  // OPENCV_IMGPROC_HAL_REPLACEMENT_HPP

#ifdef OPENCV_CORE_HAL_REPLACEMENT_HPP

// transpose
static inline int kleidicv_transpose_with_fallback(
    const uchar *src_data, size_t src_step, uchar *dst_data, size_t dst_step,
    int src_width, int src_height, int element_size) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(transpose, cv_hal_transpose2d, src_data,
                                       src_step, dst_data, dst_step, src_width,
                                       src_height, element_size);
}
#undef cv_hal_transpose2d
#define cv_hal_transpose2d kleidicv_transpose_with_fallback

// cv_hal_minMaxIdx is unstable in OpenCV
// See https://github.com/opencv/opencv/issues/25540
#if 0
// min_max_idx
static inline int kleidicv_min_max_idx_with_fallback(
    const uchar *src_data, size_t src_stride, int width, int height, int depth,
    double *min_value, double *max_value, int *min_index, int *max_index,
    uchar *mask) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(
      min_max_idx, cv_hal_minMaxIdx, src_data, src_stride, width, height, depth,
      min_value, max_value, min_index, max_index, mask);
}
#undef cv_hal_minMaxIdx
#define cv_hal_minMaxIdx kleidicv_min_max_idx_with_fallback
#endif  // 0

#if defined(cv_hal_convertTo)
static inline int kleidicv_convertTo_with_fallback(
    const uchar *src_data, size_t src_step, int src_depth, uchar *dst_data,
    size_t dst_step, int dst_depth, int width, int height, double scale,
    double shift) {
  return KLEIDICV_HAL_FALLBACK_FORWARD(convertTo, cv_hal_convertTo, src_data,
                                       src_step, src_depth, dst_data, dst_step,
                                       dst_depth, width, height, scale, shift);
}
#undef cv_hal_convertTo
#define cv_hal_convertTo kleidicv_convertTo_with_fallback
#endif  // defined(cv_hal_convertTo)

#endif  // OPENCV_CORE_HAL_REPLACEMENT_HPP

// Remove no longer needed macro definitions.
#undef KLEIDICV_HAL_FALLBACK_FORWARD

}  // namespace cv

#endif  // KLEIDICV_OPENCV_HAL_H
