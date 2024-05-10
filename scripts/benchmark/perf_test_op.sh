#!/bin/sh

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -eu

CPU_NUMBER=$1
THERMAL_ZONE_ID=$2
DISP_NAME=$3
PERF_TEST_BINARY_BASENAME=$4
GTEST_FILTER=$5
GTEST_PARAM_FILTER=$6

DEV_DIR=/data/local/tmp

CPU_MASK=$(echo "obase=16;2^${CPU_NUMBER}" | bc)

PREV_FREQ_GOVERNOR=$(cat /sys/devices/system/cpu/cpu${CPU_NUMBER}/cpufreq/scaling_governor)
echo performance > /sys/devices/system/cpu/cpu${CPU_NUMBER}/cpufreq/scaling_governor

CDEV_TRANSITION_COUNT=$(cat /sys/devices/virtual/thermal/thermal_zone${THERMAL_ZONE_ID}/cdev0/stats/total_trans)

wait_for_cooldown() {
  while [[ $(cat /sys/devices/virtual/thermal/thermal_zone${THERMAL_ZONE_ID}/temp) > 40000 ]]; do
    sleep 0.2
  done
}

FNAME=$$

wait_for_cooldown
>&2 taskset ${CPU_MASK} ${DEV_DIR}/"${PERF_TEST_BINARY_BASENAME}"_vanilla --perf_min_samples=100 --gtest_output=json:${DEV_DIR}/${FNAME}_vanilla --gtest_filter="${GTEST_FILTER}" --gtest_param_filter="${GTEST_PARAM_FILTER}"

wait_for_cooldown
>&2 taskset ${CPU_MASK} ${DEV_DIR}/"${PERF_TEST_BINARY_BASENAME}"_kleidicv --perf_min_samples=100 --gtest_output=json:${DEV_DIR}/${FNAME}_kleidicv  --gtest_filter="${GTEST_FILTER}" --gtest_param_filter="${GTEST_PARAM_FILTER}"

if [ -f ${DEV_DIR}/"${PERF_TEST_BINARY_BASENAME}"_custom ]; then
  wait_for_cooldown
  >&2 taskset ${CPU_MASK} ${DEV_DIR}/"${PERF_TEST_BINARY_BASENAME}"_custom --perf_min_samples=100 --gtest_output=json:${DEV_DIR}/${FNAME}_custom  --gtest_filter="${GTEST_FILTER}" --gtest_param_filter="${GTEST_PARAM_FILTER}"
fi

echo ${PREV_FREQ_GOVERNOR} > /sys/devices/system/cpu/cpu${CPU_NUMBER}/cpufreq/scaling_governor

if [[ ${CDEV_TRANSITION_COUNT} != $(cat /sys/devices/virtual/thermal/thermal_zone${THERMAL_ZONE_ID}/cdev0/stats/total_trans) ]]; then
  >&2 echo CPU throttling happened, exiting...
  exit 1
fi

if ! grep -q "\"tests\": 1," ${DEV_DIR}/${FNAME}_vanilla; then
  >&2 echo More than one test case was triggered, exiting
  exit 1
fi

get_mean() {
  sed -n s/\"mean\"://p ${1} | tr -d \" | tr -d ',' | tr -d ' '
}

get_gstddev() {
  sed -n s/\"gstddev\"://p ${1} | tr -d \" | tr -d ',' | tr -d ' '
}

RES="${DISP_NAME}\t$(get_mean ${DEV_DIR}/${FNAME}_vanilla)\t$(get_gstddev ${DEV_DIR}/${FNAME}_vanilla)"
rm ${DEV_DIR}/${FNAME}_vanilla
RES="${RES}\t$(get_mean ${DEV_DIR}/${FNAME}_kleidicv)\t$(get_gstddev ${DEV_DIR}/${FNAME}_kleidicv)"
rm ${DEV_DIR}/${FNAME}_kleidicv

if [ -f ${DEV_DIR}/${FNAME}_custom ]; then
  RES="${RES}\t$(get_mean ${DEV_DIR}/${FNAME}_custom)\t$(get_gstddev ${DEV_DIR}/${FNAME}_custom)"
  rm ${DEV_DIR}/${FNAME}_custom
fi

printf "${RES}"
