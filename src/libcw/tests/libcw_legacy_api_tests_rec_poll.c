/*
   Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
   Copyright (C) 2011-2020  Kamil Ignacak (acerion@wp.pl)

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




/**
   Used to determine size of input data and of buffer for received
   (polled from receiver) characters.
*/
#define REC_TEST_BUFFER_SIZE 4096




/* TODO: move this type to libcw_rec.h and use it to pass arguments to
   functions such as cw_rec_poll_representation_ics_internal(). */
typedef struct received_data {
	char character;
	char representation[20];
	bool is_iws; /* Is receiver in 'found inter-word-space' state? */
	bool is_error;
} received_data;




typedef struct tester_t {

	/* Whether generating timed events for receiver by test code
	   is in progress. */
	bool generating_in_progress;

	pthread_t receiver_test_code_thread_id;

	char input_string[REC_TEST_BUFFER_SIZE];

	/* Iterator to the array above. */
	size_t input_string_i;

	/* Array large enough to contain characters received (polled)
	   correctly and possible additional characters received
	   incorrectly. */
	char received_string[10 * REC_TEST_BUFFER_SIZE];

	/* Iterator to the array above. */
	size_t received_string_i;

	cw_gen_t * gen;
	cw_key_t key;

	cwtest_param_ranger_t speed_ranger;


	/* Input variable for the test. Decreasing or increasing
	   decides how many characters are enqueued with the same
	   speed S1. Next batch of characters will be enqueued with
	   another speed S2. Depending on how long it will take to
	   dequeue this batch, the difference between S2 and S1 may be
	   significant and this will throw receiver off. */
	int characters_to_enqueue;

} tester_t;

static tester_t g_tester;


typedef struct Receiver {
	cw_test_executor_t * cte;


	/* Timer for measuring length of dots and dashes.

	   Initial value of the timestamp is created by
	   xcwcp's receiver on first "paddle down" event in a
	   character. The timestamp is then updated by libcw
	   on specific time intervals. The intervals are a
	   function of keyboard key presses or mouse button
	   presses recorded by xcwcp. */
	struct timeval main_timer;


	/* Flag indicating if receive polling has received a
	   character, and may need to augment it with a word
	   space on a later poll. */
	volatile bool is_pending_inter_word_space;

	/* Flag indicating possible receive errno detected in
	   signal handler context and needing to be passed to
	   the foreground. */
	volatile int libcw_receive_errno;

	/* Safety flag to ensure that we keep the library in
	   sync with keyer events.  Without, there's a chance
	   that of a on-off event, one half will go to one
	   application instance, and the other to another
	   instance. */
	volatile bool tracked_key_state;

	bool c_r;
} Receiver;

static Receiver g_xcwcp_receiver;




/* Callback. */
void xcwcp_handle_libcw_keying_event(void * timer, int key_state);
void low_tone_queue_callback(void * arg);

static cwt_retv legacy_api_test_rec_poll_inner(cw_test_executor_t * cte, bool c_r);
void * receiver_input_generator_fn(void * arg);
void receiver_sk_event(Receiver * xcwcp_receiver, bool is_down);

/* Main poll function and its helpers. */
void receiver_poll_receiver(Receiver * xcwcp_receiver);
void receiver_poll_report_error(Receiver * xcwcp_receiver);
void receiver_poll_character_c_r(Receiver * xcwcp_receiver);
void receiver_poll_character_r_c(Receiver * xcwcp_receiver);
void receiver_poll_space_c_r(Receiver * xcwcp_receiver);
void receiver_poll_space_r_c(Receiver * xcwcp_receiver);

void test_callback_func(void * arg, int key_state);
void tester_start_test_code(tester_t * tester);
void tester_stop_test_code(tester_t * tester);

void tester_compare_text_buffers(tester_t * tester);
void tester_prepare_input_text_buffer(tester_t * tester);




