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


/**
   @file libcw_key.c

   @brief Straight key and iambic keyer.
*/




#include <errno.h>
#include <inttypes.h> /* uint32_t */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>   /* usleep() */




#include "libcw2.h"
#include "libcw_debug.h"
#include "libcw_gen.h"
#include "libcw_key.h"
#include "libcw_rec.h"
#include "libcw_signal.h"
#include "libcw_utils.h"




#define MSG_PREFIX_SK "libcw/sk: "
#define MSG_PREFIX_IK "libcw/ik: "
#define MSG_PREFIX    "libcw/key: "




extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;




/* ******************************************************************** */
/*                       Section:Keying control                         */
/* ******************************************************************** */
/**
   Code maintaining value of a key, and handling changes of key value.
   A key can have two values:
   @li open - a physical key with electric contacts open, no sound or
   continuous wave is generated;
   @li closed - a physical key with electric contacts closed, a sound
   or continuous wave is generated;

   Key type is not specified. This code maintains value of any type of key:
   straight key, cootie key, iambic key. All that matters is value of
   contacts (open/closed).

   Client code can register a client callback function using
   cw_register_keying_callback(). The function will be called every time a
   generator associated with the key changes value, i.e. each time value of
   key will lead to (will be reflected by) change of state of the generator.
*/




/* ******************************************************************** */
/*                        Section:Iambic keyer                          */
/* ******************************************************************** */




/*
 * The CW keyer functions implement the following state graph:
 *
 *        +-----------------------------------------------------+
 *        |          (all latches clear)                        |
 *        |                                     (dot latch)     |
 *        |                          +--------------------------+
 *        |                          |                          |
 *        |                          v                          |
 *        |      +-------------> KS_IN_DOT_[A|B] -------> KS_AFTER_DOT_[A|B]
 *        |      |(dot paddle)       ^            (delay)       |
 *        |      |                   |                          |(dash latch/
 *        |      |                   +------------+             | _B)
 *        v      |                                |             |
 * --> KS_IDLE --+                   +--------------------------+
 *        ^      |                   |            |
 *        |      |                   |            +-------------+(dot latch/
 *        |      |                   |                          | _B)
 *        |      |(dash paddle)      v            (delay)       |
 *        |      +-------------> KS_IN_DASH_[A|B] -------> KS_AFTER_DASH_[A|B]
 *        |                          ^                          |
 *        |                          |                          |
 *        |                          +--------------------------+
 *        |                                     (dash latch)    |
 *        |          (all latches clear)                        |
 *        +-----------------------------------------------------+
 */





/* See also enum of int values, declared in libcw_key.h. */
static const char * cw_iambic_keyer_graph_states[] = {
	"KS_IDLE",
	"KS_IN_DOT_A",
	"KS_IN_DASH_A",
	"KS_AFTER_DOT_A",
	"KS_AFTER_DASH_A",
	"KS_IN_DOT_B",
	"KS_IN_DASH_B",
	"KS_AFTER_DOT_B",
	"KS_AFTER_DASH_B"
};




static cw_ret_t cw_key_ik_update_graph_state_initial_internal(volatile cw_key_t * key);
static cw_ret_t cw_key_ik_set_value_internal(volatile cw_key_t * key, cw_key_value_t key_value, char symbol);
static cw_ret_t cw_key_sk_set_value_internal(volatile cw_key_t * key, cw_key_value_t key_value);




/* ******************************************************************** */
/*                       Section:Keying control                         */
/* ******************************************************************** */




/**
   Most of the time libcw just passes around key_callback_arg, not caring of
   what type it is, and not attempting to do any operations on it. On one
   occasion however, it needs to know whether key_callback_arg is of type
   'struct timeval', and if so, it must do some operation on it. I could pass
   struct with ID as key_callback_arg, but that may break some old client
   code. Instead I've created this function that has only one, very specific
   purpose: to pass to libcw a pointer to timer.

   The timer is owned by client code, and is used to measure and clock
   iambic keyer.

   @param[in] key
   @param[in] timer
*/
void cw_key_ik_register_timer_internal(volatile cw_key_t * key, struct timeval * timer)
{
#ifdef IAMBIC_KEY_HAS_TIMER
	key->ik.ik_timer = timer;
#endif

	return;
}




/**
   Comment for key used as iambic keyer:
   Iambic keyer cannot function without an associated generator. A
   keyer has to have some generator to function correctly. Generator
   doesn't care if it has any key registered or not. Thus a function
   binding a keyer and generator belongs to "iambic keyer" module.

   Remember that a generator can exist without a keyer. In applications
   that do nothing related to keying with iambic keyer, having just a
   generator is a valid situation.

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param[in,out] key key that needs to have a generator associated with it
   @param[in,out] gen generator to be used with given keyer
*/
void cw_key_register_generator(volatile cw_key_t * key, cw_gen_t * gen)
{
	if (NULL != key) {
		key->gen = gen;
	} else {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_WARNING,
			      MSG_PREFIX "Passed NULL key pointer");
	}
	if (NULL != gen) {
		gen->key = key;
	} else {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_WARNING,
			      MSG_PREFIX "Passed NULL gen pointer");
	}

	return;
}




