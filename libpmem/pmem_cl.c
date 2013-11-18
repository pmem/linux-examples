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
 * Copyright (c) 2013, NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the NetApp, Inc. nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE..
 */

/*
 * pmem_cl.c -- cache-line-based implementation of libpmem
 *
 * WARNING: This is for use with Persistent Memory -- if you use this
 * with a traditional page-cache-based memory mapped file, your changes
 * will not be durable.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/uio.h>

#define ALIGN 64	/* assumes 64B cache line size */

/*
 * pmem_map -- map the Persistent Memory
 *
 * This is just a convenience function that calls mmap() with the
 * appropriate arguments.
 *
 * This is the cache-line-based version.
 */
void *
pmem_map_cl(int fd, size_t len)
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
 * This is the cache-line-based version.
 */
void
pmem_drain_pm_stores_cl(void)
{
	/*
	 * Nothing to do here -- this implementation assumes the platform
	 * has something like Intel's ADR feature, which flushes HW buffers
	 * automatically on power loss.  This implementation further assumes
	 * the Persistent Memory hardware itself doesn't need to be alerted
	 * when something needs to be persistent.  Of course these assumptions
	 * may be wrong for different combinations of Persistent Memory
	 * products and platforms, but this is just example code.
	 *
	 * TODO: update this to work on other types of platforms.
	 */
}

/*
 * pmem_flush_cache -- flush processor cache for the given range
 *
 * This is the cache-line-based version.
 */
void
pmem_flush_cache_cl(void *addr, size_t len, int flags)
{
	uintptr_t uptr;
	
	/* loop through 64B-aligned chunks covering the given range */
	for (uptr = (uintptr_t)addr & ~(ALIGN - 1);
			uptr < (uintptr_t)addr + len; uptr += 64)
		__builtin_ia32_clflush((void *)uptr);
}

/*
 * pmem_load_cache -- load given range into the processor cache
 *
 * This is the cache-line-based version.
 */
void
pmem_load_cache_cl(void *addr, size_t len, int flags)
{
	uintptr_t uptr;

	/* loop through 64B-aligned chunks covering the given range */
	for (uptr = (uintptr_t)addr & ~(ALIGN - 1);
			uptr < (uintptr_t)addr + len; uptr += 64)
		__asm__ __volatile__ 
			( "movdqa %0, %%xmm0\n" : : "m"(*(char *)uptr) );
}

/*
 * pmem_persist -- make any cached changes to a range of PM persistent
 *
 * This is the cache-line-based version.
 */
void
pmem_persist_cl(void *addr, size_t len, int flags)
{
	pmem_flush_cache_cl(addr, len, flags);
	__builtin_ia32_sfence();
	pmem_drain_pm_stores_cl();
}

/*
 * pm_persist_iov --- make any cached changes to an array of (discontinuous)
 * ranges of PM persistent
 *
 * This is the cache-line-based version.
 */
int pmem_persist_iov_cl(const struct iovec *addrs, size_t count, int flags)
{
	int rc = 0;
	unsigned i;
	struct iovec *range;

	for (i = 0; i < count; i += 1) {
		range = (struct iovec *)(addrs + i);
		pmem_flush_cache_cl(range->iov_base, range->iov_len, flags);
	}
	__builtin_ia32_sfence();
	pmem_drain_pm_stores_cl();

	/*
	 * While this implementation cannot encounter an error condition, 
	 * other implementations may. Hence this example code template 
	 * provides a return code.
	 */
	return rc;
}

/*
 * pm_persist_iov_verify --- make any cached changes to an array of
 * (discontinuous) ranges of PM persistent with Posix synchronized I/O data
 * integrity completion, i.e. O_SYNC-like behavior
 *
 * This is the cache-line-based version.
 */
int pmem_persist_iov_verify_cl(const struct iovec *addrs, size_t count,
								int flags)
{
	int rc = 0;
	uint64_t addr;
	unsigned i;
	struct iovec *range;

	for (i = 0; i < count; i += 1) {
		range = (struct iovec *)(addrs + i);
		pmem_flush_cache_cl(range->iov_base, range->iov_len, flags);
	}
	__builtin_ia32_mfence();
	pmem_drain_pm_stores_cl();

	/*
	 * Verify that all ranges have either been successfully transferred or 
	 * diagnosed as unsuccessful 
	 *
	 * TODO: any read error should be caught and returned as EIO.
	 */
	for (i = 0; i < count; i += 1) {
		range = (struct iovec *)(addrs + i);
		pmem_load_cache_cl(range->iov_base, range->iov_len, flags);
	}

	/*
	 * TODO: On any read error, return -1 and set errno to EIO.
	 */
	return rc;
}
