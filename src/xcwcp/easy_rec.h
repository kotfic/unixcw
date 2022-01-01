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




#include <cw_rec_utils.h>
#ifdef XCWCP_WITH_REC_TEST
#include <cw_rec_tester.h>
#endif



#if defined(__cplusplus)
extern "C"
{
#endif





void easy_rec_start(cw_easy_receiver_t * easy_rec);
void easy_rec_clear(cw_easy_receiver_t * easy_rec);

bool easy_rec_poll_character(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd);
void easy_rec_poll_space(cw_easy_receiver_t * easy_rec, cw_rec_data_t * erd);

bool easy_rec_is_pending_inter_word_space(const cw_easy_receiver_t * easy_rec);

int easy_rec_get_libcw_errno(const cw_easy_receiver_t * easy_rec);
void easy_rec_clear_libcw_errno(cw_easy_receiver_t * easy_rec);


#ifdef XCWCP_WITH_REC_TEST
void easy_rec_start_test_code(cw_easy_receiver_t * easy_rec, cw_rec_tester_t * tester);
void easy_rec_stop_test_code(cw_rec_tester_t * tester);
#endif



#if defined(__cplusplus)
}
#endif




#endif // EASY_REC_H

