#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exuo pipefail

# Ensure we're at the root of the repo.
cd "$(dirname "${BASH_SOURCE[0]}")/.."

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
  -DCMAKE_CXX_FLAGS="--target=aarch64-linux-gnu -fcoverage-mapping -fprofile-instr-generate" \
  -DCMAKE_EXE_LINKER_FLAGS="--rtlib=compiler-rt -static -fuse-ld=lld -fprofile-instr-generate" \
  -DKLEIDICV_ENABLE_SME2=ON \
  -DKLEIDICV_LIMIT_SME2_TO_SELECTED_ALGORITHMS=OFF \
  -DKLEIDICV_ENABLE_SME=ON \
  -DKLEIDICV_LIMIT_SVE2_TO_SELECTED_ALGORITHMS=OFF \
  -DKLEIDICV_CHECK_BANNED_FUNCTIONS=ON

# Workaround to avoid applying clang-tidy to files in external projects.
echo '{"Checks": "-*,cppcoreguidelines-avoid-goto"}'>build/ci/clang/_deps/.clang-tidy

ninja -C build/ci/clang/

# Run tests on Clang build.
LONG_VECTOR_TESTS="GRAY2.*:RGB*:Yuv*:Rgb*:Resize*"
SME_API_TESTS="SmeApi*"
PROFILE_DIR=build/ci/clang/profiles
TEST_DIR=build/ci/clang/test
mkdir -p "${PROFILE_DIR}"
TESTRESULT=0
LLVM_PROFILE_FILE="${PROFILE_DIR}/framework-%p.profraw" qemu-aarch64 ${TEST_DIR}/framework/kleidicv-framework-test --gtest_output=xml:build/ci/test-results/clang-framework/ || TESTRESULT=1
LLVM_PROFILE_FILE="${PROFILE_DIR}/unit-neon-%p.profraw" qemu-aarch64 -cpu cortex-a35 ${TEST_DIR}/unit_neon/kleidicv-neon-unit-test --gtest_output=xml:build/ci/test-results/clang-unit-neon/ || TESTRESULT=1
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-neon-%p.profraw" qemu-aarch64 -cpu cortex-a35 ${TEST_DIR}/api/kleidicv-api-test --gtest_output=xml:build/ci/test-results/clang-neon/ || TESTRESULT=1
# To test whether the right backend is chosen KLEIDICV_PREFER_SME_BACKEND is set while there is no SME backend.
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-sve128-%p.profraw" KLEIDICV_PREFER_SME_BACKEND=ON qemu-aarch64 -cpu max,sve128=on,sme=off \
  ${TEST_DIR}/api/kleidicv-api-test --gtest_output=xml:build/ci/test-results/clang-sve128/ --vector-length=16 || TESTRESULT=1
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-sve2048-%p.profraw" qemu-aarch64 -cpu max,sve2048=on,sve-default-vector-length=256,sme=off \
  ${TEST_DIR}/api/kleidicv-api-test --gtest_filter="${LONG_VECTOR_TESTS}" --gtest_output=xml:build/ci/test-results/clang-sve2048/ --vector-length=256 || TESTRESULT=1
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-sme-%p.profraw" KLEIDICV_PREFER_SME_BACKEND=ON qemu-aarch64 -cpu max,sve128=on,sme512=on \
  ${TEST_DIR}/api/kleidicv-api-test --gtest_output=xml:build/ci/test-results/clang-sme/ --vector-length=64 || TESTRESULT=1
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-sme-api-%p.profraw" KLEIDICV_PREFER_SME_BACKEND=OFF qemu-aarch64 -cpu max,sve128=on,sme512=on \
  ${TEST_DIR}/api/kleidicv-api-test --gtest_filter="${SME_API_TESTS}" --gtest_output=xml:build/ci/test-results/clang-sme-api/ --vector-length=64 || TESTRESULT=1
