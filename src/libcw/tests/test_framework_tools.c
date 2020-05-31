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




#include <stdio.h>
#include <unistd.h>
#include <string.h>




#include "test_framework_tools.h"
#include "test_framework.h"




static void resource_meas_do_measurement(resource_meas * meas);
static void * resouce_meas_thread(void * arg);



void * resouce_meas_thread(void * arg)
{
	resource_meas * meas = (resource_meas *) arg;
	while (1) {
		resource_meas_do_measurement(meas);
		usleep(1000 * meas->meas_interval_msecs);
	}

	return NULL;
}




void resource_meas_start(resource_meas * meas, int meas_interval_msecs)
{
	memset(meas, 0, sizeof (*meas));
	meas->meas_interval_msecs = meas_interval_msecs;

	pthread_mutex_init(&meas->mutex, NULL);
	pthread_attr_init(&meas->thread_attr);
	pthread_create(&meas->thread_id, &meas->thread_attr, resouce_meas_thread, meas);
}




void resource_meas_stop(resource_meas * meas)
{
	pthread_mutex_lock(&meas->mutex);

	pthread_cancel(meas->thread_id);
	pthread_attr_destroy(&meas->thread_attr);

	pthread_mutex_unlock(&meas->mutex);
	pthread_mutex_destroy(&meas->mutex);
}




double resource_meas_get_current_cpu_usage(resource_meas * meas)
{
	pthread_mutex_lock(&meas->mutex);
	double cpu_usage = meas->current_cpu_usage;
	pthread_mutex_unlock(&meas->mutex);
	return cpu_usage;
}




double resource_meas_get_maximal_cpu_usage(resource_meas * meas)
{
	pthread_mutex_lock(&meas->mutex);
	double cpu_usage = meas->maximal_cpu_usage;
	pthread_mutex_unlock(&meas->mutex);
	return cpu_usage;
}




void resource_meas_do_measurement(resource_meas * meas)
{
	getrusage(RUSAGE_SELF, &meas->rusage_curr);

	timersub(&meas->rusage_curr.ru_utime, &meas->rusage_prev.ru_utime, &meas->user_cpu_diff);
	timersub(&meas->rusage_curr.ru_stime, &meas->rusage_prev.ru_stime, &meas->sys_cpu_diff);
	timeradd(&meas->user_cpu_diff, &meas->sys_cpu_diff, &meas->summary_cpu_usage);


	gettimeofday(&meas->timestamp_curr, NULL);
	timersub(&meas->timestamp_curr, &meas->timestamp_prev, &meas->timestamp_diff);


	meas->resource_usage = meas->summary_cpu_usage.tv_sec * 1000000 + meas->summary_cpu_usage.tv_usec;
	meas->meas_duration = meas->timestamp_diff.tv_sec * 1000000 + meas->timestamp_diff.tv_usec;

	meas->rusage_prev = meas->rusage_curr;
	meas->timestamp_prev = meas->timestamp_curr;

	pthread_mutex_lock(&meas->mutex);
	{
		meas->current_cpu_usage = meas->resource_usage * 100.0 / (meas->meas_duration * 1.0);
		// fprintf(stderr, "Curr = %06.4f, usage = %04ld, duration = %ld\n", meas->current_cpu_usage, meas->resource_usage, meas->meas_duration);
		if (meas->current_cpu_usage > meas->maximal_cpu_usage) {
			meas->maximal_cpu_usage = meas->current_cpu_usage;
		}
		/* Log the error "live" during test execution. This
		   will allow to pinpoint the faulty code faster. */
		if (meas->current_cpu_usage > LIBCW_TEST_MEAS_CPU_OK_THRESHOLD_PERCENT) {
			fprintf(stderr, "[EE] High current CPU usage: "CWTEST_CPU_FMT"\n", meas->current_cpu_usage);
		}
	}
	pthread_mutex_unlock(&meas->mutex);

#if 0
	fprintf(stderr, "user = %d.%d, system = %d.%d, total = %d.%d\n",
		user_cpu_diff.tv_sec, user_cpu_diff.tv_usec,
		sys_cpu_diff.tv_sec, sys_cpu_diff.tv_usec,
		summary_cpu_usage.tv_sec, summary_cpu_usage.tv_usec);
#endif

	return;
}




void cwtest_param_ranger_init(cwtest_param_ranger_t * ranger, int min, int max, int step, int initial_value)
{
	ranger->range_min = min;
	ranger->range_max = max;
	ranger->step = step;
	ranger->previous_value = initial_value;

	ranger->plateau_length = 0;

	if (initial_value == ranger->range_max) {
		ranger->direction = cwtest_param_ranger_direction_down; /* We can't go up, we are already at max. */
	} else {
		ranger->direction = cwtest_param_ranger_direction_up;
	}
}




bool cwtest_param_ranger_get_next(cwtest_param_ranger_t * ranger, int * new_value)
{
	if (ranger->interval_sec) {
		/* Generate new value only after specific time
		   interval has passed since last value was
		   returned. */
		const time_t now_timestamp = time(NULL);
		if (now_timestamp < ranger->previous_timestamp + ranger->interval_sec) {
			/* Don't generate new value yet. */
			return false;
		} else {
			ranger->previous_timestamp = now_timestamp;
			/* Go to code that will calculate new value. */
		}
	}


	/* TODO: when min or max is reached, we should stay on this
	   'plateau' for few calls. It may be useful to see how tested
	   objects operate with min/max values of parameters not just
	   for one period, but for few (10? 100? 1000?) periods. */

	int val = 0;
	if (ranger->direction == cwtest_param_ranger_direction_up) {
		val = ranger->previous_value + ranger->step;
		if (val >= ranger->range_max) {
			val = ranger->range_max;
			ranger->direction = cwtest_param_ranger_direction_down; /* Starting with next call, start returning decreasing values. */

			/* TODO: reached plateau here. Stay on the plateau for few calls. */
		}

	} else if (ranger->direction == cwtest_param_ranger_direction_down) {
		val = ranger->previous_value - ranger->step;
		if (val <= ranger->range_min) {
			val = ranger->range_min;
			ranger->direction = cwtest_param_ranger_direction_up; /* Starting with next call, start returning increasing values. */

			/* TODO: reached plateau here. Stay on the plateau for few calls. */
		}

	} else if (ranger->direction & cwtest_param_ranger_direction_plateau) {
		/* TODO: implement plateau. */
	} else {
		fprintf(stderr, "[EE] Unhandled direction %02x\n", ranger->direction);
		return false;
	}


	ranger->previous_value = val;

	fprintf(stderr, "[DD] Returning new parameter value %d\n", val);
	*new_value = val;

	return true;
}




void cwtest_param_ranger_with_interval_sec(cwtest_param_ranger_t * ranger, time_t interval_sec)
{
	if (interval_sec) {
		ranger->previous_timestamp = time(NULL);
		ranger->interval_sec = interval_sec;
	} else {
		ranger->previous_timestamp = 0;
		ranger->interval_sec = 0;
	}
}
