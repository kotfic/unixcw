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




/**
   \file test_framework.c

   \brief Test framework for libcw test code
*/




#include "config.h"




#include <getopt.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>

#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif




#include "libcw.h"
#include "libcw_debug.h"
#include "cw_cmdline.h"

#include "test_framework.h"
#include "test_framework_tools.h"




/* Make pause between tests.

   Let the resources measurement tool go back to zero, so that
   e.g. high CPU usage in test N is visible only in that test, but not
   in test N+1 that will be executed right after test N. */
#define LIBCW_TEST_INTER_TEST_PAUSE_MSECS (2 * LIBCW_TEST_MEAS_CPU_MEAS_INTERVAL_MSECS)




static bool cw_test_expect_op_int(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
static bool cw_test_expect_op_int_errors_only(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
static bool cw_test_expect_op_int_sub(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, bool errors_only, const char * va_buf);

static bool cw_test_expect_op_double(struct cw_test_executor_t * self, double expected_value, const char * operator, double received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
static bool cw_test_expect_op_double_errors_only(struct cw_test_executor_t * self, double expected_value, const char * operator, double received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
static bool cw_test_expect_op_double_sub(struct cw_test_executor_t * self, double expected_value, const char * operator, double received_value, bool errors_only, const char * va_buf);

static bool cw_test_expect_between_int(struct cw_test_executor_t * self, int expected_lower, int received_value, int expected_higher, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
static bool cw_test_expect_between_int_errors_only(struct cw_test_executor_t * self, int expected_lower, int received_value, int expected_higher, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));

static bool cw_test_expect_null_pointer(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));
static bool cw_test_expect_null_pointer_errors_only(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));

static bool cw_test_expect_valid_pointer(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));
static bool cw_test_expect_valid_pointer_errors_only(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));

static void cw_assert2(struct cw_test_executor_t * self, bool condition, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));


static void cw_test_print_test_header(cw_test_executor_t * self, const char * fmt, ...);
static void cw_test_print_test_footer(cw_test_executor_t * self, const char * test_name);
static void cw_test_append_status_string(cw_test_executor_t * self, char * msg_buf, int n, const char * status_string);

static int cw_test_process_args(cw_test_executor_t * self, int argc, char * const argv[]);
static int cw_test_get_repetitions_count(cw_test_executor_t * self);
static int cw_test_fill_default_sound_systems_and_topics(cw_test_executor_t * self);

static void cw_test_print_test_options(cw_test_executor_t * self);

static bool cw_test_test_topic_was_requested(cw_test_executor_t * self, int libcw_test_topic);
static bool cw_test_sound_system_was_requested(cw_test_executor_t * self, cw_sound_system sound_system);

static const char * cw_test_get_current_topic_label(cw_test_executor_t * self);
static const char * cw_test_get_current_sound_system_label(cw_test_executor_t * self);
static const char * cw_test_get_current_sound_device(cw_test_executor_t * self);

static void cw_test_set_current_topic_and_sound_system(cw_test_executor_t * self, int topic, int sound_system);

static void cw_test_print_test_stats(cw_test_executor_t * self);

static int cw_test_log_info(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));
static void cw_test_log_info_cont(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));
static void cw_test_flush_info(struct cw_test_executor_t * self);
static void cw_test_log_error(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));

static void cw_test_print_sound_systems(cw_test_executor_t * self, cw_sound_system * sound_systems, int max);
static void cw_test_print_topics(cw_test_executor_t * self, int * topics, int max);

static bool cw_test_test_topic_is_member(cw_test_executor_t * cte, int topic, int * topics, int max);
static bool cw_test_sound_system_is_member(cw_test_executor_t * cte, cw_sound_system sound_system, cw_sound_system * sound_systems, int max);

static int cw_test_main_test_loop(cw_test_executor_t * cte, cw_test_set_t * test_sets);
static unsigned int cw_test_get_total_errors_count(cw_test_executor_t * cte);




static cwt_retv iterate_over_topics(cw_test_executor_t * cte, cw_test_set_t * test_set);
static cwt_retv iterate_over_sound_systems(cw_test_executor_t * cte, cw_test_set_t * test_set, int topic);
static cwt_retv iterate_over_test_objects(cw_test_executor_t * cte, cw_test_object_t * test_objects, int topic, cw_sound_system sound_system);