/**
   Receiver should somehow receive key events from physical or logical
   key. This can be done in one of two ways:

   1. key events -> key variable -> cw_gen_state_tracking_set_value_internal() ->
      -> registered receiver -> cw_rec_mark_{begin|end}(key->rec, ...)
   2. key events -> key variable -> cw_gen_state_tracking_set_value_internal() ->
      -> registered callback function-> key->key_callback_function()
      -> cw_rec_mark_{begin|end}(rec, ...)

   When using the first way, there should be a binding between key and
   a receiver.

   The receiver can get it's properly formed input data (key down/key
   up events) from any source, so it's independent from key. On the
   other hand the key without receiver is rather useless. Therefore I
   think that the key should contain reference to a receiver, not the
   other way around.

   @internal
   @reviewed 2020-08-29
   @endinternal

   @param[in,out] key key that needs to have a receiver associated with it
   @param[in] rec receiver to be used with given key
*/
void cw_key_register_receiver(volatile cw_key_t * key, cw_rec_t * rec)
{
	if (NULL != key) {
		key->rec = rec;
	} else {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_WARNING,
			      MSG_PREFIX "Passed NULL key pointer");
	}

	return;
}




/**
   @brief Set new key value, generate appropriate tone (Mark/Space)

   Set new value of a key. Filter successive key-down or key-up
   actions into a single action (successive calls with the same value
   of @p key_value don't change internally registered value of key).

   If and only if the function recognizes change of key value, a state of
   related generator @p gen is changed accordingly (a tone is started or
   stopped).

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param[in] key key in use
   @param[in] key_value key value to be set

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_key_sk_set_value_internal(volatile cw_key_t * key, cw_key_value_t key_value)
{
	cw_assert (key, MSG_PREFIX_SK "key is NULL");
	cw_assert (key->gen, MSG_PREFIX_SK "gen is NULL");

#if 0 /* Disabled on 2020-08-03. Timer moved to iambic keyer only. */
	struct timeval t;
	gettimeofday(&t, NULL); /* TODO: isn't gettimeofday() susceptible to NTP syncs? */
	key->timer.tv_sec = t.tv_sec;
	key->timer.tv_usec = t.tv_usec;
#endif

	if (key->sk.key_value == key_value) {
		/* This may happen when dequeueing 'forever' tone multiple
		   times in a row.  TODO: in what situations do we call this
		   function when dequeueing 'forever' tone? It seems that
		   dequeueing 'forever' tone shouldn't lead to calling this
		   function. */
		return CW_SUCCESS;
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYING, CW_DEBUG_INFO,
		      MSG_PREFIX_SK "sk set value %d->%d", key->sk.key_value, key_value);

	/* Remember the new key value. */
	key->sk.key_value = key_value;

	/* TODO: if you want to have a per-key callback called on each key
	  value change, you should call it here. */

	cw_ret_t cwret = CW_FAILURE;
	if (key->sk.key_value == CW_KEY_VALUE_CLOSED) {
		/* In case of straight key we don't know at
		   all how long the tone should be (we don't
		   know for how long the key will be closed).

		   Let's enqueue a beginning of mark. A
		   constant tone will be generated until function
		   receives CW_KEY_VALUE_OPEN key value. */
		cwret = cw_gen_enqueue_begin_mark_internal(key->gen);
	} else {
		/* CW_KEY_VALUE_OPEN, time to go from Mark
		   (audible tone) to Space (silence). */
		cwret = cw_gen_enqueue_begin_space_internal(key->gen);
	}
	cw_assert (CW_SUCCESS == cwret, MSG_PREFIX_SK "failed to enqueue key value %d", key->sk.key_value);
	return cwret;
}





/**
   @brief Enqueue a symbol (Mark/Space) in queue of a generator related to a key

   This function is called when iambic keyer enters new graph state (as
   described by keyer's state graph). The keyer needs some mechanism to
   control itself, to control when to move out of current graph state into
   next graph state. The movement between graph states must be done in
   specific time periods. Iambic keyer needs to be notified whenever a
   specific time period has elapsed.

   Durations of the enqueued periods are determined by type of @p symbol
   (Space, Dot, Dash).

   Generator and its tone queue is used to implement this mechanism.
   The function enqueues a tone/symbol (Mark or Space) of specific
   length - this is the beginning of period when keyer is in new graph
   state. Then generator dequeues the tone/symbol, counts the time
   period, and (at the end of the tone/period) notifies keyer about
   end of period. (Keyer then needs to evaluate values of paddles and
   decide what's next, but that is a different story).

   As a side effect of using generator, a sound is generated (if
   generator's sound system is not Null).

   Function also calls external callback function for keying on every change
   of key's value (if the callback has been registered by client code). Key's
   value is passed to the callback as argument. Callback is called by this
   function only when there is a change of key value - this function filters
   successive key-down or key-up actions into a single action.  TODO: this is
   not entirely precise. The callback will be called from generator, after
   generator notices change of its state.

   TODO: explain difference and relation between key's value and
   keyer's graph state.

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param[in] key current key
   @param[in] key_value key value to be set
   @param[in] symbol symbol to enqueue (Space, Dot, Dash)

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_key_ik_set_value_internal(volatile cw_key_t * key, cw_key_value_t key_value, char symbol)
{
	cw_assert (key, MSG_PREFIX_IK "key is NULL");
	cw_assert (key->gen, MSG_PREFIX_IK "gen is NULL");

	if (key->ik.key_value == key_value) {
		/* This is not an error. This may happen when dequeueing
		   'forever' tone multiple times in a row. TODO: in what
		   situations do we call this function when dequeueing
		   'forever' tone? It seems that dequeueing 'forever' tone
		   shouldn't lead to calling this function. */
		return CW_SUCCESS;
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYING, CW_DEBUG_INFO,
		      MSG_PREFIX_IK "ik set value %d->%d (symbol '%c')", key->ik.key_value, key_value, symbol);

	/* Remember the new key value. */
	key->ik.key_value = key_value;

	/* TODO: if you want to have a per-key callback called on each key value
	  change, you should call it here. */

	cw_ret_t cwret = cw_gen_enqueue_symbol_no_ims_internal(key->gen, symbol);
	cw_assert (CW_SUCCESS == cwret, MSG_PREFIX_IK "failed to key symbol '%c'", symbol);
	return cwret;
}




