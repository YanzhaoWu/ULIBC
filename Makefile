ULIBCDIR := .
TESTDIR  := test
-include make.rule

OBJECTS := \
init.o \
topology.o \
online_topology.o \
numa_mapping.o \
numa_threads.o \
numa_malloc.o \
numa_loops.o \
numa_barrier.o \
tools.o


.PHONY: all lib clean update_make_headers
all: $(HWLOC_HOME) lib

lib: $(LIBNAME)

$(LIBNAME): $(OBJECTS)
	$(RM) $@
	$(AR) $@ $^
ifdef RANLIB
	$(RANLIB) $@
endif

ifeq ($(HWLOC_BUILD), yes)

# e.g.) hwloc-x.xx.tar.gz
$(HWLOC_TGZ):
	wget $(HWLOC_URL)

# e.g.) hwloc-x.xx.tar
$(HWLOC_TAR): $(HWLOC_TGZ)
	gunzip -c $(HWLOC_TGZ) > $(HWLOC_TAR)

# e.g.) hwloc-x.xx
$(HWLOC_DIR): $(HWLOC_TAR)
	@-(! test -e $(HWLOC_DIR)) && tar xvf $(HWLOC_TAR)

# e.g.) hwloc
$(HWLOC_HOME): $(HWLOC_DIR)
	cd $(HWLOC_DIR) && ./configure $(HWLOC_CONFIG_OPTS) && make all && make install
endif

test: lib $(TESTDIR)/$(wildcard *.c)

$(TESTDIR)/$(wildcard *.c):
	$(MAKE) -C $(TESTDIR)

update_make_headers:
	$(CC) -MM -I$(ULIBCDIR) *.c > make.headers

-include make.headers

clean:
	$(RM) *~ \#* *.o gmon.out $(LIBNAME) $(SOLIBNAME)
	$(MAKE) -C $(TESTDIR) clean

distclean:
	$(RM) $(HWLOC_HOME) $(HWLOC_DIR) $(HWLOC_TAR)

### Local variables:
### mode: makefile-bsdmake
### coding: utf-8-unix
### indent-tabs-mode: nil
### End:
