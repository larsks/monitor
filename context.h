#ifndef _CONTEXT_H
#define _CONTEXT_H

#include <czmq.h>

#define STATE_STARTING	0
#define STATE_STARTED	1
#define STATE_STOPPING	2
#define STATE_STOPPED	3
#define STATE_SLEEPING	4
#define STATE_QUITTING	5

#define CONTEXT(x) ((struct child_context *)x)

struct child_context {
	int	state;	// where we're at
	int	target;	// where we're going

	int	argc;
	char	**argv;
	pid_t	pid;
	int	status;

	time_t	lastexit;
	time_t	interval;
	time_t	ticks;

	zloop_t	*loop;
};

char *state_to_string(int);

#endif

