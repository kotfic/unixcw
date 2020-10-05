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
   @file libcw_alsa.c

   @brief ALSA sound system.
*/




#include <inttypes.h>
#include <stdbool.h>




#include "libcw_alsa.h"
#include "libcw_debug.h"
#include "libcw_rec.h"




#define MSG_PREFIX "libcw/alsa: "
#define CW_ALSA_SW_PARAMS_CONFIG  0




extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;
extern const unsigned int cw_supported_sample_rates[];




#ifdef LIBCW_WITH_ALSA



/*
  Call additional ALSA function to free some resources, so that valgrind doesn't complain.
  https://git.alsa-project.org/?p=alsa-lib.git;a=blob;f=MEMORY-LEAK;hb=HEAD
*/
#define WITH_ALSA_FREE_GLOBAL_CONFIG 1




#include <alsa/asoundlib.h>
#include <dlfcn.h> /* dlopen() and related symbols */




#include "libcw.h"
#include "libcw_gen.h"
#include "libcw_utils.h"




/*
  FIXME: verify how this data structure is handled when there are
  many generators.
  How many times the structure is set?
  How many times it's closed?
  Is it closed for all generators when first of these generators is destroyed?
  Do we need a reference counter for this structure?
*/
struct cw_alsa_handle_t {

	/* For pointer returned by dlopen(). This is the only non-function-pointer variable.

	   TODO: In the future it should be set with dlopen() only once (only
	   on first call to cw_is_alsa_possible()), and then it should only
	   be checked for NULLness. It should be protected with mutex to
	   avoid race conditions during checking of NULLness. */
	void * lib_handle;



	int (* snd_pcm_open)(snd_pcm_t ** pcm, const char * name, snd_pcm_stream_t stream, int mode);
	int (* snd_pcm_close)(snd_pcm_t * pcm);
	int (* snd_pcm_prepare)(snd_pcm_t * pcm);
	int (* snd_pcm_drop)(snd_pcm_t * pcm);
	snd_pcm_sframes_t (* snd_pcm_writei)(snd_pcm_t * pcm, const void * buffer, snd_pcm_uframes_t size);
#if WITH_ALSA_FREE_GLOBAL_CONFIG
	int (* snd_config_update_free_global)(void);
#endif



	const char *(* snd_strerror)(int errnum);



	/* Allocate 'hw params' variable. */
	int (* snd_pcm_hw_params_malloc)(snd_pcm_hw_params_t ** ptr);

	/* "frees a previously allocated snd_pcm_hw_params_t" */
	void (* snd_pcm_hw_params_free)(snd_pcm_hw_params_t * params);

	/* Get full ALSA HW configuration space. Notice that there is
	   no "any" function for SW parameters. */
	int (* snd_pcm_hw_params_any)(snd_pcm_t * pcm, snd_pcm_hw_params_t * hw_params);

	/* "Install one PCM hardware configuration chosen from a
	   configuration space and snd_pcm_prepare it."  */
	int (* snd_pcm_hw_params)(snd_pcm_t * pcm, snd_pcm_hw_params_t * hw_params);



	int (* snd_pcm_hw_params_set_format)(snd_pcm_t * pcm, snd_pcm_hw_params_t * params, snd_pcm_format_t val);
	int (* snd_pcm_hw_params_set_access)(snd_pcm_t * pcm, snd_pcm_hw_params_t * params, snd_pcm_access_t access);
	int (* snd_pcm_hw_params_set_channels)(snd_pcm_t * pcm, snd_pcm_hw_params_t * params, unsigned int val);



	int (* snd_pcm_hw_params_get_periods)(const snd_pcm_hw_params_t * hw_params, unsigned int * val, int * dir);
	int (* snd_pcm_hw_params_get_periods_min)(const snd_pcm_hw_params_t * hw_params, unsigned int * val, int * dir);
	int (* snd_pcm_hw_params_get_periods_max)(const snd_pcm_hw_params_t * hw_params, unsigned int * val, int * dir);
	int (* snd_pcm_hw_params_set_periods)(snd_pcm_t * pcm, snd_pcm_hw_params_t * hw_params, unsigned int val, int dir);

	/* For testing/getting period size in ALSA configuration space.
	   We can use it first on initial hw parameters obtained with
	   snd_pcm_hw_params_any().  We can also use it on the hw
	   parameters after the configuration space has been narrowed
	   down with our ALSA API calls. */
	int (* snd_pcm_hw_params_get_period_size_min)(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir);
	int (* snd_pcm_hw_params_get_period_size_max)(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir);

	/* Ask ALSA to set period size close to given value. */
	int (* snd_pcm_hw_params_set_period_size_near)(snd_pcm_t * pcm, snd_pcm_hw_params_t * params, snd_pcm_uframes_t * val, int * dir);

	/* Ask ALSA to set exact period size. */
	int (* snd_pcm_hw_params_set_period_size)(snd_pcm_t * pcm, snd_pcm_hw_params_t * params, snd_pcm_uframes_t val, int dir);

	int (* snd_pcm_hw_params_set_period_size_max)(snd_pcm_t * pcm, snd_pcm_hw_params_t * params, snd_pcm_uframes_t * val, int * dir);

	/* Get currently set period size (set with snd_pcm_hw_params_set_period_size_near()). */
	int (* snd_pcm_hw_params_get_period_size)(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir);



	/* For testing/getting buffer size in ALSA configuration space.
	   We first restrict the space by setting period size, and
	   after that ALSA may automatically restrict buffer size as
	   well - maybe even to one value. */
	int (* snd_pcm_hw_params_get_buffer_size_min)(const snd_pcm_hw_params_t * params, snd_pcm_uframes_t * val);
	int (* snd_pcm_hw_params_get_buffer_size_max)(const snd_pcm_hw_params_t * params, snd_pcm_uframes_t * val);

	/* Ask ALSA to set buffer size close to given value. */
	int (* snd_pcm_hw_params_set_buffer_size_near)(snd_pcm_t * pcm, snd_pcm_hw_params_t * params, snd_pcm_uframes_t *val);

	/* Get currently set buffer size, either internally restricted
	   by ALSA, or set with snd_pcm_hw_params_set_buffer_size_near(). */
	int (* snd_pcm_hw_params_get_buffer_size)(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val);



