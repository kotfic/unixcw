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


#include "config.h"




#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

#include <assert.h>

#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif




#include "libcw.h"
#include "libcw_debug.h"
#include "libcw_tq.h"
#include "libcw_utils.h"
#include "libcw_gen.h"
#include "libcw_legacy_api_tests.h"




/*
  TODO: see what happens when you disable the workaround and run
  './tests/libcw_tests -S p -A k'
*/
#define LIBCW_KEY_TESTS_WORKAROUND




extern const char * test_valid_representations[];
extern const char * test_invalid_representations[];




static void test_helper_tq_callback(void * ptr);

/* Helper function for iambic key tests. */
static void legacy_api_test_iambic_key_paddles_common(cw_test_executor_t * cte, const int intended_dot_paddle, const int intended_dash_paddle, char character, int n_elements);

static int legacy_api_standalone_test_setup(cw_test_executor_t * cte, bool start_gen);
static int legacy_api_standalone_test_teardown(__attribute__((unused)) cw_test_executor_t * cte);




/**
   @brief Setup test environment for a test of legacy function

   @param start_gen whether a prepared generator should be started

   @reviewed on 2020-10-04
*/
int legacy_api_standalone_test_setup(cw_test_executor_t * cte, bool start_gen)
{
	if (CW_SUCCESS != cw_generator_new(cte->current_sound_system, cte->current_sound_device)) {
		cte->log_error(cte, "Can't create generator, stopping the test\n");
		return cwt_retv_err;
	}
	if (start_gen) {
		if (CW_SUCCESS != cw_generator_start()) {
			cte->log_error(cte, "Can't start generator, stopping the test\n");
			cw_generator_delete();
			return cwt_retv_err;
		}
	}

	cw_reset_send_receive_parameters();
	cw_set_send_speed(30);
	cw_set_receive_speed(30);
	cw_disable_adaptive_receive();
	cw_reset_receive_statistics();
	cw_unregister_signal_handler(SIGUSR1);
	errno = 0;

	return cwt_retv_ok;
}




/**
   @brief Deconfigure test environment after running a test of legacy function

   @reviewed on 2020-10-04
*/
int legacy_api_standalone_test_teardown(__attribute__((unused)) cw_test_executor_t * cte)
{
	sleep(1);
	cw_generator_stop();
	sleep(1);
	cw_generator_delete();

	return 0;
}




/**
   @reviewed on 2019-10-13
*/
int legacy_api_test_low_level_gen_parameters(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	int txdot_usecs = -1;
	int txdash_usecs = -1;
	int end_of_element_usecs = -1;
	int end_of_character_usecs = -1;
	int end_of_word_usecs = -1;
	int additional_usecs = -1;
	int adjustment_usecs = -1;

	/* Print and verify default low level timing values. */
	cw_reset_send_receive_parameters();
	LIBCW_TEST_FUT(cw_get_send_parameters)(&txdot_usecs, &txdash_usecs,
					       &end_of_element_usecs, &end_of_character_usecs,
					       &end_of_word_usecs, &additional_usecs,
					       &adjustment_usecs);
	cte->log_info(cte,
		      "cw_get_send_parameters():\n"
		      "    %d, %d, %d, %d, %d, %d, %d\n",
		      txdot_usecs, txdash_usecs, end_of_element_usecs,
		      end_of_character_usecs,end_of_word_usecs, additional_usecs,
		      adjustment_usecs);

	cte->expect_op_int(cte, txdot_usecs,            ">=", 0, "send parameters: txdot_usecs");
	cte->expect_op_int(cte, txdash_usecs,           ">=", 0, "send parameters: txdash_usecs");
	cte->expect_op_int(cte, end_of_element_usecs,   ">=", 0, "send parameters: end_of_element_usecs");
	cte->expect_op_int(cte, end_of_character_usecs, ">=", 0, "send parameters: end_of_character_usecs");
	cte->expect_op_int(cte, end_of_word_usecs,      ">=", 0, "send parameters: end_of_word_usecs");
	cte->expect_op_int(cte, additional_usecs,       ">=", 0, "send parameters: additional_usecs");
	cte->expect_op_int(cte, adjustment_usecs,       ">=", 0, "send parameters: adjustment_usecs");

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-13
*/
int legacy_api_test_parameter_ranges(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	const int off_limits = 10000;

	/* Test setting and getting of some basic parameters. */

	struct {
		/* There are tree functions that take part in the
		   test: first gets range of acceptable values,
		   seconds sets a new value of parameter, and third
		   reads back the value. */

		void (* get_limits)(int *min, int *max);
		int (* set_new_value)(int new_value);
		int (* get_value)(void);

		const int expected_min;   /* Expected value of minimum. */
		const int expected_max;   /* Expected value of maximum. */

		int readback_min;   /* Value returned by 'get_limits()' function. */
		int readback_max;   /* Value returned by 'get_limits()' function. */

		const char *name;
	} test_data[] = {
		{ LIBCW_TEST_FUT(cw_get_speed_limits),
		  LIBCW_TEST_FUT(cw_set_send_speed),
		  LIBCW_TEST_FUT(cw_get_send_speed),
		  CW_SPEED_MIN,      CW_SPEED_MAX,      off_limits,  -off_limits,  "send_speed"    },

		{ LIBCW_TEST_FUT(cw_get_speed_limits),
		  LIBCW_TEST_FUT(cw_set_receive_speed),
		  LIBCW_TEST_FUT(cw_get_receive_speed),
		  CW_SPEED_MIN,      CW_SPEED_MAX,      off_limits,  -off_limits,  "receive_speed" },

		{ LIBCW_TEST_FUT(cw_get_frequency_limits),
		  LIBCW_TEST_FUT(cw_set_frequency),
		  LIBCW_TEST_FUT(cw_get_frequency),
		  CW_FREQUENCY_MIN,  CW_FREQUENCY_MAX,  off_limits,  -off_limits,  "frequency"     },

		{ LIBCW_TEST_FUT(cw_get_volume_limits),
		  LIBCW_TEST_FUT(cw_set_volume),
		  LIBCW_TEST_FUT(cw_get_volume),
		  CW_VOLUME_MIN,     CW_VOLUME_MAX,     off_limits,  -off_limits,  "volume"        },

		{ LIBCW_TEST_FUT(cw_get_gap_limits),
		  LIBCW_TEST_FUT(cw_set_gap),
		  LIBCW_TEST_FUT(cw_get_gap),
		  CW_GAP_MIN,        CW_GAP_MAX,        off_limits,  -off_limits,  "gap"           },

		{ LIBCW_TEST_FUT(cw_get_tolerance_limits),
		  LIBCW_TEST_FUT(cw_set_tolerance),
		  LIBCW_TEST_FUT(cw_get_tolerance),
		  CW_TOLERANCE_MIN,  CW_TOLERANCE_MAX,  off_limits,  -off_limits,  "tolerance"     },

		{ LIBCW_TEST_FUT(cw_get_weighting_limits),
		  LIBCW_TEST_FUT(cw_set_weighting),
		  LIBCW_TEST_FUT(cw_get_weighting),
		  CW_WEIGHTING_MIN,  CW_WEIGHTING_MAX,  off_limits,  -off_limits,  "weighting"     },

		{ NULL,
		  NULL,
		  NULL,
		  0,                 0,                 0,            0,           NULL            }
	};


	for (int i = 0; test_data[i].get_limits; i++) {
		int cwret;

		/* Get limits of values to be tested. */
		test_data[i].get_limits(&test_data[i].readback_min, &test_data[i].readback_max);
		cte->expect_op_int(cte, test_data[i].expected_min, "==", test_data[i].readback_min, "get %s limits: min", test_data[i].name);
		cte->expect_op_int(cte, test_data[i].expected_max, "==", test_data[i].readback_max, "get %s limits: min", test_data[i].name);


		/* Test setting out-of-range value lower than minimum. */
		errno = 0;
		cwret = test_data[i].set_new_value(test_data[i].readback_min - 1);
		cte->expect_op_int(cte, EINVAL, "==", errno, "cw_set_%s(min - 1):", test_data[i].name);
		cte->expect_op_int(cte, CW_FAILURE, "==", cwret, "cw_set_%s(min - 1):", test_data[i].name);


		/* Test setting out-of-range value higher than maximum. */
		errno = 0;
		cwret = test_data[i].set_new_value(test_data[i].readback_max + 1);
		cte->expect_op_int(cte, EINVAL, "==", errno, "cw_set_%s(max + 1):", test_data[i].name);
		cte->expect_op_int(cte, CW_FAILURE, "==", cwret, "cw_set_%s(max + 1):", test_data[i].name);


		/*
		  Test setting and reading back of in-range values.
		  There will be many, many iterations, so use ::expect_op_int(errors_only).
		*/
		bool set_within_range_cwret_failure = false;
		bool set_within_range_errno_failure = false;
		bool set_within_range_readback_failure = false;
		for (int value_to_set = test_data[i].readback_min; value_to_set <= test_data[i].readback_max; value_to_set++) {

			errno = 0;
			cwret = test_data[i].set_new_value(value_to_set);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "set %s within limits (cwret) (value to set = %d)", test_data[i].name, value_to_set)) {
				set_within_range_cwret_failure = true;
				break;
			}
			if (!cte->expect_op_int_errors_only(cte, 0, "==", errno, "set %s within limits (errno) (value to set = %d)", test_data[i].name, value_to_set)) {
				set_within_range_errno_failure = true;
				break;
			}

			const int readback_value = test_data[i].get_value();
			if (!cte->expect_op_int_errors_only(cte, readback_value, "==", value_to_set, "readback %s within limits (value to set = %d)", test_data[i].name, value_to_set)) {
				set_within_range_readback_failure = true;
				break;
			}
		}
		cte->expect_op_int(cte, false, "==", set_within_range_cwret_failure, "cw_get/set_%s() within range: cwret", test_data[i].name);
		cte->expect_op_int(cte, false, "==", set_within_range_errno_failure, "cw_get/set_%s() within range: errno", test_data[i].name);
		cte->expect_op_int(cte, false, "==", set_within_range_readback_failure, "cw_get/set_%s(): within range: readback", test_data[i].name);
	}


	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Fill a queue and then wait for each tone separately - repeat until
   all tones are dequeued.

   @reviewed on 2019-10-13
