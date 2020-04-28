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
	struct rusage r_prev;
	struct rusage r_curr;

	struct timeval timestamp_prev;
	struct timeval timestamp_curr;

	struct timeval u_diff;
	struct timeval s_diff;
	struct timeval sum;

	struct timeval timestamp_diff;

	suseconds_t resource_usage;
	suseconds_t meas_duration;

	pthread_mutex_t mutex;
	pthread_attr_t thread_attr;
	pthread_t thread_id;

	double cpu_usage;

} resource_meas;




void resource_meas_start(resource_meas * meas);
void resource_meas_stop(resource_meas * meas);

double resource_meas_get_cpu_usage(resource_meas * meas);




#endif /* #ifndef _LIBCW_TEST_FRAMEWORK_TOOLS_H_ */
