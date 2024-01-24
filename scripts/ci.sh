#!/usr/bin/env bash
#
# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exu

# Ensure we're at the root of the repo.
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Ensure we're doing a clean build
rm -rf build
rm -rf public

apt-get -y --no-install-recommends install qemu-user

# Check format of C++ files
CHECK_ONLY=ON VERBOSE=ON scripts/format.sh

scripts/cpplint.sh

# Check format of shell scripts
shellcheck scripts/*.sh

# Check license headers
reuse lint

# Build
cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_CLANG_TIDY=clang-tidy \
  -DCMAKE_CXX_FLAGS="--coverage -g -O0" \
  -DINTRINSICCV_ENABLE_SVE2=ON \
  -DINTRINSICCV_ENABLE_SVE2_SELECTIVELY=OFF

# Workaround to avoid applying clang-tidy to files in build directory
echo '{"Checks": "-*,cppcoreguidelines-avoid-goto"}'>build/.clang-tidy

ninja -C build

# Run tests
TESTRESULT=0
qemu-aarch64     build/test/framework/intrinsiccv-framework-test --gtest_output=xml:build/test-results/ || TESTRESULT=1
qemu-aarch64 -cpu cortex-a35 build/test/api/intrinsiccv-api-test --gtest_output=xml:build/test-results/neon/ || TESTRESULT=1
qemu-aarch64 -cpu cortex-a76 build/test/api/intrinsiccv-api-test --gtest_output=xml:build/test-results/sve2/ || TESTRESULT=1
qemu-aarch64 -cpu max,sve128=on,sme512=on \
  build/test/api/intrinsiccv-api-test --gtest_output=xml:build/test-results/sme/ --vector-length=64 || TESTRESULT=1

scripts/prefix_testsuite_names.py build/test-results/neon/intrinsiccv-api-test.xml "NEON."
scripts/prefix_testsuite_names.py build/test-results/sve2/intrinsiccv-api-test.xml "SVE2."
scripts/prefix_testsuite_names.py build/test-results/sme/intrinsiccv-api-test.xml "SME."

# Generate test coverage report
scripts/generate_coverage_report.sh

# Set up GitLab Pages (https://docs.gitlab.com/ee/user/project/pages/)
mkdir public
mv build/coverage public
echo "<meta http-equiv=\"Refresh\" content=\"0; url='coverage/coverage_report.html'\" />">public/index.html

exit $TESTRESULT
