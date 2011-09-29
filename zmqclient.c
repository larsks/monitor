#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <zmq.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

int main(int argc, char **argv) {
	void		*context = zmq_init(1);
	void		*requester = zmq_socket (context, ZMQ_REQ);
	zmq_msg_t	request;
	zmq_msg_t	reply;
	char		*msg;
	int		i;

	zmq_connect (requester, "ipc:///tmp/mysocket");

	for (i=1; i<argc; i++) {
		char	*msg = argv[i];
		int	msglen = strlen(msg);

		zmq_msg_init_size(&request, msglen);
		memcpy(zmq_msg_data(&request), msg, msglen);
		printf("sending: %s\n", msg);
		zmq_send(requester, &request, 0);
		zmq_msg_close(&request);

		zmq_msg_init(&reply);
		zmq_recv(requester, &reply, 0);
		zmq_msg_close(&reply);
	}

	zmq_close (requester);
	zmq_term (context);

	return 0;
}

