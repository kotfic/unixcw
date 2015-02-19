/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2015  Kamil Ignacak (acerion@wp.pl)

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
   \file libcw.c

*/





#include "libcw.h"
#include "libcw_gen.h"
#include "libcw_key.h"
#include "libcw_debug.h"





/* Main container for data related to generating audible Morse code.
   This is a global variable in library file, but in future the
   variable will be moved from the file to client code.

   This is a global variable that should be converted into a function
   argument; this pointer should exist only in client's code, should
   initially be returned by new(), and deleted by delete().

   TODO: perform the conversion later, when you figure out ins and
   outs of the library. */
cw_gen_t *cw_generator = NULL;





/* From libcw_debug.c. */
extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;





/* From libcw_key.c. */
extern volatile cw_key_t cw_key;





/**
   \brief Create new generator

   Allocate memory for new generator data structure, set up default values
   of some of the generator's properties.
   The function does not start the generator (generator does not produce
   a sound), you have to use cw_generator_start() for this.

   Notice that the function doesn't return a generator variable. There
   is at most one generator variable at any given time. You can't have
   two generators. In some future version of the library the function
   will return pointer to newly allocated generator, and then you
   could have as many of them as you want, but not yet.

   \p audio_system can be one of following: NULL, console, OSS, ALSA,
   PulseAudio, soundcard. See "enum cw_audio_systems" in libcw.h for
   exact names of symbolic constants.

   \param audio_system - audio system to be used by the generator
   \param device - name of audio device to be used; if NULL then library will use default device.
*/
int cw_generator_new(int audio_system, const char *device)
{
	cw_generator = cw_gen_new_internal(audio_system, device);
	if (!cw_generator) {
		cw_debug_msg ((&cw_debug_object), CW_DEBUG_STDLIB, CW_DEBUG_ERROR,
			      "libcw: can't create generator");
		return CW_FAILURE;
	} else {
		/* For some (all?) applications a key needs to have
		   some generator associated with it. */
		cw_key_register_generator_internal(&cw_key, cw_generator);

		return CW_SUCCESS;
	}
}





/**
   \brief Deallocate generator

   Deallocate/destroy generator data structure created with call
   to cw_generator_new(). You can't start nor use the generator
   after the call to this function.
*/
void cw_generator_delete(void)
{
	cw_gen_delete_internal(&cw_generator);

	return;
}





/**
   \brief Start a generator

   Start producing tones using generator created with
   cw_generator_new(). The source of tones is a tone queue associated
   with the generator. If the tone queue is empty, the generator will
   wait for new tones to be queued.

   \return CW_FAILURE on errors
   \return CW_SUCCESS on success
*/
int cw_generator_start(void)
{
	return cw_gen_start_internal(cw_generator);
}





/**
   \brief Shut down a generator

   Silence tone generated by generator (level of generated sine wave is
   set to zero, with falling slope), and shut the generator down.

   The shutdown does not erase generator's configuration.

   If you want to have this generator running again, you have to call
   cw_generator_start().
*/
void cw_generator_stop(void)
{
	cw_gen_stop_internal(cw_generator);

	return;
}





/**
   \brief Delete a generator - wrapper used in libcw_utils.c
*/
void cw_generator_delete_internal(void)
{
	if (cw_generator) {
		cw_gen_delete_internal(&cw_generator);
	}

	return;
}









