// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "multithreading_std_thread.h"

#include <algorithm>
#include <thread>
#include <vector>

#include "kleidicv/kleidicv.h"
#include "kleidicv_thread/kleidicv_thread.h"

/// An implementation of kleidicv_thread_parallel based on std::thread.
static kleidicv_error_t parallel_std_thread(kleidicv_thread_callback callback,
                                            void *callback_data,
                                            void *parallel_data,
                                            unsigned task_count) {
  const unsigned max_thread_count =
      static_cast<unsigned>(reinterpret_cast<uintptr_t>(parallel_data));
  const unsigned thread_count = std::min(task_count, max_thread_count);

  std::vector<kleidicv_error_t> results(thread_count,
                                        KLEIDICV_ERROR_CONTEXT_MISMATCH);

  std::vector<std::thread> threads(thread_count);
  for (unsigned i = 0; i < thread_count; ++i) {
    unsigned task_begin = i * task_count / thread_count;
    unsigned task_end = i + 1 == thread_count
                            ? task_count
                            : (i + 1) * task_count / thread_count;

    threads[i] = std::thread([=, &results]() {
      results[i] = callback(task_begin, task_end, callback_data);
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  for (auto r : results) {
    if (r != KLEIDICV_OK) {
      return r;
    }
  }
  return KLEIDICV_OK;
}

kleidicv_thread_multithreading get_multithreading_std_thread(
    uintptr_t thread_count) {
  // NOLINTBEGIN(performance-no-int-to-ptr)
  void *parallel_data = reinterpret_cast<void *>(thread_count);
  // NOLINTEND(performance-no-int-to-ptr)
  return kleidicv_thread_multithreading{parallel_std_thread, parallel_data};
}
