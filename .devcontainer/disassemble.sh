#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -eu

: "${COMPILER:=clang}"

FILE_PATH="$1"

echo "Active file: ${FILE_PATH}"

if [[ ! "${FILE_PATH}" =~ ^kleidicv/src/.*/.*[_neon|_sve2|_sme|_sme2].cpp$ ]]; then
  echo "Wrong source file! Please open a .cpp file from the 'kleidicv/src' directory ending '_neon', '_sve2' or '_sme'!"
  exit 0
fi

if [[ "${FILE_PATH}" =~ _neon.cpp$ ]]; then
  SIMD_BUILD_DIRECTORY=kleidicv_neon.dir
elif [[ "${FILE_PATH}" =~ _sve2.cpp$ ]]; then
  SIMD_BUILD_DIRECTORY=kleidicv_sve2.dir
elif [[ "${FILE_PATH}" =~ _sme.cpp$ ]]; then
  SIMD_BUILD_DIRECTORY=kleidicv_sme.dir
elif [[ "${FILE_PATH}" =~ _sme2.cpp$ ]]; then
  SIMD_BUILD_DIRECTORY=kleidicv_sme2.dir
else
  echo "Unexpected filename!"
  exit 1
fi

OUTPUT_FILE_STEM="$(basename "${FILE_PATH}" .cpp)"

if [[ "${COMPILER}" == "gcc" ]]; then
  BASE_BUILD_DIRECTORY="build/kleidicv-gcc/kleidicv/CMakeFiles"
  OUTPUT_FILE="gitignored/disassembly/disasm-gcc-${OUTPUT_FILE_STEM}.txt"
else
  BASE_BUILD_DIRECTORY="build/kleidicv/kleidicv/CMakeFiles"
  OUTPUT_FILE="gitignored/disassembly/disasm-${OUTPUT_FILE_STEM}.txt"
fi

OBJECT_PATH="${BASE_BUILD_DIRECTORY}/${SIMD_BUILD_DIRECTORY}/${FILE_PATH#kleidicv/}.o"

mkdir -p "$(dirname "${OUTPUT_FILE}")"

llvm-objdump -C -d -r --mattr=+sme2 "${OBJECT_PATH}" | tee "${OUTPUT_FILE}"
