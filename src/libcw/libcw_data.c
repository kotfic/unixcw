/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2020  Kamil Ignacak (acerion@wp.pl)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


/**
   @file libcw_data.c

   @brief Characters, representations, lookup and validation functions.

   The only hard data stored by libcw library seems to be:
   @li characters and their representations
   @li procedural signals
   @li phonetics

   These three groups of data, collected in three separate tables, are
   defined in this file, together with lookup functions and other related
   utility functions.

   Unit test functions for this code are at the end of the file.
*/




#include <ctype.h>
#include <errno.h>
#include <limits.h> /* UCHAR_MAX */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>




#include "libcw.h"
#include "libcw_data.h"
#include "libcw_debug.h"




#define MSG_PREFIX "libcw/data: "




extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;




static __attribute__((constructor)) void cw_data_constructor_internal(void);




/*
  Morse code characters table.  This table allows lookup of the Morse
  representation of a given alphanumeric character.  Representations
  are held as a string, with "-" representing Dash, and "."
  representing Dot.  The table ends with a NULL entry.

  Notice that ASCII characters are stored as uppercase characters.
*/
static size_t g_main_table_maximum_representation_length;
static int g_main_table_characters_count;
static const cw_entry_t * g_main_table_fast_lookup[UCHAR_MAX];  /* Fast lookup table */
const cw_entry_t CW_TABLE[] = { /* TODO: make it accessible through function only, and add static keyword. */
	/* ASCII 7bit letters */
	{'A', ".-"  },  {'B', "-..."},  {'C', "-.-."},
	{'D', "-.." },  {'E', "."   },  {'F', "..-."},
	{'G', "--." },  {'H', "...."},  {'I', ".."  },
	{'J', ".---"},  {'K', "-.-" },  {'L', ".-.."},
	{'M', "--"  },  {'N', "-."  },  {'O', "---" },
	{'P', ".--."},  {'Q', "--.-"},  {'R', ".-." },
	{'S', "..." },  {'T', "-"   },  {'U', "..-" },
	{'V', "...-"},  {'W', ".--" },  {'X', "-..-"},
	{'Y', "-.--"},  {'Z', "--.."},

	/* Numerals */
	{'0', "-----"},  {'1', ".----"},  {'2', "..---"},
	{'3', "...--"},  {'4', "....-"},  {'5', "....."},
	{'6', "-...."},  {'7', "--..."},  {'8', "---.."},
	{'9', "----."},

	/* Punctuation */
	{'"', ".-..-."},  {'\'', ".----."},  {'$', "...-..-"},
	{'(', "-.--." },  {')',  "-.--.-"},  {'+', ".-.-."  },
	{',', "--..--"},  {'-',  "-....-"},  {'.', ".-.-.-" },
	{'/', "-..-." },  {':',  "---..."},  {';', "-.-.-." },
	{'=', "-...-" },  {'?',  "..--.."},  {'_', "..--.-" },
	{'@', ".--.-."},

	/* ISO 8859-1 accented characters */
	{'\334', "..--" },   /* U with diaeresis */
	{'\304', ".-.-" },   /* A with diaeresis */
	{'\307', "-.-.."},   /* C with cedilla */
	{'\326', "---." },   /* O with diaeresis */
	{'\311', "..-.."},   /* E with acute */
	{'\310', ".-..-"},   /* E with grave */
	{'\300', ".--.-"},   /* A with grave */
	{'\321', "--.--"},   /* N with tilde */

	/* ISO 8859-2 accented characters */
	{'\252', "----" },   /* S with cedilla */
	{'\256', "--..-"},   /* Z with dot above */

	/* Non-standard procedural signal extensions to standard CW characters. */
	{'<', "...-.-" },    /* VA/SK, end of work */
	{'>', "-...-.-"},    /* BK, break */
	{'!', "...-."  },    /* SN, understood */
	{'&', ".-..."  },    /* AS, wait */
	{'^', "-.-.-"  },    /* KA, starting signal */
	{'~', ".-.-.." },    /* AL, paragraph */

	{0, NULL} /* Guard. */
};




/**
   @brief Return the number of characters present in main character lookup table

   Return the number of characters that are known to libcw.
   The number includes:
   @li ASCII 7bit letters,
   @li numerals,
   @li punctuation,
   @li ISO 8859-1 accented characters,
   @li ISO 8859-2 accented characters,
   @li non-standard procedural signal extensions to standard CW characters.

   @internal
   @reviewed 2020-07-25
   @endinternal

   @return number of characters known to libcw
*/
int cw_get_character_count(void)
{
#if 0   /* Disabled on 2020-07-25. Moved to library constructor. */
	static int g_main_table_characters_count = 0;
	if (0 == g_main_table_characters_count) {
		for (const cw_entry_t * cw_entry = CW_TABLE; cw_entry->character; cw_entry++) {
			g_main_table_characters_count++;
		}
	}
#endif
	return g_main_table_characters_count;
}




