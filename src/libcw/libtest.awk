#!/bin/awk -f
#
# Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
# Copyright (C) 2011-2021  Kamil Ignacak (acerion@wp.pl)
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
#
# Simple AWK script to produce documentation suitable for processing into
# man pages from a C source file.
# Feed output of this script to libsigs.awk and libfuncs.awk to get
# file with function signatures, and file with function
# signatures+documentation respectively.
#





# Initialize the states, tags, and indexes
BEGIN {
	IDLE = 0
	DOCUMENTATION = 1
	FUNCTION_SPECIFICATION = 2
	FUNCTION_BODY = 3
	state = IDLE

	# Iterator for functions
	fu = 0
}





function handle_global_space()
{
	do {
		# Pass.
	} while ($0 !~ /^\/\*\*/ && getline)

	# caught beginning of documentation block (or end of file)

	output_line = 0;
}







function handle_function_specification()
{
	# catch function's name
	start = match($0, /[a-zA-Z0-9_\* ]+ \**([a-zA-Z0-9_]+)\(/, ary);
	if (RSTART > 0) {
		len = RLENGTH
		name = ary[1]
		if (match(name, /^test_/)) {
			# Test function, skip.
		} else {
			all_functions[name"()"] = name"()"
			# print name
		}
	}

	do {
		# pass
	} while ($0 !~ /\)$/ && getline)
}





function handle_function_documentation()
{
	while ($0 !~ /^ *\*\//) {
		start = match($0, /test::([0-9a-zA-Z_()]+)/, ary)
		if (RSTART > 0) {
			name = ary[1]
			tested_functions[name] = name
		}
		getline
	}
}





function handle_function_body()
{
	# Ignore function body lines, but watch for a bracket that
	# closes a function
	while ($0 !~ /^\}/) {
		# read and discard lines of function body
		getline
	}
}





function print_documentation_and_specification()
{
	# Print out the specification and documentation lines we have found;
	# reorder documentation and specification so that documentation
	# lines come after the function signatures.

	for (i in all_functions) {
		if (tested_functions[all_functions[i]]) {
			print "tested:      "all_functions[i]
		} else {
			print "waiting:     "all_functions[i]
		}
	}
}





# Ignore all blank lines outside of comments and function bodies
/^[[:space:]]*$/ {
	if (state == IDLE) {
		next
	}
}





# Handle every other line in the file according to the state;
# This is the main 'loop' of the script.
{
	# Process static function declarations and change
	# state on '^/**'
	if (state == IDLE) {
		handle_global_space()
		state = DOCUMENTATION
		next
	}


	# Process function documentation blocks, stopping on ' */'.
	if (state == DOCUMENTATION) {
		handle_function_documentation()
		state = FUNCTION_SPECIFICATION
		next
	}


	# Process function specification line(s), stopping on ')$'.
	if (state == FUNCTION_SPECIFICATION) {
		handle_function_specification()
		state = FUNCTION_BODY
		next
	}


	# Process function body, stopping on '^}'
	if (state == FUNCTION_BODY) {
		handle_function_body()
		state = IDLE
	}

	# prepare for next 'documentation + specification' section
	state = IDLE
}





# Simply dump anything we have so far on end of file.
END {
	print_documentation_and_specification()
}
