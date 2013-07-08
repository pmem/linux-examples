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
 * pmemalloc_test1.c -- unit test 1 for libpmemalloc
 *
 * Usage: pmemalloc_test1 [-FMd] path [numbers...]
 *
 * Prepends any numbers given to a pmemalloc-based linked list.
 * If no numbers given, prints the list.
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

/*
 * define the nodes of the linked list used by the test code below
 */
struct node {
	struct node *next_;
	int value;
};

/*
 * define the static info we keep for the pmem pool we create
 */
struct static_info {
	struct node *rootnp_;	/* first node of the linked list */
};

char Usage[] = "[-FMd] path [strings...]";	/* for USAGE() */

int
main(int argc, char *argv[])
{
	const char *path;
	int opt;
	int fflag = 0;
	int iflag = 0;
	unsigned long icount;
	void *pmp;
	struct static_info *sp;
	struct node *parent_;
	struct node *np_;

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

		case 'f':
			fflag++;
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

	if ((pmp = pmemalloc_init(path, MY_POOL_SIZE)) == NULL)
		FATALSYS("pmemalloc_init on %s", path);

	/* fetch our static info */
	sp = (struct static_info *)pmemalloc_static_area(pmp);

	if (optind < argc) {	/* numbers supplied as arguments? */
		int i;

		if (fflag)
			USAGE("unexpected extra arguments given with -f flag");

		if (iflag)
			icount_start(icount);	/* start instruction count */

		for (i = optind; i < argc; i++) {
			int value = atoi(argv[i]);

			if ((np_ = pmemalloc_reserve(pmp,
							sizeof(*np_))) == NULL)
				FATALSYS("pmemalloc_reserve");

			/* link it in at the beginning of the list */
			PMEM(pmp, np_)->next_ = sp->rootnp_;
			PMEM(pmp, np_)->value = value;
			pmemalloc_onactive(pmp, np_,
					(void **)&sp->rootnp_, np_);
			pmemalloc_activate(pmp, np_);
		}

		if (iflag) {
			icount_stop();		/* end instruction count */

			printf("Total instruction count: %lu\n",
					icount_total());
		}
	} else if (fflag) {
		/*
		 * remove first item from list
		 */
		if (sp->rootnp_ == NULL)
			FATAL("the list is empty");

		if (iflag)
			icount_start(icount);	/* start instruction count */

		np_ = sp->rootnp_;
		pmemalloc_onfree(pmp, np_, (void **)&sp->rootnp_,
				PMEM(pmp, np_)->next_);
		pmemalloc_free(pmp, np_);

		if (iflag) {
			icount_stop();		/* end instruction count */

			printf("Total instruction count: %lu\n",
					icount_total());
		}
	} else {
		char *sep = "";

		/*
		 * traverse the list, printing the numbers found
		 */
		np_ = sp->rootnp_;
		while (np_) {
			printf("%s%d", sep, PMEM(pmp, np_)->value);
			sep = " ";
			np_ = PMEM(pmp, np_)->next_;
		}
		printf("\n");
	}

	DEBUG("Done.");
	exit(0);
}
