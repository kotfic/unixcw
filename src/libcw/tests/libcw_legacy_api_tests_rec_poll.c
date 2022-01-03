/*
   Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
   Copyright (C) 2011-2021  Kamil Ignacak (acerion@wp.pl)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "config.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#if defined(HAVE_STRING_H)
# include <string.h> /* FreeBSD 12.1 */
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif

#include <cw_common.h>
#include <cw_rec_tester.h>
#include <cw_rec_utils.h>




#include "libcw.h"
#include "libcw2.h"




#include "libcw_gen.h"
#include "libcw_key.h"
#include "libcw_tq.h"
#include "libcw_utils.h"

#include "test_framework.h"
#include "test_framework_tools.h"
#include "libcw_legacy_api_tests_rec_poll.h"




/**
   Test code for 'poll' method for libcw receiver used in xcwcp.

   xcwcp is implementing a receiver of Morse code keyed with Space or
   Enter keyboard key. Recently I have added a test code to xcwcp
   (from unixcw 3.5.1) that verifies how receive process is working
   and whether it is working correctly.

   Then I have extracted the core part of the xcwcp receiver code and
   the test code, and I have put it here.

   Now we have the test code embedded in xcwcp (so we can always have
   it as 'in vivo' test), and we have the test code here in libcw
   tests set.
*/




static cw_rec_tester_t g_tester;
static cw_easy_receiver_t g_easy_rec;
static cw_test_executor_t * g_cte;




/* Callback. */
void xcwcp_handle_libcw_keying_event(cw_easy_receiver_t * easy_rec, int key_state);

static cwt_retv legacy_api_test_rec_poll_inner(cw_test_executor_t * cte, bool get_representation);


/* Main poll function and its helpers. */
void receiver_poll_receiver(cw_easy_receiver_t * easy_rec);
void receiver_poll_report_error(cw_easy_receiver_t * easy_rec);
void receiver_poll_character_c_r(cw_easy_receiver_t * easy_rec);
void receiver_poll_character_r_c(cw_easy_receiver_t * easy_rec);
void receiver_poll_space_c_r(cw_easy_receiver_t * easy_rec);
void receiver_poll_space_r_c(cw_easy_receiver_t * easy_rec);

static int easy_rec_test_on_character_c_r(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, const struct timeval * timer);
static int easy_rec_test_on_character_r_c(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, const struct timeval * timer);
static int easy_rec_test_on_space_r_c(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, const struct timeval * timer);
static int easy_rec_test_on_space_c_r(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, const struct timeval * timer);




/**
   \brief Poll the CW library receive buffer and handle anything found
   in the buffer
*/
void receiver_poll_receiver(cw_easy_receiver_t * easy_rec)
{
	if (easy_rec->libcw_receive_errno != 0) {
		receiver_poll_report_error(easy_rec);
	}

	if (easy_rec->is_pending_iws) {
		/* Check if receiver received the pending inter-word-space. */
		if (easy_rec->get_representation) {
			/* First poll representation, then character. */
			receiver_poll_space_r_c(easy_rec);
		} else {
			/* Poll character, then poll representation. */
			receiver_poll_space_c_r(easy_rec);
		}

		if (!easy_rec->is_pending_iws) {
			/* We received the pending space. After it the
			   receiver may have received another
			   character.  Try to get it too. */
			if (easy_rec->get_representation) {
				receiver_poll_character_r_c(easy_rec);
			} else {
				/* Poll character, then poll representation. */
				receiver_poll_character_c_r(easy_rec);
			}
		}
	} else {
		/* Not awaiting a possible space, so just poll the
		   next possible received character. */
		if (easy_rec->get_representation) {
			receiver_poll_character_r_c(easy_rec);
		} else {
			/* Poll character, then poll representation. */
			receiver_poll_character_c_r(easy_rec);
		}
	}

	return;
}





/**
   \brief Handle any error registered when handling a libcw keying event
*/
void receiver_poll_report_error(cw_easy_receiver_t * easy_rec)
{
	easy_rec->libcw_receive_errno = 0;

	return;
}




