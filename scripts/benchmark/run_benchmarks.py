#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

import argparse
from dataclasses import dataclass, field, KW_ONLY
import hashlib
from pathlib import Path
from typing import Any
import sys
import subprocess
import tempfile

import yaml

import run_gtest_adb

TARGET_WORKSPACE_DIR: str = "/data/local/tmp"

THIS_FILE: Path = Path(__file__)
THIS_DIR: Path = THIS_FILE.parent

@dataclass
class Variation:
    name: str
    build_dir: str

@dataclass
class Operation:
    name: str
    executable: str
    gtest_filter: str
    gtest_param_filter: str

@dataclass
class Resolution:
    params: dict[str, str]
    operations: list[Operation]

@dataclass
class RunGTestAdbArgs:
    _: KW_ONLY
    executables: list[str]
    no_push: bool = False
    adb: str = "adb"
    serial: str
    taskset_masks: list[int]
    thermal_zones: list[int]
    threads: int = 2
    tsv: str = "gtest_adb.tsv"
    json: str = "gtest_adb.json"
    tmpdir: str = "/data/local/tmp"
    tsv_columns: list[str] = field(default_factory=lambda: ["median"])
    repetitions: int = 1
    verbose: bool = False
    gtest_filter: str
    gtest_param_filter: str
    perf_min_samples: int = 100

def main():
    parser = argparse.ArgumentParser(prog=THIS_FILE.name)
    parser.add_argument(
        "--benchmarks_yaml_path",
        type=Path,
        help="Path to the benchmarks YAML file to run",
    )
    parser.add_argument(
        "--resolution",
        type=str,
        help="Name of the resolution defined in the benchmarks file to run",
    )
    parser.add_argument(
        "--serial",
        type=str,
        help="Serial of the device to connect to. "
        "Optional if only one device is connected"
    )

    args = parser.parse_args()

    with open(args.benchmarks_yaml_path) as stream:
        try:
            benchmarks = yaml.safe_load(stream)
        except yaml.YAMLError as exc:
            print(exc)

    variations = [
        Variation("vanilla", "build/opencv-vanilla"),
        Variation("kleidicv", "build/opencv-kleidicv"),
        Variation("kleidicv_sve2", "build/opencv-kleidicv-sve2")
    ]

    executables = {
        "opencv_perf_imgproc": "bin/opencv_perf_imgproc",
        "opencv_perf_core": "bin/opencv_perf_core",
        "opencv_perf_video": "bin/opencv_perf_video",
    }

    resolutions = {
        "480": {
            "pixel_format": "640x480",
            "pixel_format_half": "320x240",
            "pixel_format_quarter": "160x120",
            "pixel_format_eighth": "80x60"
        },
        "720": {
            "pixel_format": "1280x720",
            "pixel_format_half": "640x360",
            "pixel_format_quarter": "320x180",
            "pixel_format_eighth": "160x90"
        },
        "FHD": {
            "pixel_format": "1920x1080",
            "pixel_format_half": "960x540",
            "pixel_format_quarter": "480x270",
            "pixel_format_eighth": "240x135"
        },
        "4K": {
            "pixel_format": "3840x2160",
            "pixel_format_half": "1920x1080",
            "pixel_format_quarter": "960x540",
            "pixel_format_eighth": "480x270"
        }
    }

    # Parse and store referenced objects.
    operations: list[Operation] = []
    referenced_executables = set()
    for raw_operation in benchmarks["operations"]:
        operation = Operation(*raw_operation)
        operations.append(operation)
        if operation.executable not in executables:
            raise ValueError(f"Unknown executable referenced: {operation.executable}")
        referenced_executables.add(operation.executable)
    resolution = Resolution(params=resolutions[args.resolution], operations=operations)

    adb = run_gtest_adb.ADBRunner(
        adb_command="adb",
        serial_number=args.serial,
        verbose=True,
    )

    # Push referenced executables.
    for variation in variations:
        for executable in sorted(referenced_executables):
            host_path: str = f"{THIS_DIR}/../../{variation.build_dir}/{executables[executable]}"
            target_file_name: str = f"{executable}_{variation.name}"
            if not Path(host_path).is_file():
                raise FileNotFoundError(host_path)

            target_md5: str = ""
            not_on_target: bool = False
            try:
                result = adb.check_output(f"md5sum '{TARGET_WORKSPACE_DIR}/{target_file_name}'")
                target_md5 = result.split()[0]
            except subprocess.CalledProcessError:
                not_on_target = True
            host_md5 = hashlib.md5(open(host_path, "rb").read()).hexdigest()

            if not_on_target or (target_md5 != host_md5):
                adb.push([host_path], f"{TARGET_WORKSPACE_DIR}/{target_file_name}")

    with tempfile.TemporaryDirectory() as host_temp_dir:
        for operation in resolution.operations:
            params: dict[str, str] = {
                **resolution.params,
                "name": operation.name,
            }
            gtest_param_filter = operation.gtest_param_filter.format(**params)
            for variation in variations:
                target_file_name: str = f"{operation.executable}_{variation.name}"
                json_output_path: Path = Path(f"{host_temp_dir}/{operation.name}_{variation.name}_{args.resolution}")
                run_gtest_adb_args = RunGTestAdbArgs(
                    executables=[target_file_name],
                    serial=args.serial,
                    no_push=True,
                    taskset_masks=[0x30],
                    thermal_zones=[1],
                    threads=[2],
                    json=str(json_output_path),
                    gtest_filter=operation.gtest_filter,
                    gtest_param_filter=gtest_param_filter,
                )
                print(run_gtest_adb_args)
                json_output_path.write_text(f"{{'name': '{json_output_path}'}}")
                run_gtest_adb.main(run_gtest_adb_args)
                print(f"{operation.name} - {variation.name}:\n"
                      f"    exec {variation.build_dir}/{operation.executable} '{operation.gtest_filter}' '{gtest_param_filter}'")
                print(json_output_path.read_text())

if __name__ == "__main__":
    main()