/**
   \brief Set sending speed of generator

   See libcw.h/CW_SPEED_{INITIAL|MIN|MAX} for initial/minimal/maximal value
   of send speed.

   errno is set to EINVAL if \p new_value is out of range.

   testedin::test_parameter_ranges()

   \param new_value - new value of send speed to be assigned to generator

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_set_send_speed(int new_value)
{
	int rv = cw_gen_set_speed_internal(cw_generator, new_value);
	return rv;
}





/**
   \brief Set frequency of generator

   Set frequency of sound wave generated by generator.
   The frequency must be within limits marked by CW_FREQUENCY_MIN
   and CW_FREQUENCY_MAX.

   See libcw.h/CW_FREQUENCY_{INITIAL|MIN|MAX} for initial/minimal/maximal
   value of frequency.

   errno is set to EINVAL if \p new_value is out of range.

   \param new_value - new value of frequency to be assigned to generator

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_set_frequency(int new_value)
{
	int rv = cw_gen_set_frequency_internal(cw_generator, new_value);
	return rv;
}





/**
   \brief Set volume of generator

   Set volume of sound wave generated by generator.
   The volume must be within limits marked by CW_VOLUME_MIN and CW_VOLUME_MAX.

   Note that volume settings are not fully possible for the console speaker.
   In this case, volume settings greater than zero indicate console speaker
   sound is on, and setting volume to zero will turn off console speaker
   sound.

   See libcw.h/CW_VOLUME_{INITIAL|MIN|MAX} for initial/minimal/maximal
   value of volume.
   errno is set to EINVAL if \p new_value is out of range.

   testedin::test_volume_functions()
   testedin::test_parameter_ranges()

   \param new_value - new value of volume to be assigned to generator

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_set_volume(int new_value)
{
	int rv = cw_gen_set_volume_internal(cw_generator, new_value);
	return rv;
}





/**
   \brief Set sending gap of generator

   See libcw.h/CW_GAP_{INITIAL|MIN|MAX} for initial/minimal/maximal
   value of gap.
   errno is set to EINVAL if \p new_value is out of range.

   Notice that this function also sets the same gap value for
   library's receiver.

   testedin::test_parameter_ranges()

   \param new_value - new value of gap to be assigned to generator

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_set_gap(int new_value)
{
	int rv = cw_gen_set_gap_internal(cw_generator, new_value);
	return rv;
}





/**
   \brief Set sending weighting for generator

   See libcw.h/CW_WEIGHTING_{INITIAL|MIN|MAX} for initial/minimal/maximal
   value of weighting.
   errno is set to EINVAL if \p new_value is out of range.

   testedin::test_parameter_ranges()

   \param new_value - new value of weighting to be assigned for generator

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_set_weighting(int new_value)
{
	int rv = cw_gen_set_weighting_internal(cw_generator, new_value);
	return rv;
}





/**
   \brief Get sending speed from generator

   testedin::test_parameter_ranges()

   \return current value of the generator's send speed
*/
int cw_get_send_speed(void)
{
	return cw_gen_get_speed_internal(cw_generator);
}





/**
   \brief Get frequency from generator

   Function returns "frequency" parameter of generator,
   even if the generator is stopped, or volume of generated sound is zero.

   testedin::test_parameter_ranges()

   \return current value of generator's frequency
*/
int cw_get_frequency(void)
{
	return cw_gen_get_frequency_internal(cw_generator);
}





/**
   \brief Get sound volume from generator

   Function returns "volume" parameter of generator,
   even if the generator is stopped.

   testedin::test_volume_functions()
   testedin::test_parameter_ranges()

   \return current value of generator's sound volume
*/
int cw_get_volume(void)
{
	return cw_gen_get_volume_internal(cw_generator);
}





/**
   \brief Get sending gap from generator

   testedin::test_parameter_ranges()

   \return current value of generator's sending gap
*/
int cw_get_gap(void)
{
	return cw_gen_get_gap_internal(cw_generator);
}





/**
   \brief Get sending weighting from generator

   testedin::test_parameter_ranges()

   \return current value of generator's sending weighting
*/
int cw_get_weighting(void)
{
	return cw_gen_get_weighting_internal(cw_generator);
}





/**
   \brief Get timing parameters for sending

   Return the low-level timing parameters calculated from the speed, gap,
   tolerance, and weighting set.  Parameter values are returned in
   microseconds.

   Use NULL for the pointer argument to any parameter value not required.

   \param dot_usecs
   \param dash_usecs
   \param end_of_element_usecs
   \param end_of_character_usecs
   \param end_of_word_usecs
   \param additional_usecs
   \param adjustment_usecs
*/
void cw_get_send_parameters(int *dot_usecs, int *dash_usecs,
			    int *end_of_element_usecs,
			    int *end_of_character_usecs, int *end_of_word_usecs,
			    int *additional_usecs, int *adjustment_usecs)
{
	cw_gen_get_send_parameters_internal(cw_generator,
					    dot_usecs, dash_usecs,
					    end_of_element_usecs,
					    end_of_character_usecs, end_of_word_usecs,
					    additional_usecs, adjustment_usecs);

	return;
}





