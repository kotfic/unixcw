/*
 * Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
 * Copyright (C) 2011-2020  Kamil Ignacak (acerion@wp.pl)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */




/**
   @file libcw_debug.c

   @brief Debugging facility to libcw and applications using the library.
*/




#include "config.h"




#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>




#include "libcw.h"
#include "libcw_debug.h"
#include "libcw_gen.h"
#include "libcw_utils.h"




#define MSG_PREFIX "libcw/debug: "




static struct {
	int flag;
	const char * message;
} event_logging_strings[] = {
	{ CW_DEBUG_EVENT_TONE_LOW,        "CW_DEBUG_EVENT_TONE_LOW"        },
	{ CW_DEBUG_EVENT_TONE_MID,        "CW_DEBUG_EVENT_TONE_MID"        },
	{ CW_DEBUG_EVENT_TONE_HIGH,       "CW_DEBUG_EVENT_TONE_HIGH"       },

	{ CW_DEBUG_EVENT_TQ_JUST_EMPTIED, "CW_DEBUG_EVENT_TQ_JUST_EMPTIED" },
	{ CW_DEBUG_EVENT_TQ_NONEMPTY,     "CW_DEBUG_EVENT_TQ_NONEMPTY"     },
	{ CW_DEBUG_EVENT_TQ_STILL_EMPTY,  "CW_DEBUG_EVENT_TQ_STILL_EMPTY"  }
};




/* Human-readable labels for debug levels.
   Other modules can access the table only through pointer in debug
   object. I don't expose this table (by making it globally visible)
   to decrease number of 'extern' declarations. */
static const char * cw_debug_level_labels[] = { "[DD]", "[II]", "[WW]", "[EE]" };




cw_debug_t cw_debug_object = {
	.flags = CW_DEBUG_STDLIB | CW_DEBUG_SOUND_SYSTEM,
	.n = 0,
	.n_max = 1,
	.level = CW_DEBUG_NONE,
	.level_labels = cw_debug_level_labels
};

cw_debug_t cw_debug_object_dev = {
	.flags = CW_DEBUG_SOUND_SYSTEM,
	.n = 0,
	.n_max = 1,
	.level = CW_DEBUG_NONE,
	.level_labels = cw_debug_level_labels
};

cw_debug_t cw_debug_object_ev = {
	.flags = 0,
	.n = 0,
	.n_max = CW_DEBUG_N_EVENTS_MAX,
	.level = CW_DEBUG_NONE,
	.level_labels = cw_debug_level_labels
};




static void cw_event_debugging_flush_internal(cw_debug_t * debug_object);




/**
   @brief Write all events from the debug object to a file

   Function writes all events stored in the @p debug_object to file
   associated with the object, and removes the events.

   List of events is preceded with "FLUSH START\n" line, and
   followed by "FLUSH END\n" line.

   Function is used only in advanced debugging of events in library.

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param debug_object[in] debug object from which to flush events
*/
void cw_event_debugging_flush_internal(cw_debug_t * debug_object)
{
	if (debug_object->n <= 0) {
		return;
	}

	long long int diff = debug_object->events[debug_object->n - 1].sec - debug_object->events[0].sec;
	diff = debug_object->events[debug_object->n - 1].sec - diff - 1;

	fprintf(stderr, "FLUSH START\n");
	for (int i = 0; i < debug_object->n; i++) {
		fprintf(stderr, "libcwevent:\t%06lld%06lld\t%s\n",
			debug_object->events[i].sec - diff, debug_object->events[i].usec,
			event_logging_strings[debug_object->events[i].event].message);
	}
	debug_object->n = 0;
	fprintf(stderr, "FLUSH END\n");

	fflush(stderr);

	return;
}




/**
   @brief Set a value of debug flags in given debug variable

   Assign specified value to given debug variable.

   Note that this function doesn't *append* given flag to the
   variable, it erases existing value and assigns new one. Use
   cw_debug_get_flags() if you want to OR new flag with existing ones.

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param debug_object[in] debug object for which to set flags
   @param flags[in] new value to be assigned to the object, a sum of one or
   more of CW_DEBUG_* flags from libcw.h
*/
void cw_debug_set_flags(cw_debug_t * debug_object, uint32_t flags)
{
	debug_object->flags = flags;
	return;
}




