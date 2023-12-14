// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_TEST_FRAMEWORK_ARRAY_H_
#define INTRINSICCV_TEST_FRAMEWORK_ARRAY_H_

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "framework/abstract.h"

namespace test {

/// A simple two-dimensional array representation.
template <typename ElementType>
class Array2D final : public TwoDimensional<ElementType> {
 public:
  Array2D() = default;

  explicit Array2D(size_t width, size_t height) : Array2D(width, height, 0) {}

  explicit Array2D(size_t width, size_t height, size_t padding)
      : Array2D(width, height, padding, 1) {}

  explicit Array2D(size_t width, size_t height, size_t padding, size_t channels)
      : width_{width},
        height_{height},
        channels_{channels},
        stride_{width * sizeof(ElementType) + padding} {
    try {
      data_ = std::make_unique<uint8_t[]>(height_ * stride_);
    } catch (...) {
      width_ = height_ = stride_ = 0;
      return;
    }

    fill_padding();
  }

  /// Destructor checks that padding bytes are not overwritten.
  ~Array2D() { check_padding(); }

  /// Copy constructor.
  Array2D(const Array2D &other) = delete;

  /// Move constructor.
  Array2D(Array2D &&other) = delete;

  /// Copy assignment operator.
  Array2D &operator=(const Array2D &other) = delete;

  /// Move assignment operator.
  Array2D &operator=(Array2D &&other) {
    data_ = std::move(other.data_);
    width_ = other.width_;
    height_ = other.height_;
    channels_ = other.channels_;
    stride_ = other.stride_;
    other.width_ = other.height_ = other.channels_ = other.stride_ = 0;
    return *this;
  }

  /// Fills the underlying memory range with a given value skipping padding
  /// bytes.
  void fill(ElementType value) {
    ElementType *ptr = data();
    for (size_t row = 0; row < height(); ++row) {
      for (size_t column = 0; column < width(); ++column) {
        ptr[column] = value;
      }

      ptr = add_stride(ptr, 1);
    }
  }

  /// Sets values starting in a given row starting at a given column. Returns
  /// false if there is not enough space in the row.
  void set(size_t row, size_t column,
           std::initializer_list<ElementType> values) {
    ElementType *ptr = at(row, column);
    if (!ptr) {
      return;
    }

    size_t index = 0;
    for (ElementType value : values) {
      ptr[index++] = value;
    }
  }

  /// Compares two instances for equality considering only element bytes.
  /// Returns the location of the first mismatch, if any.
  std::optional<std::tuple<size_t, size_t>> compare_to(
      const Array2D<ElementType> &other) const {
    for (size_t row = 0; row < height(); ++row) {
      for (size_t column = 0; column < width(); ++column) {
        const ElementType *lhs = at(row, column);
        const ElementType *rhs = other.at(row, column);
        if (!lhs || !rhs || lhs[0] != rhs[0]) {
          return std::make_tuple(row, column);
        }
      }
    }

    return std::nullopt;
  }

  /// Returns a pointer to the first element.
  ElementType *data() { return reinterpret_cast<ElementType *>(data_.get()); }

  /// Returns a const pointer to the first element.
  const ElementType *data() const {
    return reinterpret_cast<const ElementType *>(data_.get());
  }

  /// Returns the width of this array.
  size_t width() const override { return width_; }

  /// Returns the height of this array.
  size_t height() const override { return height_; }

  /// Returns the number of channels.
  size_t channels() const override { return channels_; };

  /// Returns the stride of this array.
  size_t stride() const { return stride_; }

  /// Returns true if this object hold actual memory, otherwise false.
  bool valid() const { return data() != nullptr; }

  /// Returns a pointer to a data element at a given row and column position, or
  /// nullptr if the requested position is invalid.
  ElementType *at(size_t row, size_t column) override {
    return const_cast<ElementType *>(
        const_cast<const Array2D<ElementType> *>(this)->at(row, column));
  }

  /// Returns a constant pointer to a data element at a given row and column
  /// position, or nullptr if the requested position is invalid.
  const ElementType *at(size_t row, size_t column) const override {
    if (!check_access(row, column)) {
      return nullptr;
    }

    const ElementType *ptr = add_stride(data(), row);
    return &ptr[column];
  }

