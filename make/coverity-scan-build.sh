#!/bin/sh
cd make
make -f makefile.boot REBOL_TOOL=r3-make OS_ID="${OS_ID}" CC="${CC}"