	int (* snd_pcm_hw_params_set_rate_near)(snd_pcm_t * pcm, snd_pcm_hw_params_t * params, unsigned int * val, int * dir);
	int (* snd_pcm_hw_params_get_rate)(snd_pcm_hw_params_t * params, unsigned int * val, int * dir);
	int (* snd_pcm_hw_params_get_rate_min)(const snd_pcm_hw_params_t * params, unsigned int * val, int * dir);
	int (* snd_pcm_hw_params_get_rate_max)(const snd_pcm_hw_params_t * params, unsigned int * val, int * dir);



#if CW_ALSA_SW_PARAMS_CONFIG
	/* Get current SW configuration. */
	int (* snd_pcm_sw_params_current)(snd_pcm_t * pcm, snd_pcm_sw_params_t * params);

	/* "Install PCM software configuration". To be used after we
	   set our values of SW parameters. */
	int (* snd_pcm_sw_params)(snd_pcm_t * pcm, snd_pcm_sw_params_t * params);

	/* Allocate 'sw params' variable. */
	int (* snd_pcm_sw_params_malloc)(snd_pcm_sw_params_t **ptr);


	/* This source:
	   http://equalarea.com/paul/alsa-audio.html#interruptex
	   is using the function in "interrupt-driven program".

	   Also this source
	   https://alsa.opensrc.org/Asynchronous_Playback_(Howto)
	   is using the function in program with "asynchronous
	   notification".

	   We don't do that here, so don't use the function. */
	//int (* snd_pcm_sw_params_set_start_threshold)(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);

	/* This source:
	   https://users.suse.com/~mana/alsa090_howto.html
	   suggests that this function can be used for playback using
	   poll(). We don't do poll().

	   See also
	   http://equalarea.com/paul/alsa-audio.html#interruptex,
	   where the function is used for "interrupt-driven
	   program. */
	// int (* snd_pcm_sw_params_set_avail_min)(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
#endif
};
typedef struct cw_alsa_handle_t cw_alsa_handle_t;




/* Constants specific to ALSA sound system configuration */
static const snd_pcm_format_t CW_ALSA_SAMPLE_FORMAT = SND_PCM_FORMAT_S16; /* "Signed 16 bit CPU endian"; I'm guessing that "CPU endian" == "native endianess" */

static cw_ret_t cw_alsa_set_hw_params_internal(cw_gen_t * gen, snd_pcm_hw_params_t * hw_params);
static cw_ret_t cw_alsa_set_hw_params_sample_rate_internal(cw_gen_t * gen, snd_pcm_hw_params_t * hw_params);
static cw_ret_t cw_alsa_set_hw_params_period_size_internal(cw_gen_t * gen, snd_pcm_hw_params_t * hw_params, snd_pcm_uframes_t * actual_period_size);
static cw_ret_t cw_alsa_set_hw_params_buffer_size_internal(cw_gen_t * gen, snd_pcm_hw_params_t * hw_params, snd_pcm_uframes_t actual_period_size);
static void cw_alsa_print_hw_params_internal(snd_pcm_hw_params_t * hw_params, const char * where);
static void cw_alsa_test_hw_period_sizes(cw_gen_t * gen);

#if CW_ALSA_SW_PARAMS_CONFIG
static cw_ret_t cw_alsa_set_sw_params_internal(cw_gen_t * gen, snd_pcm_sw_params_t * sw_params);
static void cw_alsa_print_sw_params_internal(snd_pcm_sw_params_t * sw_params, const char * where);
#endif

static int      cw_alsa_handle_load_internal(cw_alsa_handle_t * alsa_handle);
static cw_ret_t cw_alsa_write_buffer_to_sound_device_internal(cw_gen_t * gen);
static cw_ret_t cw_alsa_debug_evaluate_write_internal(cw_gen_t * gen, int snd_rv);
static cw_ret_t cw_alsa_open_and_configure_sound_device_internal(cw_gen_t * gen);
static void     cw_alsa_close_sound_device_internal(cw_gen_t * gen);




static cw_alsa_handle_t cw_alsa;




/**
   @brief Check if it is possible to open ALSA output with given device name

   The check consists of two parts:
   -# whether it's possible to load ALSA shared library,
   -# whether it's possible to open ALSA device specified by @p device_name

   If it's possible to use ALSA with given device name, the function leaves
   library handle (returned by dlopne()) open and some library function
   symbols loaded, but does not leave any ALSA PCM handle open.

   TODO: the function does too much. It a) checks if ALSA output is possible,
   and b) loads library symbols into global variable for the rest of the code
   to use. The function should have its own copy of cw_alsa_handle_t object,
   and the global object should go away (there should be per-generator
   cw_alsa_handle_t object).
   See FIXME/TODO notes in definition of struct cw_alsa_handle_t type.

   @reviewed 2020-07-04

   @param[in] device_name name of ALSA device to be used; if NULL then the
   function will use library-default device name.

   @return true if opening ALSA output succeeded
   @return false if opening ALSA output failed
*/
bool cw_is_alsa_possible(const char * device_name)
{
	/* TODO: revise logging of errors here. E.g. inability to open a
	   library is not an error, but a simple indication that ALSA is not
	   accessible on this machine, and this should not be logged as
	   error. */

	const char * library_name = "libasound.so.2";
	if (CW_SUCCESS != cw_dlopen_internal(library_name, &cw_alsa.lib_handle)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "is possible: can't access ALSA library '%s'", library_name);
		return false;
	}

	int rv = cw_alsa_handle_load_internal(&cw_alsa);
	if (0 != rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "is possible: failed to resolve ALSA symbol #%d, can't correctly load ALSA library", rv);
		dlclose(cw_alsa.lib_handle);
		return false;
	}

	const char * dev = device_name ? device_name : CW_DEFAULT_ALSA_DEVICE;
	snd_pcm_t * pcm = NULL;
	int snd_rv = cw_alsa.snd_pcm_open(&pcm,
					  dev,                     /* name */
					  SND_PCM_STREAM_PLAYBACK, /* stream (playback/capture) */
					  0);                      /* mode, 0 | SND_PCM_NONBLOCK | SND_PCM_ASYNC */
	if (0 != snd_rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "is possible: can't open ALSA device '%s': %s", dev, cw_alsa.snd_strerror(snd_rv));
		dlclose(cw_alsa.lib_handle);
		return false;
	} else {
		/*
		  Close pcm handle. A generator using ALSA sink will open its
		  own ALSA handle used for playback by the generator.

		  Don't call dlclose(). Don't un-resolve library symbols. The
		  symbols will be used by library code in this file.
		*/
		cw_alsa.snd_pcm_close(pcm);
#if WITH_ALSA_FREE_GLOBAL_CONFIG
		cw_alsa.snd_config_update_free_global();
#endif
		return true;
	}
}




