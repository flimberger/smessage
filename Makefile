# smessage - simple message viewer
# See LICENSE file for copyright and license details

include config.mk

SRC=smessage.c
OBJ=${SRC:.c=.o}

all: options smessage
.PHONY:	all

options:
	@echo smessage build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "CC      = ${CC}"
.PHONY:	options

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

smessage: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f ${OBJ} smessage
.PHONY:	clean
