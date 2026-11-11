#!/usr/bin/python3
#
# makedevs.py - generate device files inside a diskimage based on a table of devices
#
# Input File Format (whitespace-separated list of values):
#   name    type mode uid gid major minor start inc count
#
# where `type` is one of:
#   f    A regular file
#   d    Directory
#   c    Character special device file
#   b    Block special device file
#   p    Fifo (named pipe)
#   s    Socket
#
# If `count` is not '-', then it must be an integer greater than 0 , and
# `start` and `inc` must also be integers greater than or equal to 0.  It will
# create `count` files with a number appended to the name, where the first file
# will use `start`, and each subsequent file will have `inc` added to the
# previous file's number.
#
# Example:
#   /dev      d    755  0    0    -    -    -    -    -
#   /dev/mem  c    640  0    0    1    1    0    0    -
#   /dev/tty  c    666  0    0    5    0    0    0    -
#   /dev/tty  c    666  0    0    4    0    0    1    6
#   /dev/loop b    640  0    0    7    0    0    1    2
#   /dev/ata  b    640  0    0    3    0    0    0    -
#   /dev/ata  b    640  0    0    3    1    1    1    16
#   /dev/log  s    666  0    0    -    -    -    -    -
#
# The script will create files for /dev/mem, /dev/tty, /dev/ata, and /dev/log
# and also /dev/tty0 to /dev/tty5, /dev/loop0, /dev/loop1, and /dev/ata0
# to /dev/ata15.  The minor device number for /dev/tty0 to /dev/tty5 will match
# the filename, but the minor device number for /dev/ata0 will be 1, through to
# /dev/ata15 which will have minor number 16
#

import re
import os
import sys
import stat
import shutil
import socket
import logging
import os.path
import argparse


logger = logging.getLogger(__name__)


def load_input(file):
    entries = []
    f = open(file, "r") if type(file) is str else file
    try:
        for line in f:
            line = line.strip()
            m = re.match(r"^(.*)\s*#.*$", line)
            if m:
                line = m[1]
            if not line:
                continue
            columns = line.split()
            entries.append(columns)
    finally:
        f.close()
    return entries


def create_file(path, ftype, mode, uid, gid, major, minor, force=False):
    if os.path.exists(path):
        if force:
            shutil.rmtree(path)
        else:
            raise Exception(f"{path}: already exists")

    if ftype == 'f':
        logger.info(f"creating empty file {path}")
        with open(path, "w") as f:
            pass
    elif ftype == 'd':
        logger.info(f"creating directory {path} with mode={mode:o}")
        os.mkdir(path, mode=mode)
    elif ftype == 'c':
        logger.info(f"creating character device {path} with mode={mode:o} major={major} minor={minor}")
        os.mknod(path, mode=mode | stat.S_IFCHR, device=os.makedev(major, minor))
    elif ftype == 'b':
        logger.info(f"creating block device {path} with mode={mode:o} major={major} minor={minor}")
        os.mknod(path, mode=mode | stat.S_IFBLK, device=os.makedev(major, minor))
    elif ftype == 'p':
        logger.info(f"creating fifo {path} with mode={mode:o}")
        os.mkfifo(path, mode)
    elif ftype == 's':
        logger.info(f"creating socket {path} with mode={mode:o}")
        sock = socket.socket(socket.AF_UNIX)
        sock.bind(path)

    os.chown(path, uid, gid)
    os.chmod(path, mode)


def create_file_pattern(args, name, ftype, mode, uid, gid, major, minor, start, inc, count):
    path = os.path.join(args.rootdir, name.lstrip('/'))
    mode = int(mode, 8)
    uid = int(uid)
    gid = int(gid)
    major = int(major) if major.isdigit() else 0
    minor = int(minor) if minor.isdigit() else 0
    count = int(count) if count.isdigit() else 0

    if count > 0:
        for i in range(0, count):
            create_file(f"{path}{i}", ftype, mode, uid, gid, major, minor, args.force)
            minor += int(inc)
    else:
        create_file(path, ftype, mode, uid, gid, major, minor, args.force)


def main():
    parser = argparse.ArgumentParser(prog='makedevs.py', description='generate device files inside a diskimage based on a table of devices')
    parser.add_argument('-d', '--device-table', default=sys.stdin, help='input file describing the files to create')
    parser.add_argument('-f', '--force', action='store_true', help='if a file exists with the same name, it will be deleted before creating the new one')
    parser.add_argument('-v', '--verbose', action='store_true', help='print extra messages about the files being created')
    parser.add_argument('rootdir', help='the directory in which new files will be created')
    args = parser.parse_args()

    logger.addHandler(logging.StreamHandler())
    if args.verbose:
        logger.setLevel(logging.INFO)

    args.rootdir = os.path.abspath(args.rootdir)
    entries = load_input(args.device_table)
    for entry in entries:
        create_file_pattern(args, *entry)


if __name__ == '__main__':
    main()

