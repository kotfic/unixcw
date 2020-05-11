/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef H_LIBCW_UTILS
#define H_LIBCW_UTILS




#include "config.h"

#include <sys/time.h>




/* Microseconds in a second. */
enum { CW_USECS_PER_SEC = 1000000 };




int cw_timestamp_compare_internal(const struct timeval *earlier, const struct timeval *later);
int cw_timestamp_validate_internal(struct timeval *out_timestamp, const volatile struct timeval *in_timestamp);
void cw_usecs_to_timespec_internal(struct timespec *t, int usecs);




/**
   @brief Sleep for given amount of microseconds

   The function uses nanosleep(), and can handle incoming SIGALRM
   signals that cause regular nanosleep() to return. The function
   calls nanosleep() until all time specified by @param usecs has
   elapsed.

   The function may sleep a little longer than specified by @param
   usecs if it needs to spend some time handling SIGALRM signal. Other
   restrictions from nanosleep()'s man page also apply.

   @reviewed-on 2020-05-10
*/
void cw_usleep_internal(int usecs);




#if (defined(LIBCW_WITH_ALSA) || defined(LIBCW_WITH_PULSEAUDIO))
#include <stdbool.h>

bool cw_dlopen_internal(const char *name, void **handle);
#endif

void cw_finalization_schedule_internal(void);
void cw_finalization_cancel_internal(void);


#ifdef LIBCW_UNIT_TESTS
#define CW_STATIC_FUNC
#else
#define CW_STATIC_FUNC static
#endif




#endif /* #ifndef H_LIBCW_UTILS */
