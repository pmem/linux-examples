/*
 * Copyright (c) 2013, Intel Corporation
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * trivial.c -- smallest possible, self-contained example
 *
 * This file memory-maps a file and stores a string to it.  It is
 * a quick example of how to use mmap() and msync() with Persistent
 * Memory.
 *
 * To build this example:
 * 	gcc -o trivial trivial.c
 *
 * To run it, create a 4k file, run the example, and dump the result:
 * 	dd if=/dev/zero of=testfile bs=4k count=1
 * 	./trivial
 * 	od -c testfile
 */

#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	int fd;
	char *pmaddr;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s filename\n", argv[0]);
		exit(1);
	}

	if ((fd = open(argv[1], O_RDWR)) < 0) {
		perror(argv[1]);
		exit(1);
	}

	/*
	 * Map 4k from the file into our memory for read & write.
	 * Use MAP_SHARED for Persistent Memory so that stores go
	 * directly to the PM and are globally visible.
	 */
	if ((pmaddr = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED,
					fd, 0)) == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	/* don't need the fd anymore, the mapping stays around */
	close(fd);

	/* store a string to the Persistent Memory */
	strcpy(pmaddr, "Hello, Persistent Memory!");

	/*
	 * The above stores may or may not be sitting in cache at
	 * this point, depending on other system activity causing
	 * cache pressure.  Force the change to be durable (flushed
	 * all the say to the Persistent Memory) using msync().
	 */
	if (msync((void *)pmaddr, 4096, MS_SYNC) < 0) {
		perror("msync");
		exit(1);
	}

	printf("Done.\n");
	exit(0);
}
