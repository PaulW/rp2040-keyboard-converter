#!/bin/sh

set -e

echo "Cleaning up old Build Files..."
rm -rf ${APP_DIR}/build/*
cd ${APP_DIR}/src
cmake .
make clean
make ibm-5170-pcat
mv *.elf.map ../build/