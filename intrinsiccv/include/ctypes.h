// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_CTYPES_H
#define INTRINSICCV_CTYPES_H

#ifdef __cplusplus
#include <cinttypes>
#include <cstddef>
#else  // __cplusplus
#include "inttypes.h"
#include "stddef.h"
#endif  // __cplusplus

typedef struct {
  size_t x;
  size_t y;
} intrinsiccv_point_t;

typedef struct {
  size_t width;
  size_t height;
} intrinsiccv_rectangle_t;

typedef struct {
  double top;
  double left;
  double bottom;
  double right;
} intrinsiccv_border_values_t;

typedef enum {
  // The border is a constant value.
  INTRINSICCV_BORDER_TYPE_CONSTANT,
  // The border is the value of the first/last element.
  INTRINSICCV_BORDER_TYPE_REPLICATE,
  // The border is the mirrored value of the first/last elements.
  INTRINSICCV_BORDER_TYPE_REFLECT,
  // The border simply acts as a "wrap around" to the beginning/end.
  INTRINSICCV_BORDER_TYPE_WRAP,
  // Like INTRINSICCV_BORDER_TYPE_REFLECT, but the first/last elements are
  // ignored.
  INTRINSICCV_BORDER_TYPE_REVERSE,
  // The border is the "continuation" of the
  INTRINSICCV_BORDER_TYPE_TRANSPARENT,
  // The border is a hard border, there are no additional values to use.
  INTRINSICCV_BORDER_TYPE_NONE,
} intrinsiccv_border_type_t;

typedef struct {
  intrinsiccv_rectangle_t kernel;
  intrinsiccv_point_t anchor;
  intrinsiccv_border_type_t border_type;
  intrinsiccv_border_values_t border_values;
  size_t channels;
  size_t iterations;
  size_t type_size;
  void *impl;
  void *data;
} intrinsiccv_morphology_params_t;

typedef struct {
  size_t channels;
  size_t type_size;
  void *workspace;
} intrinsiccv_filter_params_t;

#endif  // INTRINSICCV_CTYPES_H
