/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_transaction_h__
#define INCLUDE_transaction_h__

#include "common.h"

#include "pool.h"
#include "git2/transaction.h"

typedef enum {
	GIT_TRANSACTION_NONE,
	GIT_TRANSACTION_REFS,
	GIT_TRANSACTION_CONFIG,
	GIT_TRANSACTION_INDEX,
} git_transaction_type;

typedef int (*git_txn_commit_cb)(git_transaction *tx);
typedef void (*git_txn_free_cb)(git_transaction *tx);

struct git_transaction {
	git_transaction_type type;
	git_pool pool;
	git_txn_commit_cb commit;
	git_txn_free_cb free;
};

git_transaction *git_transaction__alloc(git_transaction_type type, size_t objsize, git_txn_commit_cb commit_cb, git_txn_free_cb free_cb);

#endif
