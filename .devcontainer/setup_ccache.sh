#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exu

CCACHE_CACHE_DIR=$1

mkdir -p "${CCACHE_CACHE_DIR}"
mkdir -p "${HOME}/.ccache"
echo "cache_dir = ${CCACHE_CACHE_DIR}" > "${HOME}/.ccache/ccache.conf"
