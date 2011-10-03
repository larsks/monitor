#ifndef _CONTEXT_H
#define _CONTEXT_H

#include <czmq.h>

#define STATE_INITIAL	0
#define STATE_STARTING	1
#define STATE_STARTED	2
#define STATE_STOPPING	3
#define STATE_STOPPED	4
#define STATE_SLEEPING	5
#define STATE_EXIT	6

#define CONTEXT(x) ((struct child_context *)x)

struct child_context {
	int	state;		// where we're at
	int	want;		// where we're going

	int	argc;
	char	**argv;

	pid_t	pid;		// PID of child process.
	int	status;		// Exit status (from waitpid()) of child process.
	int	running;	// Are we running?

	time_t	lastexit;	// time of last process exit.
	time_t	interval;	// how long to wait until next start.

	zloop_t	*loop;
	zctx_t	*zcontext;
};

char *state_to_string(int);

#endif

