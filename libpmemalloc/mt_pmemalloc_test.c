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
 * mt_pmemalloc_test.c
 *
 * The purpoose is to verify MT safeness of the pmem_alloc library.
 * The program creates a specified number of allocator threads that
 * each create an associated freeing thread.  Allocated regions are
 * passed through a set of mailboxes to the freeing threads whose
 * job is to deallocate the regions in a different order.  If run
 * on a multi-socket & multi-core machine, this should create many
 * opportunities to find unsafe operations in the library.
 *
 * Usage: mt_pmemalloc_test [-t num_threads]
 *                          [-r runtime]
 *                          [-s alloc_size] [-d] path
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>

#include "util/util.h"
#include "icount/icount.h"
#include "libpmem/pmem.h"
#include "pmemalloc.h"

#define TRUE  1
#define FALSE 0

/*
 * Defaults.  Most can be over-ridden via command line options
 */
#define MAX_THREADS         128
#define DEFAULT_RUNTIME     60
#define DEFAULT_ALLOC_SIZE  4096
#define MAILBOXES           128

#define POOL_SIZE   ((MAX_THREADS * MAILBOXES * DEFAULT_ALLOC_SIZE) + \
                     (sizeof(void*) * MAILBOXES * MAX_THREADS) + \
                     (256 * 1024) )

/*
 * Each allocating and freeing thread pair has a list of mailboxes.
 * The malloc thread will check for a NULL (empty) mailbox
 * and set it to point to a new region.  The freeing thread
 * will search for empty (NULL) mailboxes and free any pmem found.
 *
 * This will be stored in persistent memory and pmem region pointers
 * will be set using pmemalloc_activate to veryify its mt safeness.
 */
typedef void *mailbox_array_t[MAX_THREADS][MAILBOXES];

/* 'main()' routines for the allocating and freeing threads.  */
void *alloc_main( void * );
void *free_main( void * );

/* Static data structures shared between threads */
static mailbox_array_t  *mbx_array_ptr; /* POINTER to the array in pmem */
static pthread_mutex_t start_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t start_cv = PTHREAD_COND_INITIALIZER;
static pthread_t  alloc_threads[MAX_THREADS];
static int b_start_flag = FALSE;
static int b_all_stop = FALSE;
static unsigned int num_threads = MAX_THREADS;
static unsigned int runtime = DEFAULT_RUNTIME;
static unsigned int max_malloc = DEFAULT_ALLOC_SIZE;
static void *pmp;

/* for USAGE() */
char Usage[] = "[-t num_threads] [-r runtime] [-s alloc_size] [-d] path";

