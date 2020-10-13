/*
  This file is a part of unixcw project.  unixcw project is covered by
  GNU General Public License, version 2 or later.
*/

#ifndef _LIBCW_TEST_FRAMEWORK_H_
#define _LIBCW_TEST_FRAMEWORK_H_




#include <stdio.h>
#include <stdbool.h>




#include <sys/time.h>




#include <libcw.h>



#include "test_framework_tools.h"
#include "cw_common.h"



/* Total width of test name + test status printed in console (without
   ending '\n'). Remember that some consoles have width = 80. Not
   everyone works in X. */
#define default_cw_test_print_n_chars 75





typedef struct {
	/* TODO: these should be unsigned. */
	int successes;
	int failures;
} cw_test_stats_t;




/*
  Test framework function return values.

  The values indicate success/failure of test framework functions, not
  of tests themselves.  Success/failure of tests is recorded in
  cw_test_stats_t variables in cw test executor. Failure of test
  framework function may happen e.g. because some malloc() fails, or
  because some helper function fails.
*/
typedef enum {
	cwt_retv_err = -1,
	cwt_retv_ok = 0,
} cwt_retv;




struct cw_test_set_t;




struct cw_test_executor_t;
typedef struct cw_test_executor_t {

	cw_config_t * config;

	char msg_prefix[32];

	/* gcc on Alpine 3.12 doesn't like these variables to be called
	   stdout/stderr. */
	FILE * file_out;
	FILE * file_err;

	resource_meas resource_meas;
	bool use_resource_meas;

	/* Type suitable for srand48(). */
	long int random_seed;

	/* Sound system and test topic currently tested.
	   Should be set right before calling a specific test function. */
	enum cw_audio_systems current_sound_system;
	char current_sound_device[LIBCW_SOUND_DEVICE_NAME_SIZE];
	int current_topic;

	/* Limit of characters that can be printed to console in one row. */
	int console_n_cols;

	/* This array holds stats for all distinct sound systems, and
	   is indexed by cw_audio_systems enum. This means that first
	   row will not be used (because CW_AUDIO_NONE==0 is not a
	   distinct sound system). */
	cw_test_stats_t all_stats[CW_SOUND_SYSTEM_LAST + 1][LIBCW_TEST_TOPIC_MAX];
	cw_test_stats_t * stats; /* Pointer to current stats (one of fields of ::all_stats[][]). */



	/**
	   Verify that operator @param operator is satisfied for
	   @param received_value and @param expected_value

	   Use the function to verify that a result of libcw function
	   or of other operation (@param received_value) is in proper
	   relation to expected value (@param expected_value).

	   expect_op_X(): print log specified by @param fmt and
	   following args regardless of results of the
	   verification. Include test result (success or failure) of
	   the test in statistics.

	   expect_op_X_errors_only(): print log specified by @param
	   fmt and following args only if the application of operator
	   has shown that the values DON'T satisfy the operator, which
	   is regarded as failure of expectation, i.e. an error.
	   Include test result in statistics only if the result was an
	   error.

	   You shouldn't put newline character at end of formatting
	   string. The messages printed by these functions will be followed
	   by "[ OK ]" or "[FAIL]" tags, and the newline would put these tags
	   in new line, decreasing readability of test results.

	   @return true if this comparison shows that the values satisfy the operator
	   @return false otherwise
	*/
	bool (* expect_op_int)(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
	bool (* expect_op_int_errors_only)(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
	bool (* expect_op_float)(struct cw_test_executor_t * self, float expected_value, const char * operator, float received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
	bool (* expect_op_float_errors_only)(struct cw_test_executor_t * self, float expected_value, const char * operator, float received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));

	/**
	   Verify that @param received_value is between @param
	   expected_lower and @param expect_higher (inclusive)

	   Use the function to verify that a result of libcw function
	   or of other operation (@param received_value) is larger
	   than or equal to @param expected_lower, and at the same
	   time is smaller than or equal to @param expected_higher.

	   expect_op_X(): print log specified by @param fmt and
	   following args regardless of results of the
	   verification. Include test result (success or failure) of
	   the test in statistics.

	   expect_op_X_errors_only(): print log specified by @param
	   fmt and following args only if the application of operator
	   has shown that the values DON'T satisfy the operator, which
	   is regarded as failure of expectation, i.e. an error.
	   Include test result in statistics only if the result was an
	   error.

	   @return true if this comparison shows that the given value is within specified range
	   @return false otherwise
	*/
	bool (* expect_between_int)(struct cw_test_executor_t * self, int expected_lower, int received_value, int expected_higher, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
	bool (* expect_between_int_errors_only)(struct cw_test_executor_t * self, int expected_lower, int received_value, int expected_higher, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));

	/**
	   Verify that @param pointer is NULL pointer

	   expect_op_X(): print log specified by @param fmt and
	   following args regardless of results of the
	   verification. Include test result (success or failure) of
	   the test in statistics.

	   expect_op_X_errors_only(): print log specified by @param
	   fmt and following args only if verification was negative.
	   Include test result in statistics only if the result was an
	   error.

	   @return true if this comparison shows that the pointer is truly a NULL pointer
	   @return false otherwise
	*/
	bool (* expect_null_pointer)(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));
	bool (* expect_null_pointer_errors_only)(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));

	/**
	   Verify that @param pointer is valid (non-NULL) pointer

	   Print log specified by @param fmt and following args
	   regardless of results of the verification or (in case of
	   _errors_only() variant) only if the check has shown that
	   pointer *IS* NULL pointer, which is regarded as failure
	   of expectation, i.e. an error.

	   Right now there are no additional checks of validity of the
	   pointer. Function only checks if it is non-NULL pointer.

	   @return true if this comparison shows that the pointer is a non-NULL pointer
	   @return false otherwise
	*/
	bool (* expect_valid_pointer)(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));
	bool (* expect_valid_pointer_errors_only)(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));




	/**
	   An assert - not much to explain
	*/
	void (* assert2)(struct cw_test_executor_t * self, bool condition, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));




	/**
	   @brief Print an informative header with information about current test

	   Call this function on top of a test function to display
	   some basic information about test: current test topic,
	   current sound system and name of test function. This should
	   get a basic overview of what is about to be tested now.

	   Name of the function is usually passed through @param fmt
	   argument. @param fmt can be also printf()-like format
	   string, followed by additional arguments
	*/
	void (* print_test_header)(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));

	/**
	   @brief Print a not-so-informative test footer

	   Call the function at the very end of test function to
	   display indication that test function with name @param
	   function_name has completed its work.
	*/
	void (* print_test_footer)(struct cw_test_executor_t * self, const char * function_name);




	/**
	   \brief Process command line arguments of test executable

	   @return 0 on success: program can continue after arguments
	   have been processes (or no arguments were provided),

	   Function exits with EXIT_FAILURE status when function
	   encountered errors during processing of command line
	   args. Help text is printed before the exit.

	   Function exits with EXIT_SUCCESS status when "help" option
	   was requested. Help text is printed before the exit.
	*/
	int (* process_args)(struct cw_test_executor_t * self, int argc, char * const argv[]);


	/**
	   \brief Get count of loops of a test

	   Get a loop limit for some repeating tests. It may be a
	   small value for quick tests, or it may be a large value for
	   long-term tests.
	 */
	int (* get_loops_count)(struct cw_test_executor_t * self);


	/**
	   @brief Print summary of program's arguments and options that are in effect

	   Print names of test topics that will be tested and of sound
	   systems that will be used in tests. Call this function
	   after processing command line arguments with
	   ::process_args().
	*/
	void (* print_test_options)(struct cw_test_executor_t * self);




	/**
	   @brief Print a table with summary of test statistics

	   The statistics are presented as a table. There are columns
	   with test topics and rows with sound modules. Each table
	   cell contains a total number of tests in given category and
	   number of errors (failed test functions) in that category.
	*/
	void (* print_test_stats)(struct cw_test_executor_t * self);




	/**
	   @brief Get label of currently tested test topic

	   Return pointer to static string buffer with label of
	   currently tested test topic. Notice: if the topic is not
	   set, function returns "unknown" text label.
	*/
	const char * (* get_current_topic_label)(struct cw_test_executor_t * self);

	/**
	   @brief Get label of currently used sound system

	   Return pointer to static string buffer with label of
	   currently used sound system  Notice: if the sound system is not
	   set, function returns "None" text label.
	*/
	const char * (* get_current_sound_system_label)(struct cw_test_executor_t * self);

	/**
	   @brief Get name of current sound device
	*/
	const char * (* get_current_sound_device)(struct cw_test_executor_t * self);




	/**
	   Log information to cw_test_executor_t::stdout file (if it is set).
	   Add "[II]" mark at the beginning.
	   Add message prefix at the beginning.
	   Don't add newline character at the end.

	   @return number of characters printed
	*/
        int (* log_info)(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));

	/**
	   Log text to cw_test_executor_t::stdout file (if it is set).
	   Don't add "[II]" mark at the beginning.
	   Don't add message prefix at the beginning.
	   Don't add newline character at the end.
	*/
	void (* log_info_cont)(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));

	/**
	   Flush file descriptor used to log info messages
	*/
	void (* flush_info)(struct cw_test_executor_t * self);

	/**
	   Log error to cw_test_executor_t::stdout file (if it is set).
	   Add "[EE]" mark at the beginning.
	   Add message prefix at the beginning.
	   Don't add newline character at the end.
	*/
	void (* log_error)(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));

	/**
	   See whether or not a given test topic was requested from
	   command line. By default, if not specified in command line,
	   all test topics are requested.
	*/
	bool (* test_topic_was_requested)(struct cw_test_executor_t * self, int libcw_test_topic);

	/**
	   See whether or not a given sound system was requested from
	   command line. By default, if not specified in command line,
	   all sound systems are requested.

	   However, if a host machine does not support some sound
	   system (e.g. because a library is missing), such sound
	   system is excluded from list of requested sound systems.
	*/
	bool (* sound_system_was_requested)(struct cw_test_executor_t * self, cw_sound_system sound_system);




	/**
	   @brief Main test loop that walks through given @param
	   @test_sets and executes all test function specified in
	   @param test_sets

	   @return cwt_retv_ok if test framework worked correctly
	   @return cwt_retv_err if test framework failed at some point
	*/
	cwt_retv (* main_test_loop)(struct cw_test_executor_t * cte, struct cw_test_set_t * test_sets);

	/**
	   @brief Get total count of errors after main_test_loop() has returned

	   The function can be used to check if there were any errors
	   reported by tested functions.

	   @return 0 if no errors were reported by test functions
	   @return some positive value otherwise
	*/
	unsigned int (* get_total_errors_count)(struct cw_test_executor_t * cte);

} cw_test_executor_t;




