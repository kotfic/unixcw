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



static void resource_meas_do_measurement(resource_meas * meas);
static void * resouce_meas_thread(void * arg);



void * resouce_meas_thread(void * arg)
{
	resource_meas * meas = (resource_meas *) arg;
	while (1) {
		resource_meas_do_measurement(meas);
		usleep(200 * 1000);
	}

	return NULL;
}




void resource_meas_start(resource_meas * meas)
{
	memset(meas, 0, sizeof (*meas));
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




double resource_meas_get_cpu_usage(resource_meas * meas)
{
	pthread_mutex_lock(&meas->mutex);
	double cpu_usage = meas->cpu_usage;
	pthread_mutex_unlock(&meas->mutex);
	return cpu_usage;
}




void resource_meas_do_measurement(resource_meas * meas)
{
	getrusage(RUSAGE_SELF, &meas->r_curr);

	timersub(&meas->r_curr.ru_utime, &meas->r_prev.ru_utime, &meas->u_diff);
	timersub(&meas->r_curr.ru_stime, &meas->r_prev.ru_stime, &meas->s_diff);
	timeradd(&meas->u_diff, &meas->s_diff, &meas->sum);


	gettimeofday(&meas->timestamp_curr, NULL);
	timersub(&meas->timestamp_curr, &meas->timestamp_prev, &meas->timestamp_diff);


	meas->resource_usage = meas->sum.tv_sec * 1000000 + meas->sum.tv_usec;
	meas->meas_duration = meas->timestamp_diff.tv_sec * 1000000 + meas->timestamp_diff.tv_usec;

	meas->r_prev = meas->r_curr;
	meas->timestamp_prev = meas->timestamp_curr;

	pthread_mutex_lock(&meas->mutex);
	meas->cpu_usage = meas->resource_usage * 100.0 / meas->meas_duration;
	pthread_mutex_unlock(&meas->mutex);

#if 0
	fprintf(stderr, "user = %d.%d, system = %d.%d, total = %d.%d\n",
		u_diff.tv_sec, u_diff.tv_usec,
		s_diff.tv_sec, s_diff.tv_usec,
		sum.tv_sec, sum.tv_usec);
#endif

	return;
}
