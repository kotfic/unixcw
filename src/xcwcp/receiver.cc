// Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
// Copyright (C) 2011-2020  Kamil Ignacak (acerion@wp.pl)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <cerrno>
#include <sstream>


#include "application.h"
#include "receiver.h"
#include "textarea.h"
#include "modeset.h"

#include "libcw.h"

#include "i18n.h"


#ifdef XCWCP_WITH_REC_TEST
#include <cctype>
#include <unistd.h>

#include <cw_rec_tester.h>

#include "libcw2.h"
#include "../libcw/libcw_key.h"
#include "../libcw/libcw_gen.h"
#include "../libcw/libcw_tq.h"
#include "../libcw/libcw_utils.h"
#endif



namespace cw {





/**
   \brief Poll the CW library receive buffer and handle anything found
   in the buffer

   \param current_mode
*/
void Receiver::poll(const Mode *current_mode)
{
	if (!current_mode->is_receive()) {
		return;
	}

	if (libcw_receive_errno != 0) {
		poll_report_error();
	}

	if (is_pending_inter_word_space) {

		/* Check if receiver received the pending inter-word
		   space. */
		poll_space();

		if (!is_pending_inter_word_space) {
			/* We received the pending space. After it the
			   receiver may have received another
			   character.  Try to get it too. */
			poll_character();
		}
	} else {
		/* Not awaiting a possible space, so just poll the
		   next possible received character. */
		poll_character();
	}

	return;
}





/**
   \brief Handle keyboard keys pressed in main window in receiver mode

   Function handles both press and release events, but ignores
   autorepeat.

   Call the function only when receiver mode is active.

   \param event - key event in main window to handle
   \param is_reverse_paddles
*/
void Receiver::handle_key_event(QKeyEvent *event, bool is_reverse_paddles)
{
	if (event->isAutoRepeat()) {
		/* Ignore repeated key events.  This prevents
		   autorepeat from getting in the way of identifying
		   the real keyboard events we are after. */
		return;
	}

	if (event->type() == QEvent::KeyPress
	    || event->type() == QEvent::KeyRelease) {

		const int is_down = event->type() == QEvent::KeyPress;

		if (event->key() == Qt::Key_Space
		    || event->key() == Qt::Key_Up
		    || event->key() == Qt::Key_Down
		    || event->key() == Qt::Key_Enter
		    || event->key() == Qt::Key_Return) {

			/* These keys are obvious candidates for
			   "straight key" key. */

			//fprintf(stderr, "---------- handle key event: sk: %d\n", is_down);
			sk_event(is_down);
			event->accept();

		} else if (event->key() == Qt::Key_Left) {
			ik_left_event(is_down, is_reverse_paddles);
			event->accept();

		} else if (event->key() == Qt::Key_Right) {
			ik_right_event(is_down, is_reverse_paddles);
			event->accept();

		} else {
			; /* Some other, uninteresting key. Ignore it. */
		}
	}

	return;
}





/**
   \brief Handle mouse events

   The function looks at mouse button events and interprets them as
   one of: left iambic key event, right iambic key event, straight key
   event.

   Call the function only when receiver mode is active.

   \param event - mouse event to handle
   \param is_reverse_paddles
*/
void Receiver::handle_mouse_event(QMouseEvent *event, bool is_reverse_paddles)
{
	if (event->type() == QEvent::MouseButtonPress
	    || event->type() == QEvent::MouseButtonDblClick
	    || event->type() == QEvent::MouseButtonRelease) {

		const int is_down = event->type() == QEvent::MouseButtonPress
			|| event->type() == QEvent::MouseButtonDblClick;

		if (event->button() == Qt::MidButton) {
			//fprintf(stderr, "---------- handle mouse event: sk: %d\n", is_down);
			sk_event(is_down);
			event->accept();

		} else if (event->button() == Qt::LeftButton) {
			ik_left_event(is_down, is_reverse_paddles);
			event->accept();

		} else if (event->button() == Qt::RightButton) {
			ik_right_event(is_down, is_reverse_paddles);
			event->accept();

		} else {
			; /* Some other mouse button, or mouse cursor
			     movement. Ignore it. */
		}
	}

	return;
}





/**
   \brief Handle straight key event

   \param is_down
*/
void Receiver::sk_event(bool is_down)
{
	/* Prepare timestamp for libcw on both "key up" and "key down"
	   events. There is no code in libcw that would generate
	   updated consecutive timestamps for us (as it does in case
	   of iambic keyer).

	   TODO: see in libcw how iambic keyer updates a timer, and
	   how straight key does not. Apparently the timer is used to
	   recognize and distinguish dots from dashes. Maybe straight
	   key could have such timer as well? */
	gettimeofday(&this->main_timer, NULL);
	//fprintf(stderr, "time on Skey down:  %10ld : %10ld\n", this->main_timer.tv_sec, this->main_timer.tv_usec);

	cw_notify_straight_key_event(is_down);

	return;
}





/**
   \brief Handle event on left paddle of iambic keyer

   \param is_down
   \param is_reverse_paddles
*/
void Receiver::ik_left_event(bool is_down, bool is_reverse_paddles)
{
	is_left_down = is_down;
	if (is_left_down && !is_right_down) {
		/* Prepare timestamp for libcw, but only for initial
		   "paddle down" event at the beginning of
		   character. Don't create the timestamp for any
		   successive "paddle down" events inside a character.

		   In case of iambic keyer the timestamps for every
		   next (non-initial) "paddle up" or "paddle down"
		   event in a character will be created by libcw.

		   TODO: why libcw can't create such timestamp for
		   first event for us? */
		gettimeofday(&this->main_timer, NULL);
		//fprintf(stderr, "time on Lkey down:  %10ld : %10ld\n", this->main_timer.tv_sec, this->main_timer.tv_usec);
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
void Receiver::ik_right_event(bool is_down, bool is_reverse_paddles)
{
	is_right_down = is_down;
	if (is_right_down && !is_left_down) {
		/* Prepare timestamp for libcw, but only for initial
		   "paddle down" event at the beginning of
		   character. Don't create the timestamp for any
		   successive "paddle down" events inside a character.

		   In case of iambic keyer the timestamps for every
		   next (non-initial) "paddle up" or "paddle down"
		   event in a character will be created by libcw. */
		gettimeofday(&this->main_timer, NULL);
		//fprintf(stderr, "time on Rkey down:  %10ld : %10ld\n", this->main_timer.tv_sec, this->main_timer.tv_usec);
	}

	/* Inform libcw about state of left paddle regardless of state
	   of the other paddle. */
	is_reverse_paddles
		? cw_notify_keyer_dot_paddle_event(is_down)
		: cw_notify_keyer_dash_paddle_event(is_down);

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
void Receiver::handle_libcw_keying_event(struct timeval *t, int key_state)
{
	/* Ignore calls where the key state matches our tracked key
	   state.  This avoids possible problems where this event
	   handler is redirected between application instances; we
	   might receive an end of tone without seeing the start of
	   tone. */
	if (key_state == tracked_key_state) {
		//fprintf(stderr, "tracked key state == %d\n", tracked_key_state);
		return;
	} else {
		//fprintf(stderr, "tracked key state := %d\n", key_state);
		tracked_key_state = key_state;
	}

	/* If this is a tone start and we're awaiting an inter-word
	   space, cancel that wait and clear the receive buffer. */
	if (key_state && is_pending_inter_word_space) {
		/* Tell receiver to prepare (to make space) for
		   receiving new character. */
		cw_clear_receive_buffer();

		/* The tone start means that we're seeing the next
		   incoming character within the same word, so no
		   inter-word space is possible at this point in
		   time. The space that we were observing/waiting for,
		   was just inter-character space. */
		is_pending_inter_word_space = false;
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
				libcw_receive_errno = errno;
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
   \brief Clear the library receive buffer and our own flags
*/
void Receiver::clear()
{
	cw_clear_receive_buffer();
	is_pending_inter_word_space = false;
	libcw_receive_errno = 0;
	tracked_key_state = false;

	return;
}





/**
   \brief Handle any error registered when handling a libcw keying event
*/
void Receiver::poll_report_error()
{
	/* Handle any receive errors detected on tone end but delayed until here. */

	switch (libcw_receive_errno) {
	case ENOMEM:
		app->show_status(_("Representation buffer too small"));
		break;
	case ERANGE:
		app->show_status(_("Internal error"));
		break;
	case EINVAL:
		app->show_status(_("Internal timestamp error"));
		break;
	case ENOENT:
		app->show_status(_("Badly formed CW element"));
		break;
	default:
		app->show_status(_("Internal problem"));
		break;
	}

	libcw_receive_errno = 0;

	return;
}





/**
   \brief Receive any new character from the CW library.
*/
void Receiver::poll_character()
{
	char c = 0;

	/* Don't use receiver.main_timer - it is used exclusively for
	   marking initial "key down" events. Use local throw-away
	   timer2.

	   Additionally using reveiver.main_timer here would mess up time
	   intervals measured by receiver.main_timer, and that would
	   interfere with recognizing dots and dashes. */
	struct timeval timer2;
	gettimeofday(&timer2, NULL);
	//fprintf(stderr, "poll_receive_char:  %10ld : %10ld\n", timer2.tv_sec, timer2.tv_usec);

	bool is_end_of_word;
	if (cw_receive_character(&timer2, &c, &is_end_of_word, NULL)) {
		/* Receiver stores full, well formed
		   character. Display it. */
		textarea->append(c);

#ifdef XCWCP_WITH_REC_TEST
		fprintf(stderr, "[II] Character: '%c'\n", c);

		this->rec_tester.received_string[this->rec_tester.received_string_i++] = c;

		bool is_end_of_word_r = false;
		bool is_error_r = false;
		char representation[20] = { 0 };
		int cw_ret = cw_receive_representation(&timer2, representation, &is_end_of_word_r, &is_error_r);
		if (CW_SUCCESS != cw_ret) {
			fprintf(stderr, "[EE] Character: failed to get representation\n");
			exit(EXIT_FAILURE);
		}

		if (is_end_of_word_r != is_end_of_word) {
			fprintf(stderr, "[EE] Character: 'is end of word' markers mismatch: %d != %d\n", is_end_of_word_r, is_end_of_word);
			exit(EXIT_FAILURE);
		}

		if (is_end_of_word_r) {
			fprintf(stderr, "[EE] Character: 'is end of word' marker is unexpectedly 'true'\n");
			exit(EXIT_FAILURE);
		}

		const char looked_up = cw_representation_to_character(representation);
		if (0 == looked_up) {
			fprintf(stderr, "[EE] Character: Failed to look up character for representation\n");
			exit(EXIT_FAILURE);
		}

		if (looked_up != c) {
			fprintf(stderr, "[EE] Character: Looked up character is different than received: %c != %c\n", looked_up, c);
		}

		fprintf(stderr, "[II] Character: Representation: %c -> '%s'\n", c, representation);
#endif


		/* A full character has been received. Directly after
		   it comes a space. Either a short inter-character
		   space followed by another character (in this case
		   we won't display the inter-character space), or
		   longer inter-word space - this space we would like
		   to catch and display.

		   Set a flag indicating that next poll may result in
		   inter-word space. */
		is_pending_inter_word_space = true;

		/* Update the status bar to show the character
		   received.  Put the received char at the end of
		   string to avoid "jumping" of whole string when
		   width of glyph of received char changes at variable
		   font width. */
		QString status = _("Received at %1 WPM: '%2'");
		app->show_status(status.arg(cw_get_receive_speed()).arg(c));
		//fprintf(stderr, "Received character '%c'\n", c);

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

		case ENOENT: /* Invalid character in receiver's buffer. */
			cw_clear_receive_buffer();
			textarea->append('?');
			app->show_status(QString(_("Unknown character received at %1 WPM")).arg(cw_get_receive_speed()));
			break;

		case EINVAL:
			/* Timestamp error. */
			cw_clear_receive_buffer();
			textarea->append('?');
			app->show_status(QString(_("Internal error")));
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
void Receiver::poll_space()
{
	/* Recheck the receive buffer for end of word. */
	bool is_end_of_word;

	/* We expect the receiver to contain a character, but we don't
	   ask for it this time. The receiver should also store
	   information about an inter-character space. If it is longer
	   than a regular inter-character space, then the receiver
	   will treat it as inter-word space, and communicate it over
	   is_end_of_word.

	   Don't use receiver.main_timer - it is used eclusively for
	   marking initial "key down" events. Use local throw-away
	   timer2. */
	struct timeval timer2;
	gettimeofday(&timer2, NULL);
	//fprintf(stderr, "poll_space(): %10ld : %10ld\n", timer2.tv_sec, timer2.tv_usec);

	char c = 0;
	cw_receive_character(&timer2, &c, &is_end_of_word, NULL);
	if (is_end_of_word) {
		//fprintf(stderr, "End of word '%c'\n\n", c);
		textarea->append(' ');

#ifdef XCWCP_WITH_REC_TEST
		fprintf(stderr, "[II] Space:\n");

		/* cw_receive_character() will return through 'c'
		   variable the last character that was polled before
		   space.

		   Maybe this is good, maybe this is bad, but this is
		   the legacy behaviour that we will keep
		   supporting. */
		if (' ' == c) {
			fprintf(stderr, "[EE] Space: returned character should not be space\n");
			exit(EXIT_FAILURE);
		}


		this->rec_tester.received_string[this->rec_tester.received_string_i++] = ' ';

		bool is_end_of_word_r = false;
		bool is_error_r = false;
		char representation[20] = { 0 };
		int cw_ret = cw_receive_representation(&timer2, representation, &is_end_of_word_r, &is_error_r);
		if (CW_SUCCESS != cw_ret) {
			fprintf(stderr, "[EE] Space: Failed to get representation\n");
			exit(EXIT_FAILURE);
		}

		if (is_end_of_word_r != is_end_of_word) {
			fprintf(stderr, "[EE] Space: 'is end of word' markers mismatch: %d != %d\n", is_end_of_word_r, is_end_of_word);
			exit(EXIT_FAILURE);
		}

		if (!is_end_of_word_r) {
			fprintf(stderr, "[EE] Space: 'is end of word' marker is unexpectedly 'false'\n");
			exit(EXIT_FAILURE);
		}
#endif

		cw_clear_receive_buffer();
		is_pending_inter_word_space = false;
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




#ifdef XCWCP_WITH_REC_TEST




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




void test_callback_func(void * arg, int key_state)
{
	/* Inform xcwcp receiver (which will inform libcw receiver)
	   about new state of straight key ("sk").

	   libcw receiver will process the new state and we will later
	   try to poll a character or space from it. */

	Receiver * xcwcp_receiver = (Receiver *) arg;
	//fprintf(stderr, "Callback function, key state = %d\n", key_state);
	xcwcp_receiver->sk_event(key_state);
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

	cw_rec_tester_init(&xcwcp_receiver->rec_tester);
	cw_rec_tester_init_text_buffers(&xcwcp_receiver->rec_tester, 1);


	/* Using Null sound system because this generator is only used
	   to enqueue text and control key. Sound will be played by
	   main generator used by xcwcp */
	cw_gen_config_t gen_conf;
	memset(&gen_conf, 0, sizeof (gen_conf));
	gen_conf.sound_system = CW_AUDIO_NULL;

	cw_gen_t * gen = cw_gen_new(&gen_conf);
	cw_key_t key;

	xcwcp_receiver->rec_tester.gen = gen;
	cw_tq_register_low_level_callback_internal(xcwcp_receiver->rec_tester.gen->tq, low_tone_queue_callback, &xcwcp_receiver->rec_tester, 5);

	cw_key_register_generator(&key, gen);
	cw_gen_register_value_tracking_callback_internal(gen, test_callback_func, arg);
	//cw_key_register_keying_callback(&key, test_callback_func, arg);

	/* Start sending the test string. Registered callback will be
	   called on every mark/space. Enqueue only initial part of
	   string, just to start sending, the rest should be sent by
	   'low watermark' callback. */
	cw_gen_start(gen);
	for (int i = 0; i < 5; i++) {
		const char c = xcwcp_receiver->rec_tester.input_string[xcwcp_receiver->rec_tester.input_string_i];
		if ('\0' == c) {
			/* A very short input string. */
			break;
		} else {
			cw_gen_enqueue_character(xcwcp_receiver->rec_tester.gen, c);
			xcwcp_receiver->rec_tester.input_string_i++;
		}
	}

	/* Wait for all characters to be played out. */
	cw_tq_wait_for_level_internal(gen->tq, 0);
	cw_usleep_internal(1000 * 1000);

	cw_gen_delete(&gen);
	xcwcp_receiver->rec_tester.generating_in_progress = false;

	cw_rec_tester_evaluate_receive_correctness(&xcwcp_receiver->rec_tester);


	return NULL;
}




void Receiver::start_test_code()
{
	pthread_create(&this->receiver_test_code_thread_id, NULL, receiver_input_generator_fn, this);
}




void Receiver::stop_test_code()
{
	pthread_cancel(this->receiver_test_code_thread_id);
}




#endif




}  /* namespace cw */
