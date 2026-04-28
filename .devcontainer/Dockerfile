
FROM --platform=linux/arm64 ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential gdb \
    clang-12 clangd-12 lldb-12 \
    make cmake pkg-config \
    bear \
    git curl wget \
    netcat-openbsd telnet \
    strace ltrace \
    python3 python3-pip \
    locales zsh \
 && update-alternatives --install /usr/bin/clang   clang   /usr/bin/clang-12   100 \
 && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-12 100 \
 && update-alternatives --install /usr/bin/clangd  clangd  /usr/bin/clangd-12  100 \
 && locale-gen en_GB.UTF-8 \
 && rm -rf /var/lib/apt/lists/*

ENV LANG=en_GB.UTF-8 LC_ALL=en_GB.UTF-8
WORKDIR /workspace
CMD ["sleep", "infinity"]
