/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef H_LIBCW_CONSOLE
#define H_LIBCW_CONSOLE




#include "libcw2.h"




typedef struct {
	int sound_sink_fd;
	cw_key_value_t cw_value;
} cw_console_data_t;




#include "libcw_gen.h"




cw_ret_t cw_console_fill_gen_internal(cw_gen_t * gen, const char * device_name);
cw_ret_t cw_console_silence(cw_gen_t * gen);




#endif /* #ifndef H_LIBCW_CONSOLE */
