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

#ifndef DEFAULT_SOCKET_URI
#define DEFAULT_SOCKET_URI "ipc:///var/run/monitor/%s"
#endif

#define MAX_RETRIES	2

#define	OPT_HELP	'h'
#define OPT_PIDFILE	'p'
#define OPT_FOREGROUND	'f'
#define OPT_RESTART	'R'
#define OPT_MAXINTERVAL	'I'
#define OPT_WORKDIR	'd'
#define OPT_TAG		't'
#define OPT_SOCKET	'S'

#define OPTSTRING	"hp:fRI:d:t:S:"

#define STATE_STARTED	0
#define STATE_STOPPED	1
#define STATE_SLEEPING	2
#define STATE_QUIT	3

struct child_context {
	int	state;

	int	argc;
	char	**argv;
	pid_t	pid;
	int	status;

	time_t	laststart;
	time_t	interval;
	time_t	ticks;
};

char	*pidfile	= NULL;
char	*workdir	= NULL;
FILE	*pidfd		= NULL;

char	*socket_path	= NULL;
char	*tag		= NULL;

int	foreground	= 0;
int	alwaysrestart	= 0;
int	maxinterval	= 60;

int	flag_child_died	= 0;
int	flag_received_signal = 0;
int	flag_quit	= 0;

pid_t	child_pid;
char	*child_name	= NULL;

void usage (FILE *out) {
	fprintf(out, "monitor: usage: monitor [-p pidfile] [-d dir] [-f] command [options]\n");
}

void signal_and_quit (int sig) {
	flag_quit = 1;
	syslog(LOG_INFO, "caught signal %d.", sig);
	if (child_pid) kill(child_pid, sig);
}

void signal_and_restart (int sig) {
	syslog(LOG_INFO, "caught signal %d.", sig);
	if (child_pid) kill(child_pid, sig);
}

int handle_child_start(struct child_context *cstate) {
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

	return 0;
}

int handle_child_exit(struct child_context *cstate) {
	cstate->state = STATE_QUIT;
}

int periodic(zloop_t *loop, zmq_pollitem_t *item, void *data) {
	pid_t 	pid;
	int	rc = 0;
	struct child_context *cstate;

	cstate = (struct child_context *)data;
	pid = waitpid(-1, &(cstate->status), WNOHANG);

	syslog(LOG_DEBUG, "pid = %d, state = %d", 
			pid, cstate->state);

	if (cstate->state == STATE_STARTED) {
		if (pid == -1) {
			// no processes were running.
			rc = handle_child_start(cstate);
		} else if (pid > 0) {
			// child process has died.
			rc = handle_child_exit(cstate);
		}
	} else if (cstate->state == STATE_SLEEPING) {
		cstate->ticks--;
		if (! cstate->ticks)
			cstate->state = STATE_STARTED;
	} else if (cstate->state == STATE_STOPPED) {
		// Nothing to do here.
	} else if (cstate->state == STATE_QUIT) {
		rc = -1;
	}

	return rc;
}

int handle_msg(zloop_t *loop, zmq_pollitem_t *item, void *data) {
	char *msg;

	printf("handling message.\n");
	msg = zstr_recv(item->socket);
	printf("received: %s", msg);
	zstr_send(item->socket, "Thanks.");
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
	cstate.argc = argc;
	cstate.argv = argv;
	cstate.pid = 0;

	context = zctx_new();
	assert(context != NULL);

	loop = zloop_new();
	assert(loop != NULL);

	rc = zloop_timer(loop, 1000, 0, periodic, (void *)&cstate);
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
	int c;

	while (EOF != (c = getopt(argc, argv, OPTSTRING))) {
		switch(c) {
			case OPT_HELP:
				usage(stdout);
				exit(0);
			case OPT_PIDFILE:
				pidfile = strdup(optarg);
				break;
			case OPT_FOREGROUND:
				foreground = 1;
				break;
			case OPT_RESTART:
				alwaysrestart = 1;
				break;
			case OPT_MAXINTERVAL:
				maxinterval = atoi(optarg);
				break;
			case OPT_WORKDIR:
				workdir = strdup(optarg);
				break;
			case OPT_TAG:
				tag = strdup(optarg);
				break;
			case OPT_SOCKET:
				socket_path = strdup(optarg);
				break;

			case '?':
				usage(stderr);
				exit(2);
		}
	}

	openlog("monitor", LOG_PERROR, LOG_DAEMON);

	if (! (argc-optind > 0)) {
		syslog(LOG_ERR, "no command specified.");
		exit(1);
	}

	// If the user has not provided an explicit socket AND
	// they have provided a tag, we can compute the value
	// of socket_path.  Otherwise there is no control
	// socket.
	if (socket_path == NULL && tag != NULL) {
		char *sock_tmpl = getenv("MONITOR_SOCKET_URI")
			? getenv("MONITOR_SOCKET_URI")
			: DEFAULT_SOCKET_URI;

		socket_path = (char *)malloc(strlen(sock_tmpl) + strlen(tag) + 2);
		sprintf(socket_path, sock_tmpl, tag);
	}

	if (socket_path)
		syslog(LOG_DEBUG, "using socket %s", socket_path);

	// try to chdir to work directory.
	if (workdir && chdir(workdir) != 0) {
		syslog(LOG_ERR, "%s: failed to chdir: %m",
				workdir);
		exit(1);
	}

	// open (but don't write) pid file.  We open it here so that we can
	// error out and exit if the pid file isn't writable.
	if (pidfile) {
		if ((pidfd = fopen(pidfile, "w")) == NULL) {
			syslog(LOG_ERR, "failed to open pid file \"%s\": %m", pidfile);
			exit (1);
		}
	}

	if (! foreground)
		daemon(1, 0);

	if (pidfd) {
		fprintf(pidfd, "%d\n", getpid());
		fclose(pidfd);
	}

	start_z_loop(argc-optind, argv+optind);

	if (pidfile)
		unlink(pidfile);

	return 0;
}

