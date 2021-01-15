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
   @file libcw_pa.c

   @brief PulseAudio sound system.
*/




#include <stdbool.h>




#include "config.h"
#include "libcw_debug.h"
#include "libcw_pa.h"




#define MSG_PREFIX "libcw/pulse: "




extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;




#ifdef LIBCW_WITH_PULSEAUDIO




#include <assert.h>
#include <dlfcn.h> /* dlopen() and related symbols */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>




#include "libcw.h"
#include "libcw_gen.h"
#include "libcw_pa.h"
#include "libcw_utils.h"




typedef struct cw_pa_lib_handle_t {

	/* Returned by dlopen(). To be cleaned up with dlclose(). */
	void * lib_handle;

	pa_simple *(* pa_simple_new)(const char * server_name, const char * name, pa_stream_direction_t dir, const char * device_name, const char * stream_name, const pa_sample_spec * ss, const pa_channel_map * map, const pa_buffer_attr * attr, int * error);
	void       (* pa_simple_free)(pa_simple * simples);
	int        (* pa_simple_write)(pa_simple * simple, const void * data, size_t bytes, int * error);
	pa_usec_t  (* pa_simple_get_latency)(pa_simple * simple, int * error);
	int        (* pa_simple_drain)(pa_simple * simple, int * error);

	size_t     (* pa_usec_to_bytes)(pa_usec_t t, const pa_sample_spec * spec);
	char      *(* pa_strerror)(int error);
} cw_pa_lib_handle_t;




/*
  FIXME: verify how this data structure is handled when there are
  many generators.
  How many times the structure is set?
  How many times it's closed?
  Is it closed for all generators when first of these generators is destroyed?
  Do we need a reference counter for this structure?
*/
static cw_pa_lib_handle_t g_cw_pa_lib_handle;




static pa_simple  * cw_pa_simple_new_internal(const char * picked_device_name, const char * stream_name, unsigned int * sample_rate, int * error);
static int          cw_pa_dlsym_internal(cw_pa_lib_handle_t * cw_pa);
static cw_ret_t     cw_pa_open_and_configure_sound_device_internal(cw_gen_t * gen, const cw_gen_config_t * gen_conf);
static void         cw_pa_close_sound_device_internal(cw_gen_t * gen);
static cw_ret_t     cw_pa_write_buffer_to_sound_device_internal(cw_gen_t * gen);




static const pa_sample_format_t CW_PA_SAMPLE_FORMAT = PA_SAMPLE_S16LE; /* Signed 16 bit, Little Endian */
static const int CW_PA_BUFFER_N_SAMPLES = 256;




/**
   @brief Check if it is possible to open PulseAudio output with given device
   name

   Function first tries to load PulseAudio library, and then does a test
   opening of PulseAudio output, but it closes it before returning.

   @reviewed 2020-11-14

   @param[in] device_name name of PulseAudio device to be used; if NULL then
   the function will use library-default device name.

   @return true if opening PulseAudio output succeeded
   @return false if opening PulseAudio output failed
*/
bool cw_is_pa_possible(const char * device_name)
{
	/* TODO: revise logging of errors here. E.g. inability to open a
	   library is not an error, but a simple indication that PA is not
	   accessible on this machine, and this should not be logged as
	   error. */

	const char * const library_name = "libpulse-simple.so";
	if (CW_SUCCESS != cw_dlopen_internal(library_name, &g_cw_pa_lib_handle.lib_handle)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "is possible: can't access PulseAudio library \"%s\"", library_name);
		return false;
	}

	int rv = cw_pa_dlsym_internal(&g_cw_pa_lib_handle);
	if (rv < 0) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "is possible: failed to resolve PulseAudio symbol #%d, can't correctly load PulseAudio library", rv);
		dlclose(g_cw_pa_lib_handle.lib_handle);
		return false;
	}

	char picked_device_name[LIBCW_SOUND_DEVICE_NAME_SIZE] = { 0 };
	cw_gen_pick_device_name_internal(device_name, CW_AUDIO_PA,
					 picked_device_name, sizeof (picked_device_name));

	unsigned int sample_rate = 0;
	int error = 0;
	pa_simple * simple = cw_pa_simple_new_internal(picked_device_name, "cw_is_pa_possible()", &sample_rate, &error);
	if (NULL == simple) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR, /* TODO: is this really an error? */
			      MSG_PREFIX "is possible: can't connect to PulseAudio server: %s", g_cw_pa_lib_handle.pa_strerror(error));
		if (g_cw_pa_lib_handle.lib_handle) { /* FIXME: this closing of global handle won't work well for multi-generator library. */
			dlclose(g_cw_pa_lib_handle.lib_handle);
		}
		return false;
	} else {
		/* TODO: verify this comment: We do dlclose(g_cw_pa_lib_handle.lib_handle) in cw_pa_close_sound_device_internal(). */
		g_cw_pa_lib_handle.pa_simple_free(simple);
		simple = NULL;
		return true;
	}
}




