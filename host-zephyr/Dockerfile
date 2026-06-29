FROM ghcr.io/zephyrproject-rtos/ci-base:main

ARG DEBIAN_FRONTEND=noninteractive
ARG ZEPHYR_SDK_VERSION=0.17.0

# Install Zephyr SDK (minimal — toolchains added per-board below)
WORKDIR /root/zephyrproject/zephyr-sdk
RUN wget --progress=dot:giga https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz \
    && wget --progress=dot:giga -O - https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/sha256.sum | shasum --check --ignore-missing \
    && tar --strip-components=1 -xf zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz \
    && rm zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz
RUN ./setup.sh -t arm-zephyr-eabi -c

# Install west
RUN pip3 install --no-cache-dir west

# Set up Zephyr workspace with minimal manifest
WORKDIR /root/zephyrproject/application
COPY west_lite.yml ./west_lite.yml
RUN west init -l --mf west_lite.yml .

WORKDIR /root/zephyrproject
RUN west update --stats

# Clone WAMR as a Zephyr module
WORKDIR /root/zephyrproject/modules
RUN git clone --depth 1 https://github.com/bytecodealliance/wasm-micro-runtime.git wasm-micro-runtime

# Copy our app source into the container
WORKDIR /root/zephyrproject/application
COPY CMakeLists.txt prj.conf ./
COPY src/ ./src/
COPY boards/ ./boards/

WORKDIR /root/zephyrproject
