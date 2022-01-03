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




#ifdef XCWCP_WITH_REC_TEST
cw_rec_tester_t g_rec_tester;
#endif



#ifdef XCWCP_WITH_REC_TEST
int easy_rec_test_on_character(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, struct timeval * timer);
int easy_rec_test_on_space(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, struct timeval * timer);
#endif




void easy_rec_start(cw_easy_receiver_t * easy_rec)
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








bool easy_rec_poll_character(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd)
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
	const bool received = cw_receive_character(&timer2, &erd->character, &erd->is_iws, NULL);
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
		easy_rec->is_pending_iws = true;

		//fprintf(stderr, "Received character '%c'\n", erd->character);

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
void easy_rec_poll_space(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd)
{
	/* We expect the receiver to contain a character, but we don't
	   ask for it this time. The receiver should also store
	   information about an inter-character space. If it is longer
	   than a regular inter-character space, then the receiver
	   will treat it as inter-word space, and communicate it over
	   is_iws.

	   Don't use receiver.easy_rec->main_timer - it is used eclusively for
	   marking initial "key down" events. Use local throw-away
	   timer2. */
	struct timeval timer2;
	gettimeofday(&timer2, NULL);
	//fprintf(stderr, "poll_space(): %10ld : %10ld\n", timer2.tv_sec, timer2.tv_usec);

	cw_receive_character(&timer2, &erd->character, &erd->is_iws, NULL);
	if (erd->is_iws) {
		//fprintf(stderr, "End of word '%c'\n\n", erd->character);

#ifdef XCWCP_WITH_REC_TEST
		if (CW_SUCCESS != easy_rec_test_on_space(easy_rec, erd, &timer2)) {
			exit(EXIT_FAILURE);
		}
#endif

		cw_clear_receive_buffer();
		easy_rec->is_pending_iws = false;
	} else {
		/* We don't reset easy_rec->is_pending_iws. The
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



int easy_rec_get_libcw_errno(const cw_easy_receiver_t * easy_rec)
{
	return easy_rec->libcw_receive_errno;
}


void easy_rec_clear_libcw_errno(cw_easy_receiver_t * easy_rec)
{
	easy_rec->libcw_receive_errno = 0;
}



bool easy_rec_is_pending_inter_word_space(const cw_easy_receiver_t * easy_rec)
{
	return easy_rec->is_pending_iws;
}




void easy_rec_clear(cw_easy_receiver_t * easy_rec)
{
	cw_clear_receive_buffer();
	easy_rec->is_pending_iws = false;
	easy_rec->libcw_receive_errno = 0;
	easy_rec->tracked_key_state = false;
}




#ifdef XCWCP_WITH_REC_TEST




int easy_rec_test_on_character(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, struct timeval * timer)
{
	fprintf(stderr, "[II] Character: '%c'\n", erd->character);

	g_rec_tester.received_string[g_rec_tester.received_string_i++] = erd->character;

	cw_rec_data_t test_data = { 0 };
	int cw_ret = cw_receive_representation(timer, test_data.representation, &test_data.is_iws, &test_data.is_error);
	if (CW_SUCCESS != cw_ret) {
		fprintf(stderr, "[EE] Character: failed to get representation\n");
		return CW_FAILURE;
	}

	if (test_data.is_iws != erd->is_iws) {
		fprintf(stderr, "[EE] Character: 'is end of word' markers mismatch: %d != %d\n", test_data.is_iws, erd->is_iws);
		return CW_FAILURE;
	}

	if (test_data.is_iws) {
		fprintf(stderr, "[EE] Character: 'is end of word' marker is unexpectedly 'true'\n");
		return CW_FAILURE;
	}

	const char looked_up = cw_representation_to_character(test_data.representation);
	if (0 == looked_up) {
		fprintf(stderr, "[EE] Character: Failed to look up character for representation\n");
		return CW_FAILURE;
	}

	if (looked_up != erd->character) {
		fprintf(stderr, "[EE] Character: Looked up character is different than received: %c != %c\n", looked_up, erd->character);
	}

	fprintf(stderr, "[II] Character: Representation: %c -> '%s'\n",
		erd->character, test_data.representation);

	/* Not entirely a success if looked up char does not match received
	   character, but returning failure here would lead to calling
	   exit(). */
	return CW_SUCCESS;
}




int easy_rec_test_on_space(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd, struct timeval * timer)
{
	fprintf(stderr, "[II] Space:\n");

	/* cw_receive_character() will return through 'c' variable the last
	   character that was polled before space.

	   Maybe this is good, maybe this is bad, but this is the legacy
	   behaviour that we will keep supporting. */
	if (' ' == erd->character) {
		fprintf(stderr, "[EE] Space: returned character should not be space\n");
		return CW_FAILURE;
	}


	g_rec_tester.received_string[g_rec_tester.received_string_i++] = ' ';

	cw_rec_data_t test_data = { 0 };
	int cw_ret = cw_receive_representation(timer, test_data.representation, &test_data.is_iws, &test_data.is_error);
	if (CW_SUCCESS != cw_ret) {
		fprintf(stderr, "[EE] Space: Failed to get representation\n");
		return CW_FAILURE;
	}

	if (test_data.is_iws != erd->is_iws) {
		fprintf(stderr, "[EE] Space: 'is end of word' markers mismatch: %d != %d\n", test_data.is_iws, erd->is_iws);
		return CW_FAILURE;
	}

	if (!test_data.is_iws) {
		fprintf(stderr, "[EE] Space: 'is end of word' marker is unexpectedly 'false'\n");
		return CW_FAILURE;
	}

	return CW_SUCCESS;
}




#endif

