#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exuo pipefail

# Ensure we're at the root of the repo.
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Sanitizer runs require native AArch64 hardware.
if [[ $(dpkg --print-architecture) != arm64 ]]; then
  exit 0
fi

if ! command -v qemu-aarch64; then
  apt-get update
  apt-get -y --no-install-recommends install qemu-user
fi

# Force ccache for all CMake builds.
export CMAKE_CXX_COMPILER_LAUNCHER=ccache

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

# Ensure we're doing clean builds.
SANITIZE_NEON_DIR=build/ci/sanitize-neon
SANITIZE_SCALABLE_DIR=build/ci/sanitize-scalable
SANITIZE_SME_DIR=build/ci/sanitize-sme
rm -rf "${SANITIZE_NEON_DIR}" "${SANITIZE_SCALABLE_DIR}" "${SANITIZE_SME_DIR}"

# Build Neon implementation.
SANITIZE_NEON_TEST="${SANITIZE_NEON_DIR}/test/api/kleidicv-api-test"
cmake -S . -B "${SANITIZE_NEON_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -DKLEIDICV_ENABLE_SVE2=OFF \
  -DKLEIDICV_ENABLE_SME=OFF \
  -DKLEIDICV_ENABLE_SME2=OFF \
  -DCMAKE_CXX_FLAGS="${SANITIZE_FLAGS}"
ninja -C "${SANITIZE_NEON_DIR}" kleidicv-api-test

# Build SVE2, SME and SME2 implementations.
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
SANITIZE_SME_TEST="${SANITIZE_SME_DIR}/test/api/kleidicv-api-test"
cmake -S . -B "${SANITIZE_SME_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -DKLEIDICV_ENABLE_SVE2=OFF \
  -DKLEIDICV_ENABLE_SME=ON \
  -DKLEIDICV_ENABLE_SME2=OFF \
  -DCMAKE_CXX_FLAGS="${SANITIZE_FLAGS}"
ninja -C "${SANITIZE_SME_DIR}" kleidicv-api-test

# Run tests in parallel.
TESTRESULT=0
PIDS=()

# Neon: use native hardware.
"${SANITIZE_NEON_TEST}" --vector-length=16 &
PIDS+=("$!")

# SVE2 VL128: use native hardware when available, otherwise use QEMU.
if [[ -r /proc/cpuinfo ]] && grep -qw sve2 /proc/cpuinfo; then
  KLEIDICV_PREFER_SME_BACKEND=OFF \
    "${SANITIZE_SCALABLE_TEST}" --vector-length=16 &
else
  ASAN_OPTIONS="${QEMU_ASAN_OPTIONS}" KLEIDICV_PREFER_SME_BACKEND=OFF \
    qemu-aarch64 -cpu max,sve128=on,sve-default-vector-length=16,sme=off \
    "${SANITIZE_SCALABLE_TEST}" --vector-length=16 &
fi
PIDS+=("$!")

# SVE2 VL256: run through QEMU.
ASAN_OPTIONS="${QEMU_ASAN_OPTIONS}" KLEIDICV_PREFER_SME_BACKEND=OFF \
  qemu-aarch64 -cpu max,sve256=on,sve-default-vector-length=32,sme=off \
  "${SANITIZE_SCALABLE_TEST}" --vector-length=32 &
PIDS+=("$!")

# SME and SME2: run their isolated binaries through QEMU.
QEMU_SME_CPU="max,sve128=on,sve-default-vector-length=16,sme512=on,sme-default-vector-length=64"
ASAN_OPTIONS="${QEMU_ASAN_OPTIONS}" KLEIDICV_PREFER_SME_BACKEND=ON \
  qemu-aarch64 -cpu "${QEMU_SME_CPU}" \
  "${SANITIZE_SME_TEST}" --vector-length=64 &
PIDS+=("$!")

ASAN_OPTIONS="${QEMU_ASAN_OPTIONS}" KLEIDICV_PREFER_SME_BACKEND=ON \
  qemu-aarch64 -cpu "${QEMU_SME_CPU}" \
  "${SANITIZE_SCALABLE_TEST}" --vector-length=64 &
PIDS+=("$!")

for PID in "${PIDS[@]}"; do
  wait "${PID}" || TESTRESULT=1
done

exit "${TESTRESULT}"