/**
   @brief Get list of characters present in character lookup table

   Function provides a string containing all of the characters represented
   in library's lookup table.

   The list includes:
   @li ASCII 7bit letters,
   @li numerals,
   @li punctuation,
   @li ISO 8859-1 accented characters,
   @li ISO 8859-2 accented characters,
   @li non-standard procedural signal extensions to standard CW characters.

   @p list should be allocated and managed by caller.  The length of @p list
   must be at least one greater than the number of characters represented in
   the character lookup table, returned by cw_get_character_count(). The
   string placed in @p list will be NUL-terminated.

   @internal
   @reviewed 2020-07-25
   @endinternal

   @param[out] list buffer for string with all characters
*/
void cw_list_characters(char * list)
{
	cw_assert (list, MSG_PREFIX "output pointer is NULL");

	/* Append each table character to the output string. */
	int i = 0;
	for (const cw_entry_t * cw_entry = CW_TABLE; cw_entry->character; cw_entry++) {
		list[i++] = cw_entry->character;
	}

	list[i] = '\0';

	return;
}




/**
   @brief Get length of the longest representation of characters

   Function returns the string length of the longest representation in the
   main character lookup table. It is the count of dots and dashes in the
   longest representation of characters known to libcw (not including
   terminating NUL).

   @internal
   @reviewed 2020-07-25
   @endinternal

   @return length of the longest representation
*/
int cw_get_maximum_representation_length(void)
{
#if 0   /* Disabled on 2020-07-25. Code moved to library constructor. */
	static size_t g_main_table_maximum_representation_length = 0;
	if (0 == g_main_table_maximum_representation_length) {
		/* Traverse the main lookup table, finding the longest representation. */
		for (const cw_entry_t * cw_entry = CW_TABLE; cw_entry->character; cw_entry++) {
			size_t length = strlen(cw_entry->representation);
			if (length > g_main_table_maximum_representation_length) {
				g_main_table_maximum_representation_length = length;
			}
		}
	}
#endif
	return (int) g_main_table_maximum_representation_length;
}




/**
   @brief Return representation of given character

   Look up the given character @p character, and return the representation of
   that character.  Return NULL if there is no representation for the given
   character. Otherwise return pointer to static string with representation
   of character.

   The returned pointer is owned and managed by library.

   @internal
   @reviewed 2020-07-25
   @endinternal

   @param[in] character character to look up

   @return pointer to string with representation of character on success
   @return NULL on failure (when @p character has no representation)
*/
const char * cw_character_to_representation_internal(int character)
{
#if 0   /* Disabled on 2020-07-25. Moved to library constructor. */
	static const cw_entry_t * g_main_table_fast_lookup[UCHAR_MAX];  /* Fast lookup table */
	static bool is_initialized = false;
	/* If this is the first call, set up the fast lookup table to give
	   direct access to the CW table for a given character. */
	if (!is_initialized) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_INFO,
			      MSG_PREFIX "initializing fast lookup table");

		for (const cw_entry_t * cw_entry = CW_TABLE; cw_entry->character; cw_entry++) {
			g_main_table_fast_lookup[(unsigned char) cw_entry->character] = cw_entry;
		}

		is_initialized = true;
	}
#endif

	/* There is no differentiation in the lookup and
	   representation table between upper and lower case
	   characters; everything is held as uppercase.  So before we
	   do the lookup, we convert to ensure that both cases
	   work. */
	character = toupper(character);

	/* Now use the table to lookup the table entry.  Unknown characters
	   return NULL, courtesy of the fact that explicitly uninitialized
	   static variables are initialized to zero, so lookup[x] is NULL
	   if it's not assigned to in the above loop. */
	const cw_entry_t * cw_entry = g_main_table_fast_lookup[(unsigned char) character];

	/* Lookups code may be called frequently, so first do a rough
	   test with cw_debug_has_flag(). */
	if (cw_debug_has_flag(&cw_debug_object, CW_DEBUG_LOOKUPS)) {
		if (cw_entry) {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_DEBUG,
				      MSG_PREFIX "char to representation: '%c' -> '%c'/'%s'\n", character, cw_entry->character, cw_entry->representation);
		} else if (isprint(character)) {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_DEBUG,
				      MSG_PREFIX "char to representation: '%c' -> NOTHING\n", character);
		} else {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_DEBUG,
				      MSG_PREFIX "char to representation: '0x%02x' -> NOTHING\n", (unsigned int) character);
		}
	}

	return cw_entry ? cw_entry->representation : NULL;
}




/**
   @brief Get representation of a given character

   The function is depreciated, use cw_character_to_representation() instead.

   Return the string representation of a given character @p character.

   The routine returns CW_SUCCESS on success, and fills in the string pointer
   (@p representation) passed in.  On failure, it returns CW_FAILURE and sets
   errno to ENOENT, indicating that the character @p character could not be
   found.

   The length of @p representation buffer must be at least one greater
   than the length of longest representation held in the character
   lookup table. The largest value of length is returned by
   cw_get_maximum_representation_length().

   @internal
   @reviewed 2020-07-25
   @endinternal

   @param[in] character character to look up
   @param[out] representation pointer to space for representation of character

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
int cw_lookup_character(char character, char * representation)
{
	/* Lookup the character, and if found, return the string. */
	const char * retval = cw_character_to_representation_internal(character);
	if (retval) {
		if (representation) {
			strncpy(representation, retval, g_main_table_maximum_representation_length);
		}
		return CW_SUCCESS;
	}

	/* Failed to find the requested character. */
	errno = ENOENT;
	return CW_FAILURE;
}




