#
# Build the kconfig docker image and run it to configure the system before building
#

docker build -t kconfig .
docker run -it --volume $(cd ../../ && pwd):/gloworm kconfig