*/
int legacy_api_test_cw_wait_for_tone(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, false);

	int cwret;

	/* This is a simple test, so only a handful of tones. */
	const int n_tones_to_add = 6;

	/* Duration long enough to not allow generator dequeue two tones in a
	   row too fast. */
	const int tone_duration = 1000 * 1000;

	/* Test setup. */
	{
		cw_set_volume(70);

		int freq_min, freq_max;
		cw_get_frequency_limits(&freq_min, &freq_max);
		const int delta_freq = ((freq_max - freq_min) / (n_tones_to_add - 1));      /* Delta of frequency in loops. */

		/* Test 1: enqueue n_tones_to_add tones, and wait for each of
		   them separately. Control length of tone queue in the
		   process. */

		for (int i = 0; i < n_tones_to_add; i++) {

			int readback_length = 0;       /* Measured length of tone queue. */
			int expected_length = 0;  /* Expected length of tone queue. */

			/* Monitor length of a queue as it is filled - before
			   adding a new tone. */
			readback_length = LIBCW_TEST_FUT(cw_get_tone_queue_length)();
			expected_length = i;
			cte->expect_op_int(cte, expected_length, "==", readback_length,
					   "setup: queue length before adding tone = %d", readback_length);


			/* Add a tone to queue. All frequencies should be
			   within allowed range, so there should be no
			   error. */
			const int freq = freq_min + i * delta_freq;
			cwret = LIBCW_TEST_FUT(cw_queue_tone)(tone_duration, freq);
			cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "setup: cw_queue_tone() #%02d", i);


			/* Monitor length of a queue as it is filled - after
			   adding a new tone. */
			readback_length = LIBCW_TEST_FUT(cw_get_tone_queue_length)();
			expected_length = i + 1;
			cte->expect_op_int(cte, expected_length, "==", readback_length,
					   "setup: queue length after adding tone = %d", readback_length);
		}
	}

	/* Test. */
	{
		cw_generator_start();

		/*
		  This is the proper test - waiting for dequeueing
		  tones. Notice "-1" in loop initialization. We assume here
		  that first tone has been already dequeued after generator
		  has been started.

		  TODO: redesign the test to avoid guessing how many tones
		  have been already dequeued by running generator.
		*/
		for (int i = n_tones_to_add - 1; i > 0; i--) {

			int readback_length = 0;  /* Measured length of tone queue. */
			int expected_length = 0;  /* Expected length of tone queue. */

			/* Monitor length of a queue as it is emptied - before dequeueing. */
			readback_length = LIBCW_TEST_FUT(cw_get_tone_queue_length)();
			expected_length = i;
			cte->expect_op_int(cte, expected_length, "==", readback_length,
					   "test: queue length before dequeueing = %d", readback_length);

			/* Wait for each of n_tones_to_add tones to be dequeued. */
			cwret = LIBCW_TEST_FUT(cw_wait_for_tone)();
			cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "test: cw_wait_for_tone():");

			/* Monitor length of a queue as it is emptied - after dequeueing single tone. */
			readback_length = LIBCW_TEST_FUT(cw_get_tone_queue_length)();
			expected_length = i - 1;
			cte->expect_op_int(cte, expected_length, "==", readback_length,
					   "test: queue length after dequeueing = %d", readback_length);
		}

		cw_generator_stop();
	}

	/* Test tear-down. */
	{
	}

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Fill a queue, don't wait for each tone separately, but wait for a
   whole queue to become empty.

   @reviewed on 2019-10-13