/**
   \brief Set default contents of
   cw_test_executor_t::config::tested_sound_systems[] and
   cw_test_executor_t::config::tested_areas[]

   One or both sets of defaults will be used if related argument was
   not used in command line.

   When during preparation of default set of sound system we detect
   that some sound set is not available, we will not include it in set
   of default sound systems.

   This is a private function so it is not put into cw_test_executor_t
   class.
*/
int cw_test_fill_default_sound_systems_and_topics(cw_test_executor_t * self)
{
	/* NULL means "use default device" for every sound system */
	const char * default_device = NULL;

	int dest_idx = 0;
	if (cw_is_null_possible(default_device)) {
		self->config->tested_sound_systems[dest_idx] = CW_AUDIO_NULL;
		dest_idx++;
	} else {
		self->log_info(self, "Null sound system is not available on this machine - will skip it\n");
	}

	if (cw_is_console_possible(default_device)) {
		self->config->tested_sound_systems[dest_idx] = CW_AUDIO_CONSOLE;
		dest_idx++;
	} else {
		self->log_info(self, "Console sound system is not available on this machine - will skip it\n");
	}

	if (cw_is_oss_possible(default_device)) {
		self->config->tested_sound_systems[dest_idx] = CW_AUDIO_OSS;
		dest_idx++;
	} else {
		self->log_info(self, "OSS sound system is not available on this machine - will skip it\n");
	}

	if (cw_is_alsa_possible(default_device)) {
		self->config->tested_sound_systems[dest_idx] = CW_AUDIO_ALSA;
		dest_idx++;
	} else {
		self->log_info(self, "ALSA sound system is not available on this machine - will skip it\n");
	}

	if (cw_is_pa_possible(default_device)) {
		self->config->tested_sound_systems[dest_idx] = CW_AUDIO_PA;
		dest_idx++;
	} else {
		self->log_info(self, "PulseAudio sound system is not available on this machine - will skip it\n");
	}
	self->config->tested_sound_systems[dest_idx] = CW_AUDIO_NONE; /* Guard. */



	self->config->tested_areas[0] = LIBCW_TEST_TOPIC_TQ;
	self->config->tested_areas[1] = LIBCW_TEST_TOPIC_GEN;
	self->config->tested_areas[2] = LIBCW_TEST_TOPIC_KEY;
	self->config->tested_areas[3] = LIBCW_TEST_TOPIC_REC;
	self->config->tested_areas[4] = LIBCW_TEST_TOPIC_DATA;
	self->config->tested_areas[5] = LIBCW_TEST_TOPIC_OTHER;
	self->config->tested_areas[6] = LIBCW_TEST_TOPIC_MAX; /* Guard element. */

	return 0;
}




int cw_test_process_args(cw_test_executor_t * self, int argc, char * const argv[])
{
	cw_test_fill_default_sound_systems_and_topics(self);
	if (argc == 1) {
		/* Use defaults configured by
		   cw_test_fill_default_sound_systems_and_topics(). */
		return 0;
	}

	if (CW_SUCCESS != cw_process_program_arguments(argc, argv, self->config)) {
		exit(EXIT_FAILURE);
	}
	return 0;
}




static int cw_test_get_repetitions_count(cw_test_executor_t * self)
{
	return self->config->test_repetitions;
}




bool cw_test_expect_op_int(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, const char * fmt, ...)
{
	char va_buf[128] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	return cw_test_expect_op_int_sub(self, expected_value, operator, received_value, false, va_buf);
}




bool cw_test_expect_op_int_errors_only(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, const char * fmt, ...)
{
	char va_buf[128] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	return cw_test_expect_op_int_sub(self, expected_value, operator, received_value, true, va_buf);
}




static bool cw_test_expect_op_int_sub(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, bool errors_only, const char * va_buf)
{
	bool as_expected = false;

	char msg_buf[1024] = { 0 };
	int n = snprintf(msg_buf, sizeof (msg_buf), "%s", self->msg_prefix);
	const int message_len = n + snprintf(msg_buf + n, sizeof (msg_buf) - n, "%s", va_buf);
	n += snprintf(msg_buf + n, sizeof (msg_buf) - n, "%-*s", (self->console_n_cols - n), va_buf);


	bool success = false;
	if (operator[0] == '=' && operator[1] == '=') {
		success = expected_value == received_value;

	} else if (operator[0] == '<' && operator[1] == '=') {
		success = expected_value <= received_value;

	} else if (operator[0] == '>' && operator[1] == '=') {
		success = expected_value >= received_value;

	} else if (operator[0] == '!' && operator[1] == '=') {
		success = expected_value != received_value;

	} else if (operator[0] == '<' && operator[1] == '\0') {
		success = expected_value < received_value;

	} else if (operator[0] == '>' && operator[1] == '\0') {
		success = expected_value > received_value;

	} else {
		self->log_error(self, "Unhandled operator '%s'\n", operator);
		assert(0);
	}


	if (success) {
		if (!errors_only) {
			self->stats->successes++;

			cw_test_append_status_string(self, msg_buf, message_len, "[ OK ]");
			self->log_info(self, "%s\n", msg_buf);
		}
		as_expected = true;
	} else {
		self->stats->failures++;

		cw_test_append_status_string(self, msg_buf, message_len, "[FAIL]");
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected %d, got %d   ***\n", expected_value, received_value);

		as_expected = false;
	}

	return as_expected;
}




bool cw_test_expect_op_double(struct cw_test_executor_t * self, double expected_value, const char * operator, double received_value, const char * fmt, ...)
{
	char va_buf[128] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	return cw_test_expect_op_double_sub(self, expected_value, operator, received_value, false, va_buf);
}




