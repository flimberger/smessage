NAME=smessage
VERSION=ALPHA

XINC=/usr/X11R6/include
XLIB=/usr/X11R6/lib

INC=-I${XINC}
LIB=-I${XLIB} -lX11

CDBG=-g -O0
CPPFLAGS=-D_BSD_SOURCE -D_POSIX_C_SOURCE=2 -DNAME=\"${NAME}\" -DVERSION=\"${VERSION}\"
CFLAGS=-Wall -Wextra -Werror -std=c99 -pedantic ${DBG} ${INC} ${CPPFLAGS}

LDBG=-g
LDFLAGS=${LIB} ${LDBG}

CC=cc
LD=cc

SRC=${NAME}.c
OBJ=${SRC:.c=.o}

.PHONY=all clean options

all: options ${NAME}

.c.o:
	${CC} ${CFLAGS} -c $<

${NAME}: ${OBJ}
	${LD} ${LDFLAGS} -o $@ ${OBJ}

clean:
	rm -f ${OBJ} ${NAME}

options:
	@echo CC=${CC}
	@echo CFLAGS=${CFLAGS}
	@echo LD=${LD}
	@echo LDFLAGS=${LDFLAGS}
	@echo
