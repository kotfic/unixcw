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




#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>




#include "test_framework.h"

#include "libcw_data.h"
#include "libcw_data_tests.h"
#include "libcw_debug.h"
#include "libcw_utils.h"
#include "libcw_key.h"
#include "libcw.h"
#include "libcw2.h"




/* For maximum length of 7, there should be 254 items:
   2^1 + 2^2 + 2^3 + ... + 2^7 */
#define REPRESENTATION_TABLE_SIZE ((1 << (CW_DATA_MAX_REPRESENTATION_LENGTH + 1)) - 2)




extern const cw_entry_t CW_TABLE[];
extern const char * test_valid_representations[];
extern const char * test_invalid_representations[];
extern const char * test_invalid_strings[];




static int test_phonetic_lookups_internal_sub(cw_test_executor_t * cte, char * phonetic_buffer);
static bool representation_is_valid(const char * representation);




/**
   The function builds every possible well formed representation no
   longer than 7 chars, and then calculates a hash of the
   representation. Since a representation is well formed, the tested
   function should calculate a hash.

   The function does not compare a representation and its hash to
   verify that patterns in representation and in hash match.

   TODO: add code that would compare the patterns of dots/dashes in
   representation against pattern of bits in hash.

   TODO: test calling the function with malformed representation.

   @reviewed on 2019-10-12
*/
int test_cw_representation_to_hash_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* Intended contents of input[] is something like that:
	  input[0]  = "."
	  input[1]  = "-"
	  input[2]  = ".."
	  input[3]  = "-."
	  input[4]  = ".-"
	  input[5]  = "--"
	  input[6]  = "..."
	  input[7]  = "-.."
	  input[8]  = ".-."
	  input[9]  = "--."
	  input[10] = "..-"
	  input[11] = "-.-"
	  input[12] = ".--"
	  input[13] = "---"
	  .
	  .
	  .
	  input[248] = ".-.----"
	  input[249] = "--.----"
	  input[250] = "..-----"
	  input[251] = "-.-----"
	  input[252] = ".------"
	  input[253] = "-------"
	*/
	char input[REPRESENTATION_TABLE_SIZE][CW_DATA_MAX_REPRESENTATION_LENGTH + 1];

	/* Build table of all well formed representations ("well
	   formed" as in "built from dash and dot, no longer than
	   CW_DATA_MAX_REPRESENTATION_LENGTH"). */
	long int rep_idx = 0;
	for (unsigned int rep_length = 1; rep_length <= CW_DATA_MAX_REPRESENTATION_LENGTH; rep_length++) {

		/* Build representations of all lengths, starting from
		   shortest (single dot or dash) and ending with the
		   longest representations. */

		unsigned int bit_vector_length = 1 << rep_length;

		/* A representation of length "rep_length" can have
		   2^rep_length distinct variants. The "for" loop that
		   we are in iterates over these 2^len variants.

		   E.g. bit vector (and representation) of length 2
		   has 4 variants:
		   ..
		   .-
		   -.
		   --
		*/
		for (unsigned int variant = 0; variant < bit_vector_length; variant++) {

			/* Turn every '0' in 'variant' into dot, and every '1' into dash. */
			for (unsigned int bit_pos = 0; bit_pos < rep_length; bit_pos++) {
				unsigned int bit = variant & (1 << bit_pos);
				input[rep_idx][bit_pos] = bit ? '-' : '.';
				// fprintf(stderr, "rep = %x, bit pos = %d, bit = %d\n", variant, bit_pos, bit);
			}

			input[rep_idx][rep_length] = '\0';
			//fprintf(stderr, "input[%ld] = \"%s\"\n", rep_idx, input[rep_idx]);
			rep_idx++;
		}
	}
	const long int n_representations = rep_idx;
	cte->expect_op_int(cte, n_representations, "==", REPRESENTATION_TABLE_SIZE, "internal count of representations");


	/* Compute hash for every well formed representation. */
	bool failure = false;
	for (int i = 0; i < n_representations; i++) {
		const uint8_t hash = LIBCW_TEST_FUT(cw_representation_to_hash_internal)(input[i]);
		/* The function returns values in range CW_DATA_MIN_REPRESENTATION_HASH - CW_DATA_MAX_REPRESENTATION_HASH. */
		if (!cte->expect_between_int_errors_only(cte, CW_DATA_MIN_REPRESENTATION_HASH, hash, CW_DATA_MAX_REPRESENTATION_HASH, "representation to hash: hash #%d\n", i)) {
			failure = true;
			break;
		}
	}
	cte->expect_op_int(cte, false, "==", failure, "representation to hash");


	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Verify that our fast lookup of characters works correctly.

   The verification is performed by comparing results of function
   using fast lookup table with results of function using direct
   method.

   @reviewed on 2019-10-12