*/
int legacy_api_test_cw_wait_for_tone_queue(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	/* Don't let generator run and dequeue tones just
	   yet. Running/dequeueing generator will break our 'length' test. */
	const bool start_generator = false;
	legacy_api_standalone_test_setup(cte, start_generator);

	const int n_tones_to_add = 6;     /* This is a simple test, so only a handful of tones. */

	/*
	  Test setup:
	  Add tones to tone queue.
	*/
	{
		cw_set_volume(70);

		int freq_min, freq_max;
		cw_get_frequency_limits(&freq_min, &freq_max);
		const int delta_freq = ((freq_max - freq_min) / (n_tones_to_add - 1));

		const int tone_duration = 100000;

		for (int i = 0; i < n_tones_to_add; i++) {
			const int freq = freq_min + i * delta_freq;
			int cwret = LIBCW_TEST_FUT(cw_queue_tone)(tone_duration, freq);
			const bool success = cte->expect_op_int(cte,
								CW_SUCCESS, "==", cwret,
								"%s:%d setup: cw_queue_tone(%d, %d):",
								__func__, __LINE__,
								tone_duration, freq);
			if (!success) {
				break;
			}
		}
	}

	/*
	  Test 1 (supplementary):
	  Queue with enqueued tones should have some specific length.
	*/
	{
		const int len = LIBCW_TEST_FUT(cw_get_tone_queue_length)();
		cte->expect_op_int(cte,
				   n_tones_to_add, "==", len,
				   "%s:%d: test: cw_get_tone_queue_length()",
				   __func__, __LINE__);
	}

	/*
	  Test 2 (main):
	  We should be able to wait for emptying of non-empty queue while
	  running generator is dequeueing tones.
	*/
	{
		cw_generator_start();

		int cwret = LIBCW_TEST_FUT(cw_wait_for_tone_queue)();
		cte->expect_op_int(cte,
				   CW_SUCCESS, "==", cwret,
				   "%s:%d: test: cw_wait_for_tone_queue()",
				   __func__, __LINE__);
	}

	/* Test tear-down. */
	{
	}

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Run the complete range of tone generation, at X Hz intervals, first
   up the octaves, and then down.  If the queue fills, though it
   shouldn't with this amount of data, then pause until it isn't so
   full.

   TODO: this test doesn't really test anything well. It just ensures
   that in some conditions cw_queue_tone() works correctly.

   @reviewed on 2019-10-13
*/
int legacy_api_test_cw_queue_tone(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	cw_set_volume(70);
	int duration = 20000;

	int freq_min, freq_max;
	cw_get_frequency_limits(&freq_min, &freq_max);
	const int freq_delta = 10;

	bool wait_success = true;
	bool queue_success = true;

	for (int freq = freq_min; freq < freq_max; freq += freq_delta) {
		while (true == cw_is_tone_queue_full()) {

			/* TODO: we may never get to test
			   cw_wait_for_tone() function because the
			   queue will never be full in this test. */
			int cwret = cw_wait_for_tone();
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_wait_for_tone(#1, %d)", freq)) {
				wait_success = false;
				break;
			}
		}

		int cwret = LIBCW_TEST_FUT(cw_queue_tone)(duration, freq);
		if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_queue_tone(#1, %d)", freq)) {
			queue_success = false;
			break;
		}
	}

	for (int freq = freq_max; freq > freq_min; freq -= freq_delta) {
		while (true == cw_is_tone_queue_full()) {

			/* TODO: we may never get to test
			   cw_wait_for_tone() function because the
			   queue will never be full in this test. */
			int cwret = cw_wait_for_tone();
			if (!cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_wait_for_tone(#2, %d)", freq)) {
				wait_success = false;
				break;
			}
		}

		int cwret = LIBCW_TEST_FUT(cw_queue_tone)(duration, freq);
		if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_queue_tone(#2, %d)", freq)) {
			queue_success = false;
			break;
		}
	}

	/* Final expect for 'queue' and 'wait' calls in the loop above. */
	cte->expect_op_int(cte, true, "==", queue_success, "cw_queue_tone() - enqueueing");
	cte->expect_op_int(cte, true, "==", wait_success, "cw_queue_tone() - waiting");


	/* We have been adding tones to the queue, so we can test
	   waiting for the queue to be emptied. */
	int cwret = cw_wait_for_tone_queue();
	cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_wait_for_tone_queue()");

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}





/**
   @reviewed on 2019-10-13
*/
int legacy_api_test_empty_tone_queue(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	/* Test setup. */
	{
		cw_set_volume(70);

		/* Clear tone queue and make sure that it is really
		   empty (wait for info from libcw). */
		cw_flush_tone_queue();
		cw_wait_for_tone_queue();
	}

	/* Test. */
	{
		const int capacity = LIBCW_TEST_FUT(cw_get_tone_queue_capacity)();
		cte->expect_op_int(cte, CW_TONE_QUEUE_CAPACITY_MAX, "==", capacity, "cw_get_tone_queue_capacity()");

		const int len_empty = LIBCW_TEST_FUT(cw_get_tone_queue_length)();
		cte->expect_op_int(cte, 0, "==", len_empty, "cw_get_tone_queue_length() when tq is empty");
	}

	/* Test tear-down. */
	{
	}

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-13
*/
int legacy_api_test_full_tone_queue(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	/* Test setup. */
	{
		cw_set_volume(70);

		/* FIXME: we call cw_queue_tone() until tq is full,
		   and then expect the tq to be full while we perform
		   tests. Doesn't the tq start dequeuing tones right
		   away? Can we expect the tq to be full for some time
		   after adding last tone?  Hint: check when a length
		   of tq is decreased. Probably after playing first
		   tone on tq, which - in this test - is pretty
		   long. Or perhaps not. */

		const int duration = 1000000;
		int i = 0;

		/* FIXME: cw_is_tone_queue_full() is not tested */
		while (!cw_is_tone_queue_full()) {
			const int freq = 100 + (i++ & 1) * 100;
			LIBCW_TEST_FUT(cw_queue_tone)(duration, freq);
		}
	}

	/*
	  Test 1
	  Test properties (capacity and length) of full tq.
	*/
	{
		const int capacity = LIBCW_TEST_FUT(cw_get_tone_queue_capacity)();
		cte->expect_op_int(cte, CW_TONE_QUEUE_CAPACITY_MAX, "==", capacity, "cw_get_tone_queue_capacity()");

		const int len_full = LIBCW_TEST_FUT(cw_get_tone_queue_length)();
		cte->log_info(cte, "*** you may now see \"EE: can't enqueue tone, tq is full\" message ***\n");
		cte->expect_op_int(cte, CW_TONE_QUEUE_CAPACITY_MAX, "==", len_full, "cw_get_tone_queue_length() when tq is full");
	}

	/*
	  Test 2
	  Attempt to add tone to full queue.
	*/
	{
		errno = 0;
		int cwret = LIBCW_TEST_FUT(cw_queue_tone)(1000000, 100);
		cte->expect_op_int(cte, EAGAIN, "==", errno, "cw_queue_tone() for full tq (errno)");
		cte->expect_op_int(cte, CW_FAILURE, "==", cwret, "cw_queue_tone() for full tq (cwret)");
	}

	/*
	  Test 3

	  Check again properties (capacity and length) of empty tq
	  after it has been in use.
	*/
	{
		int cwret;

		/* Empty tone queue and make sure that it is really
		   empty (wait for info from libcw). */
		LIBCW_TEST_FUT(cw_flush_tone_queue)();

		cwret = LIBCW_TEST_FUT(cw_wait_for_tone_queue)();
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_wait_for_tone_queue() after flushing");

		const int capacity = LIBCW_TEST_FUT(cw_get_tone_queue_capacity)();
		cte->expect_op_int(cte, CW_TONE_QUEUE_CAPACITY_MAX, "==", capacity, "cw_get_tone_queue_capacity() after flushing");

		/* Test that the tq is really empty after
		   cw_wait_for_tone_queue() has returned. */
		const int len_empty = LIBCW_TEST_FUT(cw_get_tone_queue_length)();
		cte->expect_op_int(cte, 0, "==", len_empty, "cw_get_tone_queue_length() after flushing");
	}

	/* Test tear-down. */
	{
	}

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




typedef struct callback_data {
	bool can_capture;
	int captured_level;
} callback_data;


/**
   @reviewed on 2019-10-13
*/
int legacy_api_test_tone_queue_callback(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	for (int i = 1; i < 10; i++) {
		/* Test the callback mechanism for very small values,
		   but for a bit larger as well. */
		int level = i <= 5 ? i : 10 * i;

		callback_data data = { 0 };
		data.captured_level = 9999999;

		int cwret = LIBCW_TEST_FUT(cw_register_tone_queue_low_callback)(test_helper_tq_callback, (void *) &data, level);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_register_tone_queue_low_callback(): threshold = %d:", level);
		sleep(1);


		/* Add a lot of tones to tone queue. "a lot" means three times more than a value of trigger level. */
		for (int j = 0; j < 3 * level; j++) {
			int duration = 10000;
			int f = 440;
			int rv = cw_queue_tone(duration, f);
			assert (rv);
		}

		/* Allow the callback to work only after initial
		   filling of queue. */
		data.can_capture = true;

		/* Wait for the queue to be drained to zero. While the
		   tq is drained, and level of tq reaches trigger
		   level, a callback will be called. Its only task is
		   to copy the current level (tq level at time of
		   calling the callback) value into
		   callback_data::captured_level.

		   Since the value of trigger level is different in
		   consecutive iterations of loop, we can test the
		   callback for different values of trigger level. */
		cw_wait_for_tone_queue();

		/* Because of order of calling callback and decreasing
		   length of queue, I think that it's safe to assume
		   that captured level may be in a range of values. */
		const int expected_lower = level - 1;
		const int expected_higher = level;
		cte->expect_between_int(cte, expected_lower, data.captured_level, expected_higher, "tone queue callback:           level at callback = %d", data.captured_level);

		cw_reset_tone_queue();
	}

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-13
*/
static void test_helper_tq_callback(void * ptr)
{
	callback_data * data = (callback_data *) ptr;

	if (data->can_capture) {
		data->captured_level = cw_get_tone_queue_length();
		data->can_capture = false;
	}

	return;
}




/**
   \brief Test control of volume

   Fill tone queue with short tones, then check that we can move the
   volume through its entire range.  Flush the queue when complete.

   @reviewed on 2019-10-13
*/
int legacy_api_test_volume_functions(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	int vol_min = -1;
	int vol_max = -1;
	const int vol_delta = 5;

	/* Test: get range of allowed volumes. */
	{
		LIBCW_TEST_FUT(cw_get_volume_limits)(&vol_min, &vol_max);

		cte->expect_op_int(cte, CW_VOLUME_MIN, "==", vol_min, "cw_get_volume_limits(): min = %d%%", vol_min);
		cte->expect_op_int(cte, CW_VOLUME_MAX, "==", vol_max, "cw_get_volume_limits(): max = %d%%", vol_max);
	}


	/*
	  Test setup.
	  Fill the tone queue with valid tones.
	*/
	{
		while (!cw_is_tone_queue_full()) {
			cw_queue_tone(100000, 440);
		}
	}

	/* Test: decrease volume from max to min. */
	{
		bool set_failure = false;
		bool get_failure = false;

		for (int volume = vol_max; volume >= vol_min; volume -= vol_delta) {

			/* We wait here for next tone so that changes
			   in volume happen once per tone - not more
			   often and not less. */
			cw_wait_for_tone();

			const int cwret = LIBCW_TEST_FUT(cw_set_volume)(volume);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_set_volume(%d) (down)", volume)) {
				set_failure = true;
				break;
			}

			const int readback = LIBCW_TEST_FUT(cw_get_volume)();
			if (!cte->expect_op_int_errors_only(cte, volume, "==", readback, "cw_get_volume() (down) -> %d", readback)) {
				get_failure = true;
				break;
			}
		}

		cte->expect_op_int(cte, false, "==", set_failure, "cw_set_volume() (down)");
		cte->expect_op_int(cte, false, "==", get_failure, "cw_get_volume() (down)");
	}

	/* Test tear-down. */
	{
		cw_flush_tone_queue();
	}


	/* ---------------- */


	/*
	  Test setup.
	  Fill the tone queue with valid tones.
	*/
	{
		while (!cw_is_tone_queue_full()) {
			cw_queue_tone(100000, 440);
		}
	}

	/* Test: increase volume from min to max. */
	{
		bool set_failure = false;
		bool get_failure = false;

		for (int volume = vol_min; volume <= vol_max; volume += vol_delta) {

			/* We wait here for next tone so that changes
			   in volume happen once per tone - not more
			   often and not less. */
			cw_wait_for_tone();

			const int cwret = LIBCW_TEST_FUT(cw_set_volume)(volume);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_set_volume(%d) (up)", volume)) {
				set_failure = true;
				break;
			}

			const int readback = LIBCW_TEST_FUT(cw_get_volume)();
			if (!cte->expect_op_int_errors_only(cte, volume, "==", readback, "cw_get_volume() (up) -> %d", readback)) {
				get_failure = true;
				break;
			}
		}

		cte->expect_op_int(cte, false, "==", set_failure, "cw_set_volume() (up)");
		cte->expect_op_int(cte, false, "==", get_failure, "cw_get_volume() (up)");
	}

	/* Test tear-down. */
	{
		cw_flush_tone_queue();
	}

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   \brief Test enqueueing most basic elements of Morse code

   @reviewed on 2019-10-13
*/
int legacy_api_test_send_primitives(cw_test_executor_t * cte)
{
	const int max = cte->get_repetitions_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, max);
	legacy_api_standalone_test_setup(cte, true);

	/* Test: sending dot. */
	{
		bool failure = false;
		for (int i = 0; i < max; i++) {
			const int cwret = LIBCW_TEST_FUT(cw_send_dot)();
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_send_dot() #%d", i)) {
				failure = true;
				break;
			}
		}
		cw_wait_for_tone_queue();
		cte->expect_op_int(cte, false, "==", failure, "cw_send_dot()");
	}

	/* Test: sending dash. */
	{
		bool failure = false;
		for (int i = 0; i < max; i++) {
			const int cwret = LIBCW_TEST_FUT(cw_send_dash)();
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_send_dash() #%d", i)) {
				failure = true;
				break;
			}
		}
		cw_wait_for_tone_queue();
		cte->expect_op_int(cte, false, "==", failure, "cw_send_dash()");
	}

	/* Test: sending character space. */
	{
		bool failure = false;
		for (int i = 0; i < max; i++) {
			const int cwret = LIBCW_TEST_FUT(cw_send_character_space)();
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_send_character_space() #%d", i)) {
				failure = true;
				break;
			}
		}
		cw_wait_for_tone_queue();
		cte->expect_op_int(cte, false, "==", failure, "cw_send_character_space()");
	}

	/* Test: sending word space. */
	{
		bool failure = false;
		for (int i = 0; i < max; i++) {
			const int cwret = LIBCW_TEST_FUT(cw_send_word_space)();
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_send_word_space() #%d", i)) {
				failure = true;
				break;
			}
		}
		cw_wait_for_tone_queue();
		cte->expect_op_int(cte, false, "==", failure, "cw_send_word_space()");
	}

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   \brief Enqueueing representations of characters

   @reviewed on 2019-10-13
