#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <syslog.h>

#include "options.h"

char	*pidfile	= NULL;
char	*workdir	= NULL;

char	*socket_path	= NULL;
char	*tag		= NULL;

int	foreground	= 0;
int	alwaysrestart	= 0;
int	maxinterval	= DEFAULT_MAXINTERVAL;
int	heartbeat	= DEFAULT_HEARTBEAT;

static void usage (FILE *out) {
	fprintf(out, "monitor: usage: monitor [-p pidfile] [-t tag] [-d dir] [-fR] command [options]\n");
}

int parse_args(int argc, char **argv) {
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

	return optind;
}

void setup_socket_path(void) {
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
}

void setup_work_dir(void) {
	// try to chdir to work directory.
	if (workdir && chdir(workdir) != 0) {
		syslog(LOG_ERR, "%s: failed to chdir: %m",
				workdir);
		exit(1);
	}
}

FILE *setup_pid_file(void) {
	FILE *pidfd = NULL;

	// open (but don't write) pid file.  We open it here so that we can
	// error out and exit if the pid file isn't writable.
	if (pidfile) {
		if ((pidfd = fopen(pidfile, "w")) == NULL) {
			syslog(LOG_ERR, "failed to open pid file \"%s\": %m", pidfile);
			exit (1);
		}
	}

	return pidfd;
}

void write_pid_file(FILE *pidfd) {
	if (pidfd) {
		fprintf(pidfd, "%d\n", getpid());
		fclose(pidfd);
	}
}

