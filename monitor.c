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

#include <sys/wait.h>

#define	OPT_HELP	'h'
#define OPT_PIDFILE	'p'
#define OPT_FOREGROUND	'f'
#define OPT_RESTART	'R'
#define OPT_MAXINTERVAL	'I'

#define OPTSTRING	"hp:fRI:"

char	*pidfile	= NULL;
FILE	*pidfd		= NULL;
int	foreground	= 0;
int	alwaysrestart	= 0;
int	maxinterval	= 60;

void usage (FILE *out) {
	fprintf(out, "monitor: usage: monitor [-p pidfile] [-d dir] [-f] command [options]\n");
}

void die(char *msg) {
	fprintf(stderr, "monitor: ERROR: %s\n", msg);
	exit(1);
}

void loop (int argc, char **argv) {
	pid_t	pid, pid2;
	int	status;
	int	interval = 2;
	time_t	lastrestart = 0;

	fprintf(stderr, "entering loop\n");
	while (1) {
		pid = fork();

		switch (pid) {
			case -1:
				exit(1);
			case 0:
				execvp(argv[0], argv);
				break;
			default:
				pid2 = waitpid(pid, &status, 0);
				fprintf(stderr, "got status = %d\n", status);
				if (status == 0)
					goto loop_exit;

				if (WIFSIGNALED(status)) {
					fprintf(stderr, "process exited due to signal = %d\n",
							WTERMSIG(status));
				} else if (WIFEXITED(status)) {
					fprintf(stderr, "process exited with status = %d\n",
							WEXITSTATUS(status));
				}

				fprintf(stderr, "%d %d %d\n", time(NULL), (int)lastrestart, interval);
				if (time(NULL) - lastrestart <= interval) {
					fprintf(stderr,"sleeping %d seconds before restart.\n", interval);
					interval = interval
						? ( interval >= maxinterval
							? maxinterval
							: 2 * interval )
						: 1;
					sleep(interval);
				}

				lastrestart = time(NULL);
				break;
		}
	}

loop_exit:
	return;
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

	loop(argc-optind, argv+optind);
	return 0;
}

