/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef _LIBCW_GEN_INTERNAL_H_
#define _LIBCW_GEN_INTERNAL_H_




#include <stdbool.h>



#include "libcw_gen.h"
#include "libcw_tq.h"
#include "libcw_utils.h"




/* Internal functions of this module, exposed to unit tests code. */




CW_STATIC_FUNC cw_ret_t cw_gen_new_open_internal(cw_gen_t * gen, int sound_system, const char * device_name);
CW_STATIC_FUNC void * cw_gen_dequeue_and_generate_internal(void * arg);
CW_STATIC_FUNC int    cw_gen_calculate_sine_wave_internal(cw_gen_t * gen, cw_tone_t * tone);
CW_STATIC_FUNC int    cw_gen_calculate_sample_amplitude_internal(cw_gen_t * gen, const cw_tone_t * tone);
CW_STATIC_FUNC int    cw_gen_write_to_soundcard_internal(cw_gen_t * gen, cw_tone_t * tone, bool is_empty_tone);
CW_STATIC_FUNC cw_ret_t cw_gen_enqueue_valid_character_no_eoc_internal(cw_gen_t * gen, char character);
CW_STATIC_FUNC void   cw_gen_recalculate_slope_amplitudes_internal(cw_gen_t * gen);
CW_STATIC_FUNC cw_ret_t cw_gen_join_thread_internal(cw_gen_t * gen);
CW_STATIC_FUNC void   cw_gen_empty_tone_calculate_samples_size_internal(const cw_gen_t * gen, cw_tone_t * tone);
CW_STATIC_FUNC void   cw_gen_tone_calculate_samples_size_internal(const cw_gen_t * gen, cw_tone_t * tone);




#endif /* #ifndef _LIBCW_GEN_INTERNAL_H_ */
