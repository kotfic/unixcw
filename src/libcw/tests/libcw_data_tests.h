/*
  This file is a part of unixcw project.  unixcw project is covered by
  GNU General Public License, version 2 or later.
*/

#ifndef _LIBCW_DATA_TESTS_H_
#define _LIBCW_DATA_TESTS_H_




#include "test_framework.h"




int test_cw_representation_to_hash_internal(cw_test_executor_t * cte);
int test_cw_representation_to_character_internal(cw_test_executor_t * cte);
int test_cw_representation_to_character_internal_speed_gain(cw_test_executor_t * cte);

cwt_retv test_data_main_table_get_count(cw_test_executor_t * cte);
cwt_retv test_data_main_table_get_contents(cw_test_executor_t * cte);
cwt_retv test_data_main_table_get_representation_len_max(cw_test_executor_t * cte);
cwt_retv test_data_main_table_lookups(cw_test_executor_t * cte);

int test_prosign_lookups_internal(cw_test_executor_t * cte);
int test_phonetic_lookups_internal(cw_test_executor_t * cte);
int test_validate_character_internal(cw_test_executor_t * cte);
int test_validate_string_internal(cw_test_executor_t * cte);
int test_validate_representation_internal(cw_test_executor_t * cte);




#endif /* #ifndef _LIBCW_DATA_TESTS_H_ */
