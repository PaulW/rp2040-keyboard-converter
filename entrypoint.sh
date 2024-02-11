#!/bin/sh

set -e

echo "Cleaning up old Build Files..."
rm -rf ${APP_DIR}/build/*
cd ${APP_DIR}/src
echo "Cleaning old build/compile files (if any)..."
rm -rf cmake_install.cmake CMakeCache.txt CMakeFiles elf2uf2 generated Makefile pico-sdk pioasm
cmake .
cmake --build .
mv *.elf.map ../build/
