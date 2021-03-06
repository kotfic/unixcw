#!/bin/bash

PACKAGE="unixcw"
VERSION="3.6.0"

# https://lintian.debian.org/tags/bad-distribution-in-changes-file.html
# https://bugs.launchpad.net/ubuntu/+source/lintian/+bug/1303603
# https://manpages.debian.org/jessie/devscripts/debuild.1.en.html
# /usr/share/lintian/profiles
VENDOR="debian"
LINTIAN_OPTS="--lintian-opts --profile $VENDOR"

debuild_command='debuild -us -uc $LINTIAN_OPTS'




echo ""
echo ""
echo "********** Preparing directories **********"
sleep 1;
cd ..
REPO=`pwd`
if [ -z "$REPO"  ]; then
    echo "REPO is empty, exiting"
    exit
elif [ "$REPO" == "/" ]; then
    echo "REPO is root directory, exiting"
    exit
fi





cd ../../build
if [ $? == 1 ] ; then
    echo "Failed to enter build directory, exiting"
    exit;
fi
BUILD=`pwd`
if [ ! -d $BUILD ]; then
    echo "Build directory $BUILD doesn't exist, exiting"
    exit;
fi





echo "REPO =  " $REPO
echo "BUILD = " $BUILD

# "-n 1" - no need to confirm a character with Enter
read -r -n 1 -p "Directories are OK? [y/N] " response
echo
case $response in
    [yY][eE][sS]|[yY])
        echo "OK"
        ;;
    *)
        echo "Aborting build"
        exit
        ;;
esac





# Clean up old files.
echo ""
echo ""
echo "********** Cleaning up build directory **********"
sleep 1;
rm -rf $BUILD/*




echo ""
echo ""
echo "********** Running 'mmake dist' in repository **********"
sleep 1;
# prepare $PACKAGE_X.Y.Z.debian.tar.gz
cd $REPO
rm -rf *.tar.gz

if [ ! -f Makefile ]; then
    ./configure
fi

# 'make' returns zero on success
if ! (make dist) ; then
    echo "Failed to build 'make dist' target, exiting"
    echo `pwd`
    exit
fi


dist=`ls *tar.gz`
if [ -z "$dist"  ]; then
    echo "Failed to find distribution archive, exiting"
    exit
else
    echo "distribution archive = $dist"
fi





echo ""
echo ""
echo "********** Moving distribution to build directory **********"
sleep 1;
mv $dist $BUILD/
cd $BUILD





tar xvfz $dist
rm $dist
dist_dir=`ls`
echo "directory with contents of distribution package is $dist_dir"
sleep 1;





echo ""
echo ""
echo "********** Preparing archives in build directory **********"
sleep 1;
tar cvfz $PACKAGE\_$VERSION.debian.tar.gz $dist_dir/debian
tar cvfz $PACKAGE\_$VERSION.orig.tar.gz $dist_dir --exclude=debian





echo ""
echo ""
echo "********** Starting build **********"
sleep 1;
# go to final build dir and start building Debian package
cd $dist_dir
echo "dist_dir = $dist_dir, pwd = `pwd`"
eval $debuild_command





# "-n 1" - no need to confirm a character with Enter
read -r -n 1 -p "Test second build in build directory? [y/N] " response
echo
case $response in
    [yY][eE][sS]|[yY])
        echo "OK"
        eval $debuild_command
        echo "Second build completed"
        ;;
    *)
        echo "Not executing second build"
        # pass
        ;;
esac

echo
