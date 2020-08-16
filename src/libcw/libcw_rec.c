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
   @file libcw_rec.c

   @brief Receiver. Receive a series of Marks and Spaces. Interpret them as
   characters.


   There are two ways of feeding Marks and Spaces to receiver.

   First of them is to notify receiver about "begin of Mark" and "end of
   Mark" events. Receiver then tries to figure out how long a Mark or Space
   is, what type of Mark (Dot/Dash) or Space (inter-mark-space,
   inter-character, inter-word) it is, and when a full character has been
   received.

   This is done with cw_rec_mark_begin() and cw_rec_mark_end() functions.

   The second method is to inform receiver not about start and stop of Marks
   (Dots/Dashes), but about full Marks themselves.  This is done with
   cw_rec_add_mark(): a function that is one level of abstraction above
   functions from first method.

   Currently there is only one method of passing received data (characters)
   from receiver to client code. This is done by client code cyclically
   polling the receiver with cw_rec_poll_representation() or
   cw_rec_poll_character() (which itself is built on top of
   cw_rec_poll_representation()).

   Duration of Marks, Spaces and few other things is in microseconds [us].
*/




#include "config.h"




#include <errno.h>
#include <limits.h> /* INT_MAX, for clang. */
#include <math.h>  /* sqrtf(), cosf() */
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h> /* struct timeval */
#include <unistd.h>




#if (defined(__unix__) || defined(unix)) && !defined(USG)
# include <sys/param.h>
#endif

#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif




#include "libcw.h"
#include "libcw2.h"
#include "libcw_data.h"
#include "libcw_debug.h"
#include "libcw_key.h"
#include "libcw_rec.h"
#include "libcw_rec_internal.h"
#include "libcw_utils.h"




#define MSG_PREFIX "libcw/rec: "




extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;




/* See also cw_rec_state_t enum in libcw_rec.h. */
static const char * cw_receiver_states[] = {
	"RS_IDLE",
	"RS_MARK",
	"RS_INTER_MARK_SPACE",
	"RS_EOC_GAP",
	"RS_EOW_GAP",
	"RS_EOC_GAP_ERR",
	"RS_EOW_GAP_ERR"
};




/* Functions handling averaging data structure in adaptive receiving
   mode. */
static void cw_rec_update_average_internal(cw_rec_averaging_t * avg, int mark_duration);
static void cw_rec_update_averages_internal(cw_rec_t * rec, int mark_duration, char mark);
static void cw_rec_reset_average_internal(cw_rec_averaging_t * avg, int initial);




/**
   @brief Allocate and initialize new receiver variable

   Before returning, the function calls
   cw_rec_sync_parameters_internal() for the receiver.

   Function may return NULL on malloc() failure.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @return freshly allocated, initialized and synchronized receiver on success
   @return NULL pointer on failure
*/
cw_rec_t * cw_rec_new(void)
{
	cw_rec_t * rec = (cw_rec_t *) malloc(sizeof (cw_rec_t));
	if (NULL == rec) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_STDLIB, CW_DEBUG_ERROR,
			      MSG_PREFIX "malloc()");
		return (cw_rec_t *) NULL;
	}
	memset(rec, 0, sizeof (cw_rec_t));

	rec->state = RS_IDLE;

	rec->speed                      = CW_SPEED_INITIAL;
	rec->tolerance                  = CW_TOLERANCE_INITIAL;
	rec->gap                        = CW_GAP_INITIAL;
	rec->is_adaptive_receive_mode   = CW_REC_ADAPTIVE_MODE_INITIAL;
	rec->noise_spike_threshold      = CW_REC_NOISE_THRESHOLD_INITIAL;

	/* TODO: this variable is not set in
	   cw_rec_reset_parameters_internal().  Why is it separated
	   from the four main variables? Is it because it is a
	   derivative of speed? But speed is a derivative of this
	   variable in adaptive speed mode. */
	rec->adaptive_speed_threshold = CW_REC_SPEED_THRESHOLD_INITIAL;


	rec->parameters_in_sync = false;
	cw_rec_sync_parameters_internal(rec);

	return rec;
}




/**
   @brief Delete a generator

   Deallocate all memory and free all resources associated with given
   receiver that was allocated with cw_rec_new()

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in,out] rec pointer to receiver
*/
void cw_rec_delete(cw_rec_t ** rec)
{
	cw_assert (rec, MSG_PREFIX "delete: 'rec' argument can't be NULL\n");

	if (NULL == rec) { /* Graceful handling of invalid argument. */
		return;
	}
	if (NULL == *rec) {
		return;
	}

	free(*rec);
	*rec = (cw_rec_t *) NULL;

	return;
}




/**
   @brief Set receiver's receiving speed

   See documentation of cw_set_send_speed() for more information.

   See libcw.h/CW_SPEED_{INITIAL|MIN|MAX} for initial/minimal/maximal
   value of receive speed.

   Notice that internally the speed is saved as float.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @exception EINVAL @p new_value is out of range.
   @exception EPERM adaptive receive speed tracking is enabled.

   @param[in,out] rec receiver for which to set new value of parameter
   @param[in] new_value new value of parameter to be set

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_rec_set_speed(cw_rec_t * rec, int new_value)
{
	if (rec->is_adaptive_receive_mode) {
		errno = EPERM;
		return CW_FAILURE;
	}

	if (new_value < CW_SPEED_MIN || new_value > CW_SPEED_MAX) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	const float diff = fabsf((1.0f * new_value) - rec->speed);
	if (diff >= 0.5f) { /* TODO: verify this comparison. */
		rec->speed = new_value;

		/* Changes of receive speed require resynchronization. */
		rec->parameters_in_sync = false;
		cw_rec_sync_parameters_internal(rec);
	}

	return CW_SUCCESS;
}




/**
   @brief Get receiver's speed

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in] rec receiver from which to get current value of parameter

   @return current value of receiver's speed
*/
float cw_rec_get_speed(const cw_rec_t * rec)
{
	return rec->speed;
}




/**
   @brief Set receiver's tolerance

   See libcw.h/CW_TOLERANCE_{INITIAL|MIN|MAX} for initial/minimal/maximal
   value of tolerance.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @exception EINVAL @p new_value is out of range.

   @param[in,out] rec receiver for which to set new value of parameter
   @param[in] new_value new value of parameter to be set

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_rec_set_tolerance(cw_rec_t * rec, int new_value)
{
	if (new_value < CW_TOLERANCE_MIN || new_value > CW_TOLERANCE_MAX) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	if (new_value != rec->tolerance) {
		rec->tolerance = new_value;

		/* Changes of tolerance require resynchronization. */
		rec->parameters_in_sync = false;
		cw_rec_sync_parameters_internal(rec);
	}

	return CW_SUCCESS;
}




/**
   @brief Get receiver's tolerance

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in] rec receiver from which to get current value of parameter

   @return current value of receiver's tolerance
*/
int cw_rec_get_tolerance(const cw_rec_t * rec)
{
	return rec->tolerance;
}




