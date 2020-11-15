/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef H_LIBCW_PA
#define H_LIBCW_PA




#include "config.h"




#ifdef LIBCW_WITH_PULSEAUDIO

#include <pulse/error.h>
#include <pulse/simple.h>

typedef struct cw_pa_data_struct {
	pa_simple * simple;    /* Audio handle. */
	pa_sample_spec spec;   /* Sample specification. */
	pa_usec_t latency_usecs;
} cw_pa_data_t;

#endif /* #ifdef LIBCW_WITH_PULSEAUDIO */




#include "libcw_gen.h"




cw_ret_t cw_pa_init_gen_internal(cw_gen_t * gen);




#endif /* #ifndef H_LIBCW_PA */
