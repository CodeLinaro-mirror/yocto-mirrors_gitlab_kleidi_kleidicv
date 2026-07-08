#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exu

cd "$(dirname "${BASH_SOURCE[0]}")/.."

sh -c "$(wget https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh -O -)" "" --unattended

if git rev-parse --git-dir >/dev/null 2>&1; then
  hooks_dir="$(git rev-parse --git-path hooks)"

  if [[ -f "${hooks_dir}/commit-msg" ]] ||
     [[ -f "${hooks_dir}/commit-msg.gerrit" ]]; then
    git config --unset core.hooksPath || true
    scripts/install-git-hooks.sh
  else
    git config core.hooksPath .githooks
  fi
fi
