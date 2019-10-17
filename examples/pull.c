#include <stdio.h>
#include "fetch.h"
#include "common.h"

/** Entry point for this command */
int pull(git_repository *repo, int argc, char **argv)
{
	git_remote *remote;
	git_fetch_options fetch_opts;

	//  git pull [options] [<repository> [<refspec>...]]

//	Default values for <repository> and <branch> are read from the "remote"
//	and "merge" configuration for the current branch as set by git-branch(1)
//		--track.

	// --rebase
	// --ff/--no-ff/--ff-only

	if (argc < 2) {
		fprintf(stderr, "usage: %s pull <repository>\n", argv[-1]);
		return EXIT_FAILURE;
	}

	if (fetch_lookup_remote(&remote, repo, argv[1]) < 0)
		return -1;

	if (git_remote_fetch(remote, NULL, &fetch_opts, "pull") < 0)
		goto on_error;




	git_remote_free(remote);

	return 0;

on_error:
	git_remote_free(remote);

	return -1;
}