/**
   @brief Configure given @p gen variable to work with PulseAudio sound system

   This function only initializes @p gen by setting some of its members. It
   doesn't interact with sound system (doesn't try to open or configure it).

   @reviewed 2020-11-12

   @param[in,out] gen generator structure to initialize

   @return CW_SUCCESS
*/
cw_ret_t cw_pa_init_gen_internal(cw_gen_t * gen)
{
	assert (gen);

	gen->sound_system                    = CW_AUDIO_PA;
	gen->open_and_configure_sound_device = cw_pa_open_and_configure_sound_device_internal;
	gen->close_sound_device              = cw_pa_close_sound_device_internal;
	gen->write_buffer_to_sound_device    = cw_pa_write_buffer_to_sound_device_internal;

	return CW_SUCCESS;
}




/**
   @brief Write generated samples to PulseAudio sound device configured and opened for generator

   @reviewed 2020-07-20

   @param[in] gen generator that will write to sound device

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
static cw_ret_t cw_pa_write_buffer_to_sound_device_internal(cw_gen_t * gen)
{
	assert (gen);
	assert (gen->sound_system == CW_AUDIO_PA);

	int error = 0;
	size_t n_bytes = sizeof (gen->buffer[0]) * gen->buffer_n_samples;
	int rv = g_cw_pa_lib_handle.pa_simple_write(gen->pa_data.simple, gen->buffer, n_bytes, &error);
	if (rv < 0) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "write: pa_simple_write() failed: %s", g_cw_pa_lib_handle.pa_strerror(error));
		return CW_FAILURE;
	} else {
		//cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO, MSG_PREFIX "written %d samples with PulseAudio", gen->buffer_n_samples);
		return CW_SUCCESS;
	}
}




/**
   @brief Wrapper for pa_simple_new()

   Wrapper for pa_simple_new() and related code. The code block contained
   in the function is useful in two different places: when first probing
   if PulseAudio output is available, and when opening PulseAudio output
   for writing.

   On success the function returns pointer to PulseAudio sink open for
   writing (playback). The function tries to set up buffering parameters
   for minimal latency, but it doesn't try too hard.

   The function *does not* set size of sound buffer in libcw's generator.

   @reviewed 2020-11-11

   @param[in] picked_device_name name of PulseAudio device to be used. Non-NULL pointer only. Empty string for default device.
   @param[in] stream_name descriptive name of client, passed to pa_simple_new
   @param[out] sample_rate sample rate configured for sound sink
   @param[out] error potential PulseAudio error code

   @return pointer to new PulseAudio sink on success
   @return NULL on failure
*/
static pa_simple * cw_pa_simple_new_internal(const char * picked_device_name, const char * stream_name, unsigned int * sample_rate, int * error)
{
	pa_sample_spec spec = { 0 };
	spec.format = CW_PA_SAMPLE_FORMAT;
	spec.rate = 44100; /* TODO: why this value is hardcoded? */
	spec.channels = 1;

	// http://www.mail-archive.com/pulseaudio-tickets@mail.0pointer.de/msg03295.html
	pa_buffer_attr attr = { 0 };
	attr.prebuf    = (uint32_t) -1;
	attr.fragsize  = (uint32_t) -1;
	/* TODO: notice that the values are smaller than in the URL above. */
	attr.tlength   = g_cw_pa_lib_handle.pa_usec_to_bytes(10 * 1000, &spec);
	attr.minreq    = g_cw_pa_lib_handle.pa_usec_to_bytes(0, &spec);
	attr.maxlength = g_cw_pa_lib_handle.pa_usec_to_bytes(10 * 1000, &spec);
	/* attr.prebuf = ; */ /* ? */
	/* attr.fragsize = sizeof(uint32_t) -1; */ /* Not relevant to playback. */

	/* If 'picked_device_name' is empty, it means 'use default device
	   name'. In that case we have to pass NULL pointer to PulseAudio
	   API. */
	const char * dev = ('\0' == picked_device_name[0]) ? NULL : picked_device_name;

	pa_simple * simple = g_cw_pa_lib_handle.pa_simple_new(NULL,                  /* Server name (NULL for default). */
							      "libcw",               /* Descriptive name of client (program name etc.). */
							      PA_STREAM_PLAYBACK,    /* Stream direction. */
							      dev,                   /* Device/sink name (NULL for default). */
							      stream_name,           /* Stream name, descriptive name for this client (program name, song title, etc.). */
							      &spec,                 /* Sample specification. */
							      NULL,                  /* Channel map (NULL for default). */
							      &attr,                 /* Buffering attributes (NULL for default). */
							      error);                /* Error buffer (when routine returns NULL). */

	*sample_rate = spec.rate;

	return simple;
}