/**
   @brief Get receiver's timing parameters and adaptive threshold

   Return the low-level timing parameters calculated from the speed,
   gap, tolerance, and weighting set.  Units of returned parameter
   values are microseconds.

   Use NULL for the pointer argument to any parameter value not required.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in] rec receiver from which to get current value of parameters
   @param[out] dot_duration_ideal
   @param[out] dash_duration_ideal
   @param[out] dot_duration_min
   @param[out] dot_duration_max
   @param[out] dash_duration_min
   @param[out] dash_duration_max
   @param[out] ims_duration_min
   @param[out] ims_duration_max
   @param[out] ims_duration_ideal
   @param[out] ics_duration_min
   @param[out] ics_duration_max
   @param[out] ics_duration_ideal
   @param[out] adaptive_threshold
*/
void cw_rec_get_parameters_internal(cw_rec_t * rec,
				    int * dot_duration_ideal, int * dash_duration_ideal,
				    int * dot_duration_min,   int * dot_duration_max,
				    int * dash_duration_min,  int * dash_duration_max,
				    int * ims_duration_min,
				    int * ims_duration_max,
				    int * ims_duration_ideal,
				    int * ics_duration_min,
				    int * ics_duration_max,
				    int * ics_duration_ideal,
				    int * adaptive_threshold)
{
	cw_rec_sync_parameters_internal(rec);

	/* Dot mark. */
	if (dot_duration_min)   { *dot_duration_min   = rec->dot_duration_min; }
	if (dot_duration_max)   { *dot_duration_max   = rec->dot_duration_max; }
	if (dot_duration_ideal) { *dot_duration_ideal = rec->dot_duration_ideal; }

	/* Dash mark. */
	if (dash_duration_min)   { *dash_duration_min   = rec->dash_duration_min; }
	if (dash_duration_max)   { *dash_duration_max   = rec->dash_duration_max; }
	if (dash_duration_ideal) { *dash_duration_ideal = rec->dash_duration_ideal; }

	/* Inter-mark-space. */
	if (ims_duration_min)   { *ims_duration_min   = rec->ims_duration_min; }
	if (ims_duration_max)   { *ims_duration_max   = rec->ims_duration_max; }
	if (ims_duration_ideal) { *ims_duration_ideal = rec->ims_duration_ideal; }

	/* Inter-character-space. */
	if (ics_duration_min)   { *ics_duration_min   = rec->ics_duration_min; }
	if (ics_duration_max)   { *ics_duration_max   = rec->ics_duration_max; }
	if (ics_duration_ideal) { *ics_duration_ideal = rec->ics_duration_ideal; }

	if (adaptive_threshold) { *adaptive_threshold = rec->adaptive_speed_threshold; }

	return;
}




/**
   @brief Set receiver's noise spike threshold

   Set the period shorter than which, on receive, received marks are ignored.
   This allows the "receive mark" functions to apply noise canceling for very
   short apparent marks.
   For useful results the value should never exceed the dot duration of a dot at
   maximum speed: 20000 microseconds (the dot duration at 60WPM).
   Setting a noise threshold of zero turns off receive mark noise canceling.

   The default noise spike threshold is 10000 microseconds.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @exception EINVAL @p new_value is out of range.

   @param[in,out] rec receiver for which to set new value of parameter
   @param[in] new_value new value of parameter to be set

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_rec_set_noise_spike_threshold(cw_rec_t * rec, int new_value)
{
	if (new_value < 0) {
		errno = EINVAL;
		return CW_FAILURE;
	}
	rec->noise_spike_threshold = new_value;

	return CW_SUCCESS;
}




/**
   @brief Get receiver's noise spike threshold

   See documentation of cw_set_noise_spike_threshold() for more information.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in] rec receiver from which to get current value of parameter

   @return current value of receiver's threshold
*/
int cw_rec_get_noise_spike_threshold(const cw_rec_t * rec)
{
	return rec->noise_spike_threshold;
}




/**
   @brief Set receiver's gap

   @internal
   @reviewed 2020-08-09
   @endinternal

   @exception EINVAL @p new_value is out of range.

   @param[in,out] rec receiver for which to set new value of parameter
   @param[in] new_value new value of parameter to be set

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_rec_set_gap(cw_rec_t * rec, int new_value)
{
	if (new_value < CW_GAP_MIN || new_value > CW_GAP_MAX) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	if (new_value != rec->gap) {
		rec->gap = new_value;

		/* Changes of gap require resynchronization. */
		rec->parameters_in_sync = false;
		cw_rec_sync_parameters_internal(rec);
	}

	return CW_SUCCESS;
}




/**
   @brief Reset averaging data structure

   Reset averaging data structure to initial state.
   To be used in adaptive receiving mode.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in,out] avg averaging data structure (for Dots or for Dashes)
   @param[in] initial initial value to be put in table of averaging data structure
*/
void cw_rec_reset_average_internal(cw_rec_averaging_t * avg, int initial)
{
	for (int i = 0; i < CW_REC_AVERAGING_DURATIONS_COUNT; i++) {
		avg->buffer[i] = initial;
	}

	avg->sum = initial * CW_REC_AVERAGING_DURATIONS_COUNT;
	avg->cursor = 0;

	return;
}




/**
   @brief Update value of average "duration of mark"

   To be used in adaptive receiving mode.

   Update table of values used to calculate averaged "duration of
   mark". The averaged duration of a mark is calculated with moving
   average function.

   The new @p mark_duration is added to @p avg, and the oldest is
   discarded. New averaged sum is calculated using updated data.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in,out] avg averaging data structure (for Dots or for Dashes)
   @param[in] mark_duration new "duration of mark" value to be added to averaging data @p avg
*/
void cw_rec_update_average_internal(cw_rec_averaging_t * avg, int mark_duration)
{
	/* TODO: write unit tests for this code. Are we removing correct "oldest" here? */
	/* Oldest mark duration goes out, new goes in. */
	avg->sum -= avg->buffer[avg->cursor];
	avg->sum += mark_duration;

	avg->average = avg->sum / CW_REC_AVERAGING_DURATIONS_COUNT;

	avg->buffer[avg->cursor++] = mark_duration;
	avg->cursor %= CW_REC_AVERAGING_DURATIONS_COUNT;

	return;
}




/**
   @brief Add a Mark or Space duration to statistics

   Add a Mark or Space duration @p duration (type of Mark or Space is
   indicated by @p type) to receiver's circular statistics buffer.
   The buffer stores only the delta from the ideal value; the ideal is
   inferred from the type @p type passed in.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in,out] rec receiver for which to update stats
   @param[in] type type of statistics to update
   @param[in] duration duration of a Mark or Space
*/
void cw_rec_duration_stats_update_internal(cw_rec_t * rec, stat_type_t type, int duration)
{
	/* Synchronize parameters if required. */
	cw_rec_sync_parameters_internal(rec);

	/* Calculate delta as difference between given duration and the ideal
	   duration value. */
	int ideal = 0;
	switch (type) {
	case CW_REC_STAT_DOT:
		ideal = rec->dot_duration_ideal;
		break;
	case CW_REC_STAT_DASH:
		ideal = rec->dash_duration_ideal;
		break;
	case CW_REC_STAT_INTER_MARK_SPACE:
		ideal = rec->ims_duration_ideal;
		break;
	case CW_REC_STAT_INTER_CHARACTER_SPACE:
		ideal = rec->ics_duration_ideal;
		break;
	default:
		ideal = duration;
		break;
	}
	const int duration_delta = duration - ideal;

	/* Add this statistic to the buffer. */
	rec->duration_stats[rec->duration_stats_idx].type = type;
	rec->duration_stats[rec->duration_stats_idx].duration_delta = duration_delta;

	rec->duration_stats_idx++;
	rec->duration_stats_idx %= CW_REC_DURATION_STATS_CAPACITY;

	return;
}




/**
   @brief Calculate and return duration statistics for given type of Mark or Space

   @internal
   @reviewed 2020-08-09
   @endinternal

   If no stats of given @p type are recorded by @p rec, function returns
   CW_SUCCESS and @p result is set to 0.0.

   On failure, CW_FAILURE is returned and @p result is set to 0.0. Currently
   only function argument errors (pointers being NULL) are considered a failure.

   @param[in] rec receiver from which to get current statistics of type @p type
   @param[in] type type of statistics to get
   @param[out] result value of retrieved stats

   @return CW_FAILURE on argument errors
   @return CW_SUCCESS otherwise
*/
cw_ret_t cw_rec_duration_stats_get_internal(const cw_rec_t * rec, stat_type_t type, float * result)
{
	if (NULL == rec) {
		return CW_FAILURE;
	}
	if (NULL == result) {
		return CW_FAILURE;
	}

	/* TODO: some locking of statistics with mutex? */

	/* Sum and count values for marks/spaces matching the given
	   type.  A cleared buffer always begins refilling at zeroth
	   mark, so to optimize we can stop on the first unoccupied
	   slot in the circular buffer. */
	int sum_of_squares = 0;
	int count = 0;
	for (int i = 0; i < CW_REC_DURATION_STATS_CAPACITY; i++) {
		if (rec->duration_stats[i].type == type) {
			const int duration_delta = rec->duration_stats[i].duration_delta;
			sum_of_squares += (duration_delta * duration_delta);
			count++;
		} else if (rec->duration_stats[i].type == CW_REC_STAT_NONE) {
			break;
		} else {
			; /* A type of statistics that we are not interested in. Continue. */
		}
	}

	if (0 == count) {
		*result = 0.0;
	} else {
		*result = sqrtf(sum_of_squares / (float) count);
	}
	return CW_SUCCESS;
}