/* ******************************************************************** */
/*                        Section:Iambic keyer                          */
/* ******************************************************************** */




/**
   @brief Enable iambic Curtis mode B

   Normally, the iambic keying functions will emulate Curtis 8044 Keyer
   mode A.  In this mode, when both paddles are pressed together, the
   last dot or dash being sent on release is completed, and nothing else
   is sent. In mode B, when both paddles are pressed together, the last
   dot or dash being sent on release is completed, then an opposite
   element is also sent. Some operators prefer mode B, but timing is
   more critical in this mode. The default mode is Curtis mode A.

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param[in] key key for which to change the parameter
*/
void cw_key_ik_enable_curtis_mode_b(volatile cw_key_t * key)
{
	key->ik.curtis_mode_b = true;
	return;
}




/**
   See documentation of cw_key_ik_enable_curtis_mode_b() for more information

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param[in] key key for which to change the parameter
*/
void cw_key_ik_disable_curtis_mode_b(volatile cw_key_t * key)
{
	key->ik.curtis_mode_b = false;
	return;
}




/**
   See documentation of cw_enable_iambic_curtis_mode_b() for more information

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param[in] key key to investigate

   @return true if Curtis mode is enabled for the key
   @return false otherwise
*/
bool cw_key_ik_get_curtis_mode_b(const volatile cw_key_t *key)
{
	return key->ik.curtis_mode_b;
}




