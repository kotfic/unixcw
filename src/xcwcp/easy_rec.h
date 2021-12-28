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

#ifndef EASY_REC_H
#define EASY_REC_H




#include <stdbool.h>
#include <stdlib.h>




#ifdef XCWCP_WITH_REC_TEST
#include <cw_rec_tester.h>
#endif



#if defined(__cplusplus)
extern "C"
{
#endif




typedef struct easy_rec_t easy_rec_t;



typedef struct {
	char c;
	char representation[20]; /* TODO: use a constant for representation's size. */
	int errno_val;
	bool is_end_of_word;
	bool is_error;
} easy_rec_data_t;




/* CW library keying event handler. */
void easy_rec_handle_libcw_keying_event(easy_rec_t * easy_rec, int key_state);


easy_rec_t * easy_rec_new(void);
void easy_rec_delete(easy_rec_t ** easy_rec); // TODO: implement and use

void easy_rec_start(easy_rec_t * easy_rec);
void easy_rec_clear(easy_rec_t * easy_rec);

void easy_rec_sk_event(easy_rec_t * easy_rec, bool is_down);
void easy_rec_ik_left_event(easy_rec_t * easy_rec, bool is_down, bool is_reverse_paddles);
void easy_rec_ik_right_event(easy_rec_t * easy_rec, bool is_down, bool is_reverse_paddles);

bool easy_rec_poll_character(easy_rec_t * easy_rec, easy_rec_data_t * erd);
void easy_rec_poll_space(easy_rec_t * easy_rec, easy_rec_data_t * erd);

bool easy_rec_is_pending_inter_word_space(const easy_rec_t * easy_rec);

int easy_rec_get_libcw_errno(const easy_rec_t * easy_rec);
void easy_rec_clear_libcw_errno(easy_rec_t * easy_rec);


#ifdef XCWCP_WITH_REC_TEST
void easy_rec_start_test_code(easy_rec_t * easy_rec);
void easy_rec_stop_test_code(easy_rec_t * easy_rec);
#endif



#if defined(__cplusplus)
}
#endif




#endif // EASY_REC_H