*/
int test_cw_representation_to_character_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	bool failure = false;
	int i = 0;

	for (const cw_entry_t * cw_entry = CW_TABLE; cw_entry->character; cw_entry++) {

		const int char_fast_lookup = LIBCW_TEST_FUT(cw_representation_to_character_internal)(cw_entry->representation);
		const int char_direct = LIBCW_TEST_FUT(cw_representation_to_character_direct_internal)(cw_entry->representation);

		if (!cte->expect_op_int_errors_only(cte, char_fast_lookup, "==", char_direct, "fast lookup vs. direct method: '%s'", cw_entry->representation)) {
			failure = true;
			break;
		}


		/* Also test old version of cw_representation_to_character(). */
		{
			char char_old_lookup = 0;
			int cwret = LIBCW_TEST_FUT(cw_lookup_representation)(cw_entry->representation, &char_old_lookup);

			if (!cte->expect_op_int_errors_only(cte, cwret, "==", CW_SUCCESS,
							    "fast lookup vs. old method: conversion from representation to character for #%d (representation '%s')",
							    i, cw_entry->representation)) {
				failure = true;
				break;
			}

			if (!cte->expect_op_int_errors_only(cte, char_fast_lookup, "==", char_old_lookup, "fast lookup vs. old method: '%s'", cw_entry->representation)) {
				failure = true;
				break;
			}

			i++;
		}
	}

	cte->expect_op_int(cte, false, "==", failure, "representation to character");

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Testing speed gain between function using direct method, and
   function with fast lookup table.  Test is preformed by using timer
   to see how much time it takes to execute a function N times.

   @reviewed on 2019-10-12
*/
int test_cw_representation_to_character_internal_speed_gain(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);


	const int N = 1000;

	struct timeval start;
	struct timeval stop;


	gettimeofday(&start, NULL);
	for (int i = 0; i < N; i++) {
		for (const cw_entry_t * cw_entry = CW_TABLE; cw_entry->character; cw_entry++) {
			__attribute__((unused)) int character = cw_representation_to_character_internal(cw_entry->representation);
		}
	}
	gettimeofday(&stop, NULL);
	const int fast_lookup = cw_timestamp_compare_internal(&start, &stop);



	gettimeofday(&start, NULL);
	for (int i = 0; i < N; i++) {
		for (const cw_entry_t * cw_entry = CW_TABLE; cw_entry->character; cw_entry++) {
			__attribute__((unused)) int character = cw_representation_to_character_direct_internal(cw_entry->representation);
		}
	}
	gettimeofday(&stop, NULL);
	const int direct = cw_timestamp_compare_internal(&start, &stop);


	const float gain = 1.0 * direct / fast_lookup;
	const bool failure = gain < 1.1f;
	cte->expect_op_int(cte, false, "==", failure, "lookup speed gain: %.2f", (double) gain);  /* Casting to double to avoid compiler warning about implicit conversion from float to double. */

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @brief Test getting number of characters known to libcw

   @reviewed 2020-08-29
