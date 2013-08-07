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
 * pmem_alloc.c -- example malloc library for Persistent Memory
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

#include "util/util.h"
#include "libpmem/pmem.h"
#include "pmemalloc.h"

/*
 * hidden bytes added to each allocation.  the metadata we keep for
 * each allocation is 64 bytes in size so when we return the
 * address after the hidden bytes, it is also 64-byte aligned.
 */
struct clump {
	size_t size;			/* size of the clump */
	size_t prevsize;		/* size of previous (lower) clump */
	struct {
		off_t off;
		void *ptr_;
	} on[PMEM_NUM_ON];
};

/*
 * pool header kept at a known location in each memory-mapped file
 */
struct pool_header {
	char signature[16];	/* must be PMEM_SIGNATURE */
	size_t totalsize;	/* total file size */
	char padding[4096 - 16 - sizeof(size_t)];
};

/*
 * definitions used internally by this implementation
 */
#define	PMEM_SIGNATURE "*PMEMALLOC_POOL"
#define	PMEM_PAGE_SIZE 4096	/* size of next three sections */
#define	PMEM_NULL_OFFSET 0	/* offset of NULL page (unmapped) */
#define	PMEM_STATIC_OFFSET 4096	/* offset of static area */
#define	PMEM_RED_OFFSET 8192	/* offset of red zone page (unmapped) */
#define	PMEM_HDR_OFFSET 12288	/* offset of pool header */
#define	PMEM_CLUMP_OFFSET 16384	/* offset of first clump */
#define	PMEM_MIN_POOL_SIZE (1024 * 1024)
#define	PMEM_CHUNK_SIZE 64	/* alignment/granularity for all allocations */
#define	PMEM_STATE_MASK 63	/* for storing state in size lower bits */
#define	PMEM_STATE_FREE 0	/* free clump */
#define	PMEM_STATE_RESERVED 1	/* reserved clump */
#define PMEM_STATE_ACTIVATING 2	/* clump in process of being activated */
#define PMEM_STATE_ACTIVE 3	/* active (allocated) clump */
#define PMEM_STATE_FREEING 4	/* clump in the process of being freed */
#define PMEM_STATE_UNUSED 5	/* must be highest value + 1 */

/*
 * Given an absolute pointer, covert it to a file offset.
 */
#define	OFF(pmp, ptr) ((uintptr_t)ptr - (uintptr_t)pmp)

/*
 * pmemalloc_recover -- recover after a possible crash
 *
 * Internal support routine, used during recovery.
 */
static void
pmemalloc_recover(void *pmp)
{
	struct clump *clp;
	int i;

	DEBUG("pmp=0x%lx", pmp);

	clp = PMEM(pmp, (struct clump *)PMEM_CLUMP_OFFSET);

	while (clp->size) {
		size_t sz = clp->size & ~PMEM_STATE_MASK;
		int state = clp->size & PMEM_STATE_MASK;

		DEBUG("[0x%lx]clump size %lx state %d",
				OFF(pmp, clp), sz, state);

		switch (state) {
		case PMEM_STATE_RESERVED:
			/* return the clump to the FREE pool */
			for (i = PMEM_NUM_ON - 1; i >= 0; i--)
				clp->on[i].off = 0;
			pmem_persist(clp, sizeof(*clp), 0);
			clp->size = sz | PMEM_STATE_FREE;
			pmem_persist(clp, sizeof(*clp), 0);
			break;

		case PMEM_STATE_ACTIVATING:
			/* finish progressing the clump to ACTIVE */
			for (i = 0; i < PMEM_NUM_ON; i++)
				if (clp->on[i].off) {
					uintptr_t *dest =
						PMEM(pmp,
						(uintptr_t *)clp->on[i].off);
					*dest = (uintptr_t)clp->on[i].ptr_;
					pmem_persist(dest, sizeof(*dest), 0);
				} else
					break;
			for (i = PMEM_NUM_ON - 1; i >= 0; i--)
				clp->on[i].off = 0;
			pmem_persist(clp, sizeof(*clp), 0);
			clp->size = sz | PMEM_STATE_ACTIVE;
			pmem_persist(clp, sizeof(*clp), 0);
			break;

		case PMEM_STATE_FREEING:
			/* finish progressing the clump to FREE */
			for (i = 0; i < PMEM_NUM_ON; i++)
				if (clp->on[i].off) {
					uintptr_t *dest =
						PMEM(pmp,
						(uintptr_t *)clp->on[i].off);
					*dest = (uintptr_t)clp->on[i].ptr_;
					pmem_persist(dest, sizeof(*dest), 0);
				} else
					break;
			for (i = PMEM_NUM_ON - 1; i >= 0; i--)
				clp->on[i].off = 0;
			pmem_persist(clp, sizeof(*clp), 0);
			clp->size = sz | PMEM_STATE_FREE;
			pmem_persist(clp, sizeof(*clp), 0);
			break;
		}

		clp = (struct clump *)((uintptr_t)clp + sz);
		DEBUG("next clp %lx, offset 0x%lx", clp, OFF(pmp, clp));
	}
}

