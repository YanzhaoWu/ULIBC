ULIBCDIR := ..
LIBDIRS  := $(ULIBCDIR)
LDLIBS   := -lulibc -lm -lpthread

CPPFLAGS := $(addprefix -I, . $(LIBDIRS) $(HWLOC_INC))
LDFLAGS  := $(addprefix -L, . $(LIBDIRS) $(HWLOC_LIB))
CC       := gcc
CFLAGS   := -g3

ifeq ($(OS), solaris)
CFLAGS   += -O3 -m64 -xopenmp -xc99 -D_XOPEN_SOURCE=600
# LDLIBS   += -llgrp -lkstat -lpicl -xopenmp
LDLIBS   += -xopenmp
LDFLAGS  += $(addprefix -R, . $(LIBDIRS) $(HWLOC_LIB)) -O3 -m64
else ifeq ($(OS), aix)
CFLAGS   += -qsmp=omp
LDLIBS   += -qsmp=omp
else ifeq ($(OS), osx)
CFLAGS   += -O2 -fopenmp -Wall -Wextra -std=c99 -D_GNU_SOURCE
LDLIBS   += -fopenmp
else
CFLAGS   += -O2 -fopenmp -Wall -Wextra -std=c99 -D_GNU_SOURCE
LDLIBS   += -fopenmp
endif

SOURCES  := $(wildcard *.c)
TARGET   := $(subst .c,, $(SOURCES))

.PHONY: all clean lib test update_make_headers
all: lib test
lib:
	make -C $(ULIBCDIR)

test: $(TARGET)

update_make_headers:
	$(CC) -MM -I$(ULIBCDIR) *.c > make.headers

-include make.headers

clean:
	$(RM) *~ \#* *.o gmon.out $(TARGET)

### Local variables:
### mode: makefile-bsdmake
### coding: utf-8-unix
### indent-tabs-mode: nil
### End:
