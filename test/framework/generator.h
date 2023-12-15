// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_TEST_FRAMEWORK_GENERATOR_H_
#define INTRINSICCV_TEST_FRAMEWORK_GENERATOR_H_

#include <random>

#include "framework/abstract.h"
#include "framework/utils.h"

namespace test {

/// Generates pseudo-random numbers of a given type.
template <typename ElementType>
class PseudoRandomNumberGenerator : public Generator<ElementType> {
 public:
  PseudoRandomNumberGenerator() : seed_{Options::seed()} { reset(); }

  /// Resets the generator to the initial state.
  void reset() override { rng_.seed(seed_); }

  /// Yields the next value or std::nullopt.
  std::optional<ElementType> next() override {
    return static_cast<ElementType>(rng_());
  }

 protected:
  uint64_t seed_;
  std::mt19937_64 rng_;
};  // end of class PseudoRandomNumberGenerator<ElementType>

/// Generator which yields values of an iterable container.
template <typename IterableType>
class SequenceGenerator : public Generator<typename IterableType::value_type> {
 public:
  explicit SequenceGenerator(const IterableType &container)
      : begin_{container.begin()},
        current_{container.begin()},
        end_{container.end()} {}

  /// Resets the generator to its initial state.
  void reset() override { current_ = begin_; }

  /// Yields the next value or std::nullopt.
  std::optional<typename IterableType::value_type> next() override {
    if (current_ == end_) {
      return std::nullopt;
    }

    return *current_++;
  }

 protected:
  typename IterableType::const_iterator begin_;
  typename IterableType::const_iterator current_;
  typename IterableType::const_iterator end_;
};  // end of class SequenceGenerator<IterableType>

}  // namespace test

#endif  // INTRINSICCV_TEST_FRAMEWORK_GENERATOR_H_
