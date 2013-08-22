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
# Makefile -- Makefile for libpmem
#

TARGETS = libpmem.a libpmem.so
INCS = -I..
OBJS = pmem.o pmem_cl.o pmem_fit.o pmem_msync.o util.o
MAPFILE = pmem.map
SOVERSION = 1
CFLAGS = -ggdb

all: $(TARGETS)

libpmem.a: $(OBJS)
	$(AR) rv $@ $(OBJS)

libpmem.so: $(OBJS)
	$(CC) $(CFLAGS) -shared -Wl,--version-script=$(MAPFILE),-soname,$(SONAME).$(SOVERSION) -o $@ $(OBJS)

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(INCS) -fPIC $<

util.o: ../util/util.c ../util/util.h
	$(CC) -c -o $@ $(CFLAGS) $(INCS) -fPIC $<

clean:
	$(RM) *.o core a.out

clobber: clean
	$(RM) $(TARGETS)

test:
	# no libpmem unit test
	@echo PASS

.PHONY: all clean clobber test
