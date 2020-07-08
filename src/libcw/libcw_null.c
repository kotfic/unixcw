/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2019  Kamil Ignacak (acerion@wp.pl)

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
   \file libcw_null.c

   \brief Null sound sink.

   No sound is being played, but time periods necessary for generator
   to operate are being measured.
*/




#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <stdbool.h>




#include "libcw_null.h"
#include "libcw_utils.h"
#include "libcw_gen.h"




static int  cw_null_open_and_configure_device_internal(cw_gen_t *gen);
static void cw_null_close_device_internal(cw_gen_t * gen);




/**
   @brief Configure given @p gen variable to work with Null sound system

   This function only sets some fields of @p gen (variables and function
   pointers). It doesn't interact with Null sound system.

   @reviewed 2017-02-04

   @param gen[in] generator structure in which to fill some fields
   @param device_name[in] name of Null device to use

   @return CW_SUCCESS
*/
cw_ret_t cw_null_fill_gen_internal(cw_gen_t * gen, const char * device_name)
{
	assert (gen);

	gen->sound_system = CW_AUDIO_NULL;
	cw_gen_set_sound_device_internal(gen, device_name);

	gen->open_and_configure_device = cw_null_open_and_configure_device_internal;
	gen->close_device              = cw_null_close_device_internal;
	// gen->write                     = cw_null_write; /* The function is called in libcw_gen.c explicitly/directly, not by a pointer. TODO: we should call this function by function pointer. */

	gen->sample_rate = 48000; /* Some asserts may check for non-zero
				     value of sample rate or its derivatives. */

	return CW_SUCCESS;
}




/**
   \brief Check if it is possible to open Null sound output

   \reviewed on 2017-02-04

   \param device - sink device, unused

   \return true - it's always possible to write to Null device
*/
bool cw_is_null_possible(__attribute__((unused)) const char *device)
{
	return true;
}




/**
   @brief Open and configure Null sound system handle stored in given generator

   @param gen[in] generator for which to open and configure sound system handle

   \return CW_SUCCESS
*/
int cw_null_open_and_configure_device_internal(cw_gen_t *gen)
{
	gen->sound_device_is_open = true;
	return CW_SUCCESS;
}




/**
   @brief Close Null device stored in given generator

   @param gen[in] generator for which to close its sound device

   \return CW_SUCCESS
*/
void cw_null_close_device_internal(cw_gen_t * gen)
{
	gen->sound_device_is_open = false;
	return;
}




/**
   \brief Write to Null sound sink configured and opened for generator

   The function doesn't really write the samples anywhere, it just
   sleeps for period of time that would be necessary to write the
   samples to a real sound device and play/sound them.

   \reviewed on 2017-02-04

   \param gen - generator

   \return CW_SUCCESS on success
   \return CW_FAILURE otherwise
*/
void cw_null_write(__attribute__((unused)) cw_gen_t *gen, cw_tone_t *tone)
{
	assert (gen);
	assert (gen->sound_system == CW_AUDIO_NULL);
	assert (tone->duration >= 0); /* TODO: shouldn't the condition be "tone->duration > 0"? */

	cw_usleep_internal(tone->duration);

	return;
}
