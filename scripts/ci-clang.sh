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
rm -rf build/ci
mkdir -p build/ci

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

scripts/prefix_testsuite_names.py build/ci/test-results/clang-unit-neon/kleidicv-neon-unit-test.xml "clang-unit-neon."
scripts/prefix_testsuite_names.py build/ci/test-results/clang-neon/kleidicv-api-test.xml "clang-neon."
scripts/prefix_testsuite_names.py build/ci/test-results/clang-sve128/kleidicv-api-test.xml "clang-sve128."
scripts/prefix_testsuite_names.py build/ci/test-results/clang-sve2048/kleidicv-api-test.xml "clang-sve2048."
scripts/prefix_testsuite_names.py build/ci/test-results/clang-sme/kleidicv-api-test.xml "clang-sme."
scripts/prefix_testsuite_names.py build/ci/test-results/clang-sme-api/kleidicv-api-test.xml "clang-sme-api."
scripts/prefix_testsuite_names.py build/ci/test-results/clang-sme2/kleidicv-api-test.xml "clang-sme2."

# Generate test coverage report.
LLVM_COV=llvm-cov scripts/generate_coverage_report.py build/ci/clang | tee build/ci/coverage-summary.txt
mkdir -p build/ci/html/coverage
mv build/ci/clang/html/coverage build/ci/html

# Run sanitizers on native hardware where possible. Use QEMU for unavailable
# backends or vector lengths.
if [[ $(dpkg --print-architecture) = arm64 ]]; then
  SANITIZE_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -Wno-pass-failed"

  # LeakSanitizer stops all process threads with an internal ptrace-based
  # tracer before checking for unreachable allocations. QEMU user-mode cannot
  # provide the guest thread semantics required by that tracer, so LSan exits
  # with "Failed spawning a tracer thread (errno 22)" after all tests pass.
  # This limitation is present in both QEMU 8.2 on Ubuntu 24.04 and QEMU 10.2
  # on Ubuntu 26.04. It was not visible with the previous emulator.
  #
  # Keep leak detection enabled for every native sanitizer run. Disable only
  # LSan's unsupported exit-time leak scan for commands executed under QEMU;
  # AddressSanitizer and UndefinedBehaviorSanitizer remain fully enabled and
  # still fail the job when they find an error.
  QEMU_ASAN_OPTIONS="${ASAN_OPTIONS-}"
  if [[ -n ${QEMU_ASAN_OPTIONS} ]]; then
    QEMU_ASAN_OPTIONS+=:
  fi
  QEMU_ASAN_OPTIONS+=detect_leaks=0

  # Neon: build without scalable backends and run natively.
  SANITIZE_NEON_DIR=build/ci/sanitize-neon
  SANITIZE_NEON_TEST="${SANITIZE_NEON_DIR}/test/api/kleidicv-api-test"
  cmake -S . -B "${SANITIZE_NEON_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
    -DKLEIDICV_ENABLE_SVE2=OFF \
    -DKLEIDICV_ENABLE_SME=OFF \
    -DKLEIDICV_ENABLE_SME2=OFF \
    -DCMAKE_CXX_FLAGS="${SANITIZE_FLAGS}"
  ninja -C "${SANITIZE_NEON_DIR}" kleidicv-api-test
  "${SANITIZE_NEON_TEST}" --vector-length=16

  # Build SVE2, SME and SME2 implementations.
  SANITIZE_SCALABLE_DIR=build/ci/sanitize-scalable
  SANITIZE_SCALABLE_TEST="${SANITIZE_SCALABLE_DIR}/test/api/kleidicv-api-test"
  cmake -S . -B "${SANITIZE_SCALABLE_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
    -DKLEIDICV_ENABLE_SVE2=ON \
    -DKLEIDICV_LIMIT_SVE2_TO_SELECTED_ALGORITHMS=OFF \
    -DKLEIDICV_ENABLE_SME=ON \
    -DKLEIDICV_ENABLE_SME2=ON \
    -DKLEIDICV_LIMIT_SME2_TO_SELECTED_ALGORITHMS=OFF \
    -DCMAKE_CXX_FLAGS="${SANITIZE_FLAGS}"
  ninja -C "${SANITIZE_SCALABLE_DIR}" kleidicv-api-test

  # Keep SME separate from SME2 because QEMU advertises both features and
  # KleidiCV dispatch prefers SME2 when both implementations are present.
  SANITIZE_SME_DIR=build/ci/sanitize-sme
  SANITIZE_SME_TEST="${SANITIZE_SME_DIR}/test/api/kleidicv-api-test"
  cmake -S . -B "${SANITIZE_SME_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
    -DKLEIDICV_ENABLE_SVE2=OFF \
    -DKLEIDICV_ENABLE_SME=ON \
    -DKLEIDICV_ENABLE_SME2=OFF \
    -DCMAKE_CXX_FLAGS="${SANITIZE_FLAGS}"
  ninja -C "${SANITIZE_SME_DIR}" kleidicv-api-test

  # SVE2 VL128: use native hardware when available, otherwise use QEMU.
  if [[ -r /proc/cpuinfo ]] && grep -qw sve2 /proc/cpuinfo; then
    KLEIDICV_PREFER_SME_BACKEND=OFF \
      "${SANITIZE_SCALABLE_TEST}" --vector-length=16
  else
    ASAN_OPTIONS="${QEMU_ASAN_OPTIONS}" KLEIDICV_PREFER_SME_BACKEND=OFF \
      qemu-aarch64 -cpu max,sve128=on,sve-default-vector-length=16,sme=off \
      "${SANITIZE_SCALABLE_TEST}" --vector-length=16
  fi

  # SVE2 VL256: run through QEMU.
  ASAN_OPTIONS="${QEMU_ASAN_OPTIONS}" KLEIDICV_PREFER_SME_BACKEND=OFF \
    qemu-aarch64 -cpu max,sve256=on,sve-default-vector-length=32,sme=off \
    "${SANITIZE_SCALABLE_TEST}" --vector-length=32

  # SME and SME2: run their isolated binaries through QEMU.
  ASAN_OPTIONS="${QEMU_ASAN_OPTIONS}" KLEIDICV_PREFER_SME_BACKEND=ON \
    qemu-aarch64 -cpu "${QEMU_SME_CPU}" \
    "${SANITIZE_SME_TEST}" --vector-length=64

  ASAN_OPTIONS="${QEMU_ASAN_OPTIONS}" KLEIDICV_PREFER_SME_BACKEND=ON \
    qemu-aarch64 -cpu "${QEMU_SME_CPU}" \
    "${SANITIZE_SCALABLE_TEST}" --vector-length=64