/*
 * pmemalloc_coalesce_free -- find adjacent free blocks and coalesce them
 *
 * Scan the pmeme pool for recovery work:
 * 	- RESERVED clumps that need to be freed
 * 	- ACTIVATING clumps that need to be ACTIVE
 * 	- FREEING clumps that need to be freed
 *
 * Internal support routine, used during recovery.
 */
static void
pmemalloc_coalesce_free(void *pmp)
{
	struct clump *clp;
	struct clump *firstfree;
	struct clump *lastfree;
	size_t csize;

	DEBUG("pmp=0x%lx", pmp);

	firstfree = lastfree = NULL;
	csize = 0;
	clp = PMEM(pmp, (struct clump *)PMEM_CLUMP_OFFSET);

	while (clp->size) {
		size_t sz = clp->size & ~PMEM_STATE_MASK;
		int state = clp->size & PMEM_STATE_MASK;

		DEBUG("[0x%lx]clump size %lx state %d",
				OFF(pmp, clp), sz, state);

		if (state == PMEM_STATE_FREE) {
			if (firstfree == NULL)
				firstfree = clp;
			else
				lastfree = clp;
			csize += sz;
		} else if (firstfree != NULL && lastfree != NULL) {
			DEBUG("coalesced size 0x%lx", csize);
			firstfree->size = csize | PMEM_STATE_FREE;
			pmem_persist(firstfree, sizeof(*firstfree), 0);
			firstfree = lastfree = NULL;
			csize = 0;
		} else {
			firstfree = lastfree = NULL;
			csize = 0;
		}

		clp = (struct clump *)((uintptr_t)clp + sz);
		DEBUG("next clp %lx, offset 0x%lx", clp, OFF(pmp, clp));
	}
	if (firstfree != NULL && lastfree != NULL) {
		DEBUG("coalesced size 0x%lx", csize);
		DEBUG("firstfree 0x%lx next clp after firstfree will be 0x%lx",
				firstfree, (uintptr_t)firstfree + csize);
		firstfree->size = csize | PMEM_STATE_FREE;
		pmem_persist(firstfree, sizeof(*firstfree), 0);
	}
}

/*
 * pmemalloc_init -- setup a Persistent Memory pool for use
 *
 * Inputs:
 *	path -- path to the file which will contain the memory pool
 *
 *	        If the file doesn't exist, it is created.  If it exists,
 *	        the state of the memory pool is initialized from the file.
 *
 *	size -- size of the memory pool in bytes
 *
 *		The size is only used when creating the memory pool
 *		the first time (the file created will be extended to
 *		that size).  The smallest size allowed is 1 meg.  The
 *		largest size allowed is whatever the underlaying file
 *		system allows as a max file size.
 *
 * Outputs:
 * 	An opaque memory pool handle is returned on success.  That
 * 	handle must be passed in to most of the other pmem routines.
 *
 * 	On error, NULL is returned and errno is set.
 *
 * This function must be called before any other pmem functions.
 */
