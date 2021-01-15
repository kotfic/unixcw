/*
 * Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
 * Copyright (C) 2011-2021  Kamil Ignacak (acerion@wp.pl)
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




#include "libcw.h"
#include "libcw2.h"




#include "libcw_gen.h"
#include "libcw_gen_tests.h"
#include "libcw_debug.h"
#include "libcw_utils.h"
#include "test_framework.h"




extern const char * test_valid_representations[];
extern const char * test_invalid_representations[];
extern const char * test_invalid_strings[];




static cwt_retv gen_setup(cw_test_executor_t * cte, cw_gen_t ** gen);
static void gen_destroy(cw_gen_t ** gen);
static cwt_retv test_cw_gen_new_start_stop_delete_sub(cw_test_executor_t * cte, const char * function_name, bool do_new, bool do_start, bool do_stop, bool do_delete);
static cwt_retv test_cw_gen_forever_sub(cw_test_executor_t * cte, int seconds);




/**
   @brief Prepare new generator, possibly with parameter values passed through command line

   Test helper function.

   @reviewed on 2020-05-07

   @return cwt_retv_ok on success
   @return cwt_retv_err on failure
*/
static cwt_retv gen_setup(cw_test_executor_t * cte, cw_gen_t ** gen)
{
	*gen = cw_gen_new(&cte->current_gen_conf);
	if (!*gen) {
		cte->log_error(cte, "Can't create generator, stopping the test\n");
		return cwt_retv_err;
	}

	cw_gen_reset_parameters_internal(*gen);
	cw_gen_sync_parameters_internal(*gen);
	cw_gen_set_speed(*gen, cte->config->send_speed);
	cw_gen_set_frequency(*gen, cte->config->frequency);

	return cwt_retv_ok;
}




/**
   @brief Delete @param gen, set the pointer to NULL

   Test helper function.

   @reviewed on 2020-05-07
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
   @brief Test creating and deleting a generator (without trying to
   start or stop it)

   @reviewed on 2020-05-07
*/
cwt_retv test_cw_gen_new_delete(cw_test_executor_t * cte)
{
	return test_cw_gen_new_start_stop_delete_sub(cte, __func__, true, false, false, true);
}




/**
   @brief Test creating, starting and deleting a generator (without
   trying to stop it)

   @reviewed on 2020-05-07
*/
cwt_retv test_cw_gen_new_start_delete(cw_test_executor_t * cte)
{
	return test_cw_gen_new_start_stop_delete_sub(cte, __func__, true, true, false, true);
}




/**
   @brief Test creating, stopping and deleting a generator (without
   trying to start it)

   @reviewed on 2020-05-07
*/
cwt_retv test_cw_gen_new_stop_delete(cw_test_executor_t * cte)
{
	return test_cw_gen_new_start_stop_delete_sub(cte, __func__, true, false, true, true);
}




/**
   @brief Test creating, starting, stopping and deleting a generator

   @reviewed on 2020-05-07
*/
cwt_retv test_cw_gen_new_start_stop_delete(cw_test_executor_t * cte)
{
	return test_cw_gen_new_start_stop_delete_sub(cte, __func__, true, true, true, true);
}




