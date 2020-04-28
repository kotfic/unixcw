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
#include <limits.h> /* UCHAR_MAX */
#include <errno.h>
#include <unistd.h>




#include "test_framework.h"

#include "libcw_gen.h"
#include "libcw_gen_tests.h"
#include "libcw_debug.h"
#include "libcw_utils.h"
#include "libcw.h"
#include "libcw2.h"




extern const char * test_valid_representations[];
extern const char * test_invalid_representations[];




static int gen_setup(cw_test_executor_t * cte, cw_gen_t ** gen);
static void gen_destroy(cw_gen_t ** gen);




/**

   \brief Prepare new generator, possibly with parameter values passed through command line

   @reviewed on 2020-04-27
*/
static int gen_setup(cw_test_executor_t * cte, cw_gen_t ** gen)
{
	*gen = cw_gen_new(cte->current_sound_system, NULL);
	if (!*gen) {
		cte->log_error(cte, "Can't create gen, stopping the test\n");
		return -1;
	}

	cw_gen_reset_parameters_internal(*gen);
	cw_gen_sync_parameters_internal(*gen);
	cw_gen_set_speed(*gen, cte->gen_speed);

	return 0;
}




/**
   @reviewed on 2020-04-27
*/
void gen_destroy(cw_gen_t ** gen)
{
	if (NULL != gen) {
		if (NULL != *gen) {
			cw_gen_delete(gen);
		}
	}
}




