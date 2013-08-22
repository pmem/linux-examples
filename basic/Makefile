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
# Makefile for basic example
#
TARGETS = basic
OBJS = basic.o util.o icount.o
LIBFILES = ../libpmem/libpmem.a

include ../Makefile.inc

#
# The allcounts entry point runs basic via the allcounts
# script, which will simulate a crash between every two adjacent
# instructions in the test code snippet in basic.c.
# The allcount script will then summarize the results of all the runs.
#
# This takes a while to run.
#
CMD = rm -f testfile%C; ./basic -i %C testfile%C short1 "this string is designed to be longer than can fit in a 64-byte cacheline" short2; ./basic testfile%C
CMDF = rm -f testfile%C; ./basic -F -i %C testfile%C short1 "this string is designed to be longer than can fit in a 64-byte cacheline" short2; ./basic testfile%C
CMDM = rm -f testfile%C; ./basic -M -i %C testfile%C short1 "this string is designed to be longer than can fit in a 64-byte cacheline" short2; ./basic testfile%C

allcounts: basic
	@../icount/allcounts -j 200 '$(CMD)'
	@rm -f testfile*

allcountsF: basic
	@../icount/allcounts -j 200 '$(CMDF)'
	@rm -f testfile*

allcountsM: basic
	@../icount/allcounts -j 200 '$(CMDM)'
	@rm -f testfile*

test: basic basictest
	@./basictest 2>&1 | tee basictest.out
	@cmp -s basictest.out basictest.pass || (echo FAIL: basictest.out does not match basictest.pass; false)
	@echo PASS

.PHONY: allcounts allcountsF allcountsM test