/**
   @brief Configure given @p gen variable to work with ALSA sound system

   This function only sets some fields of @p gen (variables and function
   pointers). It doesn't interact with ALSA sound system.

   @reviewed 2020-07-07

   @param[in] gen generator structure in which to fill some fields
   @param[in] device_name name of ALSA device to use

   @return CW_SUCCESS
*/
cw_ret_t cw_alsa_fill_gen_internal(cw_gen_t * gen, const char * device_name)
{
	gen->sound_system = CW_AUDIO_ALSA;
	cw_gen_set_sound_device_internal(gen, device_name);

	gen->open_and_configure_sound_device = cw_alsa_open_and_configure_sound_device_internal;
	gen->close_sound_device              = cw_alsa_close_sound_device_internal;
	gen->write_buffer_to_sound_device    = cw_alsa_write_buffer_to_sound_device_internal;

	return CW_SUCCESS;
}




/**
   @brief Write generated samples to ALSA sound device configured and opened for generator

   @reviewed 2020-07-07

   @param[in] gen generator that will write to sound device

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
static cw_ret_t cw_alsa_write_buffer_to_sound_device_internal(cw_gen_t * gen)
{
	assert (gen);
	assert (gen->sound_system == CW_AUDIO_ALSA);

	/* Send sound buffer to ALSA.

	   Size of correct and current data in the buffer is the same as
	   ALSA's period, so there should be no underruns. TODO: write a
	   check for this in this function. */
	const int snd_rv = cw_alsa.snd_pcm_writei(gen->alsa_data.pcm_handle, gen->buffer, gen->buffer_n_samples);
	const cw_ret_t cw_ret = cw_alsa_debug_evaluate_write_internal(gen, snd_rv);

#if 0
	/* Verbose debug code. */
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "write: written %d/%d samples", snd_rv, gen->buffer_n_samples);
#endif
	return cw_ret;
}




/**
   @brief Open and configure ALSA handle stored in given generator

   You must use cw_gen_set_sound_device_internal() before calling
   this function. Otherwise generator @p gen won't know which device to open.

   @reviewed 2020-07-07

   @param[in] gen generator for which to open and configure PCM handle

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
static cw_ret_t cw_alsa_open_and_configure_sound_device_internal(cw_gen_t * gen)
{
	int snd_rv = cw_alsa.snd_pcm_open(&gen->alsa_data.pcm_handle,
					  gen->sound_device,       /* name */
					  SND_PCM_STREAM_PLAYBACK, /* stream (playback/capture) */
					  0);                      /* mode, 0 | SND_PCM_NONBLOCK | SND_PCM_ASYNC */
	if (0 != snd_rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: can't open ALSA device '%s': %s", gen->sound_device, cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	}
	/* This is commented because blocking mode is probably already
	   configured in call to snd_pcm_open().
	snd_rv = snd_pcm_nonblock(gen->alsa_data.handle, 0);
	if (0 != snd_rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR, MSG_PREFIX "can't set block mode for ALSA handle");
		return CW_FAILURE;
	}
	*/

	snd_pcm_hw_params_t * hw_params = NULL;
	snd_rv = cw_alsa.snd_pcm_hw_params_malloc(&hw_params);
	if (0 != snd_rv || NULL == hw_params) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: can't allocate memory for ALSA hw params: %s", cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	}

	if (CW_SUCCESS != cw_alsa_set_hw_params_internal(gen, hw_params)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: can't set ALSA hw params");
		cw_alsa.snd_pcm_hw_params_free(hw_params);
		return CW_FAILURE;
	}

#if CW_ALSA_SW_PARAMS_CONFIG
	snd_pcm_sw_params_t * sw_params = NULL;
	cw_alsa.snd_pcm_sw_params_malloc(&sw_params);
	if (CW_SUCCESS != cw_alsa_set_sw_params_internal(gen, sw_params)) {
		cw_alsa.snd_pcm_hw_params_free(hw_params);
		return CW_FAILURE;
	}
#endif

	snd_rv = cw_alsa.snd_pcm_prepare(gen->alsa_data.pcm_handle);
	if (0 != snd_rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: can't prepare ALSA handler: %s", cw_alsa.snd_strerror(snd_rv));
		cw_alsa.snd_pcm_hw_params_free(hw_params);
		return CW_FAILURE;
	}

	/* Get size for generator's data buffer */
	snd_pcm_uframes_t period_size = 0; /* period size in frames */
	int dir = 1; /* TODO: why 1? Shouldn't it be zero? */
	snd_rv = cw_alsa.snd_pcm_hw_params_get_period_size(hw_params, &period_size, &dir);
	cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "open: rv = %d/%s, ALSA period size is %u frames", snd_rv, cw_alsa.snd_strerror(snd_rv), (unsigned int) period_size);

	cw_alsa.snd_pcm_hw_params_free(hw_params);

	/* The linker (?) that I use on Debian links libcw against
	   old version of get_period_size(), which returns
	   period size as return value. This is a workaround. */
	if (snd_rv > 1) {
		gen->buffer_n_samples = snd_rv;
	} else {
		gen->buffer_n_samples = period_size;
	}

#if CW_DEV_RAW_SINK
	gen->dev_raw_sink = open("/tmp/cw_file.alsa.raw", O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK);
	if (gen->dev_raw_sink == -1) {
		fprintf(stderr, MSG_PREFIX "open: failed to open dev raw sink file: '%s'\n", strerror(errno));
	}
#endif

	return CW_SUCCESS;
}




