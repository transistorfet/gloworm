#!/bin/sh
#
# Build the kconfig docker image and run it to configure the system before building
#

THISDIR=$(dirname "$0")
SRCROOT=$(realpath "$THISDIR/../..")
KCONFIG_CONFIG=${KCONFIG_CONFIG:-$SRCROOT/.config}

docker build -t kconfig $THISDIR
docker run -it --rm --env "KCONFIG_CONFIG=$KCONFIG_CONFIG" --volume "$SRCROOT:/gloworm" kconfig