*/
cwt_retv test_data_main_table_get_count(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/*
	  libcw doesn't define a constant describing the number of
	  known/supported/recognized characters, but there is a function
	  calculating the number.

	  Two things are certain
	  - the number is larger than zero,
	  - it's not larger than ASCII table size.
	*/
	const int count = LIBCW_TEST_FUT(cw_get_character_count)();

	const int lower_inclusive = 1;
	const int upper_inclusive = 127;
	cte->expect_between_int(cte,
				lower_inclusive, count, upper_inclusive,
				"character count %d",
				count);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Test getting list of characters supported by libcw

   @reviewed 2020-08-29
*/
cwt_retv test_data_main_table_get_contents(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);


	char charlist[UCHAR_MAX + 1] = { 0 };
	LIBCW_TEST_FUT(cw_list_characters)(charlist);
	cte->log_info(cte, "list of characters: %s\n", charlist);

	/* Length of the list must match the character count returned by
	   library. */
	const int extracted_count = cw_get_character_count();
	const int extracted_len = (int) strlen(charlist);
	cte->expect_op_int(cte,
			   extracted_len, "==", extracted_count,
			   "character count = %d, list length = %d",
			   extracted_count, extracted_len);


	/* The 'sanity' of count of characters in charlist has been already
	   indirectly tested in tests of cw_get_character_count(). */

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Test getting maximum length of a representation (a string of Dots/Dashes)

   @reviewed on 2020-08-29
*/
cwt_retv test_data_main_table_get_representation_len_max(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	const int max_rep_length = LIBCW_TEST_FUT(cw_get_maximum_representation_length)();

	const int lower_inclusive = CW_DATA_MIN_REPRESENTATION_LENGTH;
	const int upper_inclusive = CW_DATA_MAX_REPRESENTATION_LENGTH;
	cte->expect_between_int(cte,
				lower_inclusive, max_rep_length, upper_inclusive,
				"maximum representation length (%d)",
				max_rep_length);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/* Simple helper function for validating string with representation. */
static bool representation_is_valid(const char * representation)
{
	if (NULL == representation) {
		return false;
	}

	const size_t len = strlen(representation);
	if (len < CW_DATA_MIN_REPRESENTATION_LENGTH) {
		return false;
	}
	if (len > CW_DATA_MAX_REPRESENTATION_LENGTH) {
		return false;
	}

	for (size_t i = 0; i < len; i++) {
		if (CW_DOT_REPRESENTATION != representation[i] && CW_DASH_REPRESENTATION != representation[i]) {
			return false;
		}
	}

	return true;
}




/**
   @brief Test functions looking up characters and their representation

   @reviewed on 2019-10-12
*/
cwt_retv test_data_main_table_lookups(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);


	char charlist[UCHAR_MAX + 1] = { 0 };
	cw_list_characters(charlist);

	const int max_rep_length = cw_get_maximum_representation_length();


	bool c2r_failure = false;
	bool r2c_failure = false;
	bool two_way_failure = false;
	bool length_failure = false;

	/* For each character, look up its representation, the
	   look up each representation in the opposite
	   direction. */

	for (int i = 0; charlist[i] != '\0'; i++) {

		const char input_character = charlist[i];
		char * representation = LIBCW_TEST_FUT(cw_character_to_representation)(input_character);
		if (!cte->expect_valid_pointer_errors_only(cte, representation,
							   "character to representation: conversion from character to representation (new) for #%d (char '%c')\n",
							   i, input_character)) {
			c2r_failure = true;
			break;
		}

		bool is_valid = representation_is_valid(representation);
		if (!cte->expect_op_int_errors_only(cte,
						    is_valid, "==", true,
						    "character to representation: validity of representation (new) for #%d (char '%c')",
						    i, input_character)) {
			c2r_failure = true;
			break;
		}


		/* Also test the old version of cw_character_to_representation(). */
		{
			/*
			  Two things on purpose:
			  - size larger than
			    (CW_DATA_MAX_REPRESENTATION_LENGTH+1) to be able
			    to look at how much is copied to the buffer.
			  - initialization with something other than NUL to
                            be able to look at what is copied to the buffer.
			*/
			char representation_old[2 * (CW_DATA_MAX_REPRESENTATION_LENGTH + 1)];
			memset(representation_old, 'a', sizeof (representation_old));

			int cwret = LIBCW_TEST_FUT(cw_lookup_character)(input_character, representation_old);

			if (!cte->expect_op_int_errors_only(cte, cwret, "==", CW_SUCCESS,
							    "character to representation: conversion from character to representation (old) for #%d (char '%c')",
							    i, input_character)) {
				c2r_failure = true;
				break;
			}

			is_valid = representation_is_valid(representation_old);
			if (!cte->expect_op_int_errors_only(cte,
							    is_valid, "==", true,
							    "character to representation: validity of representation (old) for #%d (char '%c')",
							    i, input_character)) {
				c2r_failure = true;
				break;
			}

			const int cmp = strcmp(representation, representation_old);
			if (!cte->expect_op_int_errors_only(cte, cmp, "==", 0,
							    "character to representation: result of new and old method for #%d: '%s' != '%s'",
							    i, representation, representation_old)) {
				c2r_failure = true;
				break;
			}
		}

		/* Here we convert the representation back into a character. */
		char reverse_character = LIBCW_TEST_FUT(cw_representation_to_character)(representation);
		if (!cte->expect_op_int_errors_only(cte, 0, "!=", reverse_character,
						    "representation to character: conversion from representation to character (new) for #%d (representation '%s')",
						    i, representation)) {
			r2c_failure = true;
			break;
		}
		/* Also test old version of cw_representation_to_character(). */
		{
			char old_reverse_character = 0;
			int cwret = LIBCW_TEST_FUT(cw_lookup_representation)(representation, &old_reverse_character);

			if (!cte->expect_op_int_errors_only(cte, cwret, "==", CW_SUCCESS,
							    "representation to character: conversion from representation to character (old) for #%d (representation '%s')",
							    i, representation)) {
				r2c_failure = true;
				break;
			}


			if (!cte->expect_op_int_errors_only(cte, reverse_character, "==", old_reverse_character,
							    "representation to character: result of new and old method for #%d: '%c' != '%c'",
							    i, reverse_character, old_reverse_character)) {
				r2c_failure = true;
				break;
			}
		}

		/* Compare output char with input char. */
		if (!cte->expect_op_int_errors_only(cte, reverse_character, "==", input_character, "character lookup: two-way lookup for #%d ('%c' -> '%s' -> '%c')", i, input_character, representation, reverse_character)) {
			two_way_failure = true;
			break;
		}

		const int length = (int) strlen(representation);
		const int rep_length_lower = 1; /* A representation will have at least one character. */
		const int rep_length_upper = max_rep_length;
		if (!cte->expect_between_int_errors_only(cte, rep_length_lower, length, rep_length_upper, "character lookup: representation length of character '%c' (#%d)", input_character, i)) {
			length_failure = true;
			break;
		}

		free(representation);
		representation = NULL;
	}

	cte->expect_op_int(cte, false, "==", c2r_failure, "character lookup: char to representation");
	cte->expect_op_int(cte, false, "==", r2c_failure, "character lookup: representation to char");
	cte->expect_op_int(cte, false, "==", two_way_failure, "character lookup: two-way lookup");
	cte->expect_op_int(cte, false, "==", length_failure, "character lookup: length");

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   \brief Test functions looking up procedural characters and their representation.

   @revieded on 2019-10-12
*/
int test_prosign_lookups_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* Collect and print out a list of characters in the
	   procedural signals expansion table. */


	/* Test: get number of prosigns known to libcw. */
	{
		const int count = LIBCW_TEST_FUT(cw_get_procedural_character_count)();
		cte->expect_op_int_errors_only(cte, 0, "<", count, "procedural character count (%d):", count);
	}



	char procedural_characters[UCHAR_MAX + 1] = { 0 };
	/* Test: get list of characters supported by libcw. */
	{
		LIBCW_TEST_FUT(cw_list_procedural_characters)(procedural_characters); /* TODO: we need a version of the function that accepts size of buffer as argument. */
		cte->log_info(cte, "list of procedural characters: %s\n", procedural_characters);

		const int extracted_len = (int) strlen(procedural_characters);
		const int extracted_count = cw_get_procedural_character_count();

		cte->expect_op_int(cte, extracted_count, "==", extracted_len, "procedural character count = %d, list length = %d", extracted_count, extracted_len);
	}



	/* Test: expansion length. */
	int max_expansion_length = 0;
	{
		max_expansion_length = LIBCW_TEST_FUT(cw_get_maximum_procedural_expansion_length)();
		cte->expect_op_int(cte, 0, "<", max_expansion_length, "maximum procedural expansion length (%d)", max_expansion_length);
	}



	/* Test: lookup. */
	{
		/* For each procedural character, look up its
		   expansion, verify its length, and check a
		   true/false assignment to the display hint. */

		bool lookup_failure = false;
		bool length_failure = false;
		bool expansion_failure = false;

		for (int i = 0; procedural_characters[i] != '\0'; i++) {
			char expansion[256] = { 0 };
			int is_usually_expanded = -1; /* This value should be set by libcw to either 0 (false) or 1 (true). */

			const int cwret = LIBCW_TEST_FUT(cw_lookup_procedural_character)(procedural_characters[i], expansion, &is_usually_expanded);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "procedural character lookup: lookup of character '%c' (#%d)", procedural_characters[i], i)) {
				lookup_failure = true;
				break;
			}

			const int length = (int) strlen(expansion);

			if (!cte->expect_between_int_errors_only(cte, 2, length, max_expansion_length, "procedural character lookup: expansion length of character '%c' (#%d)", procedural_characters[i], i)) {
				length_failure = true;
				break;
			}

			/* Check if call to tested function has modified the flag. */
			if (!cte->expect_op_int_errors_only(cte, -1, "!=", is_usually_expanded, "procedural character lookup: expansion hint of character '%c' ((#%d)", procedural_characters[i], i)) {
				expansion_failure = true;
				break;
			}
		}

		cte->expect_op_int(cte, false, "==", lookup_failure, "procedural character lookup: lookup");
		cte->expect_op_int(cte, false, "==", length_failure, "procedural character lookup: length");
		cte->expect_op_int(cte, false, "==", expansion_failure, "procedural character lookup: expansion flag");
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2020-07-25
*/
int test_phonetic_lookups_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* For each ASCII character, look up its phonetic and check
	   for a string that start with this character, if alphabetic,
	   and false otherwise. */

	/* Test: check that maximum phonetic length is larger than
	   zero. */
	{
		const int length = LIBCW_TEST_FUT(cw_get_maximum_phonetic_length)();
		const bool failure = (length <= 0);
		cte->expect_op_int(cte, false, "==", failure, "phonetic lookup: maximum phonetic length (%d)", length);
	}

	/* Test: lookup of phonetic + reverse lookup. */
	{
		char phonetic_buffer[sizeof ("VeryLongPhoneticString")] = { 0 };

		/* Tested function should work with NULL output buffer as well. */
		test_phonetic_lookups_internal_sub(cte, phonetic_buffer);
		test_phonetic_lookups_internal_sub(cte, NULL);
	}

	/* Test: simple test that the lookup is sane. */
	{
		bool simple_failure = false;
		char phonetic_buffer[sizeof ("VeryLongPhoneticString")] = { 0 };

		struct {
			char character;
			const char * string;
		} data[] = {
			{ 'a', "Alfa" },
			{ 'A', "Alfa" },
			{ 'z', "Zulu" },
			{ 'Z', "Zulu" },
			{  0,  ""     }}; /* Guard. */

		int i = 0;
		while (data[i].character) {
			const int cwret = LIBCW_TEST_FUT(cw_lookup_phonetic)((char) data[i].character, phonetic_buffer); /* TODO: we need a version of the function that accepts size argument. */
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "phonetic lookup: simple lookup for character '%c'", data[i].character)) {
				simple_failure = true;
				break;
			}

			int cmp = strcmp(phonetic_buffer, data[i].string);
			if (!cte->expect_op_int_errors_only(cte, 0, "==", cmp, "phonetic lookup: simple lookup for character '%c'/'%s' -> '%s'",
							    data[i].character, data[i].string, phonetic_buffer)) {
				simple_failure = true;
				break;
			}

			i++;
		}

		cte->expect_op_int(cte, false, "==", simple_failure, "phonetic lookup: simple lookup test");
	}


	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Tested function should correctly handle NULL and non-NULL @p phonetic_buffer

   @reviewed on 2020-07-25
