# dmenu version
VERSION = 5.3

# paths
PREFIX = /usr/pkg
MANPREFIX = $(PREFIX)/share/man

X11INC = /usr/X11R7/include
X11LIB = /usr/X11R7/lib

# Xinerama, comment if you don't want it
#XINERAMALIBS  = -lXinerama
#XINERAMAFLAGS = -DXINERAMA

# freetype
FREETYPELIBS = -lfontconfig -lXft
FREETYPEINC = /usr/pkg/include/freetype2
# OpenBSD (uncomment)
#FREETYPEINC = $(X11INC)/freetype2
#MANPREFIX = ${PREFIX}/man

# includes and libs
INCS = -I$(X11INC) -I$(FREETYPEINC)
#LIBS = -L$(X11LIB) -lX11 $(XINERAMALIBS) $(FREETYPELIBS)
LIBS = -L${X11LIB} -lX11 -Wl,-R${X11LIB} ${XINERAMALIBS} ${FREETYPELIBS}

# flags
#CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\" $(XINERAMAFLAGS)
#CFLAGS   = -std=c99 -pedantic -Wall -Os $(INCS) $(CPPFLAGS)
#LDFLAGS  = $(LIBS)
# flags
CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
CFLAGS   = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = -s ${LIBS}

# compiler and linker
CC = cc