/**
   @brief Get representation of a given character

   On success return representation of a given character.
   Returned pointer is owned by caller of the function.

   On failure function returns NULL and sets errno.

   @exception ENOENT the character could not be found.
   @exception ENOMEM character has been found, but function failed to strdup() representation.

   @internal
   @reviewed 2020-07-25
   @endinternal

   @param[in] character character to look up

   @return pointer to freshly allocated representation on success
   @return NULL on failure
*/
char * cw_character_to_representation(int character)
{
	/* Lookup representation of the character, and if found, return copy of the representation. */

	const char * representation = cw_character_to_representation_internal(character);
	if (NULL == representation) {
		errno = ENOENT;
		return NULL;
	}

	char * r = strdup(representation);
	if (NULL == r) {
		errno = ENOMEM;
		return NULL;
	}

	return r;
}




/**
   @brief Return a hash value of a character representation

   Return a hash value, in the range
   CW_DATA_MIN_REPRESENTATION_HASH-CW_DATA_MAX_REPRESENTATION_HASH,
   for a character's @p representation.  The routine returns 0 if no
   valid hash could be made from the @p representation string.

   This hash algorithm is designed ONLY for valid CW representations;
   that is, strings composed of only "." and "-". The CW
   representations can be no longer than seven characters.

   TODO: create unit test that verifies that the longest
   representation recognized by libcw is in fact no longer than 7.

   TODO: consider creating an implementation that has the limit larger
   than 7. Then perhaps make the type of returned value to be uint16_t.

   The algorithm simply turns the representation string into a number,
   a "bitmask", based on pattern of "." and "-" in @p representation.
   The first bit set in the mask indicates the start of data (hence
   the 7-character limit) - it is not the data itself.  This mask is
   viewable as an integer in the range CW_DATA_MIN_REPRESENTATION_HASH
   (".") to CW_DATA_MAX_REPRESENTATION_HASH ("-------"), and can be
   used as an index into a fast lookup array.

   @internal
   @reviewed 2020-07-26
   @endinternal

   @param[in] representation string representing a character

   @return non-zero value of hash of valid representation (in range CW_DATA_MIN_REPRESENTATION_HASH-CW_DATA_MAX_REPRESENTATION_HASH)
   @return zero for invalid representation
*/
uint8_t cw_representation_to_hash_internal(const char * representation)
{
	/* Our algorithm can handle only 7 characters of representation.
	   And we insist on there being at least one character, too.  */
	const size_t length = strlen(representation);
	if (length > CW_DATA_MAX_REPRESENTATION_LENGTH || length < 1) {
		return 0;
	}

	/*
	  Build up the hash based on the dots and dashes; start at 1, the
	  sentinel * (start) bit.

	  TODO: why do we need the sentinel? To distinguish between hashes
	  of similar representations that have dots at the beginning?

	  representation    hash, no sentinel    hash, with sentinel
	  .._               0000 0001            0000 1001
	  ..._              0000 0001            0001 0001
	*/


	unsigned int hash = 1; /* TODO: shouldn't this be uint8_t? */
	for (size_t i = 0; i < length; i++) {
		/* Left-shift everything so far. */
		hash <<= 1;

		if (representation[i] == CW_DASH_REPRESENTATION) {
			/* Dash is represented by '1' in hash. */
			hash |= 1;
		} else if (representation[i] == CW_DOT_REPRESENTATION) {
			/* Dot is represented by '0' in hash. We don't have
			   to do anything at this point, the zero bit is
			   already at the rightmost position after
			   left-shifting hash. */
			;
		} else {
			/* Invalid element in representation string. */
			return 0;
		}
	}

	return hash;
}