bool cw_test_expect_op_double_errors_only(struct cw_test_executor_t * self, double expected_value, const char * operator, double received_value, const char * fmt, ...)
{
	char va_buf[128] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	return cw_test_expect_op_double_sub(self, expected_value, operator, received_value, true, va_buf);
}




static bool cw_test_expect_op_double_sub(struct cw_test_executor_t * self, double expected_value, const char * operator, double received_value, bool errors_only, const char * va_buf)
{
	char msg_buf[1024] = { 0 };
	int n = snprintf(msg_buf, sizeof (msg_buf), "%s", self->msg_prefix);
	const int message_len = n + snprintf(msg_buf + n, sizeof (msg_buf) - n, "%s", va_buf);
	n += snprintf(msg_buf + n, sizeof (msg_buf) - n, "%-*s", (self->console_n_cols - n), va_buf);


	bool success = false;
	if (operator[0] == '<' && operator[1] == '\0') {
		success = expected_value < received_value;

	} else if (operator[0] == '>' && operator[1] == '\0') {
		success = expected_value > received_value;

	} else {
		self->log_error(self, "Unhandled operator '%s'\n", operator);
		assert(0);
	}


	bool as_expected = false;
	if (success) {
		if (!errors_only) {
			self->stats->successes++;

			cw_test_append_status_string(self, msg_buf, message_len, "[ OK ]");
			self->log_info(self, "%s\n", msg_buf);
		}
		as_expected = true;
	} else {
		self->stats->failures++;

		cw_test_append_status_string(self, msg_buf, message_len, "[FAIL]");
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected %f, got %f   ***\n", expected_value, received_value);

		as_expected = false;
	}
	return as_expected;
}





/**
   @brief Append given status string at the end of buffer, but within cw_test::console_n_cols limit

   This is a private function so it is not put into cw_test_executor_t
   class.
*/
void cw_test_append_status_string(cw_test_executor_t * self, char * msg_buf, int n, const char * status_string)
{
	const char * separator = " "; /* Separator between test message and test status string, for better visibility of status string. */
	const size_t space_left = self->console_n_cols - n;

	if (space_left > strlen(separator) + strlen(status_string)) {
		sprintf(msg_buf + self->console_n_cols - strlen(separator) - strlen(status_string), "%s%s", separator, status_string);
	} else {
		sprintf(msg_buf + self->console_n_cols - strlen("...") - strlen(separator) - strlen(status_string), "...%s%s", separator, status_string);
	}
}




bool cw_test_expect_between_int(struct cw_test_executor_t * self, int expected_lower, int received_value, int expected_higher, const char * fmt, ...)
{
	bool as_expected = true;
	char va_buf[128] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	char msg_buf[1024] = { 0 };
	int n = snprintf(msg_buf, sizeof (msg_buf), "%s", self->msg_prefix);
	const int message_len = n + snprintf(msg_buf + n, sizeof (msg_buf) - n, "%s", va_buf);
	n += snprintf(msg_buf + n, sizeof (msg_buf) - n, "%-*s", (self->console_n_cols - n), va_buf);

	if (expected_lower <= received_value && received_value <= expected_higher) {
		self->stats->successes++;

		cw_test_append_status_string(self, msg_buf, message_len, "[ OK ]");
		//self->log_info(self, "%s\n", msg_buf);
		self->log_info(self, "%s %d %d %d\n", msg_buf, expected_lower, received_value, expected_higher);

		as_expected = true;
	} else {
		self->stats->failures++;

		cw_test_append_status_string(self, msg_buf, message_len, "[FAIL]");
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected within %d-%d, got %d   ***\n", expected_lower, expected_higher, received_value);

		as_expected = false;
	}

	return as_expected;
}




bool cw_test_expect_between_int_errors_only(struct cw_test_executor_t * self, int expected_lower, int received_value, int expected_higher, const char * fmt, ...)
{
	bool as_expected = true;
	char buf[128] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof (buf), fmt, ap);
	va_end(ap);

	if (expected_lower <= received_value && received_value <= expected_higher) {
		as_expected = true;
	} else {
		const int n = fprintf(self->stderr, "%s%s", self->msg_prefix, buf);
		self->stats->failures++;
		self->log_error(self, "%*s", self->console_n_cols - n, "failure: ");
		self->log_error(self, "expected value within %d-%d, got %d\n", expected_lower, expected_higher, received_value);
		as_expected = false;
	}

	return as_expected;
}




bool cw_test_expect_null_pointer(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...)
{
	bool as_expected = false;
	char va_buf[128] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	char msg_buf[1024] = { 0 };
	int n = snprintf(msg_buf, sizeof (msg_buf), "%s", self->msg_prefix);
	const int message_len = n + snprintf(msg_buf + n, sizeof (msg_buf) - n, "%s", va_buf);
	n += snprintf(msg_buf + n, sizeof (msg_buf) - n, "%-*s", (self->console_n_cols - n), va_buf);


	if (NULL == pointer) {
		self->stats->successes++;

		cw_test_append_status_string(self, msg_buf, message_len, "[ OK ]");
		self->log_info(self, "%s\n", msg_buf);

		as_expected = true;
	} else {
		self->stats->failures++;

		cw_test_append_status_string(self, msg_buf, message_len, "[FAIL]");
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected NULL, got %p   ***\n", pointer);

		as_expected = false;
	}


	return as_expected;
}




