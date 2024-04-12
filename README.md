<!--
SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

---
**NOT SUITABLE FOR DEPLOYMENT OR PRODUCTION USE**
---

# KleidiCV

This project is in the early stages of development and is not ready for use
except as a preview.

KleidiCV is a library aiming to give high-performing image
processing functions on Arm. It is designed to be simple to import into a wide
variety of projects.

The library provides a C interface. For details see
[the C API documentation](https://kleidicv.sites.arm.com/kleidicv/).

An adapter layer API is currently provided for:
* OpenCV - [available functionality overview](adapters/opencv/doc-opencv.md)

# Structure

The directory `kleidicv` contains generic implementation of the library.
Integration with other projects are stored in `adapters` folder. `test` contains
API and unit tests for the library. `benchmark` contains benchmark source.
`conformity` contains checks to compare the library output with different
implementations. All supporting scripts are located in `scripts`.

# Standalone build using CMake

The library can be built using CMake:
```
cmake \
-S /path/to/kleidicv \
-B build-kleidicv \
cmake --build build-kleidicv --parallel
```

To target Android devices the following CMake flags are also required:
```
-DCMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
-DANDROID_ABI=arm64-v8a
```

# Standalone build using the provided script

Build scripts for Linux/macOS are provided for convenience. To build the library run:
```
scripts/build.sh
```

To target Android devices the following command can be used:
```
BUILD_ID=android \
CMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
EXTRA_CMAKE_ARGS="-DANDROID_ABI=arm64-v8a" \
scripts/build.sh
```

The build artifacts are placed in the `build` directory.

## Build and run tests for Android

To build all the tests use the target `kleidicv-test`, to also run them use
`check-kleidicv` and set a proper `CMAKE_CROSSCOMPILING_EMULATOR`.

To build all tests:
```
BUILD_ID=android \
CMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
EXTRA_CMAKE_ARGS="-DANDROID_ABI=arm64-v8a" \
scripts/build.sh kleidicv-test
```

To run the tests:
```
BUILD_ID=android \
CMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
ADB=<path to adb executable of choice> \
CMAKE_CROSSCOMPILING_EMULATOR="<path to kleidicv>/scripts/test_android.sh;<path to kleidicv>/build/<build_id>" \
EXTRA_CMAKE_ARGS="-DANDROID_ABI=arm64-v8a" \
scripts/build.sh check-kleidicv
```

For further options please refer to the documentation in `./scripts/build.sh`
and `./scripts/test_android.sh`.

# Building with OpenCV

## Install

This library is compatible with [OpenCV](https://opencv.org) version 4.9. (OpenCV 5.x support is experimental.)

Integration consists of the following steps:
1. Download OpenCV sources:
```
wget https://github.com/opencv/opencv/archive/refs/tags/4.9.0.tar.gz
tar xf 4.9.0
cd opencv-4.9.0
```
2. Patch OpenCV:
```
patch -p1</path/to/kleidicv/adapters/opencv/opencv-4.9.patch
```

## Build Library

The project can be built using standard cmake.

```
cmake \
-S /path/to/opencv \
-B build-opencv \
-DWITH_KLEIDICV=ON \
-DKLEIDICV_SOURCE_PATH=/path/to/kleidicv \
-DCMAKE_CXX_STANDARD=14 \
-DBUILD_ANDROID_EXAMPLE=OFF \
-DBUILD_ANDROID_PROJECTS=OFF \
-DBUILD_TESTS=OFF \
-DBUILD_PERF_TESTS=OFF

cmake --build build-opencv --parallel
```

To target Android devices the following CMake flags are required here as well:
```
-DCMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
-DANDROID_ABI=arm64-v8a
```

# Benchmarking

Benchmarks can be enabled with the `cmake` argument `-DKLEIDICV_BENCHMARK=ON`.
The benchmarks are based on [Google Benchmark](https://github.com/google/benchmark).
All the standard Google Benchmark command line arguments can be used. In addition,
the image size on which the tests will be run can be set with the command line
options `--image_width=123` and `--image_height=123`.

# Build Prerequisites
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

Building for Android requires the [Android NDK](https://developer.android.com/ndk/).

Running tests on Android devices requires [ADB](https://developer.android.com/tools/adb).
Can be installed standalone by installing [Android SDK Platform-Tools](https://developer.android.com/tools/releases/platform-tools).

OpenCV has its own
[dependencies](https://docs.opencv.org/5.x/d7/d9f/tutorial_linux_install.html).
