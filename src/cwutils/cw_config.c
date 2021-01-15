/*
 * Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
 * Copyright (C) 2011-2021  Kamil Ignacak (acerion@wp.pl)
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
   \file cw_config.c

   Functions handling cw_config_t structure.
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
#include "cw_config.h"




cw_config_t * cw_config_new(const char * program_name)
{
	cw_config_t *config = (cw_config_t *) calloc(1, sizeof (cw_config_t));
	if (NULL == config) {
		fprintf(stderr, "%s: can't allocate memory for configuration\n", program_name);
		return NULL;
	}

	config->program_name = strdup(program_name);
	if (!(config->program_name)) {
		fprintf(stderr, "%s: can't allocate memory for program name\n", program_name);

		free(config);
		config = NULL;

		return NULL;
	}

	config->gen_conf.sound_system = CW_AUDIO_NONE;
	config->send_speed = CW_SPEED_INITIAL;
	config->frequency = CW_FREQUENCY_INITIAL;
	config->volume = CW_VOLUME_INITIAL;
	config->gap = CW_GAP_INITIAL;
	config->weighting = CW_WEIGHTING_INITIAL;
	config->practice_time = CW_PRACTICE_TIME_INITIAL;
	config->input_file = NULL;
	config->output_file = NULL;

	config->do_echo = true;
	config->do_errors = true;
	config->do_commands = true;
	config->do_combinations = true;
	config->do_comments = true;

	return config;
}





void cw_config_delete(cw_config_t ** config)
{
	if (*config) {
		if ((*config)->program_name) {
			free((*config)->program_name);
			(*config)->program_name = NULL;
		}
		if ((*config)->input_file) {
			free((*config)->input_file);
			(*config)->input_file = NULL;
		}
		if ((*config)->output_file) {
			free((*config)->output_file);
			(*config)->output_file = NULL;
		}
		free(*config);
		*config = NULL;
	}

	return;
}





int cw_config_is_valid(cw_config_t * config)
{
	/* Deal with odd argument combinations. */
	if ('\0' != config->gen_conf.sound_device[0]) {
		if (config->gen_conf.sound_system == CW_AUDIO_SOUNDCARD) {
			fprintf(stderr, "libcw: a device has been specified for 'soundcard' sound system\n");
			fprintf(stderr, "libcw: a device can be specified only for 'console', 'oss', 'alsa' or 'pulseaudio'\n");
			return false;
		} else if (config->gen_conf.sound_system == CW_AUDIO_NULL) {
			fprintf(stderr, "libcw: a device has been specified for 'null' sound system\n");
			fprintf(stderr, "libcw: a device can be specified only for 'console', 'oss', 'alsa' or 'pulseaudio'\n");
			return false;
		} else {
			; /* sound_system is one that accepts custom "sound device" */
		}
	} else {
		; /* no custom "sound device" specified, a default will be used */
	}

	return true;
}
