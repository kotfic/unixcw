/*
 * Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
 * Copyright (C) 2011-2019  Kamil Ignacak (acerion@wp.pl)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */





#include "libcw_utils_tests.h"
#include "libcw_data_tests.h"
#include "libcw_debug_tests.h"
#include "libcw_tq_tests.h"
#include "libcw_gen_tests.h"
#include "libcw_key_tests.h"
#include "libcw_rec_tests.h"

#include "test_framework.h"

#include "libcw_legacy_api_tests.h"
#include "libcw_legacy_api_tests_rec_poll.h"
#include "libcw_test_tq_short_space.h"
#include "libcw_gen_tests_state_callback.h"




#define WITH_LIBCW_LEGACY_API 1




cw_test_set_t cw_test_sets[] = {
#if WITH_LIBCW_LEGACY_API
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_LEGACY,

		{ LIBCW_TEST_TOPIC_TQ, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_CONSOLE, CW_AUDIO_OSS, CW_AUDIO_ALSA, CW_AUDIO_PA, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. */

		{
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_setup),

			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_cw_wait_for_tone),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_cw_wait_for_tone_queue),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_cw_queue_tone),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_empty_tone_queue),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_full_tone_queue),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_tone_queue_callback),

			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_teardown),

			LIBCW_TEST_FUNCTION_INSERT(NULL),
		}
	},
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_LEGACY,

		{ LIBCW_TEST_TOPIC_GEN, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_CONSOLE, CW_AUDIO_OSS, CW_AUDIO_ALSA, CW_AUDIO_PA, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. */

		{
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_setup),

			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_volume_functions),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_send_primitives),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_send_character_and_string),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_representations),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_teardown),

			/* These functions create and delete a
			   generator on their own, so they have to be put
			   after legacy_api_test_teardown() that
			   deletes a generator. */
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_basic_gen_operations),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_gen_remove_last_character),

			LIBCW_TEST_FUNCTION_INSERT(NULL),
		}
	},
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_LEGACY,

		{ LIBCW_TEST_TOPIC_KEY, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_CONSOLE, CW_AUDIO_OSS, CW_AUDIO_ALSA, CW_AUDIO_PA, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. */

		{
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_setup),

			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_iambic_key_dot),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_iambic_key_dash),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_iambic_key_alternating),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_iambic_key_none),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_straight_key),

			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_teardown),

			LIBCW_TEST_FUNCTION_INSERT(NULL),
		}
	},
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_LEGACY,

		{ LIBCW_TEST_TOPIC_OTHER, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_CONSOLE, CW_AUDIO_OSS, CW_AUDIO_ALSA, CW_AUDIO_PA, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. */

		{
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_setup),

			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_low_level_gen_parameters),
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_parameter_ranges),
			//LIBCW_TEST_FUNCTION_INSERT(legacy_api_cw_test_delayed_release),
			//LIBCW_TEST_FUNCTION_INSERT(legacy_api_cw_test_signal_handling), /* FIXME - not sure why this test fails :( */

			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_teardown),

			LIBCW_TEST_FUNCTION_INSERT(NULL),
		}
	},
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_LEGACY,

		{ LIBCW_TEST_TOPIC_OTHER, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_CONSOLE, CW_AUDIO_OSS, CW_AUDIO_ALSA, CW_AUDIO_PA, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. */

		{
			/* This test does its own generator setup and deconfig. */
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_tq_short_space),

			LIBCW_TEST_FUNCTION_INSERT(NULL),
		}
	},
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_LEGACY,

		{ LIBCW_TEST_TOPIC_REC, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_CONSOLE, CW_AUDIO_OSS, CW_AUDIO_ALSA, CW_AUDIO_PA, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. */

		{
			/* This test does its own generator setup and deconfig. */
			LIBCW_TEST_FUNCTION_INSERT(legacy_api_test_rec_poll),

			LIBCW_TEST_FUNCTION_INSERT(NULL),
		}
	},
