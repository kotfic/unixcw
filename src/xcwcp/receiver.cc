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
extern cw_rec_tester_t g_rec_tester;
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

	if (easy_rec_get_libcw_errno(easy_rec) != 0) {
		poll_report_error();
	}

	if (easy_rec_is_pending_inter_word_space(easy_rec)) {

		/**
		   If we received a character on an earlier poll, check again to see
		   if we need to revise the decision about whether it is the end of a
		   word too.
		*/
		/* Check if receiver received the pending inter-word
		   space. */
		cw_rec_data_t erd = { };
		easy_rec_poll_space(easy_rec, &erd);
		if (erd.is_iws) {
			//fprintf(stderr, "End of word '%c'\n\n", c);
			textarea->append(' ');
		}

		if (!easy_rec_is_pending_inter_word_space(easy_rec)) {
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
	cw_easy_receiver_sk_event(this->easy_rec, is_down);
	return;
}





/**
   \brief Handle event on left paddle of iambic keyer

   \param is_down
   \param is_reverse_paddles
*/
void Receiver::ik_left_event(bool is_down, bool is_reverse_paddles)
{
	cw_easy_receiver_ik_left_event(this->easy_rec, is_down, is_reverse_paddles);
	return;
}





/**
   \brief Handle event on right paddle of iambic keyer

   \param is_down
   \param is_reverse_paddles
*/
void Receiver::ik_right_event(bool is_down, bool is_reverse_paddles)
{
	cw_easy_receiver_ik_right_event(this->easy_rec, is_down, is_reverse_paddles);
	return;
}




/**
   \brief Clear the library receive buffer and our own flags
*/
void Receiver::clear()
{
	easy_rec_clear(easy_rec);
	return;
}





/**
   \brief Handle any error registered when handling a libcw keying event
*/
void Receiver::poll_report_error()
{
	/* Handle any receive errors detected on tone end but delayed until here. */

	switch (easy_rec_get_libcw_errno(easy_rec)) {
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

	easy_rec_clear_libcw_errno(easy_rec);

	return;
}





/**
   \brief Receive any new character from the CW library.
*/
void Receiver::poll_character()
{
	cw_rec_data_t erd = { };
	if (easy_rec_poll_character(this->easy_rec, &erd)) {
		/* Receiver stores full, well formed
		   character. Display it. */
		textarea->append(erd.character);

		/* Update the status bar to show the character
		   received.  Put the received char at the end of
		   string to avoid "jumping" of whole string when
		   width of glyph of received char changes at variable
		   font width. */
		QString status = _("Received at %1 WPM: '%2'");
		app->show_status(status.arg(cw_get_receive_speed()).arg(erd.character));
		//fprintf(stderr, "Received character '%c'\n", erd.character);

	} else {
		/* Handle receive error detected on trying to read a character. */
		switch (erd.errno_val) {
		case ENOENT:
			/* Invalid character in receiver's buffer. */
			textarea->append('?');
			app->show_status(QString(_("Unknown character received at %1 WPM")).arg(cw_get_receive_speed()));
			break;

		case EINVAL:
			/* Timestamp error. */
			textarea->append('?');
			app->show_status(QString(_("Internal error")));
			break;

		default:
			return;
		}
	}

	return;
}




#ifdef XCWCP_WITH_REC_TEST




void Receiver::start_test_code()
{
	this->easy_rec_tester = &g_rec_tester;
	easy_rec_start_test_code(this->easy_rec, this->easy_rec_tester);
}




void Receiver::stop_test_code()
{
	easy_rec_stop_test_code(this->easy_rec_tester);
}




#endif




}  /* namespace cw */
