# pg_show_vm/Makefile

MODULE_big = pg_show_vm
OBJS = pg_show_vm.o

EXTENSION = pg_show_vm
DATA = pg_show_vm--1.0.sql

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_show_vm
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

