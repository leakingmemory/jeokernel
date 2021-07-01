#!/bin/sh

tar xzvf $1

ACPICA_BUILD_DIR="`pwd`"

SRC_DIR="${ACPICA_BUILD_DIR}/acpica-unix-20210604"

cd ${SRC_DIR} && make
