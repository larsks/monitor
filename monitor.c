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

#define MAX_RETRIES	2

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

void child_died(int sig) {
	pid_t	pid;
	int	status;

	flag_child_died = 1;

	pid = wait(&status);

	if (status == 0) {
		syslog(LOG_INFO, "%s: exited with status = %d",
				child_name, WEXITSTATUS(status));
		if (! alwaysrestart)
			flag_quit = 1;
	} else if (WIFEXITED(status)) {
		syslog(LOG_ERR, "%s: exited with status = %d",
				child_name, WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		syslog(LOG_ERR, "%s: exited from signal = %d",
				child_name, WTERMSIG(status));
	}

	child_pid = 0;
}

wait_for_exit () {
	int retries = 0;

	while (child_pid && ! flag_child_died) {
		if (retries++ >= MAX_RETRIES)
			kill(child_pid, SIGKILL);

		syslog(LOG_INFO, "%s: waiting for exit.", child_name);
		sleep(1);
	}
}

void loop (int argc, char **argv) {
	int	status;
	int	interval = 0;
	time_t	lastrestart = 0;

	child_name = argv[0];

	// SIGHUP and SIGINT get passed on to the
	// child process.
	signal(SIGHUP, signal_and_restart);
	signal(SIGINT, signal_and_restart);

	// SIGTERM and SIGQUIT will get passed on to 
	// the child process and cause monitor to exit.
	signal(SIGTERM, signal_and_quit);
	signal(SIGQUIT, signal_and_quit);

	// We get this when the child process
	// dies.
	signal(SIGCHLD, child_died);

	syslog(LOG_DEBUG, "entering loop.");
	while (! flag_quit) {
		flag_child_died = 0;
		flag_received_signal = 0;

		child_pid = fork();

		switch (child_pid) {
			case -1:
				syslog(LOG_ERR, "fork() failed: %m");
				exit(1);
			case 0:
				syslog(LOG_INFO, "%s: starting", child_name);
				execvp(argv[0], argv);
				break;
		}

		// only the parent gets this far.
		while (! flag_quit) {
			pause();
			if (flag_child_died) break;
		}

		// If the child process exited with an error, activate
		// exponential backup mechanism.
		if (! flag_quit && ! flag_received_signal) {
			if (time(NULL) - lastrestart < maxinterval) {
				syslog(LOG_INFO, "%s: backing off for %d seconds.",
						child_name, interval);
				sleep(interval);
				interval = interval >= maxinterval
					? maxinterval
					: 2*interval;
			} else {
				interval = 1;
			}
		}

		lastrestart = time(NULL);
	}

cleanup:
	wait_for_exit();
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

	if (pidfile)
		unlink(pidfile);

	return 0;
}

