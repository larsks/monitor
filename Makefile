prefix		= /usr/local
exec_prefix	= $(prefix)
bindir		= $(exec_prefix)/bin

RST2MAN		= rst2man

SRCS	= monitor.c \
	  daemon.c
OBJS	= $(SRCS:.c=.o)

all: monitor

monitor: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

install: all
	install -m 755 -d $(DESTDIR)$(bindir)
	install -m 755 monitor $(DESTDIR)$(bindir)/monitor

monitor.1: README.rst
	$(RST2MAN) $< $@