/**
   \brief Poll the CW library receive buffer and handle anything found
   in the buffer
*/
void receiver_poll_receiver(Receiver * xcwcp_receiver)
{
	if (xcwcp_receiver->libcw_receive_errno != 0) {
		receiver_poll_report_error(xcwcp_receiver);
	}

	if (xcwcp_receiver->is_pending_inter_word_space) {
		/* Check if receiver received the pending inter-word-space. */
		if (xcwcp_receiver->c_r) {
			/* Poll character, then poll representation. */
			receiver_poll_space_c_r(xcwcp_receiver);
		} else {
			/* First poll representation, then character. */
			receiver_poll_space_r_c(xcwcp_receiver);
		}

		if (!xcwcp_receiver->is_pending_inter_word_space) {
			/* We received the pending space. After it the
			   receiver may have received another
			   character.  Try to get it too. */
			if (xcwcp_receiver->c_r) {
				/* Poll character, then poll representation. */
				receiver_poll_character_c_r(xcwcp_receiver);
			} else {
				receiver_poll_character_r_c(xcwcp_receiver);
			}
		}
	} else {
		/* Not awaiting a possible space, so just poll the
		   next possible received character. */
		if (xcwcp_receiver->c_r) {
			/* Poll character, then poll representation. */
			receiver_poll_character_c_r(xcwcp_receiver);
		} else {
			receiver_poll_character_r_c(xcwcp_receiver);
		}
	}

	return;
}




/**
   \brief Handler for the keying callback from the CW library
   indicating that the state of a key has changed.

   The "key" is libcw's internal key structure. It's state is updated
   by libcw when e.g. one iambic keyer paddle is constantly
   pressed. It is also updated in other situations. In any case: the
   function is called whenever state of this key changes.

   Notice that the description above talks about a key, not about a
   receiver. Key's states need to be interpreted by receiver, which is
   a separate task. Key and receiver are separate concepts. This
   function connects them.

   This function, called on key state changes, calls receiver
   functions to ensure that receiver does "receive" the key state
   changes.

   This function is called in signal handler context, and it takes
   care to call only functions that are safe within that context.  In
   particular, it goes out of its way to deliver results by setting
   flags that are later handled by receive polling.
*/
void xcwcp_handle_libcw_keying_event(void * timer, int key_state)
{
	struct timeval *t = (struct timeval *) timer;
	/* Ignore calls where the key state matches our tracked key
	   state.  This avoids possible problems where this event
	   handler is redirected between application instances; we
	   might receive an end of tone without seeing the start of
	   tone. */
	if (key_state == g_xcwcp_receiver.tracked_key_state) {
		//fprintf(stderr, "tracked key state == %d\n", g_xcwcp_receiver.tracked_key_state);
		return;
	} else {
		//fprintf(stderr, "tracked key state := %d\n", key_state);
		g_xcwcp_receiver.tracked_key_state = key_state;
	}

	/* If this is a tone start and we're awaiting an inter-word-space,
	   cancel that wait and clear the receive buffer. */
	if (key_state && g_xcwcp_receiver.is_pending_inter_word_space) {
		/* Tell receiver to prepare (to make space) for
		   receiving new character. */
		cw_clear_receive_buffer();

		/* The tone start means that we're seeing the next
		   incoming character within the same word, so no
		   inter-word-space is possible at this point in
		   time. The space that we were observing/waiting for,
		   was just inter-character-space. */
		g_xcwcp_receiver.is_pending_inter_word_space = false;
	}

	//fprintf(stderr, "calling callback, stage 2\n");

	/* Pass tone state on to the library.  For tone end, check to
	   see if the library has registered any receive error. */
	if (key_state) {
		/* Key down. */
		//fprintf(stderr, "start receive tone: %10ld . %10ld\n", t->tv_sec, t->tv_usec);
		if (!cw_start_receive_tone(t)) {
			perror("cw_start_receive_tone");
			abort();
		}
	} else {
		/* Key up. */
		//fprintf(stderr, "end receive tone:   %10ld . %10ld\n", t->tv_sec, t->tv_usec);
		if (!cw_end_receive_tone(t)) {
			/* Handle receive error detected on tone end.
			   For ENOMEM and ENOENT we set the error in a
			   class flag, and display the appropriate
			   message on the next receive poll. */
			switch (errno) {
			case EAGAIN:
				/* libcw treated the tone as noise (it
				   was shorter than noise threshold).
				   No problem, not an error. */
				break;
			case ENOMEM:
			case ERANGE:
			case EINVAL:
			case ENOENT:
				g_xcwcp_receiver.libcw_receive_errno = errno;
				cw_clear_receive_buffer();
				break;
			default:
				perror("cw_end_receive_tone");
				abort();
			}
		}
	}

	return;
}




