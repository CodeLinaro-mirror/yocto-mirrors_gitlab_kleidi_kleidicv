#!/bin/sh

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -eu

CUSTOM_BUILD_SUFFIX=$1
CPU_NUMBER=$2
THERMAL_ZONE_ID=$3
DISP_NAME=$4
PERF_TEST_BINARY_BASENAME=$5
GTEST_FILTER=$6
GTEST_PARAM_FILTER=$7

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

run_test() {
  wait_for_cooldown
  >&2 taskset ${CPU_MASK} \
    ${DEV_DIR}/"${PERF_TEST_BINARY_BASENAME}"_${1} \
      --perf_min_samples=100 \
      --gtest_output=json:${DEV_DIR}/${FNAME}_${1} \
      --gtest_filter="${GTEST_FILTER}" \
      --gtest_param_filter="${GTEST_PARAM_FILTER}"
}

run_test vanilla
run_test kleidicv
if [[ -f ${DEV_DIR}/"${PERF_TEST_BINARY_BASENAME}"_kleidicv_$CUSTOM_BUILD_SUFFIX ]]; then
  run_test kleidicv_$CUSTOM_BUILD_SUFFIX
fi

echo ${PREV_FREQ_GOVERNOR} > /sys/devices/system/cpu/cpu${CPU_NUMBER}/cpufreq/scaling_governor

if [[ ${CDEV_TRANSITION_COUNT} != $(cat /sys/devices/virtual/thermal/thermal_zone${THERMAL_ZONE_ID}/cdev0/stats/total_trans) ]]; then
  >&2 echo "BENCHMARK ERROR: CPU throttling happened, exiting..."
  exit 1
fi

if ! grep -q "\"tests\": 1," ${DEV_DIR}/${FNAME}_vanilla; then
  if grep -q "\"tests\": 0," ${DEV_DIR}/${FNAME}_vanilla; then
    >&2 echo "BENCHMARK ERROR: No test case was triggered, exiting..."
  else
    >&2 echo "BENCHMARK ERROR: More than one test case was triggered, exiting..."
  fi
  exit 1
fi

get_mean() {
  sed -n s/\"mean\"://p ${1} | tr -d \" | tr -d ',' | tr -d ' '
}

get_gstddev() {
  sed -n s/\"gstddev\"://p ${1} | tr -d \" | tr -d ',' | tr -d ' '
}

RES="${DISP_NAME}\t"

collect_run_results() {
  RES+="\t$(get_mean ${DEV_DIR}/${FNAME}_${1})\t$(get_gstddev ${DEV_DIR}/${FNAME}_${1})"
  rm ${DEV_DIR}/${FNAME}_${1}
}

collect_run_results vanilla
collect_run_results kleidicv
if [[ -f ${DEV_DIR}/${FNAME}_$CUSTOM_BUILD_SUFFIX ]]; then
  collect_run_results kleidicv_$CUSTOM_BUILD_SUFFIX
fi

printf "${RES}"