LLVM_PROFILE_FILE="${PROFILE_DIR}/api-sme2-%p.profraw" KLEIDICV_PREFER_SME_BACKEND=ON armie -mvl=16 -msvl=64 -mfeatures=scripts/armie_features.txt \
  ${TEST_DIR}/api/kleidicv-api-test --gtest_output=xml:build/ci/test-results/clang-sme2/ --vector-length=64 || TESTRESULT=1

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

# Run sanitizers on native hardware where possible. Use ArmIE for unavailable
# backends or when runtime dispatch cannot select the required backend.
if [[ $(dpkg --print-architecture) = arm64 ]]; then
  SANITIZE_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -Wno-pass-failed"
  ARMIE_ASAN_OPTIONS=abort_on_error=1:symbolize=0

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

  SANITIZE_SVE2_FEATURES="${SANITIZE_SCALABLE_DIR}/armie_features_sve2.txt"
  sed -e 's/^FEAT_SME=1$/FEAT_SME=0/' \
    -e 's/^FEAT_SME2=1$/FEAT_SME2=0/' \
    -e 's/^FEAT_SVE=0$/FEAT_SVE=1/' \
    -e 's/^FEAT_SVE2=0$/FEAT_SVE2=1/' \
    scripts/armie_features.txt >"${SANITIZE_SVE2_FEATURES}"

  # SVE2 VL128: use native hardware when available, otherwise use ArmIE.
  if [[ -r /proc/cpuinfo ]] && grep -qw sve2 /proc/cpuinfo; then
    KLEIDICV_PREFER_SME_BACKEND=OFF \
      "${SANITIZE_SCALABLE_TEST}" --vector-length=16
  else
    ASAN_OPTIONS="${ARMIE_ASAN_OPTIONS}" \
      KLEIDICV_PREFER_SME_BACKEND=OFF \
      armie -mvl=16 -mfeatures="${SANITIZE_SVE2_FEATURES}" -- \
      "${SANITIZE_SCALABLE_TEST}" --vector-length=16
  fi

  # SVE2 VL256: run through ArmIE.
  ASAN_OPTIONS="${ARMIE_ASAN_OPTIONS}" \
    KLEIDICV_PREFER_SME_BACKEND=OFF \
    armie -mvl=32 -mfeatures="${SANITIZE_SVE2_FEATURES}" -- \
    "${SANITIZE_SCALABLE_TEST}" --vector-length=32

  # SME: disable SME2 and run through ArmIE.
  SANITIZE_SME_FEATURES="${SANITIZE_SCALABLE_DIR}/armie_features_sme.txt"
  sed -e 's/^FEAT_SME2=1$/FEAT_SME2=0/' \
    scripts/armie_features.txt >"${SANITIZE_SME_FEATURES}"

  ASAN_OPTIONS="${ARMIE_ASAN_OPTIONS}" \
    KLEIDICV_PREFER_SME_BACKEND=ON \
    armie -mvl=16 -msvl=64 -mfeatures="${SANITIZE_SME_FEATURES}" -- \
    "${SANITIZE_SCALABLE_TEST}" --vector-length=64

  # SME2: run through ArmIE.
  ASAN_OPTIONS="${ARMIE_ASAN_OPTIONS}" \
    KLEIDICV_PREFER_SME_BACKEND=ON \
    armie -mvl=16 -msvl=64 -mfeatures=scripts/armie_features.txt -- \
    "${SANITIZE_SCALABLE_TEST}" --vector-length=64
fi

# Build benchmarks and without continuous load/store code path, just to prevent bitrot.
cmake -S . -B build/ci/build-benchmark -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -DCMAKE_CROSSCOMPILING_EMULATOR=qemu-aarch64 \
  -DCMAKE_CXX_COMPILER_TARGET=aarch64-linux-gnu \
  -DCMAKE_EXE_LINKER_FLAGS="--rtlib=compiler-rt -static -fuse-ld=lld" \
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
  -DCMAKE_EXE_LINKER_FLAGS="--rtlib=compiler-rt -fuse-ld=lld"
ninja -C build/ci/extract_example

# Generate documentation.
doxygen

exit $TESTRESULT
