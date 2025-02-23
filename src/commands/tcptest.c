
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_INPUT		20

int main(int argc, char **argv)
{
	int error;
	int nbytes;
	int sockfd;
	char buffer[MAX_INPUT];
	struct sockaddr_in addr;

	char *address = "192.168.1.102";
	int port = 3904;

	if (argc >= 2)
		address = argv[1];
	if (argc >= 3)
		port = strtol(argv[2], NULL, 10);

	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("Unable to create socket: %d\n", sockfd);
		return -1;
	}

	memset(&addr, '\0', sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_aton(address, &addr.sin_addr);
	//addr.sin_addr.s_addr = 0xC0A80166;

	error = connect(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
	if (error) {
		printf("Error while connecting: %d\n", error);
		return -1;
	}

	while (1) {
		nbytes = read(STDIN_FILENO, buffer, MAX_INPUT);
		if (error < 0) {
			printf("Error reading: %d\n", error);
			break;
		}

		error = send(sockfd, buffer, nbytes, 0);
		if (error < 0) {
			printf("Error sending: %d\n", error);
			break;
		}
	}

	close(sockfd);

	return 0;
}