/**
   \brief Handle any error registered when handling a libcw keying event
*/
void receiver_poll_report_error(Receiver * xcwcp_receiver)
{
	xcwcp_receiver->libcw_receive_errno = 0;

	return;
}




/**
   \brief Receive any new character from the CW library.

   The function is called c_r because primary function in production code
   polls character, and only then in test code a representation is polled.

   @reviewed 2020-08-27
*/
void receiver_poll_character_c_r(Receiver * xcwcp_receiver)
{
	/* Don't use xcwcp_receiver.main_timer - it is used exclusively for
	   marking initial "key down" events. Use local throw-away
	   local_timer.

	   Additionally using xcwcp_receiver.main_timer here would mess up
	   time intervals measured by xcwcp_receiver.main_timer, and that
	   would interfere with recognizing dots and dashes. */
	struct timeval local_timer;
	gettimeofday(&local_timer, NULL);
	//fprintf(stderr, "poll_receive_char:  %10ld : %10ld\n", local_timer.tv_sec, local_timer.tv_usec);


	const bool debug_errnos = false;
	static int prev_errno = 0;

	received_data prod = { 0 };
	cw_ret_t cwret = LIBCW_TEST_FUT(cw_receive_character)(&local_timer, &prod.character, &prod.is_iws, NULL);
	if (CW_SUCCESS == cwret) {

		prev_errno = 0;

		/* Receiver stores full, well formed  character. Display it. */
		fprintf(stderr, "[II] Polled character '%c'\n", prod.character);

		{ /* Verification code. */

			bool failure = false;
			received_data test = { 0 };
			cw_test_executor_t * cte = xcwcp_receiver->cte;

			cwret = cw_receive_representation(&local_timer, test.representation, &test.is_iws, &test.is_error);
			if (!cte->expect_op_int_errors_only(cte,
							    cwret, "==", CW_SUCCESS,
							    "Test poll representation (c_r)")) {
				failure = true;
			}

			if (!cte->expect_op_int_errors_only(cte,
							    test.is_iws, "==", prod.is_iws,
							    "Comparing 'is inter-word-space' flags: %d, %d",
							    test.is_iws, prod.is_iws)) {
				failure = true;
			}

			/* We are polling a character here, so we expect that
			   receiver will set 'is inter-word-space' flag to
			   false. */
			if (!cte->expect_op_int_errors_only(cte,
							    test.is_iws, "==", false,
							    "Evaluating 'is inter-word-space' flag")) {
				failure = true;
			}

			test.character = cw_representation_to_character(test.representation);
			if (!cte->expect_op_int_errors_only(cte,
							    0, "!=", test.character,
							    "Lookup character for representation")) {
				failure = true;
			}
			if (!cte->expect_op_int_errors_only(cte,
							    prod.character, "==", test.character,
							    "Compare polled and looked up character: %c, %c",
							    prod.character, test.character)) {
				failure = true;
			}

			fprintf(stderr, "[II] Poll character: %c -> '%s' -> %c\n",
				prod.character, test.representation, test.character);

			cte->expect_op_int(cte,
					   failure, "==", false,
					   "Polling character");

			g_tester.received_string[g_tester.received_string_i++] = prod.character;
		}

		/* A full character has been received. Directly after it
		   comes a space. Either a short inter-character-space
		   followed by another character (in this case we won't
		   display the inter-character-space), or longer
		   inter-word-space - this space we would like to catch and
		   display.

		   Set a flag indicating that next poll may result in
		   inter-word-space. */
		xcwcp_receiver->is_pending_inter_word_space = true;

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
			abort();
		}
	}

	return;
}



