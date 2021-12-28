/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2021  Kamil Ignacak (acerion@wp.pl)

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc., 51
  Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <libcw.h>
#ifdef XCWCP_WITH_REC_TEST
#include <ctype.h>
#include <unistd.h>

#include <cw_rec_tester.h>

#include "libcw2.h"
#include "../libcw/libcw_key.h"
#include "../libcw/libcw_gen.h"
#include "../libcw/libcw_tq.h"
#include "../libcw/libcw_utils.h"

#endif




#include "easy_rec.h"




struct easy_rec_t {
	/* Timer for measuring length of dots and dashes.

	   Initial value of the timestamp is created by xcwcp's receiver on
	   first "paddle down" event in a character. The timestamp is then
	   updated by libcw on specific time intervals. The intervals are a
	   function of keyboard key presses or mouse button presses recorded
	   by xcwcp. */
	struct timeval main_timer;

	/* Safety flag to ensure that we keep the library in sync with keyer
	   events. Without, there's a chance that of a on-off event, one half
	   will go to one application instance, and the other to another
	   instance. */
	volatile bool tracked_key_state;

	/* Flag indicating if receive polling has received a character, and
	   may need to augment it with a word space on a later poll. */
	volatile bool is_pending_inter_word_space;

	/* Flag indicating possible receive errno detected in signal handler
	   context and needing to be passed to the foreground. */
	volatile int libcw_receive_errno;

	/* State of left and right paddle of iambic keyer. The
	   flags are common for keying with keyboard keys and
	   with mouse buttons.

	   A timestamp for libcw needs to be generated only in
	   situations when one of the paddles comes down and
	   the other is up. This is why we observe state of
	   both paddles separately. */
	bool is_left_down;
	bool is_right_down;

#ifdef XCWCP_WITH_REC_TEST
	pthread_t receiver_test_code_thread_id;
	cw_rec_tester_t rec_tester;
#endif
};




#ifdef XCWCP_WITH_REC_TEST
static void * receiver_input_generator_fn(void * arg);
void test_callback_func(void * arg, int key_state);
void low_tone_queue_callback(void * arg);
int easy_rec_test_on_character(easy_rec_t * easy_rec, easy_rec_data_t * erd, struct timeval * timer);
int easy_rec_test_on_space(easy_rec_t * easy_rec, easy_rec_data_t * erd, struct timeval * timer);
#endif




easy_rec_t * easy_rec_new(void)
{
	return (easy_rec_t *) calloc(1, sizeof (easy_rec_t));
}




void easy_rec_delete(easy_rec_t ** easy_rec)
{
	if (easy_rec && *easy_rec) {
		free(*easy_rec);
		*easy_rec = NULL;
	}
}




void easy_rec_start(easy_rec_t * easy_rec)
{
	/* The call above registered receiver->main_timer as a generic
	   argument to a callback. However, libcw needs to know when
	   the argument happens to be of type 'struct timeval'. This
	   is why we have this second call, explicitly passing
	   receiver's timer to libcw. */
	cw_iambic_keyer_register_timer(&easy_rec->main_timer);

	gettimeofday(&easy_rec->main_timer, NULL);
	//fprintf(stderr, "time on aux config: %10ld : %10ld\n", easy_rec->main_timer.tv_sec, easy_rec->main_timer.tv_usec);
}




