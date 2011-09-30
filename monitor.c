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

#ifdef DEBUG
#define _DEBUG(x) (x)
#else
#define _DEBUG(x)
#endif

#define MAX_RETRIES	2

#define STATE_STARTING	0
#define STATE_STARTED	1
#define STATE_STOPPING	2
#define STATE_STOPPED	3
#define STATE_SLEEPING	4
#define STATE_QUITTING	5

struct child_context {
	int	state;
	int	target;

	int	argc;
	char	**argv;
	pid_t	pid;
	int	status;

	time_t	lastexit;
	time_t	interval;
	time_t	ticks;

	zloop_t	*loop;
};

int	flag_child_died	= 0;
int	flag_received_signal = 0;
int	flag_quit	= 0;

int start_child(struct child_context *cstate) {
	pid_t pid = fork();

	cstate->pid = pid;

	if (pid == -1) {
		syslog(LOG_ERR, "fork failed: %m");
		return -1;
	} else if (pid == 0) {
		syslog(LOG_INFO, "%s: starting.", cstate->argv[0]);
		execvp(cstate->argv[0], cstate->argv);

		// We should never get here.
		return -1;
	}

	cstate->state = STATE_STARTED;
	return 0;
}

int stop_child(struct child_context *cstate) {
	kill(SIGTERM, cstate->pid);
	return 0;
}

int stop_sleeping(zloop_t *loop, zmq_pollitem_t *item, void *data) {
	struct child_context *cstate;
	cstate = (struct child_context *)data;

	syslog(LOG_DEBUG, "waking up from sleep");
	cstate->state = STATE_STARTING;
	return 0;
}

int check_if_running(struct child_context *cstate) {
	pid_t	pid;
	int	status;

	pid = waitpid(-1, &status, WNOHANG);

	if (pid != 0) {
		if (pid == -1) {
			syslog(LOG_ERR, "child is not running.");
		} else {
			if (WIFEXITED(status)) {
				syslog(LOG_ERR, "child exited with status = %d", WEXITSTATUS(status));
				if (WEXITSTATUS(status) == 0 && !alwaysrestart) {
					cstate->state = STATE_QUITTING;
					goto exit_func;
				}
			} else if (WIFSIGNALED(status)) {
				syslog(LOG_ERR, "child exited from signal = %d", WTERMSIG(status));
			} else {
				syslog(LOG_ERR, "child exited for no good reason.");
			}
		}

		if (time(NULL) - cstate->lastexit < maxinterval) {
			cstate->state = STATE_SLEEPING;
			cstate->interval = cstate->interval ? cstate->interval*2 : 1;

			if (cstate->interval > maxinterval)
				cstate->interval = maxinterval;

			assert(cstate->interval > 0 && cstate->interval <= maxinterval);

			syslog(LOG_ERR, "child is restarting too frequently; sleeping for %d seconds.",
					cstate->interval);

			zloop_timer(cstate->loop, cstate->interval*1000, 1, stop_sleeping, (void *)cstate);
		} else {
			cstate->interval = 0;
			cstate->state = STATE_STARTING;
		}
		
		cstate->lastexit = time(NULL);
	}

exit_func:
	return 0;
}

int check_if_wake(struct child_context *cstate) {
	cstate->ticks -= 1;

	syslog(LOG_DEBUG, "sleeping, interval=%d, ticks=%d",
			cstate->interval, cstate->ticks);

	if (cstate->ticks == 0)
		cstate->state = STATE_STARTING;

	return 0;
}

int periodic(zloop_t *loop, zmq_pollitem_t *item, void *data) {
	pid_t 	pid;
	int	rc = 0;
	struct child_context *cstate;

	cstate = (struct child_context *)data;

	_DEBUG(syslog(LOG_DEBUG, "at top; state=%d", cstate->state));

	switch (cstate->state) {
		case STATE_STARTING:
			rc = start_child(cstate);
			break;

		case STATE_STARTED:
			rc = check_if_running(cstate);
			break;

		case STATE_STOPPING:
			rc = stop_child(cstate);
			break;

		case STATE_STOPPED:
			// nothing to do here.
			break;

		case STATE_SLEEPING:
			// nothing to do here.
			break;

		case STATE_QUITTING:
			stop_child(cstate);
			rc = -1;
			break;
	}

	_DEBUG(syslog(LOG_DEBUG, "at bottom; state=%d, rc=%d", cstate->state, rc));

	return rc;
}

char *state_to_string(int state) {
	char *str;

	switch (state) {
		case STATE_STARTING:
			str = "starting";
			break;

		case STATE_STARTED:
			str = "started";
			break;

		case STATE_STOPPING:
			str = "stopping";
			break;

		case STATE_STOPPED:
			str = "stopped";
			break;

		case STATE_SLEEPING:
			str = "sleeping";
			break;

		case STATE_QUITTING:
			str = "quitting";
			break;
	}

	return str;
}

int handle_msg(zloop_t *loop, zmq_pollitem_t *item, void *data) {
	char *msg;
	struct child_context *cstate;

	cstate = (struct child_context *)data;

	printf("handling message.\n");
	msg = zstr_recv(item->socket);
	
	if (strcmp(msg, "status") == 0) {
		zstr_send(item->socket, state_to_string(cstate->state));
	} else if (strcmp(msg, "start") == 0) {
		zstr_send(item->socket, "");
		if (cstate->state != STATE_STARTED)
			cstate->state = STATE_STARTING;
	} else if (strcmp(msg, "stop") == 0) {
		zstr_send(item->socket, "");
		if (cstate->state != STATE_STOPPED)
			cstate->state = STATE_STOPPING;
	} else if (strcmp(msg, "quit") == 0) {
		zstr_send(item->socket, "");
		cstate->state = STATE_QUITTING;
	} else {
		zstr_send(item->socket, "?");
	}

	return 0;
}

void start_z_loop (int argc, char **argv) {
	zctx_t		*context;
	zloop_t		*loop;
	zmq_pollitem_t	socket_poll = {0};
	void		*socket = NULL;
	int		rc;
	struct child_context cstate;

	memset(&cstate, 0, sizeof(struct child_context));

	cstate.state = STATE_STOPPED;
	cstate.target = STATE_STARTED;
	cstate.argc = argc;
	cstate.argv = argv;
	cstate.pid = 0;

	context = zctx_new();
	assert(context != NULL);

	loop = zloop_new();
	assert(loop != NULL);

	cstate.loop = loop;

	rc = zloop_timer(loop, heartbeat, 0, periodic, (void *)&cstate);
	assert(rc == 0);

	if (socket_path) {
		zmq_pollitem_t socket_poll;

		socket = zsocket_new(context, ZMQ_REP);
		assert(socket != NULL);

		rc = zsocket_bind(socket, socket_path);
		assert(rc == 0);

		socket_poll.socket = socket;
		socket_poll.events = ZMQ_POLLIN;
		zloop_poller(loop, &socket_poll, handle_msg, (void *)&cstate);
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
		daemon(1, 0);

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

