
#include <unistd.h>

extern int main(int argc, char **argv, char **env);

void _start(int argc, char **argv, char **env)
{
	int error;

	environ = env;
	error = main(argc, argv, env);
	exit(error);
}

