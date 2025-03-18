MODULE_big = itree
OBJS = itree_io.o itree_op.o itree_gin.o
EXTENSION = itree
DATA = itree--1.0.sql
REGRESS = itree

PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)