// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_THREAD_SME_THREAD_LIMITER_H
#define KLEIDICV_THREAD_SME_THREAD_LIMITER_H

#include "kleidicv/config.h"

#if KLEIDICV_ENABLE_SME_THREAD_DISPATCH

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

#include "heap_array.h"
#include "kleidicv/ctypes.h"

namespace kleidicv::thread_internal {

constexpr size_t kNoSmcuDomain = std::numeric_limits<size_t>::max();

struct CpuInfo {
  size_t id{};
  uint64_t midr{};
  uint64_t smidr{};
};

struct SmcuDomainInfo {
  uint64_t affinity;
  unsigned slot_count;
  bool shared;
};

// Reads Linux's CPU list format, for example "0-3,8", from path.
// Returns nullopt if the file cannot be read or is malformed.
std::optional<HeapArray<size_t>> read_online_cpu_ids(
    const char *path = "/sys/devices/system/cpu/online");

// Reads a hexadecimal uint64_t value from path.
// Returns nullopt if the file cannot be read or is malformed.
std::optional<uint64_t> read_hex_file(const char *path);

class SmcuTopology {
 public:
  static std::optional<SmcuTopology> detect();

  static std::optional<SmcuTopology> from_registers(
      const HeapArray<CpuInfo> &cpu_infos);

  [[nodiscard]] size_t domain_for_cpu(size_t cpu) const;
  [[nodiscard]] bool is_cpu_allowed(size_t cpu) const;
  [[nodiscard]] bool has_allowed_cpu() const;
  [[nodiscard]] size_t domain_count() const { return domains_.size(); }
  [[nodiscard]] unsigned slot_count(size_t domain) const;

 private:
  SmcuTopology(HeapArray<size_t> cpu_to_domain, HeapArray<bool> cpu_allowed,
               HeapArray<SmcuDomainInfo> domains)
      : cpu_to_domain_{std::move(cpu_to_domain)},
        cpu_allowed_{std::move(cpu_allowed)},
        domains_{std::move(domains)} {}

  HeapArray<size_t> cpu_to_domain_;
  HeapArray<bool> cpu_allowed_;
  HeapArray<SmcuDomainInfo> domains_;
};

class SmeThreadLimiter {
 public:
  struct AttemptResult {
    bool accepted;
    kleidicv_error_t error;
  };

  explicit SmeThreadLimiter(SmcuTopology topology);

  static const SmeThreadLimiter *instance();

  template <typename Callback>
  AttemptResult try_to_run(Callback &&callback) const {
    const int cpu = current_cpu();

    if (cpu < 0) {
      return AttemptResult{false, kleidicv_error_t{}};
    }

    return try_to_run_on_cpu(static_cast<size_t>(cpu),
                             std::forward<Callback>(callback));
  }

  template <typename Callback>
  AttemptResult try_to_run_on_cpu(size_t cpu, Callback &&callback) const {
    if (!topology_.is_cpu_allowed(cpu)) {
      return AttemptResult{false, kleidicv_error_t{}};
    }

    const size_t domain = topology_.domain_for_cpu(cpu);
    if (!try_acquire(domain)) {
      return AttemptResult{false, kleidicv_error_t{}};
    }

    SlotGuard slot_guard{&available_slots_[domain]};
    return AttemptResult{true, std::forward<Callback>(callback)()};
  }

  SmeThreadLimiter(const SmeThreadLimiter &) = delete;
  SmeThreadLimiter &operator=(const SmeThreadLimiter &) = delete;

 private:
  class SlotGuard {
   public:
    explicit SlotGuard(std::atomic<unsigned> *slot) : slot_{slot} {}
    ~SlotGuard() { slot_->fetch_add(1, std::memory_order_relaxed); }

    SlotGuard(const SlotGuard &) = delete;
    SlotGuard &operator=(const SlotGuard &) = delete;

   private:
    std::atomic<unsigned> *slot_;
  };

  static int current_cpu();
  bool try_acquire(size_t domain) const;

  SmcuTopology topology_;
  mutable HeapArray<std::atomic<unsigned>> available_slots_;
};

}  // namespace kleidicv::thread_internal

#endif  // KLEIDICV_ENABLE_SME_THREAD_DISPATCH

#endif  // KLEIDICV_THREAD_SME_THREAD_LIMITER_H