bool cw_test_expect_null_pointer_errors_only(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...)
{
	bool as_expected = false;
	char va_buf[128] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	char msg_buf[1024] = { 0 };
	int n = snprintf(msg_buf, sizeof (msg_buf), "%s", self->msg_prefix);
	const int message_len = n + snprintf(msg_buf + n, sizeof (msg_buf) - n, "%s", va_buf);
	n += snprintf(msg_buf + n, sizeof (msg_buf) - n, "%-*s", (self->console_n_cols - n), va_buf);


	if (NULL == pointer) {
		as_expected = true;
	} else {
		self->stats->failures++;

		cw_test_append_status_string(self, msg_buf, message_len, "[FAIL]");
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected NULL, got %p   ***\n", pointer);

		as_expected = false;
	}


	return as_expected;
}




bool cw_test_expect_valid_pointer(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...)
{
	bool as_expected = false;
	char va_buf[128] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	char msg_buf[1024] = { 0 };
	int n = snprintf(msg_buf, sizeof (msg_buf), "%s", self->msg_prefix);
	const int message_len = n + snprintf(msg_buf + n, sizeof (msg_buf) - n, "%s", va_buf);
	n += snprintf(msg_buf + n, sizeof (msg_buf) - n, "%-*s", (self->console_n_cols - n), va_buf);


	if (NULL != pointer) {
		self->stats->successes++;

		cw_test_append_status_string(self, msg_buf, message_len, "[ OK ]");
		self->log_info(self, "%s\n", msg_buf);

		as_expected = true;
	} else {
		self->stats->failures++;

		cw_test_append_status_string(self, msg_buf, message_len, "[FAIL]");
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected valid pointer, got NULL   ***\n");

		as_expected = false;
	}


	return as_expected;
}




bool cw_test_expect_valid_pointer_errors_only(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...)
{
	bool as_expected = false;
	char va_buf[128] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	char msg_buf[1024] = { 0 };
	int n = snprintf(msg_buf, sizeof (msg_buf), "%s", self->msg_prefix);
	const int message_len = n + snprintf(msg_buf + n, sizeof (msg_buf) - n, "%s", va_buf);
	n += snprintf(msg_buf + n, sizeof (msg_buf) - n, "%-*s", (self->console_n_cols - n), va_buf);


	if (NULL != pointer) {
		as_expected = true;
	} else {
		self->stats->failures++;

		cw_test_append_status_string(self, msg_buf, message_len, "[FAIL]");
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected valid pointer, got NULL   ***\n");

		as_expected = false;
	}


	return as_expected;
}




void cw_assert2(struct cw_test_executor_t * self, bool condition, const char * fmt, ...)
{
	if (!condition) {

		char va_buf[128] = { 0 };

		va_list ap;
		va_start(ap, fmt);
		vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
		va_end(ap);

		self->log_error(self, "Assertion failed: %s\n", va_buf);

		exit(EXIT_FAILURE);
	}

	return;
}




bool cw_test_test_topic_was_requested(cw_test_executor_t * self, int libcw_test_topic)
{
	const int n = sizeof (self->config->tested_areas) / sizeof (self->config->tested_areas[0]);

	switch (libcw_test_topic) {
	case LIBCW_TEST_TOPIC_TQ:
	case LIBCW_TEST_TOPIC_GEN:
	case LIBCW_TEST_TOPIC_KEY:
	case LIBCW_TEST_TOPIC_REC:
	case LIBCW_TEST_TOPIC_DATA:
	case LIBCW_TEST_TOPIC_OTHER:
		for (int i = 0; i < n; i++) {
			if (LIBCW_TEST_TOPIC_MAX == self->config->tested_areas[i]) {
				/* Found guard element. */
				return false;
			}
			if (libcw_test_topic == self->config->tested_areas[i]) {
				return true;
			}
		}
		return false;

	case LIBCW_TEST_TOPIC_MAX:
	default:
		fprintf(stderr, "Unexpected test topic %d\n", libcw_test_topic);
		exit(EXIT_FAILURE);
	}
}




bool cw_test_sound_system_was_requested(cw_test_executor_t * self, cw_sound_system sound_system)
{
	const int n = sizeof (self->config->tested_sound_systems) / sizeof (self->config->tested_sound_systems[0]);

	switch (sound_system) {
	case CW_AUDIO_NULL:
	case CW_AUDIO_CONSOLE:
	case CW_AUDIO_OSS:
	case CW_AUDIO_ALSA:
	case CW_AUDIO_PA:
		for (int i = 0; i < n; i++) {
			if (CW_AUDIO_NONE == self->config->tested_sound_systems[i]) {
				/* Found guard element. */
				return false;
			}
			if (sound_system == self->config->tested_sound_systems[i]) {
				return true;
			}
		}
		return false;

	case CW_AUDIO_NONE:
	case CW_AUDIO_SOUNDCARD:
	default:
		fprintf(stderr, "Unexpected sound system %d\n", sound_system);
		exit(EXIT_FAILURE);
	}
}




