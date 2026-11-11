#!/bin/sh
#
# Build the gcc-m68k docker image and run it to build the system
#

THISDIR=$(dirname "$0")
SRCROOT=$(realpath "$THISDIR/../../..")

docker build -t gcc-m68k $THISDIR
docker run -it --rm --volume "$SRCROOT:/gloworm" gcc-m68k $@

