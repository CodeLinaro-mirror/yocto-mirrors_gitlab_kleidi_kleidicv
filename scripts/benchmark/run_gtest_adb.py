#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

"""
Runs OpenCV's gtest-based benchmarks via Android ADB

The script checks the CPU temperature before running each benchmark and
if it's found to be too high then waits to allow it to cool down, to
avoid the CPU being throttled.

This script provides similar functionality to perf_test_op.sh.
It should be run from the adb host whereas perf_test_op.sh is designed
to run directly on the device.
The benefit of this script is that it can be run with --gtest_filter
matching a large number of tests whereas perf_test_op.sh must be called
for each individual test.

Example invocation that runs all 640*480 multiply tests and subtract
tests, on executables with and without KleidiCV enabled, on mid and big
cores, and writes the mean & stddev to a tsv file that can be opened in a
text editor or spreadsheet editor for further analysis:

./run_gtest_adb.py \
    --cpus 4 7 \
    --thermal_zones 1 0 \
    --serial 0123456789ABCDEF \
    --tsv multiply_subtract.tsv \
    --tsv_columns mean stddev \
    --executables opencv_perf_core_vanilla opencv_perf_core_kleidicv \
    --gtest_filter="*multiply*:*subtract*" \
    --gtest_param_filter="*640x480*"
"""

import argparse
import collections
import csv
import json
import os
import shlex
import subprocess
import tempfile
import time


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--executables",
        nargs="+",
        help="Path to the gtest executables on the host. "
        "These will be copied to the device.",
    )
    parser.add_argument("--adb", default="adb", help="Path to adb")
    parser.add_argument(
        "--cpus",
        type=int,
        nargs="+",
        help="Indices of CPUs to run on. "
        "Typically 7 for big, 4-6 for mid, 0-3 for little",
    )
    parser.add_argument(
        "--thermal_zones",
        type=int,
        nargs="+",
        help="Thermal zones of CPUs to run on, corresponding to --cpus argument. "
        "Typically 0 for big CPU, 1 for mid CPU, 2 for little CPU",
    )
    parser.add_argument(
        "--tsv",
        default="gtest_adb.tsv",
        help="tab-separated-value output file",
    )
    parser.add_argument(
        "--json",
        default="gtest_adb.json",
        help="JSON output file",
    )
    parser.add_argument(
        "--serial",
        "-s",
        help="Serial of the device to connect to. "
        "Optional if only one device is connected",
    )
    parser.add_argument(
        "--tmpdir",
        default="/data/local/tmp",
        help="Temporary directory on the device. "
        "Executables and output files will be stored here",
    )
    parser.add_argument(
        "--tsv_columns",
        default=["median"],
        nargs="*",
        help="Which measurements to write to the TSV file "
        "e.g. median mean min gmean gstddev. "
        "All measurements are always available in JSON file",
    )
    parser.add_argument(
        "--repetitions",
        type=int,
        default=1,
        help="Repeat the entire set of tests this many times. "
        "Useful for getting a sense of how much noise there is in the results",
    )
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--gtest_filter")
    parser.add_argument("--gtest_param_filter")
    parser.add_argument("--perf_min_samples", type=int, default=100)
    args = parser.parse_args()

    assert len(args.cpus) == len(args.thermal_zones)

    return args


class ADBRunner:
    def __init__(self, *, adb_command, serial_number, verbose):
        self.adb_command = adb_command
        self.serial_number = serial_number
        self.verbose = verbose

    def _make_adb_command(self):
        result = [self.adb_command]
        if self.serial_number:
            result.extend(["-s", self.serial_number])
        return result

    def _print_command(self, command):
        if self.verbose:
            print("+" + shlex.join(command))

    def check_output(self, args):
        command = self._make_adb_command() + ["shell"] + args
        self._print_command(command)
        return subprocess.check_output(
            command, stderr=subprocess.STDOUT
        ).decode()

    def push(self, filenames, dst):
        command = self._make_adb_command() + ["push", *filenames, dst]
        self._print_command(command)
        subprocess.check_output(command).decode()

    def pull(self, filenames, dst="."):
        command = self._make_adb_command() + ["pull", *filenames, dst]
        self._print_command(command)
        subprocess.check_output(command).decode()

    def read_json(self, json_filename):
        with tempfile.TemporaryDirectory() as tmpdirname:
            dst = os.path.join(tmpdirname, "t.json")
            self.pull([json_filename], dst=dst)
            with open(dst) as f:
                return json.load(f)


def wait_for_cooldown(runner, thermal_zone):
    temp_filename = (
        f"/sys/devices/virtual/thermal/thermal_zone{thermal_zone}/temp"
    )
    while True:
        temp = int(runner.check_output(["cat", temp_filename]))
        if temp < 40000:
            return
        print(f"Temperature {temp} - waiting to cool down")
        time.sleep(1)


