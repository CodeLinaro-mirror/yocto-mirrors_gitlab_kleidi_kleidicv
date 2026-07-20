// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/config.h"

#if KLEIDICV_ENABLE_SME_THREAD_DISPATCH

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "sme_thread_limiter.h"

namespace {

using kleidicv::thread_internal::CpuInfo;
using kleidicv::thread_internal::HeapArray;
using kleidicv::thread_internal::kNoSmcuDomain;
using kleidicv::thread_internal::read_hex_file;
using kleidicv::thread_internal::read_online_cpu_ids;
using kleidicv::thread_internal::SmcuTopology;
using kleidicv::thread_internal::SmeThreadLimiter;

constexpr uint64_t make_midr(uint64_t implementer, uint64_t part) {
  return (implementer << 24) | (0xFULL << 16) | (part << 4);
}

constexpr uint64_t make_smidr(uint64_t affinity, unsigned smcu_count) {
  return (static_cast<uint64_t>(smcu_count - 1) << 56) | (3ULL << 13) |
         (((affinity >> 12) & 0xFFFFF) << 32) | (affinity & 0xFFF);
}

template <typename T>
HeapArray<T> make_heap_array(std::initializer_list<T> values) {
  HeapArray<T> result;
  if (!result.allocate(values.size())) {
    ADD_FAILURE() << "Failed to allocate test array";
    return result;
  }
  size_t index = 0;
  for (const auto &value : values) {
    result[index++] = value;
  }
  return result;
}

class TemporaryFile {
 public:
  explicit TemporaryFile(const char *contents) {
    char path[] = "/tmp/kleidicv-cpu-list-XXXXXX";
    const int file = mkstemp(path);
    EXPECT_NE(file, -1);
    if (file == -1) {
      return;
    }
    path_ = path;
    const size_t length = strlen(contents);
    EXPECT_EQ(write(file, contents, length), static_cast<ssize_t>(length));
    EXPECT_EQ(close(file), 0);
  }

  ~TemporaryFile() {
    if (!path_.empty()) {
      EXPECT_EQ(unlink(path_.c_str()), 0);
    }
  }

  const char *path() const { return path_.c_str(); }

