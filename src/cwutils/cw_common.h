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


#ifndef H_CW_COMMON
#define H_CW_COMMON

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




typedef struct {
	char *program_name;
	int audio_system;
	char *audio_device;
	int send_speed;
	int frequency;
	int volume;
	int gap;
	int weighting;
	int practice_time;
	char *input_file;
	char *output_file;

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
	int do_echo;           /* Echo characters */
	int do_errors;         /* Print error messages to stderr */
	int do_commands;       /* Execute embedded commands */
	int do_combinations;   /* Execute [...] combinations */
	int do_comments;       /* Allow {...} as comments */


	/* These fields are used in libcw tests only. */
	enum cw_audio_systems tested_sound_systems[CW_SOUND_SYSTEM_LAST + 1]; /* List of distinct sound systems, indexed from zero. End of values is marked by CW_AUDIO_NONE guard. */
	int tested_areas[LIBCW_TEST_TOPIC_MAX + 1];
	char test_function_name[128];  /* Execute only a test function with this name. */
	int test_repetitions; /* How many times a single test function should be repeated? */
} cw_config_t;



extern void cw_print_help(cw_config_t *config);

extern cw_config_t *cw_config_new(const char *program_name);
extern void         cw_config_delete(cw_config_t **config);
extern int          cw_config_is_valid(cw_config_t *config);

extern int cw_generator_new_from_config(cw_config_t *config);

extern void cw_start_beep(void);
extern void cw_end_beep(void);
extern bool cw_getline(FILE *stream, char *buffer, int buffer_size);


#if defined(__cplusplus)
}
#endif


#endif /* H_CW_COMMON */
