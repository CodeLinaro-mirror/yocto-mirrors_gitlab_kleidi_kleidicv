// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "utils.h"

#if MANAGER

std::array<cv::Size, 4> typical_test_sizes(int min_width, int min_height) {
  // Tests around the minimal width and height and some rudimentary sizes
  return std::array<cv::Size, 4>{{{min_width, min_height},
                                  {min_width + 1, min_height + 1},
                                  {64 * 4, min_height * 2},
                                  {(64 * 4) + 1, min_height * 2}}};
}

#endif
