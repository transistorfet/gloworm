
#include <unistd.h>

#include "prototype.h"

int MAIN(command_sync)(void)
{
	sync();
	return 0;
}