/**
   \brief Receive any new character from the CW library.

   The function is called r_c because primary function in production code
   polls representation, and only then in test code a character is polled.

   @reviewed 2020-08-27
*/
void receiver_poll_character_r_c(Receiver * xcwcp_receiver)
{
	/* Don't use xcwcp_receiver.main_timer - it is used exclusively for
	   marking initial "key down" events. Use local throw-away
	   local_timer.

	   Additionally using xcwcp_receiver.main_timer here would mess up
	   time intervals measured by xcwcp_receiver.main_timer, and that
	   would interfere with recognizing dots and dashes. */
	struct timeval local_timer;
	gettimeofday(&local_timer, NULL);
	//fprintf(stderr, "poll_receive_char:  %10ld : %10ld\n", local_timer.tv_sec, local_timer.tv_usec);


	const bool debug_errnos = false;
	static int prev_errno = 0;

	received_data prod = { 0 };
	cw_ret_t cwret = LIBCW_TEST_FUT(cw_receive_representation)(&local_timer, prod.representation, &prod.is_iws, &prod.is_error);
	if (CW_SUCCESS == cwret) {

		prev_errno = 0;

		/* Receiver stores full, well formed representation. Display it. */
		fprintf(stderr, "[II] Polled representation '%s'\n", prod.representation);

		{ /* Verification code. */

			bool failure = false;
			received_data test = { 0 };
			cw_test_executor_t * cte = xcwcp_receiver->cte;

			cwret = cw_receive_character(&local_timer, &test.character, &test.is_iws, &test.is_error);
			if (!cte->expect_op_int_errors_only(cte,
							    cwret, "==", CW_SUCCESS,
							    "Test poll character (in r_c)")) {
				failure = true;
			}

			if (!cte->expect_op_int_errors_only(cte,
							    test.is_iws, "==", prod.is_iws,
							    "Comparing 'is inter-word-space' flags: %d, %d",
							    test.is_iws, prod.is_iws)) {
				failure = true;
			}

			/* We are polling a character here, so we expect that
			   receiver will set 'is inter-word-space' flag to
			   false. */
			if (!cte->expect_op_int_errors_only(cte,
							    test.is_iws, "==", false,
							    "Evaluating 'is inter-word-space' flag")) {
				failure = true;
			}

			const char * looked_up_representation = cw_character_to_representation(test.character);
			if (!cte->expect_valid_pointer_errors_only(cte, looked_up_representation,
								   "Lookup representation of character")) {
				failure = true;
			}
			const int cmp = strcmp(prod.representation, looked_up_representation);
			if (!cte->expect_op_int_errors_only(cte,
							    cmp, "==", 0,
							    "Compare polled and looked up representation: '%s', '%s'",
							    prod.representation, looked_up_representation)) {
				failure = true;
			}

			fprintf(stderr, "[II] Poll representation: '%s' -> %c -> '%s'\n", prod.representation, test.character, looked_up_representation);

			cte->expect_op_int(cte, failure, "==", false, "Poll representation");

			g_tester.received_string[g_tester.received_string_i++] = test.character;
		}

		/* A full character has been received. Directly after it
		   comes a space. Either a short inter-character-space
		   followed by another character (in this case we won't
		   display the inter-character-space), or longer
		   inter-word-space - this space we would like to catch and
		   display.

		   Set a flag indicating that next poll may result in
		   inter-word-space. */
		xcwcp_receiver->is_pending_inter_word_space = true;

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
			abort();
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

   @reviewed 2020-08-27
*/
void receiver_poll_space_c_r(Receiver * xcwcp_receiver)
{
	/* Recheck the receive buffer for end of word. */

	/* We expect the receiver to contain a character, but we don't
	   ask for it this time. The receiver should also store
	   information about an inter-character-space. If it is longer
	   than a regular inter-character-space, then the receiver
	   will treat it as inter-word-space, and communicate it over
	   is_iws.

	   Don't use xcwcp_receiver.main_timer - it is used eclusively for
	   marking initial "key down" events. Use local throw-away
	   local_timer. */
	struct timeval local_timer;
	gettimeofday(&local_timer, NULL);
	//fprintf(stderr, "receiver_poll_space(): %10ld : %10ld\n", local_timer.tv_sec, local_timer.tv_usec);

	received_data prod = { 0 };
	LIBCW_TEST_FUT(cw_receive_character)(&local_timer, NULL, &prod.is_iws, NULL);
	if (prod.is_iws) {
		fprintf(stderr, "[II] Polled inter-word-space\n");

		{ /* Verification code. */

			bool failure = false;
			received_data test = { 0 };
			cw_test_executor_t * cte = xcwcp_receiver->cte;
#if 0
			/* cw_receive_character() will return through
			   'c' variable the last character that was
			   polled before space.

			   Maybe this is good, maybe this is bad, but
			   this is the legacy behaviour that we will
			   keep supporting. */
			if (!cte->expect_op_int_errors_only(cte, c, "!=", ' ', "returned character should not be space")) {
				failure = true;
			}
#endif

			cw_ret_t cwret = cw_receive_representation(&local_timer, test.representation, &test.is_iws, &test.is_error);
			if (!cte->expect_op_int_errors_only(cte,
							    cwret, "==", CW_SUCCESS,
							    "Getting representation during space")) {
				failure = true;
			}

			if (!cte->expect_op_int_errors_only(cte,
							    test.is_iws, "==", prod.is_iws,
							    "Comparing 'is inter-word-space' flags: %d, %d",
							    test.is_iws, prod.is_iws)) {
				failure = true;
			}

			/* We are polling ' ' space here, so we expect that
			   receiver will set 'is inter-word-space' flag to
			   true. */
			if (!cte->expect_op_int_errors_only(cte,
							    true, "==", test.is_iws,
							    "Evaluating 'is inter-word-space' flag")) {
				failure = true;
			}

			cte->expect_op_int(cte,
					   false, "==", failure,
					   "Polling inter-word-space");

			g_tester.received_string[g_tester.received_string_i++] = ' ';
		}

		cw_clear_receive_buffer();
		xcwcp_receiver->is_pending_inter_word_space = false;
	} else {
		/* We don't reset is_pending_inter_word_space. The
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

   @reviewed 2020-08-27
*/
void receiver_poll_space_r_c(Receiver * xcwcp_receiver)
{
	/* Recheck the receive buffer for end of word. */

	/* We expect the receiver to contain a character, but we don't
	   ask for it this time. The receiver should also store
	   information about an inter-character-space. If it is longer
	   than a regular inter-character-space, then the receiver
	   will treat it as inter-word-space, and communicate it over
	   is_iws.

	   Don't use xcwcp_receiver.main_timer - it is used eclusively for
	   marking initial "key down" events. Use local throw-away
	   local_timer. */
	struct timeval local_timer;
	gettimeofday(&local_timer, NULL);
	//fprintf(stderr, "receiver_poll_space_r_c(): %10ld : %10ld\n", local_timer.tv_sec, local_timer.tv_usec);


	received_data prod = { 0 };
	LIBCW_TEST_FUT(cw_receive_representation)(&local_timer, prod.representation, &prod.is_iws, NULL);
	if (prod.is_iws) {
		fprintf(stderr, "[II] Polled inter-word-space\n");

		{ /* Verification code. */

			bool failure = false;
			received_data test = { 0 };
			cw_test_executor_t * cte = xcwcp_receiver->cte;
#if 0
			/* cw_receive_character() will return through
			   'c' variable the last character that was
			   polled before space.

			   Maybe this is good, maybe this is bad, but
			   this is the legacy behaviour that we will
			   keep supporting. */
			if (!cte->expect_op_int_errors_only(cte, c, "!=", ' ', "returned character should not be space")) {
				failure = true;
			}
#endif

			cw_ret_t cwret = cw_receive_character(&local_timer, &test.character, &test.is_iws, &test.is_error);
			if (!cte->expect_op_int_errors_only(cte,
							    cwret, "==", CW_SUCCESS,
							    "Getting character during space")) {
				failure = true;
			}

			if (!cte->expect_op_int_errors_only(cte,
							    test.is_iws, "==", prod.is_iws,
							    "Comparing 'is inter-word-space' flags: %d, %d",
							    test.is_iws, prod.is_iws)) {
				failure = true;
			}

			/* We are polling ' ' space here, so we expect that
			   receiver will set 'is inter-word-space' flag to
			   true. */
			if (!cte->expect_op_int_errors_only(cte,
							    true, "==", test.is_iws,
							    "Evaluating 'is inter-word-space' flag")) {
				failure = true;
			}

			cte->expect_op_int(cte,
					   false, "==", failure,
					   "Polling inter-word-space");

			g_tester.received_string[g_tester.received_string_i++] = ' ';
		}

		cw_clear_receive_buffer();
		xcwcp_receiver->is_pending_inter_word_space = false;
	} else {
		/* We don't reset is_pending_inter_word_space. The
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




void receiver_sk_event(Receiver * xcwcp_receiver, bool is_down)
{
	gettimeofday(&xcwcp_receiver->main_timer, NULL);
	//fprintf(stderr, "time on Skey down:  %10ld : %10ld\n", xcwcp_receiver->main_timer.tv_sec, xcwcp_receiver->main_timer.tv_usec);

	cw_notify_straight_key_event(is_down);

	return;
}




/*
  Code that generates info about timing of input events for receiver.

  We could generate the info and the events using a big array of
  timestamps and a call to usleep(), but instead we are using a new
  generator that can inform us when marks/spaces start.
*/
void * receiver_input_generator_fn(void * arg)
{
	tester_t * tester = arg;

	/* Start sending the test string. Registered callback will be
	   called on every mark/space. Enqueue only initial part of
	   string, just to start sending, the rest should be sent by
	   'low watermark' callback. */
	cw_gen_start(tester->gen);
	for (int i = 0; i < 5; i++) {
		const char c = tester->input_string[tester->input_string_i];
		if ('\0' == c) {
			/* A very short input string. */
			break;
		} else {
			cw_gen_enqueue_character(tester->gen, c);
			tester->input_string_i++;
		}
	}

	/* Wait for all characters to be played out. */
	cw_tq_wait_for_level_internal(tester->gen->tq, 0);
	cw_usleep_internal(1000 * 1000);

	cw_gen_delete(&tester->gen);
	tester->generating_in_progress = false;


	tester_compare_text_buffers(tester);


	return NULL;
}




void tester_start_test_code(tester_t * tester)
{
	tester->generating_in_progress = true;

	tester_prepare_input_text_buffer(tester);

	/* Using Null sound system because this generator is only used
	   to enqueue text and control key. Sound will be played by
	   main generator used by xcwcp */
	tester->gen = cw_gen_new(CW_AUDIO_NULL, NULL);
	cw_tq_register_low_level_callback_internal(tester->gen->tq, low_tone_queue_callback, tester, 5);

	cw_key_register_generator(&tester->key, tester->gen);
	cw_gen_register_value_tracking_callback_internal(tester->gen, test_callback_func, &g_xcwcp_receiver);


	/* TODO: use full range of allowed speeds. */
	cwtest_param_ranger_init(&tester->speed_ranger, 6 /* CW_SPEED_MIN */, 40 /* CW_SPEED_MAX */, 1, cw_gen_get_speed(tester->gen));
	cwtest_param_ranger_set_interval_sec(&tester->speed_ranger, 4);
	cwtest_param_ranger_set_plateau_length(&tester->speed_ranger, 6);


	pthread_create(&tester->receiver_test_code_thread_id, NULL, receiver_input_generator_fn, tester);
}




void tester_stop_test_code(tester_t * tester)
{
	pthread_cancel(tester->receiver_test_code_thread_id);
}




void tester_prepare_input_text_buffer(tester_t * tester)
{
	/* TODO: generate the text randomly. */

	/* Long text for longer tests. */
#define BASIC_SET_LONG \
	"the quick brown fox jumps over the lazy dog. 01234567890 paris paris paris "    \
	"abcdefghijklmnopqrstuvwxyz0123456789\"'$()+,-./:;=?_@<>!&^~ paris paris paris " \
	"one two three four five six seven eight nine ten eleven paris paris paris "

	/* Short text for occasions where I need a quick test. */
#define BASIC_SET_SHORT "one two three four paris"

#if 1
	const char input[REC_TEST_BUFFER_SIZE] = BASIC_SET_LONG BASIC_SET_LONG;
#else
	const char input[REC_TEST_BUFFER_SIZE] = BASIC_SET_SHORT;
#endif
	snprintf(tester->input_string, sizeof (tester->input_string), "%s", input);

	return;
}




/* Compare buffers with text that was sent to test generator and text
   that was received from tested production receiver.

   Compare input text with what the receiver received. */
void tester_compare_text_buffers(tester_t * tester)
{
	/* Luckily for us the text enqueued in test generator and
	   played at ~12WPM is recognized by xcwcp receiver from the
	   beginning without any problems, so we will be able to do
	   simple strcmp(). */

	/* Use multiple newlines to clearly present sent and received
	   string. It will be easier to do visual comparison of the
	   two strings if they are presented that way. */

	fprintf(stderr, "[II] Sent:     \n\n'%s'\n\n", tester->input_string);
	fprintf(stderr, "[II] Received: \n\n'%s'\n\n", tester->received_string);

	/* Trim input string. */
	if (1) {
		const size_t len = strlen(tester->input_string);
		size_t i = len - 1;
		while (tester->input_string[i] == ' ') {
			tester->input_string[i] = '\0';
			i--;
		}
	}

	/* Normalize and trim received string. */
	{
		const size_t len = strlen(tester->received_string);
		for (size_t i = 0; i < len; i++) {
			tester->received_string[i] = tolower(tester->received_string[i]);
		}

		size_t i = len - 1;
		while (tester->received_string[i] == ' ') {
			tester->received_string[i] = '\0';
			i--;
		}
	}


	const int compare_result = strcmp(tester->input_string, tester->received_string);
	if (g_xcwcp_receiver.cte->expect_op_int(g_xcwcp_receiver.cte, 0, "==", compare_result, "Final comparison of sent and received strings")) {
		fprintf(stderr, "[II] Test result: success\n");
	} else {
		fprintf(stderr, "[EE] Test result: failure\n");
		fprintf(stderr, "[EE] '%s' != '%s'\n", tester->input_string, tester->received_string);

		fprintf(stderr, "\n");

		/* Show exactly where the difference is */
		const size_t len_in = strlen(tester->input_string);
		const size_t len_rec = strlen(tester->received_string);
		const int diffs_to_show_max = 5;

		fprintf(stderr, "[EE] Printing up to %d first differences\n", diffs_to_show_max);
		int reported = 0;
		for (size_t i = 0; i < len_in && i < len_rec; i++) {
			if (tester->input_string[i] != tester->received_string[i]) {
				fprintf(stderr, "[EE] char %6zd: input %4d (%c) vs. received %4d (%c)\n",
					i,
					(int) tester->input_string[i], (int) tester->input_string[i],
					tester->received_string[i], tester->received_string[i]);
				reported++;
			}
			if (reported == diffs_to_show_max) {
				/* Don't print them all if there are more of X differences. */
				fprintf(stderr, "[EE] more differences may be present, but not showing them\n");
				break;
			}
		}
		if (0 == reported) {
			/* Because of condition in 'for' loop we might
			   skipped checking end of one of strings. */
			fprintf(stderr, "[EE] difference appears to be at end of one of strings\n");
		}

		fprintf(stderr, "\n");
	}

	return;
}




void test_callback_func(void * arg, int key_state)
{
	/* Inform libcw receiver about new state of straight key ("sk").

	   libcw receiver will process the new state and we will later
	   try to poll a character or space from it. */

	Receiver * xcwcp_receiver = (Receiver *) arg;
	//fprintf(stderr, "Callback function, key state = %d\n", key_state);
	receiver_sk_event(xcwcp_receiver, key_state);
}




/**
   @reviewed 2020-08-27
*/
cwt_retv legacy_api_test_rec_poll(cw_test_executor_t * cte)
{
	if (cwt_retv_ok == legacy_api_test_rec_poll_inner(cte, true)
	    && cwt_retv_ok == legacy_api_test_rec_poll_inner(cte, false)) {

		return cwt_retv_ok;
	} else {
		return cwt_retv_err;
	}
}




static cwt_retv legacy_api_test_rec_poll_inner(cw_test_executor_t * cte, bool c_r)
{
	cte->print_test_header(cte, __func__);

	if (c_r) {
		cte->log_info(cte, "Test mode: poll character, verify by polling representation\n");
	} else {
		cte->log_info(cte, "Test mode: poll representation, verify by polling character\n");
	}

	if (CW_SUCCESS != cw_generator_new(CW_AUDIO_PA, NULL)) {
		fprintf(stderr, "failed to create generator\n");
		return cwt_retv_err;
	}


	/* Configure test parameters. */
	g_tester.characters_to_enqueue = 5;

	/* TODO: more thorough reset of tester. */
	g_tester.input_string_i = 0;
	g_tester.received_string[0] = '\0';
	g_tester.received_string_i = 0;


	cw_clear_receive_buffer();
	cw_set_frequency(cte->config->frequency);
	cw_generator_start();
	cw_enable_adaptive_receive();

	/* Register handler as the CW library keying event callback.

	   The handler called back by libcw is important because it's
	   used to send to libcw information about timings of events
	   (key down and key up events) through xcwcp_receiver.main_timer.

	   Without the callback the library can play sounds as key or
	   paddles are pressed, but (since it doesn't receive timing
	   parameters) it won't be able to identify entered Morse
	   code. */
	cw_register_keying_callback(xcwcp_handle_libcw_keying_event, &g_xcwcp_receiver.main_timer);
	gettimeofday(&g_xcwcp_receiver.main_timer, NULL);
	//fprintf(stderr, "time on aux config: %10ld : %10ld\n", xcwcp_receiver.main_timer.tv_sec, xcwcp_receiver.main_timer.tv_usec);


	/* Prepare xcwcp_receiver object. */
	memset(&g_xcwcp_receiver, 0, sizeof (g_xcwcp_receiver));
	g_xcwcp_receiver.cte = cte;
	g_xcwcp_receiver.c_r = c_r;


	/* Start thread with test code. */
	tester_start_test_code(&g_tester);


	while (g_tester.generating_in_progress) {
		/* At 60WPM, a dot is 20ms, so polling for the maximum speed
		   library needs a 10ms timeout. */
		usleep(10);
		receiver_poll_receiver(&g_xcwcp_receiver);
#if 1
		int new_speed = 0;
		if (cwtest_param_ranger_get_next(&g_tester.speed_ranger, &new_speed)) {
			cw_gen_set_speed(g_tester.gen, new_speed);
		}
#endif
	}

	/* Stop thread with test code.
	   Is this really needed? The thread should already be stopped
	   if we get here. */
	tester_stop_test_code(&g_tester);


	/* Tell legacy objects of libcw (those in production code) to stop working. */
	cw_complete_reset();
	cw_generator_stop();
	cw_generator_delete();


	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




void low_tone_queue_callback(void * arg)
{
	tester_t * tester = (tester_t *) arg;

	for (int i = 0; i < tester->characters_to_enqueue; i++) {
		const char c = tester->input_string[tester->input_string_i];
		if ('\0' == c) {
			/* Unregister ourselves. */
			cw_tq_register_low_level_callback_internal(tester->gen->tq, NULL, NULL, 0);
			break;
		} else {
			cw_gen_enqueue_character(tester->gen, c);
			tester->input_string_i++;
		}
	}

	return;
}
