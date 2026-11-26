
#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>

typedef int pid_t;
typedef unsigned short dev_t;
typedef unsigned short mode_t;
typedef unsigned int ino_t;
typedef unsigned int nlink_t;
typedef unsigned short uid_t;
typedef unsigned short gid_t;

#define SU_UID		(uid_t) 0

typedef signed long off_t;
typedef signed long long loff_t;

typedef dev_t device_t;

#endif
