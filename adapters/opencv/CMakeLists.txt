# SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_LIST_DIR}/../../intrinsiccv/CMakeLists.txt")

project("KleidiCV OpenCV HAL")

set(KLEIDICV_HAL_VERSION "0.0.0" CACHE INTERNAL "")
set(KLEIDICV_HAL_LIBRARIES "intrinsiccv_hal" CACHE INTERNAL "")
set(KLEIDICV_HAL_HEADERS "${CMAKE_CURRENT_LIST_DIR}/intrinsiccv_hal.h" CACHE INTERNAL "")
set(KLEIDICV_HAL_INCLUDE_DIRS "$<TARGET_PROPERTY:intrinsiccv,INCLUDE_DIRECTORIES>" CACHE INTERNAL "")

add_library(intrinsiccv_hal STATIC "${CMAKE_CURRENT_LIST_DIR}/intrinsiccv_hal.cpp")
target_link_libraries(intrinsiccv_hal PUBLIC intrinsiccv)
target_include_directories(intrinsiccv_hal PRIVATE
  $<TARGET_PROPERTY:intrinsiccv,INCLUDE_DIRECTORIES>
  ${CMAKE_CURRENT_LIST_DIR}
  ${OPENCV_SOURCE_DIR}/modules/core/include
  ${OPENCV_SOURCE_DIR}/modules/imgproc/include
)
target_compile_options(intrinsiccv_hal PRIVATE $<TARGET_PROPERTY:intrinsiccv,COMPILE_OPTIONS>)
set_target_properties(intrinsiccv_hal PROPERTIES CXX_STANDARD 17)

if(NOT BUILD_SHARED_LIBS)
  ocv_install_target(intrinsiccv_neon
    EXPORT OpenCVModules
    ARCHIVE
    DESTINATION ${OPENCV_3P_LIB_INSTALL_PATH}
    COMPONENT dev
  )
  if(KLEIDICV_BUILD_SVE2)
    ocv_install_target(intrinsiccv_sve2
      EXPORT OpenCVModules
      ARCHIVE
      DESTINATION ${OPENCV_3P_LIB_INSTALL_PATH}
      COMPONENT dev
    )
  endif()
  if(KLEIDICV_BUILD_SME2)
    ocv_install_target(intrinsiccv_sme2
      EXPORT OpenCVModules
      ARCHIVE
      DESTINATION ${OPENCV_3P_LIB_INSTALL_PATH}
      COMPONENT dev
    )
  endif()
  ocv_install_target(intrinsiccv
    EXPORT OpenCVModules
    ARCHIVE
    DESTINATION ${OPENCV_3P_LIB_INSTALL_PATH}
    COMPONENT dev
  )
  ocv_install_target(intrinsiccv_hal
    EXPORT OpenCVModules
    ARCHIVE
    DESTINATION ${OPENCV_3P_LIB_INSTALL_PATH}
    COMPONENT dev
  )
endif()
