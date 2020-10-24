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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif

#include "libcw.h"
#include "cw_cmdline.h"
#include "i18n.h"
#include "cw_copyright.h"


static int cw_process_option(int opt, const char *optarg, cw_config_t *config);
static void cw_print_usage(const char *program_name);

/**
   Append option string @param option_string to given @param buffer

   Options separator string (",") is added if necessary.

   @param size - total size of @param buffer
   @param n_chars - current count of characters (excluding terminating NUL) in the @param buffer

   @return true
*/
static bool append_option(char * buffer, size_t size, int * n_chars, const char * option_string);
/**
   Fill given @param buffer with all command line switches that are
   enabled in given @param config.

   @return @param buffer
*/
static char * cw_config_get_supported_feature_cmdline_options(const cw_config_t * config, char * buffer, size_t size);



/*---------------------------------------------------------------------*/
/*  Command line helpers                                               */
/*---------------------------------------------------------------------*/





/**
   \brief Return the program's base name from the given argv0

   Function returns pointer to substring in argv[0], so I guess that the
   pointer is owned by environment (?).
   Since there is always some non-NULL argv[0], the function always returns
   non-NULL pointer.

   \param argv0 - first argument to the program, argv[0]

   \return program's name
*/
const char *cw_program_basename(const char *argv0)
{
	const char *base = strrchr(argv0, '/');
	return base ? base + 1 : argv0;
}





/**
   \brief Combine command line options and environment options

   Build a new argc and argv by combining command line and environment
   options.

   The new values are held in the heap, and the malloc'ed addresses
   are not retained, so do not call this function repeatedly,
   otherwise it will leak memory.

   Combined values are returned through \p new_argc and \p new_argv.

   \param env_variable - name of environment variable to read from
   \param argc - argc of program's main()
   \param argv - argv[] of program's main()
   \param new_argc - combined argc
   \param new_argv[] - combined argv
*/
int combine_arguments(const char *env_variable,
		      int argc, char *const argv[],
		      /* out */ int *new_argc, /* out */ char **new_argv[])
{
	/* Begin with argv[0], which stays in place. */
	char **local_argv = malloc(sizeof (*local_argv));
	if (NULL == local_argv) {
		fprintf(stderr, "malloc() failure\n"); /* TODO: better error handling. */
		return CW_FAILURE;
	}
	int local_argc = 0;
	local_argv[local_argc++] = argv[0];

	/* If options are given in an environment variable, add these next. */
	char *env_options = getenv(env_variable);
	if (env_options) {
		char *options = strdup(env_options);
		if (NULL == options) {
			fprintf(stderr, "strdup() failure\n"); /* TODO: better error handling. */
			return CW_FAILURE;
		}

		for (char *option = strtok(options, " \t");
		     option;
		     option = strtok(NULL, " \t")) {

			local_argv = realloc(local_argv, sizeof (*local_argv) * (local_argc + 1));
			if (NULL == local_argv) {
				fprintf(stderr, "realloc() error\n"); /* TODO: better error handling. */
				return CW_FAILURE;
			}
			local_argv[local_argc++] = option;
		}
	}

	/* Append the options given on the command line itself. */
	for (int arg = 1; arg < argc; arg++) {
		local_argv = realloc(local_argv,
				     sizeof (*local_argv) * (local_argc + 1));
		if (NULL == local_argv) {
			fprintf(stderr, "realloc() error\n"); /* TODO: better error handling. */
			return CW_FAILURE;
		}
		local_argv[local_argc++] = argv[arg];
	}

	/* Return the constructed argc/argv. */
	*new_argc = local_argc;
	*new_argv = local_argv;

	return CW_SUCCESS;
}





/*---------------------------------------------------------------------*/
/*  Option handling helpers                                            */
/*---------------------------------------------------------------------*/




/**
   \brief Check if target system supports long options

   \return true the system supports long options,
   \return false otherwise
*/
bool has_longopts(void)
{
#if defined(HAVE_GETOPT_LONG)
	return true;
#else
	return false;
#endif
}



