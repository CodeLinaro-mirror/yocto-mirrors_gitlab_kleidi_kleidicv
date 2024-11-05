# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

project("KleidiCV OpenCV")

file(GLOB KLEIDICV_OPENCV_API_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_api.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_api.cpp"
)

file(GLOB KLEIDICV_OPENCV_NEON_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_neon.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_neon.cpp"
)

file(GLOB KLEIDICV_OPENCV_SVE2_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_sve2.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_sve2.cpp"
)

file(GLOB KLEIDICV_OPENCV_SME2_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/src/*_sme2.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/**/*_sme2.cpp"
)

set(KLEIDICV_OPENCV_INCLUDE_DIRS
  "${CMAKE_CURRENT_LIST_DIR}/include"
  $<TARGET_PROPERTY:kleidicv,INCLUDE_DIRECTORIES>
)

set(KLEIDICV_OPENCV_WARNING_FLAGS
  "-Wall"
  "-Wextra"
  "-Wold-style-cast"
  "-Wno-shadow" # GCC's shadow declaration check is too sensitive for the library
)

set(KLEIDICV_OPENCV_CXX_FLAGS
  "-O2"
  "-g0"
  "-fomit-frame-pointer"
  "-fno-stack-protector"
  "-fno-exceptions"
  "-fno-rtti"
  "-fno-unroll-loops"
  ${KLEIDICV_OPENCV_WARNING_FLAGS}
)

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  list(APPEND KLEIDICV_OPENCV_CXX_FLAGS
    "-mllvm"
    "-inline-threshold=10000"
  )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  list(APPEND KLEIDICV_OPENCV_CXX_FLAGS
    "-flax-vector-conversions"
    "-Wno-unused-label"
  )
endif()

if (KLEIDICV_CHECK_BANNED_FUNCTIONS)
  # The `SHELL:` prefix is used to turn off de-duplication of compiler flags,
  # it is necessary if other headers are need to be force included.
  # https://cmake.org/cmake/help/latest/command/target_compile_options.html#option-de-duplication
  list(APPEND KLEIDICV_OPENCV_CXX_FLAGS "SHELL:-include kleidicv/unsafe.h")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND KLEIDICV_OPENCV_CXX_FLAGS "-O0" "-g")
else()
  list(APPEND KLEIDICV_OPENCV_CXX_FLAGS "-O2" "-g0")
endif()

add_library(kleidicv_opencv_neon OBJECT ${KLEIDICV_OPENCV_NEON_SOURCES})
target_include_directories(kleidicv_opencv_neon PRIVATE ${KLEIDICV_OPENCV_INCLUDE_DIRS})
set_target_properties(kleidicv_opencv_neon PROPERTIES CXX_STANDARD 17)
target_compile_options(kleidicv_opencv_neon PRIVATE
  ${KLEIDICV_OPENCV_CXX_FLAGS}
  "-march=armv8-a"
  "-DKLEIDICV_TARGET_NEON=1"
)

if(KLEIDICV_ENABLE_SVE2)
  add_library(kleidicv_opencv_sve2 OBJECT ${KLEIDICV_OPENCV_SVE2_SOURCES})
  target_include_directories(kleidicv_opencv_sve2 PRIVATE ${KLEIDICV_OPENCV_INCLUDE_DIRS})
  set_target_properties(kleidicv_opencv_sve2 PROPERTIES CXX_STANDARD 17)
  target_compile_options(kleidicv_opencv_sve2 PRIVATE
    ${KLEIDICV_OPENCV_CXX_FLAGS}
    "-march=armv8-a+sve2"
    "-DKLEIDICV_TARGET_SVE2=1"
  )
endif()

if(KLEIDICV_ENABLE_SME2)
  add_library(kleidicv_opencv_sme2 OBJECT ${KLEIDICV_OPENCV_SME2_SOURCES})
  target_include_directories(kleidicv_opencv_sme2 PRIVATE ${KLEIDICV_OPENCV_INCLUDE_DIRS})
  set_target_properties(kleidicv_opencv_sme2 PROPERTIES CXX_STANDARD 17)
  target_compile_options(kleidicv_opencv_sme2 PRIVATE
    ${KLEIDICV_OPENCV_CXX_FLAGS}
    "-march=armv9-a+sve2+sme2+nosimd"
    "-DKLEIDICV_TARGET_SME2=1"
  )
endif()

add_library(kleidicv_opencv STATIC ${KLEIDICV_OPENCV_API_SOURCES})
target_include_directories(kleidicv_opencv PRIVATE ${KLEIDICV_OPENCV_INCLUDE_DIRS})
set_target_properties(kleidicv_opencv PROPERTIES CXX_STANDARD 17)
target_compile_options(kleidicv_opencv PRIVATE ${KLEIDICV_OPENCV_CXX_FLAGS})
target_link_libraries(kleidicv_opencv PRIVATE kleidicv_opencv_neon)

if(KLEIDICV_ENABLE_SVE2)
  target_compile_definitions(kleidicv_opencv PRIVATE KLEIDICV_HAVE_SVE2)
  target_link_libraries(kleidicv_opencv PRIVATE kleidicv_opencv_sve2)
endif()

if(KLEIDICV_ENABLE_SME2)
  target_compile_definitions(kleidicv_opencv PRIVATE KLEIDICV_HAVE_SME2)
  target_link_libraries(kleidicv_opencv PRIVATE kleidicv_opencv_sme2)
endif()