 private:
  std::string path_;
};

TEST(ReadOnlineCpuIds, ReadsIndividualIdsAndRanges) {
  TemporaryFile file{"0-3,8,10-12\n"};
  auto cpu_ids = read_online_cpu_ids(file.path());

  const std::vector<size_t> expected{0, 1, 2, 3, 8, 10, 11, 12};
  if (!cpu_ids) {
    ADD_FAILURE() << "Failed to read CPU IDs";
    return;
  }
  EXPECT_EQ(std::vector<size_t>(cpu_ids->begin(), cpu_ids->end()), expected);
}

TEST(ReadOnlineCpuIds, ReadsSparseIndividualIds) {
  TemporaryFile file{"1,17,2048"};
  auto cpu_ids = read_online_cpu_ids(file.path());

  const std::vector<size_t> expected{1, 17, 2048};
  if (!cpu_ids) {
    ADD_FAILURE() << "Failed to read CPU IDs";
    return;
  }
  EXPECT_EQ(std::vector<size_t>(cpu_ids->begin(), cpu_ids->end()), expected);
}

TEST(ReadOnlineCpuIds, RejectsMalformedLists) {
  for (const char *contents :
       {"", "0-", "3-1", "0,", "0,,2", "0 2", "0\n1", "18446744073709551616"}) {
    TemporaryFile file{contents};
    EXPECT_FALSE(read_online_cpu_ids(file.path())) << contents;
  }
}

TEST(ReadOnlineCpuIds, ReturnsNulloptWhenFileCannotBeOpened) {
  EXPECT_FALSE(read_online_cpu_ids("/path/which/does/not/exist"));
}

TEST(ReadOnlineCpuIds, RejectsNullPath) {
  EXPECT_FALSE(read_online_cpu_ids(nullptr));
}

TEST(ReadOnlineCpuIds, RejectsFileAtCapacityLimit) {
  const std::string contents(512, '0');
  TemporaryFile file{contents.c_str()};

  EXPECT_FALSE(read_online_cpu_ids(file.path()));
}

TEST(ReadOnlineCpuIds, ReturnsNulloptWhenFileCannotBeRead) {
  EXPECT_FALSE(read_online_cpu_ids("/tmp"));
}

TEST(ReadHexFile, ReadsSupportedHexadecimalFormats) {
  struct TestCase {
    const char *contents;
    uint64_t expected;
  };
  constexpr TestCase kTestCases[] = {
      {"0", 0},
      {"0123456789abcdef", 0x0123456789ABCDEF},
      {"0XABCDEF", 0xABCDEF},
      {"0x123\n", 0x123},
      {"FEDCBA\r", 0xFEDCBA},
      {"ffffffffffffffff", std::numeric_limits<uint64_t>::max()},
  };

  for (const auto &test_case : kTestCases) {
    TemporaryFile file{test_case.contents};
    EXPECT_EQ(read_hex_file(file.path()), test_case.expected)
        << test_case.contents;
  }
}

TEST(ReadHexFile, RejectsMalformedOrOverflowingValues) {
  for (const char *contents :
       {"", "0x", "-1", " 1", "12g", "10000000000000000"}) {
    TemporaryFile file{contents};
    EXPECT_FALSE(read_hex_file(file.path())) << contents;
  }
}

TEST(ReadHexFile, ReturnsEmptyValueWhenFileCannotBeOpenedOrRead) {
  EXPECT_FALSE(read_hex_file("/path/which/does/not/exist"));
  EXPECT_FALSE(read_hex_file("/tmp"));
}

TEST(SmcuTopology, AllowsOnlySelectedArmC1Cpus) {
  constexpr uint64_t kArmImplementer = 0x41;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, make_midr(kArmImplementer, 0xD8A), make_smidr(0, 1)},
      {1, make_midr(kArmImplementer, 0xD8B), make_smidr(1, 1)},
      {2, make_midr(kArmImplementer, 0xD90), make_smidr(2, 1)},
      {3, make_midr(kArmImplementer, 0xD8C), make_smidr(3, 1)},
      {4, make_midr(kArmImplementer, 0xD85), make_smidr(4, 1)},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  ASSERT_EQ(topology->domain_count(), 5U);
  EXPECT_TRUE(topology->is_cpu_allowed(0));
  EXPECT_TRUE(topology->is_cpu_allowed(1));
  EXPECT_TRUE(topology->is_cpu_allowed(2));
  EXPECT_FALSE(topology->is_cpu_allowed(3));
  EXPECT_FALSE(topology->is_cpu_allowed(4));
}

TEST(SmcuTopology, KeepsSingleDomainNonArmPlatformWithNoAllowedCpu) {
  constexpr uint64_t kNonArmMidr = 0x510F0020;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kNonArmMidr, make_smidr(0, 1)},
      {1, kNonArmMidr, make_smidr(0, 1)},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  ASSERT_EQ(topology->domain_count(), 1U);
  EXPECT_FALSE(topology->has_allowed_cpu());
}

TEST(SmcuTopology, UsesAffinityZeroOnNonArmTopologyWithMoreSharedDomains) {
  constexpr uint64_t kNonArmMidr = 0x510F0020;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kNonArmMidr, make_smidr(0, 1)},
      {1, kNonArmMidr, make_smidr(1, 1)},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  ASSERT_EQ(topology->domain_count(), 2U);
  EXPECT_TRUE(topology->is_cpu_allowed(0));
  EXPECT_FALSE(topology->is_cpu_allowed(1));
}

TEST(SmcuTopology, IgnoresExplicitlyUnsharedZeroAffinityDomain) {
  constexpr uint64_t kNonArmMidr = 0x510F0020;
  constexpr uint64_t kExplicitlyUnsharedZeroAffinity = 2ULL << 13;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kNonArmMidr, kExplicitlyUnsharedZeroAffinity},
      {1, kNonArmMidr, make_smidr(1, 1)},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  ASSERT_EQ(topology->domain_count(), 2U);
  EXPECT_FALSE(topology->has_allowed_cpu());
}