/**
   @reviewed on 2019-10-08
*/
int test_cw_gen_new_delete(cw_test_executor_t * cte)
{
	const int max = cte->get_repetitions_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, max);

	bool failure = false;
	cw_gen_t * gen = NULL;

	/* new() + delete() */
	for (int i = 0; i < max; i++) {
		gen = LIBCW_TEST_FUT(cw_gen_new)(cte->current_sound_system, NULL);
		if (!cte->expect_valid_pointer_errors_only(cte, gen, "new/delete: failed to initialize generator (loop #%d)", i)) {
			failure = true;
			break;
		}

		/* Try to access some fields in cw_gen_t just to be sure that the gen has been allocated properly. */
		if (!cte->expect_op_int(cte, 0, "==", gen->buffer_sub_start, 1, "new/delete: buffer_sub_start in new generator is not at zero")) {
			failure = true;
			break;
		}

		gen->buffer_sub_stop = gen->buffer_sub_start + 10;
		if (!cte->expect_op_int(cte, 10, "==", gen->buffer_sub_stop, 1, "new/delete: buffer_sub_stop didn't store correct new value")) {
			failure = true;
			break;
		}

		if (!cte->expect_null_pointer_errors_only(cte, gen->client.name, "new/delete: initial value of generator's client name is not NULL")) {
			failure = true;
			break;
		}

		if (!cte->expect_valid_pointer_errors_only(cte, gen->tq, "new/delete: tone queue is NULL")) {
			failure = true;
			break;
		}

		LIBCW_TEST_FUT(cw_gen_delete)(&gen);
		if (!cte->expect_null_pointer_errors_only(cte, gen, "new/delete: delete() didn't set the pointer to NULL (loop #%d)", i)) {
			failure = true;
			break;
		}
	}
	cte->expect_op_int(cte, false, "==", failure, 0, "new/delete");

	/* Clean up after (possibly) failed test. */
	if (gen) {
		cw_gen_delete(&gen);
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-08
*/
int test_cw_gen_new_start_delete(cw_test_executor_t * cte)
{
	const int max = cte->get_repetitions_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, max);

	bool failure = false;
	cw_gen_t * gen = NULL;

	/* new() + start() + delete() (skipping stop() on purpose). */
	for (int i = 0; i < max; i++) {
		gen = LIBCW_TEST_FUT(cw_gen_new)(cte->current_sound_system, NULL);
		if (!cte->expect_valid_pointer_errors_only(cte, gen, "new/start/delete: new (loop #%d)", i)) {
			failure = true;
			break;
		}

		const int cwret = LIBCW_TEST_FUT(cw_gen_start)(gen);
		if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 1, "new/start/delete: start (loop #%d)", i)) {
			failure = true;
			break;
		}

		LIBCW_TEST_FUT(cw_gen_delete)(&gen);
		if (!cte->expect_null_pointer_errors_only(cte, gen, "new/start/delete: delete (loop #%d)", i)) {
			failure = true;
			break;
		}
	}
	cte->expect_op_int(cte, false, "==", failure, 0, "new/start/delete");

	/* Clean up after (possibly) failed test. */
	if (gen) {
		cw_gen_delete(&gen);
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-08
*/
int test_cw_gen_new_stop_delete(cw_test_executor_t * cte)
{
	const int max = cte->get_repetitions_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, max);

	bool new_failure = false;
	bool stop_failure = false;
	bool delete_failure = false;
	cw_gen_t * gen = NULL;

	/* new() + stop() + delete() (skipping start() on purpose). */
	for (int i = 0; i < max; i++) {
		gen = LIBCW_TEST_FUT(cw_gen_new)(cte->current_sound_system, NULL);
		if (!cte->expect_valid_pointer_errors_only(cte, gen, "new/stop/delete: new (loop #%d)", i)) {
			new_failure = true;
			break;
		}

		const int cwret = LIBCW_TEST_FUT(cw_gen_stop)(gen);
		if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 1, "new/stop/delete: stop (loop #%d)", i)) {
			stop_failure = true;
			break;
		}

		LIBCW_TEST_FUT(cw_gen_delete)(&gen);
		if (!cte->expect_null_pointer_errors_only(cte, gen, "new/stop/delete: delete (loop #%d)", i)) {
			delete_failure = true;
			break;
		}
	}
	cte->expect_op_int(cte, false, "==", new_failure, 0, "new/stop/delete: new");
	cte->expect_op_int(cte, false, "==", stop_failure, 0, "new/stop/delete: stop");
	cte->expect_op_int(cte, false, "==", delete_failure, 0, "new/stop/delete: delete");

	/* Clean up after (possibly) failed test. */
	if (gen) {
		cw_gen_delete(&gen);
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-08
*/
int test_cw_gen_new_start_stop_delete(cw_test_executor_t * cte)
{
	const int max = cte->get_repetitions_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, max);

	bool new_failure = false;
	bool start_failure = false;
	bool stop_failure = false;
	bool delete_failure = false;
	cw_gen_t * gen = NULL;

	/* new() + start() + stop() + delete() */
	for (int i = 0; i < max; i++) {
		gen = LIBCW_TEST_FUT(cw_gen_new)(cte->current_sound_system, NULL);
		if (!cte->expect_valid_pointer_errors_only(cte, gen, "new/start/stop/delete: new (loop #%d)", i)) {
			new_failure = true;
			break;
		}

		/* Starting/stopping a generator may be a common pattern. */
		const int sub_max = max;
		for (int j = 0; j < sub_max; j++) {
			int cwret;

			cwret = LIBCW_TEST_FUT(cw_gen_start)(gen);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 1, "new/start/stop/delete: start (loop #%d-%d)", i, j)) {
				start_failure = true;
				break;
			}

			cwret = LIBCW_TEST_FUT(cw_gen_stop)(gen);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 1, "new/start/stop/delete: stop (loop #%d-%d)", i, j)) {
				stop_failure = true;
				break;
			}
		}
		if (start_failure || stop_failure) {
			break;
		}

		LIBCW_TEST_FUT(cw_gen_delete)(&gen);
		if (!cte->expect_null_pointer_errors_only(cte, gen, "new/start/stop/delete: delete (loop #%d)", i)) {
			delete_failure = true;
			break;
		}
	}
	cte->expect_op_int(cte, false, "==", new_failure, 0, "new/start/stop/delete: new");
	cte->expect_op_int(cte, false, "==", start_failure, 0, "new/start/stop/delete: start");
	cte->expect_op_int(cte, false, "==", stop_failure, 0, "new/start/stop/delete: stop");
	cte->expect_op_int(cte, false, "==", delete_failure, 0, "new/start/stop/delete: delete");

	/* Clean up after (possibly) failed test. */
	if (gen) {
		cw_gen_delete(&gen);
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Test setting tone slope shape and length

   @reviewed on 2019-10-09
*/
int test_cw_gen_set_tone_slope(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	const int audio_system = cte->current_sound_system;

	/* Test 0: test property of newly created generator. */
	{
		cw_gen_t * gen = cw_gen_new(audio_system, NULL);
		cte->assert2(cte, gen, "set slope: 0: failed to initialize generator");

		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_RAISED_COSINE, "==", gen->tone_slope.shape, 0, "set slope: 0: initial shape (%d)", gen->tone_slope.shape);
		cte->expect_op_int(cte, CW_AUDIO_SLOPE_LEN, "==", gen->tone_slope.len, 0, "set slope: 0: initial length (%d)", gen->tone_slope.len);

		cw_gen_delete(&gen);
	}



	/* Test A: pass conflicting arguments.

	   "A: If you pass to function conflicting values of \p
	   slope_shape and \p slope_len, the function will return
	   CW_FAILURE. These conflicting values are rectangular slope
	   shape and larger than zero slope length. You just can't
	   have rectangular slopes that have non-zero length." */
	{
		cw_gen_t * gen = cw_gen_new(audio_system, NULL);
		cte->assert2(cte, gen, "set slope: A: failed to initialize generator");

		const int cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_RECTANGULAR, 10);
		cte->expect_op_int(cte, CW_FAILURE, "==", cwret, 0, "set slope: A: conflicting arguments");

		cw_gen_delete(&gen);
	}



	/* Test B: pass '-1' as both arguments.

	   "B: If you pass to function '-1' as value of both \p
	   slope_shape and \p slope_len, the function won't change
	   any of the related two generator's parameters."

	   TODO: add to function description an explicit information
	   that -1/-1 is not an error, and that CW_SUCCESS will be
	   returned.
	*/
	{
		cw_gen_t * gen = cw_gen_new(audio_system, NULL);
		cte->assert2(cte, gen, "set slope: B: failed to initialize generator");

		const int shape_before = gen->tone_slope.shape;
		const int len_before = gen->tone_slope.len;

		const int cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, -1, -1);

		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 0, "set slope: B: set tone slope <-1 -1> (cwret) ");
		cte->expect_op_int(cte, shape_before, "==", gen->tone_slope.shape, 0, "set slope: B: <-1 -1> (shape)");
		cte->expect_op_int(cte, len_before, "==", gen->tone_slope.len, 0, "set slope: B: <-1 -1> (len)");

		cw_gen_delete(&gen);
	}



	/* Test C1

	   "C1: If you pass to function '-1' as value of either \p
	   slope_shape or \p slope_len, the function will attempt to
	   set only this generator's parameter that is different than
	   '-1'." */
	{
		int cwret;
		cw_gen_t * gen = cw_gen_new(audio_system, NULL);
		cte->assert2(cte, gen, "set slope: C1: failed to initialize generator");


		/* At the beginning of test these values are
		   generator's initial values.  As test progresses,
		   some other values will be expected after successful
		   calls to tested function. */
		int expected_shape = CW_TONE_SLOPE_SHAPE_RAISED_COSINE;
		int expected_len = CW_AUDIO_SLOPE_LEN;


		/* At this point generator should have initial values
		   of its parameters (yes, that's test zero again). */
		cte->expect_op_int(cte, expected_shape, "==", gen->tone_slope.shape, 0, "set slope: C1: <x -1>: initial shape");
		cte->expect_op_int(cte, expected_len, "==", gen->tone_slope.len, 0, "set slope: C1: <x -1>: initial length");



		/* Set only new slope shape. */
		expected_shape = CW_TONE_SLOPE_SHAPE_LINEAR;
		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, expected_shape, -1);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 0, "set slope: C1: <x -1>: set");

		/* At this point only slope shape should be updated. */
		cte->expect_op_int(cte, expected_shape, "==", gen->tone_slope.shape, 0, "set slope: C1: <x -1>: get");
		cte->expect_op_int(cte, expected_len, "==", gen->tone_slope.len, 0, "set slope: C1: <x -1>: preserved length");



		/* Set only new slope length. */
		expected_len = 30;
		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, -1, expected_len);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 0, "set slope: C1: <-1 x>: set");

		/* At this point only slope length should be updated
		   (compared to previous function call). */
		cte->expect_op_int(cte, expected_len, "==", gen->tone_slope.len, 0, "set slope: C1: <-1 x>: get");
		cte->expect_op_int(cte, expected_shape, "==", gen->tone_slope.shape, 0, "set slope: C1: <-1 x>: preserved shape");



		cw_gen_delete(&gen);
	}



	/* Test C2

	   "C2: However, if selected slope shape is rectangular,
	   function will set generator's slope length to zero, even if
	   value of \p slope_len is '-1'." */
	{
		cw_gen_t * gen = cw_gen_new(audio_system, NULL);
		cte->assert2(cte, gen, "set slope: C2: failed to initialize generator");

		int cwret;

		/* At the beginning of test these values are
		   generator's initial values.  As test progresses,
		   some other values will be expected after successful
		   calls to tested function. */
		int expected_shape = CW_TONE_SLOPE_SHAPE_RAISED_COSINE;
		int expected_len = CW_AUDIO_SLOPE_LEN;


		/* At this point generator should have initial values
		   of its parameters (yes, that's test zero again). */
		cte->expect_op_int(cte, expected_shape, "==", gen->tone_slope.shape, 0, "set slope: C2: initial shape");
		cte->expect_op_int(cte, expected_len, "==", gen->tone_slope.len, 0, "set slope: C2: initial length");



		/* Set only new slope shape. */
		expected_shape = CW_TONE_SLOPE_SHAPE_RECTANGULAR;
		expected_len = 0; /* Even though we won't pass this to function, this is what we expect to get after this call: we request rectangular slope, which by its nature has zero length. */
		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, expected_shape, -1);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 0, "set slope: C2: set rectangular");



		/* At this point slope shape AND slope length should
		   be updated (slope length is updated only because of
		   requested rectangular slope shape). */
		cte->expect_op_int(cte, expected_shape, "==", gen->tone_slope.shape, 0, "set slope: C2: set rectangular: shape");
		cte->expect_op_int(cte, expected_len, "==", gen->tone_slope.len, 0, "set slope: C2: set rectangular: length");


		cw_gen_delete(&gen);
	}



	/* Test D

	   "D: Notice that the function allows non-rectangular slope
	   shape with zero length of the slopes. The slopes will be
	   non-rectangular, but just unusually short." */
	{
		int cwret;
		cw_gen_t * gen = cw_gen_new(audio_system, NULL);
		cte->assert2(cte, gen, "set slope: D: failed to initialize generator");

		const int expected_len = 0;


		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_LINEAR, expected_len);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 0, "set slope: D: <LINEAR/0>: set");
		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_LINEAR, "==", gen->tone_slope.shape, 0, "set slope: D: <LINEAR/0>: get");
		cte->expect_op_int(cte, expected_len, "==", gen->tone_slope.len, 0, "set slope: D: <LINEAR/0>");


		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_RAISED_COSINE, 0);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 0, "set slope: D: <RAISED_COSINE/0>: set");
		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_RAISED_COSINE, "==", gen->tone_slope.shape, 0, "set slope: D: <RAISED_COSINE/0>: get");
		cte->expect_op_int(cte, expected_len, "==", gen->tone_slope.len, 0, "set slope: D: <RAISED_COSINE/0>");


		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_SINE, 0);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 0, "set slope: D: <SINE/0>: set");
		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_SINE, "==", gen->tone_slope.shape, 0, "set slope: D: <SINE/0>: get");
		cte->expect_op_int(cte, expected_len, "==", gen->tone_slope.len, 0, "set slope: D: <SINE/0>");


		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_RECTANGULAR, 0);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 0, "set slope: D: <RECTANGULAR/0>: set");
		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_RECTANGULAR, "==", gen->tone_slope.shape, 0, "set slope: D: <RECTANGULAR/0>: get");
		cte->expect_op_int(cte, expected_len, "==", gen->tone_slope.len, 0, "set slope: D: <RECTANGULAR/0>");


		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_LINEAR, 0);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 0, "set slope: D: <LINEAR/0>: set");
		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_LINEAR, "==", gen->tone_slope.shape, 0, "set slope: D: <LINEAR/0>: get");
		cte->expect_op_int(cte, expected_len, "==", gen->tone_slope.len, 0, "set slope: D: <LINEAR/0>");


		cw_gen_delete(&gen);
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}