/**
   @brief Calculate and return receiver's timing statistics

   These statistics may be used to obtain a measure of the accuracy of
   received Morse code.

   The values @p dot_sd and @p dash_sd contain the standard deviation of dot
   and dash durations from the ideal values, and @p inter_mark_space_sd and
   @p inter_character_space_sd contain the deviations for inter-mark-space
   and inter-character-space.

   Statistics are held for all timings in a 256 element circular
   buffer.  If any statistic cannot be calculated, because no records
   for it exist, the returned value is 0.0.  Use NULL for the pointer
   argument to any statistic not required.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in] rec receiver from which to get current statistics
   @param[out] dot_sd
   @param[out] dash_sd
   @param[out] inter_mark_space_sd
   @param[out] inter_character_space_sd
*/
void cw_rec_get_statistics_internal(const cw_rec_t * rec, float * dot_sd, float * dash_sd,
				    float * inter_mark_space_sd, float * inter_character_space_sd)
{
	/* TODO: cw_rec_get_statistics_internal() function appears to be
	   getting standard deviations ("sd") but
	   cw_rec_duration_stats_get_internal() doesn't really calculate
	   standard deviations, at least not by a proper definition. The
	   proper definition is using mean duration where we use "ideal"
	   duration. */

	if (dot_sd) {
		cw_rec_duration_stats_get_internal(rec, CW_REC_STAT_DOT, dot_sd);
	}
	if (dash_sd) {
		cw_rec_duration_stats_get_internal(rec, CW_REC_STAT_DASH, dash_sd);
	}
	if (inter_mark_space_sd) {
		cw_rec_duration_stats_get_internal(rec, CW_REC_STAT_INTER_MARK_SPACE, inter_mark_space_sd);
	}
	if (inter_character_space_sd) {
		cw_rec_duration_stats_get_internal(rec, CW_REC_STAT_INTER_CHARACTER_SPACE, inter_character_space_sd);
	}
	return;
}




/**
   @brief Clear receiver statistics

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in] rec receiver for which to clear statistics
*/
void cw_rec_reset_statistics(cw_rec_t * rec)
{
	for (int i = 0; i < CW_REC_DURATION_STATS_CAPACITY; i++) {
		rec->duration_stats[i].type = CW_REC_STAT_NONE;
		rec->duration_stats[i].duration_delta = 0;
	}
	rec->duration_stats_idx = 0;

	return;
}




/* ******************************************************************** */
/*                           Section:Receiving                          */
/* ******************************************************************** */




/*
 * The CW receive functions implement the following state graph:
 *
 *        +-----------<------- RS_EOW_GAP_ERR ------------<--------------+
 *        |(clear)                    ^                                  |
 *        |                (pull() +  |                                  |
 *        |       space dur > eoc dur)|                                  |
 *        |                           |                                  |
 *        +-----------<-------- RS_EOC_GAP_ERR <---------------+         |
 *        |(clear)                    ^  |                     |         |
 *        |                           |  +---------------------+         |(error,
 *        |                           |    (pull() +                     |space dur > eoc dur)
 *        |                           |    space dur = eoc dur)          |
 *        v                    (error,|                                  |
 *        |       space dur = eoc dur)|  +------------->-----------------+
 *        |                           |  |
 *        +-----------<------------+  |  |
 *        |                        |  |  |
 *        |              (is noise)|  |  |
 *        |                        |  |  |
 *        v        (begin mark)    |  |  |    (end mark,noise)
 * --> RS_IDLE ------->----------- RS_MARK ------------>-----> RS_INTER_MARK_SPACE <------- +
 *     v  ^                              ^                          v v v ^ |               |
 *     |  |                              |    (begin mark)          | | | | |               |
 *     |  |     (pull() +                +-------------<------------+ | | | +---------------+
 *     |  |     space dur = eoc dur)                                  | | |      (not ready,
 *     |  |     +-----<------------+          (pull() +               | | |      buffer dot,
 *     |  |     |                  |          space dur = eoc dur)    | | |      buffer dash)
 *     |  |     +-----------> RS_EOC_GAP <-------------<--------------+ | |
 *     |  |                     |  |                                    | |
 *     |  |(clear)              |  |                                    | |
 *     |  +-----------<---------+  |                                    | |
 *     |  |                        |                                    | |
 *     |  |              (pull() + |                                    | |
 *     |  |    space dur > eoc dur)|                                    | |
 *     |  |                        |          (pull() +                 | |
 *     |  |(clear)                 v          space dur > eoc dur)      | |
 *     |  +-----------<------ RS_EOW_GAP <-------------<----------------+ |
 *     |                                                                  |
 *     |                                                                  |
 *     |               (buffer dot,                                       |
 *     |               buffer dash)                                       |
 *     +------------------------------->----------------------------------+
 */




void CW_REC_SET_STATE(cw_rec_t * rec, cw_rec_state_t new_state, cw_debug_t * debug_object)
{
	cw_debug_msg ((debug_object),
		      CW_DEBUG_RECEIVE_STATES, CW_DEBUG_INFO,
		      MSG_PREFIX "'%s': state: %s -> %s @ %s:%d",
		      (rec)->label,
		      cw_receiver_states[rec->state], cw_receiver_states[new_state], __func__, __LINE__);
	rec->state = new_state;
}




/**
   @brief Enable or disable receiver's "adaptive receiving" mode

   Set the mode of a receiver @p rec to fixed or adaptive receiving
   mode.

   If adaptive speed tracking is enabled, the receiver will attempt to
   automatically adjust the receive speed setting to match the speed of the
   incoming Morse code. If it is disabled, the receiver will use fixed speed
   settings, and reject incoming Morse code which is not at the expected
   speed.

   Adaptive speed tracking uses a moving average length of the past N marks
   as its baseline for tracking speeds.  The default state is adaptive speed
   tracking disabled.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in,out] rec receiver for which to set the mode
   @param[in] adaptive value of receiver's "adaptive mode" to be set
*/
void cw_rec_set_adaptive_mode_internal(cw_rec_t * rec, bool adaptive)
{
	/* Look for change of adaptive receive state. */
	if (rec->is_adaptive_receive_mode != adaptive) {

		rec->is_adaptive_receive_mode = adaptive;

		/* Changing the flag forces a change in low-level parameters. */
		rec->parameters_in_sync = false;
		cw_rec_sync_parameters_internal(rec);

		/* If we have just switched to adaptive mode, (re-)initialize
		   the averages array to the current Dot/Dash durations, so
		   that initial averages match the current speed. */
		if (rec->is_adaptive_receive_mode) {
			cw_rec_reset_average_internal(&rec->dot_averaging, rec->dot_duration_ideal);
			cw_rec_reset_average_internal(&rec->dash_averaging, rec->dash_duration_ideal);
		}
	}

	return;
}




/**
   @brief Enable receiver's "adaptive receiving" mode

   See cw_rec_set_adaptive_mode_internal() for more info.

   @internal
   TODO: this function and cw_rec_set_adaptive_mode_internal() are redundant.
   @endinternal

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in,out] rec receiver for which to enable the mode
*/
void cw_rec_enable_adaptive_mode(cw_rec_t * rec)
{
	cw_rec_set_adaptive_mode_internal(rec, true);
	return;
}




/**
   @brief Disable receiver's "adaptive receiving" mode

   See cw_rec_set_adaptive_mode_internal() for more info.

   @internal
   TODO: this function and cw_rec_set_adaptive_mode_internal() are redundant.
   @endinternal

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in,out] rec receiver for which to disable the mode
*/
void cw_rec_disable_adaptive_mode(cw_rec_t * rec)
{
	cw_rec_set_adaptive_mode_internal(rec, false);
	return;
}




