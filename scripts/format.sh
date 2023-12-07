#!/usr/bin/env bash
#
# SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -eu

# ------------------------------------------------------------------------------

SCRIPT_PATH="$(realpath "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")"

# ------------------------------------------------------------------------------

INTRINSICCV_ROOT_PATH="$(realpath "${SCRIPT_PATH}"/..)"

: "${CLANG_FORMAT_BIN_PATH:=clang-format}"

# ------------------------------------------------------------------------------

SOURCES="$(find \
    "${INTRINSICCV_ROOT_PATH}"/adapters \
    "${INTRINSICCV_ROOT_PATH}"/intrinsiccv \
    "${INTRINSICCV_ROOT_PATH}"/test \
    \( -name \*.cpp -o -name \*.h \) \
    -print)"

# shellcheck disable=2086
# Split ${SOURCES}.
"${CLANG_FORMAT_BIN_PATH}" -i --verbose ${SOURCES}

# ------------------------------------------------------------------------------
# End of script
# ------------------------------------------------------------------------------
