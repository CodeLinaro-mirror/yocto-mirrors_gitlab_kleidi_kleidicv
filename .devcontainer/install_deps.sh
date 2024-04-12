#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -exu

sudo apt-get update
DEBIAN_FRONTEND=noninteractive sudo apt-get install --no-install-recommends -y \
    qemu-user \
    "clangd-${LLVM_VERSION}" \
    ccache \
    gdb-multiarch \
    git-email \
    libemail-valid-perl \
    nano

# Needed to run pipx packages originally installed for the root user
sudo chmod +x /root
