/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2021  Kamil Ignacak (acerion@wp.pl)

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
   @file libcw_console.c

   @brief Console buzzer sound sink.
*/




#include <stdbool.h>




#include "config.h"
#include "libcw_console.h"
#include "libcw_debug.h"




#define MSG_PREFIX "libcw/console: "




extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;




#ifdef LIBCW_WITH_CONSOLE




#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#if   defined(HAVE_SYS_KD_H)
#       include <sys/kd.h>
#elif defined(HAVE_SYS_VTKD_H)
#       include <sys/vtkd.h>
#elif defined(HAVE_SYS_KBIO_H)
#       include <sys/kbio.h>
#endif

#if (defined(__unix__) || defined(unix)) && !defined(USG)
# include <sys/param.h>
#endif

#ifdef __FreeBSD__
#include <assert.h>
#include <dev/speaker/speaker.h>
#include <errno.h>
#include <fcntl.h>

/* We aren't supporting this ioctl yet. */
//#define LIBCW_CONSOLE_USE_SPKRTONE
#endif




#include "libcw_gen.h"
#include "libcw_utils.h"




#ifndef LIBCW_CONSOLE_USE_SPKRTONE
/* Clock tick rate used for KIOCSOUND console ioctls.  This value is taken
   from linux/include/asm-i386/timex.h, included here for portability. */
static const int KIOCSOUND_CLOCK_TICK_RATE = 1193180;
#endif



static void cw_console_close_sound_device_internal(cw_gen_t * gen);
static cw_ret_t cw_console_open_and_configure_sound_device_internal(cw_gen_t * gen, const cw_gen_config_t * gen_conf);
static cw_ret_t cw_console_write_tone_to_sound_device_internal(cw_gen_t * gen, const cw_tone_t * tone);

#ifdef LIBCW_CONSOLE_USE_SPKRTONE
static cw_ret_t cw_console_write_with_spkrtone_internal(cw_gen_t * gen, const cw_tone_t * tone);
#else
static cw_ret_t cw_console_write_with_kiocsound_internal(cw_gen_t * gen, const cw_tone_t * tone);
static cw_ret_t cw_console_kiocsound_wrapper_internal(cw_gen_t * gen, cw_key_value_t cw_value);
#endif




/**
   @brief Check if it is possible to open console buzzer output

   Function does a test opening and test writing to console buzzer device
   @p device_name, but it closes it before returning.

   The function tests that the given console buzzer device exists, and that it
   will accept the KIOCSOUND ioctl.  It unconditionally returns false
   on platforms that do no support the KIOCSOUND ioctl.

   Call to ioctl will fail if calling code doesn't have root privileges.

   This is the only place where we ask if KIOCSOUND is defined, so client
   code must call this function whenever it wants to use console output,
   as every other function called to perform console operations will
   happily assume that it is allowed to perform such operations.

   @reviewed 2020-11-14

   @param[in] device_name name of console buzzer device to be used; if NULL
   or empty then the function will use library-default device name.

   @return true if opening console output succeeded
   @return false if opening console output failed
*/
bool cw_is_console_possible(const char * device_name)
{
	/* TODO: revise logging of errors here. E.g. inability to open a file
	   is not an error, but a simple indication that console buzzer is
	   not accessible on this machine, and this should not be logged as
	   error. */

	char picked_device_name[LIBCW_SOUND_DEVICE_NAME_SIZE] = { 0 };
	cw_gen_pick_device_name_internal(device_name, CW_AUDIO_CONSOLE,
					 picked_device_name, sizeof (picked_device_name));

	int fd = open(picked_device_name, O_WRONLY);
	if (fd == -1) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open(%s): %s", picked_device_name, strerror(errno));
		return false;
	}

	errno = 0;
	int rv = 0;
#ifdef LIBCW_CONSOLE_USE_SPKRTONE
	tone_t spkrtone;
	spkrtone.frequency = 0;
	spkrtone.duration = 0;
	rv = ioctl(fd, SPKRTONE, &spkrtone);
#else
	rv = ioctl(fd, KIOCSOUND, 0);
#endif
	close(fd);
	if (rv == -1) {
		if (EPERM == errno) {
			/* Console device can be opened, even with WRONLY
			   perms, but, if you aren't root user, you can't
			   call ioctl()s on it, and - as a result - can't
			   generate sound on the device. */
			cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
				      MSG_PREFIX "ioctl(%s): %s (you probably should be running this as root)", picked_device_name, strerror(errno));
		} else {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
				      MSG_PREFIX "ioctl(%s): %s", picked_device_name, strerror(errno));
		}
		return false;
	} else {
		return true;
	}
}