/**
   @brief Update graph state of iambic keyer, enqueue tone representing value of the iambic keyer

   It seems that the function is called when a client code informs
   about change of value of one of paddles. So I think what the
   function does is that it takes the new value of paddles and
   re-evaluate internal graph state of iambic keyer.

   The function is also called in generator's thread function
   cw_gen_dequeue_and_generate_internal() each time a tone has been dequeued,
   pushed to sound system and played in full. After this playing is
   completed, i.e. after a duration of Mark or Space, the generator calls
   this function to inform iambic keyer that it's time for the keyer to
   update its internal graph state.

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param[in] key iambic key

   @return CW_FAILURE if there is a lock and the function cannot proceed
   @return CW_SUCCESS otherwise
*/
cw_ret_t cw_key_ik_update_graph_state_internal(volatile cw_key_t * key)
{
	if (NULL == key) {
		/* This function is called from generator thread. It
		   is perfectly valid situation that for some
		   applications a generator exists, but a keyer does
		   not exist.  Silently accept this situation.

		   TODO: move this check earlier in call stack, so
		   that less functions are called before silently
		   discovering that key doesn't exist. */
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_INTERNAL, CW_DEBUG_DEBUG,
			      MSG_PREFIX_IK "ik update: NULL key, silently accepting");
		return CW_SUCCESS;
	}

	/* Iambic keyer needs a generator to measure times, so the generator
	   must exist. Be paranoid and check it, just in case. */
	cw_assert (key->gen, MSG_PREFIX_IK "gen is NULL");


	/* TODO: this is not the safest locking in the world.
	   TODO: why do we need the locking at all? */
	if (key->ik.lock) {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_INTERNAL, CW_DEBUG_ERROR,
			      MSG_PREFIX_IK "ik update: lock in thread %ld", (long) pthread_self());
		return CW_FAILURE;
	}
	key->ik.lock = true;

	/* Synchronize low level timing parameters if required. */
	if (key->gen) {
		cw_gen_sync_parameters_internal(key->gen);
	}
	if (key->rec) {
		cw_rec_sync_parameters_internal(key->rec);
	}

	const int old_graph_state = key->ik.graph_state;

	/* Decide what to do based on the current graph state. */
	switch (key->ik.graph_state) {
	case KS_IDLE:
		/* Ignore calls if our graph state is idle. The initial nudge
		   from initial graph state should be done by code that
		   receives inputs from client code by calling
		   cw_key_ik_update_graph_state_initial_internal(). */
		key->ik.lock = false;
		return CW_SUCCESS;

	case KS_IN_DOT_A:
	case KS_IN_DOT_B:
		/* Verify that key value and keyer graph state are in
		   sync.  We are *at the end* of Mark (Dot), so key should
		   be (still) closed. */
		cw_assert (key->ik.key_value == CW_KEY_VALUE_CLOSED,
			   MSG_PREFIX_IK "inconsistency between keyer graph state (%s) and key value (%d)",
			   cw_iambic_keyer_graph_states[key->ik.graph_state], key->ik.key_value);

		/* We are ending a Dot, so turn off tone and begin the
		   after-dot Space.
		   No routine status checks are made! (TODO) */
		cw_key_ik_set_value_internal(key, CW_KEY_VALUE_OPEN, CW_SYMBOL_SPACE);
		key->ik.graph_state = key->ik.graph_state == KS_IN_DOT_A ? KS_AFTER_DOT_A : KS_AFTER_DOT_B;
		break;

	case KS_IN_DASH_A:
	case KS_IN_DASH_B:
		/* Verify that key value and keyer graph state are in
		   sync.  We are *at the end* of Mark (Dash), so key should
		   be (still) closed. */
		cw_assert (key->ik.key_value == CW_KEY_VALUE_CLOSED,
			   MSG_PREFIX_IK "inconsistency between keyer graph state (%s) and key value (%d)",
			   cw_iambic_keyer_graph_states[key->ik.graph_state], key->ik.key_value);

		/* We are ending a Dash, so turn off tone and begin
		   the after-dash Space.
		   No routine status checks are made! (TODO) */
		cw_key_ik_set_value_internal(key, CW_KEY_VALUE_OPEN, CW_SYMBOL_SPACE);
		key->ik.graph_state = key->ik.graph_state == KS_IN_DASH_A ? KS_AFTER_DASH_A : KS_AFTER_DASH_B;
		break;

	case KS_AFTER_DOT_A:
	case KS_AFTER_DOT_B:
		/* Verify that key value and keyer graph state are in
		   sync.  We are *at the end* of Space, so key should
		   be (still) open. */
		cw_assert (key->ik.key_value == CW_KEY_VALUE_OPEN,
			   MSG_PREFIX_IK "inconsistency between keyer graph state (%s) and key value (%d)",
			   cw_iambic_keyer_graph_states[key->ik.graph_state], key->ik.key_value);

		/* If we have just finished a Dot or a Dash and its
		   post-mark delay, then reset the latches as
		   appropriate.  Next, if in a _B graph state, go straight
		   to the opposite element graph state.  If in an _A graph state,
		   check the latch states; if the opposite latch is
		   set true, then do the iambic thing and alternate
		   dots and dashes.  If the same latch is true,
		   repeat.  And if nothing is true, then revert to
		   idling. */

		if (CW_KEY_VALUE_OPEN == key->ik.dot_paddle_value) {
			/* Client has informed us that dot paddle has
			   been released. Clear the paddle state
			   memory. */
			key->ik.dot_latch = false;
		}

		if (key->ik.graph_state == KS_AFTER_DOT_B) {
			cw_key_ik_set_value_internal(key, CW_KEY_VALUE_CLOSED, CW_DASH_REPRESENTATION);
			key->ik.graph_state = KS_IN_DASH_A;

		} else if (key->ik.dash_latch) {
			cw_key_ik_set_value_internal(key, CW_KEY_VALUE_CLOSED, CW_DASH_REPRESENTATION);
			if (key->ik.curtis_b_latch) {
				key->ik.curtis_b_latch = false;
				key->ik.graph_state = KS_IN_DASH_B;
			} else {
				key->ik.graph_state = KS_IN_DASH_A;
			}
		} else if (key->ik.dot_latch) {
			cw_key_ik_set_value_internal(key, CW_KEY_VALUE_CLOSED, CW_DOT_REPRESENTATION);
			key->ik.graph_state = KS_IN_DOT_A;
		} else {
			key->ik.graph_state = KS_IDLE;
			//cw_finalization_schedule_internal();
		}

		break;

	case KS_AFTER_DASH_A:
	case KS_AFTER_DASH_B:
		/* Verify that key value and keyer graph state are in
		   sync.  We are *at the end* of Space, so key should
		   be (still) open. */
		cw_assert (key->ik.key_value == CW_KEY_VALUE_OPEN,
			   MSG_PREFIX_IK "inconsistency between keyer graph state (%s) and key value (%d)",
			   cw_iambic_keyer_graph_states[key->ik.graph_state], key->ik.key_value);

		if (CW_KEY_VALUE_OPEN == key->ik.dash_paddle_value) {
			/* Client has informed us that dash paddle has
			   been released. Clear the paddle state
			   memory. */
			key->ik.dash_latch = false;
		}

		/* If we have just finished a dot or a dash and its
		   post-mark delay, then reset the latches as
		   appropriate.  Next, if in a _B graph state, go straight
		   to the opposite element graph state.  If in an _A graph state,
		   check the latch states; if the opposite latch is
		   set true, then do the iambic thing and alternate
		   dots and dashes.  If the same latch is true,
		   repeat.  And if nothing is true, then revert to
		   idling. */

		if (key->ik.graph_state == KS_AFTER_DASH_B) {
			cw_key_ik_set_value_internal(key, CW_KEY_VALUE_CLOSED, CW_DOT_REPRESENTATION);
			key->ik.graph_state = KS_IN_DOT_A;

		} else if (key->ik.dot_latch) {
			cw_key_ik_set_value_internal(key, CW_KEY_VALUE_CLOSED, CW_DOT_REPRESENTATION);
			if (key->ik.curtis_b_latch) {
				key->ik.curtis_b_latch = false;
				key->ik.graph_state = KS_IN_DOT_B;
			} else {
				key->ik.graph_state = KS_IN_DOT_A;
			}
		} else if (key->ik.dash_latch) {
			cw_key_ik_set_value_internal(key, CW_KEY_VALUE_CLOSED, CW_DASH_REPRESENTATION);
			key->ik.graph_state = KS_IN_DASH_A;
		} else {
			key->ik.graph_state = KS_IDLE;
			//cw_finalization_schedule_internal();
		}

		break;
	default:
		cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYER_STATES, CW_DEBUG_ERROR,
			      MSG_PREFIX_IK "ik update: invalid keyer graph state %d",
			      key->ik.graph_state);
		key->ik.lock = false;
		return CW_FAILURE;
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYER_STATES, CW_DEBUG_INFO,
		      MSG_PREFIX_IK "ik update: keyer graph state: %s -> %s",
		      cw_iambic_keyer_graph_states[old_graph_state],
		      cw_iambic_keyer_graph_states[key->ik.graph_state]);

	key->ik.lock = false;
	return CW_SUCCESS;
}