bool cw_longopts_available(void)
{
#if defined(HAVE_GETOPT_LONG)
	return true;
#else
	return false;
#endif
}





/**
   \brief Adapter wrapper round getopt() and getopt_long()

   Descriptor strings are comma-separated groups of elements of the
   form "c[:]|longopt", giving the short form option ('c'), ':' if it
   requires an argument, and the long form option.

   \param argc
   \param argv
   \param descriptor
   \param option
   \param argument

   \return true if there are still options in argv to be drawn
   \return false if argv is exhausted
*/
int get_option(int argc, char *const argv[],
	       const char *descriptor,
	       int *option, char **argument)
{
	static char *option_string = NULL;          /* Standard getopt() string */
#if defined(HAVE_GETOPT_LONG)
	static struct option *long_options = NULL;  /* getopt_long() structure */
	static char **long_names = NULL;            /* Allocated names array */
	static int long_count = 0;                  /* Entries in long_options */
#endif

	int opt;

	/* If this is the first call, build a new option_string and a
	   matching set of long options.  */
	if (!option_string) {
		/* Begin with an empty short options string. */
		option_string = strdup("");
		if (NULL == option_string) {
			fprintf(stderr, "strdup() failure\n"); /* TODO: better error handling. */
			return false;
		}

		/* Break the descriptor into comma-separated elements. */
		char *options = strdup(descriptor);
		if (NULL == options) {
			fprintf(stderr, "strdup() failure\n"); /* TODO: better error handling. */
			return false;
		}
		for (char *element = strtok(options, ",");
		     element;
		     element = strtok(NULL, ",")) {

			/* Determine if this option requires an argument. */
			int needs_arg = element[1] == ':';

			/* Append the short option character, and ':'
			   if present, to the short options string.
			   For simplicity in reallocating, assume that
			   the ':' is always there. */
			option_string = realloc(option_string, strlen(option_string) + 3);
			if (NULL == option_string) {
				fprintf(stderr, "realloc() error\n"); /* TODO: better error handling. */
				return false;
			}
			strncat(option_string, element, needs_arg ? 2 : 1);

#if defined(HAVE_GETOPT_LONG)
			/* Take a copy of the long name and add it to
			   a retained array.  Because struct option
			   makes name a const char*, we can't just
			   store it in there and then free later. */
			long_names = realloc(long_names,
						  sizeof (*long_names) * (long_count + 1));
			if (NULL == long_names) {
				fprintf(stderr, "realloc() error\n"); /* TODO: better error handling. */
				return false;
			}
			long_names[long_count] = strdup(element + (needs_arg ? 3 : 2));
			if (NULL == long_names[long_count]) {
				fprintf(stderr, "strdup() failure\n"); /* TODO: better error handling. */
				return false;
			}

			/* Add a new entry to the long options array. */
			long_options = realloc(long_options,
					       sizeof (*long_options) * (long_count + 2));
			if (NULL == long_options) {
				fprintf(stderr, "realloc() error\n"); /* TODO: better error handling. */
				return false;
			}
			long_options[long_count].name = long_names[long_count];
			long_options[long_count].has_arg = needs_arg;
			long_options[long_count].flag = NULL;
			long_options[long_count].val = element[0];
			long_count++;

			/* Set the end sentry to all zeroes. */
			memset(long_options + long_count, 0, sizeof (*long_options));
#endif
		}

		free(options);
	}

	/* Call the appropriate getopt function to get the first/next
	   option. */
#if defined(HAVE_GETOPT_LONG)
	opt = getopt_long(argc, argv, option_string, long_options, NULL);
#else
	opt = getopt(argc, argv, option_string);
#endif

	/* If no more options, clean up allocated memory before
	   returning. */
	if (opt == -1) {
#if defined(HAVE_GETOPT_LONG)

		/* Free each long option string created above, using
		   the long_names growable array because the
		   long_options[i].name aliased to it is a const
		   char*.  Then free long_names itself, and reset
		   pointer. */
		for (int i = 0; i < long_count; i++) {
			free(long_names[i]);
			long_names[i] = NULL;
		}

		free(long_names);
		long_names = NULL;

		/* Free the long options structure, and reset pointer
		   and counter. */
		free(long_options);
		long_options = NULL;
		long_count = 0;
#endif
		/* Free and reset the retained short options string. */
		free(option_string);
		option_string = NULL;
	}

	/* Return the option and argument, with false if no more
	   arguments. */
	*option = opt;
	*argument = optarg;
	return !(opt == -1);
}




