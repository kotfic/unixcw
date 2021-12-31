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
#include <stdlib.h>
#include <string.h>

#include <libcw.h>

#include "cw_rec_utils.h"




cw_easy_receiver_t * cw_easy_receiver_new(void)
{
	return (cw_easy_receiver_t *) calloc(1, sizeof (cw_easy_receiver_t));
}




void cw_easy_receiver_delete(cw_easy_receiver_t ** easy_rec)
{
	if (easy_rec && *easy_rec) {
		free(*easy_rec);
		*easy_rec = NULL;
	}
}




void cw_easy_receiver_sk_event(cw_easy_receiver_t * easy_rec, bool is_down)
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




void cw_easy_receiver_ik_left_event(cw_easy_receiver_t * easy_rec, bool is_down, bool is_reverse_paddles)
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





void cw_easy_receiver_ik_right_event(cw_easy_receiver_t * easy_rec, bool is_down, bool is_reverse_paddles)
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