/**
   @brief Test creating, starting, stopping and deleting a generator -
   test helper function

   The operations (creating, starting, stopping and deleting) are
   executed depending on value of corresponding argument of the
   function.

   @param function_name is name of function that called this subroutine

   @reviewed on 2020-05-07
*/
static cwt_retv test_cw_gen_new_start_stop_delete_sub(cw_test_executor_t * cte, const char * function_name, bool do_new, bool do_start, bool do_stop, bool do_delete)
{
	const int loops = cte->get_loops_count(cte);
	cte->print_test_header(cte, "%s (%d)", function_name, loops);

	bool new_failure = false;
	bool start_failure = false;
	bool stop_failure = false;
	bool delete_failure = false;
	cw_gen_t * gen = NULL;

	for (int i = 0; i < loops; i++) {
		cte->log_info(cte, "%s", "");
		if (do_new) {
			cte->log_info_cont(cte, "new ");
			gen = LIBCW_TEST_FUT(cw_gen_new)(&cte->current_gen_conf);
			if (!cte->expect_valid_pointer_errors_only(cte, gen, "new() (loop #%d/%d)", i + 1, loops)) {
				new_failure = true;
				break;
			}


			/* Try to access some fields in cw_gen_t just to be sure that the gen has been allocated properly. */
			if (!cte->expect_op_int_errors_only(cte, 0, "==", gen->buffer_sub_start, "buffer_sub_start in new generator is not at zero")) {
				new_failure = true;
				break;
			}
			gen->buffer_sub_stop = gen->buffer_sub_start + 10;
			if (!cte->expect_op_int_errors_only(cte, 10, "==", gen->buffer_sub_stop, "buffer_sub_stop didn't store correct new value")) {
				new_failure = true;
				break;
			}
			if (!cte->expect_null_pointer_errors_only(cte, gen->library_client.name, "initial value of generator's client name is not NULL")) {
				new_failure = true;
				break;
			}
			if (!cte->expect_valid_pointer_errors_only(cte, gen->tq, "tone queue is NULL")) {
				new_failure = true;
				break;
			}
		}



		if (do_start || do_stop) {
			if (do_start) {
				cte->log_info_cont(cte, "start ");
			}
			if (do_stop) {
				cte->log_info_cont(cte, "stop ");
			}
			/* I expect that a common pattern will be that
			   generator will be created once, then
			   started/stopped multiple times, and then deleted
			   once. So do start/stop in inner loop here. */
			int loops_inner = loops;

			if (do_start && (!do_stop)) {
				/* FIXME: there is a problem:

				   if client code will call cw_gen_start() multiple times,
				   without calling cw_gen_stop() after each start(), then we will have a
				   problem: the final delete() call in this test function (which internally
				   calls stop()) will have problem with stopping a generator thread with
				   "pthread_join(gen->thread.id, NULL);" because there were several
				   start() calls and several threads were created in single
				   generator. So for now, if this subroutine was called to do several
				   starts but no stops, limit number of inner loops.

				   The solution can be that start() function looks at state of thread,
				   and doesn't create new one if a generator thread is already running.
				*/
				loops_inner = 1;
			}

			for (int j = 0; j < loops_inner; j++) {
				if (do_start) {
					const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_start)(gen);
					if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "start() (loop #%d/%d - %d/%d)", i + 1, loops, j + 1, loops_inner)) {
						start_failure = true;
						break;
					}
				}

				if (do_stop) {
					const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_stop)(gen);
					if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "stop() (loop #%d/%d - %d/%d)", i + 1, loops_inner, j + 1, loops_inner)) {
						stop_failure = true;
						break;
					}
				}
			}
			if (start_failure || stop_failure) {
				break;
			}
		}

		if (do_delete) {
			cte->log_info_cont(cte, "delete ");
			LIBCW_TEST_FUT(cw_gen_delete)(&gen);
			if (!cte->expect_null_pointer_errors_only(cte, gen, "delete() (loop #%d/%d)", i + 1, loops)) {
				delete_failure = true;
				break;
			}
		}
		cte->log_info_cont(cte, "\n");
	}
	if (do_new) {
		cte->expect_op_int(cte, false, "==", new_failure, "%s(): new()", function_name);
	}
	if (do_start) {
		cte->expect_op_int(cte, false, "==", start_failure, "%s(): start()", function_name);
	}
	if (do_stop) {
		cte->expect_op_int(cte, false, "==", stop_failure, "%s(): stop()", function_name);
	}
	if (do_delete) {
		cte->expect_op_int(cte, false, "==", delete_failure, "%s(): delete()", function_name);
	}

	/* If test fails to delete generator, do it here. */
	if (gen) {
		cw_gen_delete(&gen);
	}

	cte->print_test_footer(cte, function_name);

	return cwt_retv_ok;
}




