/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2014  Kamil Ignacak (acerion@wp.pl)

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


/**
   \file libcw_utils.c

   Utilities.
*/




#include "config.h"


#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <inttypes.h> /* "PRIu32" */

#include <dlfcn.h> /* dlopen() and related symbols */



#if (defined(__unix__) || defined(unix)) && !defined(USG)
# include <sys/param.h>
#endif

#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif





#include "libcw.h"
#include "libcw_test.h"
#include "libcw_internal.h"
#include "libcw_debug.h"
#include "libcw_utils.h"




extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;





/**
   \brief Convert microseconds to struct timespec

   Function fills fields of struct timespec \p t (seconds and nanoseconds)
   based on value of \p usecs.
   \p usecs should be non-negative.

   This function is just a simple wrapper for few lines of code.

   testedin::test_cw_usecs_to_timespec_internal()

   \param t - pointer to existing struct to be filled with data
   \param usecs - value to convert to timespec
*/
void cw_usecs_to_timespec_internal(struct timespec *t, int usecs)
{
	assert (usecs >= 0);
	assert (t);

	int sec = usecs / 1000000;
	int usec = usecs % 1000000;

	t->tv_sec = sec;
	t->tv_nsec = usec * 1000;

	return;
}





/**
   \brief Sleep for period of time specified by given timespec

   Function sleeps for given amount of seconds and nanoseconds, as
   specified by \p n.

   The function uses nanosleep(), and can handle incoming SIGALRM signals
   that cause regular nanosleep() to return. The function calls nanosleep()
   until all time specified by \p n has elapsed.

   The function may sleep a little longer than specified by \p n if it needs
   to spend some time handling SIGALRM signal. Other restrictions from
   nanosleep()'s man page also apply.

   \param n - period of time to sleep
*/
void cw_nanosleep_internal(struct timespec *n)
{
	struct timespec rem = { .tv_sec = n->tv_sec, .tv_nsec = n->tv_nsec };

	int rv = 0;
	do {
		struct timespec req = { .tv_sec = rem.tv_sec, .tv_nsec = rem.tv_nsec };
		//fprintf(stderr, " -- sleeping for %ld s, %ld ns\n", req.tv_sec, req.tv_nsec);
		rv = nanosleep(&req, &rem);
		if (rv) {
			//fprintf(stderr, " -- remains %ld s, %ld ns\n", rem.tv_sec, rem.tv_nsec);
		}
	} while (rv);

	return;
}




#if (defined(LIBCW_WITH_ALSA) || defined(LIBCW_WITH_PULSEAUDIO))
/**
   \brief Try to dynamically open shared library

   Function tries to open a shared library specified by \p name using
   dlopen() system function. On sucess, handle to open library is
   returned via \p handle.

   Name of the library should contain ".so" suffix, e.g.: "libasound.so.2",
   or "libpulse-simple.so".

   \param name - name of library to test
   \param handle - output argument, handle to open library

   \return true on success
   \return false otherwise
*/
bool cw_dlopen_internal(const char *name, void **handle)
{
	assert (name);

	dlerror();
	void *h = dlopen(name, RTLD_LAZY);
	char *e = dlerror();

	if (e) {
		cw_debug_msg (((&cw_debug_object_dev)), CW_DEBUG_STDLIB, CW_DEBUG_ERROR,
			      "libcw: dlopen() fails for %s with error: %s", name, e);
		return false;
	} else {
		*handle = h;

		cw_debug_msg (((&cw_debug_object_dev)), CW_DEBUG_STDLIB, CW_DEBUG_DEBUG,
			      "libcw: dlopen() succeeds for %s", name);
		return true;
	}
}
#endif