/**
   Test some assertions about CW_TONE_SLOPE_SHAPE_*

   Test code in this file depends on the fact that these values are
   different than -1. I'm testing these values to be sure that when I
   get a silly idea to modify them, the test will catch this
   modification.

   @reviewed on 2019-10-09
*/
int test_cw_gen_tone_slope_shape_enums(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	const bool failure = CW_TONE_SLOPE_SHAPE_LINEAR < 0
		|| CW_TONE_SLOPE_SHAPE_RAISED_COSINE < 0
		|| CW_TONE_SLOPE_SHAPE_SINE < 0
		|| CW_TONE_SLOPE_SHAPE_RECTANGULAR < 0;

	cte->expect_op_int(cte, false, "==", failure, 0, "slope shape enums");

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   It's not a test of a "forever" function, but of "forever"
   functionality.

   @reviewed on 2019-10-26
*/
int test_cw_gen_forever_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	const int seconds = 3;
	cte->log_info(cte, "forever tone (%d seconds):\n", seconds);

	const int rv = test_cw_gen_forever_sub(cte, 2, cte->current_sound_system, (const char *) NULL);
	cte->expect_op_int(cte, 0, "==", rv, 0, "'forever' test");

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-26
*/
int test_cw_gen_forever_sub(cw_test_executor_t * cte, int seconds, int audio_system, const char *audio_device)
{
	cw_gen_t * gen = cw_gen_new(audio_system, audio_device);
	cte->assert2(cte, gen, "ERROR: failed to create generator\n");
	cw_gen_start(gen);

	sleep(1);

	cw_tone_t tone;
	/* Just some acceptable values. */
	int len = 100; /* [us] */
	int freq = 500;

	CW_TONE_INIT(&tone, freq, len, CW_SLOPE_MODE_RISING_SLOPE);
	const int cwret1 = cw_tq_enqueue_internal(gen->tq, &tone);
	cte->expect_op_int(cte, CW_SUCCESS, "==", cwret1, 1, "forever tone: enqueue first tone"); /* Use "errors only" here because this is not a core part of test. */

	CW_TONE_INIT(&tone, freq, gen->quantum_len, CW_SLOPE_MODE_NO_SLOPES);
	tone.is_forever = true;
	const int cwret2 = cw_tq_enqueue_internal(gen->tq, &tone);
	cte->expect_op_int(cte, CW_SUCCESS, "==", cwret2, 0, "forever tone: enqueue forever tone");

#ifdef __FreeBSD__  /* Tested on FreeBSD 10. */
	/* Separate path for FreeBSD because for some reason signals
	   badly interfere with value returned through second arg to
	   nanolseep().  Try to run the section in #else under FreeBSD
	   to see what happens - value returned by nanosleep() through
	   "rem" will be increasing. */
	fprintf(stderr, "enter any character to end \"forever\" tone\n");
	char c;
	scanf("%c", &c);
#else
	struct timespec t;
	cw_usecs_to_timespec_internal(&t, seconds * CW_USECS_PER_SEC);
	cw_nanosleep_internal(&t);
#endif

	/* Silence the generator. */
	CW_TONE_INIT(&tone, 0, len, CW_SLOPE_MODE_FALLING_SLOPE);
	const int cwret3 = cw_tq_enqueue_internal(gen->tq, &tone);
	cte->expect_op_int(cte, CW_SUCCESS, "==", cwret3, 0, "forever tone: silence the generator");

	return 0;
}




/**
   @reviewed on 2019-10-09
*/
int test_cw_gen_get_timing_parameters_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	int initial = -5;

	int dot_len = initial;
	int dash_len = initial;
	int eom_space_len = initial;
	int eoc_space_len = initial;
	int eow_space_len = initial;
	int additional_space_len = initial;
	int adjustment_space_len = initial;

	cw_gen_t * gen = cw_gen_new(cte->current_sound_system, NULL);
	cw_gen_start(gen);


	cw_gen_reset_parameters_internal(gen);
	/* Reset requires resynchronization. */
	cw_gen_sync_parameters_internal(gen);


	LIBCW_TEST_FUT(cw_gen_get_timing_parameters_internal)(gen,
							      &dot_len,
							      &dash_len,
							      &eom_space_len,
							      &eoc_space_len,
							      &eow_space_len,
							      &additional_space_len,
							      &adjustment_space_len);

	bool failure = (dot_len == initial)
		|| (dash_len == initial)
		|| (eom_space_len == initial)
		|| (eoc_space_len == initial)
		|| (eow_space_len == initial)
		|| (additional_space_len == initial)
		|| (adjustment_space_len == initial);
	cte->expect_op_int(cte, false, "==", failure, 0, "get timing parameters");

	cw_gen_delete(&gen);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   \brief Test setting and getting of some basic parameters

   @reviewed on 2019-10-13
*/
int test_cw_gen_parameter_getters_setters(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* No parameter should have value that is larger (for "max"
	   params) or smaller (for "min" params) than this, so this is
	   a good initial value. */
	int off_limits = 10000;

	cw_gen_t * gen = cw_gen_new(cte->current_sound_system, NULL);
	cw_gen_start(gen);

	struct {
		/* There are tree functions that take part in the
		   test: first gets range of acceptable values,
		   seconds sets a new value of parameter, and third
		   reads back the value. */

		void (* get_limits)(int * min, int * max);
		int (* set_new_value)(cw_gen_t * gen, int new_value);
		int (* get_value)(cw_gen_t const * gen);

		const int expected_min;     /* Expected value of minimum. */
		const int expected_max;     /* Expected value of maximum. */

		int readback_min;   /* Value returned by 'get_limits()' function. */
		int readback_max;   /* Value returned by 'get_limits()' function. */

		const char * name;
	} test_data[] = {
		{ LIBCW_TEST_FUT(cw_get_speed_limits),
		  LIBCW_TEST_FUT(cw_gen_set_speed),
		  LIBCW_TEST_FUT(cw_gen_get_speed),         CW_SPEED_MIN,      CW_SPEED_MAX,      off_limits,  -off_limits,  "speed"      },

		{ LIBCW_TEST_FUT(cw_get_frequency_limits),
		  LIBCW_TEST_FUT(cw_gen_set_frequency),
		  LIBCW_TEST_FUT(cw_gen_get_frequency),     CW_FREQUENCY_MIN,  CW_FREQUENCY_MAX,  off_limits,  -off_limits,  "frequency"  },

		{ LIBCW_TEST_FUT(cw_get_volume_limits),
		  LIBCW_TEST_FUT(cw_gen_set_volume),
		  LIBCW_TEST_FUT(cw_gen_get_volume),        CW_VOLUME_MIN,     CW_VOLUME_MAX,     off_limits,  -off_limits,  "volume"     },

		{ LIBCW_TEST_FUT(cw_get_gap_limits),
		  LIBCW_TEST_FUT(cw_gen_set_gap),
		  LIBCW_TEST_FUT(cw_gen_get_gap),           CW_GAP_MIN,        CW_GAP_MAX,        off_limits,  -off_limits,  "gap"        },

		{ LIBCW_TEST_FUT(cw_get_weighting_limits),
		  LIBCW_TEST_FUT(cw_gen_set_weighting),
		  LIBCW_TEST_FUT(cw_gen_get_weighting),     CW_WEIGHTING_MIN,  CW_WEIGHTING_MAX,  off_limits,  -off_limits,  "weighting"  },

		{ NULL,
		  NULL,
		  NULL,                                     0,                 0,                 0,           0,            NULL         }
	};


	bool set_within_range_cwret_failure = false;
	bool set_within_range_errno_failure = false;
	bool set_within_range_readback_failure = false;

	for (int i = 0; test_data[i].get_limits; i++) {

		int value = 0;
		int cwret = CW_FAILURE;

		/* Test getting limits of values to be tested. */
		test_data[i].get_limits(&test_data[i].readback_min, &test_data[i].readback_max);
		if (!cte->expect_op_int(cte, test_data[i].expected_min, "==", test_data[i].readback_min, 0, "get %s limits: min", test_data[i].name)) {
			break;
		}
		if (!cte->expect_op_int(cte, test_data[i].expected_max, "==", test_data[i].readback_max, 0, "get %s limits: max", test_data[i].name)) {
			break;
		}


		/* Test setting out-of-range value lower than minimum. */
		errno = 0;
		value = test_data[i].readback_min - 1;
		cwret = test_data[i].set_new_value(gen, value);
		if (!cte->expect_op_int(cte, CW_FAILURE, "==", cwret, 0, "set %s below limit (cwret)", test_data[i].name)) {
			break;
		}
		if (!cte->expect_op_int(cte, EINVAL, "==", errno, 0, "set %s below limit (errno)", test_data[i].name)) {
			break;
		}


		/* Test setting out-of-range value higher than maximum. */
		errno = 0;
		value = test_data[i].readback_max + 1;
		cwret = test_data[i].set_new_value(gen, value);
		if (!cte->expect_op_int(cte, CW_FAILURE, "==", cwret, 0, "set %s above limit (cwret)", test_data[i].name)) {
			break;
		}
		if (!cte->expect_op_int(cte, EINVAL, "==", errno, 0, "set %s above limit (errno)", test_data[i].name)) {
			break;
		}



		/* Test setting in-range values. Set with setter and then read back with getter. */
		for (int value_to_set = test_data[i].readback_min; value_to_set <= test_data[i].readback_max; value_to_set++) {
			errno = 0;
			cwret = test_data[i].set_new_value(gen, value_to_set);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 1, "set %s within limits (cwret) (value to set = %d)", test_data[i].name, value_to_set)) {
				set_within_range_cwret_failure = true;
				break;
			}
			if (!cte->expect_op_int(cte, 0, "==", errno, 1, "set %s within limits (errno) (value to set = %d)", test_data[i].name, value_to_set)) {
				set_within_range_errno_failure = true;
				break;
			}

			const int readback_value = test_data[i].get_value(gen);
			if (!cte->expect_op_int(cte, readback_value, "==", value_to_set, 1, "readback %s within limits (value to set = %d)", test_data[i].name, value_to_set)) {
				set_within_range_readback_failure = true;
				break;
			}
		}
		cte->expect_op_int(cte, false, "==", set_within_range_cwret_failure, 0, "set %s within range (cwret)", test_data[i].name);
		cte->expect_op_int(cte, false, "==", set_within_range_errno_failure, 0, "set %s above limit (errno)", test_data[i].name);
		cte->expect_op_int(cte, false, "==", set_within_range_readback_failure, 0, "set %s above limit (readback)", test_data[i].name);
	}

	cw_gen_delete(&gen);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   \brief Test control of volume

   Fill tone queue with short tones, then check that we can move the
   volume through its entire range.  Flush the queue when complete.

   @reviewed on 2019-10-10
