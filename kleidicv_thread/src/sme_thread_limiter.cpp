// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "sme_thread_limiter.h"

#include "kleidicv/config.h"

#if KLEIDICV_ENABLE_SME_THREAD_DISPATCH

#include <fcntl.h>
#include <sched.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <optional>

namespace kleidicv::thread_internal {
namespace {

constexpr uint64_t kArmImplementer = 0x41;
constexpr uint64_t kC1NanoPart = 0xD8A;
constexpr uint64_t kC1ProPart = 0xD8B;
constexpr uint64_t kC1UltraPart = 0xD8C;
constexpr uint64_t kC1PremiumPart = 0xD90;
constexpr size_t kRegisterPathCapacity = 128;
constexpr size_t kCpuListCapacity = 512;

constexpr uint64_t midr_implementer(uint64_t midr) {
  return (midr >> 24) & 0xFF;
}

constexpr uint64_t midr_part(uint64_t midr) { return (midr >> 4) & 0xFFF; }

constexpr uint64_t smidr_affinity(uint64_t smidr) {
  // Affinity2 and Affinity form the system-wide 32-bit identifier of the
  // SMCUs associated with a PE. Keep both fields so distinct sharing domains
  // cannot be merged when their low affinity bits happen to match.
  const uint64_t affinity = smidr & 0xFFF;
  const uint64_t affinity2 = (smidr >> 32) & 0xFFFFF;
  return (affinity2 << 12) | affinity;
}

enum class SmcuSharing { kUnshared, kShared, kReserved };

constexpr SmcuSharing smidr_sharing(uint64_t smidr) {
  // SH explicitly describes sharing except for the legacy zero encoding,
  // where a zero Affinity means unshared and a nonzero Affinity means shared.
  const uint64_t sharing = (smidr >> 13) & 0x3;
  switch (sharing) {
    case 0x3:
      return SmcuSharing::kShared;
    case 0x2:
      return SmcuSharing::kUnshared;
    case 0x0:
      return (smidr & 0xFFF) == 0 ? SmcuSharing::kUnshared
                                  : SmcuSharing::kShared;
    default:
      return SmcuSharing::kReserved;
  }
}

constexpr std::optional<unsigned> smidr_smcu_count(uint64_t smidr) {
  const auto encoded_count = static_cast<unsigned>((smidr >> 56) & 0xF);
  if (encoded_count == 0xF) {
    return std::nullopt;
  }
  return encoded_count + 1;
}

template <size_t RegisterNameSize>
std::optional<std::array<char, kRegisterPathCapacity>> make_register_path(
    size_t cpu, const char (&register_name)[RegisterNameSize]) {
  constexpr char kPrefix[] = "/sys/devices/system/cpu/cpu";
  constexpr char kSuffix[] = "/regs/identification/";
  constexpr size_t kMaxCpuDigits = std::numeric_limits<size_t>::digits10 + 1;
  static_assert(sizeof(kPrefix) - 1 + kMaxCpuDigits + sizeof(kSuffix) - 1 +
                    RegisterNameSize <=
                kRegisterPathCapacity);

  std::array<char, kRegisterPathCapacity> path{};
  char *position = path.data();

  std::memcpy(position, kPrefix, sizeof(kPrefix) - 1);
  position += sizeof(kPrefix) - 1;

  const auto conversion =
      std::to_chars(position, position + kMaxCpuDigits, cpu);
  if (conversion.ec != std::errc{}) {
    return std::nullopt;
  }
  position = conversion.ptr;

  std::memcpy(position, kSuffix, sizeof(kSuffix) - 1);
  position += sizeof(kSuffix) - 1;

  std::memcpy(position, register_name, RegisterNameSize);

  return path;
}

std::optional<size_t> read_raw_file(const char *path, char *buffer,
                                    size_t capacity) {
  if (!path || !buffer || capacity == 0) {
    return std::nullopt;
  }

  const int file = open(path, O_RDONLY | O_CLOEXEC);  // NOLINT
  if (file < 0) {
    return std::nullopt;
  }

  size_t length = 0;
  bool eof_reached = false;
  // A file exactly `capacity` bytes long is rejected but this way the source is
  // simpler.
  while (length < capacity) {
    const ssize_t bytes_read = read(file, buffer + length, capacity - length);
    if (bytes_read > 0) {
      length += static_cast<size_t>(bytes_read);
      continue;
    }
    if (bytes_read < 0 && errno == EINTR) {
      continue;
    }
    eof_reached = (bytes_read == 0);
    break;
  }
  close(file);

  if (eof_reached) {
    return length;
  }
  return std::nullopt;
}

std::optional<size_t> find_next_comma(const char *contents, size_t length,
                                      size_t start) {
  if (!contents || start > length) {
    return std::nullopt;
  }
  for (size_t index = start; index < length; ++index) {
    if (contents[index] == ',') {
      return index;
    }
  }
  return std::nullopt;
}

struct CpuIdRange {
  size_t first_id;
  size_t cpu_count;
};

std::optional<CpuIdRange> read_cpu_range(const char *contents, size_t begin,
                                         size_t end) {
  if (!contents || begin >= end) {
    return std::nullopt;
  }

  size_t first_id = std::numeric_limits<size_t>::max();
  const char *const range_end = contents + end;
  const auto first_result =
      std::from_chars(contents + begin, range_end, first_id);
  if (first_result.ec != std::errc{}) {
    return std::nullopt;
  }

  if (first_result.ptr == range_end) {
    return CpuIdRange{first_id, 1};
  }

  if (*first_result.ptr != '-') {
    return std::nullopt;
  }

  size_t second_id = std::numeric_limits<size_t>::max();
  const auto second_result =
      std::from_chars(first_result.ptr + 1, range_end, second_id);
  if (second_result.ec != std::errc{} ||  // NOLINT(whitespace/braces)
      second_result.ptr != range_end || second_id <= first_id) {
    return std::nullopt;
  }
  const size_t range_span = second_id - first_id;

  if (range_span == std::numeric_limits<size_t>::max()) {
    return std::nullopt;
  }
  return CpuIdRange{first_id, range_span + 1};
}

std::optional<size_t> find_shared_domain(const SmcuDomainInfo *domains,
                                         size_t domain_count,
                                         uint64_t affinity) {
  for (size_t i = 0; i < domain_count; ++i) {
    if (domains[i].shared && domains[i].affinity == affinity) {
      return i;
    }
  }
  return std::nullopt;
}

bool is_there_shared_domain_with_nonzero_affinity(
    const HeapArray<SmcuDomainInfo> &domains) {
  return std::any_of(domains.begin(), domains.end(), [](const auto &domain) {
    return domain.shared && domain.affinity != 0;
  });
}

void apply_cpu_policy_for_arm_cores(const HeapArray<CpuInfo> &cpu_infos,
                                    bool *cpu_allowed) {
  for (const auto &info : cpu_infos) {
    switch (midr_part(info.midr)) {
      case kC1NanoPart:
      case kC1ProPart:
      case kC1PremiumPart:
        cpu_allowed[info.id] = true;
        break;
      case kC1UltraPart:
      default:
        cpu_allowed[info.id] = false;
        break;
    }
  }
}

void enable_cores_for_shared_zero_affinity_domain(const SmcuDomainInfo *domains,
                                                  size_t domain_count,
                                                  const size_t *cpu_to_domain,
                                                  size_t topology_cpu_count,
                                                  bool *cpu_allowed) {
  std::optional<size_t> domain;
  for (size_t i = 0; i < domain_count; ++i) {
    if (domains[i].shared && domains[i].affinity == 0) {
      domain = i;
      break;
    }
  }

  if (!domain) {
    return;
  }

  for (size_t i = 0; i < topology_cpu_count; ++i) {
    if (static_cast<size_t>(cpu_to_domain[i]) == *domain) {
      cpu_allowed[i] = true;
    }
  }
}

std::optional<HeapArray<SmcuDomainInfo>> populate_domains(
    const HeapArray<CpuInfo> &cpu_infos, size_t *cpu_to_domain) {
  HeapArray<SmcuDomainInfo> domains;
  if (!domains.allocate(cpu_infos.size())) {
    return std::nullopt;
  }

  size_t domain_count = 0;
  for (const auto &info : cpu_infos) {
    const SmcuSharing sharing = smidr_sharing(info.smidr);
    if (sharing == SmcuSharing::kReserved) {
      return std::nullopt;
    }

    const uint64_t affinity = smidr_affinity(info.smidr);
    const bool is_shared = sharing == SmcuSharing::kShared;
    const auto smcu_count = smidr_smcu_count(info.smidr);
    if (!smcu_count) {
      return std::nullopt;
    }
    const unsigned slot_count = is_shared ? *smcu_count : 1;
    std::optional<size_t> domain_idx =
        is_shared ? find_shared_domain(domains.data(), domain_count, affinity)
                  : std::nullopt;
    if (!domain_idx) {
      if (domain_count >= domains.size()) {
        return std::nullopt;
      }
      domain_idx = domain_count;
      domains[domain_count++] = SmcuDomainInfo{affinity, slot_count, is_shared};
    } else {
      const auto &domain_info = domains[*domain_idx];
      if (domain_info.slot_count != slot_count) {
        return std::nullopt;
      }
    }

    cpu_to_domain[info.id] = *domain_idx;
  }

  if (domain_count == domains.size()) {
    return std::optional<HeapArray<SmcuDomainInfo>>{std::move(domains)};
  }

  HeapArray<SmcuDomainInfo> compact_domains;
  if (!compact_domains.allocate(domain_count)) {
    return std::nullopt;
  }
  for (size_t i = 0; i < domain_count; ++i) {
    compact_domains[i] = domains[i];
  }
  return std::optional<HeapArray<SmcuDomainInfo>>{std::move(compact_domains)};
}

}  // namespace

std::optional<uint64_t> read_hex_file(const char *path) {
  // No mode argument is required without O_CREAT or O_TMPFILE.
  const int file = open(path, O_RDONLY | O_CLOEXEC);  // NOLINT
  if (file < 0) {
    return std::nullopt;
  }

  char buffer[32];
  const ssize_t length = read(file, buffer, sizeof(buffer));
  close(file);
  if (length <= 0) {
    return std::nullopt;
  }

  size_t index = 0;
  if (length >= 2 && buffer[0] == '0' &&
      (buffer[1] == 'x' || buffer[1] == 'X')) {
    index = 2;
  }

  const char *begin = buffer + index;
  const char *buffer_end = buffer + length;
  const char *end = std::find_if(begin, buffer_end, [](char character) {
    return character == '\n' || character == '\r';
  });

  uint64_t value = 0;
  const auto conversion = std::from_chars(begin, end, value, 16);
  if (conversion.ec != std::errc{} ||  // NOLINT(whitespace/braces)
      conversion.ptr != end) {
    return std::nullopt;
  }
  return value;
}

std::optional<HeapArray<size_t>> read_online_cpu_ids(const char *path) {
  HeapArray<size_t> result;
  char buffer[kCpuListCapacity];
  const auto raw_length = read_raw_file(path, buffer, sizeof(buffer));
  if (!raw_length || *raw_length == 0) {
    return std::nullopt;
  }
  size_t length = *raw_length;
  if (buffer[length - 1] == '\n') {
    --length;
  }
  if (length == 0 || buffer[length - 1] == ',') {
    return std::nullopt;
  }

  size_t cpu_count = 0;
  size_t begin = 0;
  while (begin < length) {
    const auto comma_index = find_next_comma(buffer, length, begin);
    const size_t end = comma_index ? *comma_index : length;
    const auto range = read_cpu_range(buffer, begin, end);
    if (!range ||
        cpu_count > std::numeric_limits<size_t>::max() - range->cpu_count) {
      return std::nullopt;
    }
    cpu_count += range->cpu_count;
    begin = comma_index ? *comma_index + 1 : length;
  }

  if (!result.allocate(cpu_count)) {
    return std::nullopt;
  }

  begin = 0;
  size_t i = 0;
  while (begin < length) {
    const auto comma_index = find_next_comma(buffer, length, begin);
    const size_t end = comma_index ? *comma_index : length;
    const auto range = read_cpu_range(buffer, begin, end);
    // The buffer contents were validated above, but clang-tidy requires this
    // check before dereferencing the optional.
    if (!range) {
      return std::nullopt;
    }
    for (size_t j = 0; j < range->cpu_count; ++j) {
      result[i++] = range->first_id + j;
    }
    begin = comma_index ? *comma_index + 1 : length;
  }
  return std::optional<HeapArray<size_t>>{std::move(result)};
}

std::optional<SmcuTopology> SmcuTopology::detect() {
  const auto cpu_ids = read_online_cpu_ids();
  if (!cpu_ids) {
    return std::nullopt;
  }

  HeapArray<CpuInfo> cpu_infos;
  if (!cpu_infos.allocate(cpu_ids->size())) {
    return std::nullopt;
  }

  size_t cpu_index = 0;
  for (auto cpu_id : *cpu_ids) {
    const auto midr_path = make_register_path(cpu_id, "midr_el1");
    const auto smidr_path = make_register_path(cpu_id, "smidr_el1");
    if (!midr_path || !smidr_path) {
      return std::nullopt;
    }
    const auto midr = read_hex_file(midr_path->data());
    const auto smidr = read_hex_file(smidr_path->data());
    if (!midr || !smidr) {
      return std::nullopt;
    }
    cpu_infos[cpu_index++] = CpuInfo{cpu_id, *midr, *smidr};
  }

  return from_registers(cpu_infos);
}

std::optional<SmcuTopology> SmcuTopology::from_registers(
    const HeapArray<CpuInfo> &cpu_infos) {
  if (cpu_infos.size() == 0) {
    return std::nullopt;
  }

  size_t largest_cpu_id = 0;
  for (const auto &info : cpu_infos) {
    largest_cpu_id = std::max(largest_cpu_id, info.id);
  }
  if (largest_cpu_id == std::numeric_limits<size_t>::max()) {
    return std::nullopt;
  }
  // Include gaps for offline or absent CPU IDs in the topology arrays.
  const size_t topology_cpu_count = largest_cpu_id + 1;

  const uint64_t implementer = midr_implementer(cpu_infos[0].midr);
  for (const auto &info : cpu_infos) {
    if (midr_implementer(info.midr) != implementer) {
      return std::nullopt;
    }
  }

  HeapArray<size_t> cpu_to_domain;
  HeapArray<bool> cpu_allowed;
  if (!cpu_to_domain.allocate_and_fill(topology_cpu_count, kNoSmcuDomain) ||
      !cpu_allowed.allocate_and_fill(topology_cpu_count, false)) {
    return std::nullopt;
  }

  auto domains = populate_domains(cpu_infos, cpu_to_domain.data());
  if (!domains) {
    return std::nullopt;
  }

  if (implementer == kArmImplementer) {
    apply_cpu_policy_for_arm_cores(cpu_infos, cpu_allowed.data());
  } else if (is_there_shared_domain_with_nonzero_affinity(*domains)) {
    // If there is a shared domain with nonzero affinity, assume SME usage is
    // beneficial in the shared domain with zero affinity.
    enable_cores_for_shared_zero_affinity_domain(
        domains->data(), domains->size(), cpu_to_domain.data(),
        topology_cpu_count, cpu_allowed.data());
  }

  return SmcuTopology{std::move(cpu_to_domain), std::move(cpu_allowed),
                      std::move(*domains)};
}

size_t SmcuTopology::domain_for_cpu(size_t cpu) const {
  if (cpu >= cpu_to_domain_.size()) {
    return kNoSmcuDomain;
  }
  return cpu_to_domain_[cpu];
}

bool SmcuTopology::is_cpu_allowed(size_t cpu) const {
  return cpu < cpu_allowed_.size() && cpu_allowed_[cpu];
}

bool SmcuTopology::has_allowed_cpu() const {
  return std::any_of(cpu_allowed_.begin(), cpu_allowed_.end(),
                     [](auto allowed) { return allowed; });
}

unsigned SmcuTopology::slot_count(size_t domain) const {
  return domain < domains_.size() ? domains_[domain].slot_count : 0;
}

SmeThreadLimiter::SmeThreadLimiter(SmcuTopology topology)
    : topology_{std::move(topology)} {
  if (!available_slots_.allocate(topology_.domain_count())) {
    return;
  }
  for (size_t domain = 0; domain < topology_.domain_count(); ++domain) {
    available_slots_[domain].store(topology_.slot_count(domain),
                                   std::memory_order_relaxed);
  }
}

namespace {

SmeThreadLimiter *create_sme_thread_limiter() {
  auto topology = SmcuTopology::detect();
  if (!topology || !topology->has_allowed_cpu()) {
    return nullptr;
  }

  void *storage = std::malloc(sizeof(SmeThreadLimiter));
  if (!storage) {
    return nullptr;
  }
  return ::new (storage) SmeThreadLimiter{std::move(*topology)};
}

// This process-lifetime instance avoids C++ static destruction. Allocation
// failure safely leaves all work on the CPU backend.
const SmeThreadLimiter *const kSmeThreadLimiter = create_sme_thread_limiter();

}  // namespace

const SmeThreadLimiter *SmeThreadLimiter::instance() {
  return kSmeThreadLimiter;
}

int SmeThreadLimiter::current_cpu() { return sched_getcpu(); }

bool SmeThreadLimiter::try_acquire(size_t domain) const {
  if (domain >= available_slots_.size()) {
    return false;
  }

  std::atomic<unsigned> &available = available_slots_[domain];
  unsigned slots = available.load(std::memory_order_relaxed);
  while (slots > 0 && !available.compare_exchange_weak(
                          slots, slots - 1, std::memory_order_relaxed)) {
  }
  return slots > 0;
}

}  // namespace kleidicv::thread_internal

#endif  // KLEIDICV_ENABLE_SME_THREAD_DISPATCH
