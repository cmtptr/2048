CFLAGS ?= -O2 -pipe
CFLAGS_EXTRA := -pthread
CPPFLAGS := -pedantic -std=c11 -Wall -Werror -Wextra
DESTDIR ?= /usr/local
LDADD := -lcurses

obj := $O2048.o $Oboard.o $Oclient.o $Oevent.o
bin := $O2048

.PHONY: all clean install
all: $(bin)

clean:
	$(RM) $(obj) $(bin)

install: all
	install -m755 "$(bin)" "$(DESTDIR)/bin"

$O%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_EXTRA) $(CPPFLAGS) -c -o $@ $<

$(bin): $(obj)
	$(CC) $(CFLAGS) $(CFLAGS_EXTRA) $(LDFLAGS) -o $@ $(obj) $(LDADD)

$(bin) $(obj): makefile
$O2048.o: 2048.c 2048.h
$Oboard.o: board.c 2048.h
$Oclient.o: client.c 2048.h
$Oevent.o: event.c 2048.h