/**
   \brief Receive any new character from the CW library.

   The function is called c_r because primary function in production code
   polls character, and only then in test code a representation is polled.

   @reviewed TODO
*/
void receiver_poll_character_c_r(cw_easy_receiver_t * easy_rec)
{
	/* Don't use easy_rec->main_timer - it is used exclusively for
	   marking initial "key down" events. Use local throw-away
	   local_timer.

	   Additionally using easy_rec->main_timer here would mess up
	   time intervals measured by easy_rec->main_timer, and that
	   would interfere with recognizing dots and dashes. */
	struct timeval local_timer;
	gettimeofday(&local_timer, NULL);
	//fprintf(stderr, "poll_receive_char:  %10ld : %10ld\n", local_timer.tv_sec, local_timer.tv_usec);


	const bool debug_errnos = false;
	static int prev_errno = 0;

	cw_rec_data_t erd = { 0 };
	cw_ret_t cwret = LIBCW_TEST_FUT(cw_receive_character)(&local_timer, &erd.character, &erd.is_iws, NULL);
	if (CW_SUCCESS == cwret) {

		prev_errno = 0;

		/* Receiver stores full, well formed  character. Display it. */
		fprintf(stderr, "[II] Polled character '%c'\n", erd.character);

		/* Verification code. */
		easy_rec_test_on_character_c_r(easy_rec, &erd, &local_timer);

		/* A full character has been received. Directly after it
		   comes a space. Either a short inter-character-space
		   followed by another character (in this case we won't
		   display the inter-character-space), or longer
		   inter-word-space - this space we would like to catch and
		   display.

		   Set a flag indicating that next poll may result in
		   inter-word-space. */
		easy_rec->is_pending_iws = true;

	} else {
		/* Handle receive error detected on trying to read a character. */
		switch (errno) {
		case EAGAIN:
			/* Call made too early, receiver hasn't
			   received a full character yet. Try next
			   time. */

			if (debug_errnos && prev_errno != EAGAIN) {
				fprintf(stderr, "[NN] poll_receive_char: %d -> EAGAIN\n", prev_errno);
				prev_errno = EAGAIN;
			}
			break;

		case ERANGE:
			/* Call made not in time, or not in proper
			   sequence. Receiver hasn't received any
			   character (yet). Try harder. */
			if (debug_errnos && prev_errno != ERANGE) {
				fprintf(stderr, "[NN] poll_receive_char: %d -> RANGE\n", prev_errno);
				prev_errno = ERANGE;
			}

			break;

		case ENOENT:
			/* Invalid character in receiver's buffer. */
			if (debug_errnos && prev_errno != ENOENT) {
				fprintf(stderr, "[NN] poll_receive_char: %d -> ENONENT\n", prev_errno);
				prev_errno = ENOENT;
			}
			cw_clear_receive_buffer();
			break;

		case EINVAL:
			/* Timestamp error. */
			if (debug_errnos && prev_errno != EINVAL) {
				fprintf(stderr, "[NN] poll_receive_char: %d -> EINVAL\n", prev_errno);
				prev_errno = EINVAL;
			}
			cw_clear_receive_buffer();
			break;

		default:
			perror("cw_receive_character");
			// TODO: Perhaps this should be counted as test error
			return;
		}
	}

	return;
}