*/
int test_cw_gen_volume_functions(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	int volume_min = -1;
	int volume_max = -1;

	const int slope_mode = CW_SLOPE_MODE_NO_SLOPES; // CW_SLOPE_MODE_STANDARD_SLOPES;
	const int tone_duration = 100000; /* Duration can't be too short, because the loops will run too fast. */

	cw_gen_t * gen = cw_gen_new(cte->current_sound_system, NULL);


	/* Test: get range of allowed volumes. */
	{
		LIBCW_TEST_FUT(cw_get_volume_limits)(&volume_min, &volume_max);

		const bool failure = volume_min != CW_VOLUME_MIN
			|| volume_max != CW_VOLUME_MAX;

		cte->expect_op_int(cte, false, "==", failure, 0, "get volume limits: %d, %d", volume_min, volume_max);
	}


	const int volume_delta = 1;
	/*
	  There are more tones to be enqueued than there will be loop
	  iterators, because I don't want to run out of tones in queue
	  before I iterate over all volumes. When a queue is emptied
	  too quickly, then cw_gen_wait_for_tone(gen) used in the loop
	  will wait forever.

	  FIXME: should the cw_gen_wait_for_tone(gen) wait forever on
	  empty queue?
	*/
	const int n_enqueued = 3 * (volume_max - volume_min) / volume_delta;


	/* Test: decrease volume from max to low. */
	{
		/* Add a bunch of tones to tone queue. */
		for (int i = 0; i < n_enqueued; i++) {
			cw_tone_t tone;
			CW_TONE_INIT(&tone, 440, tone_duration, slope_mode);
			cw_tq_enqueue_internal(gen->tq, &tone);
		}

		bool set_failure = false;
		bool get_failure = false;

		cw_gen_start(gen);

		for (int vol = volume_max; vol >= volume_min; vol -= volume_delta) {
			const int cwret = LIBCW_TEST_FUT(cw_gen_set_volume)(gen, vol);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 0, "set volume (down, vol = %d)", vol)) {
				set_failure = true;
				break;
			}

			const int readback_value = LIBCW_TEST_FUT(cw_gen_get_volume)(gen);
			if (!cte->expect_op_int(cte, readback_value, "==", vol, 0, "get volume (down, vol = %d)", vol)) {
				get_failure = true;
				break;
			}

			cw_gen_wait_for_tone(gen);
		}

		cw_gen_wait_for_queue_level(gen, 0);
		cw_gen_stop(gen);

		cte->expect_op_int(cte, false, "==", set_failure, 0, "set volume (down)");
		cte->expect_op_int(cte, false, "==", get_failure, 0, "get volume (down)");
	}




	/* Test: increase volume from zero to high. */
	{
		/* Add a bunch of tones to tone queue. */
		for (int i = 0; i < n_enqueued; i++) {
			cw_tone_t tone;
			CW_TONE_INIT(&tone, 440, tone_duration, slope_mode);
			cw_tq_enqueue_internal(gen->tq, &tone);
		}

		bool set_failure = false;
		bool get_failure = false;

		cw_gen_start(gen);

		for (int vol = volume_min; vol <= volume_max; vol += volume_delta) {
			const int cwret = LIBCW_TEST_FUT(cw_gen_set_volume)(gen, vol);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 0, "set volume (up, vol = %d)", vol)) {
				set_failure = true;
				break;
			}

			const int readback_value = LIBCW_TEST_FUT(cw_gen_get_volume)(gen);
			if (!cte->expect_op_int(cte, readback_value, "==", vol, 0, "get volume (up, vol = %d)", vol)) {
				get_failure = true;
				break;
			}
			fprintf(stderr, "len = %zd\n", cw_gen_get_queue_length(gen));

			cw_gen_wait_for_tone(gen);
		}

		cw_gen_wait_for_queue_level(gen, 0);
		cw_gen_stop(gen);

		cte->expect_op_int(cte, false, "==", set_failure, 0, "set volume (up)");
		cte->expect_op_int(cte, false, "==", get_failure, 0, "get volume (up)");
	}

