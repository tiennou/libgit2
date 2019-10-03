/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "grafts.h"

#include "filebuf.h"
#include "futils.h"
#include "oidarray.h"
#include "parse.h"

struct git_grafts {
	/* Map of `git_commit_graft`s */
	git_oidmap *commits;

	/* File backing the graft. NULL if it's an in-memory graft */
	char *path;
	git_oid path_checksum;
};

int git_grafts_new(git_grafts **out)
{
	git_grafts *grafts;

	grafts = git__calloc(1, sizeof(*grafts));
	GIT_ERROR_CHECK_ALLOC(grafts);

	if ((git_oidmap_new(&grafts->commits)) < 0) {
		git__free(grafts);
		return -1;
	}

	*out = grafts;
	return 0;
}

int git_grafts_from_file(git_grafts **out, const char *path)
{
	git_grafts *grafts = NULL;
	int error;

	if ((error = git_grafts_new(&grafts)) < 0)
		goto error;
	grafts->path = git__strdup(path);
	if ((error = git_grafts_refresh(grafts)) < 0)
		goto error;

	*out = grafts;
error:
	if (error < 0)
		git_grafts_free(grafts);
	return error;
}

void git_grafts_free(git_grafts *grafts)
{
	if (!grafts)
		return;
	git__free(grafts->path);
	git_grafts_clear(grafts);
	git_oidmap_free(grafts->commits);
	git__free(grafts);
}

void git_grafts_clear(git_grafts *grafts)
{
	git_commit_graft *graft;

	assert(grafts);

	git_oidmap_foreach_value(grafts->commits, graft, {
		git__free(graft->parents.ptr);
		git__free(graft);
	});

	git_oidmap_clear(grafts->commits);
}

int git_grafts_refresh(git_grafts *grafts)
{
	git_buf contents = GIT_BUF_INIT;
	int error, updated = 0;

	if (!grafts->path)
		return 0;

	error = git_futils_readbuffer_updated(&contents, grafts->path,
					      &grafts->path_checksum, &updated);
	if (error < 0 || error == GIT_ENOTFOUND || !updated) {
		if (error == GIT_ENOTFOUND)
			error = 0;
		goto cleanup;
	}

	if ((error = git_grafts_parse(grafts, contents.ptr, contents.size)) < 0)
		goto cleanup;

cleanup:
	git_buf_dispose(&contents);
	return error;
}

int git_grafts_write_(git_grafts *grafts, const char *path)
{
	git_filebuf file = GIT_FILEBUF_INIT;
	git_buf path_buf = GIT_BUF_INIT;
	int error = 0;
	size_t idx;
	const git_oid *oid;
	git_commit_graft *graft;

	assert(grafts && path);

	if ((error = git_buf_sets(&path_buf, path)) < 0)
		return error;

	if ((error = git_filebuf_open(&file, git_buf_cstr(&path_buf), GIT_FILEBUF_HASH_CONTENTS, 0666)) < 0)
		return error;

	while ((error = git_oidmap_iterate((void *)&graft, grafts->commits, &idx, &oid)) == 0) {
		git_filebuf_write(&file, git_oid_tostr_s(oid), GIT_OID_HEXSZ);
		git_filebuf_write(&file, "\n", 1);
	}

	git_filebuf_commit(&file);

	if (git_grafts_refresh(grafts))
		return -1;

	return 0;
}

int git_grafts_write(git_grafts *grafts)
{
	assert(grafts);

	if (!grafts->path)
		return -1;

	return git_grafts_write_(grafts, grafts->path);
}

