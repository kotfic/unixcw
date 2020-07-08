/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef H_LIBCW_PA
#define H_LIBCW_PA




#include "config.h"




#ifdef LIBCW_WITH_PULSEAUDIO

#include <pulse/simple.h>
#include <pulse/error.h>

typedef struct cw_pa_data_struct {
	pa_simple *s;       /* Audio handle. */
	pa_sample_spec ss;  /* Sample specification. */
	pa_usec_t latency_usecs;

	pa_buffer_attr ba;
} cw_pa_data_t;

#endif /* #ifdef LIBCW_WITH_PULSEAUDIO */




#include "libcw_gen.h"




cw_ret_t cw_pa_fill_gen_internal(cw_gen_t * gen, const char * device_name);




#endif /* #ifndef H_LIBCW_PA */
