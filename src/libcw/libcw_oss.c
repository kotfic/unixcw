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
   @file libcw_oss.c

   @brief OSS sound system.
*/




#include <stdbool.h>




#include "config.h"
#include "libcw_debug.h"
#include "libcw_oss.h"




#define MSG_PREFIX "libcw/oss: "




extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;




#ifdef LIBCW_WITH_OSS




#include <assert.h>
#include <ctype.h>
#include <dlfcn.h> /* dlopen() and related symbols */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#if   defined(HAVE_SYS_SOUNDCARD_H)
#       include <sys/soundcard.h>
#elif defined(HAVE_SOUNDCARD_H)
#       include <soundcard.h>
#else
#
#endif




#include "libcw_gen.h"




extern const unsigned int cw_supported_sample_rates[];




/* Conditional compilation flags. */
#define CW_OSS_SET_FRAGMENT       1  /* ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &param) */
#define CW_OSS_SET_POLICY         0  /* ioctl(fd, SNDCTL_DSP_POLICY, &param) */

/* Constants specific to OSS sound system configuration. */
static const unsigned int CW_OSS_SETFRAGMENT = 7U;              /* Sound fragment size, 2^7 samples. */
static const int CW_OSS_SAMPLE_FORMAT = AFMT_S16_NE;  /* Sound format AFMT_S16_NE = signed 16 bit, native endianess; LE = Little endianess. */

static cw_ret_t cw_oss_open_device_ioctls_internal(int fd, unsigned int * sample_rate);
static cw_ret_t cw_oss_get_version_internal(int fd, cw_oss_version_t * version);
static cw_ret_t cw_oss_write_buffer_to_sound_device_internal(cw_gen_t * gen);
static cw_ret_t cw_oss_open_and_configure_sound_device_internal(cw_gen_t * gen, const cw_gen_config_t * gen_conf);
static void cw_oss_close_sound_device_internal(cw_gen_t * gen);




/**
   @brief Check if it is possible to open OSS output with given device name

   Function does a test opening and test configuration of OSS output,
   but it closes it before returning.

   @reviewed 2020-07-19

   @param[in] device_name name of OSS device to be used; if NULL then the
   function will use library-default device name.

   @return true if opening OSS output succeeded
   @return false if opening OSS output failed
*/
bool cw_is_oss_possible(const char * device_name)
{
	const char * dev = device_name ? device_name : CW_DEFAULT_OSS_DEVICE;
	/* Open the given soundcard device file, for write only. */
	int soundcard = open(dev, O_WRONLY);
	if (soundcard == -1) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
			      MSG_PREFIX "is possible: open(%s): '%s'", dev, strerror(errno));
		return false;
	}

	{
		cw_oss_version_t version = { 0 };
		cw_ret_t cw_ret = cw_oss_get_version_internal(soundcard, &version);
		if (cw_ret == CW_FAILURE) {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
				      MSG_PREFIX "is possible: can't get OSS version '%s'", strerror(errno));
			close(soundcard);
			return false;
		} else {
			cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
				      MSG_PREFIX "is possible: OSS version %u.%u.%u", version.x, version.y, version.z);
		}
	}

	/*
	  http://manuals.opensound.com/developer/OSS_GETVERSION.html:
	  about OSS_GETVERSION ioctl:
	  "This ioctl call returns the version number OSS API used in
	  the current system. Applications can use this information to
	  find out if the OSS version is new enough to support the
	  features required by the application. However this methods
	  should be used with great care. Usually it's recommended
	  that applications check availability of each ioctl() by
	  calling it and by checking if the call returned errno=EINVAL."

	  So, we call all necessary ioctls to be 100% sure that all
	  needed features are available. cw_oss_open_device_ioctls_internal()
	  doesn't specifically look for EINVAL, it only checks return
	  values from ioctl() and returns CW_FAILURE if one of ioctls()
	  returns -1. */
	unsigned int dummy = 0;
	cw_ret_t cw_ret = cw_oss_open_device_ioctls_internal(soundcard, &dummy);
	close(soundcard);
	if (cw_ret != CW_SUCCESS) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "is possible: one or more OSS ioctl() calls failed");
		return false;
	} else {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
			      MSG_PREFIX "is possible: OSS is possible");
		return true;
	}
}




/**
   @brief Configure given @p gen variable to work with OSS sound system

   This function only sets some fields of @p gen (variables and function
   pointers). It doesn't interact with OSS sound system.

   @reviewed 2020-07-19

   @param[in] gen generator structure in which to fill some fields
   @param[in] device_name name of OSS device to use

   @return CW_SUCCESS
*/
cw_ret_t cw_oss_fill_gen_internal(cw_gen_t * gen, const char * device_name)
{
	assert (gen);

	gen->sound_system = CW_AUDIO_OSS;
	cw_gen_set_sound_device_internal(gen, device_name);

	gen->open_and_configure_sound_device = cw_oss_open_and_configure_sound_device_internal;
	gen->close_sound_device              = cw_oss_close_sound_device_internal;
	gen->write_buffer_to_sound_device    = cw_oss_write_buffer_to_sound_device_internal;

	return CW_SUCCESS;
}