/**
   @brief Return character corresponding to given representation

   Look up the given @p representation, and return the character that it
   represents.

   @internal
   @reviewed 2020-07-26
   @endinternal

   @param[in] representation representation of a character to look up

   FIXME: function should be able to return zero as non-error value (?).

   @return zero if there is no character for given representation
   @return non-zero character corresponding to given representation otherwise
*/
int cw_representation_to_character_internal(const char * representation)
{
	static const cw_entry_t * lookup[UCHAR_MAX];  /* Fast lookup table */
	static bool is_complete = true;               /* Set to false if there are any
							 lookup table entries not in
							 the fast lookup table */
	static bool is_initialized = false;

	/* If this is the first call, set up the fast lookup table to give direct
	   access to the CW table for a hashed representation. */
	if (!is_initialized) {
		/* TODO: move the initialization to cw_data_constructor_internal(). */
		cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_INFO,
			      MSG_PREFIX "initialize hash lookup table");
		is_complete = CW_SUCCESS == cw_data_init_r2c_hash_table_internal(lookup);
		is_initialized = true;
	}

	/* Hash the representation to get an index for the fast lookup. */
	const uint8_t hash = cw_representation_to_hash_internal(representation);

	const cw_entry_t * cw_entry = NULL;
	/* If the hashed lookup table is complete, we can simply believe any
	   hash value that came back.  That is, we just use what is at the index
	   "hash", since this is either the entry we want, or NULL. */
	if (is_complete) {
		cw_entry = lookup[hash];
	} else {
		/* Impossible, since test_cw_representation_to_hash_internal()
		   passes without problems for all valid representations.

		   Debug message is already displayed in
		   cw_data_init_r2c_hash_table_internal(). */

		/* The lookup table is incomplete, but it doesn't have
		   to be that we are missing entry for this particular
		   hash.

		   Try to find the entry in lookup table anyway, maybe
		   it exists.

		   TODO: create tests to find situation where lookup
		   table is incomplete. */
		if (hash && lookup[hash] && lookup[hash]->representation
		    && strcmp(lookup[hash]->representation, representation) == 0) {
			/* Found it in an incomplete table. */
			cw_entry = lookup[hash];
		} else {
			/* We have no choice but to search the table entry
			   by entry, sequentially, from top to bottom. */
			for (cw_entry = CW_TABLE; cw_entry->character; cw_entry++) {
				if (0 == strcmp(cw_entry->representation, representation)) {
					break;
				}
			}

			/* If we got to the end of the table, prepare to return zero. */
			cw_entry = cw_entry->character ? cw_entry : NULL;
		}
	}


	/* Lookups code may be called frequently, so first do a rough
	   test with cw_debug_has_flag(). */
	if (cw_debug_has_flag(&cw_debug_object, CW_DEBUG_LOOKUPS)) {
		if (cw_entry) {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_DEBUG,
				      MSG_PREFIX "lookup [0x%02x]'%s' returned <'%c':\"%s\">\n",
				      hash, representation,
				      cw_entry->character, cw_entry->representation);
		} else {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_DEBUG,
				      MSG_PREFIX "lookup [0x%02x]'%s' found nothing\n",
				      hash, representation);
		}
	}

	return cw_entry ? cw_entry->character : 0;
}




/**
   @brief Return character corresponding to given representation

   Look up the given @p representation, and return the character that it
   represents.

   In contrast to cw_representation_to_character_internal(), this
   function doesn't use fast lookup table. It directly traverses the
   main character/representation table and searches for a character.

   The function shouldn't be used in production code. It has been created for
   libcw tests, for verification purposes.

   Its first purpose is to verify correctness of
   cw_representation_to_character_internal() (since this direct method
   is simpler and, well, direct) in a unit test function.

   The second purpose is to compare time of execution of the two
   functions: direct and with lookup table, and see what are the speed
   advantages of using function with lookup table.

   @internal
   @reviewed 2020-07-26
   @endinternal

   @param[in] representation representation of a character to look up

   FIXME: function should be able to return zero as non-error value (?).

   @return zero if there is no character for given representation
   @return non-zero character corresponding to given representation otherwise
*/
int cw_representation_to_character_direct_internal(const char * representation)
{
	/* Search the table entry by entry, sequentially, from top to
	   bottom. */
	for (const cw_entry_t * cw_entry = CW_TABLE; cw_entry->character; cw_entry++) {
		if (0 == strcmp(cw_entry->representation, representation)) {
			return cw_entry->character;
		}
	}
	return 0;
}




/**
   @brief Initialize representation-to-character lookup table

   Initialize @p table with values from CW_TABLE (of type cw_entry_t).
   The table is indexed with hashed representations of cw_entry_t->representation
   strings.

   @p table must be large enough to store all entries, caller must
   make sure that the condition is met.

   On failure function returns CW_FAILURE.
   On success the function returns CW_SUCCESS. Successful execution of
   the function is when all representations from CW_TABLE have valid
   hashes, and all entries from CW_TABLE have been put into @p table.

   First condition of function's success, mentioned above, should be
   always true because the CW_TABLE has been created once and it
   doesn't change, and all representations in the table are valid.
   Second condition of function's success, mentioned above, should be
   also always true because first condition is always true.

   The initialization may fail under one condition: if the lookup
   functions should operate on non-standard character table, other
   than CW_TABLE. For now it's impossible, because there is no way for
   client code to provide its own table of CW characters.

   @internal
   @reviewed 2020-07-26
   @endinternal

   @param[in] table lookup table to be initialized

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_data_init_r2c_hash_table_internal(const cw_entry_t * table[])
{
	/* For each main table entry, create a hash entry.  If the
	   hashing of any entry fails, note that the table is not
	   complete and ignore that entry for now (for the current
	   main table (CW_TABLE) this should not happen).  The hashed
	   table speeds up representation-to-character lookups by a factor of
	   5-10.

	   NOTICE: Notice that the lookup table will be marked as
	   incomplete only if one or more representations in CW_TABLE
	   aren't valid (i.e. they are made of anything more than '.'
	   or '-'). This wouldn't be a logic error, this would be an
	   error with invalid input. Such invalid input shouldn't
	   happen in properly built characters table.

	   So perhaps returning "false" tells us more about input
	   CW_TABLE than about lookup table.

	   Other possibility to consider is that "is_complete = false"
	   when length of representation is longer than
	   CW_DATA_MAX_REPRESENTATION_LENGTH Dots/Dashes. There is an
	   assumption that no representation in input CW_TABLE is
	   longer than CW_DATA_MAX_REPRESENTATION_LENGTH
	   dots/dashes. */

	bool is_complete = true;
	for (const cw_entry_t * cw_entry = CW_TABLE; cw_entry->character; cw_entry++) {
		const uint8_t hash = cw_representation_to_hash_internal(cw_entry->representation);
		if (hash) {
			table[hash] = cw_entry;
		} else {
			is_complete = false;
		}
	}

	if (!is_complete) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_WARNING,
			      MSG_PREFIX "hash lookup table incomplete");
	}

	return is_complete ? CW_SUCCESS : CW_FAILURE;
}