/**
   @brief Test setting tone slope shape and duration

   @reviewed on 2020-05-08
*/
cwt_retv test_cw_gen_set_tone_slope(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* Test 0: test property of newly created generator. */
	{
		cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);
		cte->assert2(cte, gen, "test 0: failed to create generator");

		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_RAISED_COSINE, "==", gen->tone_slope.shape, "test 0: initial slope shape (%d)", gen->tone_slope.shape);
		cte->expect_op_int(cte, CW_AUDIO_SLOPE_DURATION, "==", gen->tone_slope.duration,        "test 0: initial slope duration (%d)", gen->tone_slope.duration);

		cw_gen_delete(&gen);
	}



	/* Test A: pass conflicting arguments.

	   "A: If you pass to function conflicting values of \p
	   slope_shape and \p slope_duration, the function will return
	   CW_FAILURE. These conflicting values are rectangular slope
	   shape and larger than zero slope duration. You just can't
	   have rectangular slopes that have non-zero duration." */
	{
		cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);
		cte->assert2(cte, gen, "test A: failed to create generator");

		const int slope_duration = 10;
		const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_RECTANGULAR, slope_duration);
		cte->expect_op_int(cte, CW_FAILURE, "==", cwret, "test A: conflicting arguments");

		cw_gen_delete(&gen);
	}



	/* Test B: pass '-1' as both arguments.

	   "B: If you pass to function '-1' as value of both \p
	   slope_shape and \p slope_duration, the function won't change
	   any of the related two generator's parameters."

	   TODO: add to function description an explicit information
	   that -1/-1 is not an error, and that CW_SUCCESS will be
	   returned.
	*/
	{
		cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);
		cte->assert2(cte, gen, "test B: failed to create generator");

		const int shape_before = gen->tone_slope.shape;
		const int duration_before = gen->tone_slope.duration;

		const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, -1, -1);

		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret,                         "test B: set tone slope <-1 -1> (cwret) ");
		cte->expect_op_int(cte, shape_before, "==", gen->tone_slope.shape,       "test B: <-1 -1> (slope shape)");
		cte->expect_op_int(cte, duration_before, "==", gen->tone_slope.duration, "test B: <-1 -1> (slope duration)");

		cw_gen_delete(&gen);
	}



	/* Test C1

	   "C1: If you pass to function '-1' as value of either \p
	   slope_shape or \p slope_duration, the function will attempt to
	   set only this generator's parameter that is different than
	   '-1'." */
	{
		cw_ret_t cwret;
		cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);
		cte->assert2(cte, gen, "test C1: failed to create generator");


		/* At the beginning of test these values are
		   generator's initial values.  As test progresses,
		   some other values will be expected after successful
		   calls to tested function. */
		int expected_shape = CW_TONE_SLOPE_SHAPE_RAISED_COSINE;
		int expected_duration = CW_AUDIO_SLOPE_DURATION;


		/* At this point generator should have initial values
		   of its parameters (yes, that's test zero again). */
		cte->expect_op_int(cte, expected_shape, "==", gen->tone_slope.shape,       "test C1: <x -1>: initial slope shape");
		cte->expect_op_int(cte, expected_duration, "==", gen->tone_slope.duration, "test C1: <x -1>: initial slope duration");



		/* Set only new slope shape. */
		expected_shape = CW_TONE_SLOPE_SHAPE_LINEAR;
		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, expected_shape, -1);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "test C1: <x -1>: set");

		/* At this point only slope shape should be updated. */
		cte->expect_op_int(cte, expected_shape, "==", gen->tone_slope.shape,       "test C1: <x -1>: get");
		cte->expect_op_int(cte, expected_duration, "==", gen->tone_slope.duration, "test C1: <x -1>: preserved slope duration");



		/* Set only new slope duration. */
		expected_duration = 30;
		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, -1, expected_duration);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "set slope: C1: <-1 x>: set");

		/* At this point only slope duration should be updated
		   (compared to previous function call). */
		cte->expect_op_int(cte, expected_duration, "==", gen->tone_slope.duration, "test C1: <-1 x>: get");
		cte->expect_op_int(cte, expected_shape, "==", gen->tone_slope.shape,       "test C1: <-1 x>: preserved slope shape");



		cw_gen_delete(&gen);
	}



	/* Test C2

	   "C2: However, if selected slope shape is rectangular,
	   function will set generator's slope duration to zero, even if
	   value of \p slope_duration is '-1'." */
	{
		cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);
		cte->assert2(cte, gen, "test C2: failed to create generator");

		cw_ret_t cwret;

		/* At the beginning of test these values are
		   generator's initial values.  As test progresses,
		   some other values will be expected after successful
		   calls to tested function. */
		int expected_shape = CW_TONE_SLOPE_SHAPE_RAISED_COSINE;
		int expected_duration = CW_AUDIO_SLOPE_DURATION;


		/* At this point generator should have initial values
		   of its parameters (yes, that's test zero again). */
		cte->expect_op_int(cte, expected_shape, "==", gen->tone_slope.shape,       "test C2: initial slope shape");
		cte->expect_op_int(cte, expected_duration, "==", gen->tone_slope.duration, "test C2: initial slope duration");



		/* Set only new slope shape. */
		expected_shape = CW_TONE_SLOPE_SHAPE_RECTANGULAR;
		expected_duration = 0; /* Even though we won't pass this to function, this is what we expect to get after this call: we request rectangular slope, which by its nature has zero duration. */
		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, expected_shape, -1);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "test: C2: set rectangular");



		/* At this point slope shape AND slope duration should
		   be updated (slope duration is updated only because of
		   requested rectangular slope shape). */
		cte->expect_op_int(cte, expected_shape, "==", gen->tone_slope.shape,       "test C2: set rectangular: slope shape");
		cte->expect_op_int(cte, expected_duration, "==", gen->tone_slope.duration, "test C2: set rectangular: slope duration");


		cw_gen_delete(&gen);
	}



	/* Test D

	   "D: Notice that the function allows non-rectangular slope
	   shape with zero duration of the slopes. The slopes will be
	   non-rectangular, but just unusually short." */
	{
		cw_ret_t cwret;
		cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);
		cte->assert2(cte, gen, "test D: failed to create generator");

		const int expected_duration = 0;
		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_LINEAR, expected_duration);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret,                                 "test D: <LINEAR/0>: cwret");
		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_LINEAR, "==", gen->tone_slope.shape, "test D: <LINEAR/0>: slope shape");
		cte->expect_op_int(cte, expected_duration, "==", gen->tone_slope.duration,       "test D: <LINEAR/0>: slope duration");


		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_RAISED_COSINE, 0);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret,                                        "test D: <RAISED_COSINE/0>: cwret");
		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_RAISED_COSINE, "==", gen->tone_slope.shape, "test D: <RAISED_COSINE/0>: slope shape");
		cte->expect_op_int(cte, expected_duration, "==", gen->tone_slope.duration,              "test D: <RAISED_COSINE/0>: slope duration");


		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_SINE, 0);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret,                               "test D: <SINE/0>: cwret");
		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_SINE, "==", gen->tone_slope.shape, "test D: <SINE/0>: slope shape");
		cte->expect_op_int(cte, expected_duration, "==", gen->tone_slope.duration,     "test D: <SINE/0>: slope duration");


		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_RECTANGULAR, 0);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret,                                      "test D: <RECTANGULAR/0>: cwret");
		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_RECTANGULAR, "==", gen->tone_slope.shape, "test D: <RECTANGULAR/0>: slope shape");
		cte->expect_op_int(cte, expected_duration, "==", gen->tone_slope.duration,            "test D: <RECTANGULAR/0>: slope duration");


		cwret = LIBCW_TEST_FUT(cw_gen_set_tone_slope)(gen, CW_TONE_SLOPE_SHAPE_LINEAR, 0);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret,                                 "test D: <LINEAR/0>: cwret");
		cte->expect_op_int(cte, CW_TONE_SLOPE_SHAPE_LINEAR, "==", gen->tone_slope.shape, "test D: <LINEAR/0>: slope shape");
		cte->expect_op_int(cte, expected_duration, "==", gen->tone_slope.duration,       "test D: <LINEAR/0>: slope duration");


		cw_gen_delete(&gen);
	}

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Test some assertions about CW_TONE_SLOPE_SHAPE_*

   Test code in this file depends on the fact that these values are
   different than -1. I'm testing these values to be sure that when I
   get a silly idea to modify them, the test will catch this
   modification.

   @reviewed on 2020-05-07
