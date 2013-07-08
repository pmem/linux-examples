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
 * basic.c -- illustrate basic load/store operations on PM
 *
 * usage: basic [-FMd] [-i icount] path [strings...]
 *
 * 	Where <path> is a file on a Persistent Memory aware file system
 * 	like PMFS.  If path doesn't exist, this program will create it
 * 	with size DEFAULT_SIZE (defined below).
 *
 * 	Any strings provided will be written to the Persistent Memory,
 * 	each terminated by a '\0' character.  If no strings are
 * 	provided, the Persistent Memory is dumped out in a manner similar
 * 	to strings(1).
 *
 * 	The -F flag enables the fault injection testing version of libpmem.
 * 	This for more meaningful icount-based runs (see the -i flag below).
 *
 * 	The -M flag enables the msync-based version of libpmem.
 * 	Use this when you don't have a PM-aware file system and you're
 * 	running on traditional memory-mapped files instead.
 *
 * 	The -d flag turns on debugging prints.
 *
 * 	The -i flag, if given, turns on instruction counting for the
 * 	code snippet under test (the code between icount_start() and
 *	icount_stop() below).  Use "-i 0" to print the count, "-i N"
 *	to simulate a crash after N instructions.  For more accurate
 *	fault injection testing, use this with the -F flag described above.
 */

#define	DEFAULT_SIZE 8192

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "util/util.h"
#include "icount/icount.h"
#include "libpmem/pmem.h"

char Usage[] = "[-FMd] [-i icount] path [strings...]";	/* for USAGE() */

int
main(int argc, char *argv[])
{
	int opt;
	int iflag = 0;
	unsigned long icount;
	const char *path;
	struct stat stbuf;
	size_t size;
	int fd;
	char *pmaddr;

	Myname = argv[0];
	while ((opt = getopt(argc, argv, "FMdi:")) != -1) {
		switch (opt) {
		case 'F':
			pmem_fit_mode();
			break;

		case 'M':
			pmem_msync_mode();
			break;

		case 'd':
			Debug++;
			break;

		case 'i':
			iflag++;
			icount = strtoul(optarg, NULL, 10);
			break;

		default:
			USAGE(NULL);
		}
	}

	if (optind >= argc)
		USAGE("No path given");
	path = argv[optind++];

	if (stat(path, &stbuf) < 0) {
		/*
		 * file didn't exist, create it with DEFAULT_SIZE
		 */
		if ((fd = open(path, O_CREAT|O_RDWR, 0666)) < 0)
			FATALSYS("can't create %s", path);

		if ((errno = posix_fallocate(fd, 0, DEFAULT_SIZE)) != 0)
			FATALSYS("posix_fallocate");

		size = DEFAULT_SIZE;
	} else {
		/*
		 * file exists, just open it
		 */
		if ((fd = open(path, O_RDWR)) < 0)
			FATALSYS("open %s", path);

		size = stbuf.st_size;
	}

	/*
	 * map the file into our address space.
	 */
	if ((pmaddr = pmem_map(fd, size)) == NULL)
		FATALSYS("pmem_map");

	if (optind < argc) {	/* strings supplied as arguments? */
		int i;
		char *ptr = pmaddr;

		if (iflag)
			icount_start(icount);	/* start instruction count */

		for (i = optind; i < argc; i++) {
			size_t len = strlen(argv[i]) + 1; /* includes '\0' */

			if (len > size)
				FATAL("no more room for %d-byte string", len);

			/* store to Persistent Memory */
			strcpy(ptr, argv[i]);

			/* make that change durable */
			pmem_persist(ptr, len, 0);

			ptr += len;
			size -= len;
		}

		if (iflag) {
			icount_stop();		/* end instruction count */

			printf("Total instruction count: %lu\n",
					icount_total());
		}
	} else {
		char *ptr = pmaddr;
		char *sep = "";

		/*
		 * dump out all the strings we find in Persistent Memory
		 */
		while (ptr < &pmaddr[size]) {

			/* load from Persistent Memory */
			if (isprint(*ptr)) {
				putc(*ptr, stdout);
				sep = "\n";
			} else if (*ptr == '\0') {
				fputs(sep, stdout);
				sep = "";
			}

			ptr++;
		}
	}

	exit(0);
}