/**
   @brief Write generated samples to OSS sound system device configured and opened for generator

   @reviewed 2020-07-19

   @param[in] gen generator that will write to sound device

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_oss_write_buffer_to_sound_device_internal(cw_gen_t * gen)
{
	assert (gen);
	assert (gen->sound_system == CW_AUDIO_OSS);

	size_t n_bytes = sizeof (gen->buffer[0]) * gen->buffer_n_samples;
	ssize_t rv = write(gen->oss_data.sound_sink_fd, gen->buffer, n_bytes);
	if (rv != (ssize_t) n_bytes) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "write: %s", strerror(errno));
		return CW_FAILURE;
	}
	// cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO, MSG_PREFIX "written %d samples", gen->buffer_n_samples);

	return CW_SUCCESS;
}




/**
   @brief Open and configure OSS handle stored in given generator

   You must use cw_gen_set_sound_device_internal() before calling
   this function. Otherwise generator @p gen won't know which device to open.

   @reviewed 2020-07-19

   @param[in] gen generator for which to open and configure sound system handle
   @param[in] gen_conf

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
cw_ret_t cw_oss_open_and_configure_sound_device_internal(cw_gen_t * gen, __attribute__((unused)) const cw_gen_config_t * gen_conf)
{
	/* TODO: there seems to be some redundancy between
	   cw_oss_open_and_configure_sound_device_internal() and is_possible() function. */

	/* Open the given soundcard device file, for write only. */
	gen->oss_data.sound_sink_fd = open(gen->sound_device, O_WRONLY);
	if (-1 == gen->oss_data.sound_sink_fd) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: open(%s): '%s'", gen->sound_device, strerror(errno));
		return CW_FAILURE;
	}

	cw_ret_t cw_ret = cw_oss_open_device_ioctls_internal(gen->oss_data.sound_sink_fd, &gen->sample_rate);
	if (cw_ret != CW_SUCCESS) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: one or more OSS ioctl() calls failed");
		close(gen->oss_data.sound_sink_fd);
		return CW_FAILURE;
	}


	int size = 0;
	/* Get fragment size in bytes, may be different than requested
	   with ioctl(..., SNDCTL_DSP_SETFRAGMENT), and, in particular,
	   can be different than 2^N. */
	/* Don't let clang-tidy report warning about signed. To fix
	   the warning we would have to introduce casting, and that
	   would introduce runtime warnings in dmesg on FreeBSD. */
	/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
	int rv = ioctl(gen->oss_data.sound_sink_fd, SNDCTL_DSP_GETBLKSIZE, &size); 
	if (-1 == rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: ioctl(SNDCTL_DSP_GETBLKSIZE): '%s'", strerror(errno));
		close(gen->oss_data.sound_sink_fd);
		return CW_FAILURE;
	}

	if ((size & 0x0000ffff) != (1U << CW_OSS_SETFRAGMENT)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: OSS fragment size not set, %d", size);
		close(gen->oss_data.sound_sink_fd);
		/* FIXME */
		return CW_FAILURE;
	} else {
		/* TODO: are we reporting size correctly? Shouldn't it be
		   "(size & 0x0000ffff)" or maybe even "2^(size & 0x0000ffff)"? */
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
			      MSG_PREFIX "open: OSS fragment size = %d", size);
	}
	gen->buffer_n_samples = size;


	cw_oss_get_version_internal(gen->oss_data.sound_sink_fd, &gen->oss_data.version);

	/* Mark sound sink as now open for business. */
	gen->sound_device_is_open = true;

#if CW_DEV_RAW_SINK
	gen->dev_raw_sink = open("/tmp/cw_file.oss.raw", O_WRONLY | O_TRUNC | O_NONBLOCK);
#endif

	return CW_SUCCESS;
}




/**
   @brief Perform all necessary ioctl calls on opened OSS file descriptor

   Wrapper function for ioctl calls that need to be done when configuring
   file descriptor @p fd for OSS playback.

   @reviewed 2020-07-19

   @param[in] fd file descriptor of open OSS file;
   @param[out] sample_rate sample rate configured by ioctl calls

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
cw_ret_t cw_oss_open_device_ioctls_internal(int fd, unsigned int * sample_rate)
{
	int parameter = 0; /* Ignored. */
	/* Don't let clang-tidy report warning about signed. To fix
	   the warning we would have to introduce casting, and that
	   would introduce runtime warnings in dmesg on FreeBSD. */
	/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
	if (-1 == ioctl(fd, SNDCTL_DSP_SYNC, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_SYNC): '%s'", strerror(errno));
		return CW_FAILURE;
	}