/**
   @brief Get library's current debug flags

   Function returns value of library's internal debug variable, a sum of one
   or more of CW_DEBUG_* flags from libcw.h.

   @internal
   @reviewed 2020-08-01

   TODO: there is some initialization code using LIBCW_DEBUG env. Perhaps we
   should include it somehow in libcw2 code.
   @endinternal

   @return value of library's debug flags variable
*/
uint32_t cw_get_debug_flags(void)
{
	/* TODO: extract reading LIBCW_DEBUG env
	   variable to separate function. */

	static bool is_initialized = false;

	if (!is_initialized) {
		/* Do not overwrite any debug flags already set. */
		if (cw_debug_object.flags == 0) {

			/*
			 * Set the debug flags from LIBCW_DEBUG.  If it is an invalid
			 * numeric, treat it as 0; there is no error checking.
			 */
			const char * debug_value = getenv("LIBCW_DEBUG");
			if (debug_value) {
				cw_debug_object.flags = strtoul(debug_value, NULL, 0);
			}
		}

		is_initialized = true;
	}

	return cw_debug_object.flags;
}




/**
   @brief Get current debug flags from given debug object

   Function returns value of debug object's debug flags, a sum of one or more
   of CW_DEBUG_* flags from libcw.h.

   @internal
   @reviewed 2020-08-01
   @endinternal

   @return value of debug object's debug flags
*/
uint32_t cw_debug_get_flags(const cw_debug_t * debug_object)
{
	/* FIXME: what about initialization? */
	return debug_object->flags;
}




/**
   @brief Check if given debug flag is set

   Function checks if a specified debug flag is set in given debug object.

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param debug object[in] - debug object to be checked
   @param flag[in] flag to be checked, one of CW_DEBUG_* flags from libcw.h

   @return true if given flag is set
   @return false otherwise
*/
bool cw_debug_has_flag(const cw_debug_t * debug_object, uint32_t flag)
{
	if (debug_object) {
		return debug_object->flags & flag;
	} else {
		return false;
	}
}




#ifdef LIBCW_WITH_DEV




/**
   @brief Print configuration of generator

   Print to stderr some of generator's parameters.

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param gen[in] generator
*/
void cw_dev_debug_print_generator_setup(const cw_gen_t * gen)
{
	fprintf(stderr, "sound system:         %s\n",     cw_get_audio_system_label(gen->sound_system));
#ifdef LIBCW_WITH_OSS
	if (gen->sound_system == CW_AUDIO_OSS) {
		fprintf(stderr, "OSS version           %X.%X.%X\n",
			gen->oss_data.version_x, gen->oss_data.version_y, gen->oss_data.version_z);
	}
#endif
	fprintf(stderr, "sound device:         \"%s\"\n",  gen->sound_device);
	fprintf(stderr, "sample rate:          %d Hz\n",  gen->sample_rate);

	/* Temporarily disabled because gen->pa_data.ba is missing. */
#if 0 // def LIBCW_WITH_PULSEAUDIO
	if (gen->sound_system == CW_AUDIO_PA) {
		fprintf(stderr, "PulseAudio latency:   %llu us\n", (unsigned long long int) gen->pa_data.latency_usecs);

		if (gen->pa_data.ba.prebuf == (uint32_t) -1) {
			fprintf(stderr, "PulseAudio prebuf:    (not set)\n");
		} else {
			fprintf(stderr, "PulseAudio prebuf:    %u bytes\n", (uint32_t) gen->pa_data.ba.prebuf);
		}

		if (gen->pa_data.ba.tlength == (uint32_t) -1) {
			fprintf(stderr, "PulseAudio tlength:   (not set)\n");
		} else {
			fprintf(stderr, "PulseAudio tlength:   %u bytes\n", (uint32_t) gen->pa_data.ba.tlength);
		}

		if (gen->pa_data.ba.minreq == (uint32_t) -1) {
			fprintf(stderr, "PulseAudio minreq:    (not set)\n");
		} else {
			fprintf(stderr, "PulseAudio minreq:    %u bytes\n", (uint32_t) gen->pa_data.ba.minreq);
		}

		if (gen->pa_data.ba.maxlength == (uint32_t) -1) {
			fprintf(stderr, "PulseAudio maxlength: (not set)\n");
		} else {
			fprintf(stderr, "PulseAudio maxlength: %u bytes\n", (uint32_t) gen->pa_data.ba.maxlength);
		}

#if 0	        /* not relevant to playback */
		if (gen->pa_data.ba.fragsize == (uint32_t) -1) {
			fprintf(stderr, "PulseAudio fragsize:  (not set)\n");
		} else {
			fprintf(stderr, "PulseAudio fragsize:  %u bytes\n", (uint32_t) gen->pa_data.ba.fragsize);
		}
#endif

	}
#endif // #ifdef LIBCW_WITH_PULSEAUDIO

	fprintf(stderr, "send speed:           %d wpm\n", gen->send_speed);
	fprintf(stderr, "volume:               %d %%\n",  gen->volume_percent);
	fprintf(stderr, "frequency:            %d Hz\n",  gen->frequency);
	fprintf(stderr, "sound buffer size:    %d\n",     gen->buffer_n_samples);

	fprintf(stderr, "debug sink file:      %s\n", gen->dev_raw_sink != -1 ? "yes" : "no");

	return;
}




