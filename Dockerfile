# Build the AAMD@home CPU saxpy ELF with the public PS5 payload SDK.
# GPU (RADV) titles need mihawk-99/PS5_Vulkan + PS5_Mesa; see docs/FEASIBILITY.md.
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates clang-18 lld-18 llvm-18 make python3 wget xz-utils \
        pkg-config sudo unzip file \
    && ln -sf /usr/bin/clang-18 /usr/bin/clang \
    && ln -sf /usr/bin/clang++-18 /usr/bin/clang++ \
    && ln -sf /usr/bin/ld.lld-18 /usr/bin/ld.lld \
    && rm -rf /var/lib/apt/lists/*

# Same tarball the payload-dev CI uses (includes clang wrappers + stubs).
RUN wget -q -O /tmp/ps5-payload-dev.tar.gz \
        https://github.com/ps5-payload-dev/pacbrew-repo/releases/latest/download/ps5-payload-dev.tar.gz \
    && tar xf /tmp/ps5-payload-dev.tar.gz -C / \
    && rm /tmp/ps5-payload-dev.tar.gz

ENV PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
WORKDIR /src
COPY . /src
RUN make host-test && make cpu \
    && mkdir -p dist && cp probes/00-vulkan-saxpy/cpu/aamd-probe.elf dist/aamd-probe.elf

# docker build -t aamd-probe .
# docker run --rm aamd-probe ls -l dist/aamd-probe.elf
CMD ["ls", "-l", "dist/aamd-probe.elf", "probes/00-vulkan-saxpy/cpu/aamd-probe.elf"]