void *
pmemalloc_init(const char *path, size_t size)
{
	void *pmp;
	int err;
	int fd = -1;
	struct stat stbuf;

	DEBUG("path=%s size=0x%lx", path, size);

	if (stat(path, &stbuf) < 0) {
		struct clump cl = { 0 };
		struct pool_header hdr = { 0 };
		size_t lastclumpoff;

		if (errno != ENOENT)
			goto out;

		/*
		 * file didn't exist, we're creating a new memory pool
		 */
		if (size < PMEM_MIN_POOL_SIZE) {
			DEBUG("size %lu too small (must be at least %lu)",
					size, PMEM_MIN_POOL_SIZE);
			errno = EINVAL;
			goto out;
		}

		ASSERTeq(sizeof(cl), PMEM_CHUNK_SIZE);
		ASSERTeq(sizeof(hdr), PMEM_PAGE_SIZE);

		if ((fd = open(path, O_CREAT|O_RDWR, 0666)) < 0)
			goto out;

		if ((errno = posix_fallocate(fd, 0, size)) != 0)
			goto out;

		/*
		 * location of last clump is calculated by rounding the file
		 * size down to a multiple of 64, and then subtracting off
		 * another 64 to hold the struct clump.  the last clump is
		 * indicated by a size of zero (so no write is necessary
		 * here since the file is initially zeros.
		 */
		lastclumpoff =
			(size & ~(PMEM_CHUNK_SIZE - 1)) - PMEM_CHUNK_SIZE;

		/*
		 * create the first clump to cover the entire pool
		 */
		cl.size = lastclumpoff - PMEM_CLUMP_OFFSET;
		if (pwrite(fd, &cl, sizeof(cl), PMEM_CLUMP_OFFSET) < 0)
			goto out;
		DEBUG("[0x%lx] created clump, size 0x%lx",
				PMEM_CLUMP_OFFSET, cl.size);

		/*
		 * write the pool header
		 */
		strcpy(hdr.signature, PMEM_SIGNATURE);
		hdr.totalsize = size;
		if (pwrite(fd, &hdr, sizeof(hdr), PMEM_HDR_OFFSET) < 0)
			goto out;

		if (fsync(fd) < 0)
			goto out;

	} else {
		if ((fd = open(path, O_RDWR)) < 0)
			goto out;
		size = stbuf.st_size;

		/* XXX handle recovery case 1 described below */
	}

	/*
	 * map the file
	 */
	if ((pmp = pmem_map(fd, size)) == NULL)
		goto out;

	/*
	 * scan pool for recovery work, five kinds:
	 * 	1. pmem pool file sisn't even fully setup
	 * 	2. RESERVED clumps that need to be freed
	 * 	3. ACTIVATING clumps that need to be ACTIVE
	 * 	4. FREEING clumps that need to be freed
	 * 	5. adjacent free clumps that need to be coalesced
	 */
	pmemalloc_recover(pmp);
	pmemalloc_coalesce_free(pmp);

	DEBUG("return pmp 0x%lx", pmp);
	return pmp;

out:
	err = errno;
	if (fd != -1)
		close(fd);
	errno = err;
	return NULL;
}

/*
 * pmemalloc_static_area -- return a pointer to the static 4k area
 *
 * Outputs:
 * 	A pointer to a 4k static area is returned.  The caller may use
 * 	this area for anything -- no structure is imposed by this library.
 * 	This allows the caller to store pointers to roots of trees or
 * 	beginning of linked lists, etc. in the same file as the rest of
 * 	the memory pool.
 *
 * The pointer returned is a normal (absolute) pointer.  No need to use
 * the PMEM() macro with it.  Changes to this area are not guaranteed
 * persistent until pmem_persist() has been called, for example:
 *	pmem_persist(pmem_static_area(pmp), PMEM_STATIC_SIZE);
 */
void *
pmemalloc_static_area(void *pmp)
{
	DEBUG("pmp=0x%lx", pmp);

	return PMEM(pmp, (void *)PMEM_STATIC_OFFSET);
}

