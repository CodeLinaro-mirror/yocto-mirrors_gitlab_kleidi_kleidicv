// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_WORKSPACE_BORDER_H
#define KLEIDICV_WORKSPACE_BORDER_H

#include <sys/types.h>

#include "border_types.h"
#include "kleidicv/kleidicv.h"

namespace KLEIDICV_TARGET_NAMESPACE {

template <size_t KernelSize>
class BorderInfo {
 public:
  BorderInfo(size_t height, kleidicv::FixedBorderType border_type)
      : height_(height), border_type_(border_type) {}

  size_t translate_index(ssize_t index) {
    if (index < 0) {
      switch (border_type_) {
        case kleidicv::FixedBorderType::REPLICATE:
          return 0;
        case kleidicv::FixedBorderType::REFLECT:
          return -index - 1;
        case kleidicv::FixedBorderType::WRAP:
          return height_ + index;
        case kleidicv::FixedBorderType::REVERSE:
          return -index;
      }
    } else if (static_cast<size_t>(index) >= height_) {
      switch (border_type_) {
        case kleidicv::FixedBorderType::REPLICATE:
          return height_ - 1;
        case kleidicv::FixedBorderType::REFLECT:
          return 2 * height_ - index - 1;
        case kleidicv::FixedBorderType::WRAP:
          return index - height_;
        case kleidicv::FixedBorderType::REVERSE:
          return 2 * height_ - index - 2;
      }
    } else {
      return index;
    }
  }

 private:
  static constexpr size_t kMargin = KernelSize / 2;

  size_t height_;
  kleidicv::FixedBorderType border_type_;
};

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif
