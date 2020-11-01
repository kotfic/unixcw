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




#include "libcw2.h"




#include "libcw_debug.h"
#include "test_framework.h"




extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_dev;

/* Depending on which object file are we linked with, this will be a
   set of legacy API tests or set of all API/modern API/other stuff. */
extern cw_test_set_t cw_test_sets[];




static void cw_test_print_stats_wrapper(void);
static void signal_handler(int signal_number);
static void register_signal_handler(void);

static cw_test_executor_t g_tests_executor;




static void deinit_executor(void)
{
	cw_test_deinit(&g_tests_executor);
}




/**
   \return EXIT_SUCCESS if all tests complete successfully,
   \return EXIT_FAILURE otherwise
*/
int main(int argc, char * const argv[])
{
	fprintf(stderr, "%s\n\n", argv[0]);

	if (1) {
		/* This should be a default debug config for most of
		   the time (unless a specific feature is being
		   debugged): show all warnings an errors. */
		cw_debug_set_flags(&cw_debug_object, CW_DEBUG_MASK);
		cw_debug_object.level = CW_DEBUG_INFO;

		cw_debug_set_flags(&cw_debug_object_dev, CW_DEBUG_MASK);
		cw_debug_object_dev.level = CW_DEBUG_WARNING;
	}

	cw_test_executor_t * cte = &g_tests_executor;
	cw_test_init(cte, stdout, stderr, "libcw/tests");
	atexit(deinit_executor);

	cte->config->has_feature_sound_system = true;
	cte->config->has_feature_generator = true;
	cte->config->has_feature_libcw_test_specific = true;
	cte->config->has_feature_test_loops = true;
	cte->config->has_feature_test_name = true;
	cte->config->has_feature_test_quick_only = true;
	cte->config->has_feature_test_random_seed = true;
	cte->config->test_loops = 5;

	/* May cause exit on errors or "-h" option. */
	if (cwt_retv_ok != cte->process_args(cte, argc, argv)) {
		exit(EXIT_FAILURE);
	}

	cte->print_test_options(cte);
	/* Let the test options be clearly visible for few seconds
	   before screen is filled with testcase debugs. */
	sleep(3);


	atexit(cw_test_print_stats_wrapper);
	register_signal_handler();

	if (cwt_retv_ok != cte->main_test_loop(cte, cw_test_sets)) {
		cte->log_error(cte, "Test loop returned with error\n");
		exit(EXIT_FAILURE);
	}

	const unsigned int errors_count = cte->get_total_errors_count(cte);
	if (0 != errors_count) {
		cte->log_error(cte, "Non-zero errors count: %u\n", errors_count);
		exit(EXIT_FAILURE);
	}
	cte->log_info(cte, "Total errors count: %u\n", errors_count);

	/* "make check" facility requires this message to be
	   printed on stdout; don't localize it */
	cte->log_info(cte, "Test result: success\n\n");

	exit(EXIT_SUCCESS);
}




/* Show the signal caught, and exit. */
void signal_handler(int signal_number)
{
	g_tests_executor.log_info(&g_tests_executor, "Caught signal %d, exiting...\n", signal_number);
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
			g_tests_executor.log_error(&g_tests_executor, "Can't register signal %d: '%s'\n", SIGNALS[i], strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	return;
}




/**
   \brief Print statistics of tests

   Wrapper around call to method of global object.

   This function should be passed to atexit() to print stats before
   program exits (whether in normal way or during some error).
*/
void cw_test_print_stats_wrapper(void)
{
	g_tests_executor.print_test_stats(&g_tests_executor);
}
