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
 * pmem_fit.h -- implementation of libpmem for fault injection testing
 *
 * WARNING: This is a special implementation of libpmem designed for
 * fault injection testing.  Performance will be very slow with this
 * implementation and main memory usage will balloon as the copy-on-writes
 * happen.  Don't use this unless you read the README and know what you're
 * doing.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#include "util/util.h"

#define	ALIGN 64	/* assumes 64B cache line size */

static int PM_fd;
static uintptr_t PM_base;

/*
 * pmem_map -- map the Persistent Memory
 *
 * This is the fit version (fault injection test) that uses copy-on-write.
 */
void *
pmem_map_fit(int fd, size_t len)
{
	void *base;

	if ((base = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_PRIVATE,
					fd, 0)) == MAP_FAILED)
		return NULL;

	PM_base = (uintptr_t)base;
	PM_fd = dup(fd);

	return base;
}

/*
 * pmem_drain_pm_stores -- wait for any PM stores to drain from HW buffers
 *
 * This is the fit version (fault injection test) that uses copy-on-write.
 */
void
pmem_drain_pm_stores_fit(void)
{
	/*
	 * Nothing to do here for the fit version.
	 */
}

/*
 * pmem_flush_cache -- flush processor cache for the given range
 *
 * This is the fit version (fault injection test) that uses copy-on-write.
 */
void
pmem_flush_cache_fit(void *addr, size_t len, int flags)
{
	uintptr_t uptr;

	if (!PM_base)
		FATAL("pmem_map hasn't been called");

	/*
	 * even though pwrite() can take any random byte addresses and
	 * lengths, we simulate cache flushing by writing the full 64B
	 * chunks that cover the given range.
	 */
	for (uptr = (uintptr_t)addr & ~(ALIGN - 1);
			uptr < (uintptr_t)addr + len; uptr += ALIGN)
		if (pwrite(PM_fd, (void *)uptr, ALIGN, uptr - PM_base) < 0)
			FATALSYS("pwrite len %d offset %lu", len,
					addr - PM_base);
}

/*
 * pmem_persist_fit -- make any cached changes to a range of PM persistent
 *
 * This is the fit version (fault injection test) that uses copy-on-write.
 */
void
pmem_persist_fit(void *addr, size_t len, int flags)
{
	pmem_flush_cache_fit(addr, len, flags);
	__builtin_ia32_sfence();
	pmem_drain_pm_stores_fit();
}