*/
cwt_retv test_cw_gen_tone_slope_shape_enums(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	const bool failure = CW_TONE_SLOPE_SHAPE_LINEAR < 0
		|| CW_TONE_SLOPE_SHAPE_RAISED_COSINE < 0
		|| CW_TONE_SLOPE_SHAPE_SINE < 0
		|| CW_TONE_SLOPE_SHAPE_RECTANGULAR < 0;

	cte->expect_op_int(cte, false, "==", failure, "slope shape enums");

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   It's not a test of a "forever" function, but of "forever"
   functionality.

   Pay attention to CPU usage during execution of this function. I have been
   experiencing 100% CPU usage on one of cores with ALSA sound system when HW
   configuration of the ALSA sound system was incorrect.

   @reviewed on 2020-10-15
*/
cwt_retv test_cw_gen_forever_internal(cw_test_executor_t * cte)
{
	const int loops = cte->get_loops_count(cte);
	const int seconds = 7;

	cte->print_test_header(cte, "%s (%d loops, %d seconds)", __func__, loops, seconds);

	for (int i = 0; i < loops; i++) {
		const cwt_retv rv = test_cw_gen_forever_sub(cte, seconds);
		cte->expect_op_int(cte, cwt_retv_ok, "==", rv, "'forever' test");
	}

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @reviewed on 2020-05-10
*/
static cwt_retv test_cw_gen_forever_sub(cw_test_executor_t * cte, __attribute__((unused)) int seconds)
{
	cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);
	if (NULL == gen) {
		cte->log_error(cte, "failed to create generator\n");
		return cwt_retv_err;
	}

	cw_gen_start(gen);

	//sleep(1);

	const int slope_duration = 10000; /* [us] duration of slope that should be 'pleasing' to ear. */
	const int freq = cte->config->frequency;

	cw_tone_t tone;
	CW_TONE_INIT(&tone, freq, slope_duration, CW_SLOPE_MODE_RISING_SLOPE);
	const cw_ret_t cwret1 = cw_tq_enqueue_internal(gen->tq, &tone);
	cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret1, "enqueue first tone"); /* Use "errors only" here because this is not a core part of test. */

	CW_TONE_INIT(&tone, freq, gen->quantum_duration, CW_SLOPE_MODE_NO_SLOPES);
	tone.is_forever = true;
	tone.debug_id = 'f';
	const cw_ret_t cwret2 = cw_tq_enqueue_internal(gen->tq, &tone);
	cte->expect_op_int(cte, CW_SUCCESS, "==", cwret2, "enqueue forever tone");

#ifdef __FreeBSD__  /* Tested on FreeBSD 10. */
	/* Separate path for FreeBSD because for some reason signals
	   badly interfere with value returned through second arg to
	   nanolseep().  Try to run the section in #else under FreeBSD
	   to see what happens - value returned by nanosleep() through
	   "rem" will be increasing.  TODO: see if the problem still
	   persists after moving from signals to conditional
	   variables. */
	fprintf(stderr, "enter any character to end \"forever\" tone\n");
	char c;
	scanf("%c", &c);
#else
	cw_usleep_internal(seconds * CW_USECS_PER_SEC);
#endif

	/* Silence the generator. */
	CW_TONE_INIT(&tone, freq, slope_duration, CW_SLOPE_MODE_FALLING_SLOPE);
	tone.debug_id = 'e';
	const cw_ret_t cwret3 = cw_tq_enqueue_internal(gen->tq, &tone);
	cte->expect_op_int(cte, CW_SUCCESS, "==", cwret3, "silence the generator");

	cw_gen_wait_for_queue_level(gen, 0);
	cw_gen_wait_for_end_of_current_tone(gen);

	cw_gen_stop(gen);
	cw_gen_delete(&gen);

	return cwt_retv_ok;
}




