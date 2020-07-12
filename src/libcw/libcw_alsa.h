/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef H_LIBCW_ALSA
#define H_LIBCW_ALSA




#include "config.h"




#ifdef LIBCW_WITH_ALSA




#include <alsa/asoundlib.h>

typedef struct cw_alsa_data_struct {
	snd_pcm_t * pcm_handle; /* Output handle for sound data. */
} cw_alsa_data_t;




#endif /* #ifdef LIBCW_WITH_ALSA */




#include "libcw2.h"
#include "libcw_gen.h"




cw_ret_t cw_alsa_fill_gen_internal(cw_gen_t * gen, const char * device_name);
void cw_alsa_drop_internal(cw_gen_t * gen);




#endif /* #ifndef H_LIBCW_ALSA */