void cw_test_print_test_header(cw_test_executor_t * self, const char * fmt, ...)
{
	self->log_info_cont(self, "\n");

	self->log_info(self, "Beginning of test\n");

	{
		self->log_info(self, " ");
		for (size_t i = 0; i < self->console_n_cols - (strlen ("[II]  ")); i++) {
			self->log_info_cont(self, "-");
		}
		self->log_info_cont(self, "\n");
	}


	char va_buf[256] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	self->log_info(self, "Test name: %s\n", va_buf);
	self->log_info(self, "Current test topic: %s\n", self->get_current_topic_label(self));
	self->log_info(self, "Current sound system: %s\n", self->get_current_sound_system_label(self));
	self->log_info(self, "Current sound device: '%s'\n", self->get_current_sound_device(self));

	{
		self->log_info(self, " ");
		for (size_t i = 0; i < self->console_n_cols - (strlen ("[II]  ")); i++) {
			self->log_info_cont(self, "-");
		}
		self->log_info_cont(self, "\n");
	}
}




void cw_test_print_test_footer(cw_test_executor_t * self, const char * test_name)
{
	self->log_info(self, "End of test: %s\n", test_name);
}




const char * cw_test_get_current_sound_system_label(cw_test_executor_t * self)
{
	return cw_get_audio_system_label(self->current_sound_system);
}




const char * cw_test_get_current_sound_device(cw_test_executor_t * self)
{
	return self->current_sound_device;
}




const char * cw_test_get_current_topic_label(cw_test_executor_t * self)
{
	switch (self->current_topic) {
	case LIBCW_TEST_TOPIC_TQ:
		return "tq";
	case LIBCW_TEST_TOPIC_GEN:
		return "gen";
	case LIBCW_TEST_TOPIC_KEY:
		return "key";
	case LIBCW_TEST_TOPIC_REC:
		return "rec";
	case LIBCW_TEST_TOPIC_DATA:
		return "data";
	case LIBCW_TEST_TOPIC_OTHER:
		return "other";
	default:
		return "*** unknown ***";
	}
}




/**
   @brief Set a test topic and sound system that is about to be tested

   This is a private function so it is not put into cw_test_executor_t
   class.

   Call this function before calling each test function. Topic and
   sound system values to be passed to this function should be taken
   from the same test set that the test function is taken.
*/
void cw_test_set_current_topic_and_sound_system(cw_test_executor_t * self, int topic, int sound_system)
{
	self->current_topic = topic;
	self->current_sound_system = sound_system;

	self->current_sound_device[0] = '\0'; /* Clear value from previous run of test. */
	switch (self->current_sound_system) {
	case CW_AUDIO_ALSA:
		if ('\0' != self->config->test_alsa_device_name[0]) {
			snprintf(self->current_sound_device, sizeof (self->current_sound_device), "%s", self->config->test_alsa_device_name);
		}
		break;
	case CW_AUDIO_NULL:
	case CW_AUDIO_CONSOLE:
	case CW_AUDIO_OSS:
	case CW_AUDIO_PA:
		/* We don't have a buffer with device name for this sound system. */
		break;
	case CW_AUDIO_NONE:
	case CW_AUDIO_SOUNDCARD:
	default:
		/* Technically speaking this is an error, but we shouldn't
		   get here because test binary won't accept such sound
		   systems through command line. */
		break;
	}

	self->stats = &self->all_stats[sound_system][topic];
}




