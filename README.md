<!--
SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

---
**NOT SUITABLE FOR DEPLOYMENT OR PRODUCTION USE**
---

# IntrinsicCV

This project is in the early stages of development and is not ready for use
except as a preview.

This is a Computer Vision Library aiming to give high-performing image
processing functions on Arm. It is designed to be simple to import into a wide
variety of projects.

Library provides a C interface.

Adapter layer API is currently provided for:
* OpenCV - [available functionality overview](adapters/opencv/doc-opencv.md)

# Structure

The directory `intrinsiccv` contains generic implementation of the library.
Integration with other projects are stored in `adapters` folder. `test` contains
API and unit tests for the library. All supporting scripts are located in
`scripts`.

# Standalone build

The library can be built using CMake:
```
cmake \
-S /path/to/intrinsiccv \
-B build-intrinsiccv \
-DCMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
-DANDROID_ABI=arm64-v8a
cmake --build build-intrinsiccv --parallel
```

Builds scripts for Linux/macOS are also provided for convenience. To target
Android devices the following command should work:

```
BUILD_ID=android \
CMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
EXTRA_CMAKE_ARGS="-DANDROID_ABI=arm64-v8a" \
scripts/build.sh
```

For further options please refer to the documentation in `./scripts/build.sh`.

# Build and run tests

To build all the tests target `intrinsiccv-test`, to also run them use
`check-intrinsiccv` and set a proper `CMAKE_CROSSCOMPILING_EMULATOR`.

To build all tests for Android:
```
BUILD_ID=android \
CMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
EXTRA_CMAKE_ARGS="-DANDROID_ABI=arm64-v8a" \
scripts/build.sh intrinsiccv-test
```

To build and run all tests for Android with a single command:

```
BUILD_ID=android \
CMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
ADB=<path to adb executable of choice> \
CMAKE_CROSSCOMPILING_EMULATOR="<path to intrinsiccv>/scripts/test_android.sh;<path to intrinsiccv>/build/<build_id>" \
EXTRA_CMAKE_ARGS="-DANDROID_ABI=arm64-v8a" \
scripts/build.sh check-intrinsiccv
```

# Building with OpenCV

## Install

This library is compatible with [OpenCV](https://opencv.org) version 5.x.

Integration consists of the following steps:
1. Download OpenCV sources:
```
git clone https://github.com/opencv/opencv
cd opencv
git checkout 67a3d35b4ea1b11136042cdf5072a634d6789984
```
2. Patch OpenCV:
```
git apply /path/to/intrinsiccv/adapters/opencv/opencv-5.x.patch
```

## Build Library

The project can be built using standard cmake and ninja.

```
cmake \
-S /path/to/opencv \
-B build-opencv \
-G Ninja \
-DWITH_INTRINSICCV=ON \
-DINTRINSICCV_SOURCE_PATH=/path/to/intrinsiccv \
-DCMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
-DANDROID_ABI=arm64-v8a \
-DCMAKE_CXX_STANDARD=14 \
-DBUILD_ANDROID_EXAMPLE=OFF \
-DBUILD_ANDROID_PROJECTS=OFF \
-DBUILD_TESTS=OFF \
-DBUILD_PERF_TESTS=OFF

cmake --build build-opencv --parallel
```

### Build Prerequisites
While the core functionality of the library does not rely on any third-party libraries, there are
build prerequisites that are essential for compiling the source code and generating the executable.
Please ensure that these tools are installed on your system before proceeding with the build
process.

To successfully build and compile this project, you'll need the following tools:
- [cmake](https://cmake.org) version 3.16 or newer (3.21 is recommended),
- [ninja](https://ninja-build.org),
- [gcovr](https://gcovr.com/) and its dependencies for coverage reports,
- [rsync](https://linux.die.net/man/1/rsync) for coverage reports,
- recent version of [llvm](https://llvm.org), preferably 17 or newer.

OpenCV has its own
[dependencies](https://docs.opencv.org/5.x/d7/d9f/tutorial_linux_install.html).
