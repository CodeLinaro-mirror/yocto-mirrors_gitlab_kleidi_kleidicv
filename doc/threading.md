<!--
SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# Threading

KleidiCV provides threaded variants of selected C API functions through
`kleidicv_thread/include/kleidicv_thread/kleidicv_thread.h`.

To use these functions:

- include `kleidicv_thread/kleidicv_thread.h`;
- link with `kleidicv_thread` and `kleidicv`;
- provide a `kleidicv_thread_multithreading` value for each threaded call.

## Execution model {.section}

KleidiCV does not create worker threads, own a global thread pool, or choose the
number of worker threads. The application supplies the scheduling function
through `kleidicv_thread_multithreading`:

```{lang="c"}
kleidicv_thread_multithreading mt = {
    parallel_function,
    parallel_data,
};
```

`parallel_function` is implemented by the application. `parallel_data` is
optional application-owned context, for example a thread pool or scheduler
object.

During a threaded operation, KleidiCV creates an operation-specific work
callback and calls:

```{lang="c"}
mt.parallel(callback, callback_data, mt.parallel_data, task_count);
```

At that point:

- `callback` is provided by KleidiCV and performs the actual image-processing
  work for a range of task indices;
- `callback_data` is private KleidiCV data and must be passed back unchanged;
- `task_count` is the number of KleidiCV work items, not the number of worker
  threads.

The application-provided scheduler decides how to run those work items. It may
run them serially, split them over a thread pool, or use another task framework.

## Scheduler responsibilities {.section}

The `kleidicv_thread_parallel` implementation is responsible for:

- splitting the task index range from 0 up to `task_count` into one or more task ranges;
- calling `callback(begin, end, callback_data)` for those ranges;
- ensuring each non-empty range satisfies `0 <= begin < end <= task_count`;
- covering each task index exactly once;
- waiting for all submitted work to finish before returning;
- returning a non-`KLEIDICV_OK` value if any callback call fails;
- not retaining `callback` or `callback_data` after returning;
- keeping any data referenced by `parallel_data` valid until the
  `kleidicv_thread_*` function returns.

## Serial scheduler {.section}

A serial scheduler is valid. It executes the whole KleidiCV task range in one
callback call:

```{lang="c"}
#include "kleidicv_thread/kleidicv_thread.h"

static kleidicv_error_t serial_parallel(
    kleidicv_thread_callback callback, void *callback_data,
    void *parallel_data, unsigned task_count) {
  (void)parallel_data;
  return callback(0, task_count, callback_data);
}

static kleidicv_thread_multithreading make_serial_multithreading(void) {
  kleidicv_thread_multithreading mt = {serial_parallel, NULL};
  return mt;
}
```

Production integrations usually map task ranges to an existing scheduler. The
OpenCV adapter in `adapters/opencv/kleidicv_hal.cpp` is an in-tree example that
adapts `cv::parallel_for_` to `kleidicv_thread_parallel`.

## Calling threaded operations {.section}

Threaded functions mirror the corresponding single-threaded `kleidicv_*`
operation and add a final `kleidicv_thread_multithreading` argument. The
operation-specific image, stride, size, type, in-place, and error contracts are
the same as the corresponding single-threaded function unless the threaded
function documents a different contract.

For example, use `kleidicv_thread_min_max_u8` instead of
`kleidicv_min_max_u8` when the operation should run through the supplied
threading backend:

```{lang="c"}
kleidicv_thread_multithreading mt = make_serial_multithreading();

kleidicv_error_t result =
    kleidicv_thread_min_max_u8(src, src_stride, width, height, &min_value,
                               &max_value, mt);
```

The returned `kleidicv_error_t` is the operation result.
