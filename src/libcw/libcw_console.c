/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2020  Kamil Ignacak (acerion@wp.pl)

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
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#endif




#include "libcw_gen.h"
#include "libcw_utils.h"





/* Clock tick rate used for KIOCSOUND console ioctls.  This value is taken
   from linux/include/asm-i386/timex.h, included here for portability. */
static const int KIOCSOUND_CLOCK_TICK_RATE = 1193180;

static void cw_console_close_sound_device_internal(cw_gen_t * gen);
static cw_ret_t cw_console_open_and_configure_sound_device_internal(cw_gen_t * gen);
static cw_ret_t cw_console_write_tone_to_sound_device_internal(cw_gen_t * gen, cw_tone_t * tone);
static cw_ret_t cw_console_write_low_level_internal(cw_gen_t * gen, bool state);




/**
   @brief Check if it is possible to open console buzzer output

   Function does a test opening and test writing to console buzzer device @p
   device_name, but it closes it before returning.

   The function tests that the given console buzzer device exists, and that it
   will accept the KIOCSOUND ioctl.  It unconditionally returns false
   on platforms that do no support the KIOCSOUND ioctl.

   Call to ioctl will fail if calling code doesn't have root privileges.

   This is the only place where we ask if KIOCSOUND is defined, so client
   code must call this function whenever it wants to use console output,
   as every other function called to perform console operations will
   happily assume that it is allowed to perform such operations.

   @reviewed 2020-07-14

   @param[in] device_name name of console buzzer device to be used; if NULL
   then the function will use library-default device name.

   @return true if opening console output succeeded
   @return false if opening console output failed
*/
bool cw_is_console_possible(const char * device_name)
{
	/* TODO: revise logging of errors here. E.g. inability to open a file
	   is not an error, but a simple indication that console buzzer is
	   not accessible on this machine, and this should not be logged as
	   error. */

	/* No need to allocate space for device path, just a
	   pointer (to a memory allocated somewhere else by
	   someone else) will be sufficient in local scope. */
	const char * dev = device_name ? device_name : CW_DEFAULT_CONSOLE_DEVICE;

	int fd = open(dev, O_WRONLY);
	if (fd == -1) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open(%s): %s", dev, strerror(errno));
		return false;
	}

	errno = 0;
	int rv = ioctl(fd, KIOCSOUND, 0);
	close(fd);
	if (rv == -1) {
		if (EPERM == errno) {
			/* Console device can be opened, even with WRONLY
			   perms, but, if you aren't root user, you can't
			   call ioctl()s on it, and - as a result - can't
			   generate sound on the device. */
			cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
				      MSG_PREFIX "ioctl(%s): %s (you probably should be running this as root)", dev, strerror(errno));
		} else {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
				      MSG_PREFIX "ioctl(%s): %s", dev, strerror(errno));
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

   You must use cw_gen_set_sound_device_internal() before calling
   this function. Otherwise generator @p gen won't know which device to open.

   @reviewed 2020-07-14

   @param[in] gen generator for which to open and configure buzzer device

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
static cw_ret_t cw_console_open_and_configure_sound_device_internal(cw_gen_t * gen)
{
	assert (gen->sound_device);

	if (gen->sound_device_is_open) {
		/* Ignore the call if the console device is already open. */
		return CW_SUCCESS;
	}

	gen->sound_sink_fd = open(gen->sound_device, O_WRONLY);
	if (-1 == gen->sound_sink_fd) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open(%s): '%s'", gen->sound_device, strerror(errno));
		return CW_FAILURE;
	} else {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
			      MSG_PREFIX "open successfully, console fd = %d", gen->sound_sink_fd);
	}

	/* It doesn't have any sense for console, but some code may depend
	   on non-zero value of sample rate. */
	gen->sample_rate = 44100;

	gen->sound_device_is_open = true;

	return CW_SUCCESS;
}




/**
   @brief Stop generating sound on console buzzer using given generator

   @reviewed 2020-07-14

   @param[in] gen generator to silence
*/
void cw_console_silence(cw_gen_t * gen)
{
	ioctl(gen->sound_sink_fd, KIOCSOUND, 0);
	return;
}




