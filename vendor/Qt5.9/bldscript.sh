#!/bin/bash

# General configuration globals.
_DIRBACK_BLDSCRIPT="$(pwd)"
_TMPOUTPATH_GIT="/tmp/_qt5build_temp"

# Remove anything from a previous run.
rm -r "$_TMPOUTPATH_GIT"

# Fetch the source code.
mkdir -p "$_TMPOUTPATH_GIT"
cd "$_TMPOUTPATH_GIT"
git clone git://code.qt.io/qt/qt5.git
cd qt5

# Set-up codebase.
git checkout 5.9
perl init-repository --module-subset=qtbase,qtimageformats
./configure -static -release -platform linux-g++ -opensource \
    -nomake examples -nomake tests -no-opengl \
    -qt-zlib -no-mtdev -qt-libpng -qt-libjpeg -qt-freetype -qt-harfbuzz -dbus-linked \
    -prefix "$_TMPOUTPATH_GIT/install"

# Perform the build.
make
make install

cd ../install/lib

# Move all static lib files into their right place.
_LIBTARGET_DIR="$_DIRBACK_BLDSCRIPT/lib/linux"

find . -name "*.a" -exec mkdir -p "$(dirname "$_LIBTARGET_DIR/{}")" \; -exec mv "{}" "$(dirname "$_LIBTARGET_DIR/{}")" \;
cd ..
mv plugins "$_LIBTARGET_DIR"

# Remove the temporary output directory.
rm -r -f "$_TMPOUTPATH_GIT"

# Reinstate the current directory.
cd "$_DIRBACK_BLDSCRIPT"