int
main(int argc, char *argv[])
{
    const char *path;
    int opt;
    int retval;
    unsigned long thrd;
    int mbx;
    void **sa_ptr;
    mailbox_array_t *mbx_offset_; 

    Myname = argv[0];

    while ((opt = getopt(argc, argv, "t:r:s:d")) != -1) {
        switch (opt) {
        case 't':
            if (sscanf(optarg, "%u", &num_threads) == EOF) {
                USAGE( "-t option error");
            }
            if (num_threads > MAX_THREADS) {
                fprintf( stderr, "using max threads %d\n", MAX_THREADS );
                num_threads = MAX_THREADS;
            }
            break;

        case 'r':
            if (sscanf(optarg, "%u", &runtime)==EOF) {
                USAGE("-r option error");
            }
            break;

        case 's':
            if (sscanf(optarg, "%u", &max_malloc)==EOF) 
                USAGE("-s option error");
            break;

        case 'd':
            Debug=TRUE;
            break;

        default:
            USAGE(NULL);
        } 
    } /* end while opt */

    if (optind >= argc)
        USAGE("No path given");
    path = argv[optind++];

    if (optind < argc)
        USAGE(NULL);

    /*
     * Use the alloc_init lib function to open the pool
     * via pmfs, and map it into our address space.
     * This returns a regular (absolute) pointer.
     */
    if ((pmp = pmemalloc_init(path, POOL_SIZE)) == NULL)
        FATALSYS("pmemalloc_init on %s", path);

	/*
     * Fetch our static info.
     * The first word is used to store a relative pointer to
     * the mailbox array.  The library function converts this
     * to an absolute pointer.
     */
    sa_ptr = (void**)pmemalloc_static_area(pmp);

    /* Assume the static area for a new pmem pool is zero??????? */
    if (*sa_ptr == NULL) {
        /*
         * Create and initialize the mailbox array in PM
         */
        if ((mbx_offset_=pmemalloc_reserve(pmp,
                            sizeof(mailbox_array_t))) == NULL )
            FATALSYS("pmemalloc mailbox array");
        /*
         * Place a pointer to this array in the first word of the
         * static area on activation
         */
        pmemalloc_onactive( pmp, mbx_offset_, (void**)sa_ptr, mbx_offset_ );
        pmemalloc_activate( pmp, mbx_offset_ );
        
        /* Set the static, regular pointer to be used in the program */
        mbx_array_ptr = PMEM( pmp, mbx_offset_ );
        for (thrd=0; thrd<MAX_THREADS; ++thrd) {
            for (mbx=0; mbx<MAILBOXES; ++mbx) {
                (*mbx_array_ptr)[thrd][mbx] = NULL;
            }
        }
     } else {
         /*
          * This region already exists from a previous run.
          * Free any pmem spaces still in the mailbox.
          */
         mbx_array_ptr = PMEM( pmp, (mailbox_array_t*)*sa_ptr );
         for (thrd=0; thrd<MAX_THREADS; ++thrd) {
             for (mbx=0; mbx<MAILBOXES; ++mbx) {
                 if ((*mbx_array_ptr)[thrd][mbx] != NULL) {
                     pmemalloc_onfree( pmp,
                                    (*mbx_array_ptr)[thrd][mbx],
                                    &(*mbx_array_ptr)[thrd][mbx],
                                    NULL );
                     pmemalloc_free( pmp, (*mbx_array_ptr)[thrd][mbx] );
                 }
             }
         }
    }
    /* Commit the initialized mailbox to persistent media */
    pmem_persist( mbx_array_ptr, sizeof(mailbox_array_t), 0 );

    DEBUG( "Number of threads = %d", num_threads);
    DEBUG( "Runtime: %d seconds", runtime );
    DEBUG( "Max alloc size %d bytes", max_malloc );

    /*
     * Create each allocating thread.  Each allocating thread
     * will create its corresponding freeing thread.
     * Once each each thread is created, signal the start condition
     * so they all start running around the same time.
     */
    for (thrd=0; thrd<num_threads; ++thrd) {
        retval = pthread_create( &alloc_threads[thrd],
                    NULL,
                    alloc_main,
                    (void *) thrd );
        if (retval) {
            errno = retval;
            FATALSYS( "alloc thread create %d\n", thrd );
        }
    }

    /*
     * Synchronize the start so all the threads start
     * close to the same time.
     */
    sleep(0);

    pthread_mutex_lock( &start_lock );
    b_start_flag = TRUE;
    pthread_cond_broadcast( &start_cv );
    pthread_mutex_unlock( &start_lock );

    /*
     * Let run for the desired seconds then tell all threads to stop
     */
    sleep( runtime );
    b_all_stop = TRUE;

    /*
     * Wait for each alloating thread to complete.
     */
    for (thrd=0; thrd<num_threads; ++thrd) {
        retval = pthread_join( alloc_threads[thrd], NULL );
        if (retval) {
            errno = retval;
            FATALSYS( "Allocating thread JOIN %d", thrd );
        }
    }

    /* Commit the final mailbox array to persistent media */
    pmem_persist( mbx_array_ptr, sizeof(mailbox_array_t), 0 );

    DEBUG("Done.");
	exit(0);
}

/*
 * main() for the allocating thread.
 */