#if 0
	/*
	  FIXME: this is a second call to the function in a row
	  (second was after 'for' loop). This wait hangs the test
	  program, as if the function waited for the queue to go to
	  zero.

	  Calling "cw_gen_wait_for_queue_level(gen, 0)" on an empty
	  queue should return immediately. */
	*/
	cw_gen_wait_for_queue_level(gen, 0);
#endif

	cw_gen_delete(&gen);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   \brief Test enqueueing and playing most basic elements of Morse code

   @reviewed on 2019-10-10
*/
int test_cw_gen_enqueue_primitives(cw_test_executor_t * cte)
{
	const int max = cte->get_repetitions_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, max);

	cw_gen_t * gen = NULL;
	gen_setup(cte, &gen);
	cw_gen_start(gen);

	/* Test: sending dot. */
	{
		bool failure = false;
		for (int i = 0; i < max; i++) {
			const int cwret = LIBCW_TEST_FUT(cw_gen_enqueue_mark_internal)(gen, CW_DOT_REPRESENTATION, false);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 1, "enqueue mark internal(CW_DOT_REPRESENTATION) (i = %d)", i)) {
				failure = true;
				break;
			}
		}
		cw_gen_wait_for_queue_level(gen, 0);

		cte->expect_op_int(cte, false, "==", failure, 0, "enqueue mark internal(CW_DOT_REPRESENTATION)");
	}



	/* Test: sending dash. */
	{
		bool failure = false;
		for (int i = 0; i < max; i++) {
			const int cwret = LIBCW_TEST_FUT(cw_gen_enqueue_mark_internal)(gen, CW_DASH_REPRESENTATION, false);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 1, "enqueue mark internal(CW_DASH_REPRESENTATION) (i = %d)", i)) {
				failure = true;
				break;
			}
		}
		cw_gen_wait_for_queue_level(gen, 0);

		cte->expect_op_int(cte, false, "==", failure, 0, "enqueue mark internal(CW_DASH_REPRESENTATION)");
	}


	/* Test: sending inter-character space. */
	{
		bool failure = false;
		for (int i = 0; i < max; i++) {
			const int cwret = LIBCW_TEST_FUT(cw_gen_enqueue_eoc_space_internal)(gen);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 1, "enqueue eoc space internal() (i = %d)", i)) {
				failure = true;
				break;
			}
		}
		cw_gen_wait_for_queue_level(gen, 0);

		cte->expect_op_int(cte, false, "==", failure, 0, "enqueue eoc space internal()");
	}


	/* Test: sending inter-word space. */
	{
		bool failure = false;
		for (int i = 0; i < max; i++) {
			const int cwret = LIBCW_TEST_FUT(cw_gen_enqueue_eow_space_internal)(gen);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 1, "enqueue eow space internal() (i = %d)", i)) {
				failure = true;
				break;
			}
		}
		cw_gen_wait_for_queue_level(gen, 0);

		cte->expect_op_int(cte, false, "==", failure, 0, "enqueue eow space internal()");
	}

	gen_destroy(&gen);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   \brief Test playing representations of characters

   @reviewed on 2019-10-11
