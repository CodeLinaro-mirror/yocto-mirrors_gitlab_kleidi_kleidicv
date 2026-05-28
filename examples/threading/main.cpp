// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "kleidicv/kleidicv.h"
#include "kleidicv_thread/kleidicv_thread.h"

// This example shows how to use KleidiCV threading with std::thread as the
// application's threading model.

namespace {

struct ThreadingContext {
  unsigned thread_count;
};

bool parse_thread_count(const char *text, unsigned *thread_count) {
  errno = 0;
  char *end = nullptr;
  // Parse only base-10 values accepted fully by strtoul.
  uint64_t parsed = std::strtoul(text, &end, 10);

  // Reject conversion errors, empty input, trailing characters, zero, and
  // values that do not fit in the output type.
  if (errno != 0 || end == text || *end != '\0' || parsed == 0 ||
      parsed > UINT_MAX) {
    return false;
  }

  *thread_count = static_cast<unsigned>(parsed);
  return true;
}

void join_workers(std::vector<std::thread> *workers) {
  for (std::thread &worker : *workers) {
    // join() waits until the worker thread has finished. Every std::thread
    // created by this example must be joined before the function returns.
    worker.join();
  }
}

kleidicv_error_t parallel(kleidicv_thread_callback callback,
                          void *callback_data, void *parallel_data,
                          unsigned task_count) {
  // KleidiCV calls this function when a threaded operation has work to run.
  // task_count is the number of independent task indices KleidiCV created.
  const auto *context = static_cast<const ThreadingContext *>(parallel_data);
  if (callback == nullptr || context == nullptr || context->thread_count == 0) {
    return KLEIDICV_ERROR_NULL_POINTER;
  }

  // Do not create more workers than tasks, otherwise some workers would have no
  // range to execute.
  const unsigned worker_count = std::min(context->thread_count, task_count);
  std::vector<std::thread> workers;
  std::vector<kleidicv_error_t> results(worker_count, KLEIDICV_OK);
  workers.reserve(worker_count);

  for (unsigned index = 0; index < worker_count; ++index) {
    // Split the KleidiCV task index range evenly between workers. Each worker
    // receives the half-open range [begin, end).
    const unsigned begin = index * task_count / worker_count;
    const unsigned end = (index + 1 == worker_count)
                             ? task_count
                             : (index + 1) * task_count / worker_count;

    // emplace_back constructs a std::thread in the vector. The new thread
    // starts immediately and calls the KleidiCV callback for its range.
    workers.emplace_back([=, &results] {
      results[index] = callback(begin, end, callback_data);
    });
  }

  join_workers(&workers);

  for (kleidicv_error_t result : results) {
    if (result != KLEIDICV_OK) {
      return result;
    }
  }

  return KLEIDICV_OK;
}

}  // namespace

int main(int argc, char **argv) {
  // Use the hardware thread count as a default. The user can override it with
  // the optional command-line argument.
  unsigned thread_count = std::thread::hardware_concurrency();
  if (thread_count == 0) {
    thread_count = 1;
  }

  if (argc > 2 || (argc == 2 && !parse_thread_count(argv[1], &thread_count))) {
    std::cerr << "Usage: " << argv[0] << " [thread-count]\n";
    return EXIT_FAILURE;
  }

  constexpr size_t width = 1024;
  constexpr size_t height = 1024;
  constexpr size_t element_count = width * height;

  std::vector<uint8_t> a(element_count);
  std::vector<uint8_t> b(element_count);
  std::vector<uint8_t> threaded_output(element_count, 0);
  std::vector<uint8_t> reference_output(element_count, 0);

  for (size_t index = 0; index < element_count; ++index) {
    if (index % 4 == 0) {
      a[index] = static_cast<uint8_t>(200 + ((index * 17 + 3) % 56));
      b[index] = static_cast<uint8_t>(200 + ((index * 29 + 11) % 56));
    } else {
      a[index] = static_cast<uint8_t>((index * 17 + 3) % 121);
      b[index] = static_cast<uint8_t>((index * 29 + 11) % 121);
    }
  }

  ThreadingContext context{thread_count};
  // This structure tells KleidiCV which scheduler callback to use and passes
  // the scheduler state back to that callback.
  kleidicv_thread_multithreading mt{parallel, &context};

  // Run the normal KleidiCV API first. This is not part of the threading model;
  // it provides a reference output to check the threaded call below.
  kleidicv_error_t result = kleidicv_saturating_add_u8(
      a.data(), width * sizeof(a[0]), b.data(), width * sizeof(b[0]),
      reference_output.data(), width * sizeof(reference_output[0]), width,
      height);

  if (result != KLEIDICV_OK) {
    std::cerr << "KleidiCV call failed: " << result << '\n';
    return EXIT_FAILURE;
  }

  // Run the threaded API with the same inputs and the user-provided scheduler.
  result = kleidicv_thread_saturating_add_u8(
      a.data(), width * sizeof(a[0]), b.data(), width * sizeof(b[0]),
      threaded_output.data(), width * sizeof(threaded_output[0]), width, height,
      mt);

  if (result != KLEIDICV_OK) {
    std::cerr << "KleidiCV threaded call failed: " << result << '\n';
    return EXIT_FAILURE;
  }

  for (size_t index = 0; index < element_count; ++index) {
    if (threaded_output[index] != reference_output[index]) {
      std::cerr << "Unexpected result at index " << index << '\n';
      return EXIT_FAILURE;
    }
  }

  std::cout << "Success: processed " << width << 'x' << height
            << " elements with " << thread_count
            << " requested worker thread(s)\n";

  return EXIT_SUCCESS;
}
