SRCS	= monitor.c \
	  daemon.c
OBJS	= $(SRCS:.c=.o)

all: monitor

monitor: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