TEST(SmcuTopology, KeepsNonArmTopologyWithoutZeroAffinityDomain) {
  constexpr uint64_t kNonArmMidr = 0x510F0020;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kNonArmMidr, make_smidr(1, 1)},
      {1, kNonArmMidr, make_smidr(2, 1)},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  EXPECT_EQ(topology->domain_count(), 2U);
  EXPECT_FALSE(topology->has_allowed_cpu());
}

// Mixed CPU implementers make the topology inconsistent.
TEST(SmcuTopology, RejectsMixedImplementers) {
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, make_midr(0x51, 0x002), make_smidr(0, 1)},
      {1, make_midr(0x42, 0x002), make_smidr(1, 1)},
  });

  EXPECT_FALSE(SmcuTopology::from_registers(cpu_infos));
}

TEST(SmcuTopology, RejectsConflictingSlotCountsForSharedDomain) {
  constexpr uint64_t kArmImplementer = 0x41;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, make_midr(kArmImplementer, 0xD8B), make_smidr(4, 2)},
      {1, make_midr(kArmImplementer, 0xD8C), make_smidr(4, 1)},
  });

  EXPECT_FALSE(SmcuTopology::from_registers(cpu_infos));
}

// These values have the same low affinity byte but differ in Affinity[11:8]
// or Affinity2[51:32], so each must identify a separate shared SMCU domain.
TEST(SmcuTopology, UsesCompleteArchitecturalAffinity) {
  constexpr uint64_t kMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kMidr, 0x0000000000006001},
      {1, kMidr, 0x0000000000006101},
      {2, kMidr, 0x0000000100006001},
      {3, kMidr, 0x0000000200006001},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  ASSERT_EQ(topology->domain_count(), 4U);
  EXPECT_NE(topology->domain_for_cpu(0), topology->domain_for_cpu(1));
  EXPECT_NE(topology->domain_for_cpu(0), topology->domain_for_cpu(2));
  EXPECT_NE(topology->domain_for_cpu(2), topology->domain_for_cpu(3));
}

// SH=0b11 marks both PEs as sharing one domain. NSMC=5 in bits [59:56]
// reports six SMCUs for that domain.
TEST(SmcuTopology, DecodesSharedDomainSmcuCount) {
  constexpr uint64_t kMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kMidr, 0x0500000100006123},
      {1, kMidr, 0x0500000100006123},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  ASSERT_EQ(topology->domain_count(), 1U);
  EXPECT_EQ(topology->domain_for_cpu(0), topology->domain_for_cpu(1));
  EXPECT_EQ(topology->slot_count(0), 6U);
}

// NSMC=0b1110 is the largest valid encoding and reports 15 SMCUs.
TEST(SmcuTopology, DecodesMaximumSharedDomainSmcuCount) {
  constexpr uint64_t kMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kMidr, make_smidr(0, 15)},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  ASSERT_EQ(topology->domain_count(), 1U);
  EXPECT_EQ(topology->slot_count(0), 15U);
}

// NSMC=0b1111 is reserved, so the entire topology must be rejected.
TEST(SmcuTopology, RejectsReservedSmcuCountEncoding) {
  constexpr uint64_t kMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kMidr, make_smidr(0, 16)},
  });

  EXPECT_FALSE(SmcuTopology::from_registers(cpu_infos));
}

// SH=0b10 explicitly marks each PE as unshared. Equal affinity values must
// therefore remain separate one-slot domains.
TEST(SmcuTopology, KeepsExplicitlyUnsharedCpusSeparate) {
  constexpr uint64_t kMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kMidr, 0x0000000041004123},
      {1, kMidr, 0x0000000041004123},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  ASSERT_EQ(topology->domain_count(), 2U);
  EXPECT_NE(topology->domain_for_cpu(0), topology->domain_for_cpu(1));
  EXPECT_EQ(topology->slot_count(0), 1U);
  EXPECT_EQ(topology->slot_count(1), 1U);
}