/**
   @brief Inform iambic keyer logic about changed value(s) of iambic keyer's paddles

   Function informs the @p key that the iambic keyer paddles have changed
   value.  The new paddle values are recorded, and if either transition from
   CW_KEY_VALUE_OPEN to CW_KEY_VALUE_CLOSED, paddle latches (for iambic
   functions) are also set.

   If appropriate, this function nudges the @p key from its initial IDLE
   state, setting graph state machine into motion.  Sending the initial
   element is done in the background, so this routine returns almost
   immediately.

   @exception EBUSY if the tone queue or straight key are using the sound
   card, console speaker, or keying system.
   @internal
   TODO: clarify the above statement about "using X, Y, or Z"
   @endinternal

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key to notify about changed values of paddles
   @param[in] dot_paddle_value value of dot paddle
   @param[in] dash_paddle_value value of dash paddle

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_key_ik_notify_paddle_event(volatile cw_key_t * key, cw_key_value_t dot_paddle_value, cw_key_value_t dash_paddle_value)
{
#if 0 /* This is disabled, but I'm not sure why. */  /* This code has been disabled some time before 2017-01-31. */
	/* If the tone queue or the straight key are busy, this is going to
	   conflict with our use of the sound card, console sounder, and
	   keying system.  So return an error status in this case. */
	if (cw_tq_is_nonempty_internal(key->gen->tq) || key->sk.key_value == CW_KEY_VALUE_CLOSED) {
		errno = EBUSY;
		return CW_FAILURE;
	}
#endif

	/* Clean up and save the paddle value passed in. */
#if 0    /* This code has been disabled on 2017-01-31. */
	key->ik.dot_paddle_value = (dot_paddle_value != 0);
	key->ik.dash_paddle_value = (dash_paddle_value != 0);
#else
	key->ik.dot_paddle_value = dot_paddle_value;
	key->ik.dash_paddle_value = dash_paddle_value;
#endif

	/* Update the paddle latches if either paddle goes CLOSED.
	   The latches are checked in the signal handler, so if the
	   paddles go back to OPEN during this element, the item still
	   gets actioned.  The signal handler is also responsible for
	   clearing down the latches. TODO: verify the comment. */
	if (CW_KEY_VALUE_CLOSED == key->ik.dot_paddle_value) {
		key->ik.dot_latch = true;
	}
	if (CW_KEY_VALUE_CLOSED == key->ik.dash_paddle_value) {
		key->ik.dash_latch = true;
	}


	if (key->ik.curtis_mode_b
	    && CW_KEY_VALUE_CLOSED == key->ik.dot_paddle_value
	    && CW_KEY_VALUE_CLOSED == key->ik.dash_paddle_value) {

		/* Both paddles closed at the same time in Curtis mode B.

		   This flag is checked by the signal handler, to
		   determine whether to add mode B trailing timing
		   elements. TODO: verify this comment. */
		key->ik.curtis_b_latch = true;
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYER_STATES, CW_DEBUG_INFO,
		      MSG_PREFIX_IK "ik notify: paddles %d,%d, latches %d,%d, curtis_b %d",
		      key->ik.dot_paddle_value, key->ik.dash_paddle_value,
		      key->ik.dot_latch, key->ik.dash_latch, key->ik.curtis_b_latch);


	if (key->ik.graph_state == KS_IDLE) {
		/* If the current graph state is idle, give the graph state
		   process an initial impulse. */
		return cw_key_ik_update_graph_state_initial_internal(key);
	} else {
		/* The graph state machine for iambic keyer is already in
		   motion, no need to do anything more.

		   Current paddle values have been recorded in this
		   function. Any future changes of paddle values will
		   be also recorded by this function.

		   In both cases the main action upon values of
		   paddles and paddle latches is taken in
		   cw_key_ik_update_graph_state_internal(). */
		return CW_SUCCESS;
	}
}




/**
   @brief Initiate work of iambic keyer graph state machine

   Graph state machine for iambic keyer must be pushed from KS_IDLE graph
   state. Call this function to do this.

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key for which to initiate its work

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
static cw_ret_t cw_key_ik_update_graph_state_initial_internal(volatile cw_key_t * key)
{
	cw_assert (key, MSG_PREFIX_IK "key is NULL");
	cw_assert (key->gen, MSG_PREFIX_IK "gen is NULL");

#ifdef IAMBIC_KEY_HAS_TIMER
	if (NULL != key->ik.ik_timer) {
		struct timeval t;
		gettimeofday(&t, NULL); /* TODO: isn't gettimeofday() susceptible to NTP syncs? */
		key->ik.ik_timer->tv_sec = t.tv_sec;
		key->ik.ik_timer->tv_usec = t.tv_usec;
	}