*/
int test_cw_gen_enqueue_representations(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* Representation is valid when it contains dots and dashes
	   only.  cw_gen_enqueue_representation_partial_internal()
	   doesn't care about correct mapping of representation to a
	   character. */

	cw_gen_t * gen = NULL;
	gen_setup(cte, &gen);
	cw_gen_start(gen);

	/* Test: sending valid representations. */
	{
		bool failure = false;
		int i = 0;
		while (NULL != test_valid_representations[i]) {
			const int cwret = LIBCW_TEST_FUT(cw_gen_enqueue_representation_partial_internal)(gen, test_valid_representations[i]);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 1, "enqueue representation internal(<valid>) (%d)", i)) {
				failure = true;
				break;
			}
			i++;
		}
		cw_gen_wait_for_queue_level(gen, 0);
		cte->expect_op_int(cte, false, "==", failure, 0, "enqueue representation internal(<valid>)");
	}


	/* Test: sending invalid representations. */
	{
		bool failure = false;
		int i = 0;
		while (NULL != test_invalid_representations[i]) {
			const int cwret = LIBCW_TEST_FUT(cw_gen_enqueue_representation_partial_internal)(gen, test_invalid_representations[i]);
			if (!cte->expect_op_int(cte, CW_FAILURE, "==", cwret, 1, "enqueue representation internal(<invalid>) (%d)", i)) {
				failure = true;
				break;
			}
			i++;
		}
		cw_gen_wait_for_queue_level(gen, 0);
		cte->expect_op_int(cte, false, "==", failure, 0, "enqueue representation internal(<invalid>)");
	}