/**
   @brief Check if representation of a character is valid

   This function is depreciated, use cw_representation_is_valid() instead.

   Check that the given string is a valid Morse representation.  A valid
   string is one composed of only "." and "-" characters.  This means that
   the function checks only if representation is error-free, and not whether
   the representation represents existing/defined character.

   If representation is invalid, function returns CW_FAILURE and sets
   errno to EINVAL.

   @internal
   @reviewed 2020-07-26
   @endinternal

   @param[in] representation representation of a character to check

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
int cw_check_representation(const char * representation)
{
	const bool v = cw_representation_is_valid(representation);
	return v ? CW_SUCCESS : CW_FAILURE;
}





/**
   @brief Check if representation of a character is valid

   Check that the given string is a valid Morse representation.  A valid
   string is one composed of only "." and "-" characters.  This means that
   the function checks only if representation is error-free, and not whether
   the representation represents existing/defined character.

   @internal
   @reviewed 2020-07-26
   @endinternal

   @exception EINVAL representation is invalid

   @param[in] representation representation of a character to check

   @return true if representation is valid
   @return false if representation is invalid
*/
bool cw_representation_is_valid(const char * representation)
{
	/* Check the characters in representation. */
	for (int i = 0; representation[i]; i++) {

		if (representation[i] != CW_DOT_REPRESENTATION
		    && representation[i] != CW_DASH_REPRESENTATION) {

			errno = EINVAL;
			return false;
		}
	}

	return true;
}




/**
   @brief Get the character represented by a given Morse representation

   This function is depreciated, use cw_representation_to_character() instead.

   Function checks @p representation, and if it is valid and represents a
   known character, function returns CW_SUCCESS. Additionally, if @p character is
   non-NULL, function puts the looked up character in @p character.

   @p character should be allocated by caller. Function assumes that @p
   character being NULL pointer is a valid situation, and can return
   CW_SUCCESS in such situation.

   On error, function returns CW_FAILURE. errno is set to EINVAL if any
   character of the representation is invalid, or ENOENT to indicate that
   the character represented by @p representation could not be found.

   @internal
   @reviewed 2020-07-26
   @endinternal

   @param[in] representation representation of a character to look up
   @param[out] character location where to put looked up character

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
int cw_lookup_representation(const char * representation, char * character)
{
	/* Check the characters in representation. */
	if (!cw_representation_is_valid(representation)) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	/* Lookup the representation, and if found, return the character. */
	const int looked_up = cw_representation_to_character_internal(representation);
	if (0 != looked_up) {
		if (character) {
			*character = looked_up;
		}
		return CW_SUCCESS;
	}

	/* Failed to find the requested representation. */
	errno = ENOENT;
	return CW_FAILURE;
}




/**
   @brief Return the character represented by a given Morse representation

   Function checks @p representation, and if it is valid and represents
   a known character, function returns the character (a non-zero value).

   On error, function returns zero (character represented by @p
   representation was not found).

   @internal
   @reviewed 2020-07-26
   @endinternal

   @exception EINVAL @p representation contains invalid symbol (other than Dots and Dashes)
   @exception ENOENT a character represented by @p representation could not be found

   @param[in] representation representation of a character to look up

   @return non-zero character on success
   @return zero on failure
*/
int cw_representation_to_character(const char * representation)
{
	/* Check the characters in representation. */
	if (!cw_representation_is_valid(representation)) {
		errno = EINVAL;
		return 0;
	}

	/* Lookup the representation, and if found, return the character. */
	int character = cw_representation_to_character_internal(representation);
	if (0 != character) {
		return character;
	} else {
		/* Failed to find the requested representation. */
		errno = ENOENT;
		return 0;
	}
}




/* ******************************************************************** */
/*   Section:Extended Morse code data and lookup (procedural signals)   */
/* ******************************************************************** */




/* Ancillary procedural signals table.  This table maps procedural signal
   characters in the main table to their expansions, along with a flag noting
   if the character is usually expanded for display. */
typedef struct {
	const char character;            /* Character represented */
	const bool is_usually_expanded;  /* If expanded display is usual */
	const char * const expansion;    /* Procedural expansion of the character */
} cw_prosign_entry_t;