/**
   @brief Close ALSA device stored in given generator

   @reviewed 2020-07-07

   @param[in] gen generator for which to close its sound device
*/
static void cw_alsa_close_sound_device_internal(cw_gen_t * gen)
{
	/* "Stop a PCM dropping pending frames. " */
	cw_alsa.snd_pcm_drop(gen->alsa_data.pcm_handle);
	cw_alsa.snd_pcm_close(gen->alsa_data.pcm_handle);
#if WITH_ALSA_FREE_GLOBAL_CONFIG
	cw_alsa.snd_config_update_free_global();
#endif

	gen->sound_device_is_open = false;

	if (cw_alsa.lib_handle) { /* FIXME: this closing of global handle won't work well for multi-generator library. */
		dlclose(cw_alsa.lib_handle);
	}

#if CW_DEV_RAW_SINK
	if (gen->dev_raw_sink != -1) {
		close(gen->dev_raw_sink);
		gen->dev_raw_sink = -1;
	}
#endif
	return;
}




/**
   @brief Handle value returned by ALSA's write function (snd_pcm_writei())

   If the returned value indicates error, ALSA handle is reset by this
   function.

   This function also checks if expected number of bytes has been written.

   @reviewed 2020-07-08

   @param[in] gen generator with ALSA handle
   @param[in] snd_rv value returned by snd_pcm_writei()

   @return CW_SUCCESS if @p snd_rv is non-error and no other write-related error was detected
   @return CW_FAILURE otherwise
*/
static cw_ret_t cw_alsa_debug_evaluate_write_internal(cw_gen_t * gen, int snd_rv)
{
	if (snd_rv == -EPIPE) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING,
			      MSG_PREFIX "write: underrun");
		cw_alsa.snd_pcm_prepare(gen->alsa_data.pcm_handle); /* Reset sound sink. */
		return CW_FAILURE;

	} else if (snd_rv < 0) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING,
			      MSG_PREFIX "write: writei: %s / %d", cw_alsa.snd_strerror(snd_rv), snd_rv);
		cw_alsa.snd_pcm_prepare(gen->alsa_data.pcm_handle); /* Reset sound sink. */
		return CW_FAILURE;

	} else if (snd_rv != gen->buffer_n_samples) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING,
			      MSG_PREFIX "short write, expected to write %d bytes, written %d bytes", gen->buffer_n_samples, snd_rv);
		return CW_FAILURE;
	} else {
		return CW_SUCCESS;
	}
}




/**
   @brief Set up hardware buffer parameters of ALSA sink

   @param[in] gen generator with opened ALSA PCM handle, for which HW parameters should be configured
   @param[in] hw_params allocated hw params data structure to be used by this function

   @reviewed 2020-07-09

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
static cw_ret_t cw_alsa_set_hw_params_internal(cw_gen_t * gen, snd_pcm_hw_params_t * hw_params)
{
	/* Get full configuration space. */
	int snd_rv = cw_alsa.snd_pcm_hw_params_any(gen->alsa_data.pcm_handle, hw_params);
	if (0 != snd_rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "set hw params: can't get current hw params: %s", cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	}


	// cw_alsa_test_hw_period_sizes(gen); /* This function is only for tests. */
	cw_alsa_print_hw_params_internal(hw_params, "before limiting configuration space");


	/* Set the sample format */
	snd_rv = cw_alsa.snd_pcm_hw_params_set_format(gen->alsa_data.pcm_handle, hw_params, CW_ALSA_SAMPLE_FORMAT);
	if (0 != snd_rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "set hw params: can't set sample format: %s", cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	}


	/* Set PCM access type */
	snd_rv = cw_alsa.snd_pcm_hw_params_set_access(gen->alsa_data.pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (0 != snd_rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "set hw params: can't set access type: %s", cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	}

	/* Set number of channels */
	snd_rv = cw_alsa.snd_pcm_hw_params_set_channels(gen->alsa_data.pcm_handle, hw_params, CW_AUDIO_CHANNELS);
	if (0 != snd_rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "set hw params: can't set number of channels: %s", cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	}


	/* Don't try to over-configure ALSA, it would be a pointless
	   exercise. See comment from this article, starting
	   with "This is my soundcard initialization function":
	   http://stackoverflow.com/questions/3345083/correctly-sizing-alsa-buffers-weird-api
	   Poster sets basic sound playback parameters (channels, sampling
	   rate, sample format), saves the config (with snd_pcm_hw_params()),
	   and then only queries ALSA handle for period size and period
	   time.

	   It turns out that it works in our case: basic hw configuration
	   plus getting period size (I don't need period time).

	   Period size seems to be the most important, and most useful
	   data that I need from configured ALSA handle - this is the
	   size of sound buffer which I can fill with my data and send
	   it down to ALSA internals (possibly without worrying about
	   underruns; if I understand correctly - if I send to ALSA
	   chunks of data of proper size then I don't have to worry
	   about underruns). */


	/*
	  http://equalarea.com/paul/alsa-audio.html:

	  Buffer size:
	  This determines how large the hardware buffer is.
	  It can be specified in units of time or frames.

	  Interrupt interval:
	  This determines how many interrupts the interface will generate
	  per complete traversal of its hardware buffer. It can be set
	  either by specifying a number of periods, or the size of a
	  period. Since this determines the number of frames of space/data
	  that have to accumulate before the interface will interrupt
	  the computer. It is central in controlling latency.

	  http://www.alsa-project.org/main/index.php/FramesPeriods

	  "
	  "frame" represents the unit, 1 frame = # channels x sample_bytes.
	  In case of stereo, 2 bytes per sample, 1 frame corresponds to 2 channels x 2 bytes = 4 bytes.

	  "periods" is the number of periods in a ring-buffer.
	  In OSS, called "fragments".

	  So,
	  - buffer_size = period_size * periods
	  - period_bytes = period_size * bytes_per_frame
	  - bytes_per_frame = channels * bytes_per_sample

	  The "period" defines the frequency to update the status,
	  usually via the invocation of interrupts.  The "period_size"
	  defines the frame sizes corresponding to the "period time".
	  This term corresponds to the "fragment size" on OSS.  On major
	  sound hardwares, a ring-buffer is divided to several parts and
	  an irq is issued on each boundary. The period_size defines the
	  size of this chunk."

	  OSS            ALSA           definition
	  fragment       period         basic chunk of data sent to hw buffer
	*/


	/* Set the sample rate. */
	if (CW_SUCCESS != cw_alsa_set_hw_params_sample_rate_internal(gen, hw_params)) {
		return CW_FAILURE;
	}

	/* Set period size. */
	snd_pcm_uframes_t actual_period_size = 0;
	if (CW_SUCCESS != cw_alsa_set_hw_params_period_size_internal(gen, hw_params, &actual_period_size)) {
		return CW_FAILURE;
	}

	/* Set buffer size derived from period size. */
	if (CW_SUCCESS != cw_alsa_set_hw_params_buffer_size_internal(gen, hw_params, actual_period_size)) {
		return CW_FAILURE;
	}

	cw_alsa_print_hw_params_internal(hw_params, "after setting hw params");

	/* Save hw parameters to device */
	snd_rv = cw_alsa.snd_pcm_hw_params(gen->alsa_data.pcm_handle, hw_params);
	if (0 != snd_rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "set hw params: can't save hw parameters: %s", cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	}

	return CW_SUCCESS;
}




