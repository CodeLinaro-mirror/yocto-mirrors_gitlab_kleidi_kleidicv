#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exuo pipefail

# Ensure we're at the root of the repo.
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Use the same AArch64 target for native and cross-compiled builds.
AARCH64_TARGET_TRIPLE=aarch64-linux-gnu

# Ensure we're doing a clean build.
rm -rf build/ci/clang build/ci/test-results/clang-* build/ci/html build/ci/coverage-summary.txt
mkdir -p build/ci/html

if ! command -v qemu-aarch64; then
  apt-get update
  apt-get -y --no-install-recommends install qemu-user
fi

# Force ccache for all CMake builds.
export CMAKE_CXX_COMPILER_LAUNCHER=ccache

# Build with Clang.
cmake -S . -B build/ci/clang -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -DCMAKE_CXX_CLANG_TIDY="clang-tidy-${CLANG_TIDY_VERSION}" \
  -DCMAKE_C_COMPILER_TARGET="${AARCH64_TARGET_TRIPLE}" \
  -DCMAKE_CXX_COMPILER_TARGET="${AARCH64_TARGET_TRIPLE}" \
  -DCMAKE_CXX_FLAGS="-fcoverage-mapping -fprofile-instr-generate" \
  -DCMAKE_EXE_LINKER_FLAGS="--rtlib=compiler-rt --unwindlib=libunwind -static -fuse-ld=lld -fprofile-instr-generate" \
  -DKLEIDICV_ENABLE_SME2=ON \
  -DKLEIDICV_LIMIT_SME2_TO_SELECTED_ALGORITHMS=OFF \
  -DKLEIDICV_ENABLE_SME=ON \
  -DKLEIDICV_LIMIT_SVE2_TO_SELECTED_ALGORITHMS=OFF \
  -DKLEIDICV_CHECK_BANNED_FUNCTIONS=ON

# Workaround to avoid applying clang-tidy to files in external projects.
echo '{"Checks": "-*,cppcoreguidelines-avoid-goto"}'>build/ci/clang/_deps/.clang-tidy

ninja -C build/ci/clang/

# QEMU's max CPU exposes SME2 whenever SME is enabled, and does not provide an
# SME2-off switch. Build a separate SME-only test binary so the SME and SME2
# runs exercise distinct backends.
SME_BUILD_DIR=build/ci/clang/sme-only
cmake -S . -B "${SME_BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -DCMAKE_C_COMPILER_TARGET="${AARCH64_TARGET_TRIPLE}" \
  -DCMAKE_CXX_COMPILER_TARGET="${AARCH64_TARGET_TRIPLE}" \
  -DCMAKE_CXX_FLAGS="-fcoverage-mapping -fprofile-instr-generate" \
  -DCMAKE_EXE_LINKER_FLAGS="--rtlib=compiler-rt --unwindlib=libunwind -static -fuse-ld=lld -fprofile-instr-generate" \
  -DKLEIDICV_ENABLE_SVE2=ON \
  -DKLEIDICV_LIMIT_SVE2_TO_SELECTED_ALGORITHMS=OFF \
  -DKLEIDICV_ENABLE_SME=ON \
  -DKLEIDICV_ENABLE_SME2=OFF
ninja -C "${SME_BUILD_DIR}" kleidicv-api-test

# Run tests on Clang build.
LONG_VECTOR_TESTS="GRAY2.*:RGB*:Yuv*:Rgb*:Resize*"
SME_API_TESTS="SmeApi*"
QEMU_SME_CPU="max,sve128=on,sve-default-vector-length=16,sme512=on,sme-default-vector-length=64"
PROFILE_DIR=build/ci/clang/profiles
TEST_DIR=build/ci/clang/test
SME_TEST_DIR="${SME_BUILD_DIR}/test"
mkdir -p "${PROFILE_DIR}"
TESTRESULT=0
PIDS=()
LLVM_PROFILE_FILE="${PROFILE_DIR}/framework-%p.profraw" qemu-aarch64 ${TEST_DIR}/framework/kleidicv-framework-test --gtest_output=xml:build/ci/test-results/clang-framework/ &
PIDS+=("$!")
LLVM_PROFILE_FILE="${PROFILE_DIR}/unit-thread-%p.profraw" qemu-aarch64 ${TEST_DIR}/unit_thread/kleidicv-thread-unit-test --gtest_output=xml:build/ci/test-results/clang-unit-thread/ &
PIDS+=("$!")
LLVM_PROFILE_FILE="${PROFILE_DIR}/unit-neon-%p.profraw" qemu-aarch64 -cpu cortex-a35 ${TEST_DIR}/unit_neon/kleidicv-neon-unit-test --gtest_output=xml:build/ci/test-results/clang-unit-neon/ &
PIDS+=("$!")
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-neon-%p.profraw" qemu-aarch64 -cpu cortex-a35 ${TEST_DIR}/api/kleidicv-api-test --gtest_output=xml:build/ci/test-results/clang-neon/ &
PIDS+=("$!")
# To test whether the right backend is chosen KLEIDICV_PREFER_SME_BACKEND is set while there is no SME backend.
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-sve128-%p.profraw" KLEIDICV_PREFER_SME_BACKEND=ON qemu-aarch64 -cpu max,sve128=on,sme=off \
  ${TEST_DIR}/api/kleidicv-api-test --gtest_output=xml:build/ci/test-results/clang-sve128/ --vector-length=16 &
