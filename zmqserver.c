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
	zmq_pollitem_t
		items [1];

	zmq_bind (responder, "ipc:///tmp/mysocket");
	items[0].socket = responder;
	items[0].events = ZMQ_POLLIN;

	while (1) {
		zmq_msg_t	request;
		zmq_msg_t	reply;
		char		*msg;
		int		msglen;
		int		rc;

		rc = zmq_poll(items, 1, 500);

		if (rc < 0)
			break;

		if (rc) {
			zmq_msg_init (&request);
			zmq_recv (responder, &request, 0);
			msglen = zmq_msg_size(&request);
			msg = (char *)malloc(msglen + 1);
			memcpy(msg, zmq_msg_data(&request), msglen);
			msg[msglen] = 0;
			zmq_msg_close (&request);

			printf("got: %s\n", msg);
		} else {
			printf("nothing to receive.\n");
		}

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
