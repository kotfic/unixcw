#!/bin/bash

executable_name=libcw_test_all
executable_fullpath=src/libcw/tests/.libs/libcw_test_all
executable_args="-A t -S n"


timestamp=`date '+%Y.%m.%d_%H.%M.%S'`
log_file_name=log_valgrind_"$executable_name"_"$timestamp".log


echo "Executable name:     " $executable_name     >  $log_file_name
echo "Executable fullpath: " $executable_fullpath >> $log_file_name
echo "Executable args:     " $executable_args     >> $log_file_name
echo ""                                           >> $log_file_name


LD_LIBRARY_PATH=./src/libcw/.libs valgrind --tool=memcheck \
	 --leak-check=yes \
	 --leak-check=full \
	 -v \
	 --show-reachable=yes \
	 --track-origins=yes \
	 --num-callers=20 \
	 --track-fds=yes  \
	 $executable_fullpath $executable_args $@ 2>>$log_file_name


tail -n 20 $log_file_name
echo ""
echo "Log file:" $log_file_name