*/
static int test_phonetic_lookups_internal_sub(cw_test_executor_t * cte, char * phonetic_buffer)
{
	bool lookup_failure = false;
	bool reverse_failure = false;

	/* Notice that we go here through all possible values
	   of char, not through all values returned from
	   cw_list_characters(). */
	for (int i = 0; i < UCHAR_MAX; i++) {

		const int cwret = LIBCW_TEST_FUT(cw_lookup_phonetic)((char) i, phonetic_buffer); /* TODO: we need a version of the function that accepts size argument. */
		const bool is_alpha = (bool) isalpha(i);
		if (CW_SUCCESS == cwret) {
			/*
			  Library claims that 'i' is a byte
			  that has a phonetic (e.g. 'F' ->
			  "Foxtrot").

			  Let's verify this using result of
			  isalpha().
			*/
			if (!cte->expect_op_int_errors_only(cte, true, "==", is_alpha, "phonetic lookup (A): lookup of phonetic for '%c' (#%d)", (char) i, i)) {
				lookup_failure = true;
				break;
			}
		} else {
			/*
			  Library claims that 'i' is a byte
			  that doesn't have a phonetic.

			  Let's verify this using result of
			  isalpha().
			*/
			if (!cte->expect_op_int_errors_only(cte, false, "==", is_alpha, "phonetic lookup (B): lookup of phonetic for '%c' (#%d)", (char) i, i)) {
				lookup_failure = true;
				break;
			}
		}


		if (CW_SUCCESS == cwret && is_alpha && NULL != phonetic_buffer) {
			/* We have looked up a letter, it has
			   a phonetic.  Almost by definition,
			   the first letter of phonetic should
			   be the same as the looked up
			   letter. */
			reverse_failure = (phonetic_buffer[0] != toupper((char) i));
			if (!cte->expect_op_int_errors_only(cte, false, "==", reverse_failure, "phonetic lookup: reverse lookup for phonetic \"%s\" ('%c' / #%d)", phonetic_buffer, (char) i, i)) {
				reverse_failure = true;
				break;
			}
		}
	}

	cte->expect_op_int(cte, false, "==", lookup_failure, "phonetic lookup: lookup");
	cte->expect_op_int(cte, false, "==", reverse_failure, "phonetic lookup: reverse lookup");

	return 0;
}



