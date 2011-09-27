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

#include <sys/wait.h>

#define	OPT_HELP	'h'
#define OPT_PIDFILE	'p'
#define OPT_FOREGROUND	'f'
#define OPT_RESTART	'R'
#define OPT_MAXINTERVAL	'I'
#define OPT_WORKDIR	'd'

#define OPTSTRING	"hp:fRI:d:"

char	*pidfile	= NULL;
char	*workdir	= NULL;
FILE	*pidfd		= NULL;
int	foreground	= 0;
int	alwaysrestart	= 0;
int	maxinterval	= 60;

void usage (FILE *out) {
	fprintf(out, "monitor: usage: monitor [-p pidfile] [-d dir] [-f] command [options]\n");
}

void loop (int argc, char **argv) {
	pid_t	pid, pid2;
	int	status;
	int	interval = 0;
	time_t	lastrestart = 0;

	syslog(LOG_DEBUG, "entering loop.");
	while (1) {
		pid = fork();

		switch (pid) {
			case -1:
				syslog(LOG_ERR, "fork() failed: %m");
				exit(1);
			case 0:
				syslog(LOG_INFO, "%s: starting", argv[0]);
				execvp(argv[0], argv);
				break;
			default:
				pid2 = waitpid(pid, &status, 0);

				if (pid2 == -1) {
					syslog(LOG_ERR, "waitpid failed: %m");
					exit(1);
				}

				if (status == 0 && ! alwaysrestart)
					goto loop_exit;

				if (WIFSIGNALED(status)) {
					syslog(LOG_ERR, "%s: exited due to signal %d",
							argv[0], WTERMSIG(status));
				} else if (WIFEXITED(status)) {
					syslog(LOG_ERR, "%s: exited with status %d",
							argv[0], WEXITSTATUS(status));
				}

				if (time(NULL) - lastrestart <= maxinterval) {
					interval = interval
						? ( interval >= maxinterval
							? maxinterval
							: 2 * interval )
						: 1;
				} else {
					interval = 0;
				}

				if (interval) {
					syslog(LOG_INFO, "%s: waiting %d seconds before restart.",
							argv[0], interval);
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
			case OPT_WORKDIR:
				workdir = strdup(optarg);
				break;

			case '?':
				usage(stderr);
				exit(2);
		}
	}

	openlog("monitor", LOG_PERROR, LOG_DAEMON);

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

	loop(argc-optind, argv+optind);
	return 0;
}