/**
   Return the value of getopt()'s optind after get_options() calls complete.
*/
int get_optind(void)
{
	return optind;
}




void cw_print_help(cw_config_t *config)
{
	fprintf(stderr, _("Usage: %s [options...]\n\n"), config->program_name);

	if (!cw_longopts_available()) {
		fprintf(stderr, "%s", _("Long format of options is not supported on your system\n\n"));
	}

	if (config->has_feature_sound_system) {
		if (config->has_feature_libcw_test_specific) {
			fprintf(stderr, "%s", _("Sound system options (unstable):\n"));
			fprintf(stderr, "%s", _("  -S, --test-systems=SYSTEMS\n"));
			fprintf(stderr, "%s", _("        test one or more of these sound systems:\n"));
			fprintf(stderr, "%s", _("        n - Null\n"));
			fprintf(stderr, "%s", _("        c - console\n"));
			fprintf(stderr, "%s", _("        o - OSS\n"));
			fprintf(stderr, "%s", _("        a - ALSA\n"));
			fprintf(stderr, "%s", _("        p - PulseAudio\n"));
			fprintf(stderr, "%s", _("  If this option is not specified, the program will attempt to test all sound systems\n\n"));
		} else {
			fprintf(stderr, "%s", _("Sound system options:\n"));
			fprintf(stderr, "%s", _("  -s, --system=SYSTEM\n"));
			fprintf(stderr, "%s", _("        generate sound using SYSTEM sound system\n"));
			fprintf(stderr, "%s", _("        SYSTEM: {null|console|oss|alsa|pulseaudio|soundcard}\n"));
			fprintf(stderr, "%s", _("        'null': don't use any sound output\n"));
			fprintf(stderr, "%s", _("        'console': use system console/buzzer\n"));
			fprintf(stderr, "%s", _("               this output may require root privileges\n"));
			fprintf(stderr, "%s", _("        'oss': use OSS output\n"));
			fprintf(stderr, "%s", _("        'alsa' use ALSA output\n"));
			fprintf(stderr, "%s", _("        'pulseaudio' use PulseAudio output\n"));
			fprintf(stderr, "%s", _("        'soundcard': use either PulseAudio, OSS or ALSA\n"));
			fprintf(stderr, "%s", _("        default sound system: 'pulseaudio'->'oss'->'alsa'\n"));
		}
		fprintf(stderr, "%s", _("  -d, --device=DEVICE\n"));
		fprintf(stderr, "%s", _("        use DEVICE as output device instead of default one;\n"));
		fprintf(stderr, "%s", _("        optional for {console|oss|alsa|pulseaudio};\n"));
		fprintf(stderr, "%s", _("        default devices are:\n"));
		fprintf(stderr,       _("        'console': \"%s\"\n"), CW_DEFAULT_CONSOLE_DEVICE);
		fprintf(stderr,       _("        'oss': \"%s\"\n"), CW_DEFAULT_OSS_DEVICE);
		fprintf(stderr,       _("        'alsa': \"%s\"\n"), CW_DEFAULT_ALSA_DEVICE);
		fprintf(stderr,       _("        'pulseaudio': %s\n"), CW_DEFAULT_PA_DEVICE);

		if (config->has_feature_libcw_test_specific) {
			fprintf(stderr, "%s", _("  -X, --test-alsa-device=device\n"));
		}

		fprintf(stderr, "\n");
	}

	if (config->has_feature_generator) {
		fprintf(stderr, "%s", _("Generator options:\n"));
		fprintf(stderr, "%s", _("  -w, --wpm=WPM          set initial words per minute\n"));
		fprintf(stderr,       _("                         valid values: %d - %d\n"), CW_SPEED_MIN, CW_SPEED_MAX);
		fprintf(stderr,       _("                         default value: %d\n"), CW_SPEED_INITIAL);
		fprintf(stderr, "%s", _("  -t, --tone=HZ          set initial tone to HZ\n"));
		fprintf(stderr,       _("                         valid values: %d - %d\n"), CW_FREQUENCY_MIN, CW_FREQUENCY_MAX);
		fprintf(stderr,       _("                         default value: %d\n"), CW_FREQUENCY_INITIAL);
		fprintf(stderr, "%s", _("  -v, --volume=PERCENT   set initial volume to PERCENT\n"));
		fprintf(stderr,       _("                         valid values: %d - %d\n"), CW_VOLUME_MIN, CW_VOLUME_MAX);
		fprintf(stderr,       _("                         default value: %d\n"), CW_VOLUME_INITIAL);
		fprintf(stderr, "\n");
	}

	if (config->has_feature_dot_dash_params) {
		fprintf(stderr, "%s", _("Dot/dash options:\n"));
		fprintf(stderr, "%s", _("  -g, --gap=GAP          set extra gap between letters\n"));
		fprintf(stderr,       _("                         valid values: %d - %d\n"), CW_GAP_MIN, CW_GAP_MAX);
		fprintf(stderr,       _("                         default value: %d\n"), CW_GAP_INITIAL);
		fprintf(stderr, "%s", _("  -k, --weighting=WEIGHT set weighting to WEIGHT\n"));
		fprintf(stderr,       _("                         valid values: %d - %d\n"), CW_WEIGHTING_MIN, CW_WEIGHTING_MAX);
		fprintf(stderr,       _("                         default value: %d\n"), CW_WEIGHTING_INITIAL);
		fprintf(stderr, "\n");
	}

	if (config->has_feature_cw_specific
	    || config->has_feature_practice_time
	    || config->has_feature_infile
	    || config->has_feature_outfile
	    || config->has_feature_cw_specific) {

		fprintf(stderr, "%s", _("Other options:\n"));

		if (config->has_feature_cw_specific) {
			fprintf(stderr, "%s", _("  -e, --noecho           disable sending echo to stdout\n"));
			fprintf(stderr, "%s", _("  -m, --nomessages       disable writing messages to stderr\n"));
			fprintf(stderr, "%s", _("  -c, --nocommands       disable executing embedded commands\n"));
			fprintf(stderr, "%s", _("  -o, --nocombinations   disallow [...] combinations\n"));
			fprintf(stderr, "%s", _("  -p, --nocomments       disallow {...} comments\n"));
		}
		if (config->has_feature_practice_time) {
			fprintf(stderr, "%s", _("  -T, --time=TIME        set initial practice time (in minutes)\n"));
			fprintf(stderr,       _("                         valid values: %d - %d\n"), CW_PRACTICE_TIME_MIN, CW_PRACTICE_TIME_MAX);
			fprintf(stderr,       _("                         default value: %d\n"), CW_PRACTICE_TIME_INITIAL);
		}
		if (config->has_feature_infile) {
			fprintf(stderr, "%s", _("  -f, --infile=FILE      read practice words from FILE\n"));
		}
		if (config->has_feature_outfile) {
			fprintf(stderr, "%s", _("  -F, --outfile=FILE     write current practice words to FILE\n"));
		}
		/* TODO: this probably should be inside of "if (config->has_feature_infile)". */
		if (config->has_feature_cw_specific) {
			fprintf(stderr, "%s", _("                         default file: stdin\n"));
		}
		fprintf(stderr, "\n");
	}

	if (config->has_feature_libcw_test_specific
	    || config->has_feature_test_loops
	    || config->has_feature_test_name
	    || config->has_feature_test_quick_only
	    || config->has_feature_test_random_seed) {

		fprintf(stderr, "%s", _("Options specific to test programs (unstable):\n"));

		if (config->has_feature_libcw_test_specific) {
			fprintf(stderr, "%s", _("  -A, --test-areas=AREAS\n"));
			fprintf(stderr, "%s", _("        test one or more of these areas:\n"));
			fprintf(stderr, "%s", _("        g - generator\n"));
			fprintf(stderr, "%s", _("        t - tone queue\n"));
			fprintf(stderr, "%s", _("        k - Morse key\n"));
			fprintf(stderr, "%s", _("        r - receiver\n"));
			fprintf(stderr, "%s", _("        o - other\n"));
			fprintf(stderr, "%s", _("  If this option is not specified, the program will attempt to test all test areas\n\n"));
		}
		if (config->has_feature_test_loops) {
			fprintf(stderr, "%s", _("  -L, --test-loops=N\n"));
			fprintf(stderr, "%s", _("        execute testes functions N times in a loop\n"));
			fprintf(stderr, "%s", _("        test functions usually have some small default value\n"));
		}
		if (config->has_feature_test_name) {
			fprintf(stderr, "%s", _("  -N, --test-name=NAME\n"));
			fprintf(stderr, "%s", _("        execute only a test function specified by NAME\n"));
			fprintf(stderr, "%s", _("        this option overrides -A option\n"));
		}
		if (config->has_feature_test_quick_only) {
			fprintf(stderr, "%s", _("  -Q, --test-quick-only\n"));
			fprintf(stderr, "%s", _("        execute only test functions that take short time\n"));
		}
		if (config->has_feature_test_random_seed) {
			fprintf(stderr, "%s", _("  -D, --test-random-seed\n"));
			fprintf(stderr, "%s", _("        use given seed for randomization\n"));
		}

		fprintf(stderr, "\n");
	}

	fprintf(stderr, "%s", _("Help and version information:\n"));
	fprintf(stderr, "%s", _("  -h, --help             print this message\n"));
	fprintf(stderr, "%s", _("  -V, --version          print version information\n\n"));

	return;
}