void cw_test_print_test_stats(cw_test_executor_t * self)
{
	const char sound_systems[] = " NCOAP";

	fprintf(self->stderr, "\n\nlibcw tests: Statistics of tests (failures/total)\n\n");

	//                           12345 123456789012 123456789012 123456789012 123456789012 123456789012 123456789012
	#define SEPARATOR_LINE      "   --+------------+------------+------------+------------+------------+------------+\n"
	#define FRONT_FORMAT        "%s %c |"
	#define BACK_FORMAT         "%s\n"
	#define CELL_FORMAT_D       "% 11d |"
	#define CELL_FORMAT_S       "%11s |"

	fprintf(self->stderr,       "     | tone queue | generator  |    key     |  receiver  |    data    |    other   |\n");
	fprintf(self->stderr,       "%s", SEPARATOR_LINE);

	for (int sound = CW_AUDIO_NULL; sound <= CW_AUDIO_PA; sound++) {

		/* If a row with error counter has non-zero values,
		   use arrows at the beginning and end of the row to
		   highlight/indicate row that has non-zero error
		   counters. We want the errors to be visible and
		   stand out. */
		char error_indicator_empty[3] = "  ";
		char error_indicator_front[3] = "  ";
		char error_indicator_back[3] = "  ";
		{
			bool has_errors = false;
			for (int topic = 0; topic < LIBCW_TEST_TOPIC_MAX; topic++) {
				if (self->all_stats[sound][topic].failures) {
					has_errors = true;
					break;
				}
			}

			if (has_errors) {
				snprintf(error_indicator_front, sizeof (error_indicator_front), "%s", "->");
				snprintf(error_indicator_back, sizeof (error_indicator_back), "%s", "<-");
			}
		}



		/* Print line with errors. Print numeric values only
		   if some tests for given combination of sound
		   system/topic were performed. */
		fprintf(self->stderr, FRONT_FORMAT, error_indicator_front, sound_systems[sound]);
		for (int topic = 0; topic < LIBCW_TEST_TOPIC_MAX; topic++) {
			int total = self->all_stats[sound][topic].failures + self->all_stats[sound][topic].successes;
			int failures = self->all_stats[sound][topic].failures;

			if (0 == total && 0 == failures) {
				fprintf(self->stderr, CELL_FORMAT_S, " ");
			} else {
				fprintf(self->stderr, CELL_FORMAT_D, failures);
			}
		}
		fprintf(self->stderr, BACK_FORMAT, error_indicator_back);



		/* Print line with totals. Print numeric values only
		   if some tests for given combination of sound
		   system/topic were performed. */
		fprintf(self->stderr, FRONT_FORMAT, error_indicator_empty, sound_systems[sound]);
		for (int topic = 0; topic < LIBCW_TEST_TOPIC_MAX; topic++) {
			int total = self->all_stats[sound][topic].failures + self->all_stats[sound][topic].successes;
			int failures = self->all_stats[sound][topic].failures;

			if (0 == total && 0 == failures) {
				fprintf(self->stderr, CELL_FORMAT_S, " ");
			} else {
				fprintf(self->stderr, CELL_FORMAT_D, total);
			}
		}
		fprintf(self->stderr, BACK_FORMAT, error_indicator_empty);



		fprintf(self->stderr,       "%s", SEPARATOR_LINE);
	}

	return;
}




void cw_test_init(cw_test_executor_t * self, FILE * stdout, FILE * stderr, const char * msg_prefix)
{
	memset(self, 0, sizeof (cw_test_executor_t));

	self->config = cw_config_new("libcw tests");

	self->stdout = stdout;
	self->stderr = stderr;

	self->use_resource_meas = false;

	self->expect_op_int = cw_test_expect_op_int;
	self->expect_op_int_errors_only = cw_test_expect_op_int_errors_only;
	self->expect_op_double = cw_test_expect_op_double;
	self->expect_op_double_errors_only = cw_test_expect_op_double_errors_only;

	self->expect_between_int = cw_test_expect_between_int;
	self->expect_between_int_errors_only = cw_test_expect_between_int_errors_only;

	self->expect_null_pointer = cw_test_expect_null_pointer;
	self->expect_null_pointer_errors_only = cw_test_expect_null_pointer_errors_only;

	self->expect_valid_pointer = cw_test_expect_valid_pointer;
	self->expect_valid_pointer_errors_only = cw_test_expect_valid_pointer_errors_only;

	self->assert2 = cw_assert2;

	self->print_test_header = cw_test_print_test_header;
	self->print_test_footer = cw_test_print_test_footer;

	self->process_args = cw_test_process_args;

	self->get_repetitions_count = cw_test_get_repetitions_count;

	self->print_test_options = cw_test_print_test_options;

	self->test_topic_was_requested = cw_test_test_topic_was_requested;
	self->sound_system_was_requested = cw_test_sound_system_was_requested;

	self->get_current_topic_label = cw_test_get_current_topic_label;
	self->get_current_sound_system_label = cw_test_get_current_sound_system_label;
	self->get_current_sound_device = cw_test_get_current_sound_device;

	self->print_test_stats = cw_test_print_test_stats;

	self->log_info = cw_test_log_info;
	self->log_info_cont = cw_test_log_info_cont;
	self->flush_info = cw_test_flush_info;
	self->log_error = cw_test_log_error;

	self->main_test_loop = cw_test_main_test_loop;
	self->get_total_errors_count = cw_test_get_total_errors_count;




	self->console_n_cols = default_cw_test_print_n_chars;

	self->current_sound_system = CW_AUDIO_NONE;
	self->current_sound_device[0] = '\0';

	snprintf(self->msg_prefix, sizeof (self->msg_prefix), "%s: ", msg_prefix);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	self->random_seed = tv.tv_sec;
	srand(self->random_seed);
}




void cw_test_deinit(cw_test_executor_t * self)
{
	cw_config_delete(&self->config);
}




int cw_test_log_info(struct cw_test_executor_t * self, const char * fmt, ...)
{
	if (NULL == self->stdout) {
		return 0;
	}

	char va_buf[256] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	const int n = fprintf(self->stdout, "[II] %s", va_buf);
	fflush(self->stdout);

	return n;
}




void cw_test_log_info_cont(struct cw_test_executor_t * self, const char * fmt, ...)
{
	if (NULL == self->stdout) {
		return;
	}

	char va_buf[256] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	fprintf(self->stdout, "%s", va_buf);
	fflush(self->stdout);

	return;
}




