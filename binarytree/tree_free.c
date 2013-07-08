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
 * tree_free.c -- free the tree in Persistent Memory
 *
 * Usage: tree_free [-FMd] [-i icount] path
 *
 * This is just a simple CLI for calling tree_free().
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

#include "util/util.h"
#include "icount/icount.h"
#include "libpmem/pmem.h"
#include "tree.h"

char Usage[] = "[-FMd] [-i icount] path";		/* for USAGE() */

int
main(int argc, char *argv[])
{
	int opt;
	int iflag = 0;
	unsigned long icount;
	const char *path;

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

	if (optind < argc)
		USAGE(NULL);

	tree_init(path, 0);

	if (iflag)
		icount_start(icount);

	tree_free();

	if (iflag) {
		icount_stop();
		printf("Total instruction count: %lu\n", icount_total());
	}

	exit(0);
}