// With the legacy SH=0b00 encoding, zero Affinity means unshared while a
// nonzero Affinity means shared.
TEST(SmcuTopology, HonorsLegacySharingEncoding) {
  constexpr uint64_t kMidr = 0x510F0020;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kMidr, 0x0000000041000000},
      {1, kMidr, 0x0000000041000000},
      {2, kMidr, 0x0000000041000001},
      {3, kMidr, 0x0000000041000001},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  ASSERT_EQ(topology->domain_count(), 3U);
  EXPECT_NE(topology->domain_for_cpu(0), topology->domain_for_cpu(1));
  EXPECT_EQ(topology->domain_for_cpu(2), topology->domain_for_cpu(3));
  EXPECT_FALSE(topology->has_allowed_cpu());
}

// An all-zero SMIDR value is architecturally valid and identifies one
// unshared SMCU. It must not be confused with an unavailable register file.
TEST(SmcuTopology, TreatsAvailableZeroSmidrAsUnshared) {
  constexpr uint64_t kArmC1ProMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kArmC1ProMidr, 0},
      {1, kArmC1ProMidr, 0},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  ASSERT_EQ(topology->domain_count(), 2U);
  EXPECT_NE(topology->domain_for_cpu(0), topology->domain_for_cpu(1));
  EXPECT_EQ(topology->slot_count(0), 1U);
  EXPECT_EQ(topology->slot_count(1), 1U);
  EXPECT_TRUE(topology->is_cpu_allowed(0));
  EXPECT_TRUE(topology->is_cpu_allowed(1));
}

// SH=0b01 is reserved, so the entire topology must be rejected.
TEST(SmcuTopology, RejectsReservedSharingEncoding) {
  constexpr uint64_t kMidr = 0x510F0020;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kMidr, make_smidr(0, 1)},
      {1, kMidr, 0x0000000041002001},
  });
  EXPECT_FALSE(SmcuTopology::from_registers(cpu_infos));
}

TEST(SmcuTopology, RejectsEmptyCpuInfoArray) {
  HeapArray<CpuInfo> cpu_infos;
  EXPECT_FALSE(SmcuTopology::from_registers(cpu_infos));
}

TEST(SmcuTopology, RejectsMaximumCpuId) {
  constexpr uint64_t kArmC1ProMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {std::numeric_limits<size_t>::max(), kArmC1ProMidr, make_smidr(0, 1)},
  });

  EXPECT_FALSE(SmcuTopology::from_registers(cpu_infos));
}

TEST(SmcuTopology, MapsSparseCpuIds) {
  constexpr uint64_t kArmC1ProMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {2, kArmC1ProMidr, make_smidr(0, 1)},
      {5, kArmC1ProMidr, make_smidr(1, 1)},
  });

  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  EXPECT_TRUE(topology->is_cpu_allowed(2));
  EXPECT_TRUE(topology->is_cpu_allowed(5));
  EXPECT_FALSE(topology->is_cpu_allowed(0));
  EXPECT_FALSE(topology->is_cpu_allowed(3));
  EXPECT_EQ(topology->domain_for_cpu(0), kNoSmcuDomain);
  EXPECT_NE(topology->domain_for_cpu(2), topology->domain_for_cpu(5));
}

TEST(SmeThreadLimiter, LimitsEachDomainIndependentlyAndReleasesSlots) {
  constexpr uint64_t kArmC1ProMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kArmC1ProMidr, make_smidr(0, 1)},
      {1, kArmC1ProMidr, make_smidr(0, 1)},
      {2, kArmC1ProMidr, make_smidr(0, 1)},
  });
  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  SmeThreadLimiter limiter{std::move(*topology)};

  bool nested_callback_ran = false;
  auto first = limiter.try_to_run_on_cpu(0, [&]() {
    auto nested = limiter.try_to_run_on_cpu(1, [&]() {
      nested_callback_ran = true;
      return KLEIDICV_OK;
    });
    EXPECT_FALSE(nested.accepted);
    return KLEIDICV_ERROR_RANGE;
  });

  EXPECT_TRUE(first.accepted);
  EXPECT_EQ(first.error, KLEIDICV_ERROR_RANGE);
  EXPECT_FALSE(nested_callback_ran);

  auto after_release =
      limiter.try_to_run_on_cpu(2, []() { return KLEIDICV_OK; });
  EXPECT_TRUE(after_release.accepted);
  EXPECT_EQ(after_release.error, KLEIDICV_OK);
}