def get_run_name(rep, executable, cpu):
    return f"{os.path.basename(executable)}-{cpu}-#{rep}"


def run_executable_tests(runner, args, host_executable, cpu, thermal_zone):

    executable = os.path.join(args.tmpdir, os.path.basename(host_executable))

    output_file = f"{os.path.splitext(executable)[0]}-{cpu}.json"

    command_args = [
        executable,
        "--gtest_list_tests",
        f"--gtest_output=json:{output_file}",
    ]
    if args.gtest_filter:
        command_args.append(f"--gtest_filter={args.gtest_filter}")
    if args.gtest_param_filter:
        command_args.append(f"--gtest_param_filter={args.gtest_param_filter}")

    runner.check_output(command_args)
    testsuite_dict = runner.read_json(output_file)

    test_list = []

    for testsuite in testsuite_dict["testsuites"]:
        testsuite_name = testsuite["name"]
        for test in testsuite["testsuite"]:
            test_name = test["name"]
            test_list.append(f"{testsuite_name}.{test_name}")

    results = None
    testsuites = collections.OrderedDict()

    taskset_mask = "{:x}".format(1 << cpu)

    for test_name in test_list:
        wait_for_cooldown(runner, thermal_zone)
        runner.check_output(
            [
                "taskset",
                taskset_mask,
                executable,
                f"--gtest_output=json:{output_file}",
                f"--gtest_filter={test_name}",
                f"--perf_min_samples={args.perf_min_samples}",
            ]
        )
        test_result = runner.read_json(output_file)

        if not results:
            results = test_result

        testsuite = test_result["testsuites"][0]
        testsuite_name = testsuite["name"]
        if testsuite_name not in testsuites:
            testsuites[testsuite_name] = testsuite
        else:
            testsuites[testsuite_name]["testsuite"].extend(
                testsuite["testsuite"]
            )

        test_result = testsuite["testsuite"][0]

        output = (
            f"{executable}-{cpu}\t{test_name}"
            f"\t{test_result['value_param']}"
        )
        for key in args.tsv_columns:
            output += f"\t{test_result[key]}"
        print(output)

    results["testsuites"] = list(testsuites.values())

    return results


def run_tests_on_cpu(runner, args, rep, cpu, thermal_zone):
    scaling_governor_filename = (
        f"/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_governor"
    )
    prev_scaling_governor = runner.check_output(
        ["cat", scaling_governor_filename]
    ).strip()
    try:
        runner.check_output(
            ["echo", "performance", "~>", scaling_governor_filename]
        )
        return {
            get_run_name(rep, executable, cpu): run_executable_tests(
                runner, args, executable, cpu, thermal_zone
            )
            for executable in args.executables
        }
    finally:
        runner.check_output(
            ["echo", prev_scaling_governor, "~>", scaling_governor_filename]
        )


def get_results_table(args, results):
    # If executables have a common prefix then strip it, or omit it
    # altogether if only one executable.
    exe_common_prefix_len = len(os.path.commonprefix(args.executables))

    field_names = ["name", "value_param"]
    for cpu in args.cpus:
        for key in args.tsv_columns:
            for executable in args.executables:
                for rep in range(args.repetitions):
                    field_names.append(
                        f"{cpu} {executable[exe_common_prefix_len:]} {key} #{rep}"
                    )

    rows = [field_names]

    first_result = next(iter(results.values()))
    for testsuite_index, testsuite in enumerate(first_result["testsuites"]):
        testsuite_name = testsuite["name"]
        for test_index, test in enumerate(testsuite["testsuite"]):
            test_name = test["name"]
            value_param = test["value_param"]
            row = [f"{testsuite_name}.{test_name}", value_param]

            for cpu in args.cpus:
                for key in args.tsv_columns:
                    for executable in args.executables:
                        for rep in range(args.repetitions):
                            result = results[
                                get_run_name(rep, executable, cpu)
                            ]
                            exe_test = result["testsuites"][testsuite_index][
                                "testsuite"
                            ][test_index]
                            row.append(exe_test[key])
            rows.append(row)

    return rows


def main():
    args = parse_args()

    runner = ADBRunner(
        adb_command=args.adb, serial_number=args.serial, verbose=args.verbose
    )

    # Copy executables to device
    runner.push(args.executables, args.tmpdir)

    results = {}

    for rep in range(args.repetitions):
        for cpu, thermal_zone in zip(args.cpus, args.thermal_zones):
            results.update(
                run_tests_on_cpu(runner, args, rep, cpu, thermal_zone)
            )

    with open(args.json, "w") as f:
        json.dump(results, f, indent="  ")

    with open(args.tsv, "w", newline="") as tsvfile:
        csv.writer(tsvfile, dialect="excel-tab").writerows(
            get_results_table(args, results)
        )


if __name__ == "__main__":
    main()