/*
 * pmemalloc_reserve -- allocate memory, volatile until pmemalloc_activate()
 *
 * Inputs:
 *	pmp -- a pmp as returned by pmemalloc_init()
 *
 *	size -- number of bytes to allocate
 *
 * Outputs:
 *	On success, this function returns memory allocated from the
 *	memory-mapped file associated with pmp.  The memory is suitably
 *	aligned for any kind of variable.  The memory is not initialized.
 *
 *	On failure, this function returns NULL and errno is set.
 *
 * The memory returned is initially *volatile* meaning that if the
 * program exits (or system crashes) before pmemalloc_activate() is called
 * with the return value, it is considered incompletely allocated
 * and the memory is returned to the free pool in the memory-mapped
 * file.  It works this way to prevent memory leaks when the system
 * crashes between a successful return from pmemalloc_reserve() and when
 * the caller actually links something to point at the new memory.
 * The basic pattern for using pmemalloc_reserve() is this:
 *
 *	np_ = pmemalloc_reserve(pmp, sizeof(*np_));
 *	...fill in fields in *np_...
 *	pmemalloc_onactive(pmp, np_, &parent->next_, np_);
 *	pmemalloc_activate(pmp, np_);
 *
 * In addition to flushing the data at *np_ to persistence, the
 * pmemalloc_activate() call above also atomically marks that memory
 * as in-use and stores the pointer to the persistent-memory-
 * based pointer parent->next_ in this example).  So any crash that
 * happens before parent->next_ is set to point at the new memory will
 * result in the memory being returned back to the free list.
 */
void *
pmemalloc_reserve(void *pmp, size_t size)
{
	size_t nsize = roundup(size + PMEM_CHUNK_SIZE, PMEM_CHUNK_SIZE);
	struct clump *clp;

	DEBUG("pmp=0x%lx, size=0x%lx -> 0x%lx", pmp, size, nsize);

	clp = PMEM(pmp, (struct clump *)PMEM_CLUMP_OFFSET);

	if (clp->size == 0)
		FATAL("no clumps found");

	/* first fit */
	while (clp->size) {
		size_t sz = clp->size & ~PMEM_STATE_MASK;
		int state = clp->size & PMEM_STATE_MASK;

		DEBUG("[0x%lx] clump size 0x%lx state %d",
				OFF(pmp, clp), sz, state);

		if (state == PMEM_STATE_FREE && nsize <= sz) {
			void *ptr = (void *)
				(uintptr_t)clp + PMEM_CHUNK_SIZE -
				(uintptr_t)pmp;
			size_t leftover = sz - nsize;

			DEBUG("fit found ptr 0x%lx, leftover 0x%lx bytes",
					ptr, leftover);
			if (leftover >= PMEM_CHUNK_SIZE * 2) {
				struct clump *newclp;
				int i;

				newclp = (struct clump *)
					((uintptr_t)clp + nsize);

				DEBUG("splitting: [0x%lx] new clump",
						OFF(pmp, newclp));
				/*
				 * can go ahead and start fiddling with
				 * this freely since it is in the middle
				 * of a free clump until we change fields
				 * in *clp.  order here is important:
				 * 	1. initialize new clump
				 * 	2. persist new clump
				 * 	3. initialize existing clump do list
				 * 	4. persist existing clump
				 * 	5. set new clump size, RESERVED
				 * 	6. persist existing clump
				 */
				memset(newclp, '\0', sizeof(*newclp));
				newclp->size = leftover | PMEM_STATE_FREE;
				pmem_persist(newclp, sizeof(*newclp), 0);
				for (i = 0; i < PMEM_NUM_ON; i++) {
					clp->on[i].off = 0;
					clp->on[i].ptr_ = 0;
				}
				pmem_persist(clp, sizeof(*clp), 0);
				clp->size = nsize | PMEM_STATE_RESERVED;
				pmem_persist(clp, sizeof(*clp), 0);
			} else {
				int i;

				DEBUG("no split required");

				for (i = 0; i < PMEM_NUM_ON; i++) {
					clp->on[i].off = 0;
					clp->on[i].ptr_ = 0;
				}
				pmem_persist(clp, sizeof(*clp), 0);
				clp->size = sz | PMEM_STATE_RESERVED;
				pmem_persist(clp, sizeof(*clp), 0);
			}

			return ptr;
		}

		clp = (struct clump *)((uintptr_t)clp + sz);
		DEBUG("[0x%lx] next clump", OFF(pmp, clp));
	}

	DEBUG("no free memory of size %lu available", nsize);
	errno = ENOMEM;
	return NULL;
}

