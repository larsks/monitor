/*
 * monitor - a simple supervisor
 *
 * usage: monitor [-p <pidfile>] [-d <dir>] [-f] command [options]
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <sys/wait.h>

#include <czmq.h>

#include "options.h"
#include "context.h"

#ifdef DEBUG
#define _DEBUG(x) (x)
#else
#define _DEBUG(x)
#endif

#define MAX_RETRIES	2

void start_child(void *cstate) {
	pid_t	pid;

	syslog(LOG_DEBUG, "start_child");
	pid = fork();
	assert(pid >= 0);

	if (pid == 0) {
		execvp(CONTEXT(cstate)->argv[0],
				CONTEXT(cstate)->argv);
		syslog(LOG_CRIT, "exec failed: %m");
		_exit(1);
	}

	CONTEXT(cstate)->pid = pid;
	CONTEXT(cstate)->state = STATE_STARTED;
	syslog(LOG_INFO, "started %s with pid = %d",
			CONTEXT(cstate)->argv[0],
			(int)pid);
	return;
}

int check_if_running(void *cstate) {
	pid_t	pid;

	pid = waitpid(CONTEXT(cstate)->pid,
			&(CONTEXT(cstate)->status),
			WNOHANG);

	return pid;
}

int wakeup(zloop_t *loop, zmq_pollitem_t *item, void *cstate) {
	if (CONTEXT(cstate)->state == STATE_SLEEPING) {
		syslog(LOG_DEBUG, "waking up!");
		CONTEXT(cstate)->state = STATE_STOPPED;
	}
}

void register_wakeup(void *cstate) {
	int rc;

	syslog(LOG_DEBUG, "registering wakeup call in %d seconds.",
			CONTEXT(cstate)->interval);

	rc = zloop_timer(CONTEXT(cstate)->loop,
			CONTEXT(cstate)->interval * 1000,
			1,
			wakeup,
			cstate);

	assert(rc == 0);
}

void handle_exit(void *cstate, pid_t pid) {
	int	status;
	time_t	now;

	syslog(LOG_DEBUG, "handle_exit");
	assert(pid > 0);

	now = time(NULL);
	status = CONTEXT(cstate)->status;

	if (WIFEXITED(status)) {
		syslog(LOG_ERR, "child exited with status = %d",
				WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		syslog(LOG_ERR, "child exited from signal = %d",
				WTERMSIG(status));
	} else {
		syslog(LOG_ERR, "child exited for no good reason.");
	}

	if (now - CONTEXT(cstate)->lastexit <= maxinterval) {
		CONTEXT(cstate)->interval = CONTEXT(cstate)->interval
			? CONTEXT(cstate)->interval * 2
			: 1;
		if (CONTEXT(cstate)->interval > maxinterval)
			CONTEXT(cstate)->interval = maxinterval;

		register_wakeup(cstate);

		CONTEXT(cstate)->state = STATE_SLEEPING;
	} else {
		CONTEXT(cstate)->state = STATE_STOPPED;
		CONTEXT(cstate)->interval = 0;
	}

	CONTEXT(cstate)->lastexit = now;
}

void stop_child(void *cstate, int next_state) {
	syslog(LOG_DEBUG, "stop_child");

	if (check_if_running(cstate) == 0) {
		syslog(LOG_DEBUG, "sending SIGTERM");
		kill(SIGTERM, CONTEXT(cstate)->pid);
	} else {
		syslog(LOG_INFO, "child has stopped.");
		CONTEXT(cstate)->state = next_state;
		CONTEXT(cstate)->interval = 0;
	}
}

int periodic(zloop_t *loop, zmq_pollitem_t *item, void *cstate) {
	pid_t 	pid;
	int	rc = 0;

	_DEBUG(syslog(LOG_DEBUG, "at top; state=%s, want=%s",
				state_to_string(CONTEXT(cstate)->state),
				state_to_string(CONTEXT(cstate)->want)));

	switch (CONTEXT(cstate)->state) {
		case STATE_STARTED:
			switch (CONTEXT(cstate)->want) {
				case STATE_STOPPED:
					stop_child(cstate, STATE_STOPPED);
					break;
				case STATE_QUITTING:
					stop_child(cstate, STATE_QUITTING);
					break;
				case STATE_STARTED:
					if ((pid = check_if_running(cstate)) != 0)
						handle_exit(cstate, pid);
					break;
			}
			break;

		case STATE_STOPPED:
			switch (CONTEXT(cstate)->want) {
				case STATE_STOPPED:
					break;
				case STATE_QUITTING:
					CONTEXT(cstate)->state = STATE_QUITTING;
					break;
				case STATE_STARTED:
					start_child(cstate);
					break;
			}
			break;

		case STATE_SLEEPING:
			switch (CONTEXT(cstate)->want) {
				case STATE_STOPPED:
					CONTEXT(cstate)->state = STATE_STOPPED;
					CONTEXT(cstate)->interval = 0;
					break;
				case STATE_QUITTING:
					CONTEXT(cstate)->state = STATE_QUITTING;
					CONTEXT(cstate)->interval = 0;
					break;
			}
			break;

		case STATE_QUITTING:
			rc = -1;
			break;
	}

	_DEBUG(syslog(LOG_DEBUG, "at bottom; state=%s, want=%s",
				state_to_string(CONTEXT(cstate)->state),
				state_to_string(CONTEXT(cstate)->want)));

	return rc;
}

int handle_msg(zloop_t *loop, zmq_pollitem_t *item, void *cstate) {
	char *msg;

	printf("handling message.\n");
	msg = zstr_recv(item->socket);
	
	if (strcmp(msg, "status") == 0) {
		zstr_send(item->socket, state_to_string(CONTEXT(cstate)->state));
	} else if (strcmp(msg, "stop") == 0) {
		CONTEXT(cstate)->want = STATE_STOPPED;
		zstr_send(item->socket, "");
	} else if (strcmp(msg, "start") == 0) {
		CONTEXT(cstate)->want = STATE_STARTED;
		zstr_send(item->socket, "");
	} else if (strcmp(msg, "quit") == 0) {
		CONTEXT(cstate)->want = STATE_QUITTING;
		zstr_send(item->socket, "");
	} else {
		zstr_send(item->socket, "?");
	}

	return 0;
}

void register_periodic(void *cstate) {
	int rc;
	rc = zloop_timer(CONTEXT(cstate)->loop,
			heartbeat, 0, periodic, cstate);
	assert(rc == 0);
}

void register_socket(void *cstate) {
	int		rc;
	zmq_pollitem_t	socket_poll;
	void		*socket = NULL;

	socket = zsocket_new(CONTEXT(cstate)->zcontext, ZMQ_REP);
	assert(socket != NULL);

	rc = zsocket_bind(socket, socket_path);
	assert(rc == 0);

	socket_poll.socket = socket;
	socket_poll.events = ZMQ_POLLIN;
	zloop_poller(CONTEXT(cstate)->loop,
			&socket_poll, handle_msg, cstate);
}

void start_z_loop (int argc, char **argv) {
	zctx_t			*context;
	zloop_t			*loop;
	zmq_pollitem_t		socket_poll = {0};
	int			rc;
	struct child_context	cstate;

	memset(&cstate, 0, sizeof(struct child_context));

	context = zctx_new();
	assert(context != NULL);

	loop = zloop_new();
	assert(loop != NULL);

	cstate.state	= STATE_STOPPED;
	cstate.want	= STATE_STARTED;
	cstate.argc	= argc;
	cstate.argv	= argv;
	cstate.pid	= 0;
	cstate.loop	= loop;
	cstate.zcontext	= context;

	register_periodic(&cstate);

	if (socket_path) {
		register_socket(&cstate);
	}

	zloop_start(loop);
}

int main (int argc, char **argv) {
	int	optind;
	FILE	*pidfd;

	optind = parse_args(argc, argv);

	openlog("monitor", LOG_PERROR, LOG_DAEMON);

	if (! (argc-optind > 0)) {
		syslog(LOG_ERR, "no command specified.");
		exit(1);
	}

	setup_socket_path();
	setup_work_dir();
	pidfd = setup_pid_file();

	if (! foreground)
		background(1, 0);

	write_pid_file(pidfd);

	if (socket_path)
		syslog(LOG_DEBUG, "using socket: %s", socket_path);
	if (workdir)
		syslog(LOG_DEBUG, "using directory: %s", workdir);

	start_z_loop(argc-optind, argv+optind);

	if (pidfile)
		unlink(pidfile);

	return 0;
}