/**
   @brief Get adaptive receive speed tracking flag

   The function returns state of "adaptive receive enabled" flag.

   See cw_rec_set_adaptive_mode_internal() for more info.

   @internal
   @reviewed 2020-08-09
   @endinternal

   @param[in] rec receiver from which to get current value of parameter

   @return true if adaptive speed tracking is enabled
   @return false otherwise
*/
bool cw_rec_get_adaptive_mode(const cw_rec_t * rec)
{
	return rec->is_adaptive_receive_mode;
}




/**
   @brief Inform @p rec about beginning of a Mark

   @internal
   @reviewed 2020-08-10
   @endinternal

   @exception ERANGE invalid state of receiver was discovered.
   @exception EINVAL errors while processing or getting @p timestamp

   @param[in,out] rec receiver which to inform about beginning of Mark
   @param[in] timestamp timestamp of "beginning of Mark" event. May be NULL, then current time will be used.

   @return CW_SUCCESS when no errors occurred
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_rec_mark_begin(cw_rec_t * rec, const struct timeval * timestamp)
{
	if (rec->is_pending_inter_word_space) {

		/* Beginning of Mark in this situation means that we're
		   seeing the next incoming character within the same word,
		   so no inter-word-space will be received at this point in
		   time. The Space that we were observing/waiting for, was
		   just inter-character-space.

		   Reset state of rec and cancel the waiting for
		   inter-word-space. */
		cw_rec_reset_state(rec);
	}

	if (RS_IDLE != rec->state && RS_INTER_MARK_SPACE != rec->state) {
		/*
		  A start of Mark can only happen while we are idle (waiting
		  for beginning of a first Mark of a new character), or in
		  inter-mark-space of a current character.

		  ->state should be RS_IDLE at the beginning of new character
		  OR
		  ->state should be RS_INTER_MARK_SPACE in the middle of character (between Marks)

		  See cw_rec_add_mark() for similar condition.
		*/

		cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': mark_begin: receive state not idle and not inter-mark-space: %s",
			      rec->label,
			      cw_receiver_states[rec->state]);

		errno = ERANGE;
		return CW_FAILURE;
	}
	cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_INFO,
		      MSG_PREFIX "'%s': mark_begin: receive state: %s", rec->label, cw_receiver_states[rec->state]);

	/* Validate and save the timestamp, or get one and then save it.
	   This is a timestamp of beginning of Mark. */
	if (CW_SUCCESS != cw_timestamp_validate_internal(&rec->mark_start, timestamp)) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	if (RS_INTER_MARK_SPACE == rec->state) {
		/* Measure duration of inter-mark-space that is about to end
		   (just for statistics).

		   rec->mark_end is timestamp of end of previous Mark. It is
		   set when receiver goes into inter-mark-space state by
		   cw_rec_mark_end() or by cw_rec_add_mark(). */
		const int space_duration = cw_timestamp_compare_internal(&rec->mark_end,
									 &rec->mark_start);
		cw_rec_duration_stats_update_internal(rec, CW_REC_STAT_INTER_MARK_SPACE, space_duration);

		/* TODO: this may have been a very long space. Should
		   we accept a very long space inside a character? */
	}

	/* Set state to indicate we are inside a Mark. We don't know yet if
	   it will be recognized as valid Mark, it may be shorter than a
	   threshold. */
	CW_REC_SET_STATE (rec, RS_MARK, (&cw_debug_object));

	return CW_SUCCESS;
}




/**
   @brief Inform @p rec about end of a Mark

   @internal
   @reviewed 2020-08-09
   @endinternal

   If Mark is identified as Dot or Dash, it is added to receiver's
   representation buffer.

   @exception ERANGE invalid state of receiver was discovered
   @exception EINVAL errors while processing or getting @p timestamp
   @exception ECANCELED the Mark has been classified as noise spike and rejected
   @exception EBADMSG this function can't recognize the Mark
   @exception ENOMEM space for representation of character has been exhausted

   @param[in,out] rec receiver which to inform about end of Mark
   @param[in] timestamp timestamp of "end of Mark" event. May be NULL, then current time will be used.

   @return CW_SUCCESS when no errors occurred
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_rec_mark_end(cw_rec_t * rec, const struct timeval * timestamp)
{
	/* The receiver state is expected to be inside of a Mark, otherwise
	   there is nothing to end. */
	if (RS_MARK != rec->state) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': mark_end: receiver state not RS_MARK: %s", rec->label, cw_receiver_states[rec->state]);
		errno = ERANGE;
		return CW_FAILURE;
	}
	cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_INFO,
		      MSG_PREFIX "'%s': mark_end: receiver state: %s", rec->label, cw_receiver_states[rec->state]);

	/* Take a safe copy of the current end timestamp, in case we need
	   to put it back if we decide this Mark is really just noise. */
	struct timeval saved_end_timestamp = rec->mark_end;

	/* Save the timestamp passed in, or get one. */
	if (CW_SUCCESS != cw_timestamp_validate_internal(&rec->mark_end, timestamp)) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	/* Compare the timestamps to determine the duration of the Mark. */
	const int mark_duration = cw_timestamp_compare_internal(&rec->mark_start,
								&rec->mark_end);

#if 0
	fprintf(stderr, "------- %d.%d - %d.%d = %d (%d)\n",
		rec->mark_end.tv_sec, rec->mark_end.tv_usec,
		rec->mark_start.tv_sec, rec->mark_start.tv_usec,
		mark_duration, cw_timestamp_compare_internal(&rec->mark_start, &rec->mark_end));
#endif

	if (rec->noise_spike_threshold > 0
	    && mark_duration <= rec->noise_spike_threshold) {

		/* This pair of start()/stop() calls is just a noise,
		   ignore it.

		   Revert to state of receiver as it was before complementary
		   cw_rec_mark_begin(). After call to cw_rec_mark_begin() the
		   state was changed to RS_MARK, but what state it was before
		   call to cw_rec_mark_begin()?

		   To answer that question check position in representation
		   buffer (how many Marks are in the buffer) to see in which
		   state the receiver was *before* cw_rec_mark_begin()
		   function call, and restore this state. */
		CW_REC_SET_STATE (rec, (rec->representation_ind == 0 ? RS_IDLE : RS_INTER_MARK_SPACE), (&cw_debug_object));

		/* Put the end-of-mark timestamp back to how it was when we
		   came in to the routine. */
		rec->mark_end = saved_end_timestamp;

		cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYING, CW_DEBUG_INFO,
			      MSG_PREFIX "'%s': mark_end: '%d [us]' Mark identified as spike noise (threshold = '%d [us]')",
			      rec->label,
			      mark_duration, rec->noise_spike_threshold);

		errno = ECANCELED;
		return CW_FAILURE;
	}


	/* This was not a noise. At this point, we have to make a
	   decision about the Mark just received.  We'll use a routine
	   that compares duration of a Mark against pre-calculated Dot
	   and Dash duration ranges to tell us what it thinks this Mark
	   is (Dot or Dash).  If the routine can't decide, it will
	   hand us back an error which we return to the caller.
	   Otherwise, it returns a Mark (Dot or Dash), for us to put
	   in representation buffer. */
	char mark;
	if (CW_SUCCESS != cw_rec_identify_mark_internal(rec, mark_duration, &mark)) {
		errno = EBADMSG;
		return CW_FAILURE;
	}

	if (rec->is_adaptive_receive_mode) {
		/* Update the averaging buffers so that the adaptive
		   tracking of received Morse speed stays up to
		   date. */
		cw_rec_update_averages_internal(rec, mark_duration, mark);
	} else {
		/* Do nothing. Don't fiddle about trying to track for
		   fixed speed receive. */
	}

	/* Update Dot and Dash duration statistics.  It may seem odd to do
	   this after calling cw_rec_update_averages_internal(),
	   rather than before, as this function changes the ideal values we're
	   measuring against.  But if we're on a speed change slope, the
	   adaptive tracking smoothing will cause the ideals to lag the
	   observed speeds.  So by doing this here, we can at least
	   ameliorate this effect, if not eliminate it. */
	if (mark == CW_DOT_REPRESENTATION) {
		cw_rec_duration_stats_update_internal(rec, CW_REC_STAT_DOT, mark_duration);
	} else {
		cw_rec_duration_stats_update_internal(rec, CW_REC_STAT_DASH, mark_duration);
	}

	/* Add the Mark to the receiver's representation buffer. */
	rec->representation[rec->representation_ind++] = mark;

	/* Until we complete the whole character (all Dots and Dashes), this
	   will print only part of representation. */
	cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_INFO,
		      MSG_PREFIX "'%s': mark_end: representation recognized so far is '%s'", rec->label, rec->representation);

	/* We just added a Mark to the receive buffer.  If the buffer is
	   full, then we have to do something, even though it's unlikely.
	   What we'll do is make a unilateral declaration that if we get this
	   far, we go to inter-character-space-error state automatically. */
	if (rec->representation_ind == CW_REC_REPRESENTATION_CAPACITY - 1) {

		CW_REC_SET_STATE (rec, RS_EOC_GAP_ERR, (&cw_debug_object));

		cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': mark_end: receiver's representation buffer is full", rec->label);

		errno = ENOMEM;
		return CW_FAILURE;
	}

	/* All is well.  Move to the more normal inter-mark-space
	   state. */
	CW_REC_SET_STATE (rec, RS_INTER_MARK_SPACE, (&cw_debug_object));

	return CW_SUCCESS;
}




