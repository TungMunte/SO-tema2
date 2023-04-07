LIBC_PATH ?= ../allocator/

CC=gcc
CPPFLAGS=-I../utils
CFLAGS=-fPIC -Wall -Wextra -g
LDFLAGS=-L../allocator
LDLIBS=-losmem

SOURCEDIR=src
BUILDDIR=bin
RESULTDIR=out
SRCS=$(sort $(wildcard $(SOURCEDIR)/*.c))
BINS=$(patsubst $(SOURCEDIR)/%.c, $(BUILDDIR)/%, $(SRCS))

.PHONY: all clean check

all: $(BUILDDIR) $(BINS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)
	make -C $(LIBC_PATH)

$(BUILDDIR)/%: $(SOURCEDIR)/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

check:
	python3 checker.py

clean:
	make -C $(LIBC_PATH) clean
	rm -f $(BINS)
	rm -rf out bin