int git_grafts_parse(git_grafts *grafts, const char *content, size_t contentlen)
{
	git_array_oid_t parents = GIT_ARRAY_INIT;
	git_parse_ctx parser;
	int error;

	git_grafts_clear(grafts);

	if ((error = git_parse_ctx_init(&parser, content, contentlen)) < 0)
		goto error;

	for (; parser.remain_len; git_parse_advance_line(&parser)) {
		const char *line_start = parser.line, *line_end = parser.line + parser.line_len;
		git_oid graft_oid;

		if ((error = git_oid_fromstrn(&graft_oid, line_start, GIT_OID_HEXSZ)) < 0) {
			git_error_set(GIT_ERROR_GRAFTS, "invalid graft OID at line %" PRIuZ, parser.line_num);
			goto error;
		}
		line_start += GIT_OID_HEXSZ;

		while (line_start < line_end && *line_start == ' ') {
			git_oid *id = git_array_alloc(parents);
			GIT_ERROR_CHECK_ALLOC(id);

			if ((error = git_oid_fromstrn(id, ++line_start, GIT_OID_HEXSZ)) < 0) {
				git_error_set(GIT_ERROR_GRAFTS, "invalid parent OID at line %" PRIuZ, parser.line_num);
				goto error;
			}

			line_start += GIT_OID_HEXSZ;
		}

		if ((error = git_grafts_add(grafts, &graft_oid, parents)) < 0)
			goto error;

		git_array_clear(parents);
	}

error:
	git_array_clear(parents);
	return error;
}

int git_grafts_add(git_grafts *grafts, const git_oid *oid, git_array_oid_t parents)
{
	git_commit_graft *graft;
	git_oid *parent_oid;
	int error;
	size_t i;

	assert(grafts && oid);

	graft = git__calloc(1, sizeof(*graft));
	GIT_ERROR_CHECK_ALLOC(graft);

	git_array_init_to_size(graft->parents, git_array_size(parents));
	git_array_foreach(parents, i, parent_oid) {
		git_oid *id = git_array_alloc(graft->parents);
		GIT_ERROR_CHECK_ALLOC(id);

		git_oid_cpy(id, parent_oid);
	}
	git_oid_cpy(&graft->oid, oid);

	if ((error = git_oidmap_set(grafts->commits, &graft->oid, graft)) < 0)
		goto cleanup;

	return 0;

cleanup:
	git_array_clear(graft->parents);
	git__free(graft);
	return error;
}

int git_grafts_add_shallow(git_grafts *grafts, const git_oid *oid)
{
	git_array_oid_t tmp = GIT_ARRAY_INIT;
	return git_grafts_add(grafts, oid, tmp);
}

int git_grafts_remove(git_grafts *grafts, const git_oid *oid)
{
	git_commit_graft *graft;
	int error;

	assert(grafts && oid);

	if ((graft = git_oidmap_get(grafts->commits, oid)) == NULL)
		return GIT_ENOTFOUND;

	if ((error = git_oidmap_delete(grafts->commits, oid)) < 0)
		return error;

	git__free(graft);

	return 0;
}


int git_grafts_geti(const git_commit_graft *out, git_grafts *grafts, size_t *idx)
{
	const git_oid *oid;

	assert(grafts && idx);

	if (*idx >= 0 && *idx < git_oidmap_size(grafts->commits))
		return GIT_ERROR;

	return git_oidmap_iterate((void **)&out, grafts->commits, idx, &oid);
}

int git_grafts_get(git_commit_graft **out, git_grafts *grafts, const git_oid *oid)
{
	assert(out && grafts && oid);
	if ((*out = git_oidmap_get(grafts->commits, oid)) == NULL)
		return GIT_ENOTFOUND;
	return 0;
}

int git_grafts_get_array_oid(git_array_oid_t *out, git_grafts *grafts)
{
	const git_oid *oid;
	size_t i = 0;
	int error;

	assert(out && grafts);

	while ((error = git_oidmap_iterate(NULL, grafts->commits, &i, &oid)) == 0) {
		git_oid *cpy = git_array_alloc(*out);
		GIT_ERROR_CHECK_ALLOC(cpy);
		git_oid_cpy(cpy, oid);
	}

	return 0;
}

int git_grafts_get_oids(git_oidarray *out, git_grafts *grafts)
{
	git_array_oid_t oids = GIT_ARRAY_INIT;
	int error;

	if((error = git_grafts_get_array_oid(&oids, grafts)) < 0)
		return error;

	git_oidarray__from_array(out, &oids);

	return 0;
}

size_t git_grafts_size(git_grafts *grafts)
{
	return git_oidmap_size(grafts->commits);
}
