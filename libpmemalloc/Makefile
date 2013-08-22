# Copyright (c) 2013, Intel Corporation
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
# 
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
# 
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# Makefile -- Makefile for libpmemalloc
#

TARGETS = libpmemalloc.a libpmemalloc.so pmemalloc_test1 pmemalloc_test2\
	  pmemalloc_check
INCS = -I..
OBJS = pmemalloc.o util.o icount.o
LIBFILES = ../libpmem/libpmem.a libpmemalloc.a
MAPFILE = pmemalloc.map
SOVERSION = 1
CFLAGS = -ggdb

all: $(TARGETS)

libpmemalloc.a: $(OBJS)
	$(AR) rv $@ $(OBJS)

libpmemalloc.so: $(OBJS)
	$(CC) $(CFLAGS) -shared -Wl,--version-script=$(MAPFILE),-soname,$(SONAME).$(SOVERSION) -o $@ $(OBJS)

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(INCS) $<

libpmemalloc.a libpmemalloc.so: CFLAGS += -fPIC

pmemalloc_test1: pmemalloc_test1.o pmemalloc.h $(LIBFILES)
	$(CC) -o $@ $(CFLAGS) $(INCS) pmemalloc_test1.c $(LIBFILES)

pmemalloc_test2: pmemalloc_test2.o pmemalloc.h $(LIBFILES)
	$(CC) -o $@ $(CFLAGS) $(INCS) pmemalloc_test2.c $(LIBFILES)

pmemalloc_check: pmemalloc_check.o pmemalloc.h $(LIBFILES)
	$(CC) -o $@ $(CFLAGS) $(INCS) pmemalloc_check.c $(LIBFILES)

util.o: ../util/util.c ../util/util.h
	$(CC) -c -o $@ $(CFLAGS) $(INCS) -fPIC $<

icount.o: ../icount/icount.c ../icount/icount.h
	$(CC) -c -o $@ $(CFLAGS) $(INCS) -fPIC $<

../libpmem/libpmem.a: force
	$(MAKE) -C ../libpmem

clean:
	$(RM) *.o core a.out

clobber: clean
	$(RM) $(TARGETS)

force:

test: pmemalloc_test1 pmemalloc_check
	$(RM) testfile
	./pmemalloc_test1 $(TESTARGS) -M testfile 2 3 4
	./pmemalloc_check $(TESTARGS) testfile
	./pmemalloc_test1 $(TESTARGS) testfile
	./pmemalloc_test1 $(TESTARGS) -M -f testfile
	./pmemalloc_check $(TESTARGS) testfile
	./pmemalloc_test1 $(TESTARGS) testfile
	./pmemalloc_test1 $(TESTARGS) -M -f testfile
	./pmemalloc_check $(TESTARGS) testfile
	./pmemalloc_test1 $(TESTARGS) testfile
	./pmemalloc_test1 $(TESTARGS) -M -f testfile
	./pmemalloc_check $(TESTARGS) testfile
	./pmemalloc_test1 $(TESTARGS) testfile

#
# "make allcounts" takes a while to run.
#
CMD = rm -f testfile%C; ./pmemalloc_test1 -F -i %C testfile%C 3 2 1; ./pmemalloc_test1 testfile%C; ./pmemalloc_check testfile%C

allcounts: pmemalloc_test1 pmemalloc_check
	@../icount/allcounts -j 200 '$(CMD)'
	@rm -f testfile*

test: pmemalloc_test1 pmemalloc_check pmemalloctest
	@./pmemalloctest 2>&1 | tee pmemalloctest.out
	@cmp -s pmemalloctest.out pmemalloctest.pass || (echo FAIL: pmemalloctest.out does not match pmemalloctest.pass; false)
	@echo PASS

.PHONY: all clean clobber force allcounts test