/**
   @brief Write generator's samples to debug file

   This function does any actual writing only for generators
   configured to use OSS, Alsa and PulseAudio sound sinks. Using the
   function on generators configured with other sound sinks doesn't
   produce any output and the function always returns CW_SUCCESS.

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param gen[in] generator

   @return CW_SUCCESS on write success
   @return CW_FAILURE otherwise
*/
int cw_dev_debug_raw_sink_write_internal(cw_gen_t * gen)
{
	if (gen->sound_system != CW_AUDIO_OSS
	    && gen->sound_system != CW_AUDIO_ALSA
	    && gen->sound_system != CW_AUDIO_PA) {

		return CW_SUCCESS;
	}

	if (gen->dev_raw_sink != -1) {
#if CW_DEV_RAW_SINK_MARKERS
		/* FIXME: this will cause memory access error at
		   the end, when generator is destroyed in the
		   other thread */
		gen->buffer[0] = 0x7fff;
		gen->buffer[1] = 0x7fff;
		gen->buffer[samples - 2] = 0x8000;
		gen->buffer[samples - 1] = 0x8000;
#endif

		const size_t n_bytes = sizeof (gen->buffer[0]) * gen->buffer_n_samples;

		int rv = write(gen->dev_raw_sink, gen->buffer, n_bytes);
		if (rv == -1) {
			cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_STDLIB, CW_DEBUG_ERROR,
				      MSG_PREFIX "write error: %s (gen->dev_raw_sink = %ld, gen->buffer = %ld, n_bytes = %zu)", strerror(errno), (long) gen->dev_raw_sink, (long) gen->buffer, n_bytes);
			return CW_FAILURE;
		}
	}

	return CW_SUCCESS;
}




/**
   @brief Add debug event to debug object

   Function is used only in advanced debugging of events in library.

   @internal
   @reviewed 2020-08-01
   @endinternal

   @param debug_object[in] debug object
   @param flag[in] one of CW_DEBUG_* flags from libcw.h
   @param event[in] one of CW_DEBUG_EVENT_* flags from libcw_debug.h
   @param func[in] function where the event occurred
   @param line[in] file line in which the event occurred
*/
void cw_debug_event_internal(cw_debug_t * debug_object, uint32_t flag, uint32_t event, const char * func, int line)
{
	if (NULL == debug_object) {
		return;
	}

	if (!cw_debug_has_flag(debug_object, flag)) {
		return;
	}

	struct timeval now;
	gettimeofday(&now, NULL);

	debug_object->events[debug_object->n].event = event;
	debug_object->events[debug_object->n].sec = (long long int) now.tv_sec;
	debug_object->events[debug_object->n].usec = (long long int) now.tv_usec;

	debug_object->n++;

	if (debug_object->n >= debug_object->n_max) {
		cw_event_debugging_flush_internal(debug_object);
	}

	return;
}




#endif /* #ifdef LIBCW_WITH_DEV */
