FROM debian:stable-slim

RUN apt-get update && \
  DEBIAN_FRONTEND=noninteractive apt install -y curl cmake git gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib build-essential python3 && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/*

# Create app directory
ENV APP_DIR=/usr/local/builder

WORKDIR $APP_DIR

RUN useradd --uid 1000 builder && \
  usermod -a -G builder builder && \
  chown -R builder:builder $APP_DIR

USER builder
ENV PICO_SDK_PATH=${APP_DIR}/pico-sdk
RUN git clone https://github.com/raspberrypi/pico-sdk.git ${PICO_SDK_PATH} && \
  cd ${PICO_SDK_PATH} && \
  git submodule update --init
ENV CXX=g++
ENV PICO_PLATFORM=rp2040
ENV PICO_COMPILER=pico_arm_gcc
ENV PICO_BOARD_HEADER_DIRS=/usr/local/builder/src/board_config/
ENV PICO_BOARD=keyboard_converter_board
COPY --chown=1000:1000 entrypoint.sh .

ENTRYPOINT ["./entrypoint.sh"]
