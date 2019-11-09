/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

#ifndef LIBGIT2_NO_FEATURES_H
# include "git2/sys/features.h"
#endif

#include "git2/common.h"
#include "cc-compat.h"

/** Declare a function as always inlined. */
#if defined(_MSC_VER)
# define GIT_INLINE(type) static __inline type
#elif defined(__GNUC__)
# define GIT_INLINE(type) static __inline__ type
#else
# define GIT_INLINE(type) static type
#endif

/** Support for gcc/clang __has_builtin intrinsic */
#ifndef __has_builtin
# define __has_builtin(x) 0
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef GIT_WIN32

# include <io.h>
# include <direct.h>
# include <winsock2.h>
# include <windows.h>
# include <ws2tcpip.h>
# include "win32/msvc-compat.h"
# include "win32/mingw-compat.h"
# include "win32/w32_common.h"
# include "win32/win32-compat.h"
# include "win32/error.h"
# include "win32/version.h"
# ifdef GIT_THREADS
#	include "win32/thread.h"
# endif

#else

# include <unistd.h>
# include <strings.h>
# ifdef GIT_THREADS
#	include <pthread.h>
#	include <sched.h>
# endif
#define GIT_STDLIB_CALL

#ifdef GIT_USE_STAT_ATIMESPEC
# define st_atim st_atimespec
# define st_ctim st_ctimespec
# define st_mtim st_mtimespec
#endif

# include <arpa/inet.h>

#endif

#include "git2/types.h"
#include "git2/errors.h"
#include "errors.h"
#include "thread-utils.h"
#include "integer.h"

/*
 * Include the declarations for deprecated functions; this ensures
 * that they're decorated with the proper extern/visibility attributes.
 */
#include "git2/deprecated.h"

#include "posix.h"

#define DEFAULT_BUFSIZE 65536
#define FILEIO_BUFSIZE DEFAULT_BUFSIZE
#define FILTERIO_BUFSIZE DEFAULT_BUFSIZE
#define NETIO_BUFSIZE DEFAULT_BUFSIZE

/**
 * Check a pointer allocation result, returning -1 if it failed.
 */
#define GIT_ERROR_CHECK_ALLOC(ptr) if (ptr == NULL) { return -1; }

/**
 * Check a buffer allocation result, returning -1 if it failed.
 */
#define GIT_ERROR_CHECK_ALLOC_BUF(buf) if ((void *)(buf) == NULL || git_buf_oom(buf)) { return -1; }

/**
 * Check a return value and propagate result if non-zero.
 */
#define GIT_ERROR_CHECK_ERROR(code) \
	do { int _err = (code); if (_err) return _err; } while (0)

/**
 * Generic libgit2 option struct
 */
typedef struct {
	unsigned int version;
	char body[];
} git_struct__version;

/**
 * Check a versioned structure for validity
 */
GIT_INLINE(int) git_struct__check_version(const git_struct__version *structure, unsigned int expected_max, const char *name)
{
	unsigned int actual;

	if (!structure)
		return 0;

	actual = structure->version;
	if (actual > 0 && actual <= expected_max)
		return 0;

	git_error_set(GIT_ERROR_INVALID, "invalid version %d on %s", actual, name);
	return -1;
}

#define GIT_ERROR_CHECK_VERSION(S,V,N) if (git_struct__check_version((git_struct__version *)S,V,N) < 0) return -1

typedef int (*git_struct__upgrade_cb)(git_struct__version *new_struct, const git_struct__version *structure, const char *name);

GIT_INLINE(int) git_struct__upgrade(
	git_struct__version *structure,
	size_t struct_len,
	const git_struct__version *user_struct,
	const char *name,
	git_struct__upgrade_cb upgrader)
{
	unsigned int max_version;

	assert(structure);

	if (!user_struct)
		return 0;

	max_version = structure->version;
	GIT_ERROR_CHECK_VERSION(user_struct, max_version, name);

	if (user_struct->version == structure->version) {
		memcpy(structure, user_struct, struct_len);
		return 0;
	}

	if (!upgrader) {
		git_error_set(GIT_ERROR_INVALID, "cannot upgrade older version %d of struct %s", user_struct->version, name);
		return -1;
	}

	if (upgrader(structure, user_struct, name) < 0) {
		git_error_set(GIT_ERROR_INVALID, "failed to upgrade older version %d of struct %s", user_struct->version, name);
		return -1;
	}

	/* post-check that the upgrader did something */
	assert(structure->version == max_version);

	return 1;
}

#define GIT_STRUCT_UPGRADE(struct, struct_len, user_struct, struct_name, upgrade_func) \
	if (git_struct__upgrade((void *)struct, struct_len, \
	    (void *)user_struct, struct_name, \
		(git_struct__upgrade_cb)upgrade_func) < 0) \
		return -1;

/**
 * Initialize a structure with a version.
 */
GIT_INLINE(void) git_struct__init(void *structure, size_t len, unsigned int version)
{
	memset(structure, 0, len);
	*((int*)structure) = version;
}
#define GIT_STRUCT_INIT(S,V) git_struct__init(S, sizeof(*S), V)

#define GIT_STRUCT_INIT_FROM_TEMPLATE(PTR,VERSION,TYPE,TPL) do { \
	TYPE _tmpl = TPL; \
	GIT_ERROR_CHECK_VERSION(&(VERSION), _tmpl.version, #TYPE);	\
	memcpy((PTR), &_tmpl, sizeof(_tmpl)); } while (0)


/** Check for additive overflow, setting an error if would occur. */
#define GIT_ADD_SIZET_OVERFLOW(out, one, two) \
	(git__add_sizet_overflow(out, one, two) ? (git_error_set_oom(), 1) : 0)

/** Check for additive overflow, setting an error if would occur. */
#define GIT_MULTIPLY_SIZET_OVERFLOW(out, nelem, elsize) \
	(git__multiply_sizet_overflow(out, nelem, elsize) ? (git_error_set_oom(), 1) : 0)

/** Check for additive overflow, failing if it would occur. */
#define GIT_ERROR_CHECK_ALLOC_ADD(out, one, two) \
	if (GIT_ADD_SIZET_OVERFLOW(out, one, two)) { return -1; }

#define GIT_ERROR_CHECK_ALLOC_ADD3(out, one, two, three) \
	if (GIT_ADD_SIZET_OVERFLOW(out, one, two) || \
		GIT_ADD_SIZET_OVERFLOW(out, *(out), three)) { return -1; }

#define GIT_ERROR_CHECK_ALLOC_ADD4(out, one, two, three, four) \
	if (GIT_ADD_SIZET_OVERFLOW(out, one, two) || \
		GIT_ADD_SIZET_OVERFLOW(out, *(out), three) || \
		GIT_ADD_SIZET_OVERFLOW(out, *(out), four)) { return -1; }

#define GIT_ERROR_CHECK_ALLOC_ADD5(out, one, two, three, four, five) \
	if (GIT_ADD_SIZET_OVERFLOW(out, one, two) || \
		GIT_ADD_SIZET_OVERFLOW(out, *(out), three) || \
		GIT_ADD_SIZET_OVERFLOW(out, *(out), four) || \
		GIT_ADD_SIZET_OVERFLOW(out, *(out), five)) { return -1; }

/** Check for multiplicative overflow, failing if it would occur. */
#define GIT_ERROR_CHECK_ALLOC_MULTIPLY(out, nelem, elsize) \
	if (GIT_MULTIPLY_SIZET_OVERFLOW(out, nelem, elsize)) { return -1; }

/* NOTE: other git_error functions are in the public errors.h header file */

#include "util.h"

#endif
