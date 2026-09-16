#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exuo pipefail

# Ensure we're at the root of the repo.
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Use the same AArch64 target for native and cross-compiled builds.
AARCH64_TARGET_TRIPLE=aarch64-linux-gnu

if ! command -v qemu-aarch64; then
  apt-get update
  apt-get -y --no-install-recommends install qemu-user
fi

# Force ccache for all CMake builds.
export CMAKE_CXX_COMPILER_LAUNCHER=ccache

# Ensure we're doing clean builds.
rm -rf build/ci/build-benchmark build/ci/extract_example

# Build benchmarks without the continuous load/store code path to prevent bitrot.
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
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -DCMAKE_C_COMPILER_TARGET="${AARCH64_TARGET_TRIPLE}" \
  -DCMAKE_CXX_COMPILER_TARGET="${AARCH64_TARGET_TRIPLE}" \
  -DCMAKE_EXE_LINKER_FLAGS="--rtlib=compiler-rt --unwindlib=libunwind -fuse-ld=lld" \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64
ninja -C build/ci/extract_example