*/
int legacy_api_test_representations(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	/* Test: sending valid representations. */
	{
		bool failure = false;
		int i = 0;
		while (NULL != test_valid_representations[i]) {
			const int cwret = LIBCW_TEST_FUT(cw_send_representation)(test_valid_representations[i]);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_send_representation(valid #%d)", i)) {
				failure = true;
				break;
			}
			i++;
		}
		cte->expect_op_int(cte, false, "==", failure, "cw_send_representation(valid)");
		cw_wait_for_tone_queue();
	}

	/* Test: sending invalid representations. */
	{
		bool failure = false;
		int i = 0;
		while (NULL != test_invalid_representations[i]) {
			const int cwret = LIBCW_TEST_FUT(cw_send_representation)(test_invalid_representations[i]);
			if (!cte->expect_op_int_errors_only(cte, CW_FAILURE, "==", cwret, "cw_send_representation(invalid #%d)", i)) {
				failure = true;
				break;
			}
			i++;
		}
		cte->expect_op_int(cte, false, "==", failure, "cw_send_representation(invalid)");
		cw_wait_for_tone_queue();
	}

	/* Test: sending partial representation of a valid string. */
	{
		bool failure = false;
		int i = 0;
		while (NULL != test_valid_representations[i]) {
			const int cwret = LIBCW_TEST_FUT(cw_send_representation_partial)(test_valid_representations[i]);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_send_representation_partial(valid #%d)", i)) {
				failure = true;
				break;
			}
			i++;
		}
		cte->expect_op_int(cte, false, "==", failure, "cw_send_representation_partial(valid)");
		cw_wait_for_tone_queue();
	}

	/* Test: sending partial representation of a invalid string. */
	{
		bool failure = false;
		int i = 0;
		while (NULL != test_invalid_representations[i]) {
			const int cwret = LIBCW_TEST_FUT(cw_send_representation_partial)(test_invalid_representations[i]);
			if (!cte->expect_op_int_errors_only(cte, CW_FAILURE, "==", cwret, "cw_send_representation_partial(invalid #%d)", i)) {
				failure = true;
				break;
			}
			i++;
		}
		cte->expect_op_int(cte, false, "==", failure, "cw_send_representation_partial(invalid)");
		cw_wait_for_tone_queue();
	}

	cw_wait_for_tone_queue();

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Send all supported characters: first as individual characters, and then as a string.

   @reviewed on 2019-10-13
*/
int legacy_api_test_send_character_and_string(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	/* Test: sending all supported characters as individual characters. */
	{
		char charlist[UCHAR_MAX + 1] = { 0 }; /* TODO: get size of this buffer through cw_get_character_count(). */
		LIBCW_TEST_FUT(cw_list_characters)(charlist);

		bool failure = false;

		/* Send all the characters from the charlist individually. */

		cte->log_info(cte,
			      "cw_send_character(<valid>):\n"
			      "    ");

		for (int i = 0; charlist[i] != '\0'; i++) {

			const char character = charlist[i];
			cte->log_info_cont(cte, "%c", character);
			cte->flush_info(cte);

			const int cwret = LIBCW_TEST_FUT(cw_send_character)(character);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_send_character(%c)", character)) {
				failure = true;
				break;
			}
			cw_wait_for_tone_queue();
		}

		cte->log_info_cont(cte, "\n");
		cte->flush_info(cte);

		cte->expect_op_int(cte, false, "==", failure, "cw_send_character(<valid>)");
	}

	/* Test: sending invalid character. */
	{
		const int cwret = LIBCW_TEST_FUT(cw_send_character)(0);
		cte->expect_op_int(cte, CW_FAILURE, "==", cwret, "cw_send_character(<invalid>)");
	}

	/* Test: sending all supported characters as single string. */
	{
		char charlist[UCHAR_MAX + 1] = { 0 }; /* TODO: get size of this buffer through cw_get_character_count(). */
		LIBCW_TEST_FUT(cw_list_characters)(charlist);

		/* Send the complete charlist as a single string. */
		cte->log_info(cte,
			      "cw_send_string(<valid>):\n"
			      "    %s\n", charlist);

		const int cwret = LIBCW_TEST_FUT(cw_send_string)(charlist);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_send_string(<valid>)");


		while (cw_get_tone_queue_length() > 0) {
			cte->log_info(cte, "tone queue length %-6d\r", cw_get_tone_queue_length());
			cte->flush_info(cte);
			cw_wait_for_tone();
		}
		cte->log_info(cte, "tone queue length %-6d\n", cw_get_tone_queue_length());
	}


	/* Test: sending invalid string. */
	{
		const int cwret = LIBCW_TEST_FUT(cw_send_string)("%INVALID%");
		cte->expect_op_int(cte, CW_FAILURE, "==", cwret, "cw_send_string(<invalid>)");
	}

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Wrapper for common code used by three test functions.

   @reviewed on 2019-10-13
*/
void legacy_api_test_iambic_key_paddles_common(cw_test_executor_t * cte, const int intended_dot_paddle, const int intended_dash_paddle, char character, int n_elements)
{
	/* Test: keying alternate dit/dash. */
	{
		/* It seems like this function calls means "keyer
		   pressed until further notice".*/
		const int cwret = LIBCW_TEST_FUT(cw_notify_keyer_paddle_event)(intended_dot_paddle, intended_dash_paddle);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_notify_keyer_paddle_event(%d, %d)", intended_dot_paddle, intended_dash_paddle);

		bool success = true;
		cte->flush_info(cte);
		for (int i = 0; i < n_elements; i++) {
			success = success && LIBCW_TEST_FUT(cw_wait_for_keyer_element)();
			cte->log_info_cont(cte, "%c", character);
			cte->flush_info(cte);
		}
		cte->log_info_cont(cte, "\n");

		cte->expect_op_int(cte, true, "==", success, "cw_wait_for_keyer_element() (%c)", character);
	}

	/* Test: preserving of paddle states. */
	{
		/* State of paddles should be the same as after call
		   to cw_notify_keyer_paddle_event() above. */
		int read_back_dot_paddle;
		int read_back_dash_paddle;
		LIBCW_TEST_FUT(cw_get_keyer_paddles)(&read_back_dot_paddle, &read_back_dash_paddle);
		cte->expect_op_int(cte, intended_dot_paddle, "==", read_back_dot_paddle, "cw_get_keyer_paddles(): dot paddle");
		cte->expect_op_int(cte, intended_dash_paddle, "==", read_back_dash_paddle, "cw_get_keyer_paddles(): dash paddle");
	}

	cte->flush_info(cte);

	cw_wait_for_keyer();

	return;
}




/**
   Perform some tests on the iambic keyer.  The latch finer timing
   points are not tested here, just the basics - dots, dashes, and
   alternating dots and dashes.

   @reviewed on 2019-10-13
*/
int legacy_api_test_iambic_key_dot(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	/*
	  Test: keying dot.
	  Since a "dot" paddle is pressed, get N "dot" events from
	  the keyer.

	  This is test of legacy API, so use true/false instead of
	  CW_KEY_STATE_CLOSED (true)/CW_KEY_STATE_OPEN (false).
	*/
	const int intended_dot_paddle = true;
	const int intended_dash_paddle = false;
	const char character = CW_DOT_REPRESENTATION;
	const int n_elements = 30;
	legacy_api_test_iambic_key_paddles_common(cte, intended_dot_paddle, intended_dash_paddle, character, n_elements);

#ifndef LIBCW_KEY_TESTS_WORKAROUND
	legacy_api_standalone_test_teardown(cte);
#endif
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Perform some tests on the iambic keyer.  The latch finer timing
   points are not tested here, just the basics - dots, dashes, and
   alternating dots and dashes.

   @reviewed on 2019-10-13
*/
int legacy_api_test_iambic_key_dash(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
#ifndef LIBCW_KEY_TESTS_WORKAROUND
	legacy_api_standalone_test_setup(cte, true);
#endif

	/*
	  Test: keying dash.
	  Since a "dash" paddle is pressed, get N "dash" events from
	  the keyer.

	  This is test of legacy API, so use true/false instead of
	  CW_KEY_STATE_CLOSED (true)/CW_KEY_STATE_OPEN (false).
	*/
	const int intended_dot_paddle = false;
	const int intended_dash_paddle = true;
	const char character = CW_DASH_REPRESENTATION;
	const int n_elements = 30;
	legacy_api_test_iambic_key_paddles_common(cte, intended_dot_paddle, intended_dash_paddle, character, n_elements);

#ifndef LIBCW_KEY_TESTS_WORKAROUND
	legacy_api_standalone_test_teardown(cte);
#endif
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Perform some tests on the iambic keyer.  The latch finer timing
   points are not tested here, just the basics - dots, dashes, and
   alternating dots and dashes.

   @reviewed on 2019-10-13
*/
int legacy_api_test_iambic_key_alternating(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
#ifndef LIBCW_KEY_TESTS_WORKAROUND
	legacy_api_standalone_test_setup(cte, true);
#endif

	/*
	  Test: keying alternate dit/dash.
	  Both arguments are true, so both paddles are pressed at the
	  same time.

	  This is test of legacy API, so use true/false instead of
	  CW_KEY_STATE_CLOSED (true)/CW_KEY_STATE_OPEN (false).
	*/
	const int intended_dot_paddle = true;
	const int intended_dash_paddle = true;
	const char character = '#';
	const int n_elements = 30;
	legacy_api_test_iambic_key_paddles_common(cte, intended_dot_paddle, intended_dash_paddle, character, n_elements);

#ifndef LIBCW_KEY_TESTS_WORKAROUND
	legacy_api_standalone_test_teardown(cte);
#endif
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Perform some tests on the iambic keyer.  The latch finer timing
   points are not tested here, just the basics - dots, dashes, and
   alternating dots and dashes.

   @reviewed on 2019-10-13
*/
int legacy_api_test_iambic_key_none(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
#ifndef LIBCW_KEY_TESTS_WORKAROUND
	legacy_api_standalone_test_setup(cte, true);
#endif

	/*
	  Test: set new state of paddles: no paddle pressed.

	  This is test of legacy API, so use true/false instead of
	  CW_KEY_STATE_CLOSED (true)/CW_KEY_STATE_OPEN (false).
	*/
	const int intended_dot_paddle = false;
	const int intended_dash_paddle = false;

	/* Test: depress paddles. */
	{
		const int cwret = LIBCW_TEST_FUT(cw_notify_keyer_paddle_event)(intended_dot_paddle, intended_dot_paddle);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_notify_keyer_paddle_event(%d, %d)", intended_dot_paddle, intended_dash_paddle);
	}

	/* Test: preserving of paddle states. */
	{
		/* State of paddles should be the same as after call
		   to cw_notify_keyer_paddle_event() above. */
		int read_back_dot_paddle;
		int read_back_dash_paddle;
		LIBCW_TEST_FUT(cw_get_keyer_paddles)(&read_back_dot_paddle, &read_back_dash_paddle);
		cte->expect_op_int(cte, intended_dot_paddle, "==", read_back_dot_paddle, "cw_get_keyer_paddles(): dot paddle");
		cte->expect_op_int(cte, intended_dash_paddle, "==", read_back_dash_paddle, "cw_get_keyer_paddles(): dash paddle");
	}
	cw_wait_for_keyer();

#ifndef LIBCW_KEY_TESTS_WORKAROUND
	legacy_api_standalone_test_teardown(cte);
#endif
	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-13
*/
int legacy_api_test_straight_key(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
#ifndef LIBCW_KEY_TESTS_WORKAROUND
	legacy_api_standalone_test_setup(cte, true);
#endif

	{
		bool event_failure = false;
		bool state_failure = false;
		bool busy_failure = false;

		const int key_states[] = { CW_KEY_STATE_OPEN, CW_KEY_STATE_CLOSED };
		const int first = 1 + (lrand48() % 2);
		const int last = first + cte->get_repetitions_count(cte) + (1 + (lrand48() % 2));
		cte->log_info(cte, "Randomized key indices range: from %d to %d\n", first, last);

		/* Alternate between open and closed. */
		for (int i = first; i <= last; i++) {

			const int intended_key_state = key_states[i % 2]; /* Notice that depending on lrand48(), we may start with key open or key closed. */

			const int cwret = LIBCW_TEST_FUT(cw_notify_straight_key_event)(intended_key_state);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "cw_notify_straight_key_event(%d)", intended_key_state)) {
				event_failure = true;
				break;
			}

			const int readback_key_state = LIBCW_TEST_FUT(cw_get_straight_key_state)();
			if (!cte->expect_op_int_errors_only(cte, intended_key_state, "==", readback_key_state, "cw_get_straight_key_state() (%d)", intended_key_state)) {
				state_failure = true;
				break;
			}

			/* "busy" is misleading. This function just asks if key is down. */
			const bool is_busy = LIBCW_TEST_FUT(cw_is_straight_key_busy)();
			const bool expected_is_busy = intended_key_state == CW_KEY_STATE_CLOSED;
			if (!cte->expect_op_int_errors_only(cte, expected_is_busy, "==", is_busy, "cw_is_straight_key_busy() (%d)", intended_key_state)) {
				busy_failure = true;
				break;
			}

			cte->log_info_cont(cte, "%d", intended_key_state);
			cte->flush_info(cte);
#ifdef __FreeBSD__
			/* There is a problem with nanosleep() and
			   signals on FreeBSD. TODO: see if the
			   problem still persists after moving from
			   signals to conditional variables. */
			sleep(1);
#else
			const int usecs = CW_USECS_PER_SEC;
			cw_usleep_internal(usecs);
#endif
		}

		/* Always make the key open after the tests. */
		cw_notify_straight_key_event(CW_KEY_STATE_OPEN);

		cte->log_info_cont(cte, "\n");
		cte->flush_info(cte);

		cte->expect_op_int(cte, false, "==", event_failure, "cw_notify_straight_key_event(<key open/closed>)");
		cte->expect_op_int(cte, false, "==", state_failure, "cw_get_straight_key_state()");
		cte->expect_op_int(cte, false, "==", busy_failure, "cw_is_straight_key_busy()");
	}

	sleep(1);

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return 0;
}




# if 0
/*
 * cw_test_delayed_release()
 */
void cw_test_delayed_release(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);


	int failures = 0;
	struct timeval start, finish;
	int is_released, delay;

	/* This is slightly tricky to detect, but circumstantial
	   evidence is provided by SIGALRM disposition returning to SIG_DFL. */
	if (!cw_send_character_space()) {
		cte->log_error(cte, "cw_send_character_space()\n");
		failures++;
	}

	if (gettimeofday(&start, NULL) != 0) {
		cte->log_error(cte, "gettimeofday failed, test incomplete\n");
		return;
	}
	cte->log_info(cte, "waiting for cw_finalization delayed release");
	cte->flush_info(cte);
	do {
		struct sigaction disposition;

		sleep(1);
		if (sigaction(SIGALRM, NULL, &disposition) != 0) {
			cte->log_error(cte, "sigaction failed, test incomplete\n");
			return;
		}
		is_released = disposition.sa_handler == SIG_DFL;

		if (gettimeofday(&finish, NULL) != 0) {
			cte->log_error(cte, "gettimeofday failed, test incomplete\n");
			return;
		}

		delay = (finish.tv_sec - start.tv_sec) * 1000000 + finish.tv_usec
			- start.tv_usec;
		cte->log_info_cont(cte, ".");
		cte->flush_info(cte);
	}
	while (!is_released && delay < 20000000) {
		;
	}
	cte->log_info_cont(cte, "\n");

	/* The release should be around 10 seconds after the end of
	   the sent space.  A timeout or two might leak in, reducing
	   it by a bit; we'll be ecstatic with more than five
	   seconds. */
	if (is_released) {
		cte->log_info(cte, "cw_finalization delayed release after %d usecs\n", delay);
		if (delay < 5000000) {
			cte->log_error(cte, "cw_finalization release too quick\n");
			failures++;
		}
	} else {
		cte->log_error(cte, "cw_finalization release wait timed out\n");
		failures++;
	}


	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return;
}





/*
 * cw_test_signal_handling_callback()
 * cw_test_signal_handling()
 */
static int cw_test_signal_handling_callback_called = false;
void cw_test_signal_handling_callback(int signal_number)
{
	signal_number = 0;
	cw_test_signal_handling_callback_called = true;
}





void cw_test_signal_handling(cw_test_executor_t * cte)
{
	int failures = 0;
	struct sigaction action, disposition;

	/* Test registering, unregistering, and raising SIGUSR1.
	   SIG_IGN and handlers are tested, but not SIG_DFL, because
	   that stops the process. */
	if (cw_unregister_signal_handler(SIGUSR1)) {
		cte->log_error(cte, "cw_unregister_signal_handler invalid\n");
		failures++;
	}

	if (!cw_register_signal_handler(SIGUSR1,
                                   cw_test_signal_handling_callback)) {
		cte->log_error(cte, "cw_register_signal_handler failed\n");
		failures++;
	}

	cw_test_signal_handling_callback_called = false;
	raise(SIGUSR1);
	sleep(1);
	if (!cw_test_signal_handling_callback_called) {
		cte->log_error(cte, "cw_test_signal_handling_callback missed\n");
		failures++;
	}

	if (!cw_register_signal_handler(SIGUSR1, SIG_IGN)) {
		cte->log_error(cte, "cw_register_signal_handler (overwrite) failed\n");
		failures++;
	}

	cw_test_signal_handling_callback_called = false;
	raise(SIGUSR1);
	sleep(1);
	if (cw_test_signal_handling_callback_called) {
		cte->log_error(cte, "cw_test_signal_handling_callback called\n");
		failures++;
	}

	if (!cw_unregister_signal_handler(SIGUSR1)) {
		cte->log_error(cte, "cw_unregister_signal_handler failed\n");
		failures++;
	}

	if (cw_unregister_signal_handler(SIGUSR1)) {
		cte->log_error(cte, "cw_unregister_signal_handler invalid\n");
		failures++;
	}

	action.sa_handler = cw_test_signal_handling_callback;
	action.sa_flags = SA_RESTART;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGUSR1, &action, &disposition) != 0) {
		cte->log_error(cte, "sigaction failed, test incomplete\n");
		return failures;
	}
	if (cw_register_signal_handler(SIGUSR1, SIG_IGN)) {
		cte->log_error(cte, "cw_register_signal_handler clobbered\n");
		failures++;
	}
	if (sigaction(SIGUSR1, &disposition, NULL) != 0) {
		cte->log_error(cte, "WARNING: sigaction failed, test incomplete\n");
		return failures;
	}

	cte->log_info(cte, "cw_[un]register_signal_handler tests complete\n");
	return;
}
#endif