#if 0
	struct timespec req = { .tv_sec = 1, .tv_nsec = 0 };
	cw_nanosleep_internal(&req);
#endif
	gen_destroy(&gen);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Send all supported characters as individual characters

   @reviewed on 2019-10-08
*/
int test_cw_gen_enqueue_character(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	cw_gen_t * gen = NULL;
	gen_setup(cte, &gen);
	cw_gen_start(gen);

	/* Test: sending all supported characters as individual characters. */
	{
		char charlist[UCHAR_MAX + 1];
		bool failure = false;

		/* Send all the characters from the charlist individually. */
		cw_list_characters(charlist);
		cte->log_info(cte,
			      "enqueue character(<valid>):\n"
			      "       ");
		for (int i = 0; charlist[i] != '\0'; i++) {
			cte->log_info_cont(cte, "%c", charlist[i]);
			cte->flush_info(cte);
			const int cwret = LIBCW_TEST_FUT(cw_gen_enqueue_character)(gen, charlist[i]);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, 1, "enqueue character(<valid>) (i = %d)", i)) {
				failure = true;
				break;
			}
			cw_gen_wait_for_queue_level(gen, 0);
		}
		cte->log_info_cont(cte, "\n");
		cte->flush_info(cte);

		cte->expect_op_int(cte, false, "==", failure, 0, "enqueue character(<valid>)");
	}


	/* Test: sending invalid character. */
	{
		const char invalid_characters[] = { 0x00, 0x01 }; /* List of invalid characters to be expanded. */
		const int n = sizeof (invalid_characters) / sizeof (invalid_characters[0]);
		bool failure = false;

		for (int i = 0; i < n; i++) {
			const int cwret = LIBCW_TEST_FUT(cw_gen_enqueue_character)(gen, invalid_characters[i]);
			if (!cte->expect_op_int(cte, CW_FAILURE, "==", cwret, 1, "enqueue character(<invalid>) (i = %d)", i)) {
				failure = false;
			}
		}
		cte->expect_op_int(cte, false, "==", failure, 0, "enqueue character(<invalid>)");
	}


	gen_destroy(&gen);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Send all supported characters as a string.

   @reviewed on 2019-10-08
