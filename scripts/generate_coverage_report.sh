#!/usr/bin/env bash
#
# SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
# Collects coverage data and generates an HTML coverage report.
#
# Arguments
#   1: Path to build folder. Defaults to default build folder.

set -exu

: "${LLVM_COV:=llvm-cov}"

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

cd "${coverage_path}"

gcovr \
    --gcov-executable "${LLVM_COV} gcov" \
    --root "${build_path}" \
    --filter "${source_path}" \
    --gcov-filter "${build_path}" \
    --cobertura "${build_path}/cobertura-coverage.xml" \
    --html-details "${coverage_path}"/coverage_report.html \
    --html-title "IntrinsicCV Coverage Report" \
    --html-details-syntax-highlighting \
    --html-tab-size 2 \
    --decisions \
    --exclude-noncode-lines \
    --exclude ".*/googletest-build/" \
    --exclude ".*/googletest-src/" \
    --exclude ".*/CompilerIdCXX/" \
    --print-summary \
    -j