/**
   @brief Analyze a Mark and identify it as a Dot or Dash

   Identify a Mark (Dot/Dash) represented by a duration of mark: @p
   mark_duration.

   Identification is done using the duration ranges provided by the low
   level timing parameters.

   On success function returns CW_SUCCESS and sends back either a Dot
   or a Dash through @p mark.

   On failure it returns CW_FAILURE if the Mark is not recognizable as either
   a Dot or a Dash, and sets the receiver state to one of the error states,
   depending on the duration of Mark passed in.

   Note: for adaptive timing, the Mark should _always_ be recognized as a Dot
   or a Dash, because the duration ranges will have been set to cover 0 to
   INT_MAX.

   @internal
   @reviewed 2020-08-10
   @endinternal

   @param[in,out] rec receiver
   @param[in] mark_duration duration of Mark to analyze
   @param[out] mark variable to store identified Mark

   @return CW_SUCCESS if a mark has been identified as either Dot or Dash
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_rec_identify_mark_internal(cw_rec_t * rec, int mark_duration, char * mark)
{
	cw_assert (NULL != mark, MSG_PREFIX "output argument is NULL");

	/* Synchronize parameters if required */
	cw_rec_sync_parameters_internal(rec);

	/* If the duration was, within tolerance, a Dot, return Dot to
	   the caller.  */
	if (mark_duration >= rec->dot_duration_min
	    && mark_duration <= rec->dot_duration_max) {

		cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_INFO,
			      MSG_PREFIX "'%s': identify: mark '%d [us]' recognized as DOT (limits: %d - %d [us])",
			      rec->label, mark_duration, rec->dot_duration_min, rec->dot_duration_max);

		*mark = CW_DOT_REPRESENTATION;
		return CW_SUCCESS;
	}

	/* Do the same for a dash. */
	if (mark_duration >= rec->dash_duration_min
	    && mark_duration <= rec->dash_duration_max) {

		cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_INFO,
			      MSG_PREFIX "'%s': identify: mark '%d [us]' recognized as DASH (limits: %d - %d [us])",
			      rec->label, mark_duration, rec->dash_duration_min, rec->dash_duration_max);

		*mark = CW_DASH_REPRESENTATION;
		return CW_SUCCESS;
	}

	/* This mark is not a Dot or a Dash, so we have an error
	   case. */
	cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_ERROR,
		      MSG_PREFIX "'%s': identify: unrecognized mark, duration = %d [us]", rec->label, mark_duration);
	cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_ERROR,
		      MSG_PREFIX "'%s': identify: dot limits: %d - %d [us]", rec->label, rec->dot_duration_min, rec->dot_duration_max);
	cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_ERROR,
		      MSG_PREFIX "'%s': identify: dash limits: %d - %d [us]", rec->label, rec->dash_duration_min, rec->dash_duration_max);

	/* We should never reach here when in adaptive timing receive mode -
	   a Mark should be always recognized as Dot or Dash, and function
	   should have returned before reaching this point. */
	if (rec->is_adaptive_receive_mode) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': identify: unrecognized mark in adaptive receive", rec->label);
	} else {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': identify: unrecognized mark in non-adaptive receive", rec->label);
	}



	/* TODO: making decision about current state of receiver is
	   out of scope of this function. Move the part below to
	   separate function. */

	/* If we can't send back any result through @p mark, let's move to
	   either "inter-character-space, in error" or "end-of-word, in error"
	   state.

	   To decide which error state to choose, we will treat @p
	   mark_duration as duration of Space.

	   Depending on the duration of Space, we pick which of the error
	   states to move to, and move to it.  The comparison is against the
	   expected inter-character-space duration.  If it's larger, then fix at word
	   error, otherwise settle on char error.

	   TODO: reconsider this for a moment: the function has been
	   called because client code has received a *mark*, not a
	   space. Are we sure that we now want to treat the
	   mark_duration as duration of *space*? And do we want to
	   move to either RS_EOW_GAP_ERR or RS_EOC_GAP_ERR pretending that
	   this is a duration of *space*? */
	CW_REC_SET_STATE (rec, (mark_duration > rec->ics_duration_max ? RS_EOW_GAP_ERR : RS_EOC_GAP_ERR), (&cw_debug_object));

	return CW_FAILURE;
}




/**
   @brief Update receiver's averaging data structures with most recent data

   When in adaptive receiving mode, function updates the averages of
   Dot or Dash durations with given @p mark_duration, and recalculates the
   adaptive threshold for the next receive Mark.

   @internal
   @reviewed 2020-08-10
   @endinternal

   @param[in,out] rec receiver
   @param[in] mark_duration duration of a Mark (Dot or Dash)
   @param[in] mark CW_DOT_REPRESENTATION or CW_DASH_REPRESENTATION
*/
void cw_rec_update_averages_internal(cw_rec_t * rec, int mark_duration, char mark)
{
	/* We are not going to tolerate being called in fixed speed mode. */
	if (!rec->is_adaptive_receive_mode) {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_WARNING,
			      MSG_PREFIX "'%s': called 'adaptive' function when receiver is not in adaptive mode\n", rec->label);
		return;
	}

	/* Update moving averages for dots or dashes. */
	if (mark == CW_DOT_REPRESENTATION) {
		cw_rec_update_average_internal(&rec->dot_averaging, mark_duration);
	} else if (mark == CW_DASH_REPRESENTATION) {
		cw_rec_update_average_internal(&rec->dash_averaging, mark_duration);
	} else {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': unknown mark '%c' / '0x%x'\n", rec->label, mark, mark);
		return;
	}

	/* Recalculate the adaptive threshold. */
	const int avg_dot_duration = rec->dot_averaging.average;
	const int avg_dash_duration = rec->dash_averaging.average;
	rec->adaptive_speed_threshold = (avg_dash_duration - avg_dot_duration) / 2 + avg_dot_duration;

	/* We are in adaptive mode. Since ->adaptive_speed_threshold
	   has changed, we need to calculate new ->speed with sync().
	   Low-level parameters will also be re-synchronized to new
	   threshold/speed. */
	rec->parameters_in_sync = false;
	cw_rec_sync_parameters_internal(rec);

	if (rec->speed < CW_SPEED_MIN || rec->speed > CW_SPEED_MAX) {

		/* Clamp the speed. */
		rec->speed = rec->speed < CW_SPEED_MIN ? CW_SPEED_MIN : CW_SPEED_MAX;

		/* Direct manipulation of speed in line above
		   (clamping) requires resetting adaptive mode and
		   re-synchronizing to calculate the new threshold,
		   which unfortunately recalculates everything else
		   according to fixed speed.

		   So, we then have to reset adaptive mode and
		   re-synchronize one more time, to get all other
		   parameters back to where they should be. */

		rec->is_adaptive_receive_mode = false;
		rec->parameters_in_sync = false;
		cw_rec_sync_parameters_internal(rec);

		rec->is_adaptive_receive_mode = true;
		rec->parameters_in_sync = false;
		cw_rec_sync_parameters_internal(rec);
	}

	return;
}