/**
   @brief Set value of sample rate in given hw_params variable

   Function sets sample rate in @p hw_params, and also sets it in given @p
   gen.

   @param[in] gen generator with opened ALSA PCM handle, for which HW parameters should be configured
   @param[in] hw_params allocated hw params data structure to be used by this function

   @reviewed 2020-07-09

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
static cw_ret_t cw_alsa_set_hw_params_sample_rate_internal(cw_gen_t * gen, snd_pcm_hw_params_t * hw_params)
{
	/* Set the sample rate. This influences range of available
	   period sizes (see cw_alsa_test_hw_period_sizes()). */
	bool success = false;
	int snd_rv = 0;

	/* Start from high sample rate. For some reason trying to set a lower
	   rate first resulted in problems with these two tests:
	    - test_cw_gen_state_callback
	    - test_cw_gen_forever_internal

	   On the other hand lower sample rates seems to mean wider range of
	   supported period sizes. */
	for (int i = 0; cw_supported_sample_rates[i]; i++) {
		unsigned int rate = cw_supported_sample_rates[i];
		int dir = 0; /* Reset to zero before each ALSA API call. */
		snd_rv = cw_alsa.snd_pcm_hw_params_set_rate_near(gen->alsa_data.pcm_handle, hw_params, &rate, &dir);
		if (0 == snd_rv) {
			if (rate != cw_supported_sample_rates[i]) {
				cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING, MSG_PREFIX "imprecise sample rate:");
				cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING, MSG_PREFIX "asked for: %u", cw_supported_sample_rates[i]);
				cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING, MSG_PREFIX "got:       %u", rate);
			}
			success = true;
			gen->sample_rate = rate;
			break;
		}
	}

	if (!success) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "set hw params: can't set sample rate: %s", cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	} else {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
			      MSG_PREFIX "set hw params: sample rate: %d", gen->sample_rate);
		return CW_SUCCESS;
	}
}




/**
   @brief Set value of period size in given hw_params variable

   Function sets sample rate in @p hw_params, and also sets it in given @p
   gen.

   TODO: this shouldn't be so complicated. The function is unclear.

   @param[in] gen generator with opened ALSA PCM handle, for which HW parameters should be configured
   @param[in] hw_params allocated hw params data structure to be used by this function
   @param[out] actual_period_size period size that has been selected

   @reviewed 2020-07-09

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
static cw_ret_t cw_alsa_set_hw_params_period_size_internal(cw_gen_t * gen, snd_pcm_hw_params_t * hw_params, snd_pcm_uframes_t * actual_period_size)
{
	int snd_rv = 0;
	int dir = 0;

	uint64_t n_alsa_frames_smallest = 0;
	{
		/* Calculate duration of shortest dot (at highest speed). */
		const int unit_length = CW_DOT_CALIBRATION / CW_SPEED_MAX;
		const int weighting_length = (2 * (CW_WEIGHTING_MIN - 50) * unit_length) / 100;
		const int dot_len = unit_length + weighting_length;
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_DEBUG,
			      MSG_PREFIX "shortest dot = %d [us]", dot_len);


		/* Now calculate count of ALSA frames that will be
		   needed to play that shortest dot. */
		n_alsa_frames_smallest = gen->sample_rate * dot_len / 1000000;
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_DEBUG,
			      MSG_PREFIX "n_samples for shortest dot = %"PRIu64" [us]", n_alsa_frames_smallest);
	}

	/* We want to have few periods per shortest dot. */
	snd_pcm_uframes_t intended_period_size = n_alsa_frames_smallest / 5;

	/* See if the intended period is within range of values supported by HW. */
	snd_pcm_uframes_t period_size_min = 0;
	snd_pcm_uframes_t period_size_max = 0;
	cw_alsa.snd_pcm_hw_params_get_period_size_min(hw_params, &period_size_min, &dir);
	cw_alsa.snd_pcm_hw_params_get_period_size_max(hw_params, &period_size_max, &dir);

	if (intended_period_size < period_size_min) {
		/* Unfortunately at current sample rate the HW
		   doesn't support our preferred period size.
		   Try to set the minimal supported period
		   size instead. */
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_DEBUG,
			      MSG_PREFIX "HW doesn't support intended period size %lu, will try to use minimum period size %lu", intended_period_size, period_size_min);

		intended_period_size = period_size_min;

		snd_rv = cw_alsa.snd_pcm_hw_params_set_period_size(gen->alsa_data.pcm_handle, hw_params, intended_period_size, 1);
		if (0 != snd_rv) {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
				      MSG_PREFIX "Unable to set intended exact period size %lu for playback: %s", intended_period_size, cw_alsa.snd_strerror(snd_rv));
			return CW_FAILURE;
		}
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_DEBUG,
			      MSG_PREFIX "Configured exact near period size for playback: %lu, dir = %d", intended_period_size, dir);

	} else {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_DEBUG,
			      MSG_PREFIX "Will try to set intended near period size %lu for playback, dir = %d", intended_period_size, dir);

		snd_rv = cw_alsa.snd_pcm_hw_params_set_period_size_near(gen->alsa_data.pcm_handle, hw_params, &intended_period_size, &dir);
		if (0 != snd_rv) {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
				      MSG_PREFIX "Unable to set intended near period size %lu for playback: %s", intended_period_size, cw_alsa.snd_strerror(snd_rv));
			return CW_FAILURE;
		}
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_DEBUG,
			      MSG_PREFIX "Configured near period size for playback: %lu, dir = %d", intended_period_size, dir);
	}


	cw_alsa.snd_pcm_hw_params_get_period_size_min(hw_params, &period_size_min, &dir);
	cw_alsa.snd_pcm_hw_params_get_period_size_max(hw_params, &period_size_max, &dir);
	if (period_size_min != period_size_max) {
		/* Sometimes, for some reason, these two values can be different.
		   On my PC max = min+1 */
		dir = -1;
		snd_rv = cw_alsa.snd_pcm_hw_params_set_period_size_max(gen->alsa_data.pcm_handle, hw_params, &period_size_min, &dir);
		if (0 != snd_rv) {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING,
				      MSG_PREFIX "Unable to set period_size_max: %s", cw_alsa.snd_strerror(snd_rv));
		}
	}


	snd_rv = cw_alsa.snd_pcm_hw_params_get_period_size(hw_params, actual_period_size, &dir);
	if (0 != snd_rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "Unable to get period size for playback: %s", cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	}
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "Configured period size for playback: %lu, dir = %d", *actual_period_size, dir);

	return CW_SUCCESS;
}




