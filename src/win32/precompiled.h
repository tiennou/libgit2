#include "common.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <regex.h>

#include <direct.h>
#include <io.h>
#ifdef GIT_THREADS
#include "win32/thread.h"
#endif

#include "git2.h"
