<!--
SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# KleidiCV threading example

This example shows how an application can run a KleidiCV threaded operation.
The application provides a `kleidicv_thread_multithreading` value with a
scheduler callback. KleidiCV calls this scheduler with a task count and a
callback. The scheduler starts worker threads and calls the callback for each
task range.

For more about using the threaded API, see [Threading](../../doc/threading.md).

The example keeps the scheduler deliberately simple. It accepts the maximum
number of worker threads as a command-line argument and splits the task range
evenly between those workers.

Run the example with an optional worker count:

```{lang="text"}
kleidicv-threading-example [thread-count]
```

If the example is built natively, run the executable directly:

```{lang="text"}
build/kleidicv-threading-example/examples/threading/kleidicv-threading-example 8
```

If the example is cross-built for AArch64 with the VS Code task
`Build threading example`, run it through QEMU:

```{lang="text"}
qemu-aarch64 build/kleidicv-threading-example/examples/threading/kleidicv-threading-example 8
```

If no thread count is provided, the example uses
`std::thread::hardware_concurrency()` with a fallback to one worker. The worker
count must be a positive integer.

The operation uses two `1024x1024` `uint8_t` input images. The inputs are filled
with deterministic pseudo-random values that include both non-saturating and
saturating additions. The example runs the normal KleidiCV operation and the
threaded KleidiCV operation with the same inputs, then verifies that every
output element matches before printing a success message.

For production schedulers, equal splitting is not always ideal. Better
performance may be achieved by assigning larger chunks first and then smaller
chunks as the remaining work decreases. This can allow faster workers to take
more work and slower workers to finish sooner, which is useful on systems with a
mix of big, mid, and small CPU cores.