/*
 * pmemalloc_onactive -- set assignments for when reservation goes active
 *
 * Inputs:
 *	pmp -- a pmp as returned by pmemalloc_init()
 *
 *	parentp_ -- pointer to atomically set
 *
 *	nptr_ -- value to set in *parentp
 */
void
pmemalloc_onactive(void *pmp, void *ptr_, void **parentp_, void *nptr_)
{
	struct clump *clp;
	int i;

	DEBUG("pmp=0x%lx, ptr_=0x%lx, parentp_=0x%lx, nptr_=0x%lx",
			pmp, ptr_, parentp_, nptr_);

	clp = PMEM(pmp, (struct clump *)((uintptr_t)ptr_ - PMEM_CHUNK_SIZE));

	ASSERTeq(clp->size & PMEM_STATE_MASK, PMEM_STATE_RESERVED);

	DEBUG("[0x%lx] clump on: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
			OFF(pmp, clp),
			clp->on[0].off, clp->on[0].ptr_,
			clp->on[1].off, clp->on[1].ptr_,
			clp->on[2].off, clp->on[2].ptr_);

	for (i = 0; i < PMEM_NUM_ON; i++)
		if (clp->on[i].off == 0) {
			DEBUG("using on[%d], off 0x%lx", i, OFF(pmp, parentp_));
			/*
			 * order here is important:
			 * 1. set ptr_
			 * 2. make ptr_ persistent
			 * 3. set off
			 * 4. make off persistent
			 */
			clp->on[i].ptr_ = nptr_;
			pmem_persist(clp, sizeof(*clp), 0);
			clp->on[i].off = OFF(pmp, parentp_);
			pmem_persist(clp, sizeof(*clp), 0);
			return;
		}

	FATAL("exceeded onactive limit (%d)", PMEM_NUM_ON);
}

/*
 * pmemalloc_free -- set assignments for when allocation gets freed
 *
 * Inputs:
 *	pmp -- a pmp as returned by pmemalloc_init()
 *
 *	parentp_ -- pointer to atomically set
 *
 *	nptr_ -- value to set in *parentp
 */
void
pmemalloc_onfree(void *pmp, void *ptr_, void **parentp_, void *nptr_)
{
	struct clump *clp;
	int i;

	DEBUG("pmp=0x%lx, ptr_=0x%lx, parentp_=0x%lx, nptr_=0x%lx",
			pmp, ptr_, parentp_, nptr_);

	clp = PMEM(pmp, (struct clump *)((uintptr_t)ptr_ - PMEM_CHUNK_SIZE));

	ASSERTeq(clp->size & PMEM_STATE_MASK, PMEM_STATE_ACTIVE);

	DEBUG("[0x%lx] clump on: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
			OFF(pmp, clp),
			clp->on[0].off, clp->on[0].ptr_,
			clp->on[1].off, clp->on[1].ptr_,
			clp->on[2].off, clp->on[2].ptr_);

	for (i = 0; i < PMEM_NUM_ON; i++)
		if (clp->on[i].off == 0) {
			DEBUG("using on[%d], off 0x%lx", i, OFF(pmp, parentp_));
			/*
			 * order here is important:
			 * 1. set ptr_
			 * 2. make ptr_ persistent
			 * 3. set off
			 * 4. make off persistent
			 */
			clp->on[i].ptr_ = nptr_;
			pmem_persist(clp, sizeof(*clp), 0);
			clp->on[i].off = OFF(pmp, parentp_);
			pmem_persist(clp, sizeof(*clp), 0);
			return;
		}

	FATAL("exceeded onfree limit (%d)", PMEM_NUM_ON);
}

/*
 * pmemalloc_activate -- atomically persist memory, mark in-use, store pointers
 *
 * Inputs:
 *	pmp -- a pmp as returned by pmemalloc_init()
 *
 *	ptr_ -- memory to be persisted, as returned by pmemalloc_reserve()
 */