#endif

	if (CW_KEY_VALUE_OPEN == key->ik.dot_paddle_value && CW_KEY_VALUE_OPEN == key->ik.dash_paddle_value) {
		/* Both paddles are open/up. We certainly don't want
		   to start any process upon "both paddles open"
		   event. But the function shouldn't have been called
		   in that situation. */
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_KEYER_STATES, CW_DEBUG_ERROR,
			      MSG_PREFIX_IK "ik update initial: called the function when both paddles are open");

		/* Silently accept.
		   TODO: maybe it's a good idea, or maybe bad one to
		   return CW_SUCCESS here. */
		return CW_SUCCESS;
	}

	const int old_graph_state = key->ik.graph_state;

	if (CW_KEY_VALUE_CLOSED == key->ik.dot_paddle_value) {
		/* "Dot" paddle pressed. Pretend that we are in "after
		   dash" space, so that keyer will have to transit
		   into "dot" mark graph state. */
		key->ik.graph_state = key->ik.curtis_b_latch
			? KS_AFTER_DASH_B : KS_AFTER_DASH_A;

	} else { /* key->ik.dash_paddle == CW_KEY_VALUE_CLOSED */
		/* "Dash" paddle pressed. Pretend that we are in
		   "after dot" space, so that keyer will have to
		   transit into "dash" mark graph state. */

		key->ik.graph_state = key->ik.curtis_b_latch
			? KS_AFTER_DOT_B : KS_AFTER_DOT_A;
	}

	cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_KEYER_STATES, CW_DEBUG_DEBUG,
		      MSG_PREFIX_IK "ik update initial: keyer graph state: %s -> %s",
		      cw_iambic_keyer_graph_states[old_graph_state],
		      cw_iambic_keyer_graph_states[key->ik.graph_state]);


	/* Here comes the "real" initial transition - this is why we
	   called this function. We will transition from graph state set
	   above into "real" graph state, reflecting values of paddles. */
	cw_ret_t cwret = cw_key_ik_update_graph_state_internal(key);
	if (CW_FAILURE == cwret) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYER_STATES, CW_DEBUG_ERROR,
			      MSG_PREFIX_IK "ik update initial: call to update_graph_state_initial() failed first time");
		/* Just try again, once. */
		usleep(1000);
		cwret = cw_key_ik_update_graph_state_internal(key);
		if (CW_FAILURE == cwret) {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYER_STATES, CW_DEBUG_ERROR,
				      MSG_PREFIX_IK "ik update initial: call to update_graph_state_initial() failed twice");
		}
	}

	return cwret;
}




/**
   @brief Change value of Dot paddle

   Alter the value of Dot paddle. Value of Dash paddle remains unchanged.

   See cw_key_ik_notify_paddle_event() for details of iambic
   keyer background processing.

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key for which to change value of Dot paddle
   @param[in] dot_paddle_value new value of Dot paddle

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_key_ik_notify_dot_paddle_event(volatile cw_key_t * key, cw_key_value_t dot_paddle_value)
{
	return cw_key_ik_notify_paddle_event(key, dot_paddle_value, key->ik.dash_paddle_value);
}




/**
   @brief Change value of Dash paddle

   Alter the value of Dash paddle. Value of Dot paddle remains unchanged.

   See documentation of cw_notify_keyer_dot_paddle_event() for more information

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key for which to change value of Dash paddle
   @param[in] dash_paddle_value new value of Dash paddle

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_key_ik_notify_dash_paddle_event(volatile cw_key_t * key, cw_key_value_t dash_paddle_value)
{
	return cw_key_ik_notify_paddle_event(key, key->ik.dot_paddle_value, dash_paddle_value);
}




/**
   @brief Get the current saved values of the two paddles

   Either of the last two arguments can be NULL - it won't be updated then.

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key from which to get values of paddles
   @param[out] dot_paddle_value current value of Dot paddle
   @param[out] dash_paddle_value current value of Dash paddle
*/
void cw_key_ik_get_paddles(const volatile cw_key_t * key, cw_key_value_t * dot_paddle_value, cw_key_value_t * dash_paddle_value)
{
	if (dot_paddle_value) {
		*dot_paddle_value = key->ik.dot_paddle_value;
	}
	if (dash_paddle_value) {
		*dash_paddle_value = key->ik.dash_paddle_value;
	}
	return;
}




/**
   @brief Get the current states of paddle latches

   Function returns the current saved states of the two paddle latches.
   A paddle latch is set to true when the paddle state becomes CLOSED,
   and is cleared if the paddle state is OPEN when the element finishes
   sending.

   Either of the last two arguments can be NULL - it won't be updated then.

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key from which to get values of latches
   @param[out] dot_paddle_latch_state current state of Dot paddle latch. Will be updated with true or false. TODO: true/false or OPEN/CLOSED?
   @param[out] dash_paddle_latch_state current state of Dash paddle latch. Will be updated with true or false. TODO: true/false or OPEN/CLOSED?
*/
void cw_key_ik_get_paddle_latches_internal(volatile cw_key_t * key, /* out */ int * dot_paddle_latch_state, /* out */ int * dash_paddle_latch_state)
{
	if (dot_paddle_latch_state) {
		*dot_paddle_latch_state = key->ik.dot_latch;
	}
	if (dash_paddle_latch_state) {
		*dash_paddle_latch_state = key->ik.dash_latch;
	}
	return;
}




/**
   @brief Check if a keyer is busy

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key which business to check

   @return true if keyer is busy (keyer's graph state is other than IDLE)
   @return false if keyer is not busy (keyer's graph state is IDLE)
*/
bool cw_key_ik_is_busy_internal(const volatile cw_key_t * key)
{
	return key->ik.graph_state != KS_IDLE;
}





