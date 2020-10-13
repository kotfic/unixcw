/*
 * Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
 * Copyright (C) 2011-2019  Kamil Ignacak (acerion@wp.pl)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */




#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>




#include "libcw.h"
#include "libcw2.h"




#include "libcw_debug.h"
#include "libcw_key.h"
#include "libcw_utils.h"
#include "libcw_utils_tests.h"
#include "test_framework.h"




/**
   @reviewed on 2019-10-15
*/
int test_cw_timestamp_compare_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* TODO: I think that there may be more tests to perform for
	   the function, testing handling of overflow. */

	struct {
		struct timeval earlier;
		struct timeval later;
		int expected_delta_usecs;
		bool test_valid;
	} test_data[] = {
		{ { 17, 19 },                         { 17,                          19 },                           0, true  }, /* Two same timestamps. */
		{ { 17, 19 },                         { 17,                          20 },                           1, true  }, /* Simple one microsecond difference. */
		{ { 17, CW_USECS_PER_SEC - 1 },       { 18,                           0 },                           1, true  }, /* Less simple one microsecond difference. */

		{ { 17, CW_USECS_PER_SEC - 1 },       { 17,        CW_USECS_PER_SEC + 1 },                           2, true  }, /* Two microseconds difference with usecs larger than limit. */
		{ { 17, 1 * CW_USECS_PER_SEC },       { 17,        2 * CW_USECS_PER_SEC },        1 * CW_USECS_PER_SEC, true  }, /* One second difference because of count of microseconds. */
		{ { 17, (1 * CW_USECS_PER_SEC) - 1 }, { 17,  (2 * CW_USECS_PER_SEC) + 1 },  (1 * CW_USECS_PER_SEC) + 2, true  }, /* One second and two microseconds difference because of count of microseconds. */

		{ { 0,  0 }, { 0,  0 },  0, false } /* Guard. */
	};

	bool failure = false;
	int i = 0;
	while (test_data[i].test_valid) {
		const int calculated_delta_usecs = LIBCW_TEST_FUT(cw_timestamp_compare_internal)(&test_data[i].earlier, &test_data[i].later);
		if (!cte->expect_op_int_errors_only(cte, test_data[i].expected_delta_usecs, "==", calculated_delta_usecs, "timestamps diff: test #%d", i)) {
			failure = true;
			break;
		}
		i++;
	}

	cte->expect_op_int(cte, false, "==", failure, "timestamps diff");

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-13
*/
int test_cw_timestamp_validate_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);


	/* Test 1 - get current time. */
	{
		/* Get reference time through gettimeofday(). */
		struct timeval ref_timestamp = { 0, 0 }; /* Reference timestamp. */
		cte->assert2(cte, 0 == gettimeofday(&ref_timestamp, NULL), "failed to get reference time");

		/* Get current time through libcw function. */
		struct timeval out_timestamp = { 0, 0 };
		cw_ret_t cwret = LIBCW_TEST_FUT(cw_timestamp_validate_internal)(&out_timestamp, NULL);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "current timestamp");

		/* Check the diff between the two timestamps. On my desktop PC it's ~8us.  */
		const int diff = cw_timestamp_compare_internal(&ref_timestamp, &out_timestamp);
#if 1
		cte->log_info(cte, "delay in getting timestamp is %d microseconds\n", diff);