bool append_option(char * buffer, size_t size, int * n_chars, const char * option_string)
{
	if ((*n_chars) > 0) {
		/* Add options separator. */
		(*n_chars) += snprintf(buffer + (*n_chars), size - (*n_chars), "%s", ",");
	}
	(*n_chars) += snprintf(buffer + (*n_chars), size - (*n_chars), "%s", option_string);

	return true;
}




char * cw_config_get_supported_feature_cmdline_options(const cw_config_t * config, char * buffer, size_t size)
{
	int n = 0;

	if (config->has_feature_sound_system) {
		append_option(buffer, size, &n, "s:|system,d:|device");
	}
	if (config->has_feature_generator) {
		append_option(buffer, size, &n, "w:|wpm");
		append_option(buffer, size, &n, "t:|tone");
		append_option(buffer, size, &n, "v:|volume");
	}
	if (config->has_feature_dot_dash_params) {
		append_option(buffer, size, &n, "g:|gap");
		append_option(buffer, size, &n, "k:|weighting");
	}
	if (config->has_feature_practice_time) {
		append_option(buffer, size, &n, "T:|time");
	}
	if (config->has_feature_infile) {
		append_option(buffer, size, &n, "f:|infile");
	}
	if (config->has_feature_outfile) {
		append_option(buffer, size, &n, "F:|outfile");
	}

	if (config->has_feature_cw_specific) {
		append_option(buffer, size, &n, "e|noecho,m|nomessages,c|nocommands,o|nocombinations,p|nocomments");
	}
	if (config->has_feature_ui_colors) {
		append_option(buffer, size, &n, "c:|colours,c:|colors,m|mono");
	}

	if (config->has_feature_libcw_test_specific) {
		append_option(buffer, size, &n, "S:|test-systems");
		append_option(buffer, size, &n, "A:|test-areas");
		append_option(buffer, size, &n, "X:|test-alsa-device");
	}
	if (config->has_feature_test_loops) {
		append_option(buffer, size, &n, "L:|test-loops");
	}
	if (config->has_feature_test_name) {
		append_option(buffer, size, &n, "N:|test-name");
	}
	if (config->has_feature_test_quick_only) {
		append_option(buffer, size, &n, "Q|test-quick-only");
	}
	if (config->has_feature_test_random_seed) {
		append_option(buffer, size, &n, "D:|test-random-seed");
	}

	if (true) {
		append_option(buffer, size, &n, "h|help,V|version");
	}

	fprintf(stderr, "Command line options for supported features: '%s'\n", buffer);

	return buffer;
}