/**
   @brief Set value of buffer size in given hw_params variable

   @param[in] gen generator with opened ALSA PCM handle, for which HW parameters should be configured
   @param[in] hw_params allocated hw params data structure to be used by this function
   @param[in] actual_period_size period size that has been configured earlier with cw_alsa_set_hw_params_period_size_internal()

   @reviewed 2020-07-09

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
static cw_ret_t cw_alsa_set_hw_params_buffer_size_internal(cw_gen_t * gen, snd_pcm_hw_params_t * hw_params, snd_pcm_uframes_t actual_period_size)
{
	int snd_rv = 0;

	/* ALSA documentation says about buffer size that it
	   should be two times period size. See e.g. here:
	   https://www.alsa-project.org/wiki/FramesPeriods:
	   "Commonly this is 2*period size".

	   We can experiment here with other value. */
	const int n_periods = 3;
	snd_pcm_uframes_t intended_buffer_size = actual_period_size * n_periods;
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "Will try to set intended buffer size %lu", intended_buffer_size);
	snd_rv = cw_alsa.snd_pcm_hw_params_set_buffer_size_near(gen->alsa_data.pcm_handle, hw_params, &intended_buffer_size);
	if (0 != snd_rv) {
		cw_debug_msg ((&cw_debug_object), CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "Unable to set buffer size %lu for playback: %s", intended_buffer_size, cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	}

	/* On my Samsung N150 netbook running Alpine Linux 3.12 I found the
	   parameter to be zero in final configuration space. Better set it
	   explicitly. */
	int dir = 0;
	cw_alsa.snd_pcm_hw_params_set_periods(gen->alsa_data.pcm_handle, hw_params, n_periods, dir);
	if (0 != snd_rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "set hw params: can't set periods: %s / %u", cw_alsa.snd_strerror(snd_rv), n_periods);
		return CW_FAILURE;
	}

	return CW_SUCCESS;
}




/**
   Test/example function, not to be used in production code.

   Test relationship between selected sample rate and available range
   of period sizes.

   Example output on my old-ish PC (sample rate 4000 added for tests):
   Sample rate vs. period size range:
   sample rate = 44100, period size range = 940 - 941
   sample rate = 48000, period size range = 1024 - 1024
   sample rate = 32000, period size range = 682 - 683
   sample rate = 22050, period size range = 470 - 471
   sample rate = 16000, period size range = 341 - 342
   sample rate = 11025, period size range = 235 - 236
   sample rate = 8000, period size range = 170 - 171
   sample rate = 4000, period size range = 85 - 86

   On one hand we want to have small buffer, so that ALSA write()
   operation is blocking even for the shortest tones. So we want to
   have rather small sample rates. But on the other hand we need to be
   able to play tones with CW_FREQUENCY_MAX without
   distortion. Therefore minimal sample rate should be 8000.

   @reviewed 2020-07-10

   @param[in] gen generator variable with opened ALSA PCM handle, for which the period sizes should be tested
*/
__attribute__((unused))
static void cw_alsa_test_hw_period_sizes(cw_gen_t * gen)
{
	snd_pcm_hw_params_t * hw_params = NULL;
	int snd_rv = 0;

	snd_rv = cw_alsa.snd_pcm_hw_params_malloc(&hw_params);
	if (0 != snd_rv || NULL == hw_params) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: can't allocate memory for ALSA hw params: %s", cw_alsa.snd_strerror(snd_rv));
		return;
	}


	fprintf(stderr, "[II] Sample rate vs. period size range:\n");
	for (int i = 0; 0 != cw_supported_sample_rates[i]; i++) {
		unsigned int rate = cw_supported_sample_rates[i];

		snd_rv = cw_alsa.snd_pcm_hw_params_any(gen->alsa_data.pcm_handle, hw_params);
		if (0 != snd_rv) {
			continue;
		}

		int dir = 0;
		snd_rv = cw_alsa.snd_pcm_hw_params_set_rate_near(gen->alsa_data.pcm_handle, hw_params, &rate, &dir);
		if (0 != snd_rv) {
			continue;
		}

		snd_pcm_uframes_t period_size_min = 0;
		snd_pcm_uframes_t period_size_max = 0;
		cw_alsa.snd_pcm_hw_params_get_period_size_min(hw_params, &period_size_min, &dir);
		cw_alsa.snd_pcm_hw_params_get_period_size_max(hw_params, &period_size_max, &dir);

		fprintf(stderr, "[II]     sample rate = %7u, period size range = %lu - %lu\n",
			cw_supported_sample_rates[i], period_size_min, period_size_max);
	}

	cw_alsa.snd_pcm_hw_params_free(hw_params);
}




