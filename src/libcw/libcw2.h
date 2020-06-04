#ifndef _LIBCW_2_H_
#define _LIBCW_2_H_




#if defined(__cplusplus)
extern "C"
{
#endif




/* Instances of cw_gen_t, cw_rec_t, cw_key_t and cw_tq_t types have
   ::label[LIBCW_INSTANCE_LABEL_SIZE] field. The field can be used by
   client code to distinguish instances of the same type.

   This size includes space for terminating NUL. */
#define LIBCW_INSTANCE_LABEL_SIZE 16




#include "libcw_gen.h"




typedef int cw_ret_t;




/**
   @brief Get version of libcw shared library
*/
cw_ret_t cw_get_lib_version(int * current, int * revision, int * age);




/**
   @brief Get version of unixcw package
*/
cw_ret_t cw_get_package_version(int * major, int * minor, int * maintenance);




/* **************** Generator **************** */




/* Basic generator functions. */
cw_gen_t * cw_gen_new(int sound_system, const char * device);
void       cw_gen_delete(cw_gen_t ** gen);
int        cw_gen_stop(cw_gen_t * gen);
int        cw_gen_start(cw_gen_t * gen);




/**
   @brief Set label (name) of given generator instance

   The label can be used by client code to distinguish different
   instances of generators. The label is used in library's debug
   messages.

   @p gen can't be NULL.

   @p label can't be NULL.

   @p label can be an empty string: generator's label will become empty
   string.

   @p label longer than (`LIBCW_INSTANCE_LABEL_SIZE`-1) characters will
   be truncated and
   `cw_debug_object:CW_DEBUG_CLIENT_CODE:CW_DEBUG_WARNING` will be
   logged. This is not treated as error: function will not return
   `CW_FAILURE` because of that.

   @param[in] gen generator for which to set label
   @param[in] label new label to set for given @p gen

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise (e.g. @p gen or @p label is NULL)
*/
int cw_gen_set_label(cw_gen_t * gen, const char * label);




/**
   @brief Get label (name) of given generator instance

   @p gen and @p label can't be NULL.

   @p label should be a buffer of size at least
   `LIBCW_INSTANCE_LABEL_SIZE`.

   @p size should have value equal to size of @p label char buffer.

   @p size is not validated: it could be smaller than
   `LIBCW_INSTANCE_LABEL_SIZE`, it could be zero. Function's caller
   will get only as many characters of generator's label as he asked
   for.

   @param[in] gen generator from which to get label
   @param[in] label output buffer
   @param[out] size total size of output buffer @p label

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure (e.g. @p gen or @p label is NULL)
*/
int cw_gen_get_label(const cw_gen_t * gen, char * label, size_t size);




int cw_gen_set_tone_slope(cw_gen_t * gen, int slope_shape, int slope_duration);

/* Setters of generator's basic parameters. */
int cw_gen_set_speed(cw_gen_t * gen, int new_value);
int cw_gen_set_frequency(cw_gen_t * gen, int new_value);
int cw_gen_set_volume(cw_gen_t * gen, int new_value);
int cw_gen_set_gap(cw_gen_t * gen, int new_value);
int cw_gen_set_weighting(cw_gen_t * gen, int new_value);


/* Getters of generator's basic parameters. */
int cw_gen_get_speed(const cw_gen_t * gen);
int cw_gen_get_frequency(const cw_gen_t * gen);
int cw_gen_get_volume(const cw_gen_t * gen);
int cw_gen_get_gap(const cw_gen_t * gen);
int cw_gen_get_weighting(const cw_gen_t * gen);

int cw_gen_enqueue_character(cw_gen_t * gen, char c);
int cw_gen_enqueue_string(cw_gen_t * gen, const char * string);
int cw_gen_wait_for_queue_level(cw_gen_t * gen, size_t level);

void cw_gen_flush_queue(cw_gen_t * gen);
const char *cw_gen_get_sound_device(cw_gen_t const * gen);
int cw_gen_get_sound_system(cw_gen_t const * gen);
size_t cw_gen_get_queue_length(cw_gen_t const * gen);
int cw_gen_register_low_level_callback(cw_gen_t * gen, cw_queue_low_callback_t callback_func, void * callback_arg, size_t level);
int cw_gen_wait_for_tone(cw_gen_t * gen);
bool cw_gen_is_queue_full(cw_gen_t const * gen);




/* **************** Key **************** */




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

   @p label longer than (`LIBCW_INSTANCE_LABEL_SIZE`-1) characters will
   be truncated and
   `cw_debug_object:CW_DEBUG_CLIENT_CODE:CW_DEBUG_WARNING` will be
   logged. This is not treated as error: function will not return
   `CW_FAILURE` because of that.

   @param[in] key key for which to set label
   @param[in] label new label to set for given @p key

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise (e.g. @p key or @p label is NULL)
*/
int cw_key_set_label(cw_key_t * key, const char * label);




/**
   @brief Get label (name) of given key instance

   @p key and @p label can't be NULL.

   @p label should be a buffer of size at least
   `LIBCW_INSTANCE_LABEL_SIZE`.

   @p size should have value equal to size of @p label char buffer.

   @p size is not validated: it could be smaller than
   `LIBCW_INSTANCE_LABEL_SIZE`, it could be zero. Function's caller
   will get only as many characters of key's label as he asked
   for.

   @param[in] key key from which to get label
   @param[in] label output buffer
   @param[out] size total size of output buffer @p label

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure (e.g. @p key or @p label is NULL)
*/
int cw_key_get_label(const cw_key_t * key, char * label, size_t size);




void cw_key_register_keying_callback(volatile cw_key_t * key, cw_key_callback_t callback_func, void * callback_arg);
void cw_key_register_generator(volatile cw_key_t * key, cw_gen_t * gen);
void cw_key_register_receiver(volatile cw_key_t * key, cw_rec_t * rec);

void cw_key_ik_enable_curtis_mode_b(volatile cw_key_t * key);
void cw_key_ik_disable_curtis_mode_b(volatile cw_key_t * key);
bool cw_key_ik_get_curtis_mode_b(const volatile cw_key_t * key);
int  cw_key_ik_notify_paddle_event(volatile cw_key_t * key, int dot_paddle_state, int dash_paddle_state);
int  cw_key_ik_notify_dash_paddle_event(volatile cw_key_t * key, int dash_paddle_state);
int  cw_key_ik_notify_dot_paddle_event(volatile cw_key_t * key, int dot_paddle_state);
void cw_key_ik_get_paddles(const volatile cw_key_t * key, int * dot_paddle_state, int * dash_paddle_state);
int  cw_key_ik_wait_for_element(const volatile cw_key_t * key);
int  cw_key_ik_wait_for_keyer(volatile cw_key_t * key);

int  cw_key_sk_get_value(const volatile cw_key_t * key);
bool cw_key_sk_is_busy(volatile cw_key_t * key);
int  cw_key_sk_notify_event(volatile cw_key_t * key, int key_state);




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

   @p label longer than (`LIBCW_INSTANCE_LABEL_SIZE`-1) characters will
   be truncated and
   `cw_debug_object:CW_DEBUG_CLIENT_CODE:CW_DEBUG_WARNING` will be
   logged. This is not treated as error: function will not return
   `CW_FAILURE` because of that.

   @param[in] rec receiver for which to set label
   @param[in] label new label to set for given @p rec

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise (e.g. @p rec or @p label is NULL)
*/
int cw_rec_set_label(cw_rec_t * rec, const char * label);




/**
   @brief Get label (name) of given receiver instance

   @p rec and @p label can't be NULL.

   @p label should be a buffer of size at least
   `LIBCW_INSTANCE_LABEL_SIZE`.

   @p size should have value equal to size of @p label char buffer.

   @p size is not validated: it could be smaller than
   `LIBCW_INSTANCE_LABEL_SIZE`, it could be zero. Function's caller
   will get only as many characters of receiver's label as he asked
   for.

   @param[in] rec receiver from which to get label
   @param[in] label output buffer
   @param[out] size total size of output buffer @p label

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure (e.g. @p rec or @p label is NULL)
*/
int cw_rec_get_label(const cw_rec_t * rec, char * label, size_t size);




/* Helper receive functions. */
int  cw_rec_poll_character(cw_rec_t *rec, const struct timeval *timestamp, char *c, bool *is_end_of_word, bool *is_error);


/* Setters of receiver's essential parameters. */
int  cw_rec_set_speed(cw_rec_t * rec, int new_value);
int  cw_rec_set_tolerance(cw_rec_t * rec, int new_value);
int  cw_rec_set_gap(cw_rec_t * rec, int new_value);
int  cw_rec_set_noise_spike_threshold(cw_rec_t * rec, int new_value);
void cw_rec_set_adaptive_mode_internal(cw_rec_t *rec, bool adaptive);

/* Getters of receiver's essential parameters. */
float cw_rec_get_speed(const cw_rec_t *rec);
int   cw_rec_get_tolerance(const cw_rec_t * rec);
/* int   cw_rec_get_gap_internal(cw_rec_t *rec); */
int   cw_rec_get_noise_spike_threshold(const cw_rec_t * rec);
bool  cw_rec_get_adaptive_mode(const cw_rec_t * rec);




/**
   \brief Reset receiver's state

   The state includes, but is not limited to, state graph and
   representation buffer.

   Receiver's parameters and statistics are not reset by this function.
   To reset statistics use cw_rec_reset_statistics().
*/
void cw_rec_reset_state(cw_rec_t * rec);




/**
   \brief Reset receiver's statistics

   Reset the receiver's statistics by removing all records from it and
   returning it to its initial default state.

   reviewed-on 2017-02-02

   \param rec - receiver
*/
void cw_rec_reset_statistics(cw_rec_t * rec);




/* Main receive functions. */
int cw_rec_mark_begin(cw_rec_t * rec, const volatile struct timeval * timestamp);
int cw_rec_mark_end(cw_rec_t * rec, const volatile struct timeval * timestamp);
int cw_rec_add_mark(cw_rec_t * rec, const volatile struct timeval * timestamp, char mark);


/* Helper receive functions. */
int  cw_rec_poll_representation(cw_rec_t * rec, const struct timeval * timestamp, char * representation, bool * is_end_of_word, bool * is_error);

void cw_rec_enable_adaptive_mode(cw_rec_t * rec);
void cw_rec_disable_adaptive_mode(cw_rec_t * rec);
bool cw_rec_poll_is_pending_inter_word_space(cw_rec_t const * rec);




#if defined(__cplusplus)
}
#endif




#endif /* #ifndef _LIBCW_2_H_ */
