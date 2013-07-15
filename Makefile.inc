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
# Makefile.inc -- common Makefile definitions for PM examples
#
# Some of the examples use this file to simplify their Makefiles.
# Examples that build as shared libraries or self-comtained files
# don't benefit from this include file and just use stand-alone
# Makefiles.
#
INCS = -I..
CFLAGS = -ggdb -Wall -Werror

all: $(TARGETS)

$(TARGETS): $(OBJS) $(LIBFILES)
	$(CC) -o $@ $(CFLAGS) $^ $(LIBFILES) $(LIBS)

.c.o:
	$(CC) -c $(CFLAGS) $(INCS) $< -o $@

util.o: ../util/util.c ../util/util.h
	$(CC) -c -o $@ $(CFLAGS) $(INCS) $<

icount.o: ../icount/icount.c ../icount/icount.h
	$(CC) -c -o $@ $(CFLAGS) $(INCS) $<

../libpmem/libpmem.a: force
	$(MAKE) -C ../libpmem

../libpmemalloc/libpmemalloc.a: force
	$(MAKE) -C ../libpmemalloc

clean:
	$(RM) *.o core a.out testfile*

clobber: clean
	$(RM) $(TARGETS) $(CLOBBERFILES)

force:

.PHONY: all clean clobber force allcounts test