void cw_test_flush_info(struct cw_test_executor_t * self)
{
	if (NULL == self->stdout) {
		return;
	}
	fflush(self->stdout);
	return;
}




void cw_test_log_error(struct cw_test_executor_t * self, const char * fmt, ...)
{
	if (NULL == self->stdout) {
		return;
	}

	char va_buf[256] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	fprintf(self->stdout, "[EE] %s", va_buf);
	fflush(self->stdout);

	return;
}




/**
   @brief Print labels of sound systems specified by @param sound_systems array

   There are no more than @param max items in @param sound_systems
   vector. CW_AUDIO_NONE is considered a guard element. Function stops
   either after processing @param max elements, or at guard element
   (without printing label for the guard element) - whichever comes
   first.
*/
void cw_test_print_sound_systems(cw_test_executor_t * self, cw_sound_system * sound_systems, int max)
{
	for (int i = 0; i < max; i++) {
		if (CW_AUDIO_NONE == sound_systems[i]) {
			/* Found guard element. */
			return;
		}

		switch (sound_systems[i]) {
		case CW_AUDIO_NULL:
			self->log_info_cont(self, "null ");
			break;
		case CW_AUDIO_CONSOLE:
			self->log_info_cont(self, "console ");
			break;
		case CW_AUDIO_OSS:
			self->log_info_cont(self, "OSS ");
			break;
		case CW_AUDIO_ALSA:
			self->log_info_cont(self, "ALSA ");
			break;
		case CW_AUDIO_PA:
			self->log_info_cont(self, "PulseAudio ");
			break;
		default:
			self->log_info_cont(self, "unknown! ");
			break;
		}
	}

	return;
}




/**
   @brief Print labels of test topics specified by @param topics array

   There are no more than @param max items in @param topics
   vector. LIBCW_TEST_TOPIC_MAX is considered a guard
   element. Function stops either after processing @param max
   elements, or at guard element (without printing label for the guard
   element) - whichever comes first.
*/
void cw_test_print_topics(cw_test_executor_t * self, int * topics, int max)
{
	for (int i = 0; i < max; i++) {
		if (LIBCW_TEST_TOPIC_MAX == topics[i]) {
			/* Found guard element. */
			return;
		}

		switch (topics[i]) {
		case LIBCW_TEST_TOPIC_TQ:
			self->log_info_cont(self, "tq ");
			break;
		case LIBCW_TEST_TOPIC_GEN:
			self->log_info_cont(self, "gen ");
			break;
		case LIBCW_TEST_TOPIC_KEY:
			self->log_info_cont(self, "key ");
			break;
		case LIBCW_TEST_TOPIC_REC:
			self->log_info_cont(self, "rec ");
			break;
		case LIBCW_TEST_TOPIC_DATA:
			self->log_info_cont(self, "data ");
			break;
		case LIBCW_TEST_TOPIC_OTHER:
			self->log_info_cont(self, "other ");
			break;
		default:
			self->log_info_cont(self, "unknown! ");
			break;
		}
	}
	self->log_info_cont(self, "\n");

	return;
}




void cw_test_print_test_options(cw_test_executor_t * self)
{
	self->log_info(self, "Sound systems that will be tested: ");
	cw_test_print_sound_systems(self, self->config->tested_sound_systems, sizeof (self->config->tested_sound_systems) / sizeof (self->config->tested_sound_systems[0]));
	self->log_info_cont(self, "\n");

	self->log_info(self, "Areas that will be tested: ");
	cw_test_print_topics(self, self->config->tested_areas, sizeof (self->config->tested_areas) / sizeof (self->config->tested_areas[0]));
	self->log_info_cont(self, "\n");

	self->log_info(self, "Random seed = %lu\n", self->random_seed);

	if (strlen(self->config->test_function_name)) {
		self->log_info(self, "Single function to be tested: '%s'\n", self->config->test_function_name);
	}

	fflush(self->stdout);
}




/**
   @brief See if given @param topic is a member of given list of test topics @param topics

   The size of @param topics is specified by @param max.
*/
bool cw_test_test_topic_is_member(__attribute__((unused)) cw_test_executor_t * cte, int topic, int * topics, int max)
{
	for (int i = 0; i < max; i++) {
		if (LIBCW_TEST_TOPIC_MAX == topics[i]) {
			/* Found guard element. */
			return false;
		}
		if (topic == topics[i]) {
			return true;
		}
	}
	return false;
}




/**
   @brief See if given @param sound_system is a member of given list of test topics @param sound_system

   The size of @param sound_system is specified by @param max.
*/
bool cw_test_sound_system_is_member(__attribute__((unused)) cw_test_executor_t * cte, cw_sound_system sound_system, cw_sound_system * sound_systems, int max)
{
	for (int i = 0; i < max; i++) {
		if (CW_AUDIO_NONE == sound_systems[i]) {
			/* Found guard element. */
			return false;
		}
		if (sound_system == sound_systems[i]) {
			return true;
		}
	}
	return false;
}




