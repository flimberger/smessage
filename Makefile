# smessage - simple message viewer
# See LICENSE file for copyright and license details

include config.mk

SRC=smessage.c
OBJ=${SRC:.c=.o}

.PHONY: all clean options

all: options smessage

options:
	@echo smessage build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "CC      = ${CC}"

.c.o:
	@echo ${CC} $<
	@${CC} ${CFLAGS} -c $<

${OBJ}: config.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

smessage: ${OBJ}
	@echo ${CC} -o $@
	@${CC} ${LDFLAGS} -o $@ ${OBJ}

clean:
	@echo cleaning
	@rm -f ${OBJ} smessage
