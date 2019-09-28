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


#include "config.h"




#define _XOPEN_SOURCE 600 /* signaction() + SA_RESTART */


#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>



#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif

#include "libcw.h"
#include "libcw_test_framework.h"
#include "libcw_debug.h"
#include "libcw_tq.h"
#include "libcw_utils.h"
#include "libcw_gen.h"




#define MSG_PREFIX "libcw/legacy"



extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;



extern void (*const libcw_test_set_tq_with_audio[])(cw_test_executor_t *);
extern void (*const libcw_test_set_gen_with_audio[])(cw_test_executor_t *);
extern void (*const libcw_test_set_key_with_audio[])(cw_test_executor_t *);
extern void (*const libcw_test_set_other_with_audio[])(cw_test_executor_t *);




static void cw_test_setup(void);
//static int  cw_test_topics_with_sound_systems(cw_test_executor_t * cte);
static int  cw_test_topics_with_current_sound_system(cw_test_executor_t * cte);
static void cw_test_print_stats_wrapper(void);
static void cw_test_print_stats(cw_test_executor_t * cte);




/* This variable will be used in "forever" test. This test function
   needs to open generator itself, so it needs to know the current
   audio system to be used. _NONE is just an initial value, to be
   changed in test setup. */
int test_audio_system = CW_AUDIO_NONE;



static cw_test_executor_t g_tests_executor;


/**
   \brief Set up common test conditions

   Run before each individual test, to handle setup of common test conditions.
*/
void cw_test_setup(void)
{
	cw_reset_send_receive_parameters();
	cw_set_send_speed(30);
	cw_set_receive_speed(30);
	cw_disable_adaptive_receive();
	cw_reset_receive_statistics();
	cw_unregister_signal_handler(SIGUSR1);
	errno = 0;

	return;
}



/**
   \brief Run tests for given audio system.

   Perform a series of self-tests on library public interfaces, using
   audio system specified with \p audio_system. Range of tests is specified
   with \p testset.

   \param audio_system - audio system to use for tests
   \param stats - test statistics

   \return -1 on failure to set up tests
   \return 0 if tests were run, and no errors occurred
   \return 1 if tests were run, and some errors occurred
*/
int cw_test_topics_with_current_sound_system(cw_test_executor_t * cte)
{
	test_audio_system = cte->current_sound_system;

	cte->log_info(cte, "Testing with %s sound system\n", cte->get_current_sound_system_label(cte));

	int rv = cw_generator_new(cte->current_sound_system, NULL);
	if (rv != 1) {
		cte->log_err(cte, "Can't create generator, stopping the test\n");
		return -1;
	}
	rv = cw_generator_start();
	if (rv != 1) {
		cte->log_err(cte, "Can't start generator, stopping the test\n");
		cw_generator_delete();
		return -1;
	}

	if (cte->should_test_topic(cte, "t")) {
		for (int test = 0; libcw_test_set_tq_with_audio[test]; test++) {
			cw_test_setup();
			(*libcw_test_set_tq_with_audio[test])(cte);
		}
	}


	if (cte->should_test_topic(cte, "g")) {
		for (int test = 0; libcw_test_set_gen_with_audio[test]; test++) {
			cw_test_setup();
			(*libcw_test_set_gen_with_audio[test])(cte);
		}
	}


	if (cte->should_test_topic(cte, "k")) {
		for (int test = 0; libcw_test_set_key_with_audio[test]; test++) {
			cw_test_setup();
			(*libcw_test_set_key_with_audio[test])(cte);
		}
	}


	if (cte->should_test_topic(cte, "o")) {
		for (int test = 0; libcw_test_set_other_with_audio[test]; test++) {
			cw_test_setup();
			(*libcw_test_set_other_with_audio[test])(cte);
		}
	}


	sleep(1);
	cw_generator_stop();
	sleep(1);
	cw_generator_delete();

	/* All tests done; return success if no failures,
	   otherwise return an error status code. */
	return cte->stats->failures ? 1 : 0;
}









void cw_test_print_stats_wrapper(void)
{
	cw_test_print_stats(&g_tests_executor);
}




