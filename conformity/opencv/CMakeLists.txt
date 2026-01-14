# SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

project("OpenCV Conformity" CXX)

set(CMAKE_CXX_STANDARD 17)

# Please update SECURITY.md if adding, removing or changing the version of third party content.
set(OPENCV_VERSION "4.13.0" CACHE STRING "Version of OpenCV")
set(OPENCV_URL "https://github.com/opencv/opencv/archive/refs/tags/${OPENCV_VERSION}.tar.gz" CACHE STRING "URL of OpenCV")

# Remove the last part of the version number (e.g., 4.9.0 -> 4.9)
string(REGEX REPLACE "\.[^\.]+$" "" OPENCV_PATCH_VERSION ${OPENCV_VERSION})
set(OPENCV_PATCH_FILE "${CMAKE_CURRENT_LIST_DIR}/../../adapters/opencv/opencv-${OPENCV_PATCH_VERSION}.patch")

# The patch command can be run only with a non-empty patch file, otherwise fetch content fails.
set(OPENCV_PATCH_FILE_HAS_CONTENT FALSE)
file(STRINGS "${OPENCV_PATCH_FILE}" OPENCV_PATCH_LINES)
foreach(line IN LISTS OPENCV_PATCH_LINES)
  if (NOT line MATCHES "^(//|$)")
    set(OPENCV_PATCH_FILE_HAS_CONTENT TRUE)
    break()
  endif()
endforeach()

if (OPENCV_PATCH_FILE_HAS_CONTENT)
  set(OPENCV_PATCH_COMMAND
    patch -p1 -i "${OPENCV_PATCH_FILE}"
  )
else()
  message(STATUS "Patching OpenCV is skipped as the patch file is empty or comment-only.")
  set(OPENCV_PATCH_COMMAND
    ${CMAKE_COMMAND} -E true
  )
endif()

include(FetchContent)

FetchContent_Declare(
  OpenCV
  URL ${OPENCV_URL}
  PATCH_COMMAND ${OPENCV_PATCH_COMMAND}
)

FetchContent_MakeAvailable(OpenCV)

file(GLOB conformity_test_sources CONFIGURE_DEPENDS "test_*.cpp")

# Manager
add_executable(
  manager
  manager.cpp
  tests.cpp
  ${conformity_test_sources}
)

target_link_libraries(
  manager
  opencv_core
  opencv_imgproc
  opencv_video
)

target_include_directories(
  manager
  PRIVATE
    ${CMAKE_BINARY_DIR}
    ${OpenCV_SOURCE_DIR}/modules/core/include
    ${OpenCV_SOURCE_DIR}/modules/imgproc/include
    ${OpenCV_SOURCE_DIR}/modules/video/include
)

target_compile_definitions(
  manager
  PRIVATE MANAGER=1
)

target_compile_options(
  manager
  PRIVATE
    "-Werror"
    "-Wall"
    "-Wextra"
)

# Subordinate
add_executable(
  subordinate
  subordinate.cpp
  tests.cpp
  ${conformity_test_sources}
)

target_link_libraries(
  subordinate
  opencv_core
  opencv_imgproc
  opencv_video
)

target_include_directories(
  subordinate
  PRIVATE
    ${CMAKE_BINARY_DIR}
    ${OpenCV_SOURCE_DIR}/modules/core/include
    ${OpenCV_SOURCE_DIR}/modules/imgproc/include
    ${OpenCV_SOURCE_DIR}/modules/video/include
)

target_compile_definitions(
  subordinate
  PRIVATE MANAGER=0
)

target_compile_options(
  subordinate
  PRIVATE
    "-Werror"
    "-Wall"
    "-Wextra"
)