void *
alloc_main( void *tparam )
{
    unsigned long thread_num = (unsigned long) tparam;
    int retval;
    void **mbx_list = (*mbx_array_ptr)[thread_num];
    pthread_t  free_thread;
    void *rel_mem_ptr_ = NULL;
    unsigned int malloc_size = 0;
    int mbx;

    DEBUG( "Enter alloc thread %d\n", thread_num );

    retval = pthread_create( &free_thread,
                  NULL,
                  free_main,
                  (void *) thread_num );
    if (retval) {
        errno = retval;
        FATALSYS( "free thread create in thread %l\n", thread_num );
    }

    /*
     * Wait for the starting gun to send all the threads
     * off and running....
     */
    pthread_mutex_lock( &start_lock );
    while (!b_start_flag) {
        pthread_cond_wait( &start_cv, &start_lock );
    }
    pthread_mutex_unlock( &start_lock );  

    /*
     * Until the parent thread says to stop just keep looping
     * through the mailboxes looking for empty ones.  When one is
     * found, do a random sized alloc and put a pointer in the mailbox
     * for the freeing thread to free.
     */
    while (!b_all_stop) {
        for (mbx=0; mbx<MAILBOXES; ++mbx) {
            if (mbx_list[mbx] == NULL) {
                malloc_size = (int)random() % max_malloc;

                if( (rel_mem_ptr_=pmemalloc_reserve( pmp,
                                        malloc_size )) != NULL ) {
                    pmemalloc_onactive( pmp, rel_mem_ptr_,
                                        &mbx_list[mbx],
                                        rel_mem_ptr_ );
                    pmemalloc_activate( pmp, rel_mem_ptr_ );
                    DEBUG( "malloc %d bytes", malloc_size );
                } else {
                    /*
                     * Malloc failed.
                     * Sleep to let the Freeing thread catch up.
                     */
                    DEBUG( "malloc failed for size %d\n", malloc_size );
                    sleep(0);
                }
            } /* end if mbx empty */
        } /* end for each mbx */
    } /* end while not all_stop */

    /*
     * Time to stop.  Wait for the freeing thread to complete.
     */
    retval = pthread_join( free_thread, NULL );
    if (retval) {
        errno = retval;
        FATALSYS( "Join with freeing thread %d\n", thread_num );
    }
    pthread_exit( NULL );
}
 
/*
 * 'main' for the freeing thread
 */
void *
free_main (void *tparam )
{
    unsigned long thread_num = (unsigned long)tparam;
    void **mbx_list = (*mbx_array_ptr)[thread_num];
    int mbx_index;

    DEBUG( "Enter free thread %d\n", thread_num );
    /*
     * Wait for the starting gun....
     */
    pthread_mutex_lock( &start_lock );
    while (!b_start_flag) {
        pthread_cond_wait( &start_cv, &start_lock );
    }
    pthread_mutex_unlock( &start_lock );  

    while (!b_all_stop) {
        /*
         * Pick a random mbx and if that mailbox is full, free it.
         */
        mbx_index = ((int)random()) % MAILBOXES;
        if (mbx_list[mbx_index] != NULL) {
            /*
             * Tell the library to reliably set the mailbox
             * to NULL when we free this chunk of PM.
             */
            pmemalloc_onfree( pmp, mbx_list[mbx_index],
                              &mbx_list[mbx_index], NULL );
            pmemalloc_free( pmp, mbx_list[mbx_index] );
            DEBUG( "Free thread %d, mailbox %d", thread_num, mbx_index ); 
        }
    }
    /*
     * Make one last pass through to free any remaining chunks.
     */
    for (mbx_index=0; mbx_index<MAILBOXES; ++mbx_index) {
        if (mbx_list[mbx_index] != NULL) {
            pmemalloc_onfree( pmp, mbx_list[mbx_index],
                              &mbx_list[mbx_index], NULL );
            pmemalloc_free( pmp, mbx_list[mbx_index] );
        }
    }
    pthread_exit( NULL );
}



