#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <czmq.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

int quit = 0;

void sighandler(int sig) {
	fprintf(stderr, "caught signal %d.\n", sig);
}

int handle_msg(zloop_t *loop, zmq_pollitem_t *item, void *data) {
	char *msg;

	printf("handling message.\n");
	msg = zstr_recv(item->socket);
	printf("received: %s\n", msg);
	zstr_send(item->socket, "Thanks.");
	return 0;
}

int idle(zloop_t *loop, zmq_pollitem_t *item, void *data) {
	printf("idle...\n");
}

int main(int argc, char **argv) {
	zctx_t	*context;
	zloop_t	*loop;
	zmq_pollitem_t
		socket_poll;
	void	*socket;
	int	rc;

	context = zctx_new();
	assert(context != NULL);
	socket = zsocket_new(context, ZMQ_REP);
	assert(socket != NULL);

	rc = zsocket_bind(socket, "ipc:///tmp/mysocket");
	assert(rc == 0);

	signal(SIGINT, sighandler);

	loop = zloop_new();
	socket_poll.socket = socket;
	socket_poll.events = ZMQ_POLLIN;
	zloop_poller(loop, &socket_poll, handle_msg, NULL);
	zloop_timer(loop, 1000, 0, idle, NULL);

	while (!quit) {
		zloop_start(loop);
	}
}