/**
   @brief Test validation of individual characters

   @reviewed on 2020-08-22
*/
int test_validate_character_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* Test: validation of individual characters. */

	bool failure_valid = false;
	bool failure_invalid = false;

	char charlist[UCHAR_MAX + 1];
	cw_list_characters(charlist);

	for (int i = 0; i < UCHAR_MAX; i++) {
		if (i == '\b') {
			/* For a short moment in development of post-3.5.1 I
			   had a code that treated backspace as special
			   character that removed last/previous character
			   from the queue. This special behaviour has been
			   removed before post-3.5.1 was published.

			   Backspace should be handled in User Interface,
			   e.g. by calling cw_gen_remove_last_character(),
			   not in internals of library.

			   Test that backspace is not treated as valid character. */
			{
				const bool is_valid = (bool) LIBCW_TEST_FUT(cw_check_character)(i);
				if (!cte->expect_op_int_errors_only(cte, false, "==", is_valid, "validate character (old): valid character <backspace> / #%d", i)) {
					failure_valid = true;
					break;
				}
			}
			{
				const bool is_valid = LIBCW_TEST_FUT(cw_character_is_valid)(i);
				if (!cte->expect_op_int_errors_only(cte, false, "==", is_valid, "validate character (new): character <backspace> / #%d", i)) {
					failure_valid = true;
					break;
				}
			}
		} else if (i == ' ' || (i != 0 && strchr(charlist, toupper(i)) != NULL)) {

			/* Here we have a valid character, that is
			   recognized/supported as 'sendable' by libcw.
			   cw_check_character()/cw_character_is_valid()
			   should confirm it. */
			{
				const bool is_valid = (bool) LIBCW_TEST_FUT(cw_check_character)(i);
				if (!cte->expect_op_int_errors_only(cte, true, "==", is_valid, "validate character (old): valid character '%c' / #%d not recognized as valid", (char ) i, i)) {
					failure_valid = true;
					break;
				}
			}
			{
				const bool is_valid = LIBCW_TEST_FUT(cw_character_is_valid)(i);
				if (!cte->expect_op_int_errors_only(cte, true, "==", is_valid, "validate character (new): valid character '%c' / #%d not recognized as valid", (char ) i, i)) {
					failure_valid = true;
					break;
				}
			}
		} else {
			/* The 'i' character is not recognized/supported by
			   libcw.
			   cw_check_character()/cw_character_is_valid()
			   should return false to signify that the char is
			   invalid. */
			{
				const bool is_valid = (bool) LIBCW_TEST_FUT(cw_check_character)(i);
				if (!cte->expect_op_int_errors_only(cte, false, "==", is_valid, "validate character (old): invalid character '%c' / #%d recognized as valid", (char ) i, i)) {
					failure_invalid = true;
					break;
				}
			}
			{
				const bool is_valid = LIBCW_TEST_FUT(cw_character_is_valid)(i);
				if (!cte->expect_op_int_errors_only(cte, false, "==", is_valid, "validate character (new): invalid character '%c' / #%d recognized as valid", (char ) i, i)) {
					failure_invalid = true;
					break;
				}
			}
		}
	}

	cte->expect_op_int(cte, false, "==", failure_valid, "validate character: valid characters");
	cte->expect_op_int(cte, false, "==", failure_invalid, "validate character: invalid characters");

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @brief Test validation of strings: valid strings and invalid strings

   @reviewed 2020-08-22
