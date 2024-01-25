#!/usr/bin/env bash
#
# SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
# Collects coverage data and generates Cobertura & HTML coverage reports.
#
# Arguments
#   1: Path to build folder. Defaults to default build folder.

set -eu

# ------------------------------------------------------------------------------
# Mandatory arguments check
# ------------------------------------------------------------------------------

if [[ -z "${LLVM_COV+x}" ]]; then
    echo "Required variable 'LLVM_COV' is not set." \
         "Please set it to point to an llvm-cov instance which is compatible" \
         "with the toolchain the binaries were built with."
    exit 1
fi

set -x

# ------------------------------------------------------------------------------
# Automatic configuration
# ------------------------------------------------------------------------------

SCRIPT_PATH="$(realpath "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")"

source_path="$(realpath "${SCRIPT_PATH}"/..)"
build_path="$(realpath "${1:-${source_path}/build}")"
coverage_path="${build_path}"/coverage

# ------------------------------------------------------------------------------

rm -rf "${coverage_path}"
mkdir "${coverage_path}"

cd "${source_path}"

gcovr \
    -j \
    --gcov-executable "${LLVM_COV} gcov" \
    --cobertura "${build_path}/cobertura-coverage.xml" \
    --html-details "${coverage_path}"/coverage_report.html \
    --html-title "IntrinsicCV Coverage Report" \
    --html-details-syntax-highlighting \
    --html-tab-size 2 \
    --decisions \
    --exclude-noncode-lines \
    --exclude "${build_path}" \
    --print-summary \
    "${build_path}"