PIDS+=("$!")
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-sve2048-%p.profraw" qemu-aarch64 -cpu max,sve2048=on,sve-default-vector-length=256,sme=off \
  ${TEST_DIR}/api/kleidicv-api-test --gtest_filter="${LONG_VECTOR_TESTS}" --gtest_output=xml:build/ci/test-results/clang-sve2048/ --vector-length=256 &
PIDS+=("$!")
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-sme-%p.profraw" KLEIDICV_PREFER_SME_BACKEND=ON qemu-aarch64 -cpu "${QEMU_SME_CPU}" \
  ${SME_TEST_DIR}/api/kleidicv-api-test --gtest_output=xml:build/ci/test-results/clang-sme/ --vector-length=64 &
PIDS+=("$!")
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-sme-api-%p.profraw" KLEIDICV_PREFER_SME_BACKEND=OFF qemu-aarch64 -cpu "${QEMU_SME_CPU}" \
  ${SME_TEST_DIR}/api/kleidicv-api-test --gtest_filter="${SME_API_TESTS}" --gtest_output=xml:build/ci/test-results/clang-sme-api/ --vector-length=64 &
PIDS+=("$!")
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-sme2-%p.profraw" KLEIDICV_PREFER_SME_BACKEND=ON qemu-aarch64 -cpu "${QEMU_SME_CPU}" \
  ${TEST_DIR}/api/kleidicv-api-test --gtest_output=xml:build/ci/test-results/clang-sme2/ --vector-length=64 &
PIDS+=("$!")

for PID in "${PIDS[@]}"; do
  wait "${PID}" || TESTRESULT=1
done

scripts/prefix_testsuite_names.py build/ci/test-results/clang-framework/kleidicv-framework-test.xml "clang-framework." || TESTRESULT=1
scripts/prefix_testsuite_names.py build/ci/test-results/clang-unit-thread/kleidicv-thread-unit-test.xml "clang-unit-thread." || TESTRESULT=1
scripts/prefix_testsuite_names.py build/ci/test-results/clang-unit-neon/kleidicv-neon-unit-test.xml "clang-unit-neon." || TESTRESULT=1
scripts/prefix_testsuite_names.py build/ci/test-results/clang-neon/kleidicv-api-test.xml "clang-neon." || TESTRESULT=1
scripts/prefix_testsuite_names.py build/ci/test-results/clang-sve128/kleidicv-api-test.xml "clang-sve128." || TESTRESULT=1
scripts/prefix_testsuite_names.py build/ci/test-results/clang-sve2048/kleidicv-api-test.xml "clang-sve2048." || TESTRESULT=1
scripts/prefix_testsuite_names.py build/ci/test-results/clang-sme/kleidicv-api-test.xml "clang-sme." || TESTRESULT=1
scripts/prefix_testsuite_names.py build/ci/test-results/clang-sme-api/kleidicv-api-test.xml "clang-sme-api." || TESTRESULT=1
scripts/prefix_testsuite_names.py build/ci/test-results/clang-sme2/kleidicv-api-test.xml "clang-sme2." || TESTRESULT=1

# Generate test coverage report.
LLVM_COV=llvm-cov scripts/generate_coverage_report.py build/ci/clang | tee build/ci/coverage-summary.txt
mv build/ci/clang/html/coverage build/ci/html

# Generate documentation.
doxygen

exit $TESTRESULT