void cw_test_print_stats(cw_test_executor_t * cte)
{
	cte->log_info_cont(cte, "\n\n");
	cte->log_info(cte, "Statistics of tests:\n");

	cte->log_info(cte, "Tests not requiring any audio system:            ");
	if (cte->stats_indep.failures + cte->stats_indep.successes) {
		cte->log_info_cont(cte, "errors: %03d, total: %03d\n",
					cte->stats_indep.failures, cte->stats_indep.failures + cte->stats_indep.successes);
	} else {
		cte->log_info_cont(cte, "no tests were performed\n");
	}

	cte->log_info(cte, "Tests performed with NULL audio system:          ");
	if (cte->stats_null.failures + cte->stats_null.successes) {
		cte->log_info_cont(cte, "errors: %03d, total: %03d\n",
					cte->stats_null.failures, cte->stats_null.failures + cte->stats_null.successes);
	} else {
		cte->log_info_cont(cte, "no tests were performed\n");
	}

	cte->log_info(cte, "Tests performed with console audio system:       ");
	if (cte->stats_console.failures + cte->stats_console.successes) {
		cte->log_info_cont(cte, "errors: %03d, total: %03d\n",
					cte->stats_console.failures, cte->stats_console.failures + cte->stats_console.successes);
	} else {
		cte->log_info_cont(cte, "no tests were performed\n");
	}

	cte->log_info(cte, "Tests performed with OSS audio system:           ");
	if (cte->stats_oss.failures + cte->stats_oss.successes) {
		cte->log_info_cont(cte, "errors: %03d, total: %03d\n",
					cte->stats_oss.failures, cte->stats_oss.failures + cte->stats_oss.successes);
	} else {
		cte->log_info_cont(cte, "no tests were performed\n");
	}

	cte->log_info(cte, "Tests performed with ALSA audio system:          ");
	if (cte->stats_alsa.failures + cte->stats_alsa.successes) {
		cte->log_info_cont(cte, "errors: %03d, total: %03d\n",
		       cte->stats_alsa.failures, cte->stats_alsa.failures + cte->stats_alsa.successes);
	} else {
		cte->log_info_cont(cte, "no tests were performed\n");
	}

	cte->log_info(cte, "Tests performed with PulseAudio audio system:    ");
	if (cte->stats_pa.failures + cte->stats_pa.successes) {
		cte->log_info_cont(cte, "errors: %03d, total: %03d\n",
		       cte->stats_pa.failures, cte->stats_pa.failures + cte->stats_pa.successes);
	} else {
		cte->log_info_cont(cte, "no executor were performed\n");
	}

	return;
}





/**
   \return EXIT_SUCCESS if all tests complete successfully,
   \return EXIT_FAILURE otherwise
*/
int main(int argc, char *const argv[])
{
	int rv = 0;

	static const int SIGNALS[] = { SIGHUP, SIGINT, SIGQUIT, SIGPIPE, SIGTERM, 0 };

	//cw_debug_set_flags(&cw_debug_object_dev, CW_DEBUG_RECEIVE_STATES | CW_DEBUG_TONE_QUEUE | CW_DEBUG_GENERATOR | CW_DEBUG_KEYING);
	//cw_debug_object_dev.level = CW_DEBUG_DEBUG;

	cw_test_init(&g_tests_executor, stdout, stderr, MSG_PREFIX);

	if (!g_tests_executor.process_args(&g_tests_executor, argc, argv)) {
		cw_test_print_help(argv[0]);
		exit(EXIT_FAILURE);
	}

	atexit(cw_test_print_stats_wrapper);

	/* Arrange for the test to exit on a range of signals. */
	for (int i = 0; SIGNALS[i] != 0; i++) {
		if (!cw_register_signal_handler(SIGNALS[i], SIG_DFL)) {
			g_tests_executor.log_err(&g_tests_executor, "Failed to register signal handler for signal %d\n", SIGNALS[i]);
			exit(EXIT_FAILURE);
		}
	}

	rv = cw_test_topics_with_sound_systems(&g_tests_executor, cw_test_topics_with_current_sound_system);

	return rv == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