/**
   @reviewed on 2020-05-07
*/
cwt_retv test_cw_gen_get_timing_parameters_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	int initial = -5;

	int dot_duration = initial;
	int dash_duration = initial;
	int ims_duration = initial;
	int ics_duration = initial;
	int iws_duration = initial;
	int additional_space_duration = initial;
	int adjustment_space_duration = initial;

	cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);
	cw_gen_start(gen);


	cw_gen_reset_parameters_internal(gen);
	/* Reset requires resynchronization. */
	cw_gen_sync_parameters_internal(gen);


	LIBCW_TEST_FUT(cw_gen_get_timing_parameters_internal)(gen,
							      &dot_duration,
							      &dash_duration,
							      &ims_duration,
							      &ics_duration,
							      &iws_duration,
							      &additional_space_duration,
							      &adjustment_space_duration);

	bool failure = (dot_duration == initial)
		|| (dash_duration == initial)
		|| (ims_duration == initial)
		|| (ics_duration == initial)
		|| (iws_duration == initial)
		|| (additional_space_duration == initial)
		|| (adjustment_space_duration == initial);
	cte->expect_op_int(cte, false, "==", failure, "get timing parameters");

	cw_gen_delete(&gen);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Test setting and getting of some basic parameters

   @reviewed on 2020-05-07
*/
cwt_retv test_cw_gen_parameter_getters_setters(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* No parameter will have value that is larger (for "max"
	   params) or smaller (for "min" params) than this, so this is
	   a good initial value. */
	int off_limits = 10000;

	cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);
	/* It shouldn't matter for functions tested here if generator is started or not. */
	cw_gen_start(gen);

	struct {
		/* There are tree functions that take part in the
		   test: first gets range of acceptable values,
		   seconds sets a new value of parameter, and third
		   reads back the value. */

		void (* get_limits)(int * min, int * max);
		cw_ret_t (* set_new_value)(cw_gen_t * gen, int new_value);
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


	for (int i = 0; test_data[i].get_limits; i++) {

		int value = 0;
		int cwret = CW_FAILURE;

		/* Test getting limits of values to be tested. */
		test_data[i].get_limits(&test_data[i].readback_min, &test_data[i].readback_max);
		if (!cte->expect_op_int(cte, test_data[i].expected_min, "==", test_data[i].readback_min, "get %s limits: min", test_data[i].name)) {
			break;
		}
		if (!cte->expect_op_int(cte, test_data[i].expected_max, "==", test_data[i].readback_max, "get %s limits: max", test_data[i].name)) {
			break;
		}


		/* Test setting out-of-range value lower than minimum. */
		errno = 0;
		value = test_data[i].readback_min - 1;
		cwret = test_data[i].set_new_value(gen, value);
		if (!cte->expect_op_int(cte, CW_FAILURE, "==", cwret, "set %s below limit (cwret)", test_data[i].name)) {
			break;
		}
		if (!cte->expect_op_int(cte, EINVAL, "==", errno, "set %s below limit (errno)", test_data[i].name)) {
			break;
		}


		/* Test setting out-of-range value higher than maximum. */
		errno = 0;
		value = test_data[i].readback_max + 1;
		cwret = test_data[i].set_new_value(gen, value);
		if (!cte->expect_op_int(cte, CW_FAILURE, "==", cwret, "set %s above limit (cwret)", test_data[i].name)) {
			break;
		}
		if (!cte->expect_op_int(cte, EINVAL, "==", errno, "set %s above limit (errno)", test_data[i].name)) {
			break;
		}



		/* Test setting in-range values. Set with setter and then read back with getter. */
		bool set_within_range_cwret_failure = false;
		bool set_within_range_errno_failure = false;
		bool set_within_range_readback_failure = false;
		for (int value_to_set = test_data[i].readback_min; value_to_set <= test_data[i].readback_max; value_to_set++) {
			errno = 0;
			cwret = test_data[i].set_new_value(gen, value_to_set);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "set %s within limits (cwret) (value to set = %d)", test_data[i].name, value_to_set)) {
				set_within_range_cwret_failure = true;
				break;
			}
			if (!cte->expect_op_int_errors_only(cte, 0, "==", errno, "set %s within limits (errno) (value to set = %d)", test_data[i].name, value_to_set)) {
				set_within_range_errno_failure = true;
				break;
			}

			const int readback_value = test_data[i].get_value(gen);
			if (!cte->expect_op_int_errors_only(cte, readback_value, "==", value_to_set, "readback %s within limits (value to set = %d)", test_data[i].name, value_to_set)) {
				set_within_range_readback_failure = true;
				break;
			}
		}
		cte->expect_op_int(cte, false, "==", set_within_range_cwret_failure,    "set %s within range (cwret)", test_data[i].name);
		cte->expect_op_int(cte, false, "==", set_within_range_errno_failure,    "set %s within range (errno)", test_data[i].name);
		cte->expect_op_int(cte, false, "==", set_within_range_readback_failure, "set %s within range (readback)", test_data[i].name);
	}

	cw_gen_delete(&gen);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Test control of generator's volume

   Check that we can set the volume through its entire range.

   @reviewed on 2020-05-05