/**
   \brief Handle straight key event

   \param is_down
*/
void easy_rec_sk_event(easy_rec_t * easy_rec, bool is_down)
{
	/* Inform xcwcp receiver (which will inform libcw receiver)
	   about new state of straight key ("sk").

	   libcw receiver will process the new state and we will later
	   try to poll a character or space from it. */

	//fprintf(stderr, "Callback function, key state = %d\n", key_state);


	/* Prepare timestamp for libcw on both "key up" and "key down"
	   events. There is no code in libcw that would generate
	   updated consecutive timestamps for us (as it does in case
	   of iambic keyer).

	   TODO: see in libcw how iambic keyer updates a timer, and
	   how straight key does not. Apparently the timer is used to
	   recognize and distinguish dots from dashes. Maybe straight
	   key could have such timer as well? */
	gettimeofday(&easy_rec->main_timer, NULL);
	//fprintf(stderr, "time on Skey down:  %10ld : %10ld\n", easy_rec->main_timer.tv_sec, easy_rec->main_timer.tv_usec);

	cw_notify_straight_key_event(is_down);

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
void easy_rec_handle_libcw_keying_event(easy_rec_t * easy_rec, int key_state)
{
	/* Ignore calls where the key state matches our tracked key
	   state.  This avoids possible problems where this event
	   handler is redirected between application instances; we
	   might receive an end of tone without seeing the start of
	   tone. */
	if (key_state == easy_rec->tracked_key_state) {
		//fprintf(stderr, "tracked key state == %d\n", easy_rec->tracked_key_state);
		return;
	} else {
		//fprintf(stderr, "tracked key state := %d\n", key_state);
		easy_rec->tracked_key_state = key_state;
	}

	/* If this is a tone start and we're awaiting an inter-word
	   space, cancel that wait and clear the receive buffer. */
	if (key_state && easy_rec->is_pending_inter_word_space) {
		/* Tell receiver to prepare (to make space) for
		   receiving new character. */
		cw_clear_receive_buffer();

		/* The tone start means that we're seeing the next
		   incoming character within the same word, so no
		   inter-word space is possible at this point in
		   time. The space that we were observing/waiting for,
		   was just inter-character space. */
		easy_rec->is_pending_inter_word_space = false;
	}

	//fprintf(stderr, "calling callback, stage 2\n");

	/* Pass tone state on to the library.  For tone end, check to
	   see if the library has registered any receive error. */
	if (key_state) {
		/* Key down. */
		//fprintf(stderr, "start receive tone: %10ld . %10ld\n", easy_rec->main_timer->tv_sec, easy_rec->main_timer->tv_usec);
		if (!cw_start_receive_tone(&easy_rec->main_timer)) {
			perror("cw_start_receive_tone");
			return;
		}
	} else {
		/* Key up. */
		//fprintf(stderr, "end receive tone:   %10ld . %10ld\n", easy_rec->main_timer->tv_sec, easy_rec->main_timer->tv_usec);
		if (!cw_end_receive_tone(&easy_rec->main_timer)) {
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
				easy_rec->libcw_receive_errno = errno;
				cw_clear_receive_buffer();
				break;
			default:
				perror("cw_end_receive_tone");
				return;
			}
		}
	}

	return;
}




bool easy_rec_poll_character(easy_rec_t * easy_rec, easy_rec_data_t * erd)
{
	/* Don't use receiver.easy_rec->main_timer - it is used exclusively for
	   marking initial "key down" events. Use local throw-away
	   timer2.

	   Additionally using reveiver.easy_rec->main_timer here would mess up time
	   intervals measured by receiver.easy_rec->main_timer, and that would
	   interfere with recognizing dots and dashes. */
	struct timeval timer2;
	gettimeofday(&timer2, NULL);
	//fprintf(stderr, "poll_receive_char:  %10ld : %10ld\n", timer2.tv_sec, timer2.tv_usec);

	errno = 0;
	const bool received = cw_receive_character(&timer2, &erd->c, &erd->is_end_of_word, NULL);
	erd->errno_val = errno;
	if (received) {

#ifdef XCWCP_WITH_REC_TEST
		if (CW_SUCCESS != easy_rec_test_on_character(easy_rec, erd, &timer2)) {
			exit(EXIT_FAILURE);
		}
#endif
		/* A full character has been received. Directly after
		   it comes a space. Either a short inter-character
		   space followed by another character (in this case
		   we won't display the inter-character space), or
		   longer inter-word space - this space we would like
		   to catch and display.

		   Set a flag indicating that next poll may result in
		   inter-word space. */
		easy_rec->is_pending_inter_word_space = true;

		//fprintf(stderr, "Received character '%c'\n", erd->c);

		return true;

	} else {
		/* Handle receive error detected on trying to read a character. */
		switch (erd->errno_val) {
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

		case EINVAL:
			/* Timestamp error. */
			cw_clear_receive_buffer();
			break;

		default:
			perror("cw_receive_character");
		}

		return false;
	}
}




