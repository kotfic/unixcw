/*
 * Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
 * Copyright (C) 2011-2019  Kamil Ignacak (acerion@wp.pl)
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
   \file cw_common.c

   Code that is common for all _applications_ from unixcw package.
   Wrappers for some libcw functions, that probably don't belong to libcw.c.
*/




#include "config.h"

#include <stdio.h>  /* fprintf(stderr, ...) */
#include <stdlib.h> /* malloc() / free() */

#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif




#include "cw_common.h"
#include "libcw.h"
#include "libcw_gen.h"




static int cw_generator_apply_config(cw_config_t * config);




/**
   \brief Create new cw generator, apply given configuration

   Create new cw generator (using sound system from given \p config), and
   then apply rest of parameters from \p config to that generator.

   \p config should be first created with cw_config_new().

   \param config - configuration to be applied to generator

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_generator_new_from_config(cw_config_t *config)
{
	if (config->gen_conf.sound_system == CW_AUDIO_NULL) {
		if (cw_is_null_possible(config->gen_conf.sound_device)) {
			if (cw_generator_new(CW_AUDIO_NULL, config->gen_conf.sound_device)) {
				if (cw_generator_apply_config(config)) {
					return CW_SUCCESS;
				} else {
					fprintf(stderr, "%s: failed to apply configuration\n", config->program_name);
					return CW_FAILURE;
				}
			} else {
				fprintf(stderr, "%s: failed to open Null output\n", config->program_name);
			}
		} else {
			fprintf(stderr, "%s: Null output not available\n", config->program_name);
		}
		/* fall through to try with next sound system type */
	}


	if (config->gen_conf.sound_system == CW_AUDIO_NONE
	    || config->gen_conf.sound_system == CW_AUDIO_PA
	    || config->gen_conf.sound_system == CW_AUDIO_SOUNDCARD) {

		/* 'dev' may be NULL, sound system will use default device. */
		const char * dev = cw_gen_pick_device_name_internal(config->gen_conf.sound_device, CW_AUDIO_PA);

		if (cw_is_pa_possible(config->gen_conf.sound_device)) {
			if (cw_generator_new(CW_AUDIO_PA, config->gen_conf.sound_device)) {
				if (cw_generator_apply_config(config)) {
					return CW_SUCCESS;
				} else {
					fprintf(stderr, "%s: failed to apply configuration\n", config->program_name);
					return CW_FAILURE;
				}
			} else {
				fprintf(stderr, "%s: failed to open PulseAudio output with device '%s'\n",
					config->program_name,
					dev ? dev : CW_DEFAULT_PA_DEVICE);
			}
		} else {
			fprintf(stderr, "%s: PulseAudio output is not available with device '%s'\n",
				config->program_name,
				dev ? dev : CW_DEFAULT_PA_DEVICE);
		}
		/* fall through to try with next sound system type */
	}

	if (config->gen_conf.sound_system == CW_AUDIO_NONE
	    || config->gen_conf.sound_system == CW_AUDIO_OSS
	    || config->gen_conf.sound_system == CW_AUDIO_SOUNDCARD) {

		/* 'dev' may be NULL, sound system will use default device. */
		const char * dev = cw_gen_pick_device_name_internal(config->gen_conf.sound_device, CW_AUDIO_OSS);

		if (cw_is_oss_possible(config->gen_conf.sound_device)) {
			if (cw_generator_new(CW_AUDIO_OSS, config->gen_conf.sound_device)) {
				if (cw_generator_apply_config(config)) {
					return CW_SUCCESS;
				} else {
					fprintf(stderr, "%s: failed to apply configuration\n", config->program_name);
					return CW_FAILURE;
				}
			} else {
				fprintf(stderr,
					"%s: failed to open OSS output with device '%s'\n",
					config->program_name,
					dev ? dev : CW_DEFAULT_OSS_DEVICE);
			}
		} else {
			fprintf(stderr, "%s: OSS output is not available with device '%s'\n",
				config->program_name,
				dev ? dev : CW_DEFAULT_OSS_DEVICE);
		}
		/* fall through to try with next sound system type */
	}


	if (config->gen_conf.sound_system == CW_AUDIO_NONE
	    || config->gen_conf.sound_system == CW_AUDIO_ALSA
	    || config->gen_conf.sound_system == CW_AUDIO_SOUNDCARD) {

		/* 'dev' may be NULL, sound system will use default device. */
		const char * dev = cw_gen_pick_device_name_internal(config->gen_conf.sound_device, CW_AUDIO_ALSA);

		if (cw_is_alsa_possible(config->gen_conf.sound_device)) {
			if (cw_generator_new(CW_AUDIO_ALSA, config->gen_conf.sound_device)) {
				if (cw_generator_apply_config(config)) {
					return CW_SUCCESS;
				} else {
					fprintf(stderr, "%s: failed to apply configuration\n", config->program_name);
					return CW_FAILURE;
				}
			} else {
				fprintf(stderr,
					"%s: failed to open ALSA output with device '%s'\n",
					config->program_name,
					dev ? dev : CW_DEFAULT_ALSA_DEVICE);
			}
		} else {
			fprintf(stderr, "%s: ALSA output is not available with device '%s'\n",
				config->program_name,
				dev ? dev : CW_DEFAULT_ALSA_DEVICE);
		}
		/* fall through to try with next sound system type */
	}


	if (config->gen_conf.sound_system == CW_AUDIO_NONE
	    || config->gen_conf.sound_system == CW_AUDIO_CONSOLE) {

		/* 'dev' may be NULL, sound system will use default device. */
		const char * dev = cw_gen_pick_device_name_internal(config->gen_conf.sound_device, CW_AUDIO_CONSOLE);

		if (cw_is_console_possible(config->gen_conf.sound_device)) {
			if (cw_generator_new(CW_AUDIO_CONSOLE, config->gen_conf.sound_device)) {
				if (cw_generator_apply_config(config)) {
					return CW_SUCCESS;
				} else {
					fprintf(stderr, "%s: failed to apply configuration\n", config->program_name);
					return CW_FAILURE;
				}
			} else {
				fprintf(stderr,
					"%s: failed to open console output with device '%s'\n",
					config->program_name,
					dev ? dev : CW_DEFAULT_CONSOLE_DEVICE);
			}
		} else {
			fprintf(stderr, "%s: console output is not available with device '%s'\n",
				config->program_name,
				dev ? dev : CW_DEFAULT_CONSOLE_DEVICE);
		}
		/* fall through to try with next sound system type */
	}

	/* there is no next sound system type to try */
	return CW_FAILURE;
}





