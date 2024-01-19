#!/usr/bin/env bash
#
# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exu

# Ensure we're at the root of the repo.
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Ensure we're doing a clean build
rm -rf build

# Check format of C++ files
CHECK_ONLY=ON VERBOSE=ON scripts/format.sh

# Check license headers
reuse lint

# Build and run tests
cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_CLANG_TIDY=clang-tidy \
  -DCMAKE_CXX_FLAGS="--coverage -g -O0"

# Workaround to avoid applying clang-tidy to files in build directory
echo '{"Checks": "-*,cppcoreguidelines-avoid-goto"}'>build/.clang-tidy

GTEST_OUTPUT=xml:$(pwd)/build/test-results/ \
  ninja -C build check-intrinsiccv

# Generate test coverage report
gcovr \
  -j \
  --gcov-executable "llvm-cov gcov" \
  --exclude-noncode-lines \
  --cobertura build/cobertura-coverage.xml \
  --print-summary \
  --exclude build \
  build
