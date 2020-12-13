# Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
# Copyright (C) 2011-2020  Kamil Ignacak (acerion@wp.pl)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

-include $(top_builddir)/Makefile.inc




TESTS = $(check_SCRIPTS)

# targets to be built in this directory
check_PROGRAMS = libcw_tests

# run test programs
check_SCRIPTS = libcw_test_quick.sh

libcw_test_quick.sh:
# empty




# List of files that implement tests of specific bug fixes.
bug_test_files = \
	libcw_test_tq_short_space.c \
	libcw_test_tq_short_space.h

test_framework_files = \
	test_framework.c \
	test_framework.h \
	test_framework_tools.c \
	test_framework_tools.h

modern_api_test_files = \
	libcw_data_tests.c \
	libcw_data_tests.h \
	libcw_gen_tests.c \
	libcw_gen_tests.h \
	libcw_gen_tests_state_callback.c \
	libcw_gen_tests_state_callback.h \
	libcw_rec_tests.c \
	libcw_rec_tests.h \
	libcw_utils_tests.c \
	libcw_utils_tests.h \
	libcw_key_tests.c \
	libcw_key_tests.h \
	libcw_debug_tests.c \
	libcw_debug_tests.h \
	libcw_tq_tests.c \
	libcw_tq_tests.h

legacy_api_test_files = \
	libcw_legacy_api_tests.c \
	libcw_legacy_api_tests.h \
	libcw_legacy_api_tests_rec_poll.c \
	libcw_legacy_api_tests_rec_poll.h




libcw_tests_SOURCES = \
	$(legacy_api_test_files) \
	$(modern_api_test_files) \
	$(bug_test_files) \
	$(test_framework_files) \
	test_data.c \
	test_sets.c \
	test_main.c

libcw_tests_CPPFLAGS = $(AM_CPPFLAGS) -DLIBCW_UNIT_TESTS
#libcw_tests_CFLAGS = -I../../

# Target-specific linker flags (objects to link). Order is important:
# first static libraries then dynamic. Otherwise linker may not find
# symbols from the dynamic library.
libcw_tests_LDADD  = $(top_builddir)/src/cwutils/lib_libcw_tests.a
libcw_tests_LDADD += $(top_builddir)/src/cwutils/lib_rec_tests.a
libcw_tests_LDADD += $(INTL_LIB) -lm -lpthread $(DL_LIB) -L../.libs -lcw_test




EXTRA_DIST = \
	$(check_SCRIPTS) \
	count_functions_under_test.py




# sources, references
#
# source of snippet related to "check_SCRIPTS" and related sh script:
# http://www.freesoftwaremagazine.com/books/agaal/automatically_writing_makefiles_with_autotools