/**
   @brief Print most important fields of @p hw_params structure

   @reviewed 2020-07-10

   @param[in] hw_params structure from which to print some fields
   @param[in] where debug indicator used in debug messages printed by this function
*/
static void cw_alsa_print_hw_params_internal(snd_pcm_hw_params_t * hw_params, const char * where)
{
	int dir = 0;
	int dir_min = 0;
	int dir_max = 0;

	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "HW params %s:", where);


	snd_pcm_uframes_t period_size_min = 0;
	snd_pcm_uframes_t period_size_max = 0;
	dir = 0;
	cw_alsa.snd_pcm_hw_params_get_period_size_min(hw_params, &period_size_min, &dir);
	dir = 0;
	cw_alsa.snd_pcm_hw_params_get_period_size_max(hw_params, &period_size_max, &dir);
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "    Range of period sizes: %lu - %lu", period_size_min, period_size_max);


	snd_pcm_uframes_t current_period_size = 0;
	dir = 0;
	cw_alsa.snd_pcm_hw_params_get_period_size(hw_params, &current_period_size, &dir);
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "    Current period size: %lu, dir = %d", current_period_size, dir);


	snd_pcm_uframes_t buffer_size_min = 0;
	snd_pcm_uframes_t buffer_size_max = 0;
	cw_alsa.snd_pcm_hw_params_get_buffer_size_min(hw_params, &buffer_size_min);
	cw_alsa.snd_pcm_hw_params_get_buffer_size_max(hw_params, &buffer_size_max);
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "    Range of buffer sizes: %lu - %lu", buffer_size_min, buffer_size_max);


	snd_pcm_uframes_t current_buffer_size = 0;
	cw_alsa.snd_pcm_hw_params_get_buffer_size(hw_params, &current_buffer_size);
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "    Current buffer size: %lu", current_buffer_size);


	/* How many periods in a buffer? */
	unsigned int n_periods = 0;
	unsigned int n_periods_min = 0;
	unsigned int n_periods_max = 0;
	dir = 0;
	cw_alsa.snd_pcm_hw_params_get_periods(hw_params, &n_periods, &dir);
	cw_alsa.snd_pcm_hw_params_get_periods_min(hw_params, &n_periods_min, &dir);
	cw_alsa.snd_pcm_hw_params_get_periods_max(hw_params, &n_periods_max, &dir);
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "    Count of periods in buffer: %u (min = %u, max = %u)", n_periods, n_periods_min, n_periods_max);
	/* The ratio should be a value with zero fractional part. TODO: write a test for it. */
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "    Ratio of period size to buffer size = %f",
		      (1.0 * current_buffer_size) / current_period_size);


	dir_min = 0;
	dir_max = 0;
	unsigned int rate_min = 0;
	unsigned int rate_max = 0;
	cw_alsa.snd_pcm_hw_params_get_rate_min(hw_params, &rate_min, &dir_min);
	cw_alsa.snd_pcm_hw_params_get_rate_max(hw_params, &rate_max, &dir_max);
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "    Range of sample rates: %u - %u (dir min = %d, dir max = %d)",
		      rate_min, rate_max, dir_min, dir_max);
}




