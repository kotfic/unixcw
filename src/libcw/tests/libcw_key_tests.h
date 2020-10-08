/*
  This file is a part of unixcw project.  unixcw project is covered by
  GNU General Public License, version 2 or later.
*/

#ifndef _LIBCW_KEY_TESTS_H_
#define _LIBCW_KEY_TESTS_H_




#include "test_framework.h"




int test_keyer(cw_test_executor_t * cte);
int test_straight_key(cw_test_executor_t * cte);




typedef struct test_straight_key_data_t {
	/* How many times to alternate between key states? */
	int loops;

	/* For how long to stay in each state? */
	int usecs;

	/* Values of keys, between which the test will alternate. */
	cw_key_value_t values_set[2];

	/* Test name, displayed in console. */
	char test_name[32];

	int (* legacy_set)(int key_state);
	int (* legacy_get)(void);
	bool (* legacy_is_busy)(void);

	cw_ret_t (* modern_set)(volatile cw_key_t * key, cw_key_value_t key_value);
	cw_ret_t (* modern_get)(const volatile cw_key_t * key, cw_key_value_t * key_value);
} test_straight_key_data_t;
#define TEST_STRAIGHT_KEY_DATA_COUNT 4



cwt_retv test_helper_test_straight_key(cw_test_executor_t * cte, volatile cw_key_t * key, test_straight_key_data_t * test_data);




#endif /* #ifndef _LIBCW_KEY_TESTS_H_ */
