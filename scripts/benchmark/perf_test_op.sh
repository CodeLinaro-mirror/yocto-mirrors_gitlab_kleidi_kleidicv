#!/bin/sh

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -eu

CUSTOM_BUILD_SUFFIX=$1
CPU_MASK=$2
THERMAL_ZONE_ID=$3
DISP_NAME=$4
PERF_TEST_BINARY_BASENAME=$5
GTEST_FILTER=$6
GTEST_PARAM_FILTER=$7

: "${DEV_DIR:=/data/local/tmp}"

get_freq_governor() {
  cat "/sys/devices/system/cpu/cpu${1}/cpufreq/scaling_governor"
}
set_freq_governor() {
  echo "${2}" >"/sys/devices/system/cpu/cpu${1}/cpufreq/scaling_governor"
}

i=0
x=1
c=$((0x${CPU_MASK}))
CPUS=''
PREV_GOVERNORS=''
while [ "$c" -ge "$i" ]; do
  if [ 0 -ne $(( c & x )) ]; then
    CPUS="$i $CPUS"
    PREV_GOVERNORS="$(get_freq_governor "$i") $PREV_GOVERNORS"
    set_freq_governor "$i" performance
  fi
  i=$((i + 1))
  x=$((x * 2))
done

THERMAL_ZONE_DIR="/sys/devices/virtual/thermal/thermal_zone${THERMAL_ZONE_ID}"

CDEV_TRANSITION_COUNT_FILE="${THERMAL_ZONE_DIR}/cdev0/stats/total_trans"
START_CDEV_TRANSITION_COUNT=$(cat "${CDEV_TRANSITION_COUNT_FILE}")

THERMAL_ZONE_TEMPERATURE_FILE="${THERMAL_ZONE_DIR}/temp"

wait_for_cooldown() {
  # shellcheck disable=SC3043
  # Many shells support 'local', therefore this warning is ignored.
  local cur_tmp
  cur_tmp=$(cat "${THERMAL_ZONE_TEMPERATURE_FILE}")
  if [ "${cur_tmp}" -gt 40000 ]; then
    >&2 echo "Too hot (${cur_tmp})! Cooling..."
  fi

  while [ "$(cat "${THERMAL_ZONE_TEMPERATURE_FILE}")" -gt 40000 ]; do
    sleep 0.2
  done
}

FNAME=$$

run_test() {
  wait_for_cooldown
  >&2 taskset "${CPU_MASK}" \
    "${DEV_DIR}/${PERF_TEST_BINARY_BASENAME}_$1" \
      --perf_min_samples=100 \
      --gtest_output=json:"${DEV_DIR}/${FNAME}_$1" \
      --gtest_filter="${GTEST_FILTER}" \
      --gtest_param_filter="${GTEST_PARAM_FILTER}"
}

run_test vanilla
run_test kleidicv
if [ -f "${DEV_DIR}/${PERF_TEST_BINARY_BASENAME}_kleidicv_${CUSTOM_BUILD_SUFFIX}" ]; then
  run_test "kleidicv_${CUSTOM_BUILD_SUFFIX}"
fi

while [ -n "${CPUS}" ]; do
  c=$(echo "${CPUS}" | cut -d ' ' -f1)
  CPUS=$(echo "${CPUS}" | cut -d ' ' -f2- -s)
  g=$(echo "${PREV_GOVERNORS}" | cut -d ' ' -f1)
  PREV_GOVERNORS=$(echo "${PREV_GOVERNORS}" | cut -d ' ' -f2- -s)
  set_freq_governor "$c" "$g"
done

if [ "${START_CDEV_TRANSITION_COUNT}" != "$(cat "${CDEV_TRANSITION_COUNT_FILE}")" ]; then
  >&2 echo "BENCHMARK ERROR: CPU throttling happened, exiting..."
  exit 1
fi

if ! grep -q "\"tests\": 1," "${DEV_DIR}/${FNAME}_vanilla"; then
  if grep -q "\"tests\": 0," "${DEV_DIR}/${FNAME}_vanilla"; then
    >&2 echo "BENCHMARK ERROR: No test case was triggered, exiting..."
  else
    >&2 echo "BENCHMARK ERROR: More than one test case was triggered, exiting..."
  fi
  exit 1
fi

get_mean() {
  sed -n s/\"mean\"://p "${1}" | tr -d \" | tr -d ',' | tr -d ' '
}

get_gstddev() {
  sed -n s/\"gstddev\"://p "${1}" | tr -d \" | tr -d ',' | tr -d ' '
}

RES="${DISP_NAME}"

collect_run_results() {
  RES="${RES}\t$(get_mean "${DEV_DIR}/${FNAME}_${1}")\t$(get_gstddev "${DEV_DIR}/${FNAME}_$1")"
  rm "${DEV_DIR}/${FNAME}_$1"
}

collect_run_results vanilla
collect_run_results kleidicv
if [ -f "${DEV_DIR}/${FNAME}_kleidicv_${CUSTOM_BUILD_SUFFIX}" ]; then
  collect_run_results "kleidicv_${CUSTOM_BUILD_SUFFIX}"
fi

echo "${RES}"