/**
   \brief Validate and return timestamp

   If an input timestamp \p in_timestamp is given (non-NULL pointer),
   validate it for correctness, and if valid, copy contents of
   \p in_timestamp into \p out_timestamp and return CW_SUCCESS.

   If \p in_timestamp is non-NULL and the timestamp is invalid, return
   CW_FAILURE with errno set to EINVAL.

   If \p in_timestamp is not given (NULL), get current time (with
   gettimeofday()), put it in \p out_timestamp and return
   CW_SUCCESS. If call to gettimeofday() fails, return CW_FAILURE.

   \p out_timestamp cannot be NULL.

   testedin::test_cw_timestamp_validate_internal()

   \param out_timestamp - timestamp to be used by client code after the function call
   \param in_timestamp - timestamp to be validated

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_timestamp_validate_internal(struct timeval *out_timestamp, const struct timeval *in_timestamp)
{
	cw_assert (out_timestamp, "pointer to output timestamp is NULL");

	if (in_timestamp) {
		if (in_timestamp->tv_sec < 0
		    || in_timestamp->tv_usec < 0
		    || in_timestamp->tv_usec >= CW_USECS_PER_SEC) {

			errno = EINVAL;
			return CW_FAILURE;
		} else {
			*out_timestamp = *in_timestamp;
			return CW_SUCCESS;
		}
	} else {
		if (gettimeofday(out_timestamp, NULL)) {
			if (out_timestamp->tv_usec < 0) {
				// fprintf(stderr, "Negative usecs in %s\n", __func__);
			}

			perror ("libcw: gettimeofday");
			return CW_FAILURE;
		} else {
			return CW_SUCCESS;
		}
	}
}





/**
   \brief Compare two timestamps

   Compare two timestamps, and return the difference between them in
   microseconds, taking care to clamp values which would overflow an int.

   This routine always returns a positive integer in the range 0 to INT_MAX.

   testedin::test_cw_timestamp_compare_internal()

   \param earlier - timestamp to compare
   \param later - timestamp to compare

   \return difference between timestamps (in microseconds)
*/
int cw_timestamp_compare_internal(const struct timeval *earlier,
				  const struct timeval *later)
{

	/* Compare the timestamps, taking care on overflows.

	   At 4 WPM, the dash length is 3*(1200000/4)=900,000 usecs, and
	   the word gap is 2,100,000 usecs.  With the maximum Farnsworth
	   additional delay, the word gap extends to 20,100,000 usecs.
	   This fits into an int with a lot of room to spare, in fact, an
	   int can represent 2,147,483,647 usecs, or around 33 minutes.
	   This is way, way longer than we'd ever want to differentiate,
	   so if by some chance we see timestamps farther apart than this,
	   and it ought to be very, very unlikely, then we'll clamp the
	   return value to INT_MAX with a clear conscience.

	   Note: passing nonsensical or bogus timevals in may result in
	   unpredictable results.  Nonsensical includes timevals with
	   -ve tv_usec, -ve tv_sec, tv_usec >= 1,000,000, etc.
	   To help in this, we check all incoming timestamps for
	   "well-formedness".  However, we assume the  gettimeofday()
	   call always returns good timevals.  All in all, timeval could
	   probably be a better thought-out structure. */

	/* Calculate an initial delta, possibly with overflow. */
	int delta_usec = (later->tv_sec - earlier->tv_sec) * CW_USECS_PER_SEC
		+ later->tv_usec - earlier->tv_usec;

	/* Check specifically for overflow, and clamp if it did. */
	if ((later->tv_sec - earlier->tv_sec) > (INT_MAX / CW_USECS_PER_SEC) + 1
	    || delta_usec < 0) {

		delta_usec = INT_MAX;
		// fprintf(stderr, "earlier =           %10ld : %10ld\n", earlier->tv_sec, earlier->tv_usec);
		// fprintf(stderr, "later   =           %10ld : %10ld\n", later->tv_sec, later->tv_usec);
	}

	/* TODO: add somewhere a debug message informing that we are
	   returning INT_MAX. */

	return delta_usec;
}





/* ******************************************************************** */
/*             Section:Unit tests for internal functions                */
/* ******************************************************************** */





#ifdef LIBCW_UNIT_TESTS



