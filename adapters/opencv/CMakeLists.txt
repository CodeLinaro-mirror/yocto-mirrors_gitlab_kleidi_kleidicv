# SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_LIST_DIR}/../../kleidicv/CMakeLists.txt")
include("${CMAKE_CURRENT_LIST_DIR}/../../kleidicv_opencv/CMakeLists.txt")
include("${CMAKE_CURRENT_LIST_DIR}/../../kleidicv_thread/CMakeLists.txt")

project("KleidiCV OpenCV HAL")

set(KLEIDICV_HAL_VERSION "0.4.0" CACHE INTERNAL "")
set(KLEIDICV_HAL_LIBRARIES "kleidicv_hal" CACHE INTERNAL "")
set(KLEIDICV_HAL_HEADERS "${CMAKE_CURRENT_LIST_DIR}/kleidicv_hal.h" CACHE INTERNAL "")
set(KLEIDICV_HAL_INCLUDE_DIRS "$<TARGET_PROPERTY:kleidicv_opencv,INCLUDE_DIRECTORIES>;$<TARGET_PROPERTY:kleidicv_thread,INCLUDE_DIRECTORIES>" CACHE INTERNAL "")

add_library(kleidicv_hal STATIC "${CMAKE_CURRENT_LIST_DIR}/kleidicv_hal.cpp")
target_link_libraries(kleidicv_hal PUBLIC kleidicv kleidicv_opencv kleidicv_thread)
target_include_directories(kleidicv_hal PRIVATE
  ${KLEIDICV_HAL_INCLUDE_DIRS}
  ${CMAKE_CURRENT_LIST_DIR}
  ${OpenCV_SOURCE_DIR}/modules/core/include
  ${OpenCV_SOURCE_DIR}/modules/imgproc/include
)
target_compile_options(
  kleidicv_hal
  PRIVATE
    $<TARGET_PROPERTY:kleidicv,COMPILE_OPTIONS>
    "-Wno-old-style-cast"
)

set_target_properties(kleidicv_hal PROPERTIES CXX_STANDARD 17)

function(kleidicv_ocv_install_target name)
  ocv_install_target(${name}
    EXPORT OpenCVModules
    ARCHIVE
    DESTINATION ${OPENCV_3P_LIB_INSTALL_PATH}
    COMPONENT dev
  )
endfunction()

if(NOT BUILD_SHARED_LIBS)
  kleidicv_ocv_install_target(kleidicv_neon)
  kleidicv_ocv_install_target(kleidicv_opencv_neon)
  if(KLEIDICV_ENABLE_SVE2)
    kleidicv_ocv_install_target(kleidicv_sve2)
    kleidicv_ocv_install_target(kleidicv_opencv_sve2)
  endif()
  if(KLEIDICV_ENABLE_SME2)
    kleidicv_ocv_install_target(kleidicv_sme2)
    kleidicv_ocv_install_target(kleidicv_opencv_sme2)
  endif()
  kleidicv_ocv_install_target(kleidicv)
  kleidicv_ocv_install_target(kleidicv_opencv)
  kleidicv_ocv_install_target(kleidicv_thread)
  kleidicv_ocv_install_target(kleidicv_hal)
endif()
