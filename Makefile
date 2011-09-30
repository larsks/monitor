prefix		= /usr/local
exec_prefix	= $(prefix)
bindir		= $(exec_prefix)/bin

CFLAGS		= -g
CPPFLAGS	= -I/opt/local/include
LDFLAGS		= -L/opt/local/lib
LIBS		= -lczmq

#%.o: %.c
#	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) $<

SRCS	= monitor.c \
	  daemon.c
OBJS	= $(SRCS:.c=.o)

all: monitor

monitor: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

install: all
	install -m 755 -d $(DESTDIR)$(bindir)
	install -m 755 monitor $(DESTDIR)$(bindir)/monitor

clean:
	rm -f $(OBJS) monitor

sockwatch: sockwatch.o
	$(CC) $(LDFLAGS) -o $@ sockwatch.o $(LIBS)

zmq: zmqserver zmqclient

zmqserver: zmqserver.o
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS) -lczmq
zmqclient: zmqclient.o
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS) -lzmq
