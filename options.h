#ifndef _OPTIONS_H
#define _OPTIONS_H

#ifndef DEFAULT_SOCKET_URI
#define DEFAULT_SOCKET_URI "ipc:///var/run/monitor/%s"
#endif

#ifndef DEFAULT_MAXINTERVAL
#define DEFAULT_MAXINTERVAL 30
#endif

#ifndef DEFAULT_HEARTBEAT
#define DEFAULT_HEARTBEAT 500
#endif

#define	OPT_HELP	'h'
#define OPT_PIDFILE	'p'
#define OPT_FOREGROUND	'f'
#define OPT_RESTART	'R'
#define OPT_MAXINTERVAL	'I'
#define OPT_WORKDIR	'd'
#define OPT_TAG		't'
#define OPT_SOCKET	'S'

#define OPTSTRING	"hp:fRI:d:t:S:"

extern char	*pidfile;
extern char	*workdir;
extern FILE	*pidfd;
extern char	*socket_path;
extern char	*tag;
extern int	foreground;
extern int	alwaysrestart;
extern int	maxinterval;
extern int	heartbeat;

int parse_args(int argc, char **argv);
void setup_socket_path(void);
void setup_work_dir(void);
FILE *setup_pid_file(void);
void write_pid_file(FILE *pidfd);

#endif