void
pmemalloc_activate(void *pmp, void *ptr_)
{
	struct clump *clp;
	size_t sz;
	int i;

	DEBUG("pmp=%lx, ptr_=%lx", pmp, ptr_);

	clp = PMEM(pmp, (struct clump *)((uintptr_t)ptr_ - PMEM_CHUNK_SIZE));

	ASSERTeq(clp->size & PMEM_STATE_MASK, PMEM_STATE_RESERVED);

	DEBUG("[0x%lx] clump on: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
			OFF(pmp, clp),
			clp->on[0].off, clp->on[0].ptr_,
			clp->on[1].off, clp->on[1].ptr_,
			clp->on[2].off, clp->on[2].ptr_);

	sz = clp->size & ~PMEM_STATE_MASK;

	/*
	 * order here is important:
	 * 1. persist *ptr_
	 * 2. set state to ACTIVATING
	 * 3. persist *clp (now we're committed to progressing to STATE_ACTIVE)
	 * 4. execute "on" list, persisting each one
	 * 5. clear out "on" list, last to first
	 * 5. set state to ACTIVE
	 * 6. persist *clp
	 */
	pmem_persist(PMEM(pmp, ptr_), clp->size - PMEM_CHUNK_SIZE, 0);
	clp->size = sz | PMEM_STATE_ACTIVATING;
	pmem_persist(clp, sizeof(*clp), 0);
	for (i = 0; i < PMEM_NUM_ON; i++)
		if (clp->on[i].off) {
			uintptr_t *dest =
				PMEM(pmp, (uintptr_t *)clp->on[i].off);
			*dest = (uintptr_t)clp->on[i].ptr_;
			pmem_persist(dest, sizeof(*dest), 0);
		} else
			break;
	for (i = PMEM_NUM_ON - 1; i >= 0; i--)
		clp->on[i].off = 0;
	pmem_persist(clp, sizeof(*clp), 0);
	clp->size = sz | PMEM_STATE_ACTIVE;
	pmem_persist(clp, sizeof(*clp), 0);
}

/*
 * pmemalloc_free -- free memory
 *
 * Inputs:
 *	pmp -- a pmp as returned by pmemalloc_init()
 *
 *	ptr_ -- memory to be freed, as returned by pmemalloc_reserve()
 */
void
pmemalloc_free(void *pmp, void *ptr_)
{
	struct clump *clp;
	size_t sz;
	int state;
	int i;

	DEBUG("pmp=%lx, ptr_=%lx", pmp, ptr_);

	clp = PMEM(pmp, (struct clump *)((uintptr_t)ptr_ - PMEM_CHUNK_SIZE));


	DEBUG("[0x%lx] clump on: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
			OFF(pmp, clp),
			clp->on[0].off, clp->on[0].ptr_,
			clp->on[1].off, clp->on[1].ptr_,
			clp->on[2].off, clp->on[2].ptr_);

	sz = clp->size & ~PMEM_STATE_MASK;
	state = clp->size & PMEM_STATE_MASK;

	if (state != PMEM_STATE_RESERVED && state != PMEM_STATE_ACTIVE)
		FATAL("freeing clumb in bad state: %d", state);

	if (state == PMEM_STATE_ACTIVE) {
		/*
		 * order here is important:
		 * 1. set state to FREEING
		 * 2. persist *clp (now we're committed towards STATE_FREE)
		 * 3. execute onfree stores, persisting each one
		 * 4. set state to FREE
		 * 5. persist *clp
		 */
		clp->size = sz | PMEM_STATE_FREEING;
		pmem_persist(clp, sizeof(*clp), 0);
		for (i = 0; i < PMEM_NUM_ON; i++)
			if (clp->on[i].off) {
				uintptr_t *dest =
					PMEM(pmp, (uintptr_t *)clp->on[i].off);
				*dest = (uintptr_t)clp->on[i].ptr_;
				pmem_persist(dest, sizeof(*dest), 0);
			} else
				break;
		for (i = PMEM_NUM_ON - 1; i >= 0; i--)
			clp->on[i].off = 0;
		pmem_persist(clp, sizeof(*clp), 0);
	}
	clp->size = sz | PMEM_STATE_FREE;
	pmem_persist(clp, sizeof(*clp), 0);

	/*
	 * at this point we may have adjacent free clumps that need
	 * to be coalesced.  there are three interesting cases:
	 * 	case 1: the clump below us is free (need to combine two clumps)
	 * 	case 2: the clump above us is free (need to combine two clumps)
	 * 	case 3: both are free (need to combining three clumps)
	 * XXX this can be much more optimal by using clp->prevsize to
	 *     get back to the clump below us.  for now, we just invoke
	 *     the recovery code for coalescing.
	 */
	pmemalloc_coalesce_free(pmp);
}