/**
   @brief Add Dot or Dash to receiver's representation buffer

   Function adds a @p mark (either a Dot or a Dash) to the
   receiver's representation buffer.

   Since we can't add a Mark to the buffer without any accompanying timing
   information, the function also accepts @p timestamp of the "end of mark"
   event.  If the @p timestamp is NULL, the timestamp for current time is
   used.

   The receiver's state is updated as if we had just received a call
   to cw_rec_mark_end().

   @internal
   @reviewed 2020-08-10
   @endinternal

   @exception ERANGE invalid state of receiver was discovered.
   @exception EINVAL errors while processing or getting @p timestamp
   @exception ENOMEM space for representation of character has been exhausted

   @param[in,out] rec receiver
   @param[in] timestamp timestamp of "end of mark" event. May be NULL, then current time will be used.
   @param[in] mark Mark to be inserted into receiver's representation buffer

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_rec_add_mark(cw_rec_t * rec, const struct timeval * timestamp, char mark)
{
	/* The receiver's state is expected to be idle or
	   inter-mark-space in order to use this routine. */
	if (RS_IDLE != rec->state && RS_INTER_MARK_SPACE != rec->state) {

		/*
		  Adding of a Mark can only happen while we are idle (waiting
		  for beginning of a first Mark of a new character), or in
		  inter-mark-space of a current character.

		  ->state should be RS_IDLE at the beginning of new character
		  OR
		  ->state should be RS_INTER_MARK_SPACE in the middle of character (between Marks)

		  See cw_rec_mark_begin() for similar condition.
		*/
		errno = ERANGE;
		return CW_FAILURE;
	}

	/* This routine functions as if we have just seen a Mark end,
	   yet without really seeing a Mark start.

	   It doesn't matter that we don't know timestamp of start of
	   this Mark: start timestamp would be needed only to
	   determine Mark duration (and from the Mark duration to
	   determine Mark type (Dot/Dash)). But since the Mark type
	   has been determined by @p mark, we don't need timestamp for
	   beginning of Mark.

	   What does matter is timestamp of end of this Mark. This is
	   because the receiver representation routines that may be
	   called later look at the time since the last end of Mark
	   to determine whether we are at the end of a word, or just
	   at the end of a character. */
	if (CW_SUCCESS != cw_timestamp_validate_internal(&rec->mark_end, timestamp)) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	/* Add the mark to the receiver's representation buffer. */
	rec->representation[rec->representation_ind++] = mark;

	/* We just added a Mark to the receiver's buffer.  As in
	   cw_rec_mark_end(): if the buffer is full full, then we have to do
	   something, even though it's unlikely to actually be full. */
	if (rec->representation_ind == CW_REC_REPRESENTATION_CAPACITY - 1) {

		CW_REC_SET_STATE (rec, RS_EOC_GAP_ERR, (&cw_debug_object));

		cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': add_mark: receiver's representation buffer is full", rec->label);

		errno = ENOMEM;
		return CW_FAILURE;
	}

	/* Since we effectively just saw the end of a Mark, move to
	   the inter-mark-space state. */
	CW_REC_SET_STATE (rec, RS_INTER_MARK_SPACE, (&cw_debug_object));

	return CW_SUCCESS;
}




/**
   @brief Try to poll fully received representation from receiver

   Try to get fully received representation of received character and
   receiver's state flags. The representation is fully received when @p rec
   has recognized that after the last Mark in the representation an
   inter-character-space or inter-word-space has occurred.

   @p timestamp helps to recognize that space. If you call this function
   after a certain period after adding a last Mark to the @p rec, the
   receiver will know that either inter-character-space or inter-word-space
   has elapsed.

   The representation is appended to end of @p representation.

   If receiver is not done with recognizing and receiving a representation,
   it will return error (because representation is not ready yet).

   Function returns a representation of a character. After the character may
   come an inter-character-space or inter-word-space. Which of the two occurs
   depends on value of a timestamp, either passed explicitly through @p
   timestamp, or generated internally by the function at the moment of the
   call. Depending on the duration of the Space, @p is_end_of_word is set
   accordingly.

   @internal
   The fact that the representation is appended may be a problem. Why not
   simply copy (instead of append) the representation?
   @endinternal

   @internal
   @reviewed 2020-08-11
   @endinternal

   @exception ERANGE invalid state of receiver was discovered.
   @exception EINVAL errors while processing or getting @p timestamp
   @exception EAGAIN function called too early, representation not ready yet

   @param[in,out] rec receiver
   @param[in] timestamp (may be NULL)
   @param[out] representation representation of character from receiver's buffer
   @param[out] is_end_of_word flag indicating if receiver is at end of word (may be NULL)
   @param[out] is_error flag indicating whether receiver is in error state (may be NULL)

   @return CW_SUCCESS if a correct representation has been returned through @p representation
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_rec_poll_representation(cw_rec_t * rec,
				    const struct timeval * timestamp,
				    char * representation,
				    bool * is_end_of_word,
				    bool * is_error)
{
	if (RS_EOW_GAP == rec->state || RS_EOW_GAP_ERR == rec->state) {

		/* Until receiver is notified about new mark, its
		   state won't change, and representation stored by
		   receiver's buffer won't change.

		   Repeated calls of this function when receiver is in
		   this state will simply return the same
		   representation over and over again.

		   Because the state of receiver is settled as EOW, @p
		   timestamp is uninteresting. We don't expect it to hold any
		   useful information that could influence receiver's state
		   or representation buffer (the EOW space can't turn into
		   even longer space). */

		cw_rec_poll_representation_eow_internal(rec, representation, is_end_of_word, is_error);
		return CW_SUCCESS;

	} else if (RS_IDLE == rec->state || RS_MARK == rec->state) {

		/* Not a good time/state to call this get()
		   function. */
		errno = ERANGE;
		return CW_FAILURE;

	} else {
		/* Pass to handling other states. */
	}



	/* Four receiver states were covered above, so we are left
	   with these three: */
	cw_assert (RS_INTER_MARK_SPACE == rec->state
		   || RS_EOC_GAP == rec->state
		   || RS_EOC_GAP_ERR == rec->state,

		   MSG_PREFIX "poll: unexpected receiver state %d", rec->state);

	/* Receiver is in one of these states
	   - inter-mark-space, or
	   - inter-character-space, or
	   - end-of-word gap.
	   To see which case is true, calculate duration of this Space
	   by comparing current/given timestamp with end of last
	   Mark. */
	struct timeval now_timestamp;
	if (CW_SUCCESS != cw_timestamp_validate_internal(&now_timestamp, timestamp)) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	const int space_duration = cw_timestamp_compare_internal(&rec->mark_end, &now_timestamp);
	if (INT_MAX == space_duration) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': poll: space duration == INT_MAX", rec->label);

		errno = EINVAL;
		return CW_FAILURE;
	}

	/* Synchronize parameters if required */
	cw_rec_sync_parameters_internal(rec);

	if (space_duration >= rec->ics_duration_min
	    && space_duration <= rec->ics_duration_max) {

		//fprintf(stderr, "ICS: space duration = %d (%d - %d)\n", space_duration, rec->ics_duration_min, rec->ics_duration_max);

		/* The space is, within tolerance, an inter-character-space.

		   We have a complete character representation in
		   receiver's buffer and we can return it. */
		cw_rec_poll_representation_eoc_internal(rec, space_duration, representation, is_end_of_word, is_error);
		return CW_SUCCESS;

	} else if (space_duration > rec->ics_duration_max) {

		// fprintf(stderr, "EOW: space duration = %d (> %d) ------------- \n", space_duration, rec->ics_duration_max);

		/* The space is too long for inter-character-space
		   state. This should be end-of-word state. We have
		   to inform client code about this, too.

		   We have a complete character representation in
		   receiver's buffer and we can return it. */
		cw_rec_poll_representation_eow_internal(rec, representation, is_end_of_word, is_error);
		return CW_SUCCESS;

	} else { /* space_duration < rec->ics_duration_min */
		/* We are still inside a character (inside an
		   inter-mark-space, to be precise). The receiver
		   can't return a representation, because building a
		   representation is not finished yet.

		   So it is too early to return a representation,
		   because it's not complete yet. */

		errno = EAGAIN;
		return CW_FAILURE;
	}
}




