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
 * pmemalloc.h -- example malloc library for Persistent Memory
 */

#include <stdint.h>

/*
 * size of the static area returned by pmem_static_area()
 */
#define	PMEM_STATIC_SIZE 4096

/*
 * number of onactive/onfree values allowed
 */
#define	PMEM_NUM_ON 3

/*
 * given a relative pointer, add in the base associated
 * with the given Persistent Memory Pool (pmp).
 */
#define	PMEM(pmp, ptr_) ((typeof(ptr_))(pmp + (uintptr_t)ptr_))

void *pmemalloc_init(const char *path, size_t size);
void *pmemalloc_static_area(void *pmp);
void *pmemalloc_reserve(void *pmp, size_t size);
void pmemalloc_onactive(void *pmp, void *ptr_, void **parentp_, void *nptr_);
void pmemalloc_onfree(void *pmp, void *ptr_, void **parentp_, void *nptr_);
void pmemalloc_activate(void *pmp, void *ptr_);
void pmemalloc_free(void *pmp, void *ptr_);
void pmemalloc_check(const char *path);
