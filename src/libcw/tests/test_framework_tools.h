/*
  This file is a part of unixcw project.  unixcw project is covered by
  GNU General Public License, version 2 or later.
*/

#ifndef _LIBCW_TEST_FRAMEWORK_TOOLS_H_
#define _LIBCW_TEST_FRAMEWORK_TOOLS_H_



#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>




/* Format to be used when printing CPU usage with printf(). */
#define CWTEST_CPU_FMT "%03d%%"




typedef struct {

	/* At what intervals the measurement should be taken. */
	int meas_interval_msecs;

	struct rusage rusage_prev;
	struct rusage rusage_curr;

	struct timeval timestamp_prev;
	struct timeval timestamp_curr;

	struct timeval user_cpu_diff; /* User CPU time used. */
	struct timeval sys_cpu_diff;  /* System CPU time used. */
	struct timeval summary_cpu_usage; /* User + System CPU time used. */

	struct timeval timestamp_diff;

	suseconds_t resource_usage;
	/* At what interval the last two measurements were really taken. */
	suseconds_t meas_duration;

	pthread_mutex_t mutex;
	pthread_attr_t thread_attr;
	pthread_t thread_id;

	int current_cpu_usage; /* Last calculated value of CPU usage. */
	int maximal_cpu_usage; /* Maximum detected during measurements run. */

} resource_meas;




/**
   @brief Start measurement process

   This function also resets to zero 'max resource usage' field in
   @param meas, so that a new value can be calculated during new
   measurement.

   @param meas_interval_msecs - at what intervals the measurements should be taken
*/
void resource_meas_start(resource_meas * meas, int meas_interval_msecs);
void resource_meas_stop(resource_meas * meas);




/**
   @brief Get current CPU usage

   The value may change from one measurement to another - may rise and
   fall.
*/
int resource_meas_get_current_cpu_usage(resource_meas * meas);




/**
   @brief Get maximal CPU usage calculated since measurement has been started

   This function returns the highest value detected since measurement
   was started with resource_meas_start(). The value may be steady or
   may go up. The value is reset to zero each time a
   resource_meas_start() function is called.
*/
int resource_meas_get_maximal_cpu_usage(resource_meas * meas);




/**
   Direction in which values returned by calls to _get_next() will go:
   will they increase, will they decrease or will they stay on
   constant level (plateau) for few calls to _get_next().

   Using bits to mark direction. Once we reach plateau, we have to
   remember in which direction to go once we get off from the plateau.
*/
typedef enum cwtest_param_ranger_direction {
	cwtest_param_ranger_direction_up      = 0x01,
	cwtest_param_ranger_direction_down    = 0x02,
	cwtest_param_ranger_direction_plateau = 0x04
} cwtest_param_ranger_direction;




/**
   Object for obtaining varying values of some integer parameter from
   specified range on each call to _get_next() function.

   Currently the returned values can change linearly up and down
   between min and max. Possibly in the future the ranger will support
   modes other than linear: random, sine, or other.
*/
typedef struct cwtest_param_ranger_t {
	/* Minimal value of parameter values generated. */
	int range_min;

	/* Maximal value of parameter values generated. */
	int range_max;

	/* By how much the returned value changes on each successful
	   call to _get_next(). */
	int step;

	/* Internal helper variable. Value returned by previous call
	   to _get_next(), used in calculating new value returned by
	   _get_next(). */
	int previous_value;

	/* Internal helper variable. In linear generation method: flag
	   that dictates if values returned by _get_next() are
	   linearly increasing or decreasing. */
	cwtest_param_ranger_direction direction;

	/* Internal helper variable. Timestamp at which previous new
	   value was returned by _get_next() */
	time_t previous_timestamp;

	/* Time interval at which new values are calculated and
	   returned by _get_next(). If time between previous_timestamp
	   and current time is less than interval_sec, _get_next()
	   will return false. */
	time_t interval_sec;

	/* When parameter reaches minimum or maximum value, how many
	   next calls to _get_next() should return the same min/max
	   value (how many calls to _get_next() should stay on the
	   plateau)?

	   Set to zero to disable this feature.

	   Value is zero by default.

	   This is a bit fuzzy parameter, with possible off-by-one
	   error. Don't set it to 1 or 2 or such small value and
	   expect _get_next() to return min/max values exactly 1 or 2
	   times. Set it to 10 or 20, and expect approximately 10 or
	   approximately 20 calls to _get_next() to return min/max.

	   Unit: times (successful calls to _get_next() that return
	   min/max).
	*/
	int plateau_length;

	/* Internal helper variable. If plateau_length is non-zero,
	   how many next calls to _get_next() should return previous
	   (minimal or maximal) value? */
	int plateau_remaining;
} cwtest_param_ranger_t;




/**
   @brief Initialize @p ranger

   @param ranger ranger to initialize
   @param min minimal value of range of values returned by _get_next(), e.g. CW_SPEED_MIN
   @param max maximal value of range of values returned by _get_next(), e.g. CW_SPEED_MAX
   @param step by how much the returned value changes on each successful call to _get_next(), e.g. CW_SPEED_STEP or 4
   @param initial_value initial parameter value stored by @p ranger and used to calculate first value returned by _get_next(), e.g. value returned by cw_gen_get_speed()
*/
void cwtest_param_ranger_init(cwtest_param_ranger_t * ranger, int min, int max, int step, int initial_value);




/**
   @brief Configure ranger to generate new value only if specific
   interval has passed since previous successful call to _get_next()

   If @p interval_sec is non-zero, calling _get_next() on @p ranger will
   return new value only if at least @p interval_sec seconds passed
   since last successful call.

   If you have some control loop executed every 100ms, and you want to
   be able to operate on @p ranger in this loop, but want to get
   values less frequently than every 100ms, you can configure desired
   time interval for @p ranger with this function. The calls to
   _get_next() will then return success and new value each @p
   interval_sec.

   Pass zero value of @p interval_sec to disable this feature for @p
   ranger.

   @param ranger
   @param interval_sec at what intervals the call to _get_next() will return true and will return next value of parameter
*/
void cwtest_param_ranger_set_interval_sec(cwtest_param_ranger_t * ranger, time_t interval_sec);




/**
   @brief Configure plateau length for ranger

   When value calculated by ranger reaches min or max, decide how many
   following calls to _get_next() will return the same value equal to
   min or max. The values returned by _get_next() will stay on that
   'plateau' for approximately @p plateau_length calls. After
   approximately @p plateau_length calls, the values returned by
   _get_next() will leave the plateau and will start to change again.

   Pass zero value of @p plateau_length to disable this feature for @p
   ranger.

   @param ranger
   @param plateau_length how many calls to _get_next() will return non-changing min or max value, when the min/max is reached
*/
void cwtest_param_ranger_set_plateau_length(cwtest_param_ranger_t * ranger, int plateau_length);




/**
   @brief Get next value from @p ranger

   On successful call function returns true, and new value is returned
   through @p new_value. Otherwise false is returned.

   If @param ranger is configured to use intervals (with
   cwtest_param_ranger_set_interval_sec()), only calls that are
   separated by at least given time interval will return
   true. Otherwise each call to _get_next() will be successful.

   @return true if ranger has returned new value through @p new_value
   @return false otherwise
*/
bool cwtest_param_ranger_get_next(cwtest_param_ranger_t * ranger, int * new_value);




#endif /* #ifndef _LIBCW_TEST_FRAMEWORK_TOOLS_H_ */
