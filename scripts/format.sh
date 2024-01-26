#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

# Runs clang-format or checks whether the source is well-formatted.
#
# Options:
#   CHECK_ONLY:             If set to 'ON', the script exists with non-zero value if source is not formatted. Defaults to 'OFF'.
#   CLANG_FORMAT_BIN_PATH:  Clang-format binary, defaults to 'clang-format=17'.
#   VERBOSE:                If set to 'ON', verbose output is printed. Defaults to 'OFF'.
# ------------------------------------------------------------------------------

set -eu

# ------------------------------------------------------------------------------

SCRIPT_PATH="$(realpath "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")"

# ------------------------------------------------------------------------------

INTRINSICCV_ROOT_PATH="$(realpath "${SCRIPT_PATH}"/..)"

: "${CHECK_ONLY:=OFF}"
: "${CLANG_FORMAT_BIN_PATH:=clang-format-17}"
: "${VERBOSE:=OFF}"

# ------------------------------------------------------------------------------

SOURCES="$(find \
    "${INTRINSICCV_ROOT_PATH}"/adapters \
    "${INTRINSICCV_ROOT_PATH}"/intrinsiccv \
    "${INTRINSICCV_ROOT_PATH}"/test \
    \( -name \*.cpp -o -name \*.h \) \
    -print)"

if [[ "${CHECK_ONLY}" == "ON" ]]; then
  FORMAT_FLAGS="--dry-run -Werror"
else
  FORMAT_FLAGS="-i"
fi

if [[ "${VERBOSE}" == "ON" ]]; then
  FORMAT_FLAGS="${FORMAT_FLAGS} --verbose"
fi

# shellcheck disable=2086
# Split ${SOURCES} and ${FORMAT_FLAGS}.
"${CLANG_FORMAT_BIN_PATH}" ${FORMAT_FLAGS} ${SOURCES}

# ------------------------------------------------------------------------------
# End of script
# ------------------------------------------------------------------------------
