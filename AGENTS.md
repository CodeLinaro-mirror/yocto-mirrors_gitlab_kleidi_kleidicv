<!--
SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# Agents Guide

## Repository Overview

This repository contains KleidiCV, an Arm image-processing library focused on
high-performance AArch64 implementations. The top-level build is CMake-based
and includes the main library, threading support, C examples, tests, and
benchmarks.

## Key Paths

- `CMakeLists.txt`: top-level build entry point
- `kleidicv/`: main library sources
- `kleidicv_thread/`: threading support
- `examples/`: example programs
- `test/`: API and unit tests
- `benchmark/`: benchmark targets
- `doc/`: build, test, integration, and platform documentation
- `.devcontainer/devcontainer.json`: development container configuration
- `docker/Dockerfile`: container image definition

## Workspace Tasks

Do not hardcode build or test commands in agent instructions. Use the tasks
defined in `.vscode/tasks.json` as the source of truth for common validation
flows.

Once a source file is edited run the `Format source` VS Code task.

The default validation for changes is to run the `All API tests` VS Code task but 
to keep test cycle short the `GTEST_FILTER` environment variable should be set
accordingly which tests to run.

## Development Environment

The devcontainer builds from `.devcontainer/Dockerfile`, enables `ccache`, and keeps
the workspace mounted at its real path instead of `/workspace`. The Docker
image installs the cross-compilation and LLVM tooling used by the project.
Agents should expect to be running inside a VS Code dev container unless a
task explicitly says otherwise.

## Working Notes For Agents

- Prefer small, targeted changes and avoid reformatting unrelated files.
- Check for existing uncommitted changes before editing shared setup files.
- Use the documentation in `doc/` for background, but prefer
  `.vscode/tasks.json` for build, test, formatting, and CI entry points.
- Before choosing validation steps, check whether an appropriate VS Code task
  already exists and use it when possible.
- If a task touches container or toolchain setup, review both
  `.devcontainer/devcontainer.json` and `docker/Dockerfile` together.
