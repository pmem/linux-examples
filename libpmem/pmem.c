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
 * pmem.c -- entry points for libpmem
 */

#include <sys/types.h>

#include "pmem.h"

/* dispatch tables for the various versions of libpmem */
void *pmem_map_cl(int fd, size_t len);
void pmem_persist_cl(void *addr, size_t len, int flags);
void pmem_flush_cache_cl(void *addr, size_t len, int flags);
void pmem_drain_pm_stores_cl(void);
void *pmem_map_msync(int fd, size_t len);
void pmem_persist_msync(void *addr, size_t len, int flags);
void pmem_flush_cache_msync(void *addr, size_t len, int flags);
void pmem_drain_pm_stores_msync(void);
void *pmem_map_fit(int fd, size_t len);
void pmem_persist_fit(void *addr, size_t len, int flags);
void pmem_flush_cache_fit(void *addr, size_t len, int flags);
void pmem_drain_pm_stores_fit(void);
#define	PMEM_CL_INDEX 0
#define	PMEM_MSYNC_INDEX 1
#define	PMEM_FIT_INDEX 2
static void *(*Map[])(int fd, size_t len) =
		{ pmem_map_cl, pmem_map_msync, pmem_map_fit };
static void (*Persist[])(void *addr, size_t len, int flags) =
		{ pmem_persist_cl, pmem_persist_msync, pmem_persist_fit };
static void (*Flush[])(void *addr, size_t len, int flags) =
		{ pmem_flush_cache_cl, pmem_flush_cache_msync,
			pmem_flush_cache_fit };
static void (*Drain_pm_stores[])(void) =
		{ pmem_drain_pm_stores_cl, pmem_drain_pm_stores_msync,
		pmem_drain_pm_stores_fit };
static int Mode = PMEM_CL_INDEX;	/* current libpmem mode */

/*
 * pmem_msync_mode -- switch libpmem to msync mode
 *
 * Must be called before any other libpmem routines.
 */
void
pmem_msync_mode(void)
{
	Mode = PMEM_MSYNC_INDEX;
}

/*
 * pmem_fit_mode -- switch libpmem to fault injection test mode
 *
 * Must be called before any other libpmem routines.
 */
void
pmem_fit_mode(void)
{
	Mode = PMEM_FIT_INDEX;
}

/*
 * pmem_map -- map the Persistent Memory
 */
void *
pmem_map(int fd, size_t len)
{
	return (*Map[Mode])(fd, len);
}

/*
 * pmem_persist -- make any cached changes to a range of PM persistent
 */
void
pmem_persist(void *addr, size_t len, int flags)
{
	(*Persist[Mode])(addr, len, flags);
}

/*
 * pmem_flush_cache -- flush processor cache for the given range
 */
void
pmem_flush_cache(void *addr, size_t len, int flags)
{
	(*Flush[Mode])(addr, len, flags);
}

/*
 * pmem_fence -- fence/store barrier for Peristent Memory
 */
void
pmem_fence(void)
{
	__builtin_ia32_sfence();
}

/*
 * pmem_drain_pm_stores -- wait for any PM stores to drain from HW buffers
 */
void
pmem_drain_pm_stores(void)
{
	(*Drain_pm_stores[Mode])();
}