extern cw_gen_t **cw_generator;



unsigned int test_cw_forever(void)
{
	int p = fprintf(stderr, "libcw: CW_AUDIO_FOREVER_USECS:");

	int rv = cw_generator_new(CW_AUDIO_OSS, NULL);
	assert (rv);

	cw_generator_start();

	sleep(1);

	cw_tone_t tone;

	tone.usecs = 100;
	tone.frequency = 500;
	tone.slope_mode = CW_SLOPE_MODE_RISING_SLOPE;
	cw_tone_queue_enqueue_internal((*cw_generator)->tq, &tone);

	tone.slope_mode = CW_SLOPE_MODE_NO_SLOPES;
	tone.usecs = CW_AUDIO_FOREVER_USECS;
	tone.frequency = 500;
	cw_tone_queue_enqueue_internal((*cw_generator)->tq, &tone);

	cw_wait_for_tone_queue();

	sleep(6);

	tone.usecs = 100;
	tone.frequency = 500;
	tone.slope_mode = CW_SLOPE_MODE_FALLING_SLOPE;
	cw_tone_queue_enqueue_internal((*cw_generator)->tq, &tone);

	cw_generator_stop();
	cw_generator_delete();


	CW_TEST_PRINT_TEST_RESULT(false, p);

	return 0;
}





/**
   tests::cw_timestamp_compare_internal()
*/
unsigned int test_cw_timestamp_compare_internal(void)
{
	int p = fprintf(stderr, "libcw: cw_timestamp_compare_internal():");

	struct timeval earlier_timestamp;
	struct timeval later_timestamp;

	/* TODO: I think that there may be more tests to perform for
	   the function, testing handling of overflow. */

	int expected_deltas[] = { 0,
				  1,
				  1001,
				  CW_USECS_PER_SEC - 1,
				  CW_USECS_PER_SEC,
				  CW_USECS_PER_SEC + 1,
				  2 * CW_USECS_PER_SEC - 1,
				  2 * CW_USECS_PER_SEC,
				  2 * CW_USECS_PER_SEC + 1,
				  -1 }; /* Guard. */


	earlier_timestamp.tv_sec = 3;
	earlier_timestamp.tv_usec = 567;

	int i = 0;
	while (expected_deltas[i] != -1) {

		later_timestamp.tv_sec = earlier_timestamp.tv_sec + (expected_deltas[i] / CW_USECS_PER_SEC);
		later_timestamp.tv_usec = earlier_timestamp.tv_usec + (expected_deltas[i] % CW_USECS_PER_SEC);

		int delta = cw_timestamp_compare_internal(&earlier_timestamp, &later_timestamp);
		cw_assert (delta == expected_deltas[i], "test #%d: unexpected delta: %d != %d", i, delta, expected_deltas[i]);

		i++;
	}

	CW_TEST_PRINT_TEST_RESULT(false, p);

	return 0;
}