/**
   @brief Initialize cw_text_executor_t object @param cte

   Some messages printed by logger function of cw_text_executor_t
   object will be prefixed with @param msg_prefix.

   No resources are allocated in @param cte during the call, so there
   is no "deinit" function.
*/
void cw_test_init(cw_test_executor_t * self, FILE * stdout, FILE * stderr, const char * msg_prefix);




/**
   @brief Deinitialize cw_test_executor_t object @param cte
*/
void cw_test_deinit(cw_test_executor_t * self);




/**
   @return cwt_retv_ok if test framework managed to successfully run the test (regardless of results of the test itself)
   @return cwt_retv_err if test framework failed to run the test (the test didn't manage to test production code)
*/
typedef cwt_retv (* cw_test_function_t)(cw_test_executor_t * cte);

typedef enum cw_test_set_valid {
	LIBCW_TEST_SET_INVALID,
	LIBCW_TEST_SET_VALID,
} cw_test_set_valid;

typedef enum cw_test_api_tested {
	LIBCW_TEST_API_LEGACY, /* Tests of functions from libcw.h. Legacy API that does not allow using multiple gen/key/rec objects. */
	LIBCW_TEST_API_MODERN, /* Tests of internal functions that operate on explicit gen/key/rec objects (functions that accept such objects as arguments). */
} cw_test_api_tested;