*/
cwt_retv test_cw_gen_volume_functions(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	const int volume_delta = 1; /* [%] volume change in percent. */
	const int tone_freq = cte->config->frequency;
	const int tone_duration = 70000; /* [microseconds] Duration can't be too short, because the loops will run too fast. */

	cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);




	/* Test: get range of allowed volumes. */
	int volume_min = -1;
	int volume_max = -1;
	{
		LIBCW_TEST_FUT(cw_get_volume_limits)(&volume_min, &volume_max);
		const bool failure = volume_min != CW_VOLUME_MIN || volume_max != CW_VOLUME_MAX;
		cte->expect_op_int(cte, false, "==", failure, "get volume limits: %d, %d", volume_min, volume_max);
	}




	/* Test: decrease volume from max to min. */
	{
		/* Few initial tones to make tone queue non-empty. */
		for (int i = 0; i < 3; i++) {
			cw_tone_t tone;
			const int slope_mode = i == 0 ? CW_SLOPE_MODE_RISING_SLOPE : CW_SLOPE_MODE_NO_SLOPES; /* Rising slope only in first tone in series of tones. */
			CW_TONE_INIT(&tone, tone_freq, tone_duration, slope_mode);
			cw_tq_enqueue_internal(gen->tq, &tone);
		}

		bool set_failure = false;
		bool get_failure = false;

		cw_gen_start(gen);

		for (int vol = volume_max; vol >= volume_min; vol -= volume_delta) {

			cw_tone_t tone;
			const int slope_mode = vol == volume_min ? CW_SLOPE_MODE_FALLING_SLOPE : CW_SLOPE_MODE_NO_SLOPES; /* No slopes in the middle - keep each level of tone constant at the beginning and end. */
			CW_TONE_INIT(&tone, tone_freq, tone_duration, slope_mode);
			cw_tq_enqueue_internal(gen->tq, &tone);

			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_set_volume)(gen, vol);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "set volume (down, vol = %d)", vol)) {
				set_failure = true;
				break;
			}

			const int readback_value = LIBCW_TEST_FUT(cw_gen_get_volume)(gen);
			if (!cte->expect_op_int(cte, readback_value, "==", vol, "get volume (down, vol = %d)", vol)) {
				get_failure = true;
				break;
			}
			//fprintf(stderr, "len = %zd\n", cw_gen_get_queue_length(gen));

			cw_gen_wait_for_queue_level(gen, 1);
		}

		cw_gen_wait_for_queue_level(gen, 0);
		usleep(2 * tone_duration);
		cw_gen_stop(gen);

		cte->expect_op_int(cte, false, "==", set_failure, "set volume (down)");
		cte->expect_op_int(cte, false, "==", get_failure, "get volume (down)");
	}




	/* Test: increase volume from min to max. */
	{
		/* Few initial tones to make tone queue non-empty. */
		for (int i = 0; i < 3; i++) {
			cw_tone_t tone;
			const int slope_mode = i == 0 ? CW_SLOPE_MODE_RISING_SLOPE : CW_SLOPE_MODE_NO_SLOPES; /* Rising slope only in first tone in series of tones. */
			CW_TONE_INIT(&tone, tone_freq, tone_duration, slope_mode);
			cw_tq_enqueue_internal(gen->tq, &tone);
		}

		bool set_failure = false;
		bool get_failure = false;

		cw_gen_start(gen);

		for (int vol = volume_min; vol <= volume_max; vol += volume_delta) {

			cw_tone_t tone;
			const int slope_mode = vol == volume_max ? CW_SLOPE_MODE_FALLING_SLOPE : CW_SLOPE_MODE_NO_SLOPES; /* No slopes in the middle - keep each level of tone constant at the beginning and end. */
			CW_TONE_INIT(&tone, tone_freq, tone_duration, slope_mode);
			cw_tq_enqueue_internal(gen->tq, &tone);

			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_set_volume)(gen, vol);
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "set volume (up, vol = %d)", vol)) {
				set_failure = true;
				break;
			}

			const int readback_value = LIBCW_TEST_FUT(cw_gen_get_volume)(gen);
			if (!cte->expect_op_int(cte, readback_value, "==", vol, "get volume (up, vol = %d)", vol)) {
				get_failure = true;
				break;
			}
			//fprintf(stderr, "len = %zd\n", cw_gen_get_queue_length(gen));

			cw_gen_wait_for_queue_level(gen, 1);
		}

		cw_gen_wait_for_queue_level(gen, 0);
		usleep(2 * tone_duration);
		cw_gen_stop(gen);

		cte->expect_op_int(cte, false, "==", set_failure, "set volume (up)");
		cte->expect_op_int(cte, false, "==", get_failure, "get volume (up)");
	}

#if 0
	/*
	  FIXME: check behaviour of these function for empty tone
	  queue, particularly if each of the functions is called one
	  after another, like this:
	  cw_gen_wait_for_end_of_current_tone(gen);
	  cw_gen_wait_for_end_of_current_tone(gen);

	  or this:
	  cw_gen_wait_for_queue_level(gen, 0);
	  cw_gen_wait_for_queue_level(gen, 0);
	*/
	cw_gen_wait_for_end_of_current_tone(gen);
	cw_gen_wait_for_queue_level(gen, 0);
#endif

	cw_gen_delete(&gen);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Test enqueueing and playing most basic elements of Morse code

   @reviewed on 2020-05-09
