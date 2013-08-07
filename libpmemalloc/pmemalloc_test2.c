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
 * pmemalloc_test2.c -- unit test 2 for libpmemalloc
 *
 * Usage: pmemalloc_test2 [-FMd] path
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
#include "pmemalloc.h"

#define	MY_POOL_SIZE	(10 * 1024 * 1024)
#define NPTRS 4096

char Usage[] = "[-FMd] path";	/* for USAGE() */

int
main(int argc, char *argv[])
{
	const char *path;
	int opt;
	void *pmp;
	int i;
	void *ptrs[NPTRS];

	Myname = argv[0];
	while ((opt = getopt(argc, argv, "FMdfi:")) != -1) {
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

		default:
			USAGE(NULL);
		}
	}

	if (optind >= argc)
		USAGE("No path given");
	path = argv[optind++];

	if (optind < argc)
		USAGE(NULL);

	if ((pmp = pmemalloc_init(path, MY_POOL_SIZE)) == NULL)
		FATALSYS("pmemalloc_init on %s", path);

	for (i = 0; i < NPTRS; i++) {
		if ((ptrs[i] = pmemalloc_reserve(pmp, 10 + i)) == NULL)
			FATALSYS("pmemalloc_reserve: iteration %d", i);

		pmemalloc_activate(pmp, ptrs[i]);
	}

	pmemalloc_check(path);

	for (i = 0; i < NPTRS; i += 2) {
		pmemalloc_free(pmp, ptrs[i]);
	}

	pmemalloc_check(path);

	for (i = 0; i < NPTRS; i += 2) {
		if ((ptrs[i] = pmemalloc_reserve(pmp, 1 + i)) == NULL)
			FATALSYS("pmemalloc_reserve: iteration %d", i);

		pmemalloc_activate(pmp, ptrs[i]);
	}

	pmemalloc_check(path);

	for (i = 0; i < NPTRS; i++) {
		pmemalloc_free(pmp, ptrs[i]);
	}

	pmemalloc_check(path);

	DEBUG("Done.");
	exit(0);
}
