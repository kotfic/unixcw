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


#ifndef H_CW_COMMON
#define H_CW_COMMON

#if defined(__cplusplus)
extern "C" {
#endif




#include <stdbool.h>
#include <stdio.h>




#include <libcw.h>




#include "cw_config.h"




extern void cw_print_help(cw_config_t *config);

extern int cw_generator_new_from_config(cw_config_t *config);

extern void cw_start_beep(void);
extern void cw_end_beep(void);
extern bool cw_getline(FILE *stream, char *buffer, int buffer_size);




#if defined(__cplusplus)
}
#endif




#endif /* H_CW_COMMON */
