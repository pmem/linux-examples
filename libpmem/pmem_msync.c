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
 * pmem_msync.c -- msync-based implementation of libpmem
 */

#include <sys/mman.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include "util/util.h"

#define	ALIGN 4096	/* assumes 4k page size for use with msync() */

/*
 * pmem_map -- map the Persistent Memory
 *
 * This is just a convenience function that calls mmap() with the
 * appropriate arguments.
 *
 * This is the msync-based version.
 */
void *
pmem_map_msync(int fd, size_t len)
{
	void *base;

	if ((base = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED,
					fd, 0)) == MAP_FAILED)
		return NULL;

	return base;
}

/*
 * pmem_drain_pm_stores -- wait for any PM stores to drain from HW buffers
 *
 * This is the msync-based version.
 */
void
pmem_drain_pm_stores_msync(void)
{
	/*
	 * Nothing to do here for the msync-based version.
	 */
}

/*
 * pmem_flush_cache -- flush processor cache for the given range
 *
 * This is the msync-based version.
 */
void
pmem_flush_cache_msync(void *addr, size_t len, int flags)
{
	uintptr_t uptr;

	/*
	 * msync requires len to be a multiple of pagesize, so
	 * adjust addr and len to represent the full 4k chunks
	 * covering the given range.
	 */

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (ALIGN - 1);

	/* round addr down to page boundary */
	uptr = (uintptr_t)addr & ~(ALIGN - 1);

	/* round len up to multiple of page size */
	len = (len + (ALIGN - 1)) & ~(ALIGN - 1);

	if (msync((void *)uptr, len, MS_SYNC) < 0)
		FATALSYS("msync");
}

/*
 * pmem_persist_msync -- make any cached changes to a range of PM persistent
 *
 * This is the msync-based version.
 */
void
pmem_persist_msync(void *addr, size_t len, int flags)
{
	pmem_flush_cache_msync(addr, len, flags);
	__builtin_ia32_sfence();
	pmem_drain_pm_stores_msync();
}