/**
   @brief Resolve/get symbols from PulseAudio library

   Function resolves/gets addresses of few PulseAudio functions used by
   libcw and stores them in @p cw_pa variable.

   On failure the function returns negative value, different for every
   symbol that the funciton failed to resolve. Function stops and returns
   on first failure.

   @reviewed 2020-07-20

   @param[in,out] cw_pa libcw pa data structure with library handle to opened PulseAudio library

   @return 0 on success
   @return negative value on failure
*/
static int cw_pa_dlsym_internal(cw_pa_lib_handle_t * cw_pa)
{
	*(void **) &(cw_pa->pa_simple_new)         = dlsym(cw_pa->lib_handle, "pa_simple_new");
	if (!cw_pa->pa_simple_new)         return -(__LINE__);
	*(void **) &(cw_pa->pa_simple_free)        = dlsym(cw_pa->lib_handle, "pa_simple_free");
	if (!cw_pa->pa_simple_free)        return -(__LINE__);
	*(void **) &(cw_pa->pa_simple_write)       = dlsym(cw_pa->lib_handle, "pa_simple_write");
	if (!cw_pa->pa_simple_write)       return -(__LINE__);
	*(void **) &(cw_pa->pa_strerror)           = dlsym(cw_pa->lib_handle, "pa_strerror");
	if (!cw_pa->pa_strerror)           return -(__LINE__);
	*(void **) &(cw_pa->pa_simple_get_latency) = dlsym(cw_pa->lib_handle, "pa_simple_get_latency");
	if (!cw_pa->pa_simple_get_latency) return -(__LINE__);
	*(void **) &(cw_pa->pa_simple_drain)       = dlsym(cw_pa->lib_handle, "pa_simple_drain");
	if (!cw_pa->pa_simple_drain)       return -(__LINE__);
	*(void **) &(cw_pa->pa_usec_to_bytes)      = dlsym(cw_pa->lib_handle, "pa_usec_to_bytes");
	if (!cw_pa->pa_usec_to_bytes)      return -(__LINE__);

	return 0;
}




/**
   @brief Open and configure PulseAudio handle stored in given generator

   @reviewed 2020-09-20

   @param[in,out] gen generator for which to open and configure sound system handle
   @param[in] gen_conf

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
static cw_ret_t cw_pa_open_and_configure_sound_device_internal(cw_gen_t * gen, const cw_gen_config_t * gen_conf)
{
	if (gen->sound_device_is_open) {
		/* Ignore the call if the device is already open. */
		return CW_SUCCESS;
	}

	cw_gen_pick_device_name_internal(gen_conf->sound_device, gen->sound_system,
					 gen->picked_device_name, sizeof (gen->picked_device_name));

	unsigned int sample_rate = 0;
	int error = 0;
	gen->pa_data.simple = cw_pa_simple_new_internal(gen->picked_device_name,
							gen->library_client.name ? gen->library_client.name : "app",
							&sample_rate,
							&error);

 	if (NULL == gen->pa_data.simple) {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open device: can't connect to PulseAudio server: %s", g_cw_pa_lib_handle.pa_strerror(error));
		return false;
	}

	gen->buffer_n_samples = CW_PA_BUFFER_N_SAMPLES;
	gen->sample_rate = sample_rate;
	gen->pa_data.latency_usecs = g_cw_pa_lib_handle.pa_simple_get_latency(gen->pa_data.simple, &error);

	if ((pa_usec_t) -1 == gen->pa_data.latency_usecs) {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open device: pa_simple_get_latency() failed: %s", g_cw_pa_lib_handle.pa_strerror(error));
	}

#if CW_DEV_RAW_SINK
	gen->dev_raw_sink = open("/tmp/cw_file.pa.raw", O_WRONLY | O_TRUNC | O_NONBLOCK);
#endif
	assert (gen && gen->pa_data.simple);

	gen->sound_device_is_open = true;

	return CW_SUCCESS;
}




/**
   @brief Close PulseAudio device stored in given generator

   @reviewed 2020-07-20

   @param[in,out] gen generator for which to close its sound device
*/
static void cw_pa_close_sound_device_internal(cw_gen_t * gen)
{
	if (gen->pa_data.simple) {
		/* Make sure that every single sample was played */
		int error = 0;
		if (g_cw_pa_lib_handle.pa_simple_drain(gen->pa_data.simple, &error) < 0) {
			cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
				      MSG_PREFIX "close device: pa_simple_drain() failed: %s", g_cw_pa_lib_handle.pa_strerror(error));
		}
		g_cw_pa_lib_handle.pa_simple_free(gen->pa_data.simple);
		gen->pa_data.simple = NULL;
	} else {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING,
			      MSG_PREFIX "close device: called the function for NULL PA sink");
	}

	if (g_cw_pa_lib_handle.lib_handle) { /* FIXME: this closing of global handle won't work well for multi-generator library. */
		dlclose(g_cw_pa_lib_handle.lib_handle);
	}

	gen->sound_device_is_open = false;

#if CW_DEV_RAW_SINK
	if (gen->dev_raw_sink != -1) {
		close(gen->dev_raw_sink);
		gen->dev_raw_sink = -1;
	}
#endif
	return;
}




#else /* #ifdef LIBCW_WITH_PULSEAUDIO */




bool cw_is_pa_possible(__attribute__((unused)) const char * device_name)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "This sound system has been disabled during compilation");
	return false;
}




cw_ret_t cw_pa_init_gen_internal(__attribute__((unused)) cw_gen_t * gen)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "This sound system has been disabled during compilation");
	return CW_FAILURE;
}




#endif /* #ifdef LIBCW_WITH_PULSEAUDIO */