/**
   @brief Return representation and flags of receiver that is at inter-character-space state

   Return representation of received character and receiver's state
   flags after receiver has encountered inter-character-space. The
   representation is appended to end of @p representation.

   Update receiver's state so that it matches inter-character-space state.

   Since this is _eoc_ function, @p is_end_of_word is set to false.

   @internal
   @reviewed 2020-08-11
   @endinternal

   @param[in,out] rec receiver
   @param[in] space_duration duration of current inter-character-space
   @param[out] representation representation of character from receiver's buffer
   @param[out] is_end_of_word flag indicating if receiver is at end of word (may be NULL)
   @param[out] is_error flag indicating whether receiver is in error state (may be NULL)
*/
void cw_rec_poll_representation_eoc_internal(cw_rec_t * rec,
					     int space_duration,
					     char * representation,
					     bool * is_end_of_word,
					     bool * is_error)
{
	if (RS_INTER_MARK_SPACE == rec->state) {
		/* Current state of receiver is inter-mark-space, but real
		   duration of current space (@p space_duration) turned out
		   to be a bit longer than acceptable inter-mark-space
		   (duration of Space indicates that it's
		   inter-character-space). This is why this function is
		   called _eoc_.

		   Update duration statistics for Space identified as
		   inter-character-space. */
		cw_rec_duration_stats_update_internal(rec, CW_REC_STAT_INTER_CHARACTER_SPACE, space_duration);

		/* Set the new real state of receiver. */
		CW_REC_SET_STATE (rec, RS_EOC_GAP, (&cw_debug_object));
	} else {
		cw_assert (RS_EOC_GAP == rec->state || RS_EOC_GAP_ERR == rec->state,
			   MSG_PREFIX "poll eoc: unexpected state of receiver: %d / %s",
			   rec->state, cw_receiver_states[rec->state]);

		/* We are already in RS_EOC_GAP or RS_EOC_GAP_ERR, so nothing to do. */
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_INFO,
		      MSG_PREFIX "'%s': poll eoc: state: %s", rec->label, cw_receiver_states[rec->state]);

	/* Return receiver's state. */
	if (is_end_of_word) {
		*is_end_of_word = false;
	}
	if (is_error) {
		*is_error = (RS_EOC_GAP_ERR == rec->state);
	}

	/* Append representation from receiver's buffer to caller's buffer. */
	*representation = '\0';
	strncat(representation, rec->representation, rec->representation_ind);

	/* Since we are in eoc state, there will be no more Dots or Dashes added to current representation. */
	rec->representation[rec->representation_ind] = '\0';

	return;
}




/**
   @brief Return representation and flags of receiver that is at end-of-word Space state

   Return representation of received character and receiver's state
   flags after receiver has encountered end-of-word gap. The
   representation is appended to end of @p representation.

   Update receiver's state so that it matches end-of-word state.

   Since this is _eow_ function, @p is_end_of_word is set to true.

   @internal
   @reviewed 2020-08-11
   @endinternal

   @param[in,out] rec receiver
   @param[out] representation representation of character from receiver's buffer
   @param[out] is_end_of_word flag indicating if receiver is at end of word (may be NULL)
   @param[out] is_error flag indicating whether receiver is in error state (may be NULL)
*/
void cw_rec_poll_representation_eow_internal(cw_rec_t * rec,
					     char * representation,
					     bool * is_end_of_word,
					     bool * is_error)
{
	if (RS_EOC_GAP == rec->state || RS_INTER_MARK_SPACE == rec->state) {
		CW_REC_SET_STATE (rec, RS_EOW_GAP, (&cw_debug_object)); /* Transition of state. */

	} else if (RS_EOC_GAP_ERR == rec->state) {
		CW_REC_SET_STATE (rec, RS_EOW_GAP_ERR, (&cw_debug_object)); /* Transition of state with preserving error. */

	} else if (RS_EOW_GAP_ERR == rec->state || RS_EOW_GAP == rec->state) {
		; /* No need to change state. */

	} else {
		cw_assert (0, MSG_PREFIX "poll eow: unexpected receiver state %d / %s", rec->state, cw_receiver_states[rec->state]);
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_RECEIVE_STATES, CW_DEBUG_INFO,
		      MSG_PREFIX "'%s': poll eow: state: %s", rec->label, cw_receiver_states[rec->state]);

	/* Return receiver's state. */
	if (is_end_of_word) {
		*is_end_of_word = true;
	}
	if (is_error) {
		*is_error = (RS_EOW_GAP_ERR == rec->state);
	}

	/* Append representation from receiver's buffer to caller's buffer. */
	*representation = '\0';
	strncat(representation, rec->representation, rec->representation_ind);

	/* Since we are in eow state, there will be no more Dots or Dashes added to current representation. */
	rec->representation[rec->representation_ind] = '\0';

	return;
}




/**
   @brief Try to poll fully received character from receiver

   Try to get fully received and recognized character and receiver's state
   flags. The character is fully received when @p rec has recognized that
   after the last Mark in the character an inter-character-space or
   inter-word-space has occurred.

   @p timestamp helps to recognize that Space. If you call this function
   after a certain period after adding a last Mark to the @p rec, the
   receiver will know that either inter-character-space or inter-word-space
   has elapsed.

   The character is returned through @p character.

   Function returns a character. After the character may come an
   inter-character-space or inter-word-space. Which of the two occurs depends
   on value of a timestamp, either passed explicitly through @p timestamp, or
   generated internally by the function at the moment of the call. Depending
   on the duration of the Space, @p is_end_of_word is set accordingly.

   @internal
   @reviewed 2020-08-11
   @endinternal

   @exception ERANGE invalid state of receiver was discovered.
   @exception EINVAL errors while processing or getting @p timestamp
   @exception EAGAIN function called too early, character not ready yet
   @exception ENOENT function can't convert representation retrieved from receiver into a character

   @param[in,out] rec receiver
   @param[in] timestamp (may be NULL)
   @param[out] character character received by receiver
   @param[out] is_end_of_word flag indicating if receiver is at end of word (may be NULL)
   @param[out] is_error flag indicating whether receiver is in error state (may be NULL)

   @return CW_SUCCESS if a character has been recognized by receiver and is returned through @p character
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_rec_poll_character(cw_rec_t * rec,
			       const struct timeval * timestamp,
			       char * character,
			       bool * is_end_of_word,
			       bool * is_error)
{
	/* TODO: in theory we don't need these intermediate bool
	   variables, since is_end_of_word and is_error won't be
	   modified by any function on !success. */
	bool end_of_word, error;

	char representation[CW_REC_REPRESENTATION_CAPACITY + 1];

	/* See if we can obtain a representation from receiver. */
	cw_ret_t cwret = cw_rec_poll_representation(rec, timestamp,
						    representation,
						    &end_of_word, &error);
	if (CW_SUCCESS != cwret) {
		return CW_FAILURE;
	}

	/* Look up the representation using the lookup functions. */
	int looked_up = cw_representation_to_character_internal(representation);
	if (0 == looked_up) {
		errno = ENOENT;
		return CW_FAILURE;
	}

	/* A full character has been received. Directly after it comes a
	   Space. Either a short inter-character-space followed by another
	   character (in this case we won't display the
	   inter-character-space), or longer inter-word space - this Space we
	   would like to catch and display.

	   Set a flag indicating that next poll may result in
	   inter-word space.

	   FIXME: why we don't set this flag in
	   cw_rec_poll_representation()? */
	if (!end_of_word) {
		rec->is_pending_inter_word_space = true;
	}

	/* If we got this far, all is well, so return what we received. */
	if (character) {
		*character = looked_up;
	}
	if (is_end_of_word) {
		*is_end_of_word = end_of_word;
	}
	if (is_error) {
		*is_error = error;
	}
	return CW_SUCCESS;
}




/**
   @internal
   @reviewed 2020-08-11
   @endinternal
*/
void cw_rec_reset_state(cw_rec_t * rec)
{
	memset(rec->representation, 0, sizeof (rec->representation));
	rec->representation_ind = 0;

	rec->is_pending_inter_word_space = false;

	CW_REC_SET_STATE (rec, RS_IDLE, (&cw_debug_object));

	return;
}