/**
   tests::cw_timestamp_validate_internal()
*/
unsigned int test_cw_timestamp_validate_internal(void)
{
	int p = fprintf(stderr, "libcw: cw_timestamp_validate_internal():");

	struct timeval out_timestamp;
	struct timeval in_timestamp;
	struct timeval ref_timestamp; /* Reference timestamp. */
	int rv = 0;


	/* Test 1 - get current time. */
	out_timestamp.tv_sec = 0;
	out_timestamp.tv_usec = 0;

	cw_assert (!gettimeofday(&ref_timestamp, NULL), "failed to get reference time");

	rv = cw_timestamp_validate_internal(&out_timestamp, NULL);
	cw_assert (rv, "test 1: failed to get current timestamp with cw_timestamp_validate_internal(), errno = %d", errno);

#if 0
	fprintf(stderr, "\nINFO: delay in getting timestamp is %d microseconds\n",
		cw_timestamp_compare_internal(&ref_timestamp, &out_timestamp));
#endif



	/* Test 2 - validate valid input timestamp and copy it to
	   output timestamp. */
	out_timestamp.tv_sec = 0;
	out_timestamp.tv_usec = 0;
	in_timestamp.tv_sec = 1234;
	in_timestamp.tv_usec = 987;

	rv = cw_timestamp_validate_internal(&out_timestamp, &in_timestamp);
	cw_assert (rv, "test 2: failed to validate timestamp with cw_timestamp_validate_internal(), errno = %d", errno);
	cw_assert (out_timestamp.tv_sec == in_timestamp.tv_sec, "test 2: failed to correctly copy seconds: %d != %d",
		   (int) out_timestamp.tv_sec, (int) in_timestamp.tv_sec);
	cw_assert (out_timestamp.tv_usec == in_timestamp.tv_usec, "test 2: failed to correctly copy microseconds: %d != %d",
		   (int) out_timestamp.tv_usec, (int) in_timestamp.tv_usec);



	/* Test 3 - detect invalid seconds in input timestamp. */
	out_timestamp.tv_sec = 0;
	out_timestamp.tv_usec = 0;
	in_timestamp.tv_sec = -1;
	in_timestamp.tv_usec = 987;

	rv = cw_timestamp_validate_internal(&out_timestamp, &in_timestamp);
	cw_assert (!rv, "test 3: failed to recognize invalid seconds");
	cw_assert (errno == EINVAL, "failed to properly set errno, errno is %d", errno);




	/* Test 4 - detect invalid microseconds in input timestamp (microseconds too large). */
	out_timestamp.tv_sec = 0;
	out_timestamp.tv_usec = 0;
	in_timestamp.tv_sec = 123;
	in_timestamp.tv_usec = CW_USECS_PER_SEC + 1;

	rv = cw_timestamp_validate_internal(&out_timestamp, &in_timestamp);
	cw_assert (!rv, "test 4: failed to recognize invalid microseconds");
	cw_assert (errno == EINVAL, "test 4: failed to properly set errno, errno is %d", errno);




	/* Test 5 - detect invalid microseconds in input timestamp (microseconds negative). */
	out_timestamp.tv_sec = 0;
	out_timestamp.tv_usec = 0;
	in_timestamp.tv_sec = 123;
	in_timestamp.tv_usec = -1;

	rv = cw_timestamp_validate_internal(&out_timestamp, &in_timestamp);
	cw_assert (!rv, "test 5: failed to recognize invalid microseconds");
	cw_assert (errno == EINVAL, "test 5: failed to properly set errno, errno is %d", errno);


	CW_TEST_PRINT_TEST_RESULT(false, p);

	return 0;
}





/**
   tests::cw_usecs_to_timespec_internal()
*/
unsigned int test_cw_usecs_to_timespec_internal(void)
{
	int p = fprintf(stderr, "libcw: cw_usecs_to_timespec_internal():");

	struct {
		int input;
		struct timespec t;
	} input_data[] = {
		/* input in ms    /   expected output seconds : milliseconds */
		{           0,    {   0,             0 }},
		{     1000000,    {   1,             0 }},
		{     1000004,    {   1,          4000 }},
		{    15000350,    {  15,        350000 }},
		{          73,    {   0,         73000 }},
		{          -1,    {   0,             0 }},
	};

	int i = 0;
	while (input_data[i].input != -1) {
		struct timespec result = { .tv_sec = 0, .tv_nsec = 0 };
		cw_usecs_to_timespec_internal(&result, input_data[i].input);
#if 0
		fprintf(stderr, "input = %d usecs, output = %ld.%ld\n",
			input_data[i].input, (long) result.tv_sec, (long) result.tv_nsec);
#endif
		assert(result.tv_sec == input_data[i].t.tv_sec);
		assert(result.tv_nsec == input_data[i].t.tv_nsec);

		i++;
	}

	CW_TEST_PRINT_TEST_RESULT(false, p);

	return 0;
}





#endif /* #ifdef LIBCW_UNIT_TESTS */