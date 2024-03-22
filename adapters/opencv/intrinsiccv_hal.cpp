// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv_hal.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>

#include "intrinsiccv/intrinsiccv.h"
#include "opencv2/imgproc/hal/interface.h"

namespace intrinsiccv::hal {

static int convert_error(intrinsiccv_error_t e) {
  switch (e) {
    case INTRINSICCV_OK:
      return CV_HAL_ERROR_OK;
    case INTRINSICCV_ERROR_NOT_IMPLEMENTED:
      return CV_HAL_ERROR_NOT_IMPLEMENTED;
    // Even if IntrinsicCV returns this error it's possible that another
    // implementation could handle the misalignment.
    case INTRINSICCV_ERROR_ALIGNMENT:
      return CV_HAL_ERROR_NOT_IMPLEMENTED;
    default:
      return CV_HAL_ERROR_UNKNOWN;
  }
}

// Returns the size in bytes of an OpenCV type.
static size_t get_type_size(int depth) {
  switch (depth) {
    case CV_8U:
    case CV_8S:
      return 1;
    case CV_16U:
    case CV_16S:
      return 2;
    case CV_32S:
      return 4;
    default:
      return SIZE_MAX;
  }
}

// Note: 'dcn' is already accounted for in 'dst_step'.
int gray_to_bgr(const uchar *src_data, size_t src_step, uchar *dst_data,
                size_t dst_step, int width, int height, int depth, int dcn) {
  if (INTRINSICCV_UNLIKELY((dcn != 3) && (dcn != 4))) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (depth == CV_8U) {
    if (dcn == 3) {
      return convert_error(intrinsiccv_gray_to_rgb_u8(
          reinterpret_cast<const uint8_t *>(src_data), src_step,
          reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height));
    }
    return convert_error(intrinsiccv_gray_to_rgba_u8(
        reinterpret_cast<const uint8_t *>(src_data), src_step,
        reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height));
  }

  return CV_HAL_ERROR_NOT_IMPLEMENTED;
}

int bgr_to_bgr(const uchar *src_data, size_t src_step, uchar *dst_data,
               size_t dst_step, int width, int height, int depth, int scn,
               int dcn, bool swapBlue) {
  if (INTRINSICCV_UNLIKELY(((scn != 3) && (scn != 4)) ||
                           ((dcn != 3) && (dcn != 4)))) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (depth == CV_8U) {
    if (scn == 3 && dcn == 3) {
      if (swapBlue) {
        return convert_error(intrinsiccv_rgb_to_bgr_u8(
            reinterpret_cast<const uint8_t *>(src_data), src_step,
            reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height));
      }
      return convert_error(intrinsiccv_rgb_to_rgb_u8(
          reinterpret_cast<const uint8_t *>(src_data), src_step,
          reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height));
    }

    if (scn == 4 && dcn == 4) {
      if (swapBlue) {
        return convert_error(intrinsiccv_rgba_to_bgra_u8(
            reinterpret_cast<const uint8_t *>(src_data), src_step,
            reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height));
      }
      return convert_error(intrinsiccv_rgba_to_rgba_u8(
          reinterpret_cast<const uint8_t *>(src_data), src_step,
          reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height));
    }

    if (scn == 3 && dcn == 4) {
      if (swapBlue) {
        return convert_error(intrinsiccv_rgb_to_bgra_u8(
            reinterpret_cast<const uint8_t *>(src_data), src_step,
            reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height));
      }
      return convert_error(intrinsiccv_rgb_to_rgba_u8(
          reinterpret_cast<const uint8_t *>(src_data), src_step,
          reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height));
    }

    if (scn == 4 && dcn == 3) {
      if (swapBlue) {
        return convert_error(intrinsiccv_rgba_to_bgr_u8(
            reinterpret_cast<const uint8_t *>(src_data), src_step,
            reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height));
      }
      return convert_error(intrinsiccv_rgba_to_rgb_u8(
          reinterpret_cast<const uint8_t *>(src_data), src_step,
          reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height));
    }
  }

  return CV_HAL_ERROR_NOT_IMPLEMENTED;
}

int yuv_to_bgr(const uchar *src_data, size_t src_step, uchar *dst_data,
               size_t dst_step, int dst_width, int dst_height, int dcn,
               bool swapBlue, int uIdx) {
  const uchar *uv_data =
      reinterpret_cast<const uint8_t *>(src_data) + dst_height * src_step;
  return yuv_to_bgr_ex(src_data, src_step, uv_data, src_step, dst_data,
                       dst_step, dst_width, dst_height, dcn, swapBlue, uIdx);
}

int yuv_to_bgr_ex(const uchar *y_data, size_t y_step, const uchar *uv_data,
                  size_t uv_step, uchar *dst_data, size_t dst_step,
                  int dst_width, int dst_height, int dcn, bool swapBlue,
                  int uIdx) {
  const bool is_bgr = !swapBlue;
  const bool is_nv21 = (uIdx != 0);

  if (dcn == 3) {
    if (is_bgr) {
      return convert_error(intrinsiccv_yuv_sp_to_bgr_u8(
          reinterpret_cast<const uint8_t *>(y_data), y_step,
          reinterpret_cast<const uint8_t *>(uv_data), uv_step,
          reinterpret_cast<uint8_t *>(dst_data), dst_step, dst_width,
          dst_height, is_nv21));
    }
    return convert_error(intrinsiccv_yuv_sp_to_rgb_u8(
        reinterpret_cast<const uint8_t *>(y_data), y_step,
        reinterpret_cast<const uint8_t *>(uv_data), uv_step,
        reinterpret_cast<uint8_t *>(dst_data), dst_step, dst_width, dst_height,
        is_nv21));
  }

  if (dcn == 4) {
    if (is_bgr) {
      return convert_error(intrinsiccv_yuv_sp_to_bgra_u8(
          reinterpret_cast<const uint8_t *>(y_data), y_step,
          reinterpret_cast<const uint8_t *>(uv_data), uv_step,
          reinterpret_cast<uint8_t *>(dst_data), dst_step, dst_width,
          dst_height, is_nv21));
    }
    return convert_error(intrinsiccv_yuv_sp_to_rgba_u8(
        reinterpret_cast<const uint8_t *>(y_data), y_step,
        reinterpret_cast<const uint8_t *>(uv_data), uv_step,
        reinterpret_cast<uint8_t *>(dst_data), dst_step, dst_width, dst_height,
        is_nv21));
  }

  return CV_HAL_ERROR_NOT_IMPLEMENTED;
}

int threshold(const uchar *src_data, size_t src_step, uchar *dst_data,
              size_t dst_step, int width, int height, int depth, int cn,
              double thresh, double maxValue, int thresholdType) {
  if ((depth == CV_8U) && (thresholdType == 0 /* THRESH_BINARY */)) {
    size_t width_in_elements = width * cn;
    return convert_error(intrinsiccv_threshold_binary_u8(
        reinterpret_cast<const uint8_t *>(src_data), src_step,
        reinterpret_cast<uint8_t *>(dst_data), dst_step, width_in_elements,
        height, static_cast<uint8_t>(thresh), static_cast<uint8_t>(maxValue)));
  }

  return CV_HAL_ERROR_NOT_IMPLEMENTED;
}

// Converts an OpenCV border type to an IntrinsicCV border type.
static int from_opencv(int opencv_border_type,
                       intrinsiccv_border_type_t &border_type) {
  switch (opencv_border_type) {
    default:
      return 1;
    case CV_HAL_BORDER_CONSTANT:
      border_type = intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_CONSTANT;
      break;
    case CV_HAL_BORDER_REPLICATE:
      border_type =
          intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_REPLICATE;
      break;
    case CV_HAL_BORDER_REFLECT:
      border_type = intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_REFLECT;
      break;
    case CV_HAL_BORDER_REFLECT_101:
      border_type = intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_REVERSE;
      break;
    case CV_HAL_BORDER_WRAP:
      border_type = intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_WRAP;
      break;
    case CV_HAL_BORDER_TRANSPARENT:
      border_type =
          intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_TRANSPARENT;
      break;
    case CV_HAL_BORDER_ISOLATED:
      border_type = intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_NONE;
      break;
  }

  return 0;
}

int gaussian_blur(const uchar *src_data, size_t src_step, uchar *dst_data,
                  size_t dst_step, int width, int height, int depth, int cn,
                  size_t margin_left, size_t margin_top, size_t margin_right,
                  size_t margin_bottom, size_t ksize_width, size_t ksize_height,
                  double sigmaX, double sigmaY, int border_type) {
  if (src_data == dst_data) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if ((sigmaX != 0.0) || (sigmaY != 0.0)) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if ((margin_left != 0) || (margin_top != 0) || (margin_right != 0) ||
      (margin_bottom != 0)) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  switch (depth) {
    case CV_8U:
      break;

    default:
      return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  intrinsiccv_border_type_t intrinsiccv_border_type;
  if (from_opencv(border_type, intrinsiccv_border_type)) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  decltype(intrinsiccv_gaussian_blur_3x3_u8) impl{nullptr};
  if ((ksize_width == 3) && (ksize_height == 3) && (width >= 3) &&
      (height >= 3)) {
    impl = intrinsiccv_gaussian_blur_3x3_u8;
  } else if ((ksize_width == 5) && (ksize_height == 5) && (width >= 5) &&
             (height >= 5)) {
    impl = intrinsiccv_gaussian_blur_5x5_u8;
  } else {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  intrinsiccv_filter_context_t *context;
  size_t type_size = get_type_size(depth);
  if (type_size == SIZE_MAX) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }
  type_size *= 2; /* widening */

  intrinsiccv_rectangle_t image = {
      .width = static_cast<decltype(intrinsiccv_rectangle_t::width)>(width),
      .height = static_cast<decltype(intrinsiccv_rectangle_t::height)>(height)};
  if (intrinsiccv_error_t create_err =
          intrinsiccv_filter_create(&context, cn, type_size, image)) {
    return convert_error(create_err);
  }

  intrinsiccv_error_t blur_err =
      impl(reinterpret_cast<const uint8_t *>(src_data), src_step,
           reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height, cn,
           intrinsiccv_border_type, context);

  intrinsiccv_error_t release_err = intrinsiccv_filter_release(context);

  return convert_error(blur_err ? blur_err : release_err);
}

struct MorphologyParams {
  intrinsiccv_morphology_context_t *context;
  decltype(intrinsiccv_dilate_u8) impl;
};

int morphology_init(cvhalFilter2D **cvcontext, int operation, int src_type,
                    int dst_type, int max_width, int max_height,
                    int kernel_type, uchar *kernel_data, size_t kernel_step,
                    int kernel_width, int kernel_height, int anchor_x,
                    int anchor_y, int cvborder_type,
                    const double cvborder_values[4], int iterations,
                    bool allow_submatrix, bool allow_in_place) {
  // Some parameters are unused.
  (void)allow_in_place;

  if (src_type != dst_type) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (CV_MAT_DEPTH(src_type) != CV_8U) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (allow_submatrix) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  intrinsiccv_border_type_t border_type;
  if (from_opencv(cvborder_type, border_type)) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (border_type !=
          intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_CONSTANT &&
      border_type !=
          intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_REPLICATE) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  size_t kernel_width_sz = static_cast<size_t>(kernel_width);
  size_t kernel_height_sz = static_cast<size_t>(kernel_height);
  size_t kernel_area = kernel_width_sz * kernel_height_sz;

  switch (CV_MAT_DEPTH(kernel_type)) {
    case CV_8U: {
      size_t nonzero_count = 0;
      if (intrinsiccv_error_t err = intrinsiccv_count_nonzeros_u8(
              static_cast<uint8_t *>(kernel_data), kernel_step, kernel_width_sz,
              kernel_height_sz, &nonzero_count)) {
        return convert_error(err);
      }
      if (nonzero_count != kernel_area) {
        return CV_HAL_ERROR_NOT_IMPLEMENTED;
      }
      break;
    }
    default:
      return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  // Use std::unique_ptr<T> to make error returns safer.
  auto params = std::make_unique<MorphologyParams>();
  if (!params) {
    return CV_HAL_ERROR_UNKNOWN;
  }

  size_t channels = (src_type >> CV_CN_SHIFT) + 1;
  size_t type_size = get_type_size(CV_MAT_DEPTH(src_type));
  if (SIZE_MAX == type_size) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  intrinsiccv_border_values_t border_values = {};
  if (border_type ==
      intrinsiccv_border_type_t::INTRINSICCV_BORDER_TYPE_CONSTANT) {
    border_values.top = cvborder_values[0];
    border_values.left = cvborder_values[1];
    border_values.bottom = cvborder_values[2];
    border_values.right = cvborder_values[3];

    // In case of default border values for dilate() '0' should be used.
    auto border_max_value =
        std::numeric_limits<decltype(border_values.top)>::max();
    if ((operation == CV_HAL_MORPH_DILATE) &&
        (border_values.top == border_max_value) &&
        (border_values.left == border_max_value) &&
        (border_values.bottom == border_max_value) &&
        (border_values.right == border_max_value)) {
      border_values.top = 0;
      border_values.left = 0;
      border_values.bottom = 0;
      border_values.right = 0;
    }
  }

  switch (operation) {
    case CV_HAL_MORPH_DILATE:
      params->impl = intrinsiccv_dilate_u8;
      break;
    case CV_HAL_MORPH_ERODE:
      params->impl = intrinsiccv_erode_u8;
      break;
    default:
      return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (intrinsiccv_error_t err = intrinsiccv_morphology_create(
          &params->context,
          intrinsiccv_rectangle_t{static_cast<size_t>(kernel_width),
                                  static_cast<size_t>(kernel_height)},
          intrinsiccv_point_t{static_cast<size_t>(anchor_x),
                              static_cast<size_t>(anchor_y)},
          border_type, border_values, channels, iterations, type_size,
          intrinsiccv_rectangle_t{static_cast<size_t>(max_width),
                                  static_cast<size_t>(max_height)})) {
    return convert_error(err);
  }

  *cvcontext = reinterpret_cast<cvhalFilter2D *>(params.release());
  return CV_HAL_ERROR_OK;
}

int morphology_operation(cvhalFilter2D *cvcontext, uchar *src_data,
                         size_t src_step, uchar *dst_data, size_t dst_step,
                         int width, int height, int src_full_width,
                         int src_full_height, int src_roi_x, int src_roi_y,
                         int dst_full_width, int dst_full_height, int dst_roi_x,
                         int dst_roi_y) {
  // Some parameters are unused.
  (void)src_full_width;
  (void)src_full_height;
  (void)src_roi_x;
  (void)src_roi_y;
  (void)dst_full_width;
  (void)dst_full_height;
  (void)dst_roi_x;
  (void)dst_roi_y;

  auto params = reinterpret_cast<MorphologyParams *>(cvcontext);
  return convert_error(
      params->impl(reinterpret_cast<const uint8_t *>(src_data), src_step,
                   reinterpret_cast<uint8_t *>(dst_data), dst_step,
                   static_cast<size_t>(width), static_cast<size_t>(height),
                   params->context));
}

int morphology_free(cvhalFilter2D *cvcontext) {
  std::unique_ptr<MorphologyParams> params(
      reinterpret_cast<MorphologyParams *>(cvcontext));
  return convert_error(intrinsiccv_morphology_release(params->context));
}

int resize(int src_type, const uchar *src_data, size_t src_step, int src_width,
           int src_height, uchar *dst_data, size_t dst_step, int dst_width,
           int dst_height, double inv_scale_x, double inv_scale_y,
           int interpolation) {
  if (src_data == dst_data) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  size_t channels = (src_type >> CV_CN_SHIFT) + 1;
  if (channels != 1) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (CV_MAT_DEPTH(src_type) != CV_8U) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  switch (interpolation) {
    case CV_HAL_INTER_LINEAR:
      if ((inv_scale_x == 0 || inv_scale_x == 2) &&
          (inv_scale_y == 0 || inv_scale_y == 2)) {
        return convert_error(intrinsiccv_resize_linear_u8(
            src_data, src_step, src_width, src_height, dst_data, dst_step,
            dst_width, dst_height));
      }
      break;
  }
  return CV_HAL_ERROR_NOT_IMPLEMENTED;
}

int sobel(const uchar *src_data, size_t src_step, uchar *dst_data,
          size_t dst_step, int width, int height, int src_depth, int dst_depth,
          int cn, int margin_left, int margin_top, int margin_right,
          int margin_bottom, int dx, int dy, int ksize, double scale,
          double delta, int border_type) {
  if (src_data == dst_data) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (src_depth != CV_8U) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (dst_depth != CV_16S) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (ksize != 3) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (width < 3 || height < 3) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (scale != 1.0) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (delta != 0.0) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (border_type != CV_HAL_BORDER_REPLICATE) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (margin_left || margin_top || margin_right || margin_bottom) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (dx == 1 && dy == 0) {
    return convert_error(intrinsiccv_sobel_3x3_horizontal_s16_u8(
        src_data, src_step, reinterpret_cast<int16_t *>(dst_data), dst_step,
        width, height, cn));
  }

  if (dx == 0 && dy == 1) {
    return convert_error(intrinsiccv_sobel_3x3_vertical_s16_u8(
        src_data, src_step, reinterpret_cast<int16_t *>(dst_data), dst_step,
        width, height, cn));
  }

  return CV_HAL_ERROR_NOT_IMPLEMENTED;
}

#if INTRINSICCV_EXPERIMENTAL_FEATURE_CANNY
int canny(const uchar *src_data, size_t src_step, uchar *dst_data,
          size_t dst_step, int width, int height, int cn, double lowThreshold,
          double highThreshold, int ksize, bool L2gradient) {
  if (ksize != 3) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (width < 3 || height < 3) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (L2gradient != false) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (cn != 1) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  return convert_error(intrinsiccv_canny_u8(
      reinterpret_cast<const uint8_t *>(src_data), src_step,
      reinterpret_cast<uint8_t *>(dst_data), dst_step,
      static_cast<size_t>(width), static_cast<size_t>(height), lowThreshold,
      highThreshold));
}
#endif  // INTRINSICCV_EXPERIMENTAL_FEATURE_CANNY

int transpose(const uchar *src_data, size_t src_step, uchar *dst_data,
              size_t dst_step, int src_width, int src_height,
              int element_size) {
  if ((element_size != 1) && (element_size != 2)) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  // Inplace transpose is only implemented if width and height is equal
  if (src_data == dst_data && src_width != src_height) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  return convert_error(intrinsiccv_transpose(
      reinterpret_cast<const void *>(src_data), src_step,
      reinterpret_cast<void *>(dst_data), dst_step,
      static_cast<size_t>(src_width), static_cast<size_t>(src_height),
      static_cast<size_t>(element_size)));
}

template <typename T, typename FunctionType>
intrinsiccv_error_t call_min_max(FunctionType min_max_func,
                                 const uchar *src_data, size_t src_stride,
                                 int width, int height, double *min_value,
                                 double *max_value) {
  T tmp_min_value, tmp_max_value;
  T *p_min_value = min_value ? &tmp_min_value : nullptr;
  T *p_max_value = max_value ? &tmp_max_value : nullptr;
  intrinsiccv_error_t err =
      min_max_func(reinterpret_cast<const T *>(src_data), src_stride,
                   static_cast<size_t>(width), static_cast<size_t>(height),
                   p_min_value, p_max_value);
  if (min_value) {
    *min_value = static_cast<double>(tmp_min_value);
  }
  if (max_value) {
    *max_value = static_cast<double>(tmp_max_value);
  }
  return err;
}

template <typename T, typename FunctionType>
intrinsiccv_error_t call_min_max_loc(FunctionType min_max_loc_func,
                                     const uchar *src_data, size_t src_stride,
                                     int width, int height, double *min_value,
                                     double *max_value, int *min_index,
                                     int *max_index) {
  size_t tmp_min_offset, tmp_max_offset;
  size_t *p_min_offset = (min_value || min_index) ? &tmp_min_offset : nullptr;
  size_t *p_max_offset = (max_value || max_index) ? &tmp_max_offset : nullptr;

  intrinsiccv_error_t err =
      min_max_loc_func(reinterpret_cast<const T *>(src_data), src_stride,
                       static_cast<size_t>(width), static_cast<size_t>(height),
                       p_min_offset, p_max_offset);
  if (min_value) {
    *min_value = static_cast<double>(src_data[tmp_min_offset]);
  }
  if (max_value) {
    *max_value = static_cast<double>(src_data[tmp_max_offset]);
  }
  if (min_index) {
    /* row */ min_index[0] = tmp_min_offset / src_stride;
    /* col */ min_index[1] = (tmp_min_offset % src_stride) / sizeof(T);
  }
  if (max_index) {
    /* row */ max_index[0] = tmp_max_offset / src_stride;
    /* col */ max_index[1] = (tmp_max_offset % src_stride) / sizeof(T);
  }
  return err;
}

int min_max_idx(const uchar *src_data, size_t src_step, int width, int height,
                int depth, double *minVal, double *maxVal, int *minIdx,
                int *maxIdx, uchar *mask) {
  if (mask) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (minIdx || maxIdx) {
    if (depth == CV_8U) {
      return convert_error(call_min_max_loc<uint8_t>(
          intrinsiccv_min_max_loc_u8, src_data, src_step, width, height, minVal,
          maxVal, minIdx, maxIdx));
    }
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  switch (depth) {
    case CV_8S:
      return convert_error(call_min_max<int8_t>(intrinsiccv_min_max_s8,
                                                src_data, src_step, width,
                                                height, minVal, maxVal));
    case CV_8U:
      return convert_error(call_min_max<uint8_t>(intrinsiccv_min_max_u8,
                                                 src_data, src_step, width,
                                                 height, minVal, maxVal));
    case CV_16S:
      return convert_error(call_min_max<int16_t>(intrinsiccv_min_max_s16,
                                                 src_data, src_step, width,
                                                 height, minVal, maxVal));
    case CV_16U:
      return convert_error(call_min_max<uint16_t>(intrinsiccv_min_max_u16,
                                                  src_data, src_step, width,
                                                  height, minVal, maxVal));
    case CV_32S:
      return convert_error(call_min_max<int32_t>(intrinsiccv_min_max_s32,
                                                 src_data, src_step, width,
                                                 height, minVal, maxVal));
    default:
      return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }
}

int convertTo(const uchar *src_data, size_t src_step, int src_depth,
              uchar *dst_data, size_t dst_step, int dst_depth, int width,
              int height, double scale, double shift) {
  if (src_depth != dst_depth) {
    // type conversion
    if (scale == 1.0 && shift == 0.0) {
      // float32 to int8
      if (src_depth == CV_32F && dst_depth == CV_8S) {
        return convert_error(intrinsiccv_type_conversion_f32_s8(
            reinterpret_cast<const float *>(src_data), src_step,
            reinterpret_cast<int8_t *>(dst_data), dst_step, width, height));
      }
      // float32 to uint8
      if (src_depth == CV_32F && dst_depth == CV_8U) {
        return convert_error(intrinsiccv_type_conversion_f32_u8(
            reinterpret_cast<const float *>(src_data), src_step,
            reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height));
      }
    }
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  // no scaling, no advantage
  if (fabs(scale - 1.0) < std::numeric_limits<double>::epsilon() &&
      fabs(shift) < std::numeric_limits<double>::epsilon()) {
    return CV_HAL_ERROR_NOT_IMPLEMENTED;
  }

  if (src_depth == CV_8U) {
    return convert_error(intrinsiccv_scale_u8(
        reinterpret_cast<const uint8_t *>(src_data), src_step,
        reinterpret_cast<uint8_t *>(dst_data), dst_step, width, height,
        static_cast<float>(scale), static_cast<float>(shift)));
  }
  return CV_HAL_ERROR_NOT_IMPLEMENTED;
}

}  // namespace intrinsiccv::hal