static int g_prosign_table_characters_count;
static size_t g_prosign_table_maximum_expansion_length;
static const cw_prosign_entry_t g_prosign_table[] = {
	/* Standard procedural signals */
	{ '"', false, "AF"  },   { '\'', false,  "WG"  },  { '$', false, "SX"  },
	{ '(', false, "KN"  },   { ')',  false,  "KK"  },  { '+', false, "AR"  },
	{ ',', false, "MIM" },   { '-',  false,  "DU"  },  { '.', false, "AAA" },
	{ '/', false, "DN"  },   { ':',  false,  "OS"  },  { ';', false, "KR"  },
	{ '=', false, "BT"  },   { '?',  false,  "IMI" },  { '_', false, "IQ"  },
	{ '@', false, "AC"  },

	/* Non-standard procedural signal extensions to standard CW characters. */
	{ '<', true,  "VA" },  /* VA/SK, end of work */
	{ '>', true,  "BK" },  /* BK, break */
	{ '!', true,  "SN" },  /* SN, understood */
	{ '&', true,  "AS" },  /* AS, wait */
	{ '^', true,  "KA" },  /* KA, starting signal */
	{ '~', true,  "AL" },  /* AL, paragraph */

	{  0,  false,  NULL } /* Guard. */
};

static const cw_prosign_entry_t * g_prosign_table_fast_lookup[UCHAR_MAX];  /* Fast lookup table. */




/**
   @brief Get number of procedural signals

   @internal
   @reviewed 2020-07-26
   @endinternal

   @return the number of characters stored in the procedural signal expansion lookup table
*/
int cw_get_procedural_character_count(void)
{
#if 0   /* Disabled on 2020-07-25. Moved to library constructor. */
	static int g_prosign_table_characters_count = 0;
	if (0 == g_prosign_table_characters_count) {
		for (const cw_prosign_entry_t *e = g_prosign_table; e->character; e++) {
			g_prosign_table_characters_count++;
		}
	}
#endif

	return g_prosign_table_characters_count;
}




/**
   @brief Get list of characters for which procedural expansion is available

   Function copies into preallocated buffer @p list a string
   containing all of the Morse characters for which procedural
   expansion is available.  The length of @p list must be at least by
   one greater than the number of characters represented in the
   procedural signal expansion lookup table, returned by
   cw_get_procedural_character_count().

   @p list buffer is allocated and managed by caller.

   @internal
   @reviewed 2020-07-26
   @endinternal

   @param[out] list buffer for returned string
*/
void cw_list_procedural_characters(char * list)
{
	/* Append each table character to the output string. */
	int i = 0;
	for (const cw_prosign_entry_t *e = g_prosign_table; e->character; e++) {
		list[i++] = e->character;
	}

	list[i] = '\0';

	return;
}




/**
   @brief Get length of the longest procedural expansion

   Function returns the string length of the longest expansion
   in the procedural signal expansion table.

   @internal
   @reviewed 2020-07-26
   @endinternal

   @return the string length of the longest expansion of procedural signals
*/
int cw_get_maximum_procedural_expansion_length(void)
{
#if 0   /* Disabled on 2020-07-25. Moved to library constructor. */
	static size_t g_prosign_table_maximum_expansion_length = 0;
	if (0 == g_prosign_table_maximum_expansion_length) {
		/* Traverse the prosign table, finding the longest expansion. */
		for (const cw_prosign_entry_t *e = g_prosign_table; e->character; e++) {
			size_t length = strlen(e->expansion);
			if (length > g_prosign_table_maximum_expansion_length) {
				g_prosign_table_maximum_expansion_length = length;
			}
		}
	}
#endif
	return (int) g_prosign_table_maximum_expansion_length;
}




/**
   @brief Get the string expansion of a given Morse code procedural signal character

   Function looks up the given procedural character @p character, and returns
   the expansion of that procedural character, with a display hint in @p
   is_usually_expanded.

   Pointer returned by the function is owned and managed by library.
   @p is_usually_expanded pointer is owned by client code.

   @internal
   @reviewed 2020-07-26
   @endinternal

   @param[in] character character to look up
   @param[out] is_usually_expanded display hint

   @return expansion of input character on success
   @return NULL if there is no table entry for the given character
*/
const char * cw_lookup_procedural_character_internal(int character, bool * is_usually_expanded)
{
#if 0   /* Disabled on 2020-07-25. Code moved to library constructor. */
	/* If this is the first call, set up the fast lookup table to
	   give direct access to the procedural expansions table for
	   a given character. */
	static bool is_initialized = false;
	if (!is_initialized) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_INFO,
			      MSG_PREFIX "initialize prosign fast lookup table");

		for (const cw_prosign_entry_t *e = g_prosign_table; e->character; e++) {
			g_prosign_table_fast_lookup[(unsigned char) e->character] = e;
		}
		is_initialized = true;
	}