/**
   @brief Open and configure console buzzer device stored in given generator

   The function doesn't check if ioctl(fd, KIOCSOUND, ...) works,
   the client code must use cw_is_console_possible() instead, prior
   to calling this function.

   @reviewed 2020-11-12

   @param[in] gen generator for which to open and configure buzzer device
   @param[in] gen_conf

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
static cw_ret_t cw_console_open_and_configure_sound_device_internal(cw_gen_t * gen, const cw_gen_config_t * gen_conf)
{
	if (gen->sound_device_is_open) {
		/* Ignore the call if the device is already open. */
		return CW_SUCCESS;
	}

	cw_gen_pick_device_name_internal(gen_conf->sound_device, gen->sound_system,
					 gen->picked_device_name, sizeof (gen->picked_device_name));

	gen->console.sound_sink_fd = open(gen->picked_device_name, O_WRONLY);
	if (-1 == gen->console.sound_sink_fd) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open(%s): '%s'", gen->picked_device_name, strerror(errno));
		return CW_FAILURE;
	} else {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
			      MSG_PREFIX "open successfully, console fd = %d", gen->console.sound_sink_fd);
	}

	/* It doesn't have any sense for console, but some code may depend
	   on non-zero value of sample rate. */
	gen->sample_rate = 44100;

	gen->sound_device_is_open = true;

	return CW_SUCCESS;
}




/**
   @brief Stop generating sound on console buzzer using given generator

   @reviewed 2020-09-19

   @param[in] gen generator to silence

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_console_silence_internal(cw_gen_t * gen)
{
	gen->console.cw_value = CW_KEY_VALUE_OPEN;

	int rv = 0;
#ifdef LIBCW_CONSOLE_USE_SPKRTONE
	tone_t spkrtone;
	spkrtone.frequency = 0; /* https://man.netbsd.org/speaker.4: "A frequency of zero is interpreted as a rest." */
	spkrtone.duration = 0;
	rv = ioctl(gen->console.sound_sink_fd, SPKRTONE, &spkrtone);
#else
	rv = cw_console_kiocsound_wrapper_internal(gen, gen->console.cw_value);
#endif

	if (0 == rv) {
		return CW_SUCCESS;
	} else {
		return CW_FAILURE;
	}
}




/**
   @brief Close console buzzer device stored in given generator

   @reviewed 2020-07-14

   @param[in] gen generator for which to close its sound device
*/
static void cw_console_close_sound_device_internal(cw_gen_t * gen)
{
	close(gen->console.sound_sink_fd);
	gen->console.sound_sink_fd = -1;
	gen->sound_device_is_open = false;

	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "console closed");

	return;
}




/**
   @brief Write to console buzzer configured and opened for generator

   Function behaving like a device, to which one does a blocking write.
   It generates sound with parameters (frequency and duration) specified
   in @p tone.

   After playing X microseconds of tone the function returns. The function is
   intended to behave like a blocking write() function.

   @reviewed 2020-09-18

   @param[in] gen generator to use to play a tone
   @param[in] tone tone to play with generator

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
static cw_ret_t cw_console_write_tone_to_sound_device_internal(cw_gen_t * gen, const cw_tone_t * tone)
{
	assert (gen);
	assert (gen->sound_system == CW_AUDIO_CONSOLE);
	assert (tone->duration >= 0); /* TODO: shouldn't the condition be "tone->duration > 0"? */

#ifdef LIBCW_CONSOLE_USE_SPKRTONE
	return cw_console_write_with_spkrtone_internal(gen, tone);
#else
	return cw_console_write_with_kiocsound_internal(gen, tone);
#endif
}




#ifdef LIBCW_CONSOLE_USE_SPKRTONE
/**
   @brief Generate a sound using console buzzer and SPKRTONE ioctl

   The function calls the SPKRTONE ioctl to generate a particular
   tone. The ioctl generates tone of specified duration.

   The function will produce a silence if generator's volume is zero.

   @reviewed 2020-09-19

   @param[in] gen generator
   @param[in] tone tone to generate

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
static cw_ret_t cw_console_write_with_spkrtone_internal(cw_gen_t * gen, const cw_tone_t * tone)
{
	/* TODO: according to this man page: https://man.netbsd.org/speaker.4
	   it *may* be possible to control volume of tone on console
	   device on a BSD system. */

  	tone_t spkrtone = { 0 };
	if (0 == gen->volume_percent) {
		spkrtone.frequency = 0; /* https://man.netbsd.org/speaker.4: "A frequency of zero is interpreted as a rest." */
	} else {
		spkrtone.frequency = tone->frequency;
		fprintf(stderr, "%d\n", tone->frequency);
	}
	spkrtone.duration = tone->duration / (10 * 1000); /* .duration in tone_t is "in 1/100ths of a second" (https://man.netbsd.org/speaker.4). */
	const int rv = ioctl(gen->console.sound_sink_fd, SPKRTONE, &spkrtone);
	if (0 != rv) {
		return CW_FAILURE;
	} else {
		return CW_SUCCESS;
	}
}




#else