#if 0
	/*
	   This ioctl call failed on FreeBSD 10, which resulted in
	   libcw failing to open OSS device. A bit of digging on the
	   web revealed this:

	   OSS4:
	   http://manuals.opensound.com/developer/SNDCTL_DSP_POST.html:
	   "This ioctl call is provided for compatibility with older
	   applications. It has no practical purpose and should in no
	   case be used in new applications."
	*/

	parameter = 0; /* Ignored. */
	if (-1 == ioctl(fd, SNDCTL_DSP_POST, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_POST): '%s'", strerror(errno));
		return CW_FAILURE;
	}
#endif
	/* Set the sample format. */
	parameter = CW_OSS_SAMPLE_FORMAT;
	/* Don't cast second argument of ioctl() to int, because you will get
	   this warning in dmesg (found on FreeBSD 12.1):
	   "ioctl sign-extension ioctl ffffffffc0045005" */
	/* Don't let clang-tidy report warning about signed. To fix
	   the warning we would have to introduce casting, and that
	   would introduce runtime warnings in dmesg on FreeBSD. */
	/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
	if (-1 == ioctl(fd, SNDCTL_DSP_SETFMT, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_SETFMT): '%s'", strerror(errno));
		return CW_FAILURE;
	}
	if (parameter != CW_OSS_SAMPLE_FORMAT) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: sample format not supported");
		/* TODO: can't we try some other sample format? */
		return CW_FAILURE;
	}

	/* Set up mono/stereo mode. */
	parameter = CW_AUDIO_CHANNELS;
	/* Don't cast second argument of ioctl() to int, because you will get
	   this warning in dmesg (found on FreeBSD 12.1):
	   "ioctl sign-extension ioctl ffffffffc0045006" */
	/* Don't let clang-tidy report warning about signed. To fix
	   the warning we would have to introduce casting, and that
	   would introduce runtime warnings in dmesg on FreeBSD. */
	/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
	if (-1 == ioctl(fd, SNDCTL_DSP_CHANNELS, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_CHANNELS): '%s'", strerror(errno));
		return CW_FAILURE;
	}
	if (parameter != CW_AUDIO_CHANNELS) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: number of channels not supported");
		return CW_FAILURE;
	}

	/* Set up a standard sampling rate based on the notional correct
	   value, and retain the one we actually get. */
	unsigned int rate = 0;
	bool success = false;
	for (int i = 0; cw_supported_sample_rates[i]; i++) {
		rate = cw_supported_sample_rates[i];
		/* Don't cast second argument of ioctl() to int, because you will get
		   this warning in dmesg (found on FreeBSD 12.1):
		   "ioctl sign-extension ioctl ffffffffc0045002" */
		/* Don't let clang-tidy report warning about signed. To fix
		   the warning we would have to introduce casting, and that
		   would introduce runtime warnings in dmesg on FreeBSD. */
		/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
		if (0 == ioctl(fd, SNDCTL_DSP_SPEED, &rate)) {
			if (rate != cw_supported_sample_rates[i]) {
				cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING, MSG_PREFIX "ioctls: imprecise sample rate:");
				cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING, MSG_PREFIX "ioctls: asked for: %u", cw_supported_sample_rates[i]);
				cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING, MSG_PREFIX "ioctls: got:       %u", rate);
			}
			success = true;
			break;
		}
	}

	if (!success) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_SPEED): '%s'", strerror(errno));
		return CW_FAILURE;
	} else {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
			      MSG_PREFIX "OSS sample rate = %u\n", rate);
		*sample_rate = rate;
	}


	audio_buf_info buff;
	/* Don't let clang-tidy report warning about signed. To fix
	   the warning we would have to introduce casting, and that
	   would introduce runtime warnings in dmesg on FreeBSD. */
	/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
	if (-1 == ioctl(fd, SNDCTL_DSP_GETOSPACE, &buff)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_GETOSPACE): '%s'", strerror(errno));
		return CW_FAILURE;
	} else {
		/*
		fprintf(stderr, "before:\n");
		fprintf(stderr, "buff.fragments = %d\n", buff.fragments);
		fprintf(stderr, "buff.fragsize = %d\n", buff.fragsize);
		fprintf(stderr, "buff.bytes = %d\n", buff.bytes);
		fprintf(stderr, "buff.fragstotal = %d\n", buff.fragstotal);
		*/
	}


