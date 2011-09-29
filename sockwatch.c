#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define BUFLEN 4192

int main(int argc, char **argv) {
	int			sockfd,
				clientfd;
	struct sockaddr_un	addr,
				clientaddr;
	int			clientaddrlen;
	char			*sun_path="/tmp/mysocket";
	fd_set			readfds;
	int			quit = 0;
	struct timeval		timeout;

	unlink(sun_path);
	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, sun_path, strlen(sun_path) + 1);

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un))) {
		perror("bind");
		exit(1);
	}

	listen(sockfd, 1);
	while (! quit) {
		int res;

		timeout.tv_sec=5;
		timeout.tv_usec=0;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		if (clientfd) FD_SET(clientfd, &readfds);
		res = select(clientfd ? clientfd+1 : sockfd+1,
				&readfds, NULL, NULL, &timeout);
		printf("got res=%d\n", res);

		switch (res) {
			case -1:	perror("select");
				 	exit(1);
			case 0:		printf("timeout\n");
					break;
		}

		if (FD_ISSET(sockfd, &readfds)) {
			clientfd = accept(sockfd,
					(struct sockaddr *)&clientaddr,
					&clientaddrlen);
		}

		if (clientfd && FD_ISSET(clientfd, &readfds)) {
			char data[BUFLEN];
			size_t nbytes;

			nbytes = read(clientfd, data, BUFLEN-1);
			data[nbytes+1] = 0;

			if (! nbytes) {
				close(clientfd);
				clientfd=0;
			} else {
				printf("read %d bytes from client: %s\n",
						(int)nbytes,
						data);
			}
		}
	}

	return 0;
}

