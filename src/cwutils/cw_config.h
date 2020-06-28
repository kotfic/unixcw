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


#ifndef H_CW_CONFIG
#define H_CW_CONFIG




#if defined(__cplusplus)
extern "C" {
#endif




#include <stdbool.h>
#include <stdio.h>




#include <libcw.h>




#define CW_PRACTICE_TIME_MIN        1
#define CW_PRACTICE_TIME_MAX       99
#define CW_PRACTICE_TIME_INITIAL   15
#define CW_PRACTICE_TIME_STEP       1




/* TODO: this has to be moved back to test framework. */
#define LIBCW_TEST_ALL_TOPICS           "tgkrdo"   /* generator, tone queue, key, receiver, data, other. */
#define LIBCW_TEST_ALL_SOUND_SYSTEMS    "ncoap"   /* Null, console, OSS, ALSA, PulseAudio. */
enum {
	/* Explicitly stated values in this enum shall never
	   change. */
	LIBCW_TEST_TOPIC_TQ      = 0,
	LIBCW_TEST_TOPIC_GEN     = 1,
	LIBCW_TEST_TOPIC_KEY     = 2,
	LIBCW_TEST_TOPIC_REC     = 3,
	LIBCW_TEST_TOPIC_DATA    = 4,
	LIBCW_TEST_TOPIC_OTHER,
	LIBCW_TEST_TOPIC_MAX
};




typedef enum cw_audio_systems cw_sound_system; /* Someday it will go into libcw.h. */




typedef struct cw_config_t {
	char * program_name;
	cw_sound_system sound_system;
	char * sound_device;
	int send_speed;
	int frequency;
	int volume;
	int gap;
	int weighting;
	int practice_time;
	char * input_file;
	char * output_file;

	bool has_feature_sound_system;  /* Does the program have sound system output, for which we can specify sound system type and device? */
	bool has_feature_speed;         /* Generator speed. */
	bool has_feature_tone;          /* Sound tone (frequency). */
	bool has_feature_volume;        /* Sound volume. */
	bool has_feature_gap;           /* */
	bool has_feature_weighting;     /* */
	bool has_feature_practice_time; /* For cwcp/xcwcp program: allows specifying how long a training session will take. */
	bool has_feature_infile;        /* Allows specifying some input data for a program from input file. */
	bool has_feature_outfile;       /* */

	bool has_feature_cw_specific;   /* Does the program have features specific to cw program (i.e. is this program the cw program)? */
	bool has_feature_ui_colors;     /* Can we control color theme of UI (cwcp-specific). */

	bool has_feature_test_repetitions; /* Does the test program allow specifying count of repetitions of each test function? */
	bool has_feature_test_name;        /* Does the test program allow specifying single one test function to be executed? */
	bool has_feature_libcw_test_specific;

	/*
	 * Program-specific state variables, settable from the command line, or from
	 * embedded input stream commands.  These options may be set by the embedded
	 * command parser to values other than strictly TRUE or FALSE; all non-zero
	 * values are equivalent to TRUE.
	 *
	 * These fields are used only in cw.
	 */
	bool do_echo;           /* Echo characters */
	bool do_errors;         /* Print error messages to stderr */
	bool do_commands;       /* Execute embedded commands */
	bool do_combinations;   /* Execute [...] combinations */
	bool do_comments;       /* Allow {...} as comments */


	/* These fields are used in libcw tests only. */
	cw_sound_system tested_sound_systems[CW_SOUND_SYSTEM_LAST + 1]; /* List of distinct sound systems, indexed from zero. End of values is marked by CW_AUDIO_NONE guard. */
	int tested_areas[LIBCW_TEST_TOPIC_MAX + 1];
	char test_function_name[128];  /* Execute only a test function with this name. */
	int test_repetitions; /* How many times a single test function should be repeated? */

	/* Names of specific sound devices that should be used for tests. If
	   a test should be executed for a group of sound systems, we may
	   want to specify which exactly sound device to use for each of
	   these sound systems. So we need per-sound-system options/fields. */
	char test_alsa_device_name[CW_SOUND_DEVICE_NAME_SIZE];
} cw_config_t;




/**
   Create new configuration with default values

   Function returns pointer to config variable, with fields
   of config initialized to valid default values.

   \param program_name - human-readable name of application calling the function

   \return pointer to config on success
   \return NULL on failure
*/
extern cw_config_t * cw_config_new(const char * program_name);




/**
   \brief Delete configuration variable

   Deallocate given configuration, assign NULL to \p config

   \param config - configuration variable to deallocate
*/
extern void cw_config_delete(cw_config_t ** config);




/**
   \brief Validate configuration

   Check consistency and correctness of configuration.

   Currently the function only checks if "sound device" command line
   argument has been specified at the same time when "soundcard"
   has been specified as sound system. This is an inconsistency as
   you can specify sound device only for specific sound system ("soundcard"
   is just a general sound system).

   \param config - configuration to validate

   \return true if configuration is valid
   \return false if configuration is invalid
*/
extern int cw_config_is_valid(cw_config_t * config);




#if defined(__cplusplus)
}
#endif




#endif /* H_CW_CONFIG */