/**
   \brief Receive any new character from the CW library.

   The function is called r_c because primary function in production code
   polls representation, and only then in test code a character is polled.

   @reviewed TODO
*/
void receiver_poll_character_r_c(cw_easy_receiver_t * easy_rec)
{
	/* Don't use easy_rec->main_timer - it is used exclusively for
	   marking initial "key down" events. Use local throw-away
	   local_timer.

	   Additionally using easy_rec->main_timer here would mess up
	   time intervals measured by easy_rec->main_timer, and that
	   would interfere with recognizing dots and dashes. */
	struct timeval local_timer;
	gettimeofday(&local_timer, NULL);
	//fprintf(stderr, "poll_receive_char:  %10ld : %10ld\n", local_timer.tv_sec, local_timer.tv_usec);


	const bool debug_errnos = false;
	static int prev_errno = 0;

	cw_rec_data_t erd = { 0 };
	cw_ret_t cwret = LIBCW_TEST_FUT(cw_receive_representation)(&local_timer, erd.representation, &erd.is_iws, &erd.is_error);
	if (CW_SUCCESS == cwret) {

		prev_errno = 0;

		/* Receiver stores full, well formed representation. Display it. */
		fprintf(stderr, "[II] Polled representation '%s'\n", erd.representation);

		/* Verification code. */
		easy_rec_test_on_character_r_c(easy_rec, &erd, &local_timer);

		/* A full character has been received. Directly after it
		   comes a space. Either a short inter-character-space
		   followed by another character (in this case we won't
		   display the inter-character-space), or longer
		   inter-word-space - this space we would like to catch and
		   display.

		   Set a flag indicating that next poll may result in
		   inter-word-space. */
		easy_rec->is_pending_iws = true;

	} else {
		/* Handle receive error detected on trying to read a character. */
		switch (errno) {
		case EAGAIN:
			/* Call made too early, receiver hasn't
			   received a full character yet. Try next
			   time. */

			if (debug_errnos && prev_errno != EAGAIN) {
				fprintf(stderr, "[NN] poll_receive_representation: %d -> EAGAIN\n", prev_errno);
				prev_errno = EAGAIN;
			}
			break;

		case ERANGE:
			/* Call made not in time, or not in proper
			   sequence. Receiver hasn't received any
			   character (yet). Try harder. */
			if (debug_errnos && prev_errno != ERANGE) {
				fprintf(stderr, "[NN] poll_receive_representation: %d -> RANGE\n", prev_errno);
				prev_errno = ERANGE;
			}

			break;

		case EINVAL:
			/* Invalid timestamp. */
			if (debug_errnos && prev_errno != EINVAL) {
				fprintf(stderr, "[NN] poll_receive_representation: %d -> EINVAL\n", prev_errno);
				prev_errno = EINVAL;
			}
			cw_clear_receive_buffer();
			break;

		default:
			perror("cw_receive_representation");
			// TODO: Perhaps this should be counted as test error
			return;
		}
	}

	return;
}




/**
   If we received a character on an earlier poll, check again to see
   if we need to revise the decision about whether it is the end of a
   word too.

   The function is called c_r because primary function in production code
   polls character, and only then in test code a representation is polled.

   @reviewed TODO
*/
void receiver_poll_space_c_r(cw_easy_receiver_t * easy_rec)
{
	/* Recheck the receive buffer for end of word. */

	/* We expect the receiver to contain a character, but we don't
	   ask for it this time. The receiver should also store
	   information about an inter-character-space. If it is longer
	   than a regular inter-character-space, then the receiver
	   will treat it as inter-word-space, and communicate it over
	   is_iws.

	   Don't use easy_rec->main_timer - it is used eclusively for
	   marking initial "key down" events. Use local throw-away
	   local_timer. */
	struct timeval local_timer;
	gettimeofday(&local_timer, NULL);
	//fprintf(stderr, "receiver_poll_space(): %10ld : %10ld\n", local_timer.tv_sec, local_timer.tv_usec);

	cw_rec_data_t erd = { 0 };
	LIBCW_TEST_FUT(cw_receive_character)(&local_timer, NULL, &erd.is_iws, NULL);
	if (erd.is_iws) {
		fprintf(stderr, "[II] Polled inter-word-space\n");

		/* Verification code. */
		easy_rec_test_on_space_c_r(easy_rec, &erd, &local_timer);

		cw_clear_receive_buffer();
		easy_rec->is_pending_iws = false;
	} else {
		/* We don't reset is_pending_iws. The
		   space that currently lasts, and isn't long enough
		   to be considered inter-word-space, may grow to
		   become the inter-word-space. Or not.

		   This growing of inter-character-space into
		   inter-word-space may be terminated by incoming next
		   tone (key down event) - the tone will mark
		   beginning of new character within the same
		   word. And since a new character begins, the flag
		   will be reset (elsewhere). */
	}

	return;
}




/**
   If we received a character on an earlier poll, check again to see
   if we need to revise the decision about whether it is the end of a
   word too.

   The function is called r_c because primary function in production code
   polls representation, and only then in test code a character is polled.

   @reviewed TODO
*/
void receiver_poll_space_r_c(cw_easy_receiver_t * easy_rec)
{
	/* Recheck the receive buffer for end of word. */

	/* We expect the receiver to contain a character, but we don't
	   ask for it this time. The receiver should also store
	   information about an inter-character-space. If it is longer
	   than a regular inter-character-space, then the receiver
	   will treat it as inter-word-space, and communicate it over
	   is_iws.

	   Don't use easy_rec->main_timer - it is used eclusively for
	   marking initial "key down" events. Use local throw-away
	   local_timer. */
	struct timeval local_timer;
	gettimeofday(&local_timer, NULL);
	//fprintf(stderr, "receiver_poll_space_r_c(): %10ld : %10ld\n", local_timer.tv_sec, local_timer.tv_usec);


	cw_rec_data_t erd = { 0 };
	LIBCW_TEST_FUT(cw_receive_representation)(&local_timer, erd.representation, &erd.is_iws, NULL);
	if (erd.is_iws) {
		fprintf(stderr, "[II] Polled inter-word-space\n");

		/* Verification code. */
		easy_rec_test_on_space_r_c(easy_rec, &erd, &local_timer);

		cw_clear_receive_buffer();
		easy_rec->is_pending_iws = false;
	} else {
		/* We don't reset is_pending_iws. The
		   space that currently lasts, and isn't long enough
		   to be considered inter-word-space, may grow to
		   become the inter-word-space. Or not.

		   This growing of inter-character-space into
		   inter-word-space may be terminated by incoming next
		   tone (key down event) - the tone will mark
		   beginning of new character within the same
		   word. And since a new character begins, the flag
		   will be reset (elsewhere). */
	}

	return;
}