/**
   @brief Start generating a sound using console buzzer and KIOCSOUND ioctl

   The function calls the KIOCSOUND ioctl to start a particular tone.
   Once started, the console tone generation needs no maintenance.

   The function only initializes generation, you have to do another
   function call to change the tone generated.

   @reviewed 2020-09-19

   @param[in] gen generator
   @param[in] tone tone to generate

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
static cw_ret_t cw_console_write_with_kiocsound_internal(cw_gen_t * gen, const cw_tone_t * tone)
{
	const cw_key_value_t cw_value = tone->frequency > 0 && gen->volume_percent > 0
		? CW_KEY_VALUE_CLOSED : CW_KEY_VALUE_OPEN;

	if (cw_value == gen->console.cw_value) {
		/* Simulate blocking write() and let buzzer keep doing what it is doing. */
		cw_usleep_internal(tone->duration);
		return CW_SUCCESS;
	} else {
		gen->console.cw_value = cw_value;
	}

	const int rv = cw_console_kiocsound_wrapper_internal(gen, gen->console.cw_value);
	/* Simulate blocking write() because cw_console_kiocsound_wrapper_internal() is not blocking. */
	cw_usleep_internal(tone->duration);

	cw_ret_t cwret = CW_SUCCESS;
	switch (tone->slope_mode) {
	case CW_SLOPE_MODE_FALLING_SLOPE:
		/* Falling slope causes the console to produce sound, so at
		   the end of the slope - the console is left in "generate"
		   state. We have to explicitly stop generating sound at
		   the end of falling slope. */
	case CW_SLOPE_MODE_STANDARD_SLOPES:
		/* It seems that it's a good idea to turn off the console
		   buzzer after playing standard tone. In theory the console
		   buzzer would be turned off by "silence" tone coming right
		   after an audible tone, but in practice it may not be
		   always so.*/
		cwret = cw_console_silence_internal(gen);
		break;
	case CW_SLOPE_MODE_NO_SLOPES: /* TODO: do we handle CW_SLOPE_MODE_NO_SLOPES correctly? */
	case CW_SLOPE_MODE_RISING_SLOPE:
	default:
		/* No change to state of buzzer. No failure. */
		cwret = CW_SUCCESS;
		break;
	}

	if (0 == rv && CW_SUCCESS == cwret) {
		return CW_SUCCESS;
	} else {
		return CW_FAILURE;
	}
}




/**
   @brief Wrapper for KIOCSOUND ioctl

   The function *does not* block and *does not* sleep for any duration
   of time. Simulating blocking write to a sound sink should be done
   by caller of the function. This is done to ensure that the API is
   simple and list of arguments is short.

   The function does not filter out consecutive calls with identical
   parameters. This is left to the caller. This is done to ensure that
   when the function is called to silence a console, the console will
   be guaranteed to be silenced.

   The function will produce a silence if generator's volume is zero.

   @reviewed 2020-09-19

   @param[in] gen generator to use to generate a sound with KIOCSOUND ioctl
   @param[in] cw_value value dictating what to do with sound sink: generate a tone or produce silence

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_console_kiocsound_wrapper_internal(cw_gen_t * gen, cw_key_value_t cw_value)
{
	/* Calculate the correct argument for KIOCSOUND.  There's nothing we
	   can do to control the volume, but if we find the volume is set to
	   zero, the one thing we can do is to just turn off tones.  A bit
	   crude, but perhaps just slightly better than doing nothing. */
	int argument = 0;
	if (gen->volume_percent > 0 && CW_KEY_VALUE_CLOSED == cw_value) {
		argument = KIOCSOUND_CLOCK_TICK_RATE / gen->frequency;
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "KIOCSOUND arg = %d (current cw value: %s, frequency: %d Hz, volume: %d %%)",
		      argument,
		      CW_KEY_VALUE_CLOSED == cw_value ? "closed" : "open",
		      gen->frequency, gen->volume_percent);

	/* TODO: take a look at KDMKTONE ioctl argument. */
	if (0 != ioctl(gen->console.sound_sink_fd, KIOCSOUND, argument)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctl KIOCSOUND: '%s'", strerror(errno));
		return CW_FAILURE;
	} else {
		return CW_SUCCESS;
	}
}
#endif /* #ifdef LIBCW_CONSOLE_USE_SPKRTONE */




/**
   @brief Configure given @p gen variable to work with Console sound system

   This function only initializes @p gen by setting some of its members. It
   doesn't interact with sound system (doesn't try to open or configure it).

   @reviewed 2020-11-12

   @param[in,out] gen generator structure to initialize

   @return CW_SUCCESS
*/
cw_ret_t cw_console_init_gen_internal(cw_gen_t * gen)
{
	assert (gen);

	gen->sound_system                    = CW_AUDIO_CONSOLE;
	gen->open_and_configure_sound_device = cw_console_open_and_configure_sound_device_internal;
	gen->close_sound_device              = cw_console_close_sound_device_internal;
	gen->write_tone_to_sound_device      = cw_console_write_tone_to_sound_device_internal;

	return CW_SUCCESS;
}




#else /* #ifdef LIBCW_WITH_CONSOLE */




bool cw_is_console_possible(__attribute__((unused)) const char * device_name)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "This sound system has been disabled during compilation");
	return false;
}




cw_ret_t cw_console_init_gen_internal(__attribute__((unused)) cw_gen_t * gen)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "This sound system has been disabled during compilation");
	return CW_FAILURE;
}




cw_ret_t cw_console_silence_internal(__attribute__((unused)) cw_gen_t * gen)
{
	return CW_FAILURE;
}




#endif /* #ifdef LIBCW_WITH_CONSOLE */
