#include "context.h"

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