/**
   @brief Wait for end of element from the keyer

   Waits until the end of the current element, Dot or Dash, from the keyer.

   The function always returns CW_SUCCESS.

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key on which to wait

   @return CW_SUCCESS
*/
cw_ret_t cw_key_ik_wait_for_end_of_current_element(const volatile cw_key_t * key)
{
	/* TODO: test and describe behaviour of function when the key is in IDLE state. */

	/* First wait for the graph state to move to idle (or just do nothing
	   if it's not), or to one of the after- graph states. */
	pthread_mutex_lock(&key->gen->tq->wait_mutex);
	while (key->ik.graph_state != KS_IDLE
	       && key->ik.graph_state != KS_AFTER_DOT_A
	       && key->ik.graph_state != KS_AFTER_DOT_B
	       && key->ik.graph_state != KS_AFTER_DASH_A
	       && key->ik.graph_state != KS_AFTER_DASH_B) {

		pthread_cond_wait(&key->gen->tq->wait_var, &key->gen->tq->wait_mutex);
		/* cw_signal_wait_internal(); */ /* Old implementation was using signals. */ /* This code has been disabled some time before 2017-01-31. */
	}
	pthread_mutex_unlock(&key->gen->tq->wait_mutex);


	/* Now wait for the graph state to move to idle (unless it is, or was,
	   already), or one of the in- graph states, at which point we know
	   we're actually at the end of the element we were in when we
	   entered this routine. */
	pthread_mutex_lock(&key->gen->tq->wait_mutex);
	while (key->ik.graph_state != KS_IDLE
	       && key->ik.graph_state != KS_IN_DOT_A
	       && key->ik.graph_state != KS_IN_DOT_B
	       && key->ik.graph_state != KS_IN_DASH_A
	       && key->ik.graph_state != KS_IN_DASH_B) {

		pthread_cond_wait(&key->gen->tq->wait_var, &key->gen->tq->wait_mutex);
		/* cw_signal_wait_internal(); */ /* Old implementation was using signals. */ /* This code has been disabled some time before 2017-01-31. */
	}
	pthread_mutex_unlock(&key->gen->tq->wait_mutex);

	return CW_SUCCESS;
}




/**
   @brief Wait for the current keyer cycle to complete and the key to go into IDLE state

   The routine returns CW_SUCCESS on success.

   It returns CW_FAILURE (with errno set to EDEADLK) if either paddle
   value is CW_KEY_VALUE_CLOSED.

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key on which to wait

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_key_ik_wait_for_keyer(volatile cw_key_t * key)
{
	/* Check that neither paddle is CLOSED; if either is, then the
	   signal cycle is going to continue forever, and we'll never
	   return from this routine.
	   TODO: verify this comment. If this is a correct behaviour, then it
	   would limit the number of scenarios where this function could be
	   used. */
	if (CW_KEY_VALUE_CLOSED == key->ik.dot_paddle_value || CW_KEY_VALUE_CLOSED == key->ik.dash_paddle_value) {
		errno = EDEADLK;
		return CW_FAILURE;
	}

	/* Wait for the keyer graph state to go idle. */
	pthread_mutex_lock(&key->gen->tq->wait_mutex);
	while (key->ik.graph_state != KS_IDLE) {
		pthread_cond_wait(&key->gen->tq->wait_var, &key->gen->tq->wait_mutex);
		/* cw_signal_wait_internal(); */ /* Old implementation was using signals. */ /* This code has been disabled some time before 2017-01-31. */
	}
	pthread_mutex_unlock(&key->gen->tq->wait_mutex);

	return CW_SUCCESS;
}




/**
   @brief Reset iambic keyer data, stop generating sound on associated generator

   Clear all latches and paddle values of iambic keyer, return to
   Curtis 8044 Keyer mode A, and return to silence.  This function is
   suitable for calling from an application exit handler.

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key to reset
*/
void cw_key_ik_reset_internal(volatile cw_key_t * key)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYER_STATES, CW_DEBUG_DEBUG,
		      MSG_PREFIX_IK "ik reset: keyer graph state %s -> KS_IDLE", cw_iambic_keyer_graph_states[key->ik.graph_state]);
	key->ik.graph_state = KS_IDLE;

	key->ik.key_value = CW_KEY_VALUE_OPEN;

	key->ik.dot_paddle_value = CW_KEY_VALUE_OPEN;
	key->ik.dash_paddle_value = CW_KEY_VALUE_OPEN;
	key->ik.dot_latch = false;
	key->ik.dash_latch = false;
	key->ik.curtis_mode_b = false;
	key->ik.curtis_b_latch = false;

	/* Silence sound and stop any background soundcard tone generation. */
	cw_gen_silence_internal(key->gen);
	cw_finalization_schedule_internal(); /* TODO: do we still need this? */

	cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_KEYER_STATES, CW_DEBUG_DEBUG,
		      MSG_PREFIX_IK "ik reset: keyer graph state -> %s (reset)",
		      cw_iambic_keyer_graph_states[key->ik.graph_state]);

	return;
}




#ifdef IAMBIC_KEY_HAS_TIMER
/**
   Iambic keyer has an internal timer variable. On some occasions the
   variable needs to be updated.

   I thought that it needs to be updated by client application on key
   paddle events, but it turns out that it should be also updated in
   generator dequeue code. Not sure why.

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key with timer to be updated
   @param[in] usecs amount of increase (usually duration of a tone) for internal timer
*/
void cw_key_ik_increment_timer_internal(volatile cw_key_t * key, int usecs)
{
	if (NULL == key) {
		/* This function is called from generator thread. It
		   is perfectly valid situation that for some
		   applications a generator exists, but a keyer does
		   not exist.  Silently accept this situation. */
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_INTERNAL, CW_DEBUG_DEBUG,
			      MSG_PREFIX_IK "ik increment: NULL key, silently accepting");
		return;
	}

	if (key->ik.graph_state != KS_IDLE && NULL != key->ik.ik_timer) {
		/* Update timestamp that clocks iambic keyer
		   with current time interval. This must be
		   done only when iambic keyer is in
		   use. Calling the code when straight key is
		   in use will cause problems, so don't clock
		   a straight key with this. */

		cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYING, CW_DEBUG_INFO,
			      MSG_PREFIX_IK "ik increment: incrementing timer by %d [us]\n", usecs);

		key->ik.ik_timer->tv_usec += usecs % CW_USECS_PER_SEC;
		key->ik.ik_timer->tv_sec  += usecs / CW_USECS_PER_SEC + key->ik.ik_timer->tv_usec / CW_USECS_PER_SEC;
		key->ik.ik_timer->tv_usec %= CW_USECS_PER_SEC;
	}

	return;
}
#endif