// TODO: can we return true when a space has been successfully polled,
// instead of returning it through erd?
void easy_rec_poll_space(easy_rec_t * easy_rec, easy_rec_data_t * erd)
{
	/* We expect the receiver to contain a character, but we don't
	   ask for it this time. The receiver should also store
	   information about an inter-character space. If it is longer
	   than a regular inter-character space, then the receiver
	   will treat it as inter-word space, and communicate it over
	   is_end_of_word.

	   Don't use receiver.easy_rec->main_timer - it is used eclusively for
	   marking initial "key down" events. Use local throw-away
	   timer2. */
	struct timeval timer2;
	gettimeofday(&timer2, NULL);
	//fprintf(stderr, "poll_space(): %10ld : %10ld\n", timer2.tv_sec, timer2.tv_usec);

	cw_receive_character(&timer2, &erd->c, &erd->is_end_of_word, NULL);
	if (erd->is_end_of_word) {
		//fprintf(stderr, "End of word '%c'\n\n", erd->c);

#ifdef XCWCP_WITH_REC_TEST
		if (CW_SUCCESS != easy_rec_test_on_space(easy_rec, erd, &timer2)) {
			exit(EXIT_FAILURE);
		}
#endif

		cw_clear_receive_buffer();
		easy_rec->is_pending_inter_word_space = false;
	} else {
		/* We don't reset easy_rec->is_pending_inter_word_space. The
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



int easy_rec_get_libcw_errno(const easy_rec_t * easy_rec)
{
	return easy_rec->libcw_receive_errno;
}


void easy_rec_clear_libcw_errno(easy_rec_t * easy_rec)
{
	easy_rec->libcw_receive_errno = 0;
}



bool easy_rec_is_pending_inter_word_space(const easy_rec_t * easy_rec)
{
	return easy_rec->is_pending_inter_word_space;
}




void easy_rec_clear(easy_rec_t * easy_rec)
{
	cw_clear_receive_buffer();
	easy_rec->is_pending_inter_word_space = false;
	easy_rec->libcw_receive_errno = 0;
	easy_rec->tracked_key_state = false;
}





void easy_rec_ik_left_event(easy_rec_t * easy_rec, bool is_down, bool is_reverse_paddles)
{
	easy_rec->is_left_down = is_down;
	if (easy_rec->is_left_down && !easy_rec->is_right_down) {
		/* Prepare timestamp for libcw, but only for initial
		   "paddle down" event at the beginning of
		   character. Don't create the timestamp for any
		   successive "paddle down" events inside a character.

		   In case of iambic keyer the timestamps for every
		   next (non-initial) "paddle up" or "paddle down"
		   event in a character will be created by libcw.

		   TODO: why libcw can't create such timestamp for
		   first event for us? */
		gettimeofday(&easy_rec->main_timer, NULL);
		//fprintf(stderr, "time on Lkey down:  %10ld : %10ld\n", easy_rec->main_timer.tv_sec, easy_rec->main_timer.tv_usec);
	}

	/* Inform libcw about state of left paddle regardless of state
	   of the other paddle. */
	is_reverse_paddles
		? cw_notify_keyer_dash_paddle_event(is_down)
		: cw_notify_keyer_dot_paddle_event(is_down);
	return;
}




/**
   \brief Handle event on right paddle of iambic keyer

   \param is_down
   \param is_reverse_paddles
*/
void easy_rec_ik_right_event(easy_rec_t * easy_rec, bool is_down, bool is_reverse_paddles)
{
	easy_rec->is_right_down = is_down;
	if (easy_rec->is_right_down && !easy_rec->is_left_down) {
		/* Prepare timestamp for libcw, but only for initial
		   "paddle down" event at the beginning of
		   character. Don't create the timestamp for any
		   successive "paddle down" events inside a character.

		   In case of iambic keyer the timestamps for every
		   next (non-initial) "paddle up" or "paddle down"
		   event in a character will be created by libcw. */
		gettimeofday(&easy_rec->main_timer, NULL);
		//fprintf(stderr, "time on Rkey down:  %10ld : %10ld\n", easy_rec->main_timer.tv_sec, easy_rec->main_timer.tv_usec);
	}

	/* Inform libcw about state of left paddle regardless of state
	   of the other paddle. */
	is_reverse_paddles
		? cw_notify_keyer_dot_paddle_event(is_down)
		: cw_notify_keyer_dash_paddle_event(is_down);
	return;
}




#ifdef XCWCP_WITH_REC_TEST




void easy_rec_start_test_code(easy_rec_t * easy_rec)
{
	pthread_create(&easy_rec->receiver_test_code_thread_id, NULL, receiver_input_generator_fn, easy_rec);
}




void easy_rec_stop_test_code(easy_rec_t * easy_rec)
{
	pthread_cancel(easy_rec->receiver_test_code_thread_id);
}




void test_callback_func(void * arg, int key_state)
{
	/* Inform xcwcp receiver (which will inform libcw receiver)
	   about new state of straight key ("sk").

	   libcw receiver will process the new state and we will later
	   try to poll a character or space from it. */

	easy_rec_t * easy_rec = (easy_rec_t *) arg;
	//fprintf(stderr, "Callback function, key state = %d\n", key_state);
	easy_rec_sk_event(easy_rec, key_state);
}