/**
   \brief Low-level primitive for sending a dot mark

   Low-level primitive function able to play/send single dot mark. The
   function appends to a tone queue a normal inter-mark gap after the
   dot mark.

   testedin::test_send_primitives()

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_send_dot(void)
{
	return cw_gen_play_mark_internal(cw_generator, CW_DOT_REPRESENTATION);
}





/**
   \brief Low-level primitive for sending a dash mark

   Low-level primitive function able to play/send single dash mark.
   The function appends to a tone queue a normal inter-mark gap after
   the dash mark.

   testedin::test_send_primitives()

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_send_dash(void)
{
	return cw_gen_play_mark_internal(cw_generator, CW_DASH_REPRESENTATION);
}





/**

   The function plays space timed to exclude the expected prior
   dot/dash inter-mark gap.
   FIXME: fix this description.

   testedin::test_send_primitives()

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_send_character_space(void)
{
	return cw_gen_play_character_space_internal(cw_generator);
}





/**

   The function sends space timed to exclude both the expected prior
   dot/dash inter-mark gap and the prior end of character space.
   FIXME: fix this description.

   testedin::test_send_primitives()

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_send_word_space(void)
{
	return cw_gen_play_word_space_internal(cw_generator);
}





/**
   \brief Check, then send the given string as dots and dashes.

   The representation passed in is assumed to be a complete Morse
   character; that is, all post-character delays will be added when
   the character is sent.

   On success, the routine returns CW_SUCCESS.
   On failure, it returns CW_FAILURE, with errno set to EINVAL if any
   character of the representation is invalid, EBUSY if the sound card,
   console speaker, or keying system is busy, or EAGAIN if the tone
   queue is full, or if there is insufficient space to queue the tones
   or the representation.

   testedin::test_representations()

   \param representation - representation to send

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_send_representation(const char *representation)
{
	return cw_gen_play_representation_internal(cw_generator, representation, false);
}





/**
   \brief Check, then send the given string as dots and dashes

   The \p representation passed in is assumed to be only part of a larger
   Morse representation; that is, no post-character delays will be added
   when the character is sent.

   On success, the routine returns CW_SUCCESS.
   On failure, it returns CW_FAILURE, with errno set to EINVAL if any
   character of the representation is invalid, EBUSY if the sound card,
   console speaker, or keying system is busy, or EAGAIN if the tone queue
   is full, or if there is insufficient space to queue the tones for
   the representation.

   testedin::test_representations()
*/
int cw_send_representation_partial(const char *representation)
{
	return cw_gen_play_representation_internal(cw_generator, representation, true);
}





/**
   \brief Look up and send a given ASCII character as Morse

   The end of character delay is appended to the Morse sent.

   On success the routine returns CW_SUCCESS.
   On failure the function returns CW_FAILURE and sets errno.

   errno is set to ENOENT if the given character \p c is not a valid
   Morse character.
   errno is set to EBUSY if current audio sink or keying system is
   busy.
   errno is set to EAGAIN if the generator's tone queue is full, or if
   there is insufficient space to queue the tones for the character.

   This routine returns as soon as the character has been successfully
   queued for sending; that is, almost immediately.  The actual sending
   happens in background processing.  See cw_wait_for_tone() and
   cw_wait_for_tone_queue() for ways to check the progress of sending.

   testedin::test_send_character_and_string()

   \param c - character to send

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_send_character(char c)
{
	return cw_gen_play_character_internal(cw_generator, c);
}





/**
   \brief Look up and send a given ASCII character as Morse code

   "partial" means that the "end of character" delay is not appended
   to the Morse code sent by the function, to support the formation of
   combination characters.

   On success the function returns CW_SUCCESS.
   On failure the function returns CW_FAILURE and sets errno.

   errno is set to ENOENT if the given character \p c is not a valid
   Morse character.
   errno is set to EBUSY if the audio sink or keying system is busy.
   errno is set to EAGAIN if the tone queue is full, or if there is
   insufficient space to queue the tones for the character.

   This routine queues its arguments for background processing.  See
   cw_wait_for_tone() and cw_wait_for_tone_queue() for ways to check
   the progress of sending.

   \param c - character to send

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_send_character_partial(char c)
{
	return cw_gen_play_character_parital_internal(cw_generator, c);
}





/**
   \brief Send a given ASCII string in Morse code

   errno is set to ENOENT if any character in the string is not a
   valid Morse character.

   errno is set to EBUSY if audio sink or keying system is busy.

   errno is set to EAGAIN if the tone queue is full or if the tone
   queue runs out of space part way through queueing the string.
   However, an indeterminate number of the characters from the string
   will have already been queued.

   For safety, clients can ensure the tone queue is empty before
   queueing a string, or use cw_send_character() if they need finer
   control.

   This routine queues its arguments for background processing, the
   actual sending happens in background processing. See
   cw_wait_for_tone() and cw_wait_for_tone_queue() for ways to check
   the progress of sending.

   testedin::test_send_character_and_string()

   \param string - string to send

   \return CW_SUCCESS on success
   \return CW_FAILURE on failure
*/
int cw_send_string(const char *string)
{
	return cw_gen_play_string_internal(cw_generator, string);
}