/**
   @reviewed on 2019-10-13
*/
int legacy_api_test_basic_gen_operations(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
#if 0
	/* We don't call it here because generator is not created
	   yet. Setup is handled by test code below.. */
	legacy_api_standalone_test_setup(cte, true);
#endif


	int cwret = CW_FAILURE;

	/* Test setting up generator. */
	{
		cwret = LIBCW_TEST_FUT(cw_generator_new)(cte->current_sound_system, cte->current_sound_device);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_generator_new()");
		if (cwret != CW_SUCCESS) {
			return -1;
		}

		cw_reset_send_receive_parameters();

		cwret = LIBCW_TEST_FUT(cw_set_send_speed)(12);
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_set_send_speed()");

		cwret = LIBCW_TEST_FUT(cw_generator_start)();
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_generator_start()");
	}

	/* Test using generator. */
	{
		cwret = LIBCW_TEST_FUT(cw_send_string)("one ");
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_send_string()");

		cwret = LIBCW_TEST_FUT(cw_wait_for_tone_queue)();
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_wait_for_tone_queue()");

		cwret = LIBCW_TEST_FUT(cw_send_string)("two");
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_send_string()");

		cwret = LIBCW_TEST_FUT(cw_wait_for_tone_queue)();
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_wait_for_tone_queue()");

		cwret = LIBCW_TEST_FUT(cw_send_string)("three");
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_send_string()");

		cwret = LIBCW_TEST_FUT(cw_wait_for_tone_queue)();
		cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "cw_wait_for_tone_queue()");
	}

	/* Deconfigure generator. These functions don't return a
	   value, so we can't verify anything. */
	{
		LIBCW_TEST_FUT(cw_generator_stop)();
		LIBCW_TEST_FUT(cw_generator_delete)();
	}


	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @brief Test removing a character from end of enqueued characters

   @reviewed on 2020-08-24