/*
  Code that generates info about timing of input events for receiver.

  We could generate the info and the events using a big array of
  timestamps and a call to usleep(), but instead we are using a new
  generator that can inform us when marks/spaces start.
*/
void * receiver_input_generator_fn(void * arg)
{
	easy_rec_t * easy_rec = (easy_rec_t *) arg;

	cw_rec_tester_init(&easy_rec->rec_tester);
	cw_rec_tester_init_text_buffers(&easy_rec->rec_tester, 1);


	/* Using Null sound system because this generator is only used
	   to enqueue text and control key. Sound will be played by
	   main generator used by xcwcp */
	cw_gen_config_t gen_conf;
	memset(&gen_conf, 0, sizeof (gen_conf));
	gen_conf.sound_system = CW_AUDIO_NULL;

	cw_gen_t * gen = cw_gen_new(&gen_conf);
	cw_key_t key;

	easy_rec->rec_tester.gen = gen;
	cw_tq_register_low_level_callback_internal(easy_rec->rec_tester.gen->tq, low_tone_queue_callback, &easy_rec->rec_tester, 5);

	cw_key_register_generator(&key, gen);
	cw_gen_register_value_tracking_callback_internal(gen, test_callback_func, arg);
	//cw_key_register_keying_callback(&key, test_callback_func, arg);

	/* Start sending the test string. Registered callback will be
	   called on every mark/space. Enqueue only initial part of
	   string, just to start sending, the rest should be sent by
	   'low watermark' callback. */
	cw_gen_start(gen);
	for (int i = 0; i < 5; i++) {
		const char c = easy_rec->rec_tester.input_string[easy_rec->rec_tester.input_string_i];
		if ('\0' == c) {
			/* A very short input string. */
			break;
		} else {
			cw_gen_enqueue_character(easy_rec->rec_tester.gen, c);
			easy_rec->rec_tester.input_string_i++;
		}
	}

	/* Wait for all characters to be played out. */
	cw_tq_wait_for_level_internal(gen->tq, 0);
	cw_usleep_internal(1000 * 1000);

	cw_gen_delete(&gen);
	easy_rec->rec_tester.generating_in_progress = false;

	cw_rec_tester_evaluate_receive_correctness(&easy_rec->rec_tester);


	return NULL;
}




void low_tone_queue_callback(void * arg)
{
	cw_rec_tester_t * tester = (cw_rec_tester_t *) arg;

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




int easy_rec_test_on_character(easy_rec_t * easy_rec, easy_rec_data_t * erd, struct timeval * timer)
{
	fprintf(stderr, "[II] Character: '%c'\n", erd->c);

	easy_rec->rec_tester.received_string[easy_rec->rec_tester.received_string_i++] = erd->c;

	easy_rec_data_t test_data = { 0 };
	int cw_ret = cw_receive_representation(timer, test_data.representation, &test_data.is_end_of_word, &test_data.is_error);
	if (CW_SUCCESS != cw_ret) {
		fprintf(stderr, "[EE] Character: failed to get representation\n");
		return CW_FAILURE;
	}

	if (test_data.is_end_of_word != erd->is_end_of_word) {
		fprintf(stderr, "[EE] Character: 'is end of word' markers mismatch: %d != %d\n", test_data.is_end_of_word, erd->is_end_of_word);
		return CW_FAILURE;
	}

	if (test_data.is_end_of_word) {
		fprintf(stderr, "[EE] Character: 'is end of word' marker is unexpectedly 'true'\n");
		return CW_FAILURE;
	}

	const char looked_up = cw_representation_to_character(test_data.representation);
	if (0 == looked_up) {
		fprintf(stderr, "[EE] Character: Failed to look up character for representation\n");
		return CW_FAILURE;
	}

	if (looked_up != erd->c) {
		fprintf(stderr, "[EE] Character: Looked up character is different than received: %c != %c\n", looked_up, erd->c);
	}

	fprintf(stderr, "[II] Character: Representation: %c -> '%s'\n",
		erd->c, test_data.representation);

	/* Not entirely a success if looked up char does not match received
	   character, but returning failure here would lead to calling
	   exit(). */
	return CW_SUCCESS;
}




int easy_rec_test_on_space(easy_rec_t * easy_rec, easy_rec_data_t * erd, struct timeval * timer)
{
	fprintf(stderr, "[II] Space:\n");

	/* cw_receive_character() will return through 'c' variable the last
	   character that was polled before space.

	   Maybe this is good, maybe this is bad, but this is the legacy
	   behaviour that we will keep supporting. */
	if (' ' == erd->c) {
		fprintf(stderr, "[EE] Space: returned character should not be space\n");
		return CW_FAILURE;
	}


	easy_rec->rec_tester.received_string[easy_rec->rec_tester.received_string_i++] = ' ';

	easy_rec_data_t test_data = { 0 };
	int cw_ret = cw_receive_representation(timer, test_data.representation, &test_data.is_end_of_word, &test_data.is_error);
	if (CW_SUCCESS != cw_ret) {
		fprintf(stderr, "[EE] Space: Failed to get representation\n");
		return CW_FAILURE;
	}

	if (test_data.is_end_of_word != erd->is_end_of_word) {
		fprintf(stderr, "[EE] Space: 'is end of word' markers mismatch: %d != %d\n", test_data.is_end_of_word, erd->is_end_of_word);
		return CW_FAILURE;
	}

	if (!test_data.is_end_of_word) {
		fprintf(stderr, "[EE] Space: 'is end of word' marker is unexpectedly 'false'\n");
		return CW_FAILURE;
	}

	return CW_SUCCESS;
}




#endif

