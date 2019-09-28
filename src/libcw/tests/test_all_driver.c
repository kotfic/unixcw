/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2019  Kamil Ignacak (acerion@wp.pl)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#define _XOPEN_SOURCE 600 /* signaction() + SA_RESTART */
#include "config.h"




#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>




#include "libcw_gen.h"
#include "libcw_debug.h"
#include "libcw2.h"

#include "libcw_test_framework.h"




#define MSG_PREFIX  "libcw modern API"




extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_dev;

extern cw_test_function_t cw_unit_tests_other_s[];
extern cw_test_function_t cw_unit_tests_tq[];
extern cw_test_function_t cw_unit_tests_gen[];
extern cw_test_function_t cw_unit_tests_key[];
extern cw_test_function_t cw_unit_tests_rec1[];




static cw_test_executor_t g_tests_executor;




static void cw_test_print_stats_wrapper(void);
static int cw_test_topics_with_current_sound_system(cw_test_executor_t * cte);
static void signal_handler(int signal_number);
static void register_signal_handler(void);




/**
   \return EXIT_SUCCESS if all tests complete successfully,
   \return EXIT_FAILURE otherwise
*/
int main(int argc, char *const argv[])
{
	fprintf(stderr, "%s\n\n", MSG_PREFIX);

	//cw_debug_set_flags(&cw_debug_object, CW_DEBUG_RECEIVE_STATES | CW_DEBUG_TONE_QUEUE | CW_DEBUG_GENERATOR | CW_DEBUG_KEYING);
	//cw_debug_object.level = CW_DEBUG_ERROR;

	//cw_debug_set_flags(&cw_debug_object_dev, CW_DEBUG_RECEIVE_STATES | CW_DEBUG_TONE_QUEUE | CW_DEBUG_GENERATOR | CW_DEBUG_KEYING);
	//cw_debug_object_dev.level = CW_DEBUG_DEBUG;


	cw_test_init(&g_tests_executor, stdout, stderr, MSG_PREFIX);

	if (!g_tests_executor.process_args(&g_tests_executor, argc, argv)) {
		cw_test_print_help(argv[0]);
		exit(EXIT_FAILURE);
	}


	atexit(cw_test_print_stats_wrapper);
	register_signal_handler();

	int rv = cw_test_topics_with_sound_systems(&g_tests_executor, cw_test_topics_with_current_sound_system);

	/* "make check" facility requires this message to be
	   printed on stdout; don't localize it */
	g_tests_executor.log_info(&g_tests_executor, "Test result: success\n\n");

	return rv == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}




/* Show the signal caught, and exit. */
void signal_handler(int signal_number)
{
	fprintf(stderr, "\n%s: caught signal %d, exiting...\n", MSG_PREFIX, signal_number);
	exit(EXIT_SUCCESS);
}




void register_signal_handler(void)
{
	/* Set up signal handler to exit on a range of signals. */
	const int SIGNALS[] = { SIGHUP, SIGINT, SIGQUIT, SIGPIPE, SIGTERM, 0 };
	for (int i = 0; SIGNALS[i]; i++) {

		struct sigaction action;
		memset(&action, 0, sizeof(action));
		action.sa_handler = signal_handler;
		action.sa_flags = 0;
		int rv = sigaction(SIGNALS[i], &action, (struct sigaction *) NULL);
		if (rv == -1) {
			g_tests_executor.log_err(&g_tests_executor, "Can't register signal %d: '%s'\n", SIGNALS[i], strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	return;
}




/**
   \brief Run tests for given audio system.

   Perform a series of self-tests on library public interfaces, using
   audio system specified with \p audio_system.  Tests should be
   performed on topics specified with \p topics.

   \param audio_system - audio system to use for tests
   \param topics - libcw test topics to be tested

   \return 0
*/
int cw_test_topics_with_current_sound_system(cw_test_executor_t * cte)
{
	cte->log_info(cte, "Testing with %s sound system\n", cte->get_current_sound_system_label(cte));



	if (cte->should_test_topic(cte, "t")) {
		int i = 0;
		while (cw_unit_tests_tq[i]) {
			cte->stats = &cte->stats2[cte->current_sound_system][LIBCW_TEST_TOPIC_TQ];
			(*cw_unit_tests_tq[i])(cte);
			i++;
		}
		cte->log_info_cont(cte, "\n");
	}

	if (cte->should_test_topic(cte, "g")) {
		int i = 0;
		while (cw_unit_tests_gen[i]) {
			cte->stats = &cte->stats2[cte->current_sound_system][LIBCW_TEST_TOPIC_GEN];
			(*cw_unit_tests_gen[i])(cte);
			i++;
		}
		cte->log_info_cont(cte, "\n");
	}

	if (cte->should_test_topic(cte, "k")) {
		int i = 0;
		while (cw_unit_tests_key[i]) {
			cte->stats = &cte->stats2[cte->current_sound_system][LIBCW_TEST_TOPIC_KEY];
	                (*cw_unit_tests_key[i])(cte);
			i++;
		}
		cte->log_info_cont(cte, "\n");
	}

	if (cte->should_test_topic(cte, "r")) {
		int i = 0;
		while (cw_unit_tests_rec1[i]) {
			cte->stats = &cte->stats2[cte->current_sound_system][LIBCW_TEST_TOPIC_REC];
	                (*cw_unit_tests_rec1[i])(cte);
			i++;
		}
		cte->log_info_cont(cte, "\n");
	}

	if (cte->should_test_topic(cte, "o")) {
		int i = 0;
		while (cw_unit_tests_other_s[i]) {
			cte->stats = &cte->stats2[cte->current_sound_system][LIBCW_TEST_TOPIC_OTHER];
	                (*cw_unit_tests_other_s[i])(cte);
			i++;
		}
		cte->log_info_cont(cte, "\n");
	}




	/* All tests done; return success if no failures,
	   otherwise return an error status code. */
	return 0;
}




/**
   \brief Run a series of tests for specified audio systems and topics

   Function attempts to run a set of testcases for every audio system
   specified in \p audio_systems and for every topic specified in \p topics.

   These testcases require some kind of audio system configured. The
   function calls cw_test_topics_with_current_sound_system() to do the configuration and
   run the tests.

   \p audio_systems is a list of audio systems to be tested: "ncoap".
   Pass NULL pointer to attempt to test all of audio systems supported
   by libcw.

   \param topics is a list of libcw test topics to be tested.

   \param audio_systems - list of audio systems to be tested
   \param topics - list of topics systems to be tested
*/
void cw_test_print_stats_wrapper(void)
{
	g_tests_executor.print_test_stats(&g_tests_executor);
}
