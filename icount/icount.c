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
 * icount.c -- quick & dirty instruction count
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>

#include "util/util.h"
#include "icount/icount.h"

static pid_t Tracer_pid;		/* PID of tracer process */
static int Tracerpipe[2];		/* pipe from tracer to parent */
static unsigned long Total;		/* instruction count */
static unsigned long Nothing;
static jmp_buf Trigjmp;

/*
 * handler -- catch SIGRTMIN+15 in tracee once tracer is established
 */
static void
handler()
{
	longjmp(Trigjmp, 1);
}

/*
 * pretrigger -- internal function used to detect start of tracing
 *
 * This function must immediately precede trigger() in this file.
 */
static void
pretrigger(void)
{
	int x = getpid();

	/*
	 * this is just a few instructions to make sure
	 * this function doesn't get optimized away into
	 * nothing.  the tracer will find us here and send
	 * us a signal, causing us to longjmp out.
	 */
	while (1) {
		x++;	/* wait for tracer to attach */
		Nothing += x;
	}
}

/*
 * trigger -- internal function used to detect start of tracing
 */
static void
trigger(void)
{
	Nothing = 0;	/* avoid totally empty function */
}

/*
 * tracer -- internal function used in child process to trace parent
 */
static void
tracer(unsigned long ttl)
{
	pid_t ppid = getppid();
	int status;
	int triggered = 0;
	int signaled = 0;

	if (ptrace(PTRACE_ATTACH, ppid, 0, 0) < 0)
		FATALSYS("PTRACE_ATTACH");

	while (1) {
		if (waitpid(ppid, &status, 0) < 0)
			FATALSYS("waitpid(pid=%d)", ppid);

		if (WIFSTOPPED(status)) {
			struct user_regs_struct regs;

			if (triggered)
				Total++;

			if (ttl && Total >= ttl) {
				if (kill(ppid, SIGKILL) < 0)
					FATAL("SIGKILL %d", ppid);
				printf("Program terminated after %lu "
						"instructions\n",
						Total);
				fflush(stdout);
				_exit(0);
			}

			if (ptrace(PTRACE_GETREGS, ppid, 0, &regs) < 0)
				FATALSYS("PTRACE_GETREGS");

			if ((unsigned long)regs.rip >=
					(unsigned long)pretrigger &&
			    (unsigned long)regs.rip <
					(unsigned long)trigger) {
				if (!signaled) {
					if (ptrace(PTRACE_SYSCALL, ppid, 0,
							SIGRTMIN+15) < 0)
						FATALSYS("PTRACE_SYSCALL");
					signaled = 1;
					continue;
				}
			} else if ((unsigned long) regs.rip ==
					(unsigned long) trigger) {
				triggered = 1;
			} else if ((unsigned long) regs.rip ==
					(unsigned long) icount_stop) {
				if (ptrace(PTRACE_DETACH, ppid, 0, 0) < 0)
					FATALSYS("PTRACE_DETACH");
				break;
			}

			if (ptrace(PTRACE_SINGLESTEP, ppid, 0, 0) < 0)
				FATALSYS("PTRACE_SINGLESTEP");
		} else if (WIFEXITED(status))
			FATAL("tracee: exit %d", WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
			FATAL("tracee: %s", strsignal(WTERMSIG(status)));
		else
			FATAL("unexpected wait status: 0x%x", status);
	}

	/*
	 * our counting is done, send the count back to the tracee
	 * via the pipe and exit.
	 */
	if (write(Tracerpipe[1], &Total, sizeof(Total)) < 0)
		FATALSYS("write to pipe");
	close(Tracerpipe[1]);
	_exit(0);
}

/*
 * icount_start -- begin instruction counting
 *
 * Inputs:
 * 	life_remaining -- simulate a crash after counting this many
 * 			  instructions.  pass in 0ull to disable this
 * 			  feature.
 *
 * There is some variability on how far the parent can get before the child
 * attaches to it for tracing, especially when the system is loaded down
 * (e.g. due to zillions of parallel icount traces happening at once).
 * To coordinate this, we use a multi-step procedure where the tracee
 * runs until it gets to the function pretrigger() and the tracer() detects
 * that and raises a signal (SIGRTMIN+15) in the tracee, who then catches
 * it and longjmps out of signal handler, back here, and then calls trigger().
 *
 * The multi-step coordination sounds complex, but it handles the
 * surprisingly common cases where the tracer attached to the tracee
 * before it even finished executing the libc fork() wrapper, and the
 * other end of the spectrum where the tracee ran way too far ahead
 * before the tracer attached and the tracer missed the call to trigger().
 */
void
icount_start(unsigned long life_remaining)
{
	if (Tracer_pid) {
		/* nested call not allowed */
		icount_stop();
		FATAL("icount_start called while counting already active");
	}

	Total = 0ull;

	if (pipe(Tracerpipe) < 0)
		FATALSYS("pipe");

	if (signal(SIGRTMIN+15, handler) == SIG_ERR)
		FATALSYS("signal: SIGRTMIN+15");

	if ((Tracer_pid = fork()) < 0)
		FATALSYS("fork");
	else if (Tracer_pid) {
		/* parent */
		close(Tracerpipe[1]);
		if (!setjmp(Trigjmp))
			pretrigger();
		close(-1);		/* harmless syscall for tracer */
		trigger();
		return;
	} else {
		/* child */
		close(Tracerpipe[0]);
		tracer(life_remaining);
	}
}

/*
 * icount_stop -- stop counting instructions
 */
void
icount_stop(void)
{
	int status;

	if (read(Tracerpipe[0], &Total, sizeof(Total)) < 0)
		FATALSYS("read from pipe");
	close(Tracerpipe[0]);

	/* wait for child */
	if (waitpid(Tracer_pid, &status, 0) < 0)
		FATALSYS("waitpid(pid=%d)", Tracer_pid);
	Tracer_pid = 0;
}

/*
 * icount_total -- return total from last count exercise
 *
 * Outputs:
 * 	The return value is the total count of instructions
 * 	executed between the last calls to icount_start()
 * 	and icount_stop().
 *
 * This function only returns valid counts when counting is not
 * active and when a count has previously been started and stopped.
 */
unsigned long
icount_total(void)
{
	return Total;
}