*/
cwt_retv test_cw_gen_enqueue_primitives(cw_test_executor_t * cte)
{
	const int loops = cte->get_loops_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, loops);

	cw_gen_t * gen = NULL;
	if (cwt_retv_ok != gen_setup(cte, &gen)) {
		cte->log_error(cte, "%s:%d: Failed to create generator\n", __func__, __LINE__);
		return cwt_retv_err;
	}
	cw_gen_start(gen);

	/* Test: playing dots. */
	{
		bool failure = false;
		for (int i = 0; i < loops; i++) {
			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_mark_internal)(gen, CW_DOT_REPRESENTATION, false);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "enqueue mark internal(CW_DOT_REPRESENTATION) (i = %d)", i)) {
				failure = true;
				break;
			}
		}
		cw_gen_wait_for_queue_level(gen, 0);

		cte->expect_op_int(cte, false, "==", failure, "enqueue mark internal(CW_DOT_REPRESENTATION)");
	}



	/* Test: playing dashes. */
	{
		bool failure = false;
		for (int i = 0; i < loops; i++) {
			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_mark_internal)(gen, CW_DASH_REPRESENTATION, false);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "enqueue mark internal(CW_DASH_REPRESENTATION) (i = %d)", i)) {
				failure = true;
				break;
			}
		}
		cw_gen_wait_for_queue_level(gen, 0);

		cte->expect_op_int(cte, false, "==", failure, "enqueue mark internal(CW_DASH_REPRESENTATION)");
	}


	/* Test: playing inter-character-spaces. */
	{
		bool failure = false;
		for (int i = 0; i < loops; i++) {
			/* TODO: this function adds 2-Unit space, not a
			   regular 3-Unit inter-character-space. Be aware of
			   this fact. */
			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_2u_ics_internal)(gen);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "enqueue ics internal() (i = %d)", i)) {
				failure = true;
				break;
			}
		}
		cw_gen_wait_for_queue_level(gen, 0);

		cte->expect_op_int(cte, false, "==", failure, "enqueue ics internal()");
	}


	/* Test: playing inter-word-spaces. */
	{
		bool failure = false;
		for (int i = 0; i < loops; i++) {
			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_iws_internal)(gen);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "enqueue iws internal() (i = %d)", i)) {
				failure = true;
				break;
			}
		}
		cw_gen_wait_for_queue_level(gen, 0);

		cte->expect_op_int(cte, false, "==", failure, "enqueue iws internal()");
	}

	gen_destroy(&gen);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Test playing representations of characters

   @reviewed on 2020-05-09
*/
cwt_retv test_cw_gen_enqueue_representations(cw_test_executor_t * cte)
{
	const int loops = cte->get_loops_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, loops);

	/* Representation is valid when it contains dots and dashes only.
	   cw_gen_enqueue_representation*() doesn't check if given
	   representation represents a supported character. */

	cw_gen_t * gen = NULL;
	if (cwt_retv_ok != gen_setup(cte, &gen)) {
		cte->log_error(cte, "%s:%d: Failed to create generator\n", __func__, __LINE__);
		return cwt_retv_err;
	}
	cw_gen_start(gen);

	/* Test: playing valid representations. */
	{
		bool failure = false;
		for (int rep = 0; rep < loops; rep++) {
			int i = 0;
			while (NULL != test_valid_representations[i]) {
				cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_representation)(gen, test_valid_representations[i]);
				if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "enqueue representation(<valid>) (%d)", i)) {
					failure = true;
					break;
				}
				cwret = LIBCW_TEST_FUT(cw_gen_enqueue_representation_no_ics)(gen, test_valid_representations[i]);
				if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "enqueue representation, no ics(<valid>) (%d)", i)) {
					failure = true;
					break;
				}
				i++;
			}
			if (failure) {
				break;
			}
		}
		cw_gen_wait_for_queue_level(gen, 0);
		cte->expect_op_int(cte, false, "==", failure, "enqueue representation internal(<valid>)");
	}


	/* Test: trying to play invalid representations. */
	{
		bool failure = false;
		for (int rep = 0; rep < loops; rep++) {
			int i = 0;
			while (NULL != test_invalid_representations[i]) {
				cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_representation)(gen, test_invalid_representations[i]);
				if (!cte->expect_op_int_errors_only(cte, CW_FAILURE, "==", cwret, "enqueue representation(<invalid>) (%d)", i)) {
					failure = true;
					break;
				}
				cwret = LIBCW_TEST_FUT(cw_gen_enqueue_representation_no_ics)(gen, test_invalid_representations[i]);
				if (!cte->expect_op_int_errors_only(cte, CW_FAILURE, "==", cwret, "enqueue representation, no ics(<invalid>) (%d)", i)) {
					failure = true;
					break;
				}
				i++;
			}
			if (failure) {
				break;
			}
		}
		cw_gen_wait_for_queue_level(gen, 0);
		cte->expect_op_int(cte, false, "==", failure, "enqueue representation internal(<invalid>)");
	}

#if 0
	/* TODO: remove this. We wait here for generator's queue to
	   drain completely, and this should be done in some other
	   way. */
	cw_usleep_internal(1 * CW_USECS_PER_SEC);
#endif
	gen_destroy(&gen);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Play all supported characters as individual characters

   @reviewed on 2020-05-09
