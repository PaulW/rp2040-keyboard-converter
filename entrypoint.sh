#!/bin/sh

set -e

echo "Cleaning up old Build Files..."
rm -rf ${APP_DIR}/build/*
echo "Creating Temporary Build Location..."
mkdir -p ${APP_DIR}/build-tmp
cd ${APP_DIR}/build-tmp
echo "Cleaning old build/compile files (if any)..."
rm -rf cmake_install.cmake CMakeCache.txt CMakeFiles elf2uf2 generated Makefile pico-sdk pioasm
cmake ${APP_DIR}/src
cmake --build .
mv *.elf.map ../build/