typedef	struct cw_test_object_t {
	cw_test_function_t test_function;
	const char * name;                 /* Unique label/name of test function, used to execute only one test function from whole set. Can be empty/NULL. */
	bool is_quick;                     /* Is the execution of the function quick? Is the time short enough to execute the function during "make check" target during builds? */
} cw_test_object_t;




typedef struct cw_test_set_t {
	cw_test_set_valid set_valid; /* Invalid test set is a guard element in array of test sets. */
	cw_test_api_tested api_tested;

	/* libcw areas tested by given test set. */
	int tested_areas[LIBCW_TEST_TOPIC_MAX];

	/* Distinct sound systems that need to be configured to test
	   given test set. This array is indexed from zero. End of
	   this array is marked by guard element CW_AUDIO_NONE. */
	cw_sound_system tested_sound_systems[CW_SOUND_SYSTEM_LAST + 1];

	cw_test_object_t test_objects[100]; /* Right now my test sets have only a few test functions. For now 100 is a safe limit. */
} cw_test_set_t;




#define LIBCW_TEST_STRINGIFY(x) #x
#define LIBCW_TEST_TOSTRING(x) LIBCW_TEST_STRINGIFY(x)
#define LIBCW_TEST_FUNCTION_INSERT(function_pointer, _is_quick_) { .test_function = (function_pointer), .name = LIBCW_TEST_TOSTRING(function_pointer), .is_quick = _is_quick_ }

/* FUT = "Function under test". A function from libcw library that is
   the subject of a test. */
#define LIBCW_TEST_FUT(x) x

/* At what intervals the CPU usage will be measured. */
#define LIBCW_TEST_MEAS_CPU_MEAS_INTERVAL_MSECS 200

/* What is the top CPU usage threshold during test function's execution. */
#define LIBCW_TEST_MEAS_CPU_OK_THRESHOLD_PERCENT 4.0F




#endif /* #ifndef _LIBCW_TEST_FRAMEWORK_H_ */
