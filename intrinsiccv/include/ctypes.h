// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

/// @file
/// @brief C-style enums and structs
///

#ifndef INTRINSICCV_CTYPES_H
#define INTRINSICCV_CTYPES_H

#ifdef __cplusplus
#include <cinttypes>
#include <cstddef>
#else  // __cplusplus
#include "inttypes.h"
#include "stddef.h"
#endif  // __cplusplus

#include <config.h>

/// IntrinsicCV error types
typedef enum INTRINSICCV_NODISCARD {
  /// Success.
  INTRINSICCV_OK = 0,
  /// Requested operation is not implemented.
  INTRINSICCV_ERROR_NOT_IMPLEMENTED,
  /// Null pointer was passed as an argument.
  INTRINSICCV_ERROR_NULL_POINTER,
  /// A value was encountered outside the representable range.
  INTRINSICCV_ERROR_RANGE,
  /// Could not allocate memory.
  INTRINSICCV_ERROR_ALLOCATION,
  /// A value did not meet alignment requirements.
  INTRINSICCV_ERROR_ALIGNMENT,
} intrinsiccv_error_t;

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

/// IntrinsicCV border types
typedef enum {
  /// The border is a constant value.
  INTRINSICCV_BORDER_TYPE_CONSTANT,
  /// The border is the value of the first/last element.
  INTRINSICCV_BORDER_TYPE_REPLICATE,
  /// The border is the mirrored value of the first/last elements.
  INTRINSICCV_BORDER_TYPE_REFLECT,
  /// The border simply acts as a "wrap around" to the beginning/end.
  INTRINSICCV_BORDER_TYPE_WRAP,
  /// Like INTRINSICCV_BORDER_TYPE_REFLECT, but the first/last elements are
  /// ignored.
  INTRINSICCV_BORDER_TYPE_REVERSE,
  /// The border is the "continuation" of the input rows. It is the caller's
  /// responsibility to provide the input data (and an appropriate stride value)
  /// in a way that the rows can be under and over read. E.g. can be used when
  /// executing an operation on a region of a picture.
  INTRINSICCV_BORDER_TYPE_TRANSPARENT,
  /// The border is a hard border, there are no additional values to use.
  INTRINSICCV_BORDER_TYPE_NONE,
} intrinsiccv_border_type_t;

typedef struct intrinsiccv_morphology_context_t_
    intrinsiccv_morphology_context_t;

typedef struct intrinsiccv_filter_context_t_ intrinsiccv_filter_context_t;

#endif  // INTRINSICCV_CTYPES_H
