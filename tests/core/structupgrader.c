#include "clar_libgit2.h"

/** Option Structure Upgrading Scaffolding */

typedef struct {
	unsigned int version;

	const char *name;
} test_struct_v1;

#define V1_NAME_DEFAULT "default"
#define V1_OPTIONS_INIT { 1, V1_NAME_DEFAULT }

typedef struct {
	unsigned int version;

	const char *name;

	unsigned char blob;
} test_struct_v2;

#define V2_NAME_DEFAULT "new-default"
#define V2_OPTIONS_INIT { 2, V2_NAME_DEFAULT, 'c' }

typedef struct {
	unsigned int version;

	unsigned char blob;
} test_struct_v3;

#define V3_OPTIONS_INIT { 3, 'b' }

struct test_upgrade__data {
	/* test expectations */
	unsigned int current_version; /* 0 for NULL */
	unsigned int max_version;
	int upgrade_result;

	/* runtime details */
	git_struct__version *default_struct;
	size_t struct_size;
	const char *struct_name;

	git_struct__upgrade_cb upgrader;
	size_t upgrade_counter;
};

static struct test_upgrade__data *test_options__current = NULL;

/** Low-level test upgrader */

static int test_options__upgrader(git_struct__version *upgraded, const git_struct__version *current, const char *name)
{
	cl_assert(upgraded && current && name);

	cl_assert_(test_options__current->upgrade_counter != 0, "unexpected upgrade request");
	test_options__current->upgrade_counter--;

	if (test_options__current->current_version != 0) {
		cl_assert(current != NULL);
		cl_assert_equal_i(test_options__current->current_version, current->version);
		cl_assert_(current->version < upgraded->version, "invalid upgrade");
	}

	return test_options__current->upgrader(upgraded, current, name);
}

static int test_options__upgrade(const void *user_opts)
{
	int result;

	assert(test_options__current != NULL);
	assert(test_options__current->upgrade_counter == 0 || test_options__current->upgrader);

	cl_assert_equal_i(test_options__current->max_version,
					  test_options__current->default_struct->version);

	/* precondition checks done, perform the upgrade */
	result = git_struct__upgrade(test_options__current->default_struct,
								 test_options__current->struct_size,
								 user_opts,
								 test_options__current->struct_name,
								 test_options__upgrader);

	cl_assert_equal_i_(0, test_options__current->upgrade_counter, "unexpected upgrades left");

	return result;
}

static void test_options_data__init(struct test_upgrade__data *data, git_struct__version *struct_var, size_t size, const char *name)
{
	memset(data, 0, sizeof(*data));

	test_options__current = data;

	data->default_struct = struct_var;
	data->struct_size = size;
	data->struct_name = name;
}

#define TEST_OPTIONS_DECLARE_DATA(data, struct_var, name) \
	struct test_upgrade__data data; \
	test_options_data__init(&data, (git_struct__version *)&struct_var, sizeof(struct_var), name);

/** Upgrade testing scaffolding */

/* … */

/** Actual test suite */

void test_core_structupgrader__initialize(void)
{
}

void test_core_structupgrader__cleanup(void)
{
	assert(test_options__current != NULL);
	test_options__current = NULL;
}

void test_core_structupgrader__upgrade_options_v1_default(void)
{
	test_struct_v1 defaults = V1_OPTIONS_INIT;

	TEST_OPTIONS_DECLARE_DATA(data, defaults, "test_struct_v1");

	data.max_version = 1;

	test_options__upgrade(NULL);

	defaults = *(test_struct_v1 *)data.default_struct;
	cl_assert_equal_s(V1_NAME_DEFAULT, defaults.name);
}

void test_core_structupgrader__upgrade_options_v1_normal(void)
{
	test_struct_v1 defaults = V1_OPTIONS_INIT;
	test_struct_v1 opts = V1_OPTIONS_INIT;

	TEST_OPTIONS_DECLARE_DATA(data, defaults, "test_struct_v1");

	data.current_version = 1;
	data.max_version = 1;

	opts.name = "borg";

	cl_git_pass(test_options__upgrade(&opts));

	defaults = *(test_struct_v1 *)data.default_struct;
	cl_assert_equal_s("borg", defaults.name);
}

static int upgrade_options_v1_to_v2(test_struct_v2 *upgraded, const test_struct_v1 *current, const char *name)
{
	cl_assert(upgraded && current);
	cl_assert_equal_i(1, current->version);
//	cl_assert_equal_s("test_struct_v2", name);
	GIT_UNUSED(name);

	/* make sure we only upgrade non-default values */
	if (strcmp(V1_NAME_DEFAULT, current->name) != 0)
		upgraded->name = current->name;

	return 0;
}

static int upgrade_options_v1_to_v2_failing(test_struct_v2 *upgraded, const test_struct_v1 *current, const char *name)
{
	cl_assert(upgraded && current);
	cl_assert_equal_i(1, current->version);
	cl_assert_equal_s("test_struct_v2", name);

	git_error_set(GIT_ERROR_NET, "failed to upgrade struct");
	return -1;
}