*/
int test_cw_gen_enqueue_string(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	cw_gen_t * gen = NULL;
	gen_setup(cte, &gen);
	cw_gen_start(gen);

	/* Test: sending all supported characters as single string. */
	{
		char charlist[UCHAR_MAX + 1];
		cw_list_characters(charlist);

		/* Send the complete charlist as a single string. */
		cte->log_info(cte,
			      "enqueue string(<valid>):\n"
			      "       %s\n", charlist);
		const int enqueue_cwret = LIBCW_TEST_FUT(cw_gen_enqueue_string)(gen, charlist);
		cte->expect_op_int(cte, CW_SUCCESS, "==", enqueue_cwret, 0, "enqueue string(<valid>)");


		while (cw_gen_get_queue_length(gen) > 0) {
			cte->log_info(cte, "tone queue length %-6zu\r", cw_gen_get_queue_length(gen));
			cte->flush_info(cte);
			cw_gen_wait_for_tone(gen);
		}
		cte->log_info(cte, "tone queue length %-6zu\n", cw_gen_get_queue_length(gen));
		cte->flush_info(cte);
		cw_gen_wait_for_queue_level(gen, 0);
	}


	/* Test: sending invalid string. */
	{
		const char * invalid_strings[] = { "%INVALID%" }; /* List of invalid strings to be expanded. */
		const int n = sizeof (invalid_strings) / sizeof (invalid_strings[0]);
		bool failure = false;

		for (int i = 0; i < n; i++) {
			const int cwret = LIBCW_TEST_FUT(cw_gen_enqueue_string)(gen, invalid_strings[i]);
			if (!cte->expect_op_int(cte, CW_FAILURE, "==", cwret, 1, "enqueue string(<invalid>) (i = %d)", i)) {
				failure = false;
			}
		}
		cte->expect_op_int(cte, false, "==", failure, 0, "enqueue string(<invalid>)");
	}

	gen_destroy(&gen);

	cte->print_test_footer(cte, __func__);

	return 0;
}
