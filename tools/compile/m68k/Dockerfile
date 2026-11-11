#
# A docker container that can compile gloworm for the m68k
#
FROM debian

RUN apt-get update
RUN apt-get install -y \
	make \
	git \
	bsdextrautils \
	gcc-m68k-linux-gnu \
	binutils-m68k-linux-gnu \
	kconfig-frontends-nox

RUN mkdir /gloworm
WORKDIR /gloworm

ENTRYPOINT ["make"]