TEST(SmeThreadLimiter, AllowsConcurrentUseOfSeparateDomains) {
  constexpr uint64_t kArmC1ProMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kArmC1ProMidr, make_smidr(0, 1)},
      {1, kArmC1ProMidr, make_smidr(0, 1)},
      {2, kArmC1ProMidr, make_smidr(3, 1)},
  });
  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  SmeThreadLimiter limiter{std::move(*topology)};

  bool second_domain_ran = false;
  auto first = limiter.try_to_run_on_cpu(0, [&]() {
    auto second = limiter.try_to_run_on_cpu(2, [&]() {
      second_domain_ran = true;
      return KLEIDICV_OK;
    });
    EXPECT_TRUE(second.accepted);
    return KLEIDICV_OK;
  });

  EXPECT_TRUE(first.accepted);
  EXPECT_TRUE(second_domain_ran);
}

TEST(SmeThreadLimiter, RejectsOutOfRangeCpu) {
  constexpr uint64_t kArmC1ProMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kArmC1ProMidr, make_smidr(0, 1)},
  });
  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  SmeThreadLimiter limiter{std::move(*topology)};

  bool callback_ran = false;
  auto result = limiter.try_to_run_on_cpu(3, [&]() {
    callback_ran = true;
    return KLEIDICV_OK;
  });

  EXPECT_FALSE(result.accepted);
  EXPECT_FALSE(callback_ran);
}

TEST(SmeThreadLimiter, EnforcesSlotCountAcrossWorkerThreads) {
  constexpr uint64_t kArmC1ProMidr = 0x410FD8B0;
  auto cpu_infos = make_heap_array<CpuInfo>({
      {0, kArmC1ProMidr, make_smidr(0, 2)},
      {1, kArmC1ProMidr, make_smidr(0, 2)},
      {2, kArmC1ProMidr, make_smidr(0, 2)},
  });
  auto topology = SmcuTopology::from_registers(cpu_infos);
  if (!topology) {
    ADD_FAILURE() << "Failed to create topology";
    return;
  }
  SmeThreadLimiter limiter{std::move(*topology)};

  std::mutex mutex;
  std::condition_variable condition;
  unsigned active = 0;
  bool release = false;
  std::atomic<unsigned> accepted{0};

  auto hold_slot = [&](int cpu) {
    auto result = limiter.try_to_run_on_cpu(cpu, [&]() {
      std::unique_lock lock{mutex};
      ++active;
      condition.notify_all();
      condition.wait(lock, [&]() { return release; });
      return KLEIDICV_OK;
    });
    if (result.accepted) {
      accepted.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::thread first{hold_slot, 0};
  std::thread second{hold_slot, 1};

  bool both_active = false;
  {
    std::unique_lock lock{mutex};
    both_active = condition.wait_for(lock, std::chrono::seconds{5},
                                     [&]() { return active == 2; });
  }

  if (both_active) {
    auto third =
        limiter.try_to_run_on_cpu(2, []() { return KLEIDICV_ERROR_RANGE; });
    EXPECT_FALSE(third.accepted);
  }

  {
    std::lock_guard lock{mutex};
    release = true;
  }
  condition.notify_all();
  first.join();
  second.join();

  ASSERT_TRUE(both_active);
  EXPECT_EQ(accepted.load(std::memory_order_relaxed), 2U);
  auto after_release =
      limiter.try_to_run_on_cpu(2, []() { return KLEIDICV_OK; });
  EXPECT_TRUE(after_release.accepted);
}

}  // namespace

#endif
