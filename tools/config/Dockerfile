#
# A docker container that uses Debian's kconfig package, extracted from the
# Linux kernel source, to configure the build system
#
FROM debian

RUN apt-get update
RUN apt-get install -y kconfig-frontends-nox

RUN mkdir /gloworm
WORKDIR /gloworm

CMD ["/usr/bin/kconfig-mconf", "/gloworm/Kconfig"]
