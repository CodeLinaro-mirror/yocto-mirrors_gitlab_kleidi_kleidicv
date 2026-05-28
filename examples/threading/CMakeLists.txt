# SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

project("KleidiCV threading example")

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

add_executable(
  kleidicv-threading-example
  main.cpp
)

target_include_directories(
  kleidicv-threading-example
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../kleidicv/include
  ${CMAKE_CURRENT_BINARY_DIR}/../../kleidicv/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../kleidicv_thread/include
)

set_target_properties(kleidicv-threading-example PROPERTIES CXX_STANDARD 17)

target_link_libraries(
  kleidicv-threading-example
  kleidicv_thread
  kleidicv
  Threads::Threads
)
