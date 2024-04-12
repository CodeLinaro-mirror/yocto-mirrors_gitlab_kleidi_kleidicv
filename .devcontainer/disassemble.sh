#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -eu

FILE_PATH="$1"

echo "Active file: ${FILE_PATH}"

if [[ ! "${FILE_PATH}" =~ ^kleidicv/src/.*/.*[_neon|_sve2|_sme2].cpp$ ]]; then
  echo "Wrong source file! Please open a .cpp file from the 'kleidicv/src' directory ending '_neon', '_sve2' or '_sme2'!"
  exit 0
fi

if [[ "${FILE_PATH}" =~ _neon.cpp$ ]]; then
  SIMD_BUILD_DIRECTORY=kleidicv_neon.dir
elif [[ "${FILE_PATH}" =~ _sve2.cpp$ ]]; then
  SIMD_BUILD_DIRECTORY=kleidicv_sve2.dir
elif [[ "${FILE_PATH}" =~ _sme2.cpp$ ]]; then
  SIMD_BUILD_DIRECTORY=kleidicv_sme2.dir
else
  echo "Unexpected filename!"
  exit 1
fi

OBJECT_PATH="build/kleidicv/kleidicv/CMakeFiles/${SIMD_BUILD_DIRECTORY}/${FILE_PATH#kleidicv/}.o"

llvm-objdump -d -r --mattr=+sme2 "${OBJECT_PATH}"