#endif
		cte->expect_op_int(cte, 100, ">", diff, "delay in getting timestamp");
	}



	struct test_data {
		bool valid;
		struct timeval in;
		int expected_cwret;
		int expected_errno;
		const char * name;
	} test_data[] = {
		{ true,  { 1234,                  987 }, CW_SUCCESS,        0,  "valid"                    }, /* Test 2 - validate valid input timestamp and copy it to output timestamp. */
		{ true,  {   -1,                  987 }, CW_FAILURE,   EINVAL,  "invalid seconds"          }, /* Test 3 - detect invalid seconds in input timestamp. */
		{ true,  {  123, CW_USECS_PER_SEC + 1 }, CW_FAILURE,   EINVAL,  "microseconds too large"   }, /* Test 4 - detect invalid microseconds in input timestamp (microseconds too large). */
		{ true,  {  123,                   -1 }, CW_FAILURE,   EINVAL,  "microseconds negative"    }, /* Test 5 - detect invalid microseconds in input timestamp (microseconds negative). */
		{ false, {    0,                    0 }, CW_SUCCESS,        0,  ""                         }, /* Guard. */
	};

	int i = 0;
	while (test_data[i].valid) {

		struct timeval out = { 0, 0 };
		errno = 0;

		cw_ret_t cwret = LIBCW_TEST_FUT(cw_timestamp_validate_internal)(&out, &test_data[i].in);
		cte->expect_op_int(cte, test_data[i].expected_cwret, "==", cwret, "%s (cwret)", test_data[i].name);
		cte->expect_op_int(cte, test_data[i].expected_errno, "==", errno, "%s (errno)", test_data[i].name);

		if (CW_SUCCESS == test_data[i].expected_cwret) {
			cte->expect_op_int(cte, test_data[i].in.tv_sec, "==", out.tv_sec, "%s (copy sec)", test_data[i].name);
			cte->expect_op_int(cte, test_data[i].in.tv_usec, "==", out.tv_usec, "%s (copy usec)", test_data[i].name);
		}

		i++;
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-13
*/
int test_cw_usecs_to_timespec_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	struct {
		int input;
		struct timespec t;
	} input_data[] = {
		/* input in ms    /   expected output seconds:milliseconds */
		{           0,    {   0,             0 }},
		{     1000000,    {   1,             0 }},
		{     1000004,    {   1,          4000 }},
		{    15000350,    {  15,        350000 }},
		{          73,    {   0,         73000 }},
		{          -1,    {   0,             0 }}, /* Guard. */
	};

	bool seconds_failure = false;
	bool microseconds_failure = false;

	int i = 0;
	while (input_data[i].input != -1) {
		struct timespec result = { .tv_sec = 0, .tv_nsec = 0 };
		LIBCW_TEST_FUT(cw_usecs_to_timespec_internal)(&result, input_data[i].input);
#if 0
		fprintf(stderr, "input = %d usecs, output = %ld.%ld\n",
			input_data[i].input, (long) result.tv_sec, (long) result.tv_nsec);
#endif
		if (!cte->expect_op_int_errors_only(cte, input_data[i].t.tv_sec, "==", result.tv_sec, "test %d: seconds", i)) {
			seconds_failure = true;
			break;
		}
		if (!cte->expect_op_int_errors_only(cte, input_data[i].t.tv_nsec, "==", result.tv_nsec, "test %d: microseconds", i)) {
			microseconds_failure = true;
			break;
		}

		i++;
	}

	cte->expect_op_int(cte, false, "==", seconds_failure, "seconds");
	cte->expect_op_int(cte, false, "==", microseconds_failure, "microseconds");

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-13
*/
int test_cw_version_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	int readback_current = 77;
	int readback_revision = 88;
	int readback_age = 99; /* Dummy initial values. */
	LIBCW_TEST_FUT(cw_get_lib_version)(&readback_current, &readback_revision, &readback_age);

	/* Library's version is defined in LIBCW_VERSION. cw_version()
	   uses three calls to strtol() to get three parts of the
	   library version.

	   Let's use a different approach to convert LIBCW_VERSION
	   into numbers. */

#define VERSION_LEN_MAX 30
	cte->assert2(cte, strlen(LIBCW_VERSION) <= VERSION_LEN_MAX, "LIBCW_VERSION longer than expected!\n");

	char buffer[VERSION_LEN_MAX + 1] = { 0 };
	strncpy(buffer, LIBCW_VERSION, VERSION_LEN_MAX);
	buffer[VERSION_LEN_MAX] = '\0';
#undef VERSION_LEN_MAX

	char *str = buffer;
	int expected_current = 0;
	int expected_revision = 0;
	int expected_age = 0;

	bool tokens_failure = false;

	int i_tokens = 0;
	for (; ; i_tokens++, str = NULL) {

		char * token = strtok(str, ":");
		if (token == NULL) {
			/* We should end tokenizing process after 3 valid tokens, no more and no less. */
			cte->expect_op_int(cte, 3, "==", i_tokens, "stopping at token %d", i_tokens);
			break;
		}

		if (0 == i_tokens) {
			expected_current = atoi(token);
		} else if (1 == i_tokens) {
			expected_revision = atoi(token);
		} else if (2 == i_tokens) {
			expected_age = atoi(token);
		} else {
			tokens_failure = true;
			break;

		}
	}

	cte->expect_op_int(cte, false, "==", tokens_failure, "number of tokens");
	cte->expect_op_int(cte, readback_current, "==", expected_current, "current: %d / %d", readback_current, expected_current);
	cte->expect_op_int(cte, readback_revision, "==", expected_revision, "revision: %d / %d", readback_revision, expected_revision);
	cte->expect_op_int(cte, readback_age, "==", expected_age, "age: %d / %d", readback_age, expected_age);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-13
*/
int test_cw_license_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* Well, there isn't much to test here. The function just
	   prints the license to stdout, and that's it. */

	LIBCW_TEST_FUT(cw_license)();
	cte->expect_op_int(cte, false, "==", false, "libcw license:");

	cte->print_test_footer(cte, __func__);

	return 0;
}
