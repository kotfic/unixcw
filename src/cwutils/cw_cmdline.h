/*
 * Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
 * Copyright (C) 2011-2021  Kamil Ignacak (acerion@wp.pl)
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

#ifndef _CW_CMDLINE_H
#define _CW_CMDLINE_H

#include <stdbool.h>
#if defined(HAVE_GETOPT_H)
# include <getopt.h>
#endif

#include "cw_common.h" /* cw_config_t */

#if defined(__cplusplus)
extern "C" {
#endif




extern const char *cw_program_basename(const char *argv0);

/**
   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
extern int combine_arguments(const char *env_variable,
			     int argc, char *const argv[],
			     int *new_argc, char **new_argv[]);




/**
   @brief Parse program's arguments

   Parse arguments of a program. Usually these are combined program
   arguments, i.e. those passed explicitly through command line
   switches, and those that are stored in ENV variable.

   The combination of the two argument lists into one list should be
   done with combine_arguments() before calling this function.

   Results of the parsing are stored in @param config.

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
extern cw_ret_t cw_process_program_arguments(int argc, char *const argv[], cw_config_t * config);




/**
   \brief Check if target system supports long form of command line options

   \return true the system supports long options,
   \return false otherwise
*/
extern bool cw_longopts_available(void);
extern bool has_longopts(void);




extern int get_option(int argc, char *const argv[],
                      const char *descriptor,
                      int *option, char **argument);
extern int get_optind(void);



#if defined(__cplusplus)
}
#endif
#endif  /* _CW_CMDLINE_H */
