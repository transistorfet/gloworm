
#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>

typedef int pid_t;
typedef short device_t;

typedef unsigned short mode_t;
typedef unsigned long offset_t;
typedef unsigned int inode_t;
typedef unsigned int nlink_t;
typedef unsigned short uid_t;
typedef unsigned short gid_t;

#define SU_UID		(uid_t) 0


// Standard type names
typedef device_t dev_t;
typedef inode_t ino_t;
typedef signed long off_t;

#endif