*/
int test_validate_string_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* Test: validation of string as a whole. */


	/* Check the whole library-provided character list item as a single
	   string.

	   TODO: we should have array of valid strings, like we have for
	   invalid strings below. */
	char charlist[UCHAR_MAX + 1];
	cw_list_characters(charlist);
	{
		const bool is_valid = (bool) LIBCW_TEST_FUT(cw_check_string)(charlist);
		cte->expect_op_int(cte, true, "==", is_valid, "validate string (old): valid string");
	}
	{
		const bool is_valid = LIBCW_TEST_FUT(cw_string_is_valid)(charlist);
		cte->expect_op_int(cte, true, "==", is_valid, "validate string (new): valid string");
	}


	/* Test invalid string. */
	{
		int i = 0;
		while (NULL != test_invalid_strings[i]) {
			const char * test_string = test_invalid_strings[i];
			const bool is_valid = LIBCW_TEST_FUT(cw_check_string)(test_string);
			cte->expect_op_int(cte, false, "==", is_valid, "validate string (old): invalid string %d/'%s'", i, test_string);
			i++;
		}
	}
	{
		int i = 0;
		while (NULL != test_invalid_strings[i]) {
			const char * test_string = test_invalid_strings[i];
			const bool is_valid = LIBCW_TEST_FUT(cw_string_is_valid)(test_string);
			cte->expect_op_int(cte, false, "==", is_valid, "validate string (new): invalid string %d/'%s'", i, test_string);
			i++;
		}
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @brief Test validation of representations of characters

   @reviewed 2020-08-22
*/
int test_validate_representation_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* Test: validating valid representations. */
	{
		int i = 0;
		bool failure = false;
		while (NULL != test_valid_representations[i]) {
			{
				const bool is_valid = (bool) LIBCW_TEST_FUT(cw_check_representation)(test_valid_representations[i]);
				if (!cte->expect_op_int_errors_only(cte, true, "==", is_valid, "valid representation (old) (i = %d)", i)) {
					failure = true;
					break;
				}
			}
			{
				const bool is_valid = LIBCW_TEST_FUT(cw_representation_is_valid)(test_valid_representations[i]);
				if (!cte->expect_op_int_errors_only(cte, true, "==", is_valid, "valid representation (new) (i = %d)", i)) {
					failure = true;
					break;
				}
			}
			i++;
		}
		cte->expect_op_int(cte, false, "==", failure, "valid representations");
	}


	/* Test: validating invalid representations. */
	{
		int i = 0;
		bool failure = false;
		while (NULL != test_invalid_representations[i]) {
			{
				const bool is_valid = (bool) LIBCW_TEST_FUT(cw_check_representation)(test_invalid_representations[i]);
				if (!cte->expect_op_int_errors_only(cte, false, "==", is_valid, "invalid representation (old) (i = %d)", i)) {
					failure = true;
					break;
				}
			}
			{
				const bool is_valid = LIBCW_TEST_FUT(cw_representation_is_valid)(test_invalid_representations[i]);
				if (!cte->expect_op_int_errors_only(cte, false, "==", is_valid, "invalid representation (new) (i = %d)", i)) {
					failure = true;
					break;
				}
			}
			i++;
		}
		cte->expect_op_int(cte, false, "==", failure, "invalid representations");
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}
