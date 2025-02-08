 
#ifndef _PROTOTYPE_H
#define _PROTOTYPE_H

#include <kernel/kconfig.h>

#if (defined(CONFIG_SHELL_WITH_UTILS) && defined(IN_SHELL)) || (defined(CONFIG_SHELL_IN_KERNEL) && defined(IN_KERNEL))
	#define MAIN(name)	name
#else
	#define MAIN(name)	main
#endif

#endif

