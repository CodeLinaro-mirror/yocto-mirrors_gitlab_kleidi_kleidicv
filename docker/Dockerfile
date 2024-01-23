# SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

FROM ubuntu:23.10

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get -y --no-install-recommends install \
      ca-certificates \
      cmake \
      doxygen \
      g++-aarch64-linux-gnu \
      gcc-aarch64-linux-gnu \
      git \
      gpg \
      graphviz \
      less \
      libnuma1 \
      liburing2 \
      lsb-release \
      ninja-build \
      patch \
      pipx \
      python3-six \
      shellcheck \
      software-properties-common \
      wget

# Install latest version of most LLVM tools except clang-format because
# we prioritise stability when it comes to how we format our code.
ENV LLVM_VERSION=19
RUN wget -q https://apt.llvm.org/llvm-snapshot.gpg.key -O /etc/apt/trusted.gpg.d/apt.llvm.org.asc \
    && echo "ce6eee4130298f79b0e0f09a89f93c1bc711cd68e7e3182d37c8e96c5227e2f0 /etc/apt/trusted.gpg.d/apt.llvm.org.asc" | sha256sum -c - \
    && . /etc/os-release \
    && add-apt-repository "deb http://apt.llvm.org/${VERSION_CODENAME}/ llvm-toolchain-${VERSION_CODENAME} main" \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get -y --no-install-recommends install \
        clang-${LLVM_VERSION} \
        clang-format-17 \
        clang-tidy-${LLVM_VERSION} \
        clang-tools-${LLVM_VERSION} \
        libclang-rt-${LLVM_VERSION}-dev \
        llvm-${LLVM_VERSION}

RUN pipx install cpplint==1.6.1
RUN pipx install gcovr==6.0
RUN pipx install reuse==3.0.0

ENV PATH=${PATH}:/usr/lib/llvm-${LLVM_VERSION}/bin:/root/.local/bin
ENV CC=clang-${LLVM_VERSION}
ENV CXX=clang++-${LLVM_VERSION}
