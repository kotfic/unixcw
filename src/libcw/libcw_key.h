/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef H_LIBCW_KEY
#define H_LIBCW_KEY




#include <stdbool.h>




#include "libcw2.h"




/* KS stands for Keyer State. */
enum {
	KS_IDLE,
	KS_IN_DOT_A,
	KS_IN_DASH_A,
	KS_AFTER_DOT_A,
	KS_AFTER_DASH_A,
	KS_IN_DOT_B,
	KS_IN_DASH_B,
	KS_AFTER_DOT_B,
	KS_AFTER_DASH_B
};



/* For modern API. */
typedef void (* cw_key_callback_t)(volatile struct timeval * timestamp, int key_state, void * callback_arg);


struct cw_key_struct {
	/* Straight key and iambic keyer needs a generator to produce
	   a sound on "Key Down" events. Maybe we don't always need a
	   sound, but sometimes we do want to have it.

	   Additionally iambic keyer needs a generator for timing
	   purposes. Even if we came up with a different mechanism for
	   timing the key, we still would need to use generator to
	   produce a sound - then we would have a kind of
	   duplication. So let's always use a generator. Sometimes for
	   timing iambic keyer, sometimes for generating sound, but
	   always the same generator.

	   In any case - a key needs to have access to a generator
	   (but a generator doesn't need a key). This is why the key
	   data type has a "generator" field, not the other way
	   around. */
	cw_gen_t * gen;


	/* There should be a binding between key and a receiver.

	   The receiver can get it's properly formed input data (key
	   down/key up events) from any source (any code that can call
	   receiver's 'mark_begin()' and 'mark_end()' functions), so
	   receiver is independent from key. On the other hand the key
	   without receiver is rather useless. Therefore I think that
	   the key should contain reference to a receiver, not the
	   other way around.

	   There may be one purpose of having a key without libcw
	   receiver: iambic keyer mechanism may be used to ensure a
	   functioning iambic keyer, but there may be a
	   different/external/3-rd party receiver that is
	   controlled/fed by cw_key_t::key_callback_func
	   function. TODO: verify that the callback still exists in
	   cw_key_t. */
	cw_rec_t * rec;


	/* Straight key. */
	struct {
		cw_key_value_t key_value;
	} sk;


	/* Iambic keyer.  The keyer functions maintain the current
	   known state of the paddles, and latch false-to-true
	   transitions while busy, to form the iambic effect.  For
	   Curtis mode B, the keyer also latches any point where both
	   paddle values are CLOSED at the same time. */
	struct {
		int graph_state;       /* State of iambic keyer state machine. */

		/* Overall value of key. Whether the key is generating a
		   Dash/Dot (CW_KEY_VALUE_CLOSED) or Space (CW_KEY_VALUE_OPEN). */
		cw_key_value_t key_value;

		/* Current values of paddles. Correspond directly to states
		   of electric contacts in user's equipment (e.g. paddles,
		   keyboard arrow keys). */
		cw_key_value_t dot_paddle_value;
		cw_key_value_t dash_paddle_value;

		bool dot_latch;        /* Dot false->true latch */
		bool dash_latch;       /* Dash false->true latch */

		/* Iambic keyer "Curtis" mode A/B selector.  Mode A and mode B timings
		   differ slightly, and some people have a preference for one or the other.
		   Mode A is a bit less timing-critical, so we'll make that the default. */
		bool curtis_mode_b;

		/* Curtis Dot&Dash latch */
		bool curtis_b_latch;

		/* FIXME: describe why we need this flag. */
		bool lock;

#define IAMBIC_KEY_HAS_TIMER
#ifdef IAMBIC_KEY_HAS_TIMER
		/* Timer for receiving of iambic keying, owned by client code. */
		struct timeval * ik_timer;
#endif
	} ik;

	char label[LIBCW_OBJECT_INSTANCE_LABEL_SIZE];
};




cw_ret_t cw_key_ik_update_graph_state_internal(volatile cw_key_t * key);
#ifdef IAMBIC_KEY_HAS_TIMER
void cw_key_ik_increment_timer_internal(volatile cw_key_t * key, int usecs);
#endif
void cw_key_ik_register_timer_internal(volatile cw_key_t * key, struct timeval * timer);


void cw_key_ik_get_paddle_latches_internal(volatile cw_key_t * key, int * dot_paddle_latch_state, int * dash_paddle_latch_state);
bool cw_key_ik_is_busy_internal(const volatile cw_key_t * key);

void cw_key_ik_reset_internal(volatile cw_key_t * key);
void cw_key_ik_reset_state_internal(volatile cw_key_t * key);

void cw_key_sk_reset_internal(volatile cw_key_t * key);
void cw_key_sk_reset_state_internal(volatile cw_key_t * key);



#endif // #ifndef H_LIBCW_KEY
