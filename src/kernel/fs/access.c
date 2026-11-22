
#include <sys/types.h>
#include <attributes.h>

#include "access.h"


int verify_mode_access(uid_t current_uid, mode_t require_mode, uid_t file_uid, __unused gid_t file_gid, mode_t file_mode)
{
	// TODO implement support for groups, but searching all current user's groups to make sure they're a member of the GID

	if (current_uid == SU_UID || current_uid == file_uid) {
		return require_mode == (require_mode & ((file_mode >> 6) & 0x07));
	} else {
		return require_mode == (require_mode & (file_mode & 0x07));
	}
}

