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

#define	OPT_HELP	'h'
#define OPT_PIDFILE	'p'
#define OPT_FOREGROUND	'f'

#define OPTSTRING	"hp:f"

char	*pidfile	= NULL;
FILE	*pidfd		= NULL;
int	foreground	= 0;

void usage (FILE *out) {
	fprintf(out, "monitor: usage: monitor [-p pidfile] [-d dir] [-f] command [options]\n");
}

void die(char *msg) {
	fprintf(stderr, "monitor: ERROR: %s\n", msg);
	exit(1);
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

			case '?':
				usage(stderr);
				exit(2);
		}
	}

	if (pidfile)
		if ((pidfd = fopen(pidfile, "w")) == NULL)
			die("Failed to open PID file.");

	if (! foreground)
		daemon(1, 0);

	if (pidfd) {
		fprintf(pidfd, "%d\n", getpid());
		fclose(pidfd);
	}

	return 0;
}