*/
cwt_retv legacy_api_test_gen_remove_last_character(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, "%s", __func__);
	legacy_api_standalone_test_setup(cte, true);

	const int n = 4;
	bool failure = false;
	for (int to_remove = 0; to_remove <= n; to_remove++) {

		cte->log_info(cte, "You will now hear 'oooo' followed by %d 's' characters\n", n - to_remove);
		cw_send_string("oooo" "ssss");

		/* Remove N characters from end. */
		for (int i = 0; i < to_remove; i++) {
			cw_ret_t cwret = LIBCW_TEST_FUT(cw_generator_remove_last_character());
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret,
							    "remove last %d characters, removing %d-th character",
							    to_remove, i)) {
				failure = true;
				break;
			}
		}

		cw_wait_for_tone_queue();

		/* TODO: this sleep should be randomized from 0 to 1000 *
		   1000 microseconds, to detect e.g. problems with ALSA
		   underruns. I have noticed that when this sleep is small or
		   zero, there is no underrun, but with 10^6 us delay an
		   underrun may happen, because there is a long period in
		   which we don't supply new frames to HW.

		   Some cases of buffer underrun have been fixed with calls
		   to snd_pcm_drain(), but perhaps not all of them. */
		cw_usleep_internal(1000 * 1000);

		if (failure) {
			break;
		}
	}

	cte->expect_op_int(cte, false, "==", failure, "remove last character");

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}

