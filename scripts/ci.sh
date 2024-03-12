#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exu

# Ensure we're at the root of the repo.
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Ensure we're doing a clean build
rm -rf build

apt-get -y --no-install-recommends install qemu-user

# Check format of C++ files
CHECK_ONLY=ON VERBOSE=ON scripts/format.sh

scripts/cpplint.sh

# Check format of shell scripts
shellcheck scripts/*.sh

# Check license headers
reuse lint

# Generate documentation
doxygen

# Build
cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_CLANG_TIDY=clang-tidy \
  -DCMAKE_CXX_FLAGS="--target=aarch64-linux-gnu --coverage" \
  -DINTRINSICCV_ENABLE_SVE2=ON \
  -DINTRINSICCV_ENABLE_SVE2_SELECTIVELY=OFF \
  -DINTRINSICCV_CHECK_BANNED_FUNCTIONS=ON

# Workaround to avoid applying clang-tidy to files in build directory
echo '{"Checks": "-*,cppcoreguidelines-avoid-goto"}'>build/.clang-tidy

ninja -C build

# Build with GCC
CC=aarch64-linux-gnu-gcc CXX=aarch64-linux-gnu-g++ cmake -S . -B build/gcc -G Ninja
ninja -C build/gcc

# Run tests
LONG_VECTOR_TESTS="GRAY2.*:RGB*"
TESTRESULT=0
qemu-aarch64     build/test/framework/intrinsiccv-framework-test --gtest_output=xml:build/test-results/ || TESTRESULT=1
qemu-aarch64 -cpu cortex-a35 build/test/api/intrinsiccv-api-test --gtest_output=xml:build/test-results/clang-neon/ || TESTRESULT=1
qemu-aarch64 -cpu max,sve128=on,sme=off \
  build/test/api/intrinsiccv-api-test --gtest_output=xml:build/test-results/clang-sve128/ --vector-length=16 || TESTRESULT=1
qemu-aarch64 -cpu max,sve2048=on,sve-default-vector-length=256,sme=off \
  build/test/api/intrinsiccv-api-test --gtest_filter="${LONG_VECTOR_TESTS}" --gtest_output=xml:build/test-results/clang-sve2048/ --vector-length=256 || TESTRESULT=1
qemu-aarch64 -cpu max,sve128=on,sme512=on \
  build/test/api/intrinsiccv-api-test --gtest_output=xml:build/test-results/clang-sme/ --vector-length=64 || TESTRESULT=1
qemu-aarch64 -cpu cortex-a35 build/gcc/test/api/intrinsiccv-api-test --gtest_output=xml:build/test-results/gcc-neon/ || TESTRESULT=1

scripts/prefix_testsuite_names.py build/test-results/clang-neon/intrinsiccv-api-test.xml "clang-neon."
scripts/prefix_testsuite_names.py build/test-results/clang-sve128/intrinsiccv-api-test.xml "clang-sve128."
scripts/prefix_testsuite_names.py build/test-results/clang-sve2048/intrinsiccv-api-test.xml "clang-sve2048."
scripts/prefix_testsuite_names.py build/test-results/clang-sme/intrinsiccv-api-test.xml "clang-sme."
scripts/prefix_testsuite_names.py build/test-results/gcc-neon/intrinsiccv-api-test.xml "gcc-neon."

# Generate test coverage report
LLVM_COV=llvm-cov scripts/generate_coverage_report.py

# Sanitizers don't work when run through qemu so must be run natively.
if [[ $(dpkg --print-architecture) = arm64 ]]; then
  # Clang address & undefined behaviour sanitizers
  cmake -S . -B build/sanitize -G Ninja \
    -DINTRINSICCV_ENABLE_SME2=OFF \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -Wno-pass-failed"
  ninja -C build/sanitize intrinsiccv-api-test
  build/sanitize/test/api/intrinsiccv-api-test
fi

# TODO: Cross-build OpenCV
if [[ $(dpkg --print-architecture) = arm64 ]]; then
  # Check OpenCV-IntrinsicCV integration
  scripts/ci-opencv.sh
fi

exit $TESTRESULT
