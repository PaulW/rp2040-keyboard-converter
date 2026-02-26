FROM debian:stable-slim

RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt install -y curl cmake git gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib libusb-1.0-0-dev build-essential python3 && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/*

# Create app directory
ENV APP_DIR=/usr/local/builder

WORKDIR $APP_DIR

RUN useradd --uid 1001 builder && \
  usermod -a -G builder builder && \
  chown -R builder:builder $APP_DIR

# ─── Static-analysis tools (clang-tidy, cppcheck) ────────────────────────────
# Installed in the main image so analysis can run against fully-built artefacts
# (including generated PIO headers).  Not invoked unless ANALYSE=1 is passed.
# ------------------------------------------------------------------------------
# CPPCHECK_VERSION: built from source to match the CodeRabbit version exactly.
ARG CPPCHECK_VERSION=2.19.0

RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt-get install -y \
    --no-install-recommends \
    clang \
    clang-tidy && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/* && \
  git clone --depth 1 --branch ${CPPCHECK_VERSION} \
    https://github.com/danmar/cppcheck.git /tmp/cppcheck-src && \
  cmake -S /tmp/cppcheck-src -B /tmp/cppcheck-build \
    -DCMAKE_BUILD_TYPE=Release \
    -DHAVE_RULES=OFF && \
  cmake --build /tmp/cppcheck-build --parallel "$(nproc)" && \
  cmake --install /tmp/cppcheck-build && \
  rm -rf /tmp/cppcheck-src /tmp/cppcheck-build

USER builder
# Set up pico-sdk
ARG PICO_SDK_VERSION
ENV PICO_SDK_PATH=${APP_DIR}/pico-sdk
RUN git clone --branch ${PICO_SDK_VERSION} https://github.com/raspberrypi/pico-sdk.git ${PICO_SDK_PATH} && \
  cd ${PICO_SDK_PATH} && \
  git submodule update --init
ENV CXX=g++

# Set up picotool (required from pico-sdk 2.0.0)
ENV PICO_TOOL_PATH=${APP_DIR}/picotool

RUN mkdir -p ${PICO_TOOL_PATH} && \
  mkdir -p ${PICO_TOOL_PATH}/build && \
  mkdir -p ${PICO_TOOL_PATH}/bin

RUN git clone --branch ${PICO_SDK_VERSION} https://github.com/raspberrypi/picotool.git ${PICO_TOOL_PATH}/src

RUN cd ${PICO_TOOL_PATH}/build && \
  cmake -DCMAKE_INSTALL_PREFIX=${PICO_TOOL_PATH}/bin -DPICOTOOL_FLAT_INSTALL=1 ../src/ && \
  cmake --build . && \
  cmake --install .

ENV picotool_DIR=${PICO_TOOL_PATH}/bin

ENV PICO_PLATFORM=rp2040
ENV PICO_COMPILER=pico_arm_gcc
ENV PICO_BOARD_HEADER_DIRS=/usr/local/builder/src/board_config/
ENV PICO_BOARD=keyboard_converter_board
COPY --chown=1001:1001 entrypoint.sh .
COPY --chown=1001:1001 tools/analyse.sh .
COPY --chown=1001:1001 tools/filter_compile_db.py .
COPY --chown=1001:1001 tools/parse_compile_db.py .
COPY --chown=1001:1001 .clang-tidy .

ENTRYPOINT ["./entrypoint.sh"]