cw_ret_t cw_process_program_arguments(int argc, char *const argv[], cw_config_t *config)
{
	int option = 0;
	char * argument = NULL;

	/* All options that can be present in command line. I will be
	   snprintf()-ing to the buffer, so I specify N times the expected
	   size, just to be safe. */
	char all_cmdline_options[5 * sizeof ("s:|system,d:|device,w:|wpm,t:|tone,v:|volume,g:|gap,k:|weighting,f:|infile,F:|outfile,e|noecho,m|nomessages,c|nocommands,o|nocombinations,p|nocomments,h|help,V|version")];
	cw_config_get_supported_feature_cmdline_options(config, all_cmdline_options, sizeof (all_cmdline_options));

	while (get_option(argc, argv, all_cmdline_options, &option, &argument)) {
		if (!cw_process_option(option, argument, config)) {
			return CW_FAILURE;
		}
	}

	if (optind != argc) {
		fprintf(stderr, "%s: expected argument after options\n", config->program_name);
		cw_print_usage(config->program_name);
		return CW_FAILURE;
	} else {
		return CW_SUCCESS;
	}
}




int cw_process_option(int opt, const char *optarg, cw_config_t *config)
{
	size_t optarg_len = 0;
	int dest_idx = 0;

	switch (opt) {
	case 's':
		if (!strcmp(optarg, "null")
		    || !strcmp(optarg, "n")) {

			config->gen_conf.sound_system = CW_AUDIO_NULL;
		} else if (!strcmp(optarg, "alsa")
		    || !strcmp(optarg, "a")) {

			config->gen_conf.sound_system = CW_AUDIO_ALSA;
		} else if (!strcmp(optarg, "oss")
			   || !strcmp(optarg, "o")) {

			config->gen_conf.sound_system = CW_AUDIO_OSS;
		} else if (!strcmp(optarg, "pulseaudio")
			   || !strcmp(optarg, "p")) {

			config->gen_conf.sound_system = CW_AUDIO_PA;
		} else if (!strcmp(optarg, "console")
			   || !strcmp(optarg, "c")) {

			config->gen_conf.sound_system = CW_AUDIO_CONSOLE;

		} else if (!strcmp(optarg, "soundcard")
			   || !strcmp(optarg, "s")) {

			config->gen_conf.sound_system = CW_AUDIO_SOUNDCARD;
		} else {
			fprintf(stderr, "%s: invalid sound system (option 's'): %s\n", config->program_name, optarg);
			return CW_FAILURE;
		}
		break;

	case 'd':
		// fprintf(stderr, "%s: d:%s\n", config->program_name, optarg);
		if (optarg && strlen(optarg)) {
			const size_t len_max = sizeof (config->gen_conf.sound_device) - 1;
			if (strlen(optarg) >= len_max) {
				fprintf(stderr, "%s: device name can't be longer than %zd characters\n", config->program_name, len_max);
				return CW_FAILURE;
			}
			snprintf(config->gen_conf.sound_device, sizeof (config->gen_conf.sound_device), "%s", optarg);
		} else {
			fprintf(stderr, "%s: no device specified for option -d\n", config->program_name);
			return CW_FAILURE;
		}
		break;

	case 'w':
		{
			// fprintf(stderr, "%s: w:%s\n", config->program_name, optarg);
			int speed = atoi(optarg);
			if (speed < CW_SPEED_MIN || speed > CW_SPEED_MAX) {
				fprintf(stderr, "%s: speed out of range: %d\n", config->program_name, speed);
				return CW_FAILURE;
			} else {
				config->send_speed = speed;
			}
			break;
		}

	case 't':
		{
			// fprintf(stderr, "%s: t:%s\n", config->program_name, optarg);
			int frequency = atoi(optarg);
			if (frequency < CW_FREQUENCY_MIN || frequency > CW_FREQUENCY_MAX) {
				fprintf(stderr, "%s: frequency out of range: %d\n", config->program_name, frequency);
				return CW_FAILURE;
			} else {
				config->frequency = frequency;
			}
			break;
		}

	case 'v':
		{
			// fprintf(stderr, "%s: v:%s\n", config->program_name, optarg);
			int volume = atoi(optarg);
			if (volume < CW_VOLUME_MIN || volume > CW_VOLUME_MAX) {
				fprintf(stderr, "%s: volume level out of range: %d\n", config->program_name, volume);
				return CW_FAILURE;
			} else {
				config->volume = volume;
			}
			break;
		}

	case 'g':
		{
			// fprintf(stderr, "%s: g:%s\n", config->program_name, optarg);
			int gap = atoi(optarg);
			if (gap < CW_GAP_MIN || gap > CW_GAP_MAX) {
				fprintf(stderr, "%s: gap out of range: %d\n", config->program_name, gap);
				return CW_FAILURE;
			} else {
				config->gap = gap;
			}
			break;
		}

	case 'k':
		{
			// fprintf(stderr, "%s: k:%s\n", config->program_name, optarg);
			int weighting = atoi(optarg);
			if (weighting < CW_WEIGHTING_MIN || weighting > CW_WEIGHTING_MAX) {
				fprintf(stderr, "%s: weighting out of range: %d\n", config->program_name, weighting);
				return CW_FAILURE;
			} else {
				config->weighting = weighting;
			}
			break;
		}

	case 'T':
		{
			// fprintf(stderr, "%s: T:%s\n", config->program_name, optarg);
			int time = atoi(optarg);
			if (time < 0) {
				fprintf(stderr, "%s: practice time is negative\n", config->program_name);
				return CW_FAILURE;
			} else {
				config->practice_time = time;
			}
			break;
		}

	case 'f':
		if (optarg && strlen(optarg)) {
			config->input_file = strdup(optarg);
		} else {
			fprintf(stderr, "%s: no input file specified for option -f\n", config->program_name);
			return CW_FAILURE;
		}
		/* TODO: access() */
		break;

	case 'F':
		if (optarg && strlen(optarg)) {
			config->output_file = strdup(optarg);
		} else {
			fprintf(stderr, "%s: no output file specified for option -F\n", config->program_name);
			return CW_FAILURE;
		}
		/* TODO: access() */
		break;

        case 'e':
		config->do_echo = false;
		break;

        case 'm':
		config->do_errors = false;
		break;

        case 'c':
		config->do_commands = false;
		break;

        case 'o':
		config->do_combinations = false;
		break;

        case 'p':
		config->do_comments = false;
		break;

	case 'h':
	case '?':
		cw_print_help(config);
		exit(EXIT_SUCCESS);
	case 'V':
		fprintf(stderr, _("%s version %s\n"), config->program_name, PACKAGE_VERSION);
		fprintf(stderr, "%s\n", CW_COPYRIGHT);
		exit(EXIT_SUCCESS);



	case 'S':
		optarg_len = strlen(optarg);
		if (optarg_len > strlen(LIBCW_TEST_ALL_SOUND_SYSTEMS)) {
			fprintf(stderr, "Too many values for 'sound system' option: '%s'\n", optarg);
			goto help_and_error;
		}

		dest_idx = 0;
		for (size_t i = 0; i < optarg_len; i++) {
			const int val = optarg[i];
			if (NULL == strchr(LIBCW_TEST_ALL_SOUND_SYSTEMS, val)) {
				fprintf(stderr, "Unsupported sound system '%c'\n", val);
				goto help_and_error;
			}

			/* If user has explicitly requested a sound system,
			   then we have to fail if the system is not available.
			   Otherwise we may mislead the user. */
			switch (val) {
			case 'n':
				if (cw_is_null_possible(NULL)) {
					config->tested_sound_systems[dest_idx] = CW_AUDIO_NULL;
					dest_idx++;
				} else {
					fprintf(stderr, "Requested null sound system is not available on this machine\n");
					goto help_and_error;
				}
				break;
			case 'c':
				if (cw_is_console_possible(NULL)) {
					config->tested_sound_systems[dest_idx] = CW_AUDIO_CONSOLE;
					dest_idx++;
				} else {
					fprintf(stderr, "Requested console sound system is not available on this machine\n");
					goto help_and_error;

				}
				break;
			case 'o':
				if (cw_is_oss_possible(NULL)) {
					config->tested_sound_systems[dest_idx] = CW_AUDIO_OSS;
					dest_idx++;
				} else {
					fprintf(stderr, "Requested OSS sound system is not available on this machine\n");
					goto help_and_error;

				}
				break;
			case 'a':
				if (cw_is_alsa_possible(NULL)) {
					config->tested_sound_systems[dest_idx] = CW_AUDIO_ALSA;
					dest_idx++;
				} else {
					fprintf(stderr, "Requested ALSA sound system is not available on this machine\n");
					goto help_and_error;

				}
				break;
			case 'p':
				if (cw_is_pa_possible(NULL)) {
					config->tested_sound_systems[dest_idx] = CW_AUDIO_PA;
					dest_idx++;
				} else {
					fprintf(stderr, "Requested PulseAudio sound system is not available on this machine\n");
					goto help_and_error;

				}
				break;
			default:
				fprintf(stderr, "Unsupported sound system '%c'\n", val);
				goto help_and_error;
			}
		}
		config->tested_sound_systems[dest_idx] = CW_AUDIO_NONE; /* Guard element. */
		break;

	case 'A':
		optarg_len = strlen(optarg);
		if (optarg_len > strlen(LIBCW_TEST_ALL_TOPICS)) {
			fprintf(stderr, "Too many values for 'areas' option: '%s'\n", optarg);
			return -1;
		}

		dest_idx = 0;
		for (size_t i = 0; i < optarg_len; i++) {
			const int val = optarg[i];
			if (NULL == strchr(LIBCW_TEST_ALL_TOPICS, val)) {
				fprintf(stderr, "Unsupported test area '%c'\n", val);
				goto help_and_error;
			}
			switch (val) {
			case 't':
				config->tested_areas[dest_idx] = LIBCW_TEST_TOPIC_TQ;
				break;
			case 'g':
				config->tested_areas[dest_idx] = LIBCW_TEST_TOPIC_GEN;
				break;
			case 'k':
				config->tested_areas[dest_idx] = LIBCW_TEST_TOPIC_KEY;
				break;
			case 'r':
				config->tested_areas[dest_idx] = LIBCW_TEST_TOPIC_REC;
				break;
			case 'd':
				config->tested_areas[dest_idx] = LIBCW_TEST_TOPIC_DATA;
				break;
			case 'o':
				config->tested_areas[dest_idx] = LIBCW_TEST_TOPIC_OTHER;
				break;
			default:
				fprintf(stderr, "Unsupported test area: '%c'\n", val);
				goto help_and_error;
			}
			dest_idx++;
		}
		config->tested_areas[dest_idx] = LIBCW_TEST_TOPIC_MAX; /* Guard element. */
		break;

	case 'N':
		snprintf(config->test_function_name, sizeof (config->test_function_name), "%s", optarg);
		break;

	case 'L':
		config->test_loops = atoi(optarg);
		break;

	case 'Q':
		config->test_quick_only = true;
		break;

	case 'X':
		snprintf(config->test_alsa_device_name, sizeof (config->test_alsa_device_name), "%s", optarg);
		break;

	case 'D':
		config->test_random_seed = atol(optarg);
		break;

	default: /* '?' */
		cw_print_usage(config->program_name);
		return CW_FAILURE;
	}

	return CW_SUCCESS;

 help_and_error:
	cw_print_usage(config->program_name);
	return CW_FAILURE;
}




void cw_print_usage(const char *program_name)
{
	const char *format = cw_longopts_available()
		? _("Try '%s --help' for more information.\n")
		: _("Try '%s -h' for more information.\n");

	fprintf(stderr, format, program_name);
	return;
}