*/
cwt_retv test_cw_gen_enqueue_character(cw_test_executor_t * cte)
{
	const int loops = cte->get_loops_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, loops);

	cw_gen_t * gen = NULL;
	if (cwt_retv_ok != gen_setup(cte, &gen)) {
		cte->log_error(cte, "%s:%d: Failed to create generator\n", __func__, __LINE__);
		return cwt_retv_err;
	}
	cw_gen_start(gen);

	/* Test: play all supported characters as individual characters. */
	{
		char charlist[UCHAR_MAX + 1] = { 0 };
		bool failure = false;

		/* Enqueue all the characters from the charlist individually. */
		cw_list_characters(charlist);
		cte->log_info(cte,
			      "enqueue character(<valid>):\n"
			      "       ");
		for (int i = 0; charlist[i] != '\0'; i++) {
			cte->log_info_cont(cte, "%c", charlist[i]);
			cte->flush_info(cte);
			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_character)(gen, charlist[i]);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "enqueue character(<valid>) (i = %d, char = '%c')", i, charlist[i])) {
				failure = true;
				break;
			}
			cw_gen_wait_for_queue_level(gen, 0); /* TODO: the queue level should be randomized. */
		}
		cte->log_info_cont(cte, "\n");
		cte->flush_info(cte);

		cte->expect_op_int(cte, false, "==", failure, "enqueue character(<valid>)");
	}


	/* Test: trying to play invalid characters. */
	{
		bool failure = false;
		for (int rep = 0; rep < loops; rep++) {
			const char invalid_characters[] = { 0x00, 0x01 }; /* List of invalid characters to be enqueued. */
			const int n = sizeof (invalid_characters) / sizeof (invalid_characters[0]);

			for (int i = 0; i < n; i++) {
				const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_character)(gen, invalid_characters[i]);
				if (!cte->expect_op_int_errors_only(cte, CW_FAILURE, "==", cwret, "enqueue character(<invalid>) (i = %d)", i)) {
					failure = true;
					break;
				}
			}
			if (failure) {
				break;
			}
		}
		cte->expect_op_int(cte, false, "==", failure, "enqueue character(<invalid>)");
	}


	gen_destroy(&gen);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Enqueue all supported characters as a single string

   @reviewed on 2020-05-09
*/
cwt_retv test_cw_gen_enqueue_string(cw_test_executor_t * cte)
{
	const int loops = cte->get_loops_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, loops);

	cw_gen_t * gen = NULL;
	if (cwt_retv_ok != gen_setup(cte, &gen)) {
		cte->log_error(cte, "%s:%d: Failed to create generator\n", __func__, __LINE__);
		return cwt_retv_err;
	}
	cw_gen_start(gen);

	/* Test: playing all supported characters as single string. */
	{
		char charlist[UCHAR_MAX + 1] = { 0 };
		cw_list_characters(charlist);

		/* Enqueue the complete charlist as a single string. */
		cte->log_info(cte,
			      "enqueue string(<valid>):\n"
			      "       %s\n", charlist);
		const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_string)(gen, charlist);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "enqueue string(<valid>)");


		while (cw_gen_get_queue_length(gen) > 0) {
			cte->log_info(cte, "tone queue length %-6zu\r", cw_gen_get_queue_length(gen));
			cte->flush_info(cte);
			cw_gen_wait_for_end_of_current_tone(gen);
		}
		cte->log_info(cte, "tone queue length %-6zu\n", cw_gen_get_queue_length(gen));
		cte->flush_info(cte);
		cw_gen_wait_for_queue_level(gen, 0);
	}


	/* Test: trying to enqueue invalid strings. */
	{
		bool failure = false;
		for (int rep = 0; rep < loops; rep++) {
			int i = 0;
			while (NULL != test_invalid_strings[i]) {
				const char * test_string = test_invalid_strings[i];
				const cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_string)(gen, test_string);
				if (!cte->expect_op_int_errors_only(cte, CW_FAILURE, "==", cwret, "enqueue string(<invalid>) (i = %d, string = '%s')", i, test_string)) {
					failure = true;
					break;
				}
				i++;
			}
			if (failure) {
				break;
			}
		}
		cte->expect_op_int(cte, false, "==", failure, "enqueue string(<invalid>)");
	}

	gen_destroy(&gen);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Test removing a character from end of enqueued characters

   @reviewed on 2020-08-24
*/
cwt_retv test_cw_gen_remove_last_character(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, "%s", __func__);

	cw_gen_t * gen = NULL;
	if (cwt_retv_ok != gen_setup(cte, &gen)) {
		cte->log_error(cte, "%s:%d: Failed to create generator\n", __func__, __LINE__);
		return cwt_retv_err;
	}

	const int n = 4;
	bool failure = false;
	for (int to_remove = 0; to_remove <= n; to_remove++) {
		cw_gen_start(gen);

		cte->log_info(cte, "You will now hear 'oooo' followed by %d 's' characters\n", n - to_remove);
		cw_gen_enqueue_string(gen, "oooo" "ssss");

		/* Remove N characters from end. */
		for (int i = 0; i < to_remove; i++) {
			cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_remove_last_character(gen));
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret,
							    "remove last %d characters, removing %d-th character",
							    to_remove, i)) {
				failure = true;
				break;
			}
		}

		cw_gen_wait_for_queue_level(gen, 0);
		cw_usleep_internal(1000 * 1000);
		cw_gen_stop(gen);

		if (failure) {
			break;
		}
	}

	gen_destroy(&gen);

	cte->expect_op_int(cte, false, "==", failure, "remove last character");

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}