/**
   @brief Get the number of Marks (Dots/Dashes) the receiver's buffer can accommodate

   The value returned by this function is the maximum number of Marks written
   out by cw_rec_poll_representation() (not including terminating NUL).

   @internal
   @reviewed 2020-08-11
   @endinternal

   @return number of Marks that can be stored in receiver's representation buffer
*/
int cw_rec_get_receive_buffer_capacity_internal(void)
{
	return CW_REC_REPRESENTATION_CAPACITY;
}




/**
   @brief Get number of Marks (Dots and Dashes) currently stored in receiver's representation buffer

   @internal
   @reviewed on 2020-08-11
   @endinternal

   @param[in] rec receiver from which to get current number of Marks
*/
int cw_rec_get_buffer_length_internal(const cw_rec_t * rec)
{
	return rec->representation_ind;
}




/**
   @brief Reset receiver's essential parameters to their initial values

   @internal
   @reviewed 2020-08-11
   @endinternal

   @param[in,out] rec receiver
*/
void cw_rec_reset_parameters_internal(cw_rec_t * rec)
{
	cw_assert (NULL != rec, MSG_PREFIX "reset parameters: receiver is NULL");

	rec->speed = CW_SPEED_INITIAL;
	rec->tolerance = CW_TOLERANCE_INITIAL;
	rec->is_adaptive_receive_mode = CW_REC_ADAPTIVE_MODE_INITIAL;
	rec->noise_spike_threshold = CW_REC_NOISE_THRESHOLD_INITIAL;

	/* FIXME: consider resetting ->gap as well. */

	rec->parameters_in_sync = false;

	return;
}




/**
   @brief Synchronize receivers' parameters

   @internal
   @reviewed 2020-08-11
   @endinternal

   @param[in,out] rec receiver
*/
void cw_rec_sync_parameters_internal(cw_rec_t * rec)
{
	cw_assert (NULL != rec, MSG_PREFIX "sync parameters: receiver is NULL");

	/* Do nothing if we are already synchronized. */
	if (rec->parameters_in_sync) {
		return;
	}

	/* First, depending on whether we are set for fixed speed or
	   adaptive speed, calculate either the threshold from the
	   receive speed, or the receive speed from the threshold,
	   knowing that the threshold is always, effectively, two dot
	   durations.  Weighting is ignored for receive parameters,
	   although the core unit duration is recalculated for the
	   receive speed, which may differ from the send speed. */

	/* FIXME: shouldn't we move the calculation of unit_duration (that
	   depends on rec->speed) after the calculation of
	   rec->speed? */
	const int unit_duration = CW_DOT_CALIBRATION / rec->speed;

	if (rec->is_adaptive_receive_mode) {
		rec->speed = CW_DOT_CALIBRATION	/ (rec->adaptive_speed_threshold / 2.0);
	} else {
		rec->adaptive_speed_threshold = 2 * unit_duration;
	}



	rec->dot_duration_ideal = unit_duration;
	rec->dash_duration_ideal = 3 * unit_duration;
	rec->ims_duration_ideal = unit_duration;
	rec->ics_duration_ideal = 3 * unit_duration;

	/* These two lines mimic calculations done in
	   cw_gen_sync_parameters_internal().  See the function for
	   more comments. */
	rec->additional_delay = rec->gap * unit_duration;
	rec->adjustment_delay = (7 * rec->additional_delay) / 3;

	/* Set duration ranges of low level parameters. The duration
	   ranges depend on whether we are required to adapt to the
	   incoming Morse code speeds. */
	if (rec->is_adaptive_receive_mode) {
		/* Adaptive receiving mode. */
		rec->dot_duration_min = 0;
		rec->dot_duration_max = 2 * rec->dot_duration_ideal;

		/* Any mark longer than Dot is a Dash in adaptive
		   receiving mode. */

		/* FIXME: shouldn't this be '= rec->dot_duration_max + 1'?
		   now the duration ranges for Dot and Dash overlap. */
		rec->dash_duration_min = rec->dot_duration_max;
		rec->dash_duration_max = INT_MAX;

#if 0
		int debug_ics_duration_max = rec->ics_duration_max;
#endif

		/* Make the inter-mark-space be anything up to the
		   adaptive threshold durations - that is two Dots.  And
		   the inter-character-space is anything longer than
		   that, and shorter than five Dots. */
		rec->ims_duration_min = rec->dot_duration_min;
		rec->ims_duration_max = rec->dot_duration_max;
		rec->ics_duration_min = rec->ims_duration_max;
		rec->ics_duration_max = 5 * rec->dot_duration_ideal;

#if 0
		if (debug_ics_duration_max != rec->ics_duration_max) {
			fprintf(stderr, "ics_duration_max changed from %d to %d --------\n", debug_ics_duration_max, rec->ics_duration_max);
		}
#endif

	} else {
		/* Fixed speed receiving mode. */

		int tolerance = (rec->dot_duration_ideal * rec->tolerance) / 100; /* [%] */
		rec->dot_duration_min = rec->dot_duration_ideal - tolerance;
		rec->dot_duration_max = rec->dot_duration_ideal + tolerance;
		rec->dash_duration_min = rec->dash_duration_ideal - tolerance;
		rec->dash_duration_max = rec->dash_duration_ideal + tolerance;

		/* Make the inter-mark-space the same as the dot
		   duration range. */
		rec->ims_duration_min = rec->dot_duration_min;
		rec->ims_duration_max = rec->dot_duration_max;

		/* Make the inter-character-space, expected to be
		   three dots, the same as dash duration range at the
		   lower end, but make it the same as the dash duration
		   range _plus_ the "Farnsworth" delay at the top of
		   the duration range. */
		rec->ics_duration_min = rec->dash_duration_min;
		rec->ics_duration_max = rec->dash_duration_max
			+ rec->additional_delay + rec->adjustment_delay;

		/* Any gap longer than ics_duration_max is by implication
		   end-of-word gap. */
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_PARAMETERS, CW_DEBUG_INFO,
		      MSG_PREFIX "'%s': sync parameters: receive usec timings <%.2f [wpm]>: dot: %d-%d [ms], dash: %d-%d [ms], ims: %d-%d[%d], %d-%d[%d], thres: %d [us]",
		      rec->label,
		      (double) rec->speed, /* Casting to double to avoid compiler warning about implicit conversion from float to double. */
		      rec->dot_duration_min, rec->dot_duration_max,
		      rec->dash_duration_min, rec->dash_duration_max,
		      rec->ims_duration_min, rec->ims_duration_max, rec->ims_duration_ideal,
		      rec->ics_duration_min, rec->ics_duration_max, rec->ics_duration_ideal,
		      rec->adaptive_speed_threshold);

	/* Receiver parameters are now in sync. */
	rec->parameters_in_sync = true;

	return;
}



/**
   @internal
   @reviewed 2020-08-11
   @endinternal
*/
int cw_rec_set_label(cw_rec_t * rec, const char * label)
{
	if (NULL == rec) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'rec' argument is NULL");
		return CW_FAILURE;
	}
	if (NULL == label) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': 'label' argument is NULL", rec->label);
		return CW_FAILURE;
	}
	if (strlen(label) > (LIBCW_OBJECT_INSTANCE_LABEL_SIZE - 1)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_WARNING,
			      MSG_PREFIX "'%s': new label '%s' too long, truncating", rec->label, label);
		/* Not an error, just log warning. New label will be truncated. */
	}

	/* Notice that empty label is acceptable. In such case we will
	   erase old label. */

	snprintf(rec->label, sizeof (rec->label), "%s", label);

	return CW_SUCCESS;
}




/**
   @internal
   @reviewed 2020-08-11
   @endinternal
*/
int cw_rec_get_label(const cw_rec_t * rec, char * label, size_t size)
{
	if (NULL == rec) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'rec' argument is NULL");
		return CW_FAILURE;
	}
	if (NULL == label) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': 'label' argument is NULL", rec->label);
		return CW_FAILURE;
	}

	/* Notice that we don't care if size is zero. */

	snprintf(label, size, "%s", rec->label);

	return CW_SUCCESS;
}
