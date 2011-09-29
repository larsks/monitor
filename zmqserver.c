#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <zmq.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

int main(int argc, char **argv) {
	void	*context = zmq_init(1);
	void	*responder = zmq_socket (context, ZMQ_REP);

	zmq_bind (responder, "ipc:///tmp/mysocket");

	while (1) {
		zmq_msg_t	request;
		zmq_msg_t	reply;
		char		*msg;

		printf("init\n");
		zmq_msg_init (&request);
		printf("recv\n");
		zmq_recv (responder, &request, 0);
		msg = (char *)malloc(zmq_msg_size(&request) + 1);
		memcpy(msg, zmq_msg_data(&request), zmq_msg_size(&request));
		zmq_msg_close (&request);

		printf("got: %s\n", msg);

		// Do some 'work'
		printf("sleep\n");
		sleep (1);

		// Send reply back to client
		zmq_msg_init_size (&reply, 6);
		memcpy (zmq_msg_data (&reply), "Thanks", 6);
		printf("send\n");
		zmq_send (responder, &reply, 0);
		zmq_msg_close (&reply);
	}
	// We never get here but if we did, this would be how we end
	zmq_close (responder);
	zmq_term (context);
	return 0;
}

