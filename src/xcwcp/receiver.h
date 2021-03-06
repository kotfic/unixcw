// Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
// Copyright (C) 2011-2021  Kamil Ignacak (acerion@wp.pl)
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

#ifndef H_XCWCP_RECEIVER
#define H_XCWCP_RECEIVER

#include <QMouseEvent>
#include <QKeyEvent>




#include "cw_rec_utils.h"
#include "cw_rec_tester.h"




namespace cw {





	class Application;
	class TextArea;
	class Mode;





	/* Class Receiver encapsulates the main application receiver
	   data and functions.  Receiver abstracts states associated
	   with receiving, event handling, libcw keyer event handling,
	   and data passed between signal handler and foreground
	   contexts. */
	class Receiver {
	public:
		Receiver(Application *a, TextArea *t) :
		app (a),
		textarea (t)
			{ easy_rec = cw_easy_receiver_new(); }

		~Receiver() { cw_easy_receiver_delete(&easy_rec); }

		/* Poll timeout handler. */
		void poll(const Mode *current_mode);

		/* Keyboard key event handler. */
		void handle_key_event(QKeyEvent *event, bool is_reverse_paddles);

		/* Mouse button press event handler. */
		void handle_mouse_event(QMouseEvent *event, bool is_reverse_paddles);

		/* Straight key and iambic keyer event handler
		   helpers. */
		void sk_event(bool is_down);
		void ik_left_event(bool is_down, bool is_reverse_paddles);
		void ik_right_event(bool is_down, bool is_reverse_paddles);

		/* CW library keying event handler. */
		void handle_libcw_keying_event(struct timeval *t, int key_state);

		/* Clear out queued data on stop, mode change, etc. */
		void clear();

#ifdef XCWCP_WITH_REC_TEST
		void start_test_code();
		void stop_test_code();
		cw_rec_tester_t rec_tester = {};
#endif
		cw_easy_receiver_t * easy_rec = nullptr;

	private:
		Application *app = nullptr;
		TextArea *textarea = nullptr;

		/* Poll primitives to handle receive errors,
		   characters, and inter-word spaces. */
		void poll_report_error();
		void poll_character();

		/* Prevent unwanted operations. */
		Receiver(const Receiver &);
		Receiver &operator=(const Receiver &);
	};





}  /* namespace cw */





#endif  /* #endif H_XCWCP_RECEIVER */
