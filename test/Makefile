PREFIX   := $(HOME)/local
SOURCES  := $(wildcard *.c)
TARGET   := $(subst .c,, $(SOURCES))
CPPFLAGS := $(addprefix -I, $(PREFIX)/include)

ifeq ($(shell uname -s), SunOS)
CC       := cc
CFLAGS   := -O3 -m64 -xopenmp -xc99 -D_XOPEN_SOURCE=600
LDFLAGS  := $(addprefix -L, $(PREFIX)/lib) $(addprefix -R, $(PREFIX)/lib)
LDLIBS   := $(PREFIX)/lib/libulibc.a -lm $(PREFIX)/lib/libhwloc.a -llgrp -lkstat -lpicl -xopenmp
else ifeq ($(shell uname -s), Darwin)
CC       := gcc
CFLAGS   := -O2 -g3 -fopenmp -Wall -Wextra -std=c99 -D_GNU_SOURCE
LDFLAGS  := $(addprefix -L, $(PREFIX)/lib) -Wl,-rpath,$(PREFIX)/lib
LDLIBS   := -lulibc -lm
else
CC       := gcc
CFLAGS   := -O2 -g3 -fopenmp -Wall -Wextra -std=c99 -D_GNU_SOURCE
LDFLAGS  := $(addprefix -L, $(PREFIX)/lib) -Wl,-no-as-needed -Wl,-R$(PREFIX)/lib -Wl,-rpath-link $(PREFIX)/lib
LDLIBS   := -lulibc -lm
endif



.PHONY: all clean test
all: test

test: $(TARGET)

clean:
	$(RM) *~ \#* *.o gmon.out $(TARGET)

### Local variables:
### mode: makefile-bsdmake
### coding: utf-8-unix
### indent-tabs-mode: nil
### End:
