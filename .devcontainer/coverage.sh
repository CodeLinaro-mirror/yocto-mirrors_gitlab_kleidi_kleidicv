#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -eu

BUILD_ID="kleidicv-coverage" \
COVERAGE="ON" \
CMAKE_CXX_FLAGS="--target=aarch64-linux-gnu" \
CMAKE_EXE_LINKER_FLAGS="--rtlib=compiler-rt -static -fuse-ld=lld" \
EXTRA_CMAKE_ARGS="-DKLEIDICV_ENABLE_SME2=ON -DKLEIDICV_LIMIT_SME2_TO_SELECTED_ALGORITHMS=OFF -DKLEIDICV_LIMIT_SVE2_TO_SELECTED_ALGORITHMS=OFF" \
./scripts/build.sh kleidicv-test

# Clean any coverage results from previous runs
find build/kleidicv-coverage/ -type f -name *.gcda -delete

LONG_VECTOR_TESTS="GRAY2.*:RGB*:Yuv*:Rgb*"

qemu-aarch64 build/kleidicv-coverage/test/framework/kleidicv-framework-test
qemu-aarch64 -cpu cortex-a35 build/kleidicv-coverage/test/api/kleidicv-api-test
qemu-aarch64 -cpu max,sve128=on,sme=off build/kleidicv-coverage/test/api/kleidicv-api-test --vector-length=16
qemu-aarch64 -cpu max,sve2048=on,sve-default-vector-length=256,sme=off \
  build/kleidicv-coverage/test/api/kleidicv-api-test --gtest_filter="${LONG_VECTOR_TESTS}" --vector-length=256
qemu-aarch64 -cpu max,sve128=on,sme512=on build/kleidicv-coverage/test/api/kleidicv-api-test --vector-length=64

# Generate test coverage report
LLVM_COV=llvm-cov scripts/generate_coverage_report.py
