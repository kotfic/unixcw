/*
  This file is a part of unixcw project.  unixcw project is covered by
  GNU General Public License, version 2 or later.
*/

#ifndef _LIBCW_TEST_FRAMEWORK_TOOLS_H_
#define _LIBCW_TEST_FRAMEWORK_TOOLS_H_



#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>






typedef struct {

	/* At what intervals the measurement should be taken. */
	int meas_interval_msecs;

	struct rusage r_prev;
	struct rusage r_curr;

	struct timeval timestamp_prev;
	struct timeval timestamp_curr;

	struct timeval u_diff;
	struct timeval s_diff;
	struct timeval sum;

	struct timeval timestamp_diff;

	suseconds_t resource_usage;
	/* At what interval the last two measurements were really taken. */
	suseconds_t meas_duration;

	pthread_mutex_t mutex;
	pthread_attr_t thread_attr;
	pthread_t thread_id;

	double current_cpu_usage; /* Last calculated value of CPU usage. */
	double maximal_cpu_usage; /* Maximum detected during measurements run. */

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
double resource_meas_get_current_cpu_usage(resource_meas * meas);




/**
   @brief Get maximal CPU usage calculated since measurement has been started

   This function returns the highest value detected since measurement
   was started with resource_meas_start(). The value may be steady or
   may go up. The value is reset to zero each time a
   resource_meas_start() function is called.
*/
double resource_meas_get_maximal_cpu_usage(resource_meas * meas);




#endif /* #ifndef _LIBCW_TEST_FRAMEWORK_TOOLS_H_ */