#define GIT_STRUCT_UPGRADE_CALLBACK(name) \
	(int (*)(git_struct__version *, const git_struct__version *, const char *))name

void test_core_structupgrader__upgrade_options_v2_default(void)
{
	test_struct_v2 defaults = V2_OPTIONS_INIT;

	TEST_OPTIONS_DECLARE_DATA(data, defaults, "test_struct_v2");

	data.current_version = 0;
	data.max_version = 2;

	data.upgrader = GIT_STRUCT_UPGRADE_CALLBACK(upgrade_options_v1_to_v2);

	cl_git_pass(test_options__upgrade(NULL));
}

void test_core_structupgrader__upgrade_options_v2_normal(void)
{
	test_struct_v2 defaults = V2_OPTIONS_INIT;
	test_struct_v2 opts = V2_OPTIONS_INIT;

	TEST_OPTIONS_DECLARE_DATA(data, defaults, "test_struct_v2");

	data.current_version = 2;
	data.max_version = 2;

	cl_git_pass(test_options__upgrade(&opts));
}

void test_core_structupgrader__upgrade_options_v2_upgrade_from_v1(void)
{
	test_struct_v2 defaults = V2_OPTIONS_INIT;
	test_struct_v2 opts = V1_OPTIONS_INIT;

	TEST_OPTIONS_DECLARE_DATA(data, defaults, "test_struct_v2");

	data.current_version = 1;
	data.max_version = 2;

	data.upgrader = GIT_STRUCT_UPGRADE_CALLBACK(upgrade_options_v1_to_v2);
	data.upgrade_counter = 1;

	cl_git_fail_with(1, test_options__upgrade(&opts));
}

void test_core_structupgrader__upgrade_options_v2_upgrade_from_v1_failing(void)
{
	test_struct_v2 defaults = V2_OPTIONS_INIT;
	test_struct_v2 opts = V1_OPTIONS_INIT;

	TEST_OPTIONS_DECLARE_DATA(data, defaults, "test_struct_v2");

	data.current_version = 1;
	data.max_version = 2;

	data.upgrader = GIT_STRUCT_UPGRADE_CALLBACK(upgrade_options_v1_to_v2_failing);
	data.upgrade_counter = 1;

	cl_git_fail_with(-1, test_options__upgrade(&opts));
	cl_assert_equal_s("failed to upgrade older version 1 of struct test_struct_v2", git_error_last()->message);
}

static int upgrade_options_v1_to_v2_to_v3(test_struct_v3 *upgraded, const test_struct_v1 *current, const char *name)
{
	test_struct_v2 opts_v2;
	int error;

	cl_assert(current->version == 1 || current->version == 2);
	cl_assert_equal_s("test_struct_v3", name);

	if (current->version == 1 &&
		(error = upgrade_options_v1_to_v2(&opts_v2, current, name)) < 0)
		return error;

	/* name was removed in v3 */
	upgraded->blob = opts_v2.blob;

	return 0;
}

void test_core_structupgrader__upgrade_options_v3_default(void)
{
	test_struct_v3 defaults = V3_OPTIONS_INIT;

	TEST_OPTIONS_DECLARE_DATA(data, defaults, "test_struct_v3");

	data.current_version = 0;
	data.max_version = 3;

	cl_git_pass(test_options__upgrade(NULL));
}

void test_core_structupgrader__upgrade_options_v3_normal(void)
{
	test_struct_v3 defaults = V3_OPTIONS_INIT;
	test_struct_v3 opts = V3_OPTIONS_INIT;

	TEST_OPTIONS_DECLARE_DATA(data, defaults, "test_struct_v3");

	data.current_version = 3;
	data.max_version = 3;

	cl_git_pass(test_options__upgrade(&opts));
}

void test_core_structupgrader__upgrade_options_v3_from_v1(void)
{
	test_struct_v3 defaults = V3_OPTIONS_INIT;
	test_struct_v1 opts = V1_OPTIONS_INIT;

	TEST_OPTIONS_DECLARE_DATA(data, defaults, "test_struct_v3");

	data.current_version = 1;
	data.max_version = 3;

	data.upgrader = GIT_STRUCT_UPGRADE_CALLBACK(upgrade_options_v1_to_v2_to_v3);
	data.upgrade_counter = 1;

	cl_git_fail_with(1, test_options__upgrade(&opts));
}

void test_core_structupgrader__upgrade_options_v3_from_v2(void)
{
	test_struct_v3 defaults = V3_OPTIONS_INIT;
	test_struct_v2 opts = V2_OPTIONS_INIT;

	TEST_OPTIONS_DECLARE_DATA(data, defaults, "test_struct_v3");

	data.current_version = 2;
	data.max_version = 3;

	data.upgrader = GIT_STRUCT_UPGRADE_CALLBACK(upgrade_options_v1_to_v2_to_v3);
	data.upgrade_counter = 1;

	cl_git_fail_with(1, test_options__upgrade(&opts));
}
