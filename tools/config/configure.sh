#
# Build the kconfig docker image and run it to configure the system before building
#

KCONFIG_CONFIG=${KCONFIG_CONFIG:-$(pwd)/.config}

docker build -t kconfig .
docker run -it --rm --env "KCONFIG_CONFIG=$KCONFIG_CONFIG" --volume $(cd ../../ && pwd):/gloworm kconfig