#if CW_OSS_SET_FRAGMENT
	/*
	 * Live a little dangerously, by trying to set the fragment size of the
	 * card.  We'll try for a relatively short fragment of 128 bytes.  This
	 * gives us a little better granularity over the amounts of audio data
	 * we write periodically to the soundcard output buffer.  We may not get
	 * the requested fragment size, and may be stuck with the default.  The
	 * argument has the format 0xMMMMSSSS - fragment size is 2^SSSS, and
	 * setting 0x7fff for MMMM allows as many fragments as the driver can
	 * support.
	 */
	/* parameter = 0x7fff << 16 | CW_OSS_SETFRAGMENT; */
	parameter = 0x0032U << 16U | CW_OSS_SETFRAGMENT;

	/* Don't cast second argument of ioctl() to int, because you will get
	   this warning in dmesg (found on FreeBSD 12.1):
	   "ioctl sign-extension ioctl ffffffffc004500a" */
	/* Don't let clang-tidy report warning about signed. To fix
	   the warning we would have to introduce casting, and that
	   would introduce runtime warnings in dmesg on FreeBSD. */
	/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
	if (-1 == ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_SETFRAGMENT): '%s'", strerror(errno));
		return CW_FAILURE;
	}
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "ioctls: fragment size is 2^%d = %d", parameter & 0x0000ffff, 2 << ((parameter & 0x0000ffffU) - 1));

	/* Query fragment size just to get the driver buffers set. */
	/* Don't let clang-tidy report warning about signed. To fix
	   the warning we would have to introduce casting, and that
	   would introduce runtime warnings in dmesg on FreeBSD. */
	/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
	if (-1 == ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_GETBLKSIZE): '%s'", strerror(errno));
		return CW_FAILURE;
	}

	if (parameter != (1U << CW_OSS_SETFRAGMENT)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: OSS fragment size not set, %d", parameter);
	}

#endif
#if CW_OSS_SET_POLICY
	parameter = 5; /* TODO: what does this value mean? */
	if (-1 == ioctl(fd, SNDCTL_DSP_POLICY, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_POLICY): '%s'", strerror(errno));
		return CW_FAILURE;
	}
#endif

	/* Don't let clang-tidy report warning about signed. To fix
	   the warning we would have to introduce casting, and that
	   would introduce runtime warnings in dmesg on FreeBSD. */
	/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
	if (-1 == ioctl(fd, SNDCTL_DSP_GETOSPACE, &buff)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_GETOSPACE): '%s'", strerror(errno));
		return CW_FAILURE;
	} else {
		/*
		fprintf(stderr, "after:\n");
		fprintf(stderr, "buff.fragments = %d\n", buff.fragments);
		fprintf(stderr, "buff.fragsize = %d\n", buff.fragsize);
		fprintf(stderr, "buff.bytes = %d\n", buff.bytes);
		fprintf(stderr, "buff.fragstotal = %d\n", buff;3R.fragstotal);
		*/
	}

	return CW_SUCCESS;
}




/**
   @brief Close OSS device stored in given generator

   @reviewed 2020-07-19

   @param[in] gen generator for which to close its sound device
*/
void cw_oss_close_sound_device_internal(cw_gen_t * gen)
{
	close(gen->oss_data.sound_sink_fd);
	gen->oss_data.sound_sink_fd = -1;
	gen->sound_device_is_open = false;

#if CW_DEV_RAW_SINK
	if (gen->dev_raw_sink != -1) {
		close(gen->dev_raw_sink);
		gen->dev_raw_sink = -1;
	}
#endif
	return;
}




/**
   @brief Get version number of OSS API

   @reviewed 2020-09-14

   @param[in] fd opened file descriptor for OSS device
   @param[out] version structure with version digits

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_oss_get_version_internal(int fd, cw_oss_version_t * version)
{
	assert (fd != -1);

	int parameter = 0;
	/* Don't let clang-tidy report warning about signed. To fix
	   the warning we would have to introduce casting, and that
	   would introduce runtime warnings in dmesg on FreeBSD. */
	/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
	if (-1 == ioctl(fd, OSS_GETVERSION, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "get version: ioctl OSS_GETVERSION");
		return CW_FAILURE;
	} else {
		const unsigned int u_parameter = (unsigned int) parameter;
		version->x = (u_parameter & 0xFF0000U) >> 16U;
		version->y = (u_parameter & 0x00FF00U) >> 8U;
		version->z = (u_parameter & 0x0000FFU) >> 0U;
		return CW_SUCCESS;
	}
}




#else /* #ifdef LIBCW_WITH_OSS */




bool cw_is_oss_possible(__attribute__((unused)) const char * device)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "This sound system has been disabled during compilation");
	return false;
}




cw_ret_t cw_oss_fill_gen_internal(__attribute__((unused)) cw_gen_t * gen, __attribute__((unused)) const char * device_name)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "This sound system has been disabled during compilation");
	return CW_FAILURE;
}




#endif /* #ifdef LIBCW_WITH_OSS */