#endif

	/* Lookup the procedural signal table entry.  Unknown characters
	   return NULL.  All procedural signals are non-alphabetical, so no
	   need to use any uppercase coercion here. */
	const cw_prosign_entry_t * cw_prosign = g_prosign_table_fast_lookup[(unsigned char) character];

	/* Lookups code may be called frequently, so first do a rough
	   test with cw_debug_has_flag(). */
	if (cw_debug_has_flag((&cw_debug_object), CW_DEBUG_LOOKUPS)) {
		if (cw_prosign) {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_DEBUG,
				      MSG_PREFIX "prosign lookup '%c' -> '%c'/'%s'/%d\n",
				      character, cw_prosign->character,
				      cw_prosign->expansion, cw_prosign->is_usually_expanded);
		} else if (isprint(character)) {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_DEBUG,
				      MSG_PREFIX "prosign lookup '%c' -> NOTHING\n", character);
		} else {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_DEBUG,
				      MSG_PREFIX "prosign lookup '0x%02x' -> NOTHING\n", (unsigned int) character);
		}
	}

	/* If found, return any display hint and the expansion; otherwise, NULL. */
	if (cw_prosign) {
		*is_usually_expanded = cw_prosign->is_usually_expanded;
		return cw_prosign->expansion;
	} else {
		return NULL;
	}
}




/**
   @brief Get the string expansion of a given Morse code procedural signal character

   On success the function
   @li fills @p expansion with the string expansion of a given Morse code
   procedural signal character @p character;
   @li sets @p is_usually_expanded to appropriate value as a display hint for the caller;
   @li returns CW_SUCCESS.

   Both @p expansion and @p is_usually_expanded must be allocated and
   managed by caller. They can be NULL, then the function won't
   attempt to use them.

   The length of @p expansion must be at least by one greater than the
   longest expansion held in the procedural signal character lookup
   table, as returned by cw_get_maximum_procedural_expansion_length().

   @internal
   @reviewed 2020-07-26
   @endinternal

   @exception ENOENT procedural signal character @p character cannot be found

   @param[in] character character to look up
   @param[out] expansion buffer to fill with expansion of the character
   @param[out] is_usually_expanded visual hint

   @return CW_FAILURE on failure (character cannot be found)
   @return CW_SUCCESS on success
*/
int cw_lookup_procedural_character(char character, char *expansion, int * is_usually_expanded)
{
	bool is_expanded;

	/* Lookup, and if found, return the string and display hint. */
	const char * retval = cw_lookup_procedural_character_internal(character, &is_expanded);
	if (retval) {
		if (expansion) {
			strncpy(expansion, retval, g_prosign_table_maximum_expansion_length + 1);
		}
		if (is_usually_expanded) {
			*is_usually_expanded = is_expanded;
		}
		return CW_SUCCESS;
	}

	/* Failed to find the requested procedural signal character. */
	errno = ENOENT;
	return CW_FAILURE;
}




/* ******************************************************************** */
/*                     Section:Phonetic alphabet                        */
/* ******************************************************************** */




/* Phonetics table.  Not really CW, but it might be handy to have.
   The table contains ITU/NATO phonetics. */
static size_t g_phonetics_table_maximum_phonetic_length;
static const char * const g_phonetics_table[] = {
	"Alfa",
	"Bravo",
	"Charlie",
	"Delta",
	"Echo",
	"Foxtrot",
	"Golf",
	"Hotel",
	"India",
	"Juliett",
	"Kilo",
	"Lima",
	"Mike",
	"November",
	"Oscar",
	"Papa",
	"Quebec",
	"Romeo",
	"Sierra",
	"Tango",
	"Uniform",
	"Victor",
	"Whiskey",
	"X-ray",
	"Yankee",
	"Zulu",
	NULL /* guard */
};




/**
   @brief Get length of a longest phonetic

   @internal
   @reviewed 2020-07-26
   @endinternal

   @return the string length of the longest phonetic in the phonetics lookup table
*/
int cw_get_maximum_phonetic_length(void)
{
#if 0   /* Disabled on 2020-07-25. Moved to library constructor. */
	static size_t g_phonetics_table_maximum_phonetic_length = 0;
	if (0 == g_phonetics_table_maximum_phonetic_length) {
		/* Traverse the phonetics table, finding the longest phonetic string. */
		for (int phonetic = 0; g_phonetics_table[phonetic]; phonetic++) {
			size_t length = strlen(g_phonetics_table[phonetic]);
			if (length > g_phonetics_table_maximum_phonetic_length) {
				g_phonetics_table_maximum_phonetic_length = length;
			}
		}
	}
#endif

	return (int) g_phonetics_table_maximum_phonetic_length;
}




/**
   @brief Look up the phonetic of a given character

   On success the routine copies a phonetic corresponding to @p character
   into @p buffer. @p buffer is managed and owned by caller.

   It is NOT considered an error if @p buffer is NULL. In such case the
   function will just verify if @p character can be represented by a
   phonetic, i.e. if @p character is a letter.

   If non-NULL, the size of @p buffer must be greater by at least 1 than the
   longest phonetic held in the phonetic lookup table, as returned by
   cw_get_maximum_phonetic_length().

   @internal
   @reviewed 2020-07-25
   @endinternal

   @exception ENOENT phonetic for given character cannot be found

   @param[in] character character to look up
   @param[out] buffer buffer for phonetic of a character (may be NULL)

   @return CW_SUCCESS on success (phonetic has been found and - if @p buffer is non-NULL) has been copied to the buffer
   @return CW_FAILURE on failure (phonetic for given character cannot be found)
*/
int cw_lookup_phonetic(char character, char * buffer)
{
	/* Coerce to uppercase, and verify the input argument. */
	character = toupper(character);
	if (character >= 'A' && character <= 'Z') {
		if (NULL != buffer) {
			strncpy(buffer, g_phonetics_table[character - 'A'], g_phonetics_table_maximum_phonetic_length + 1);
		} else {
			/* Simply ignore the fact that caller didn't specify output buffer. */
		}
		return CW_SUCCESS;
	}

	/* No such phonetic. */
	errno = ENOENT;
	return CW_FAILURE;
}