int cw_test_main_test_loop(cw_test_executor_t * cte, cw_test_set_t * test_sets)
{
	int set = 0;
	while (LIBCW_TEST_SET_VALID == test_sets[set].set_valid) {
		cw_test_set_t * test_set = &test_sets[set];
		iterate_over_topics(cte, test_set);
		set++;
	}

	return 0;
}




static cwt_retv iterate_over_topics(cw_test_executor_t * cte, cw_test_set_t * test_set)
{
	for (int topic = LIBCW_TEST_TOPIC_TQ; topic < LIBCW_TEST_TOPIC_MAX; topic++) {
		if (!cte->test_topic_was_requested(cte, topic)) {
			continue;
		}
		const int topics_max = sizeof (test_set->tested_areas) / sizeof (test_set->tested_areas[0]);
		if (!cw_test_test_topic_is_member(cte, topic, test_set->tested_areas, topics_max)) {
			continue;
		}

		if (cwt_retv_ok != iterate_over_sound_systems(cte, test_set, topic)) {
			cte->log_error(cte, "Test framework failed for topic %d\n", topic);
			return cwt_retv_err;
		}
	}

	return cwt_retv_ok;
}




static cwt_retv iterate_over_sound_systems(cw_test_executor_t * cte, cw_test_set_t * test_set, int topic)
{
	for (cw_sound_system sound_system = CW_SOUND_SYSTEM_FIRST; sound_system <= CW_SOUND_SYSTEM_LAST; sound_system++) {
		if (!cte->sound_system_was_requested(cte, sound_system)) {
			continue;
		}
		const int systems_max = sizeof (test_set->tested_sound_systems) / sizeof (test_set->tested_sound_systems[0]);
		if (!cw_test_sound_system_is_member(cte, sound_system, test_set->tested_sound_systems, systems_max)) {
			continue;
		}

		if (cwt_retv_ok != iterate_over_test_objects(cte, test_set->test_objects, topic, sound_system)) {
			cte->log_error(cte, "Test framework failed for topic %d, sound system %d\n", topic, sound_system);
			return cwt_retv_err;
		}
	}

	return cwt_retv_ok;
}




static cwt_retv iterate_over_test_objects(cw_test_executor_t * cte, cw_test_object_t * test_objects, int topic, cw_sound_system sound_system)
{
	for (cw_test_object_t * test_obj = test_objects; NULL != test_obj->test_function; test_obj++) {
		bool execute = true;
		if (0 != strlen(cte->config->test_function_name)) {
			if (0 != strcmp(cte->config->test_function_name, test_obj->name)) {
				execute = false;
			}
		}

		if (!execute) {
			continue;
		}

		if (cte->use_resource_meas) {
			/* Starting measurement right before it has
			   something to measure.

			   Starting measurements resets old results from
			   previous measurement. This is significant when we
			   want to reset 'max resources' value - we want to
			   measure the 'max resources' value only per test
			   object, not per whole test set. */
			resource_meas_start(&cte->resource_meas, LIBCW_TEST_MEAS_CPU_MEAS_INTERVAL_MSECS);
		}

		cw_test_set_current_topic_and_sound_system(cte, topic, sound_system);
		//fprintf(stderr, "+++ %s +++\n", test_obj->name);
		const cwt_retv retv = test_obj->test_function(cte);


		if (cte->use_resource_meas) {
			usleep(1000 * LIBCW_TEST_INTER_TEST_PAUSE_MSECS);
			/*
			  First stop the test, then display CPU usage summary.

			  Otherwise it may happen that the summary will say that max
			  CPU usage during test was zero, but then the meas object
			  will take the last measurement, detect high CPU usage, and will
			  display the high CPU usage information *after* the summary.
			*/
			resource_meas_stop(&cte->resource_meas);

			const double current_cpu_usage = resource_meas_get_current_cpu_usage(&cte->resource_meas);
			const double max_cpu_usage = resource_meas_get_maximal_cpu_usage(&cte->resource_meas);
			cte->log_info(cte, "CPU usage: last = "CWTEST_CPU_FMT", max = "CWTEST_CPU_FMT"\n", current_cpu_usage, max_cpu_usage);
			if (max_cpu_usage > LIBCW_TEST_MEAS_CPU_OK_THRESHOLD_PERCENT) {
				cte->stats->failures++;
				cte->log_error(cte, "Registered high CPU usage "CWTEST_CPU_FMT" during execution of '%s'\n",
					       max_cpu_usage, test_obj->name);
			}
		}

		if (cwt_retv_ok != retv) {
			return cwt_retv_err;
		}
	}

	return cwt_retv_ok;
}




unsigned int cw_test_get_total_errors_count(cw_test_executor_t * cte)
{
	unsigned int result = 0;
	for (cw_sound_system sound_system = CW_AUDIO_NULL; sound_system <= CW_AUDIO_PA; sound_system++) {
		for (int topic = 0; topic < LIBCW_TEST_TOPIC_MAX; topic++) {
			result += cte->all_stats[sound_system][topic].failures;
		}
	}
	return result;
}