/*
 * pmemalloc_check -- check the consistency of a pmem pool
 *
 * Inputs:
 *	path -- path to the file which contains the memory pool
 *
 * The current state of the pmem pool is printed.  This routine does
 * not make any changes to the pmem pool (maps it read-only, in fact).
 * It is not necessary to call pmemalloc_init() before calling this.
 */
void pmemalloc_check(const char *path)
{
	void *pmp;
	int fd;
	struct stat stbuf;
	struct clump *clp;
	struct clump *lastclp;
	struct pool_header *hdrp;
	size_t clumptotal;
	/*
	 * stats we keep for each type of memory:
	 * 	stats[PMEM_STATE_FREE] for free clumps
	 * 	stats[PMEM_STATE_RESERVED] for reserved clumps
	 * 	stats[PMEM_STATE_ACTIVATING] for activating clumps
	 * 	stats[PMEM_STATE_ACTIVE] for active clumps
	 * 	stats[PMEM_STATE_FREEING] for freeing clumps
	 * 	stats[PMEM_STATE_UNUSED] for overall totals
	 */
	struct {
		size_t largest;
		size_t smallest;
		size_t bytes;
		unsigned count;
	} stats[PMEM_STATE_UNUSED + 1] = { 0 };
	const char *names[] = {
		"Free",
		"Reserved",
		"Activating",
		"Active",
		"Freeing",
		"TOTAL",
	};
	int i;

	DEBUG("path=%s", path);

	if ((fd = open(path, O_RDONLY)) < 0)
		FATALSYS("%s", path);

	if (fstat(fd, &stbuf) < 0)
		FATALSYS("fstat");

	DEBUG("file size 0x%lx", stbuf.st_size);

	if (stbuf.st_size < PMEM_MIN_POOL_SIZE)
		FATAL("size %lu too small (must be at least %lu)",
					stbuf.st_size, PMEM_MIN_POOL_SIZE);

	if ((pmp = mmap(NULL, stbuf.st_size, PROT_READ, MAP_SHARED,
					fd, 0)) == MAP_FAILED)
		FATALSYS("mmap");
	DEBUG("pmp %lx", pmp);

	close(fd);

	hdrp = PMEM(pmp, (struct pool_header *)PMEM_HDR_OFFSET);
	DEBUG("   hdrp 0x%lx (off 0x%lx)", hdrp, OFF(pmp, hdrp));

	if (strcmp(hdrp->signature, PMEM_SIGNATURE))
		FATAL("failed signature check");
	DEBUG("signature check passed");

	clp = PMEM(pmp, (struct clump *)PMEM_CLUMP_OFFSET);
	/*
	 * location of last clump is calculated by rounding the file
	 * size down to a multiple of 64, and then subtracting off
	 * another 64 to hold the struct clump.  the last clump is
	 * indicated by a size of zero.
	 */
	lastclp = PMEM(pmp, (struct clump *)
		(stbuf.st_size & ~(PMEM_CHUNK_SIZE - 1)) - PMEM_CHUNK_SIZE);
	DEBUG("    clp 0x%lx (off 0x%lx)", clp, OFF(pmp, clp));
	DEBUG("lastclp 0x%lx (off 0x%lx)", lastclp, OFF(pmp, lastclp));

	clumptotal = (uintptr_t)lastclp - (uintptr_t)clp;

	DEBUG("expected clumptotal: %lu", clumptotal);

	/*
	 * check that:
	 *
	 *   the overhead size (stuff up to CLUMP_OFFSET)
	 * + clumptotal
	 * + last clump marker (CHUNK_SIZE)
	 * + any bytes we rounded off the end
	 * = file size
	 */
	if (PMEM_CLUMP_OFFSET + clumptotal + 
		(stbuf.st_size & (PMEM_CHUNK_SIZE - 1)) + PMEM_CHUNK_SIZE
		== stbuf.st_size) {
		DEBUG("section sizes correctly add up to file size");
	} else {
		FATAL("CLUMP_OFFSET %d + clumptotal %lu + rounded %d + "
				"CHUNK_SIZE %d = %lu, (not st_size %lu)",
				PMEM_CLUMP_OFFSET, clumptotal,
				(stbuf.st_size & (PMEM_CHUNK_SIZE - 1)),
				PMEM_CHUNK_SIZE,
				PMEM_CLUMP_OFFSET + clumptotal +
				(stbuf.st_size & (PMEM_CHUNK_SIZE - 1)) +
				PMEM_CHUNK_SIZE,
				stbuf.st_size);
	}

	if (clp->size == 0)
		FATAL("no clumps found");

	while (clp->size) {
		size_t sz = clp->size & ~PMEM_STATE_MASK;
		int state = clp->size & PMEM_STATE_MASK;

		DEBUG("[%u]clump size 0x%lx state %d",
				OFF(pmp, clp), sz, state);
		DEBUG("on: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
			clp->on[0].off, clp->on[0].ptr_,
			clp->on[1].off, clp->on[1].ptr_,
			clp->on[2].off, clp->on[2].ptr_);

		if (sz > stats[PMEM_STATE_UNUSED].largest)
			stats[PMEM_STATE_UNUSED].largest = sz;
		if (stats[PMEM_STATE_UNUSED].smallest == 0 ||
		    sz < stats[PMEM_STATE_UNUSED].smallest)
			stats[PMEM_STATE_UNUSED].smallest = sz;
		stats[PMEM_STATE_UNUSED].bytes += sz;
		stats[PMEM_STATE_UNUSED].count++;

		switch (state) {
		case PMEM_STATE_FREE:
			DEBUG("clump state: free");
			ASSERTeq(clp->on[0].off, 0);
			ASSERTeq(clp->on[1].off, 0);
			ASSERTeq(clp->on[2].off, 0);
			break;

		case PMEM_STATE_RESERVED:
			DEBUG("clump state: reserved");
			break;

		case PMEM_STATE_ACTIVATING:
			DEBUG("clump state: activating");
			break;

		case PMEM_STATE_ACTIVE:
			DEBUG("clump state: active");
			ASSERTeq(clp->on[0].off, 0);
			ASSERTeq(clp->on[1].off, 0);
			ASSERTeq(clp->on[2].off, 0);
			break;

		case PMEM_STATE_FREEING:
			DEBUG("clump state: freeing");
			break;

		default:
			FATAL("unknown clump state: %d", state);
		}

		if (sz > stats[state].largest)
			stats[state].largest = sz;
		if (stats[state].smallest == 0 ||
		    sz < stats[state].smallest)
			stats[state].smallest = sz;
		stats[state].bytes += sz;
		stats[state].count++;

		clp = (struct clump *)((uintptr_t)clp + sz);
		DEBUG("next clp 0x%lx, offset 0x%lx", clp, OFF(pmp, clp));
	}

	if (clp == lastclp)
		DEBUG("all clump space accounted for");
	else
		FATAL("clump list stopped at %lx instead of %lx", clp, lastclp);

	if (munmap(pmp, stbuf.st_size) < 0)
		FATALSYS("munmap");

	/*
	 * print the report
	 */
	printf("Summary of pmem pool:\n");
	printf("File size: %lu, %d allocatable bytes in pool\n\n",
			stbuf.st_size, clumptotal);
	printf("     State      Bytes     Clumps    Largest   Smallest\n");
	for (i = 0; i < PMEM_STATE_UNUSED + 1; i++) {
		printf("%10s %10d %10d %10d %10d\n",
				names[i],
				stats[i].bytes,
				stats[i].count,
				stats[i].largest,
				stats[i].smallest);
	}
}
