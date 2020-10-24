/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef _LIBCW_2_H_
#define _LIBCW_2_H_




#include "libcw.h"




#if defined(__cplusplus)
extern "C"
{
#endif




/* Instances of cw_gen_t, cw_rec_t, cw_key_t and cw_tq_t types have
   ::label[LIBCW_OBJECT_INSTANCE_LABEL_SIZE] field. The field can be used by
   client code to distinguish instances of the same type.

   This size includes space for terminating NUL. */
#define LIBCW_OBJECT_INSTANCE_LABEL_SIZE 32




/*
  E.g. "default", "plughw", "/dev/something"
  Includes space for terminating NUL.

  Console/ALSA/OSS device names are rather short. In case of PulseAudio we
  should expect something longer.

  pacmd list-sinks | grep "name:"
       name: <alsa_output.pci-0000_00_1b.0.analog-stereo>
*/
#define LIBCW_SOUND_DEVICE_NAME_SIZE  128




/* First and last distinct sound system. SOUNDCARD doesn't count as distinct sound system - it is a collective one. */
#define CW_SOUND_SYSTEM_FIRST CW_AUDIO_NULL
#define CW_SOUND_SYSTEM_LAST  CW_AUDIO_PA




typedef int cw_ret_t;

struct cw_key_struct;
typedef struct cw_key_struct cw_key_t;

struct cw_rec_struct;
typedef struct cw_rec_struct cw_rec_t;

typedef enum cw_audio_systems cw_sound_system_t;

typedef struct cw_gen_config_t {
	cw_sound_system_t sound_system;
	char sound_device[LIBCW_SOUND_DEVICE_NAME_SIZE];
	unsigned int alsa_period_size;
} cw_gen_config_t;




/**
   @brief Get version of libcw shared library
*/
cw_ret_t cw_get_lib_version(int * current, int * revision, int * age);




/**
   @brief Get version of unixcw package
*/
cw_ret_t cw_get_package_version(int * major, int * minor, int * maintenance);




/* **************** Generator **************** */




/**
   @brief Create new generator

   Allocate new generator, set default values of generator's fields, assign a
   sound system and device name to it, open the sound sink, return the
   generator.

   Returned pointer is owned by caller. Delete the allocated generator with
   cw_gen_delete().

   @internal
   @reviewed 2020-10-21
   @endinternal

   @param[in] gen_conf configuration of generator to be used for new generator

   @return pointer to new generator on success
   @return NULL on failure
*/
cw_gen_t * cw_gen_new(const cw_gen_config_t * gen_conf);




/**
   @brief Delete a generator

   Delete a generator that has been created with cw_gen_new().

   @internal
   @reviewed 2020-08-04
   @endinternal

   @param[in,out] gen pointer to generator to delete
*/
void cw_gen_delete(cw_gen_t ** gen);




/**
   @brief Start a generator

   Start given @p gen. As soon as there are tones enqueued in generator, the
   generator will start playing them.

   @internal
   @reviewed 2020-08-04
   @endinternal

   @param[in] gen generator to start

   @return CW_SUCCESS on success
   @return CW_FAILURE on errors
*/
cw_ret_t cw_gen_start(cw_gen_t * gen);




/**
   @brief Stop a generator

   1. Empty generator's tone queue.
   2. Silence generator's sound sink.
   3. Stop generator' "dequeue and generate" thread function.
   4. If the thread does not stop in one second, kill it.

   You have to use cw_gen_start() if you want to enqueue and
   generate tones with the same generator again.

   The function may return CW_FAILURE only when silencing of
   generator's sound sink fails.
   Otherwise function returns CW_SUCCESS.

   @internal
   @reviewed 2020-08-04
   @endinternal

   @param[in] gen generator to stop

   @return CW_SUCCESS if all three (four) actions completed (successfully)
   @return CW_FAILURE if any of the actions failed (see note above)
*/
cw_ret_t cw_gen_stop(cw_gen_t * gen);




/**
   @brief Set label (name) of given generator instance

   The label can be used by client code to distinguish different
   instances of generators. The label is used in library's debug
   messages.

   @p gen can't be NULL.

   @p label can't be NULL.

   @p label can be an empty string: generator's label will become empty
   string.

   @p label longer than (LIBCW_OBJECT_INSTANCE_LABEL_SIZE-1) characters will
   be truncated and
   `cw_debug_object:CW_DEBUG_CLIENT_CODE:CW_DEBUG_WARNING` will be
   logged. This is not treated as error: function will not return
   `CW_FAILURE` because of that.

   @internal
   @reviewed 2020-08-07
   @endinternal

   @param[in] gen generator for which to set label
   @param[in] label new label to set for given @p gen

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise (e.g. @p gen or @p label is NULL)
*/
cw_ret_t cw_gen_set_label(cw_gen_t * gen, const char * label);




/**
   @brief Get label (name) of given generator instance

   @p gen and @p label can't be NULL.

   @p label should be a buffer of size at least
   LIBCW_OBJECT_INSTANCE_LABEL_SIZE.

   @p size should have value equal to size of @p label char buffer.

   @p size is not validated: it could be smaller than
   LIBCW_OBJECT_INSTANCE_LABEL_SIZE, it could be zero. Function's caller
   will get only as many characters of generator's label as he asked
   for.

   @internal
   @reviewed 2020-08-07
   @endinternal

   @param[in] gen generator from which to get label
   @param[out] label output buffer
   @param[in] size total size of output buffer @p label

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure (e.g. @p gen or @p label is NULL)
*/
cw_ret_t cw_gen_get_label(const cw_gen_t * gen, char * label, size_t size);




cw_ret_t cw_gen_set_tone_slope(cw_gen_t * gen, int slope_shape, int slope_duration);




/**
   @brief Set sending speed of generator

   See libcw.h/CW_SPEED_{INITIAL|MIN|MAX} for initial/minimal/maximal value
   of send speed.

   @exception EINVAL @p new_value is out of range.

   @internal
   @reviewed 2020-08-05
   @endinternal

   @param[in] gen generator for which to set new value of parameter
   @param[in] new_value new value of parameter to be set

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_set_speed(cw_gen_t * gen, int new_value);




/**
   @brief Set frequency of generator

   Set frequency of sound wave generated by generator.
   The frequency must be within limits marked by CW_FREQUENCY_MIN
   and CW_FREQUENCY_MAX.

   See libcw.h/CW_FREQUENCY_{INITIAL|MIN|MAX} for initial/minimal/maximal
   value of frequency.

   @exception EINVAL @p new_value is out of range.

   @internal
   @reviewed 2020-08-05
   @endinternal

   @param[in] gen generator for which to set new value of parameter
   @param[in] new_value new value of parameter to be set

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_set_frequency(cw_gen_t * gen, int new_value);




/**
   @brief Set volume of generator

   Set volume of sound wave generated by generator.
   The volume must be within limits marked by CW_VOLUME_MIN and CW_VOLUME_MAX.

   Note that volume settings are not fully possible for the console speaker.
   In this case, volume settings greater than zero indicate console speaker
   sound is on, and setting volume to zero will turn off console speaker
   sound.

   See libcw.h/CW_VOLUME_{INITIAL|MIN|MAX} for initial/minimal/maximal
   value of volume.

   @exception EINVAL if @p new_value is out of range.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator for which to set new value of parameter
   @param[in] new_value new value of parameter to be set

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_set_volume(cw_gen_t * gen, int new_value);




/**
   @brief Set sending gap of generator

   See libcw.h/CW_GAP_{INITIAL|MIN|MAX} for initial/minimal/maximal
   value of gap.

   @exception EINVAL if @p new_value is out of range.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator for which to set new value of parameter
   @param[in] new_value new value of parameter to be set

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_set_gap(cw_gen_t * gen, int new_value);




/**
   @brief Set sending weighting for generator

   See libcw.h/CW_WEIGHTING_{INITIAL|MIN|MAX} for initial/minimal/maximal
   value of weighting.

   @exception EINVAL if @p new_value is out of range.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in,out] gen generator for which to set new value of parameter
   @param[in] new_value new value of parameter to be set

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_set_weighting(cw_gen_t * gen, int new_value);




/**
   @brief Get sending speed from generator

   Returned value is in range CW_SPEED_MIN-CW_SPEED_MAX [wpm].

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator from which to get current value of parameter

   @return current value of the generator's send speed
*/
int cw_gen_get_speed(const cw_gen_t * gen);




/**
   @brief Get frequency from generator

   Function returns "frequency" parameter of generator,
   even if the generator is stopped, or volume of generated sound is zero.

   Returned value is in range CW_FREQUENCY_MIN-CW_FREQUENCY_MAX [Hz].

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator from which to get current value of parameter

   @return current value of generator's frequency
*/
int cw_gen_get_frequency(const cw_gen_t * gen);




/**
   @brief Get sound volume from generator

   Function returns "volume" parameter of generator, even if the
   generator is stopped.  Returned value is in range
   CW_VOLUME_MIN-CW_VOLUME_MAX [%].

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator from which to get current value of parameter

   @return current value of generator's sound volume
*/
int cw_gen_get_volume(const cw_gen_t * gen);




/**
   @brief Get sending gap from generator

   Returned value is in range CW_GAP_MIN-CW_GAP_MAX.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator from which to get current value of parameter

   @return current value of generator's sending gap
*/
int cw_gen_get_gap(const cw_gen_t * gen);




/**
   @brief Get sending weighting from generator

   Returned value is in range CW_WEIGHTING_MIN-CW_WEIGHTING_MAX.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator from which to get current value of parameter

   @return current value of generator's sending weighting
*/
int cw_gen_get_weighting(const cw_gen_t * gen);




/**
   @brief Enqueue the given representation in generator, to be sent using Morse code

   Function enqueues given @p representation using given @p gen.  *Every*
   mark (Dot/Dash) from the @p representation is followed by a standard
   inter-mark-space. Inter-character-space is added at the end.

   @p representation must be valid, i.e. it must meet criteria of
   cw_representation_is_valid().

   @exception EAGAIN there is not enough space in tone queue to enqueue @p
   representation.
   @exception EINVAL representation is not valid.

   @internal
   @reviewed 2020-08-29
   @endinternal

   @param[in] gen generator used to enqueue the representation
   @param[in] representation representation to enqueue

   @return CW_FAILURE on failure
   @return CW_SUCCESS on success
*/
cw_ret_t cw_gen_enqueue_representation(cw_gen_t * gen, const char * representation);




/**
   @brief Enqueue the given representation in generator, to be sent using Morse code

   Function enqueues given @p representation using given @p gen.  *Every*
   mark (Dot/Dash) from the @p representation is followed by a standard
   inter-mark-space. Inter-character-space is NOT added at the end (hence
   no_ics in function's name).

   @p representation must be valid, i.e. it must meet criteria of
   cw_representation_is_valid().

   @exception EAGAIN there is not enough space in tone queue to enqueue @p
   representation.
   @exception EINVAL representation is not valid.

   @internal
   @reviewed 2020-08-29
   @endinternal

   @param[in] gen generator used to enqueue the representation
   @param[in] representation representation to enqueue

   @return CW_FAILURE on failure
   @return CW_SUCCESS on success
*/
cw_ret_t cw_gen_enqueue_representation_no_ics(cw_gen_t * gen, const char * representation);




/**
   @brief Enqueue a given ASCII character in generator, to be sent using Morse code

   Inter-mark-space and inter-character-space is appended at the end of
   enqueued Marks.

   @exception ENOENT the given character @p character is not a character that
   can be represented in Morse code.

   @exception EBUSY generator's sound sink or keying system is busy.

   @exception EAGAIN generator's tone queue is full, or there is
   insufficient space to queue the tones for the character.

   This routine returns as soon as the character and trailing spaces
   (inter-mark-space and inter-character-space) have been successfully queued for
   sending/playing by the generator, without waiting for generator to even
   start playing the character.  The actual sending happens in background
   processing. See cw_gen_wait_for_end_of_current_tone() and
   cw_gen_wait_for_queue_level() for ways to check the progress of sending.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator to enqueue the character to
   @param[in] character character to enqueue in generator

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_enqueue_character(cw_gen_t * gen, char character);




/**
   @brief Enqueue a given ASCII character in generator, to be sent using Morse code

   "_no_ics" means that the inter-character-space is not appended at the end
   of Marks and Spaces enqueued in generator (but the last inter-mark-space
   is). This enables the formation of combination characters by client code.

   This routine returns as soon as the character has been successfully queued
   for sending/playing by the generator, without waiting for generator to
   even start playing the character. The actual sending happens in background
   processing.  See cw_gen_wait_for_end_of_current_tone() and
   cw_gen_wait_for_queue() for ways to check the progress of sending.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @exception ENOENT the given character @p character is not a character that
   can be represented in Morse code.

   @exception EBUSY generator's sound sink or keying system is busy.

   @exception EAGAIN generator's tone queue is full, or there is
   insufficient space to queue the tones for the character.

   @param[in] gen generator to use
   @param[in] character character to enqueue

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_enqueue_character_no_ics(cw_gen_t * gen, char character);





/**
   @brief Enqueue a given ASCII string in generator, to be sent using Morse code

   For safety, clients can ensure the tone queue is empty before
   queueing a string, or use cw_gen_enqueue_character() if they
   need finer control.

   @internal
   TODO: how client can ensure the tone queue is empty?
   @endinternal

   This routine returns as soon as the string has been successfully queued
   for sending/playing by the generator, without waiting for generator to
   even start playing the string. The actual playing/sending happens in
   background. See cw_gen_wait_for_end_of_current_tone() and
   cw_gen_wait_for_queue() for ways to check the progress of sending.

   @exception ENOENT @p string argument is invalid (one or more characters in
   the string is not a valid Morse character). No tones from such string are
   going to be enqueued.

   @exception EBUSY generator's sound sink or keying system is busy

   @exception EAGAIN generator's tone queue is full or the tone queue
   is likely to run out of space part way through queueing the string.
   However, an indeterminate number of the characters from the string
   will have already been queued.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator to use
   @param[in] string string to enqueue

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_enqueue_string(cw_gen_t * gen, const char * string);




/**
   @brief Wait for generator's tone queue to drain until only as many tones as given in @p level remain queued

   This function is for use by programs that want to optimize
   themselves to avoid the cleanup that happens when generator's tone
   queue drains completely. Such programs have a short time in which
   to add more tones to the queue.

   @internal
   TODO: in current implementation of library most of the cleanup has been removed.
   @endinternal

   The function returns when queue's level is equal to or lower than @p
   level.  If at the time of function call the level of queue is already
   equal to or lower than @p level, function returns immediately.

   Notice that generator must be running (started with cw_gen_start())
   when this function is called, otherwise it will be waiting forever
   for a change of tone queue's level that will never happen.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator on which to wait
   @param[in] level level in queue, at which to return

   @return CW_SUCCESS
*/
cw_ret_t cw_gen_wait_for_queue_level(cw_gen_t * gen, size_t level);




/**
   @brief Cancel all pending queued tones in a generator, and return to silence

   If there is a tone in progress, the function will wait until this
   last one has completed, then silence the tones.

   @internal
   TODO: verify the above comment. Do we really wait until last tone has
   completed? What if the tone is very long?
   @endinternal

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator to flush
*/
void cw_gen_flush_queue(cw_gen_t * gen);




/**
   @brief Remove one last character from queue of already enqueued characters

   If the character is not actually played by sound sink yet, library may be
   able to remove the character. The character's Dots and Dashes won't be
   played.

   This function may be useful if user presses backspace in UI to remove/undo
   a character.

   If your application doesn't enqueue whole characters or strings of
   characters but is using low-level cw_gen_enqueue_mark_internal() functions
   to enqueue individual Marks, don't use this function. This function won't
   be able to recognize whole characters and is likely to remove more tones
   than expected.

   @reviewed 2020-08-24

   @param[in] gen generator from which to remove the last character

   @return CW_SUCCESS if function managed to remove a last character before it has been played
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_gen_remove_last_character(cw_gen_t * gen);




/**
   @brief Get string with generator's sound device path/name

   Device name is copied to @p buffer, a memory area owned by caller.

   @internal
   @reviewed 2020-08-07
   @endinternal

   @param[in] gen generator from which to get sound device name
   @param[out] buffer space for returned device name@
   @param[in] size size of @p buffer, including space for terminating NUL

   @return CW_SUCCESS
*/
cw_ret_t cw_gen_get_sound_device(cw_gen_t const * gen, char * buffer, size_t size);




/**
   @brief Get id of sound system used by given generator (one of enum cw_audio_system values)

   You can use cw_get_audio_system_label() to get string corresponding
   to value returned by this function.

   @internal
   @reviewed 2020-08-07
   @endinternal

   @param[in] gen generator from which to get sound system id

   @return sound system's id
*/
int cw_gen_get_sound_system(const cw_gen_t * gen);




/**
   @brief Get length of tone queue of the generator

   @internal
   @reviewed 2020-08-07
   @endinternal

   @param[in] gen generator from which to get tone queue length

   @return length of tone queue
*/
size_t cw_gen_get_queue_length(const cw_gen_t * gen);




typedef void (* cw_queue_low_callback_t)(void *);
/**
   @brief Register a 'low level in tone queue' callback for given generator

   @internal
   See also cw_tq_register_low_level_callback_internal()
   @endinternal

   @internal
   @reviewed 2020-08-07
   @endinternal

   @param[in,out] gen generator
   @param[in] callback_func callback function to be registered
   @param[in] callback_arg pointer to be passed to the callback when the callback is called
   @param[in] level level of tone queue at which the callback will be called.

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_gen_register_low_level_callback(cw_gen_t * gen, cw_queue_low_callback_t callback_func, void * callback_arg, size_t level);




/**
   @brief Wait for the current tone to complete

   @internal
   @reviewed 2020-08-07
   @endinternal

   @param[in] gen generator to wait on

   @return CW_SUCCESS
*/
cw_ret_t cw_gen_wait_for_end_of_current_tone(cw_gen_t * gen);




/**
   @brief See if generator's tone queue is full

   @internal
   @reviewed 2020-08-07
   @endinternal

   @return true if the queue is full
   @return false otherwise
*/
bool cw_gen_is_queue_full(cw_gen_t const * gen);




typedef void (* cw_gen_value_tracking_callback_t)(void * callback_arg, int state);
void cw_gen_register_value_tracking_callback_internal(cw_gen_t * gen, cw_gen_value_tracking_callback_t callback_func, void * callback_arg);




/* **************** Key **************** */




/*
  For straight key this is a value of a single (and the only) element of a
  key.

  For a iambic keyer and other key types this a value of one of two paddles.

  A state of key's internal graph (state machine) can have values that have
  _STATE_ in their names. I'm making here a distinction between "value" and
  "state", reserving word "state" for state of the internal graph (state
  machine).
*/
typedef enum {
	CW_KEY_VALUE_OPEN = CW_KEY_STATE_OPEN,      /* Space, no sound. */
	CW_KEY_VALUE_CLOSED = CW_KEY_STATE_CLOSED,  /* Mark, sound. */
} cw_key_value_t;




cw_key_t * cw_key_new(void);
void cw_key_delete(cw_key_t ** key);




/**
   @brief Set label (name) of given key instance

   The label can be used by client code to distinguish different
   instances of keys. The label is used in library's debug
   messages.

   @p key can't be NULL.

   @p label can't be NULL.

   @p label can be an empty string: key's label will become empty
   string.

   @p label longer than (LIBCW_OBJECT_INSTANCE_LABEL_SIZE-1) characters will
   be truncated and
   `cw_debug_object:CW_DEBUG_CLIENT_CODE:CW_DEBUG_WARNING` will be
   logged. This is not treated as error: function will not return
   `CW_FAILURE` because of that.

   @param[in] key key for which to set label
   @param[in] label new label to set for given @p key

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise (e.g. @p key or @p label is NULL)
*/
cw_ret_t cw_key_set_label(cw_key_t * key, const char * label);




/**
   @brief Get label (name) of given key instance

   @p key and @p label can't be NULL.

   @p label should be a buffer of size at least
   LIBCW_OBJECT_INSTANCE_LABEL_SIZE.

   @p size should have value equal to size of @p label char buffer.

   @p size is not validated: it could be smaller than
   LIBCW_OBJECT_INSTANCE_LABEL_SIZE, it could be zero. Function's caller will
   get only as many characters of key's label as he asked for.

   @param[in] key key from which to get label
   @param[out] label output buffer
   @param[in] size total size of output buffer @p label

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure (e.g. @p key or @p label is NULL)
*/
cw_ret_t cw_key_get_label(const cw_key_t * key, char * label, size_t size);




void cw_key_register_generator(volatile cw_key_t * key, cw_gen_t * gen);
void cw_key_register_receiver(volatile cw_key_t * key, cw_rec_t * rec);

void cw_key_ik_enable_curtis_mode_b(volatile cw_key_t * key);
void cw_key_ik_disable_curtis_mode_b(volatile cw_key_t * key);
bool cw_key_ik_get_curtis_mode_b(const volatile cw_key_t * key);
cw_ret_t cw_key_ik_notify_paddle_event(volatile cw_key_t * key, cw_key_value_t dot_paddle_value, cw_key_value_t dash_paddle_value);
cw_ret_t cw_key_ik_notify_dash_paddle_event(volatile cw_key_t * key, cw_key_value_t dash_paddle_value);
cw_ret_t cw_key_ik_notify_dot_paddle_event(volatile cw_key_t * key, cw_key_value_t dot_paddle_value);
void cw_key_ik_get_paddles(const volatile cw_key_t * key, cw_key_value_t * dot_paddle_value, cw_key_value_t * dash_paddle_value);
cw_ret_t cw_key_ik_wait_for_end_of_current_element(const volatile cw_key_t * key);
cw_ret_t cw_key_ik_wait_for_keyer(volatile cw_key_t * key);

cw_ret_t cw_key_sk_get_value(const volatile cw_key_t * key, cw_key_value_t * key_value);
cw_ret_t cw_key_sk_set_value(volatile cw_key_t * key, cw_key_value_t key_value);




/* **************** Receiver **************** */




/* Creator and destructor. */
cw_rec_t * cw_rec_new(void);
void       cw_rec_delete(cw_rec_t ** rec);




/**
   @brief Set label (name) of given receiver instance

   The label can be used by client code to distinguish different
   instances of receivers. The label is used in library's debug
   messages.

   @p rec can't be NULL.

   @p label can't be NULL.

   @p label can be an empty string: receiver's label will become empty
   string.

   @p label longer than (LIBCW_OBJECT_INSTANCE_LABEL_SIZE-1) characters will
   be truncated and
   `cw_debug_object:CW_DEBUG_CLIENT_CODE:CW_DEBUG_WARNING` will be
   logged. This is not treated as error: function will not return
   `CW_FAILURE` because of that.

   @param[in,out] rec receiver for which to set label
   @param[in] label new label to set for given @p rec

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise (e.g. @p rec or @p label is NULL)
*/
int cw_rec_set_label(cw_rec_t * rec, const char * label);




/**
   @brief Get label (name) of given receiver instance

   @p rec and @p label can't be NULL.

   @p label should be a buffer of size at least
   LIBCW_OBJECT_INSTANCE_LABEL_SIZE.

   @p size should have value equal to size of @p label char buffer.

   @p size is not validated: it could be smaller than
   LIBCW_OBJECT_INSTANCE_LABEL_SIZE, it could be zero. Function's caller will
   get only as many characters of receiver's label as he asked for.

   @param[in] rec receiver from which to get label
   @param[out] label output buffer
   @param[in] size total size of output buffer @p label

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure (e.g. @p rec or @p label is NULL)
*/
int cw_rec_get_label(const cw_rec_t * rec, char * label, size_t size);




/* Helper receive functions. */
cw_ret_t cw_rec_poll_character(cw_rec_t * rec, const struct timeval * timestamp, char * character, bool * is_end_of_word, bool * is_error);


/* Setters of receiver's essential parameters. */
cw_ret_t cw_rec_set_speed(cw_rec_t * rec, int new_value);
cw_ret_t cw_rec_set_tolerance(cw_rec_t * rec, int new_value);
cw_ret_t cw_rec_set_gap(cw_rec_t * rec, int new_value);
cw_ret_t cw_rec_set_noise_spike_threshold(cw_rec_t * rec, int new_value);
void cw_rec_set_adaptive_mode_internal(cw_rec_t * rec, bool adaptive);

/* Getters of receiver's essential parameters. */
float cw_rec_get_speed(const cw_rec_t * rec);
int   cw_rec_get_tolerance(const cw_rec_t * rec);
/* int   cw_rec_get_gap_internal(cw_rec_t * rec); */
int   cw_rec_get_noise_spike_threshold(const cw_rec_t * rec);
bool  cw_rec_get_adaptive_mode(const cw_rec_t * rec);




/**
   @brief Reset receiver's state machine to initial state

   The state includes, but is not limited to, state graph and
   representation buffer.

   Receiver's parameters and statistics are not reset by this function.
   To reset statistics use cw_rec_reset_statistics().

   @param[in,out] rec receiver to reset
*/
void cw_rec_reset_state(cw_rec_t * rec);




/**
   @brief Reset receiver's statistics

   Reset the receiver's statistics by removing all records from it and
   returning it to its initial default state.

   #reviewed 2017-02-02

   @param[in,out] rec receiver for which to reset statistics
*/
void cw_rec_reset_statistics(cw_rec_t * rec);




/* Main receive functions. */
cw_ret_t cw_rec_mark_begin(cw_rec_t * rec, const struct timeval * timestamp);
cw_ret_t cw_rec_mark_end(cw_rec_t * rec, const struct timeval * timestamp);
cw_ret_t cw_rec_add_mark(cw_rec_t * rec, const struct timeval * timestamp, char mark);


/* Helper receive functions. */
cw_ret_t cw_rec_poll_representation(cw_rec_t * rec, const struct timeval * timestamp, char * representation, bool * is_end_of_word, bool * is_error);

void cw_rec_enable_adaptive_mode(cw_rec_t * rec);
void cw_rec_disable_adaptive_mode(cw_rec_t * rec);




#if defined(__cplusplus)
}
#endif




#endif /* #ifndef _LIBCW_2_H_ */
