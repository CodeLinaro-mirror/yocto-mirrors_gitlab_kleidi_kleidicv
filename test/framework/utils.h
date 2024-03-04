// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_TEST_FRAMEWORK_UTILS_H_
#define INTRINSICCV_TEST_FRAMEWORK_UTILS_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <tuple>
#include <type_traits>

#include "framework/abstract.h"
#include "framework/types.h"
#include "intrinsiccv/ctypes.h"

#define INTRINSICCV_API(name, impl, type)                                     \
  template <typename ElementType,                                             \
            std::enable_if_t<std::is_same_v<ElementType, type>, bool> = true> \
  static decltype(auto) name() {                                              \
    return &impl;                                                             \
  }

// Generates a fatal failure with a generic message, and returns with a given
// value.
#define TEST_FAIL_WITH(return_value, message)                          \
  do {                                                                 \
    GTEST_MESSAGE_("Failed", ::testing::TestPartResult::kFatalFailure) \
        << message;                                                    \
    return (return_value);                                             \
  } while (0 != 0)

// Returns if the test has any failures.
#define ASSERT_NO_FAILURES()           \
  if (::testing::Test::HasFailure()) { \
    return;                            \
  }

class MockMallocToFail {
 public:
  static void enable() { enabled = true; }
  static void disable() { enabled = false; }
  static bool is_enabled() { return enabled; }

 private:
  static bool enabled;
};

namespace test {

class Options {
 public:
  // Returns the vector length being tested. This is in bytes.
  static size_t vector_length() { return vector_length_; }

  // Returns seed to use.
  static uint64_t seed() { return seed_; }

  // Returns the number of lanes in a vector for a given integral type.
  template <typename ElementType,
            std::enable_if_t<std::is_integral_v<ElementType>, bool> = true>
  static size_t vector_lanes() {
    return vector_length_ / sizeof(ElementType);
  }

  // Sets the vector length in bytes.
  static void set_vector_length(size_t value) {
    // Check for power of two.
    if ((value == 0) || ((value & (value - 1)) != 0)) {
      std::cerr << "Vector length must be a power of two: " << value
                << std::endl;
      std::exit(1);
    }

    vector_length_ = value;
  }

  // Sets the seed.
  static void set_seed(uint64_t value) { seed_ = value; }

 private:
  // Vector length being tested.
  static size_t vector_length_;
  // Seed to use.
  static uint64_t seed_;
};  // end of class Options

// Prints all the elements in a two-dimensional space.
template <typename ElementType>
void dump(const TwoDimensional<ElementType> *elements);

// Returns default border values.
std::array<intrinsiccv_border_values_t, 1> default_border_values();

// Returns an array of just a few small layouts.
std::array<test::ArrayLayout, 6> small_array_layouts(size_t min_width,
                                                     size_t min_height);
// Returns an array of default tested layouts.
std::array<test::ArrayLayout, 14> default_array_layouts(size_t min_width,
                                                        size_t min_height);

namespace internal {
template <typename Function, typename Tuple>
class NullPointerTester {
  // Set the given argument to null and test that the function diagnoses the
  // error correctly.
  template <typename ArgType, size_t ArgIndex>
  static typename std::enable_if<std::is_pointer_v<ArgType>>::type
  test_with_null_arg(Function f, Tuple t) {
    std::get<ArgIndex>(t) = nullptr;
    EXPECT_EQ(INTRINSICCV_ERROR_NULL_POINTER, std::apply(f, t));
  }

  // Skip arguments that aren't pointers.
  template <typename ArgType, size_t ArgIndex>
  static typename std::enable_if<!std::is_pointer_v<ArgType>>::type
  test_with_null_arg(Function, const Tuple &) {}

 public:
  template <int ArgIndex>
  static void test(Function f, const Tuple &t) {
    // Recurse to test earlier arguments first
    if constexpr (ArgIndex > 0) {
      test<ArgIndex - 1>(f, t);
    }
    using ArgType = typename std::tuple_element_t<ArgIndex, Tuple>;
    test_with_null_arg<ArgType, ArgIndex>(f, t);
  }
};

template <typename Func>
class ParamsExtractor;
template <typename Ret, typename... Params>
class ParamsExtractor<Ret (*)(Params...)> {
 public:
  using Tuple = std::tuple<Params...>;
};
}  // namespace internal

// Tests that the function returns INTRINSICCV_ERROR_NULL_POINTER if any of its
// pointer arguments are null.
template <typename Function, typename... Args>
void test_null_args(Function f, Args... args) {
  // Ensure that the function parameter types are used otherwise arguments may
  // not be recognised as pointers.
  using Tuple = typename internal::ParamsExtractor<Function>::Tuple;
  constexpr int LastArgIndex = std::tuple_size_v<Tuple> - 1;
  using Tester = internal::NullPointerTester<Function, Tuple>;
  Tester::template test<LastArgIndex>(f, Tuple(args...));
}

}  // namespace test

#endif  // INTRINSICCV_TEST_FRAMEWORK_UTILS_H_
