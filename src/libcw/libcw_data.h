/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef H_LIBCW_DATA
#define H_LIBCW_DATA




#include <stdbool.h>
#include <stdint.h>




#include "libcw2.h"




#define CW_DATA_MAX_REPRESENTATION_LENGTH 7 /* CHAR_BIT - 1 */
#define CW_DATA_MIN_REPRESENTATION_HASH 2
#define CW_DATA_MAX_REPRESENTATION_HASH 255




typedef struct cw_entry_struct {
	const char character;               /* Character represented. */
	const char *const representation;   /* Dot-dash pattern of the character. */
} cw_entry_t;





/* Functions handling representation of a character.
   Representation looks like this: ".-" for "a", "--.." for "z", etc. */
cw_ret_t cw_representation_lookup_init_internal(const cw_entry_t * lookup[]);
int cw_representation_to_character_internal(const char * representation);
int cw_representation_to_character_direct_internal(const char * representation);
uint8_t cw_representation_to_hash_internal(const char * representation); /* TODO: uint8_t will be enough for everyone? */
const char * cw_character_to_representation_internal(int character);
const char * cw_lookup_procedural_character_internal(int character, bool * is_usually_expanded);




#endif /* #ifndef H_LIBCW_DATA */
