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

# ------------------------------------------------------------------------------
# Mandatory arguments check
# ------------------------------------------------------------------------------

if [[ -z "${LLVM_COV}" ]]; then
    echo "Required variable 'LLVM_COV' is not set. Please set it to point to llvm-cov command."
    exit 1
fi

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

pushd "${coverage_path}" || exit 1

gcovr \
    --gcov-executable "${LLVM_COV} gcov" \
    --root "${build_path}" \
    --filter "${source_path}" \
    --gcov-filter "${build_path}" \
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

popd || exit 1

tar -czf "${build_path}"/coverage.tar.gz -C "${build_path}" coverage

# ------------------------------------------------------------------------------
# End of script
# ------------------------------------------------------------------------------
