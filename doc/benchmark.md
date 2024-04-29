<!--
SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# Benchmarking KleidiCV

Building of KleidiCV's benchmarks can be enabled with the `cmake` argument `-DKLEIDICV_BENCHMARK=ON`.
The benchmarks are based on [Google Benchmark](https://github.com/google/benchmark).
All the standard Google Benchmark command line arguments can be used. In addition,
the image size on which the tests will be run can be set with the command line
options `--image_width=123` and `--image_height=123`.