/**
   @brief Close console buzzer device stored in given generator

   @reviewed 2020-07-14

   @param[in] gen generator for which to close its sound device
*/
static void cw_console_close_sound_device_internal(cw_gen_t * gen)
{
	close(gen->sound_sink_fd);
	gen->sound_sink_fd = -1;
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

   @reviewed 2020-07-16

   @param[in] gen generator to use to play a tone
   @param[in] tone tone to play with generator

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
static cw_ret_t cw_console_write_tone_to_sound_device_internal(cw_gen_t * gen, cw_tone_t * tone)
{
	assert (gen);
	assert (gen->sound_system == CW_AUDIO_CONSOLE);
	assert (tone->duration >= 0); /* TODO: shouldn't the condition be "tone->duration > 0"? */

	cw_ret_t cw_ret_1 = cw_console_write_low_level_internal(gen, (bool) tone->frequency);
	cw_usleep_internal(tone->duration);

	cw_ret_t cw_ret_2 = CW_FAILURE;
	if (tone->slope_mode == CW_SLOPE_MODE_FALLING_SLOPE) {
		/* Falling slope causes the console to produce sound, so at
		   the end of the slope - the console is left in "generate"
		   state. We have to explicitly stop generating sound at
		   the end of falling slope. */
		cw_ret_2 = cw_console_write_low_level_internal(gen, false);
	} else if (tone->slope_mode == CW_SLOPE_MODE_STANDARD_SLOPES) {
		/* It seems that it's a good idea to turn off the console
		   buzzer after playing standard tone. In theory the console
		   buzzer would be turned off by "silence" tone coming right
		   after an audible tone, but in practice it may not be
		   always so.*/
		cw_ret_2 = cw_console_write_low_level_internal(gen, false);
	} else {
		/* No change to state of buzzer. No failure. */
		/* TODO: how to handle CW_SLOPE_MODE_NO_SLOPES? */
		cw_ret_2 = CW_SUCCESS;
	}

	if (CW_SUCCESS == cw_ret_1 && CW_SUCCESS == cw_ret_2) {
		return CW_SUCCESS;
	} else {
		return CW_FAILURE;
	}
}




/**
   @brief Start generating a sound using console buzzer

   The function calls the KIOCSOUND ioctl to start a particular tone.
   Once started, the console tone generation needs no maintenance.

   The function only initializes generation, you have to do another
   function call to change the tone generated.

   @reviewed 2020-07-16

   @param[in] gen generator
   @param[in] state flag deciding if a sound should be generated (logical true) or not (logical false)

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
static cw_ret_t cw_console_write_low_level_internal(cw_gen_t * gen, bool state)
{
	static bool local_state = false; /* TODO: this won't work well with multi-generator setup. */
	if (local_state == state) {
		return CW_SUCCESS;
	} else {
		local_state = state;
	}

	/* TODO: take a look at KDMKTONE ioctl argument. */

	/* Calculate the correct argument for KIOCSOUND.  There's nothing we
	   can do to control the volume, but if we find the volume is set to
	   zero, the one thing we can do is to just turn off tones.  A bit
	   crude, but perhaps just slightly better than doing nothing. */
	int argument = 0;
	if (gen->volume_percent > 0 && local_state) {
		argument = KIOCSOUND_CLOCK_TICK_RATE / gen->frequency;
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "KIOCSOUND arg = %d (switch: %d, frequency: %d Hz, volume: %d %%)",
		      argument, local_state, gen->frequency, gen->volume_percent);

	if (-1 == ioctl(gen->sound_sink_fd, KIOCSOUND, argument)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctl KIOCSOUND: '%s'", strerror(errno));
		return CW_FAILURE;
	} else {
		return CW_SUCCESS;
	}
}




/**
   @brief Configure given @p gen variable to work with Console sound system

   This function only sets some fields of @p gen (variables and function
   pointers). It doesn't interact with Console sound system.

   @reviewed 2020-07-16

   @param[in] gen generator structure in which to fill some fields
   @param[in] device_name name of Console device to use

   @return CW_SUCCESS
*/
cw_ret_t cw_console_fill_gen_internal(cw_gen_t * gen, const char * device_name)
{
	assert (gen);

	gen->sound_system = CW_AUDIO_CONSOLE;
	cw_gen_set_sound_device_internal(gen, device_name);

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




cw_ret_t cw_console_fill_gen_internal(__attribute__((unused)) cw_gen_t * gen, __attribute__((unused)) const char * device_name)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "This sound system has been disabled during compilation");
	return CW_FAILURE;
}




static cw_ret_t cw_console_write_tone_to_sound_device_internal(__attribute__((unused)) cw_gen_t * gen, __attribute__((unused)) cw_tone_t * tone)
{
	return CW_FAILURE;
}




void cw_console_silence(__attribute__((unused)) cw_gen_t * gen)
{
	return;
}




#endif /* #ifdef LIBCW_WITH_CONSOLE */