/**
   @brief Check if given character is valid

   Check that a given character is valid, i.e. it is present in library's
   character-to-representation lookup table and can be represented with Dots
   and Dashes.

   Space character (' ') is also considered to be a valid character.

   @exception ENOENT on failure (character is invalid).

   Notice the difference in errno set by cw_character_is_valid() (ENOENT) and
   cw_string_is_valid() (EINVAL). This different behaviour goes way back to
   unixcw-2.3 and is preserved in new versions for backwards compatibility
   reasons.

   @internal
   @reviewed 2020-08-24
   @endinternal

   @param[in] character character to check

   @return true if character is valid
   @return false otherwise
*/
bool cw_character_is_valid(char character)
{
	/* If the character is the Space special-case, or it is in the
	   lookup table, return success. */
	if (character == ' ' || NULL != cw_character_to_representation_internal(character)) {
		return true;
	} else {
		errno = ENOENT;
		return false;
	}
}





/**
   @internal
   @reviewed 2020-07-25
   @endinternal
*/
int cw_check_character(char character)
{
	return (int) cw_character_is_valid(character);
}




/**
   @brief Check if all characters in given string are valid

   Check that each character in the given string is valid and can be
   converted by libcw to Morse code symbols sent as a Morse character.

   Space character (' ') is also considered to be a valid character.

   @exception EINVAL on failure (when an invalid character has been found in @p
   string).

   Notice the difference in errno set by cw_character_is_valid() (ENOENT) and
   cw_string_is_valid() (EINVAL). This different behaviour goes way back to
   unixcw-2.3 and is preserved in new versions for backwards compatibility
   reasons.

   @internal
   @reviewed 2020-07-25
   @endinternal

   @param[in] string string to check

   @return true if all characters in string are valid
   @return false otherwise
*/
bool cw_string_is_valid(const char * string)
{
	/* Check that each character satisfies validity as specified by
	   cw_character_is_valid(). */
	for (int i = 0; string[i] != '\0'; i++) {
		if (!cw_character_is_valid(string[i])) {
			errno = EINVAL;
			return CW_FAILURE;
		}
	}

	return CW_SUCCESS;
}




/**
   @internal
   @reviewed 2020-07-25
   @endinternal
*/
int cw_check_string(const char * string)
{
	return cw_string_is_valid(string);
}




// @cond LIBCW_INTERNAL

/**
   @brief Constructor for 'data' module of libcw

   Since we may have multiple gen/rec/key objects per application, it's
   better to initialize library's data structures before the multiple objects
   start calling library functions that have internal static 'is_initialized'
   flags.

   @internal
   @reviewed 2020-07-25
   @endinternal
*/
void cw_data_constructor_internal(void)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_INTERNAL, CW_DEBUG_INFO,
		      MSG_PREFIX "constructor for 'data' module started");



	cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_INFO,
		      MSG_PREFIX "initializing main fast lookup table");
	for (const cw_entry_t * cw_entry = CW_TABLE; cw_entry->character; cw_entry++) {

		/* Initialize lookup table for main characters. */
		g_main_table_fast_lookup[(unsigned char) cw_entry->character] = cw_entry;

		/* Calculate the length of the longest representation in main lookup table. */
		const size_t length = strlen(cw_entry->representation);
		if (length > g_main_table_maximum_representation_length) {
			g_main_table_maximum_representation_length = length;
		}

		/* Calculate count of characters in main lookup table. */
		g_main_table_characters_count++;
	}



	cw_debug_msg (&cw_debug_object, CW_DEBUG_LOOKUPS, CW_DEBUG_INFO,
		      MSG_PREFIX "initialize prosign fast lookup table");
	for (const cw_prosign_entry_t * entry = g_prosign_table; entry->character; entry++) {

		/* Initialize lookup table for procedural characters. */
		g_prosign_table_fast_lookup[(unsigned char) entry->character] = entry;

		/* Calculate the length of the longest expansion of prosign. */
		const size_t length = strlen(entry->expansion);
		if (length > g_prosign_table_maximum_expansion_length) {
		        g_prosign_table_maximum_expansion_length = length;
		}

		/* Calculate the count of procedural characters. */
		g_prosign_table_characters_count++;
	}



	/* Calculate the length of the longest phonetic. */
	for (int phonetic = 0; g_phonetics_table[phonetic]; phonetic++) {
		const size_t length = strlen(g_phonetics_table[phonetic]);
		if (length > g_phonetics_table_maximum_phonetic_length) {
			g_phonetics_table_maximum_phonetic_length = length;
		}
	}



	cw_debug_msg (&cw_debug_object, CW_DEBUG_INTERNAL, CW_DEBUG_INFO,
		      MSG_PREFIX "constructor for 'data' module completed");
}
//@endcond
