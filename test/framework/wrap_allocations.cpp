// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdlib>

#include "framework/utils.h"

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
extern "C" void* __real_malloc(size_t);
extern "C" void* __real_calloc(size_t, size_t);

extern "C" void* __wrap_malloc(size_t size) {
  if (AllocationFailureMock::is_enabled()) {
    return nullptr;
  }

  return __real_malloc(size);
}

extern "C" void* __wrap_calloc(size_t count, size_t size) {
  if (AllocationFailureMock::is_enabled()) {
    return nullptr;
  }

  return __real_calloc(count, size);
}
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
