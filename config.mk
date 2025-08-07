# slstatus version
VERSION = 0

# customize below to fit your system

# Optional integration with MPD (mpdonair module).
#
# Options:
#    MPD_TITLE_LENGTH    The number of UTF-8 characters to include in the output
#    MPD_ON_TEXT_FITS    What should happen when the title fits within the output text, one of:
#                           NO_SCROLL              Do not scroll the text (remains static)
#                           FULL_SPACE_SEPARATOR   Make sure the text clears before it starts again
#                           FORCE_SCROLL           Force scrolling anyway using the given loop text
#    MPD_LOOP_TEXT       The transition text that is printed before looping the title
#
# Uncomment the below to use mpdonair.
#MPDLIBS = -lmpdclient -lgrapheme
#MPDINCS = -I/usr/local/lib
#MPDFLAGS = -DHAVE_MPD=1 -DMPD_TITLE_LENGTH=20 -DMPD_ON_TEXT_FITS=NO_SCROLL -DMPD_LOOP_TEXT='" ~ "'

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

PKG_CONFIG = pkg-config

CONFIG = `$(PKG_CONFIG) --libs libconfig`

# flags
CPPFLAGS = -D_DEFAULT_SOURCE $(MPDFLAGS)
CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Wno-unused-parameter -Os
LDFLAGS  = -s
# OpenBSD: add -lsndio
# FreeBSD: add -lkvm -lsndio
LDLIBS   = `$(PKG_CONFIG) --libs x11` $(MPDLIBS) $(CONFIG)
LDINCS   = $(MPDINCS)

# compiler and linker
# CC = cc