/**
   \brief Apply given configuration to a generator

   Function applies frequency, volume, sending speed, gap and weighting
   to current generator. The generator should exist (it should be created
   by cw_generator_new().

   The function is just a wrapper for few common function calls, to be used
   in cw_generator_new_from_config().

   \param config - current configuration

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_generator_apply_config(cw_config_t *config)
{
	if (!cw_set_frequency(config->frequency)) {
		return CW_FAILURE;
	}
	if (!cw_set_volume(config->volume)) {
		return CW_FAILURE;
	}
	if (!cw_set_send_speed(config->send_speed)) {
		return CW_FAILURE;
	}
	if (!cw_set_gap(config->gap)) {
		return CW_FAILURE;
	}
	if (!cw_set_weighting(config->weighting)) {
		return CW_FAILURE;
	}

	return CW_SUCCESS;
}





/**
   \brief Generate a tone that indicates a start
*/
void cw_start_beep(void)
{
	cw_flush_tone_queue();
	cw_queue_tone(20000, 500);
	cw_queue_tone(20000, 1000);
	cw_wait_for_tone_queue();
	return;
}





/**
   \brief Generate a tone that indicates an end
*/
void cw_end_beep(void)
{
      cw_flush_tone_queue();
      cw_queue_tone(20000, 500);
      cw_queue_tone(20000, 1000);
      cw_queue_tone(20000, 500);
      cw_queue_tone(20000, 1000);
      cw_wait_for_tone_queue();
      return;
}




/**
   \brief Get line from FILE

   Line of text is returned through \p buffer that should be allocated
   by caller. Total buffer size (including space for ending NUL) is
   given by \p buffer_size.

   The function adds ending NULL.

   Function strips newline character from the line read from file. The
   newline character is not put into \p buffer.

   \return true on successful reading of line
   \return false otherwise
*/
bool cw_getline(FILE *stream, char *buffer, int buffer_size)
{
	if (!feof(stream) && fgets(buffer, buffer_size, stream)) {

		size_t bytes = strlen(buffer);
		while (bytes > 0 && strchr("\r\n", buffer[bytes - 1])) {
			buffer[--bytes] = '\0';
		}

		return true;
	}

	return false;
}
