/*
 * Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
 * Copyright (C) 2011-2017  Kamil Ignacak (acerion@wp.pl)
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

#ifndef H_LIBCW_DEBUG
#define H_LIBCW_DEBUG




#include <assert.h>
#include <stdbool.h>
#include <stdint.h> /* uint32_t */
#include <stdio.h>




#if defined(__cplusplus)
extern "C"
{
#endif




/* ************************** */
/* Basic debugging utilities. */
/* ************************** */




#define CW_DEBUG_N_EVENTS_MAX (1024 * 128)




typedef struct {
	uint32_t flags; /* See libcw.h, enum with CW_DEBUG_* values. */

	int n;       /* Event counter. */
	int n_max;   /* Flush threshold. */

	/* Current debug level. */
	int level;

	/* Human-readable labels for debug levels. */
	const char ** level_labels;

	/* This is used only by utilities declared in section "Stuff used for
	   advanced logging of events in library.". */
	struct {
		uint32_t event;   /* Event ID. One of CW_DEBUG_EVENT_* values. */
		long long sec;    /* Time of registering the event - second. */
		long long usec;   /* Time of registering the event - microsecond. */
	} events[CW_DEBUG_N_EVENTS_MAX] __attribute__((deprecated));
} cw_debug_t;




void     cw_debug_set_flags(cw_debug_t * debug_object, uint32_t flags);
uint32_t cw_debug_get_flags(const cw_debug_t * debug_object);
bool     cw_debug_has_flag(const cw_debug_t * debug_object, uint32_t flag);




void     cw_set_debug_flags(uint32_t flags)    __attribute__ ((deprecated));
uint32_t cw_get_debug_flags(void)              __attribute__ ((deprecated));




#define cw_debug_msg(debug_object, flag, debug_level, ...) {	\
	if ((debug_level) >= (debug_object)->level) {		\
		if ((debug_object)->flags & (uint32_t) (flag)) {		\
			fprintf(stderr, "%s ", (debug_object)->level_labels[(debug_level)]); \
			if ((debug_level) == CW_DEBUG_DEBUG || (debug_level) == CW_DEBUG_ERROR) { \
				fprintf(stderr, "%s: %d: ", __func__, __LINE__); \
			}						\
			fprintf(stderr, __VA_ARGS__);			\
			fprintf(stderr, "\n");				\
		}							\
	}								\
}




/* ********************************************************************************** */
/* ******** Don't use any items from below this line, they are deprecated. ********** */
/* ********************************************************************************** */




/* ******************************* */
/* Additional debugging utilities. */
/* ******************************* */

/*
  Code in this section is considered deprecated. Don't use it in client code.

  Code in this section is marked as deprecated because it is redundant. Use
  either cw_debug_msg() or fprintf().
*/

/**
   @brief Print debug message - verbose version

   This macro behaves much like fprintf(stderr, ...) function, caller
   only have to provide format string with converesion specifiers and
   list of arguments for this format string.

   Each message is preceeded with name of function that called the
   macro.

   See "C: A Reference Manual", chapter 3.3.10 for more information on
   variable argument lists in macros (it requires C99).
*/
#ifndef NDEBUG
#define cw_vdm(...) fprintf(stderr, "%s():%d: ", __func__, __LINE__); fprintf(stderr, __VA_ARGS__);
#else
#define cw_vdm(...)
#endif




/* ***************************************************** */
/* Stuff used for advanced logging of events in library. */
/* ***************************************************** */

/*
  Code in this section is considered deprecated. Don't use it in client code.

  Code in this section is marked as deprecated because it should be used only
  during development of libcw, and should not be used by client code.
*/

void cw_debug_event_internal(cw_debug_t *debug_object, uint32_t flag, uint32_t event, const char *func, int line) __attribute__ ((deprecated));
#define cw_debug_ev(debug_object, flag, event)				\
	{								\
		cw_debug_event_internal((debug_object), flag, event, __func__, __LINE__); \
	}

enum {
	CW_DEBUG_EVENT_TONE_LOW  = 0,         /* Tone with non-zero frequency. */
	CW_DEBUG_EVENT_TONE_MID,              /* A state between LOW and HIGH, probably unused. */
	CW_DEBUG_EVENT_TONE_HIGH,             /* Tone with zero frequency. */
	CW_DEBUG_EVENT_TQ_JUST_EMPTIED,       /* A last tone from libcw's queue of tones has been dequeued, making the queue empty. */
	CW_DEBUG_EVENT_TQ_NONEMPTY,           /* A tone from libcw's queue of tones has been dequeued, but the queue is still non-empty. */
	CW_DEBUG_EVENT_TQ_STILL_EMPTY         /* libcw's queue of tones has been asked for tone, but there were no tones on the queue. */
} __attribute__ ((deprecated));




/* ********** */
/* Assertions */
/* ********** */

/*
  Code in this section is considered deprecated. Don't use it in client code.

  Code in this section is considered as deprecated because it should be used
  only during development of libcw, and should not be used by client code.
*/

/**
   @brief Assert macro with message
*/
#ifndef NDEBUG
#define cw_assert(expr, ...)					\
	if (! (expr)) {						\
		fprintf(stderr, "\n\nassertion failed in:\n");	\
		fprintf(stderr, "file %s\n", __FILE__);		\
		fprintf(stderr, "line %d\n", __LINE__);		\
		cw_vdm (__VA_ARGS__);				\
		fprintf(stderr, "\n\n");			\
		assert (expr);                                  \
	}
#else
	/* "if ()" expression prevents compiler warnings about unused
	   variables. */
#define cw_assert(expr, ...) { if (expr) {} }
#endif




/* ******************************************************* */
/* Utilities used only during development of libcw itself. */
/* ******************************************************* */

/*
  Code in this section is considered deprecated. Don't use it in client code.

  Code in this section is marked as deprecated because it should be used only
  during development of libcw, and should not be used by client code.
*/

#ifdef LIBCW_WITH_DEV
/* Including private header in public header is accepted here only because it
   is inside of "build with development tests" flag. */
#include "libcw_gen.h"
int  cw_dev_debug_raw_sink_write_internal(cw_gen_t * gen) __attribute__ ((deprecated));
void cw_dev_debug_print_generator_setup_internal(const cw_gen_t * gen) __attribute__ ((deprecated));
#endif




#if defined(__cplusplus)
}
#endif




#endif  /* H_LIBCW_DEBUG */