/**
   @reviewed 2020-08-27
*/
cwt_retv legacy_api_test_rec_poll(cw_test_executor_t * cte)
{
	const cwt_retv retv1 = legacy_api_test_rec_poll_inner(cte, false);
	const cwt_retv retv2 = legacy_api_test_rec_poll_inner(cte, true);

	if (cwt_retv_ok == retv1 && cwt_retv_ok == retv2) {
		return cwt_retv_ok;
	} else {
		return cwt_retv_err;
	}
}




static cwt_retv legacy_api_test_rec_poll_inner(cw_test_executor_t * cte, bool get_representation)
{
	cte->print_test_header(cte, __func__);

	if (get_representation) {
		cte->log_info(cte, "Test mode: poll representation, verify by polling character\n");
	} else {
		cte->log_info(cte, "Test mode: poll character, verify by polling representation\n");
	}

	if (CW_SUCCESS != cw_generator_new(cte->current_gen_conf.sound_system, cte->current_gen_conf.sound_device)) {
		fprintf(stderr, "failed to create generator\n");
		return cwt_retv_err;
	}


	cw_clear_receive_buffer();
	cw_set_frequency(cte->config->frequency);
	cw_generator_start();
	cw_enable_adaptive_receive();

	/* Register handler as the CW library keying event callback.

	   The handler called back by libcw is important because it's
	   used to send to libcw information about timings of events
	   (key down and key up events) through easy_rec.main_timer.

	   Without the callback the library can play sounds as key or
	   paddles are pressed, but (since it doesn't receive timing
	   parameters) it won't be able to identify entered Morse
	   code. */
	cw_register_keying_callback(cw_easy_receiver_handle_libcw_keying_event, &g_easy_rec);
	gettimeofday(&g_easy_rec.main_timer, NULL);
	//fprintf(stderr, "time on aux config: %10ld : %10ld\n", easy_rec.main_timer.tv_sec, easy_rec.main_timer.tv_usec);


	/* Prepare easy_rec object. */
	memset(&g_easy_rec, 0, sizeof (g_easy_rec));
	g_cte = cte;
	g_easy_rec.get_representation = get_representation;


	cw_rec_tester_init(&g_tester);
	cw_rec_tester_configure(&g_tester, &g_easy_rec, true);
	cw_rec_tester_start_test_code(&g_easy_rec, &g_tester);


	while (g_tester.generating_in_progress) {
		/* At 60WPM, a dot is 20ms, so polling for the maximum speed
		   library needs a 10ms timeout. */
		usleep(10);
		receiver_poll_receiver(&g_easy_rec);
#if 0 /* TODO: restore ranger code. */
		int new_speed = 0;
		if (cwtest_param_ranger_get_next(&g_tester.speed_ranger, &new_speed)) {
			cw_gen_set_speed(g_tester.gen, new_speed);
		}
#endif
	}

	/*
	  Stop thread with test code.

	  TODO: Is this really needed? The thread should already be stopped
	  if we get here. Calling this function leads to this problem in valgrind:

	  ==2402==    by 0x596DEDA: pthread_cancel_init (unwind-forcedunwind.c:52)
	  ==2402==    by 0x596A4EF: pthread_cancel (pthread_cancel.c:38)
	  ==2402==    by 0x1118BE: tester_stop_test_code (libcw_legacy_api_tests_rec_poll.c:958)
	  ==2402==    by 0x1118BE: legacy_api_test_rec_poll_inner (libcw_legacy_api_tests_rec_poll.c:1181)
	  ==2402==    by 0x111C6D: legacy_api_test_rec_poll (libcw_legacy_api_tests_rec_poll.c:1098)
	  ==2402==    by 0x11E1D5: iterate_over_test_objects (test_framework.c:1372)
	  ==2402==    by 0x11E1D5: iterate_over_sound_systems (test_framework.c:1329)
	  ==2402==    by 0x11E1D5: iterate_over_topics (test_framework.c:1306)
	  ==2402==    by 0x11E1D5: cw_test_main_test_loop (test_framework.c:1282)
	  ==2402==    by 0x10E703: main (test_main.c:130)
	*/
	/* TODO: remove this function altogether. */
	//cw_rec_tester_stop_test_code(&g_tester);

	const int receive_correctness = cw_rec_tester_evaluate_receive_correctness(&g_tester);
	if (g_cte->expect_op_int(g_cte, 0, "==", receive_correctness, "Final comparison of receive correctness")) {
		fprintf(stderr, "[II] Test result: success\n");
	} else {
		fprintf(stderr, "[EE] Test result: failure\n");
	}

	/* Tell legacy objects of libcw (those in production code) to stop working. */
	cw_complete_reset();
	cw_generator_stop();
	cw_generator_delete();


	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




static int easy_rec_test_on_character_c_r(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, const struct timeval * timer)
{
	int test_cwret = CW_SUCCESS;
	cw_rec_data_t test = { 0 };
	cw_test_executor_t * cte = g_cte;

	cw_ret_t cwret = cw_receive_representation(timer, test.representation, &test.is_iws, &test.is_error);
	if (!cte->expect_op_int_errors_only(cte,
					    cwret, "==", CW_SUCCESS,
					    "Test poll representation (c_r)")) {
		test_cwret = CW_FAILURE;
	}

	if (!cte->expect_op_int_errors_only(cte,
					    test.is_iws, "==", erd->is_iws,
					    "Comparing 'is inter-word-space' flags: %d, %d",
					    test.is_iws, erd->is_iws)) {
		test_cwret = CW_FAILURE;
	}

	/* We are polling a character here, so we expect that
	   receiver will set 'is inter-word-space' flag to
	   false. */
	if (!cte->expect_op_int_errors_only(cte,
					    test.is_iws, "==", false,
					    "Evaluating 'is inter-word-space' flag")) {
		test_cwret = CW_FAILURE;
	}

	test.character = cw_representation_to_character(test.representation);
	if (!cte->expect_op_int_errors_only(cte,
					    0, "!=", test.character,
					    "Lookup character for representation")) {
		test_cwret = CW_FAILURE;
	}
	if (!cte->expect_op_int_errors_only(cte,
					    erd->character, "==", test.character,
					    "Compare polled and looked up character: %c, %c",
					    erd->character, test.character)) {
		test_cwret = CW_FAILURE;
	}

	fprintf(stderr, "[II] Poll character: %c -> '%s' -> %c\n",
		erd->character, test.representation, test.character);

	cte->expect_op_int(cte,
			   test_cwret, "==", CW_SUCCESS,
			   "Polling character");

	g_tester.received_string[g_tester.received_string_i++] = erd->character;

	return test_cwret;
}





static int easy_rec_test_on_character_r_c(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, const struct timeval * timer)
{
	int test_cwret = CW_SUCCESS;
	cw_rec_data_t test = { 0 };
	cw_test_executor_t * cte = g_cte;

	cw_ret_t cwret = cw_receive_character(timer, &test.character, &test.is_iws, &test.is_error);
	if (!cte->expect_op_int_errors_only(cte,
					    cwret, "==", CW_SUCCESS,
					    "Test poll character (in r_c)")) {
		test_cwret = CW_FAILURE;
	}

	if (!cte->expect_op_int_errors_only(cte,
					    test.is_iws, "==", erd->is_iws,
					    "Comparing 'is inter-word-space' flags: %d, %d",
					    test.is_iws, erd->is_iws)) {
		test_cwret = CW_FAILURE;
	}

	/* We are polling a character here, so we expect that
	   receiver will set 'is inter-word-space' flag to
	   false. */
	if (!cte->expect_op_int_errors_only(cte,
					    test.is_iws, "==", false,
					    "Evaluating 'is inter-word-space' flag")) {
		test_cwret = CW_FAILURE;
	}

	char * looked_up_representation = cw_character_to_representation(test.character);
	if (!cte->expect_valid_pointer_errors_only(cte, looked_up_representation,
						   "Lookup representation of character")) {
		test_cwret = CW_FAILURE;
	}
	const int cmp = strcmp(erd->representation, looked_up_representation);
	if (!cte->expect_op_int_errors_only(cte,
					    cmp, "==", 0,
					    "Compare polled and looked up representation: '%s', '%s'",
					    erd->representation, looked_up_representation)) {
		test_cwret = CW_FAILURE;
	}

	fprintf(stderr, "[II] Poll representation: '%s' -> %c -> '%s'\n", erd->representation, test.character, looked_up_representation);

	cte->expect_op_int(cte,
			   test_cwret, "==", CW_SUCCESS,
			   "Poll representation");

	g_tester.received_string[g_tester.received_string_i++] = test.character;

	free(looked_up_representation);

	return test_cwret;
}




static int easy_rec_test_on_space_r_c(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, const struct timeval * timer)
{
	int test_cwret = CW_SUCCESS;
	cw_rec_data_t test = { 0 };
	cw_test_executor_t * cte = g_cte;
#if 0
	/* cw_receive_character() will return through
	   'c' variable the last character that was
	   polled before space.

	   Maybe this is good, maybe this is bad, but
	   this is the legacy behaviour that we will
	   keep supporting. */
	if (!cte->expect_op_int_errors_only(cte, c, "!=", ' ', "returned character should not be space")) {
		test_cwret = CW_FAILURE;
	}
#endif

	cw_ret_t cwret = cw_receive_character(timer, &test.character, &test.is_iws, &test.is_error);
	if (!cte->expect_op_int_errors_only(cte,
					    cwret, "==", CW_SUCCESS,
					    "Getting character during space")) {
		test_cwret = CW_FAILURE;
	}

	if (!cte->expect_op_int_errors_only(cte,
					    test.is_iws, "==", erd->is_iws,
					    "Comparing 'is inter-word-space' flags: %d, %d",
					    test.is_iws, erd->is_iws)) {
		test_cwret = CW_FAILURE;
	}

	/* We are polling ' ' space here, so we expect that
	   receiver will set 'is inter-word-space' flag to
	   true. */
	if (!cte->expect_op_int_errors_only(cte,
					    true, "==", test.is_iws,
					    "Evaluating 'is inter-word-space' flag")) {
		test_cwret = CW_FAILURE;
	}

	cte->expect_op_int(cte,
			   test_cwret, "==", CW_SUCCESS,
			   "Polling inter-word-space");

	g_tester.received_string[g_tester.received_string_i++] = ' ';

	return test_cwret;
}




static int easy_rec_test_on_space_c_r(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, const struct timeval * timer)
{
	int test_cwret = CW_SUCCESS;
	cw_rec_data_t test = { 0 };
	cw_test_executor_t * cte = g_cte;
#if 0
	/* cw_receive_character() will return through
	   'c' variable the last character that was
	   polled before space.

	   Maybe this is good, maybe this is bad, but
	   this is the legacy behaviour that we will
	   keep supporting. */
	if (!cte->expect_op_int_errors_only(cte, c, "!=", ' ', "returned character should not be space")) {
		test_cwret = CW_FAILURE;
	}
#endif

	cw_ret_t cwret = cw_receive_representation(timer, test.representation, &test.is_iws, &test.is_error);
	if (!cte->expect_op_int_errors_only(cte,
					    cwret, "==", CW_SUCCESS,
					    "Getting representation during space")) {
		test_cwret = CW_FAILURE;
	}

	if (!cte->expect_op_int_errors_only(cte,
					    test.is_iws, "==", erd->is_iws,
					    "Comparing 'is inter-word-space' flags: %d, %d",
					    test.is_iws, erd->is_iws)) {
		test_cwret = CW_FAILURE;
	}

	/* We are polling ' ' space here, so we expect that
	   receiver will set 'is inter-word-space' flag to
	   true. */
	if (!cte->expect_op_int_errors_only(cte,
					    true, "==", test.is_iws,
					    "Evaluating 'is inter-word-space' flag")) {
		test_cwret = CW_FAILURE;
	}

	cte->expect_op_int(cte,
			   CW_SUCCESS, "==", test_cwret,
			   "Polling inter-word-space");

	g_tester.received_string[g_tester.received_string_i++] = ' ';

	return test_cwret;
}


