# Copyright (c) 2021-2022 <RealAstolfo@gmx.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published by
# the Free Software Foundation
#

prefix := /usr/local
sbindir := $(prefix)/sbin

VER := $(shell head -n 1 NEWS | cut -d : -f 1)

DEBUG := -ggdb
CFLAGS := -O2 -Wall $(DEBUG)
CPPFLAGS := -DVERSION=\"$(VER)\" -DCONFIG=\"$(sysconfdir)/actkbd.conf\"
LDLIBS := -lglfw -lGL -lm -lGLU -lGLEW
all: actkbd-nuklear

install:
	install -D -m755 actkbd-nuklear $(sbindir)/actkbd-nuklear
	install -d -m755 $(sysconfdir)

clean:
	rm -f actkbd-nuklear *.o
