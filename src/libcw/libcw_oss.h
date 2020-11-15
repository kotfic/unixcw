/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef H_LIBCW_OSS
#define H_LIBCW_OSS




typedef struct cw_oss_version {
	unsigned int x;
	unsigned int y;
	unsigned int z;
} cw_oss_version_t;




typedef struct cw_oss_data_struct {
	cw_oss_version_t version;
	int sound_sink_fd;
} cw_oss_data_t;




#include "libcw_gen.h"




cw_ret_t cw_oss_init_gen_internal(cw_gen_t * gen);




#endif /* #ifndef H_LIBCW_OSS */