/* ******************************************************************** */
/*                        Section:Straight key                          */
/* ******************************************************************** */




/**
   @brief Set new value of straight key

   If @p key_value indicates no change of value (internal value of key is the
   same as @p key_value), the call is ignored.

   The function replaces cw_notify_straight_key_event().

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key straight key to update
   @param[in] key_value new value of straight key

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_key_sk_set_value(volatile cw_key_t * key, cw_key_value_t key_value)
{
#if 0 /* This is disabled, but I'm not sure why. */  /* This code has been disabled some time before 2017-01-31. */
	/* If the tone queue or the keyer are busy, we can't use the
	   sound card, console sounder, or the key control system. */
	if (cw_tq_is_nonempty_internal(key->gen->tq) || cw_key_ik_is_busy_internal(key)) {
		errno = EBUSY;
		return CW_FAILURE;
	}
#endif

	return cw_key_sk_set_value_internal(key, key_value);
}




/**
   @brief Get current value of straight key

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key to get value from
   @param[out] key_value value of key

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise (e.g. argument errors)
*/
cw_ret_t cw_key_sk_get_value(const volatile cw_key_t * key, cw_key_value_t * key_value)
{
	if (NULL == key || NULL == key_value) {
		return CW_FAILURE;
	} else {
		*key_value = key->sk.key_value;
		return CW_SUCCESS;
	}
}




/**
   @brief Clear the straight key value,stop generating sound on associated generator

   This function is suitable for calling from an application exit handler.

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key key to reset
*/
void cw_key_sk_reset_internal(volatile cw_key_t * key)
{
	key->sk.key_value = CW_KEY_VALUE_OPEN;

	/* Stop any tone generation. */
	cw_gen_silence_internal(key->gen);
	//cw_finalization_schedule_internal();

	cw_debug_msg (&cw_debug_object, CW_DEBUG_STRAIGHT_KEY_STATES, CW_DEBUG_INFO,
		      MSG_PREFIX_SK "sk: key value ->OPEN (reset)");

	return;
}




/**
   @brief Create new key

   Returned pointer is owned by caller and should be deallocated with cw_key_delete().

   @internal
   @reviewed 2020-08-02
   @endinternal

   @return pointer to new key on success
   @return NULL on failure
*/
cw_key_t * cw_key_new(void)
{
	cw_key_t * key = (cw_key_t *) calloc(1, sizeof (cw_key_t));
	if (NULL == key) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_STDLIB, CW_DEBUG_ERROR,
			      MSG_PREFIX "new: calloc()");
		return (cw_key_t *) NULL;
	}

	key->gen = (cw_gen_t *) NULL;
	key->rec = (cw_rec_t *) NULL;

	key->sk.key_value = CW_KEY_VALUE_OPEN;

	key->ik.graph_state = KS_IDLE;
	key->ik.key_value = CW_KEY_VALUE_OPEN;

	key->ik.dot_paddle_value = CW_KEY_VALUE_OPEN;
	key->ik.dash_paddle_value = CW_KEY_VALUE_OPEN;
	key->ik.dot_latch = false;
	key->ik.dash_latch = false;

	key->ik.curtis_mode_b = false;
	key->ik.curtis_b_latch = false;

	key->ik.lock = false;

#ifdef IAMBIC_KEY_HAS_TIMER
	key->ik.ik_timer = NULL;
#endif

	return key;
}




/**
   @brief Delete key

   Deallocate a key allocated with cw_key_new().

   @p key is deallocated. Pointer to @p key is set to NULL.

   @internal
   @reviewed 2020-08-02
   @endinternal

   @param[in] key pointer to key to delete
*/
void cw_key_delete(cw_key_t ** key)
{
	cw_assert (NULL != key, MSG_PREFIX "key is NULL");

	if (NULL == *key) {
		return;
	}

	if (NULL != (*key)->gen) {
		/* Unregister. */
		(*key)->gen->key = NULL;
	}

	free(*key);
	*key = (cw_key_t *) NULL;

	return;
}




/**
   @internal
   @reviewed 2020-08-02
   @endinternal
*/
cw_ret_t cw_key_set_label(cw_key_t * key, const char * label)
{
	if (NULL == key) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'key' argument is NULL");
		return CW_FAILURE;
	}
	if (NULL == label) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': 'label' argument is NULL", key->label);
		return CW_FAILURE;
	}
	if (strlen(label) > (LIBCW_OBJECT_INSTANCE_LABEL_SIZE - 1)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_WARNING,
			      MSG_PREFIX "'%s': new label '%s' too long, truncating", key->label, label);
		/* Not an error, just log warning. New label will be truncated. */
	}

	/* Notice that empty label is acceptable. In such case we will
	   erase old label. */

	snprintf(key->label, sizeof (key->label), "%s", label);

	return CW_SUCCESS;
}




/**
   @reviewed 2020-05-23
*/
cw_ret_t cw_key_get_label(const cw_key_t * key, char * label, size_t size)
{
	if (NULL == key) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'key' argument is NULL");
		return CW_FAILURE;
	}
	if (NULL == label) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': 'label' argument is NULL", key->label);
		return CW_FAILURE;
	}

	/* Notice that we don't care if size is zero. */
	snprintf(label, size, "%s", key->label);

	return CW_SUCCESS;
}
