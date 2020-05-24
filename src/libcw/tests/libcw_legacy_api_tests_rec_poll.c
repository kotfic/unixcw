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

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "libcw.h"
#include "libcw2.h"
//#include "../../libcw/libcw_key.h"
//#include "../../libcw/libcw_gen.h"
//#include "../../libcw/libcw_rec.h"
#include "../../libcw/libcw_utils.h"
#include "test_framework.h"




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




typedef struct Receiver {
	char test_input_string[REC_TEST_BUFFER_SIZE];

	/* Array large enough to contain characters received (polled)
	   correctly and possible additional characters received
	   incorrectly. */
	char test_received_string[10 * REC_TEST_BUFFER_SIZE];

	 /* Iterator to the array above. */
	int test_received_string_i;

	pthread_t receiver_test_code_thread_id;

	/* Whether generating timed events for receiver by test code
	   is in progress. */
	bool generating_in_progress;

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
} Receiver;

static Receiver g_xcwcp_receiver;




/* Callback. */
void xcwcp_handle_libcw_keying_event(void * timer, int key_state);

int legacy_api_test_rec_poll(cw_test_executor_t * cte);
void * receiver_input_generator_fn(void * arg);
void receiver_sk_event(Receiver * xcwcp_receiver, bool is_down);

/* Main poll function and its helpers. */
void receiver_poll_receiver(Receiver * xcwcp_receiver);
void receiver_poll_report_error(Receiver * xcwcp_receiver);
void receiver_poll_character(Receiver * xcwcp_receiver);
void receiver_poll_space(Receiver * xcwcp_receiver);

void test_callback_func(volatile struct timeval * tv, int key_state, void * arg);
void receiver_start_test_code(Receiver * xcwcp_receiver);
void receiver_stop_test_code(Receiver * xcwcp_receiver);