#if CW_ALSA_SW_PARAMS_CONFIG
static cw_ret_t cw_alsa_set_sw_params_internal(cw_gen_t * gen, snd_pcm_sw_params_t * sw_params)
{
	snd_pcm_t *handle = gen->alsa_data.handle;


	int snd_rv;
	/* Get the current sw_params. */
	snd_rv = cw_alsa.snd_pcm_sw_params_current(handle, sw_params);
	if (0 != snd_rv) {
		cw_debug_msg ((&cw_debug_object), CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "Unable to determine current sw_params for playback: %s", cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	}



	/* write the parameters to the playback device */
	snd_rv = cw_alsa.snd_pcm_sw_params(handle, sw_params);
	if (0 != snd_rv) {
		cw_debug_msg ((&cw_debug_object), CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "Unable to set sw params for playback: %s", cw_alsa.snd_strerror(snd_rv));
		return CW_FAILURE;
	}

	return CW_SUCCESS;
}




static void cw_alsa_print_sw_params_internal(snd_pcm_sw_params_t * sw_params, const char * where)
{
}




#endif




/**
   @brief Resolve/get symbols from ALSA library

   Function resolves/gets addresses of ALSA functions used by libcw and
   stores them in @p alsa_handle.

   On failure the function returns negative value, different for every symbol
   that the funciton failed to resolve. Function stops and returns on first
   failure.

   @reviewed 2020-07-10

   @param[in] alsa_handle structure with function pointers to set

   @return 0 on success
   @return negative value on failure, indicating which library symbol failed to load
*/
static int cw_alsa_handle_load_internal(cw_alsa_handle_t * alsa_handle)
{
	*(void **) &(alsa_handle->snd_pcm_open)    = dlsym(alsa_handle->lib_handle, "snd_pcm_open");
	if (!alsa_handle->snd_pcm_open)            return -1;
	*(void **) &(alsa_handle->snd_pcm_close)   = dlsym(alsa_handle->lib_handle, "snd_pcm_close");
	if (!alsa_handle->snd_pcm_close)           return -2;
	*(void **) &(alsa_handle->snd_pcm_prepare) = dlsym(alsa_handle->lib_handle, "snd_pcm_prepare");
	if (!alsa_handle->snd_pcm_prepare)         return -3;
	*(void **) &(alsa_handle->snd_pcm_drop)    = dlsym(alsa_handle->lib_handle, "snd_pcm_drop");
	if (!alsa_handle->snd_pcm_drop)            return -4;
	*(void **) &(alsa_handle->snd_pcm_writei)  = dlsym(alsa_handle->lib_handle, "snd_pcm_writei");
	if (!alsa_handle->snd_pcm_writei)          return -5;
#if WITH_ALSA_FREE_GLOBAL_CONFIG
	*(void **) &(alsa_handle->snd_config_update_free_global)  = dlsym(alsa_handle->lib_handle, "snd_config_update_free_global");
	if (!alsa_handle->snd_config_update_free_global)          return -6;
#endif

	*(void **) &(alsa_handle->snd_strerror) = dlsym(alsa_handle->lib_handle, "snd_strerror");
	if (!alsa_handle->snd_strerror)         return -10;

	*(void **) &(alsa_handle->snd_pcm_hw_params_malloc) = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_malloc");
	if (!alsa_handle->snd_pcm_hw_params_malloc)         return -20;
	*(void **) &(alsa_handle->snd_pcm_hw_params_free)   = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_free");
	if (!alsa_handle->snd_pcm_hw_params_free)           return -21;
	*(void **) &(alsa_handle->snd_pcm_hw_params_any)    = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_any");
	if (!alsa_handle->snd_pcm_hw_params_any)            return -22;
	*(void **) &(alsa_handle->snd_pcm_hw_params)        = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params");
	if (!alsa_handle->snd_pcm_hw_params)                return -23;

	*(void **) &(alsa_handle->snd_pcm_hw_params_set_format)   = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_set_format");
	if (!alsa_handle->snd_pcm_hw_params_set_format)           return -31;
	*(void **) &(alsa_handle->snd_pcm_hw_params_set_access)   = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_set_access");
	if (!alsa_handle->snd_pcm_hw_params_set_access)           return -32;
	*(void **) &(alsa_handle->snd_pcm_hw_params_set_channels) = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_set_channels");
	if (!alsa_handle->snd_pcm_hw_params_set_channels)         return -33;

	*(void **) &(alsa_handle->snd_pcm_hw_params_get_periods)          = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_periods");
	if (!alsa_handle->snd_pcm_hw_params_get_periods)                  return -(__LINE__);
	*(void **) &(alsa_handle->snd_pcm_hw_params_get_periods_min)      = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_periods_min");
	if (!alsa_handle->snd_pcm_hw_params_get_periods_min)              return -(__LINE__);
	*(void **) &(alsa_handle->snd_pcm_hw_params_get_periods_max)      = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_periods_max");
	if (!alsa_handle->snd_pcm_hw_params_get_periods_max)              return -(__LINE__);
	*(void **) &(alsa_handle->snd_pcm_hw_params_set_periods)          = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_set_periods");
	if (!alsa_handle->snd_pcm_hw_params_set_periods)                  return -(__LINE__);


	*(void **) &(alsa_handle->snd_pcm_hw_params_get_period_size_min)  = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_period_size_min");
	if (!alsa_handle->snd_pcm_hw_params_get_period_size_min)          return -41;
	*(void **) &(alsa_handle->snd_pcm_hw_params_get_period_size_max)  = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_period_size_max");
	if (!alsa_handle->snd_pcm_hw_params_get_period_size_max)          return -42;
	*(void **) &(alsa_handle->snd_pcm_hw_params_set_period_size_near) = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_set_period_size_near");
	if (!alsa_handle->snd_pcm_hw_params_set_period_size_near)         return -43;
	*(void **) &(alsa_handle->snd_pcm_hw_params_set_period_size)      = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_set_period_size");
	if (!alsa_handle->snd_pcm_hw_params_set_period_size)              return -44;
	*(void **) &(alsa_handle->snd_pcm_hw_params_set_period_size_max)  = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_set_period_size_max");
	if (!alsa_handle->snd_pcm_hw_params_set_period_size_max)          return -45;
	*(void **) &(alsa_handle->snd_pcm_hw_params_get_period_size)      = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_period_size");
	if (!alsa_handle->snd_pcm_hw_params_get_period_size)              return -46;


	*(void **) &(alsa_handle->snd_pcm_hw_params_get_buffer_size_min)  = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_buffer_size_min");
	if (!alsa_handle->snd_pcm_hw_params_get_buffer_size_min)          return -50;
	*(void **) &(alsa_handle->snd_pcm_hw_params_get_buffer_size_max)  = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_buffer_size_max");
	if (!alsa_handle->snd_pcm_hw_params_get_buffer_size_max)          return -51;
	*(void **) &(alsa_handle->snd_pcm_hw_params_set_buffer_size_near) = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_set_buffer_size_near");
	if (!alsa_handle->snd_pcm_hw_params_set_buffer_size_near)         return -52;
	*(void **) &(alsa_handle->snd_pcm_hw_params_get_buffer_size)      = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_buffer_size");
	if (!alsa_handle->snd_pcm_hw_params_get_buffer_size)              return -53;

	*(void **) &(alsa_handle->snd_pcm_hw_params_set_rate_near) = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_set_rate_near");
	if (!alsa_handle->snd_pcm_hw_params_set_rate_near)         return -60;
	*(void **) &(alsa_handle->snd_pcm_hw_params_get_rate)      = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_rate");
	if (!alsa_handle->snd_pcm_hw_params_get_rate)              return -61;
	*(void **) &(alsa_handle->snd_pcm_hw_params_get_rate_min)  = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_rate_min");
	if (!alsa_handle->snd_pcm_hw_params_get_rate_min)          return -62;
	*(void **) &(alsa_handle->snd_pcm_hw_params_get_rate_max)  = dlsym(alsa_handle->lib_handle, "snd_pcm_hw_params_get_rate_max");
	if (!alsa_handle->snd_pcm_hw_params_get_rate_max)          return -63;


#if CW_ALSA_SW_PARAMS_CONFIG
	*(void **) &(alsa_handle->snd_pcm_sw_params_current) = dlsym(alsa_handle->lib_handle, "snd_pcm_sw_params_current");
	if (!alsa_handle->snd_pcm_sw_params_current)         return -101;
	*(void **) &(alsa_handle->snd_pcm_sw_params)         = dlsym(alsa_handle->lib_handle, "snd_pcm_sw_params");
	if (!alsa_handle->snd_pcm_sw_params)                 return -102;
	*(void **) &(alsa_handle->snd_pcm_sw_params_malloc)  = dlsym(alsa_handle->lib_handle, "snd_pcm_sw_params_malloc");
	if (!alsa_handle->snd_pcm_sw_params_malloc)          return -103;
#endif

	return 0;
}





/**
   @brief Call ALSA's snd_pcm_drop() function for given generator

   @reviewed 2017-02-05

   @param[in] gen generator with ALSA PCM handle
*/
void cw_alsa_drop_internal(cw_gen_t * gen)
{
	/* TODO: why do we need this test? When cw_alsa function would be
	   called with non-ALSA sound system? */
	if (gen->sound_system != CW_AUDIO_ALSA) {
		return;
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "PCM drop");
	cw_alsa.snd_pcm_drop(gen->alsa_data.pcm_handle);

	return;
}




#else /* #ifdef LIBCW_WITH_ALSA */




bool cw_is_alsa_possible(__attribute__((unused)) const char * device_name)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "This sound system has been disabled during compilation");
	return false;
}




cw_ret_t cw_alsa_fill_gen_internal(__attribute__((unused)) cw_gen_t * gen, __attribute__((unused)) const char * device_name)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "This sound system has been disabled during compilation");
	return CW_FAILURE;
}




void cw_alsa_drop_internal(__attribute__((unused)) cw_gen_t * gen)
{
	/* Don't log anything. */
	return;
}




#endif /* #ifdef LIBCW_WITH_ALSA */
