/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef _LIBCW_TQ_INTERNAL_H_
#define _LIBCW_TQ_INTERNAL_H_




#include "libcw_tq.h"
#include "libcw_utils.h"




/* Internal functions of this module, exposed to unit tests code. */




CW_STATIC_FUNC cw_ret_t cw_tq_set_capacity_internal(cw_tone_queue_t * tq, size_t capacity, size_t high_water_mark);
CW_STATIC_FUNC size_t cw_tq_get_high_water_mark_internal(const cw_tone_queue_t * tq) __attribute__((unused));
CW_STATIC_FUNC size_t cw_tq_prev_index_internal(const cw_tone_queue_t * tq, size_t ind) __attribute__((unused));
CW_STATIC_FUNC size_t cw_tq_next_index_internal(const cw_tone_queue_t * tq, size_t ind);
CW_STATIC_FUNC bool   cw_tq_dequeue_sub_internal(cw_tone_queue_t * tq, cw_tone_t * tone);
CW_STATIC_FUNC void   cw_tq_make_empty_internal(cw_tone_queue_t * tq);




#endif /* #ifndef _LIBCW_TQ_INTERNAL_H_ */