 private:
  /// Returns the offset to the padding within a row.
  size_t padding_offset() const { return width() * sizeof(ElementType); }

  /// Returns true if a row has padding, otherwise false.
  size_t has_padding() const { return padding_offset() != stride(); }

  /// Checks that an access is valid or not.
  bool check_access(size_t row, size_t column) const {
    return valid() && (row < height()) && (column < width());
  }

  /// Fills padding bytes, if present.
  void fill_padding() {
    if (!valid() || !has_padding()) {
      return;
    }

    uint8_t *ptr = data_.get();
    for (size_t row = 0; row < height(); ++row) {
      for (size_t column = padding_offset(); column < stride(); ++column) {
        ptr[column] = kPaddingValue;
      }

      ptr += stride();
    }
  }

  /// Checks for clobbered padding, if present.
  void check_padding() const {
    if (!valid() || !has_padding()) {
      return;
    }

    const uint8_t *ptr = data_.get();
    for (size_t row = 0; row < height(); ++row) {
      for (size_t offset = padding_offset(); offset < stride(); ++offset) {
        if (ptr[offset] != kPaddingValue) {
          FAIL() << "Padding byte was overwritten at (row=" << row
                 << ", offset=" << offset << ")" << std::endl;
          return;
        }
      }

      ptr += stride();
    }
  }

  /// Adds stride to a pointer.
  ElementType *add_stride(ElementType *ptr, size_t count) {
    size_t address = reinterpret_cast<size_t>(ptr);
    address += count * stride();
    return reinterpret_cast<ElementType *>(address);
  }

  /// Adds stride to a pointer.
  const ElementType *add_stride(const ElementType *ptr, size_t count) const {
    size_t address = reinterpret_cast<size_t>(ptr);
    address += count * stride();
    return reinterpret_cast<const ElementType *>(address);
  }

  /// Constant value of row padding bytes.
  static constexpr uint8_t kPaddingValue = std::numeric_limits<uint8_t>::max();

  /// Smart pointer to the managed memory.
  std::unique_ptr<uint8_t[]> data_;
  /// Width a row in the array.
  size_t width_{0};
  /// Number of rows in the array.
  size_t height_{0};
  /// Number of channels.
  size_t channels_{0};
  /// Stride in bytes between the first elements of two consecutive rows.
  size_t stride_{0};
};  // end of class Array2D<ElementType>

/// Compares two \ref Array2D objects for equality.
#define EXPECT_EQ_ARRAY2D(__lhs, __rhs)                       \
  do {                                                        \
    ASSERT_EQ((__lhs).width(), (__rhs).width())               \
        << "Mismatch in width." << std::endl;                 \
    ASSERT_EQ((__lhs).height(), (__rhs).height())             \
        << "Mismatch in height." << std::endl;                \
    ASSERT_EQ((__lhs).channels(), (__rhs).channels())         \
        << "Mismatch in channels." << std::endl;              \
    auto mismatch = (__lhs).compare_to((__rhs));              \
    if (mismatch) {                                           \
      auto [row, col] = *mismatch;                            \
      FAIL() << "Mismatch at (row=" << row << ", col=" << col \
             << "): " << (__lhs).at(row, col)[0] << " vs "    \
             << (__rhs).at(row, col)[0] << "." << std::endl;  \
    }                                                         \
  } while (0 != 0)

/// Compares two \ref Array2D objects for inequality.
#define EXPECT_NE_ARRAY2D(__lhs, __rhs)                                    \
  do {                                                                     \
    ASSERT_EQ((__lhs).width(), (__rhs).width())                            \
        << "Mismatch in width." << std::endl;                              \
    ASSERT_EQ((__lhs).height(), (__rhs).height())                          \
        << "Mismatch in height." << std::endl;                             \
    ASSERT_EQ((__lhs).channels(), (__rhs).channels())                      \
        << "Mismatch in channels." << std::endl;                           \
    auto mismatch = (__lhs).compare_to((__rhs));                           \
    if (!mismatch) {                                                       \
      FAIL() << "Objects are equal, but expected to differ." << std::endl; \
    }                                                                      \
  } while (0 != 0)

}  // namespace test

#endif  // INTRINSICCV_TEST_FRAMEWORK_ARRAY_H_
