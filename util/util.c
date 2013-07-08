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
 * util.c -- some simple utility routines
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>

#include "util.h"

/*
 * debug -- printf-like debug messages
 */
void
debug(const char *file, int line, const char *func, const char *fmt, ...)
{
	va_list ap;
	int save_errno;

	if (!Debug)
		return;

	save_errno = errno;

	fprintf(stderr, "debug: %s:%d %s()", file, line, func);
	if (fmt) {
		fprintf(stderr, ": ");
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
	fprintf(stderr, "\n");
	errno = save_errno;
}

/*
 * fatal -- printf-like error exits, with and without errno printing
 */
void
fatal(int err, const char *file, int line, const char *func,
	const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "ERROR: %s:%d %s()", file, line, func);
	if (fmt) {
		fprintf(stderr, ": ");
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
	if (err)
		fprintf(stderr, ": %s", strerror(err));
	fprintf(stderr, "\n");
	exit(1);
}

/*
 * exename -- figure out the name used to run this program
 *
 * Internal -- used by usage().
 */
static const char *
exename(void)
{
	char proc[PATH_MAX];
	static char exename[PATH_MAX];
	int nbytes;

	snprintf(proc, PATH_MAX, "/proc/%d/exe", getpid());
	if ((nbytes = readlink(proc, exename, PATH_MAX)) < 0)
		strcpy(exename, "Unknown");
	else
		exename[nbytes] = '\0';

	return exename;
}

/*
 * usage -- printf-like usage message emitter
 */
void
usage(const char *argfmt, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "Usage: %s", (Myname == NULL) ? exename() : Myname);
	if (argfmt)
		fprintf(stderr, " %s", argfmt);
	if (fmt) {
		fprintf(stderr, ": ");
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
	fprintf(stderr, "\n");
	exit(1);
}
