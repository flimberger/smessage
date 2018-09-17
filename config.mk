# smessage version
VERSION = ALPHA

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# includes and libs
INCS =
LIBS = -lX11

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS   = -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = -s ${LIBS}

# compiler and linker
CC ?= c99
