#ifndef _CONTEXT_H
#define _CONTEXT_H

#include <czmq.h>

#define STATE_INITIAL	0
#define STATE_STARTING	1
#define STATE_STARTED	2
#define STATE_STOPPING	3
#define STATE_STOPPED	4
#define STATE_SLEEPING	5
#define STATE_QUITTING	6

#define CONTEXT(x) ((struct child_context *)x)

struct child_context {
	int	state;		// where we're at
	int	state_prev;	// where we've been
	int	state_next;	// where we're going

	int	argc;
	char	**argv;

	int	running;	// TRUE if the process is running.
	pid_t	pid;		// PID of child process.
	int	status;		// Exit status (from waitpid()) of child process.

	time_t	lastexit;	// time of last process exit.
	time_t	interval;	// how long to wait until next start.

	zloop_t	*loop;
};

char *state_to_string(int);

#endif

