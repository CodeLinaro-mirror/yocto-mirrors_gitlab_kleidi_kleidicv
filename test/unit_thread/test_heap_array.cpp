// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

#include "framework/utils.h"
#include "gtest/gtest.h"
#include "heap_array.h"
#include "test_config.h"

namespace {

using kleidicv::thread_internal::HeapArray;

static_assert(!std::is_copy_constructible_v<HeapArray<int>>);
static_assert(!std::is_copy_assignable_v<HeapArray<int>>);
static_assert(std::is_nothrow_move_constructible_v<HeapArray<int>>);
static_assert(std::is_nothrow_move_assignable_v<HeapArray<int>>);

struct LifetimeTracker {
  LifetimeTracker() { ++live_count; }
  ~LifetimeTracker() { --live_count; }

  static size_t live_count;
};

size_t LifetimeTracker::live_count = 0;

TEST(HeapArray, IsEmptyByDefault) {
  HeapArray<int> array;

  EXPECT_EQ(array.data(), nullptr);
  EXPECT_EQ(array.size(), 0U);
  EXPECT_EQ(array.begin(), nullptr);
  EXPECT_EQ(array.end(), nullptr);
}

TEST(HeapArray, AllocatesValueInitializedElements) {
  HeapArray<int> array;

  ASSERT_TRUE(array.allocate(3));
  ASSERT_NE(array.data(), nullptr);
  EXPECT_EQ(array.size(), 3U);
  EXPECT_EQ(array.end(), array.begin() + 3);
  EXPECT_EQ(array[0], 0);
  EXPECT_EQ(array[1], 0);
  EXPECT_EQ(array[2], 0);

  array[1] = 42;
  EXPECT_EQ(array.data()[1], 42);
}

TEST(HeapArray, ProvidesConstAccessToElements) {
  HeapArray<int> array;
  ASSERT_TRUE(array.allocate_and_fill(2, 11));
  const HeapArray<int> &const_array = array;

  EXPECT_EQ(const_array.data(), array.data());
  EXPECT_EQ(const_array.begin(), array.data());
  EXPECT_EQ(const_array.end(), array.data() + 2);
  EXPECT_EQ(const_array[1], 11);
}

TEST(HeapArray, ConstEndIsNullWhenEmpty) {
  const HeapArray<int> array;

  EXPECT_EQ(array.end(), nullptr);
}

TEST(HeapArray, AllocatesAndFillsElements) {
  HeapArray<int> array;

  ASSERT_TRUE(array.allocate_and_fill(3, 17));
  EXPECT_EQ(array[0], 17);
  EXPECT_EQ(array[1], 17);
  EXPECT_EQ(array[2], 17);
}

TEST(HeapArray, AcceptsZeroSizedAllocation) {
  HeapArray<int> array;

  EXPECT_TRUE(array.allocate(0));
  EXPECT_EQ(array.data(), nullptr);
  EXPECT_EQ(array.size(), 0U);
}

TEST(HeapArray, RejectsAllocationWhenAlreadyAllocated) {
  HeapArray<int> array;

  ASSERT_TRUE(array.allocate(2));
  int *const original_data = array.data();
  EXPECT_FALSE(array.allocate(3));
  EXPECT_EQ(array.data(), original_data);
  EXPECT_EQ(array.size(), 2U);
}

TEST(HeapArray, RejectsAllocationSizeOverflow) {
  HeapArray<int> array;
  constexpr size_t kOverflowingCount =
      std::numeric_limits<size_t>::max() / sizeof(int) + 1;

  EXPECT_FALSE(array.allocate(kOverflowingCount));
  EXPECT_EQ(array.data(), nullptr);
  EXPECT_EQ(array.size(), 0U);
}

#ifdef KLEIDICV_ALLOCATION_TESTS
TEST(HeapArray, ReturnsFalseWhenHeapAllocationFails) {
  HeapArray<LifetimeTracker> array;

  AllocationFailureMock::enable();
  const bool result = array.allocate(1);
  AllocationFailureMock::disable();

  EXPECT_FALSE(result);
  EXPECT_EQ(array.data(), nullptr);
  EXPECT_EQ(array.size(), 0U);
  EXPECT_EQ(LifetimeTracker::live_count, 0U);
}

TEST(HeapArray, AllocateAndFillReturnsFalseWhenHeapAllocationFails) {
  HeapArray<int> array;

  AllocationFailureMock::enable();
  const bool result = array.allocate_and_fill(1, 23);
  AllocationFailureMock::disable();

  EXPECT_FALSE(result);
  EXPECT_EQ(array.data(), nullptr);
  EXPECT_EQ(array.size(), 0U);
}
#endif

TEST(HeapArray, MoveConstructionTransfersOwnership) {
  HeapArray<int> source;
  ASSERT_TRUE(source.allocate_and_fill(2, 7));
  int *const original_data = source.data();

  HeapArray<int> destination{std::move(source)};

  EXPECT_EQ(destination.data(), original_data);
  EXPECT_EQ(destination.size(), 2U);
  EXPECT_EQ(destination[0], 7);
  EXPECT_EQ(destination[1], 7);
}

TEST(HeapArray, MoveAssignmentReleasesOldElementsAndTransfersOwnership) {
  EXPECT_EQ(LifetimeTracker::live_count, 0U);
  {
    HeapArray<LifetimeTracker> source;
    HeapArray<LifetimeTracker> destination;
    ASSERT_TRUE(source.allocate(2));
    ASSERT_TRUE(destination.allocate(3));
    EXPECT_EQ(LifetimeTracker::live_count, 5U);
    LifetimeTracker *const source_data = source.data();

    destination = std::move(source);

    EXPECT_EQ(LifetimeTracker::live_count, 2U);
    EXPECT_EQ(destination.data(), source_data);
    EXPECT_EQ(destination.size(), 2U);

    HeapArray<LifetimeTracker> &alias = destination;
    destination = std::move(alias);
    EXPECT_EQ(LifetimeTracker::live_count, 2U);
    EXPECT_EQ(destination.data(), source_data);
  }
  EXPECT_EQ(LifetimeTracker::live_count, 0U);
}

}  // namespace
