# Compiler -- settings for linux

CFLAGS	=	-rdynamic -Wall -W
CC	=	gcc

# Build rules

all:
	$(CC) htget.c ${CFLAGS} -o htget

uninstall:
	rm /usr/bin/htget
	rm /usr/man/man1/htget.1
	rm -r /usr/doc/htget
	

install:
	install htget /usr/bin
	install htget.1 /usr/man/man1
	install -d /usr/doc/htget
	install README /usr/doc/htget/README
	cat reminder

	