void compare_text_buffers(Receiver * xcwcp_receiver);
void prepare_input_text_buffer(Receiver * xcwcp_receiver);




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
		/* Check if receiver received the pending inter-word
		   space. */
		receiver_poll_space(xcwcp_receiver);

		if (!xcwcp_receiver->is_pending_inter_word_space) {
			/* We received the pending space. After it the
			   receiver may have received another
			   character.  Try to get it too. */
			receiver_poll_character(xcwcp_receiver);
		}
	} else {
		/* Not awaiting a possible space, so just poll the
		   next possible received character. */
		receiver_poll_character(xcwcp_receiver);
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

	/* If this is a tone start and we're awaiting an inter-word
	   space, cancel that wait and clear the receive buffer. */
	if (key_state && g_xcwcp_receiver.is_pending_inter_word_space) {
		/* Tell receiver to prepare (to make space) for
		   receiving new character. */
		cw_clear_receive_buffer();

		/* The tone start means that we're seeing the next
		   incoming character within the same word, so no
		   inter-word space is possible at this point in
		   time. The space that we were observing/waiting for,
		   was just inter-character space. */
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
*/
void receiver_poll_character(Receiver * xcwcp_receiver)
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

	char c = 0;
	bool is_end_of_word_c;
	if (cw_receive_character(&local_timer, &c, &is_end_of_word_c, NULL)) {

		/* Receiver stores full, well formed  character. Display it. */
		fprintf(stderr, "[II] Character: '%c'\n", c);

		{ /* Actual test code. */

			bool failure = false;
			xcwcp_receiver->test_received_string[xcwcp_receiver->test_received_string_i++] = c;

			bool is_end_of_word_r = false;
			bool is_error_r = false;
			char representation[20] = { 0 };
			int cw_ret = cw_receive_representation(&local_timer, representation, &is_end_of_word_r, &is_error_r);
			if (!xcwcp_receiver->cte->expect_op_int_errors_only(xcwcp_receiver->cte, cw_ret, "==", CW_SUCCESS, "receive representation for character")) {
				failure = true;
			}

			if (!xcwcp_receiver->cte->expect_op_int_errors_only(xcwcp_receiver->cte, is_end_of_word_r, "==", is_end_of_word_c, "'is end of word' markers mismatch: %d != %d", is_end_of_word_r, is_end_of_word_c)) {
				failure = true;
			}

			if (!xcwcp_receiver->cte->expect_op_int_errors_only(xcwcp_receiver->cte, is_end_of_word_r, "==", false, "'is end of word' marker is unexpectedly 'true'")) {
				failure = true;
			}

			const char looked_up = cw_representation_to_character(representation);
			if (!xcwcp_receiver->cte->expect_op_int_errors_only(xcwcp_receiver->cte, 0, "!=", looked_up, "Failed to look up character for representation")) {
				failure = true;
			}
			if (!xcwcp_receiver->cte->expect_op_int_errors_only(xcwcp_receiver->cte, c, "==", looked_up, "Looked up character is different than received: %c != %c\n", looked_up, c)) {
				failure = true;
			}

			fprintf(stderr, "[II] Character: Representation: %c -> '%s'\n", c, representation);

			xcwcp_receiver->cte->expect_op_int(xcwcp_receiver->cte, failure, "==", false, "Polling character");
		}

		/* A full character has been received. Directly after
		   it comes a space. Either a short inter-character
		   space followed by another character (in this case
		   we won't display the inter-character space), or
		   longer inter-word space - this space we would like
		   to catch and display.

		   Set a flag indicating that next poll may result in
		   inter-word space. */
		xcwcp_receiver->is_pending_inter_word_space = true;

	} else {
		/* Handle receive error detected on trying to read a character. */
		switch (errno) {
		case EAGAIN:
			/* Call made too early, receiver hasn't
			   received a full character yet. Try next
			   time. */
			break;

		case ERANGE:
			/* Call made not in time, or not in proper
			   sequence. Receiver hasn't received any
			   character (yet). Try harder. */
			break;

		case ENOENT:
			/* Invalid character in receiver's buffer. */
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
   If we received a character on an earlier poll, check again to see
   if we need to revise the decision about whether it is the end of a
   word too.
*/
void receiver_poll_space(Receiver * xcwcp_receiver)
{
	/* Recheck the receive buffer for end of word. */

	/* We expect the receiver to contain a character, but we don't
	   ask for it this time. The receiver should also store
	   information about an inter-character space. If it is longer
	   than a regular inter-character space, then the receiver
	   will treat it as inter-word space, and communicate it over
	   is_end_of_word.

	   Don't use xcwcp_receiver.main_timer - it is used eclusively for
	   marking initial "key down" events. Use local throw-away
	   local_timer. */
	struct timeval local_timer;
	gettimeofday(&local_timer, NULL);
	//fprintf(stderr, "receiver_poll_space(): %10ld : %10ld\n", local_timer.tv_sec, local_timer.tv_usec);

	char c = 0;
	bool is_end_of_word_c;
	cw_receive_character(&local_timer, NULL, &is_end_of_word_c, NULL);
	if (is_end_of_word_c) {
		fprintf(stderr, "[II] Space:\n");

		{ /* Actual test code. */

			bool failure = false;

			/* cw_receive_character() will return through
			   'c' variable the last character that was
			   polled before space.

			   Maybe this is good, maybe this is bad, but
			   this is the legacy behaviour that we will
			   keep supporting. */
			if (!xcwcp_receiver->cte->expect_op_int_errors_only(xcwcp_receiver->cte, c, "!=", ' ', "returned character should not be space")) {
				failure = true;
			}


			xcwcp_receiver->test_received_string[xcwcp_receiver->test_received_string_i++] = ' ';

			bool is_end_of_word_r = false;
			bool is_error_r = false;
			char representation[20] = { 0 };
			int cw_ret = cw_receive_representation(&local_timer, representation, &is_end_of_word_r, &is_error_r);
			if (!xcwcp_receiver->cte->expect_op_int_errors_only(xcwcp_receiver->cte, cw_ret, "==", CW_SUCCESS, "Failed to get representation of space")) {
				failure = true;
			}

			if (!xcwcp_receiver->cte->expect_op_int_errors_only(xcwcp_receiver->cte, is_end_of_word_r, "==", is_end_of_word_c, "'is end of word' markers mismatch: %d != %d", is_end_of_word_r, is_end_of_word_c)) {
				failure = true;
			}

			if (!xcwcp_receiver->cte->expect_op_int_errors_only(xcwcp_receiver->cte, true, "==", is_end_of_word_r, "'is end of word' marker is unexpectedly 'false'")) {
				failure = true;
			}

			xcwcp_receiver->cte->expect_op_int(xcwcp_receiver->cte, false, "==", failure, "Polling inter-word space");
		}

		cw_clear_receive_buffer();
		xcwcp_receiver->is_pending_inter_word_space = false;
	} else {
		/* We don't reset is_pending_inter_word_space. The
		   space that currently lasts, and isn't long enough
		   to be considered inter-word space, may grow to
		   become the inter-word space. Or not.

		   This growing of inter-character space into
		   inter-word space may be terminated by incoming next
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
	Receiver * xcwcp_receiver = (Receiver *) arg;


	prepare_input_text_buffer(xcwcp_receiver);


	/* Using Null sound system because this generator is only used
	   to enqueue text and control key. Sound will be played by
	   main generator used by xcwcp */
	cw_gen_t * gen = cw_gen_new(CW_AUDIO_NULL, NULL);
	cw_gen_set_label(gen, "input gener. gen");

	cw_rec_t * rec = cw_rec_new();
	cw_rec_set_label(rec, "input gener. rec");

	cw_key_t key;
	cw_key_set_label(&key, "input gener. key");

	cw_key_register_generator(&key, gen);
	cw_key_register_receiver(&key, rec);
	cw_key_register_keying_callback(&key, test_callback_func, arg);


	/* Start sending the test string. Registered callback will be
	   called on every mark/space. */
	cw_gen_start(gen);
	cw_gen_enqueue_string(gen, xcwcp_receiver->test_input_string);

	/* Wait for all characters to be played out. */
	cw_tq_wait_for_level_internal(gen->tq, 0);
	cw_usleep_internal(1000 * 1000);

	cw_gen_delete(&gen);
	cw_rec_delete(&rec);
	xcwcp_receiver->generating_in_progress = false;


	compare_text_buffers(&g_xcwcp_receiver);


	return NULL;
}




void receiver_start_test_code(Receiver * xcwcp_receiver)
{
	xcwcp_receiver->generating_in_progress = true;
	pthread_create(&xcwcp_receiver->receiver_test_code_thread_id, NULL, receiver_input_generator_fn, xcwcp_receiver);
}




void receiver_stop_test_code(Receiver * xcwcp_receiver)
{
	pthread_cancel(xcwcp_receiver->receiver_test_code_thread_id);
}




void prepare_input_text_buffer(Receiver * xcwcp_receiver)
{
#if 1
	const char input[REC_TEST_BUFFER_SIZE] =
		"the quick brown fox jumps over the lazy dog. 01234567890 "     /* Simple test. */
		"abcdefghijklmnopqrstuvwxyz0123456789\"'$()+,-./:;=?_@<>!&^~ "  /* Almost all characters. */
		"one two three four five six seven eight nine ten eleven";      /* Words and spaces. */
#else
	/* Short test for occasions where I need a quick test. */
	const char input[REC_TEST_BUFFER_SIZE] = "one two";
	//const char input[REC_TEST_BUFFER_SIZE] = "the quick brown fox jumps over the lazy dog. 01234567890";
#endif
	snprintf(xcwcp_receiver->test_input_string, sizeof (xcwcp_receiver->test_input_string), "%s", input);

	return;
}




/* Compare buffers with text that was sent to test generator and text
   that was received from tested production receiver.

   Compare input text with what the receiver received. */
void compare_text_buffers(Receiver * xcwcp_receiver)
{
	/* Luckily for us the text enqueued in test generator and
	   played at ~12WPM is recognized by xcwcp receiver from the
	   beginning without any problems, so we will be able to do
	   simple strcmp(). */

	fprintf(stderr, "[II] Sent:     '%s'\n", xcwcp_receiver->test_input_string);
	fprintf(stderr, "[II] Received: '%s'\n", xcwcp_receiver->test_received_string);

	/* Normalize received string. */
	{
		const size_t len = strlen(xcwcp_receiver->test_received_string);
		for (size_t i = 0; i < len; i++) {
			xcwcp_receiver->test_received_string[i] = tolower(xcwcp_receiver->test_received_string[i]);
		}
		if (xcwcp_receiver->test_received_string[len - 1] == ' ') {
			xcwcp_receiver->test_received_string[len - 1] = '\0';
		}
	}


	const int compare_result = strcmp(xcwcp_receiver->test_input_string, xcwcp_receiver->test_received_string);
	if (xcwcp_receiver->cte->expect_op_int(xcwcp_receiver->cte, compare_result, "==", 0, "Final comparison of sent and received strings")) {
		fprintf(stderr, "[II] Test result: success\n");
	} else {
		fprintf(stderr, "[EE] Test result: failure\n");
		fprintf(stderr, "[EE] '%s' != '%s'\n", xcwcp_receiver->test_input_string, xcwcp_receiver->test_received_string);
	}

	return;
}




void test_callback_func(volatile struct timeval * tv, int key_state, void * arg)
{
	/* Inform libcw receiver about new state of straight key ("sk").

	   libcw receiver will process the new state and we will later
	   try to poll a character or space from it. */

	Receiver * xcwcp_receiver = (Receiver *) arg;
	//fprintf(stderr, "Callback function, key state = %d\n", key_state);
	receiver_sk_event(xcwcp_receiver, key_state);
}




int legacy_api_test_rec_poll(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	if (CW_SUCCESS != cw_generator_new(CW_AUDIO_PA, NULL)) {
		fprintf(stderr, "failed to create generator\n");
		return -1;
	}


	cw_clear_receive_buffer();
	cw_generator_start();

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


	/* Start thread with test code. */
	receiver_start_test_code(&g_xcwcp_receiver);

	while (g_xcwcp_receiver.generating_in_progress) {
		/* At 60WPM, a dot is 20ms, so polling for the maximum speed
		   library needs a 10ms timeout. */
		usleep(10);
		receiver_poll_receiver(&g_xcwcp_receiver);
	}

	/* Stop thread with test code.
	   Is this really needed? The thread should already be stopped
	   if we get here. */
	receiver_stop_test_code(&g_xcwcp_receiver);


	/* Tell legacy objects of libcw (those in production code) to stop working. */
	cw_complete_reset();
	cw_generator_stop();
	cw_generator_delete();


	cte->print_test_footer(cte, __func__);

	return 0;
}