#endif /* #if WITH_LIBCW_LEGACY_API */
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_MODERN,

		{ LIBCW_TEST_TOPIC_OTHER, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. */

		{
			/* cw_utils topic */
			LIBCW_TEST_FUNCTION_INSERT(test_cw_timestamp_compare_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_timestamp_validate_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_usecs_to_timespec_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_version_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_license_internal),

			/* cw_debug topic */
			LIBCW_TEST_FUNCTION_INSERT(test_cw_debug_flags_internal),

			LIBCW_TEST_FUNCTION_INSERT(NULL),
		}
	},
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_MODERN,

		{ LIBCW_TEST_TOPIC_DATA, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. */

		{
			/* cw_data topic */
			LIBCW_TEST_FUNCTION_INSERT(test_cw_representation_to_hash_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_representation_to_character_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_representation_to_character_internal_speed_gain),

			LIBCW_TEST_FUNCTION_INSERT(test_data_main_table_get_count),
			LIBCW_TEST_FUNCTION_INSERT(test_data_main_table_get_contents),
			LIBCW_TEST_FUNCTION_INSERT(test_data_main_table_get_representation_len_max),
			LIBCW_TEST_FUNCTION_INSERT(test_data_main_table_lookups),

			LIBCW_TEST_FUNCTION_INSERT(test_prosign_lookups_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_phonetic_lookups_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_validate_character_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_validate_string_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_validate_representation_internal),

			LIBCW_TEST_FUNCTION_INSERT(NULL),
		}

	},
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_MODERN,

		{ LIBCW_TEST_TOPIC_TQ, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_CONSOLE, CW_AUDIO_OSS, CW_AUDIO_ALSA, CW_AUDIO_PA, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. All sound systems are included in tests of tq, because sometimes a running gen is necessary. */

		{
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_test_capacity_A),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_test_capacity_B),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_wait_for_level_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_is_full_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_enqueue_dequeue_internal),
#if 0
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_enqueue_args_internal),
#endif
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_enqueue_internal_B),

			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_new_delete_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_capacity_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_length_internal_1),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_prev_index_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_next_index_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_callback),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_gen_operations_A),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_gen_operations_B),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_tq_operations_C),

			LIBCW_TEST_FUNCTION_INSERT(NULL),
		}
	},
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_MODERN,

		{ LIBCW_TEST_TOPIC_GEN, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_CONSOLE, CW_AUDIO_OSS, CW_AUDIO_ALSA, CW_AUDIO_PA, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. */

		{
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_new_delete),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_new_start_delete),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_new_stop_delete),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_new_start_stop_delete),

			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_set_tone_slope),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_tone_slope_shape_enums),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_get_timing_parameters_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_parameter_getters_setters),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_volume_functions),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_enqueue_primitives),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_enqueue_representations),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_enqueue_character),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_enqueue_string),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_remove_last_character),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_forever_internal),

			LIBCW_TEST_FUNCTION_INSERT(test_cw_gen_state_callback),

			LIBCW_TEST_FUNCTION_INSERT(NULL),
		}
	},
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_MODERN,

		{ LIBCW_TEST_TOPIC_KEY, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_CONSOLE, CW_AUDIO_OSS, CW_AUDIO_ALSA, CW_AUDIO_PA, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. */

		{
			LIBCW_TEST_FUNCTION_INSERT(test_keyer),
			LIBCW_TEST_FUNCTION_INSERT(test_straight_key),

			LIBCW_TEST_FUNCTION_INSERT(NULL),
		}
	},
	{
		LIBCW_TEST_SET_VALID,
		LIBCW_TEST_API_MODERN,

		{ LIBCW_TEST_TOPIC_REC, LIBCW_TEST_TOPIC_MAX }, /* Topics. */
		{ CW_AUDIO_NULL, CW_AUDIO_NONE /* Guard. */ }, /* Sound systems. */

		{
			LIBCW_TEST_FUNCTION_INSERT(test_cw_rec_get_receive_parameters),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_rec_parameter_getters_setters_1),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_rec_parameter_getters_setters_2),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_rec_identify_mark_internal),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_rec_test_with_constant_speeds),
			LIBCW_TEST_FUNCTION_INSERT(test_cw_rec_test_with_varying_speeds),

			LIBCW_TEST_FUNCTION_INSERT(NULL)
		}
	},


	/* Guard. */
	{
		LIBCW_TEST_SET_INVALID,
		LIBCW_TEST_API_MODERN, /* This field doesn't matter here, test set is invalid. */

		{ LIBCW_TEST_TOPIC_MAX },
		{ CW_AUDIO_NONE /* Guard. */ },
		{
			LIBCW_TEST_FUNCTION_INSERT(NULL)
		}
	}
};