fi

# Build benchmarks and without continuous load/store code path, just to prevent bitrot.
cmake -S . -B build/ci/build-benchmark -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -DCMAKE_CROSSCOMPILING_EMULATOR=qemu-aarch64 \
  -DCMAKE_C_COMPILER_TARGET="${AARCH64_TARGET_TRIPLE}" \
  -DCMAKE_CXX_COMPILER_TARGET="${AARCH64_TARGET_TRIPLE}" \
  -DCMAKE_EXE_LINKER_FLAGS="--rtlib=compiler-rt --unwindlib=libunwind -static -fuse-ld=lld" \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DKLEIDICV_BENCHMARK=ON \
  -DKLEIDICV_ENABLE_SME=ON \
  -DKLEIDICV_ENABLE_SME2=ON \
  -DKLEIDICV_LIMIT_SME2_TO_SELECTED_ALGORITHMS=OFF \
  -DKLEIDICV_LIMIT_SVE2_TO_SELECTED_ALGORITHMS=OFF \
  -DKLEIDICV_NEON_USE_CONTINUOUS_MULTIVEC_LS=OFF
ninja -C build/ci/build-benchmark kleidicv-benchmark

# Build examples to prevent bitrot.
cmake -S ./examples/extract_one_operation -B build/ci/extract_example -G Ninja \
  -DCMAKE_C_COMPILER_TARGET="${AARCH64_TARGET_TRIPLE}" \
  -DCMAKE_CXX_COMPILER_TARGET="${AARCH64_TARGET_TRIPLE}" \
  -DCMAKE_EXE_LINKER_FLAGS="--rtlib=compiler-rt --unwindlib=libunwind -fuse-ld=lld"
ninja -C build/ci/extract_example

# Generate documentation.
doxygen

exit $TESTRESULT
