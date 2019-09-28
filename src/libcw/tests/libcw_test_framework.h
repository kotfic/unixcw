/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef _LIBCW_TEST_UTILS_H_
#define _LIBCW_TEST_UTILS_H_




#include <stddef.h> /* size_t */
#include <stdio.h>
#include <stdbool.h>


#include <libcw.h>


#define out_file stdout

/* Total width of test name + test status printed in console (without
   ending '\n'). Remember that some consoles have width = 80. Not
   everyone works in X. */
#define default_cw_test_print_n_chars 75

#define LIBCW_TEST_ALL_TOPICS           "gtkro"   /* generator, tone queue, key, receiver, other. */
#define LIBCW_TEST_ALL_SOUND_SYSTEMS    "ncoap"   /* null, console, oss, alsa, pulseaudio. */




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
	int successes;
	int failures;
} cw_test_stats_t;




struct cw_test_executor_t;
typedef struct cw_test_executor_t {
	char msg_prefix[32];
	FILE * stdout;
	FILE * stderr;

	int current_sound_system;

	/* Limit of characters that can be printed to console in one row. */
	int console_n_cols;

	cw_test_stats_t stats_indep;
	cw_test_stats_t stats_null;
	cw_test_stats_t stats_console;
	cw_test_stats_t stats_oss;
	cw_test_stats_t stats_alsa;
	cw_test_stats_t stats_pa;
	cw_test_stats_t * stats; /* Pointer to current stats. */
	cw_test_stats_t stats2[CW_AUDIO_SOUNDCARD][LIBCW_TEST_TOPIC_MAX];

	char tested_sound_systems[sizeof (LIBCW_TEST_ALL_SOUND_SYSTEMS)];
	char tested_topics[sizeof (LIBCW_TEST_ALL_TOPICS)];


	bool (* expect_eq_int)(struct cw_test_executor_t * self, int expected_value, int received_value, const char * fmt, ...) __attribute__ ((format (printf, 4, 5)));
	bool (* expect_eq_int_errors_only)(struct cw_test_executor_t * self, int expected_value, int received_value, const char * fmt, ...) __attribute__ ((format (printf, 4, 5)));

	bool (* expect_null_pointer)(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));
	bool (* expect_null_pointer_errors_only)(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));

	bool (* expect_valid_pointer)(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));
	bool (* expect_valid_pointer_errors_only)(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));


	void (* print_test_header)(struct cw_test_executor_t * self, const char * text);
	void (* print_test_footer)(struct cw_test_executor_t * self, const char * text);
	int (* process_args)(struct cw_test_executor_t * self, int argc, char * const argv[]);
	void (* print_test_stats)(struct cw_test_executor_t * self);

	const char * (* get_current_sound_system_label)(struct cw_test_executor_t * self);
	void (* set_current_sound_system)(struct cw_test_executor_t * self, int sound_system);

	/**
	   Log information to cw_test_executor_t::stdout file (if it is set).
	   Add "[II]" mark at the beginning.
	   Add message prefix at the beginning.
	   Don't add newline character at the end.
	*/
	void (* log_info)(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));
	/**
	   Log information to cw_test_executor_t::stdout file (if it is set).
	   Don't add "[II]" mark at the beginning.
	   Don't add message prefix at the beginning.
	   Don't add newline character at the end.
	*/
	void (* log_info_cont)(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));
	/**
	   Log error to cw_test_executor_t::stdout file (if it is set).
	   Add "[EE]" mark at the beginning.
	   Add message prefix at the beginning.
	   Don't add newline character at the end.
	*/
	void (* log_err)(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));

	bool (* should_test_topic)(struct cw_test_executor_t * self, const char * topic);
	bool (* should_test_sound_system)(struct cw_test_executor_t * self, const char * sound_system);
} cw_test_executor_t;



void cw_test_init(cw_test_executor_t * self, FILE * stdout, FILE * stderr, const char * msg_prefix);

void cw_test_print_help(const char *progname);


typedef int (* cw_test_function_t)(cw_test_executor_t * cte);

typedef int (* tester_fn)(cw_test_executor_t * cte);

int cw_test_topics_with_sound_systems(cw_test_executor_t * cte, tester_fn test_topics_with_current_sound_system);



#endif /* #ifndef _LIBCW_TEST_UTILS_H_ */
