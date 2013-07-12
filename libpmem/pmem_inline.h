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
 * pmem_inline.h -- inline versions of pmem for performance
 *
 * If your program is so latency sensitive that the overhead of
 * function calls need to be optimized out, include this file
 * instead of pmem.h.  Of course, the downside is that the pmem
 * library functions will then be compiled into your program so
 * if the pmem library gets updated, your program will have to be
 * recompiled to use the updates.
 *
 * This inlined version only contains the cache-line version of
 * the pmem interfaces (not the "msync mode" or "fault injection
 * test mode" interfaces available with the full libpmem.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>

#define	ALIGN 64	/* assumes 64B cache line size */

static inline void *
pmem_map(int fd, size_t len)
{
	void *base;

	if ((base = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED,
					fd, 0)) == MAP_FAILED)
		return NULL;

	return base;
}

static inline void
pmem_drain_pm_stores(void)
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

static inline void
pmem_flush_cache(void *addr, size_t len, int flags)
{
	uintptr_t uptr;
	
	/* loop through 64B-aligned chunks covering the given range */
	for (uptr = (uintptr_t)addr & ~(ALIGN - 1);
			uptr < (uintptr_t)addr + len; uptr += 64)
		__builtin_ia32_clflush((void *)uptr);
}

static inline void
pmem_persist(void *addr, size_t len, int flags)
{
	pmem_flush_cache(addr, len, flags);
	__builtin_ia32_sfence();
	pmem_drain_pm_stores();
}
