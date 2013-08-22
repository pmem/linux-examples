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
# Makefile for binarytree example
#
TARGETS = tree_insert tree_walk tree_free tree_wordfreq
OBJS = tree.o util.o icount.o
LIBFILES = ../libpmemalloc/libpmemalloc.a ../libpmem/libpmem.a
CLOBBERFILES = 4000.txt 4300.txt 4302.txt 4309.txt 4500.txt\
	       4000.zip 4300.zip 4302.zip 4309.zip 4500.zip

include ../Makefile.inc

tree_insert: tree_insert.o
tree_walk: tree_walk.o
tree_free : tree_free.o
tree_wordfreq : tree_wordfreq.o

tree.c: tree.h

#
# The allcounts entry point runs pmem_basic_icount via the allcounts
# script, which will simulate a crash between every two adjacent
# instructions in the test code snippet in pmem_basic_icount.c.
# The allcount script will then summarize the results of all the runs.
#
# This takes a while to run.
#
CMD = rm -f testfile%C; ./tree_insert -F -i %C testfile%C one two three four; ../libpmemalloc/pmemalloc_check testfile%C; ./tree_walk testfile%C; ../libpmemalloc/pmemalloc_check testfile%C; rm -f testfile%C

allcounts: tree_insert ../libpmemalloc/pmemalloc_check tree_walk
	@../icount/allcounts -j 200 '$(CMD)'
	@rm -f testfile*

../libpmemalloc/pmemalloc_check:
	$(MAKE) -C ../libpmemalloc

test: tree_walk tree_wordfreq freqtest
	@./freqtest -n
	@./freqtest 2>&1 | tee freqtest.out
	@cmp -s freqtest.out freqtest.pass || (echo FAIL: freqtest.out does not match freqtest.pass; false)
	@echo PASS

.PHONY: allcounts test
