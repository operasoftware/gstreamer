#!/bin/sh
# Run this to generate all the initial makefiles, etc.

DIE=0
package=GStreamer
srcfile=gst/gstobject.h

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $package."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile $package."
	echo "Get ftp://ftp.cygnus.com/pub/home/tromey/automake-1.2d.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

(libtool --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have libtool installed to compile $package."
	echo "Get ftp://alpha.gnu.org/gnu/libtool-1.3.5.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

libtool_version=`libtool --version | sed 's/^.* \([0-9\.]*\) .*$/\1/'`
libtool_major=`echo $libtool_version | cut -d. -f1`
libtool_minor=`echo $libtool_version | cut -d. -f2`
libtool_micro=`echo $libtool_version | cut -d. -f3`
if [ $libtool_major -lt 1 -o $libtool_minor -lt 3 -o $libtool_micro -lt 5 ];then
	echo
	echo "You must have libtool 1.3.5 or greater to compile $packate."
	echo "Get ftp://alpha.gnu.org/gnu/libtool-1.3.5.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
fi

if test "$DIE" -eq 1; then
	exit 1
fi

test -f $srcfile || {
	echo "You must run this script in the top-level $package directory"
	exit 1
}

if test -z "$*"; then
	echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi

libtoolize --copy --force
aclocal $ACLOCAL_FLAGS
autoheader
autoconf
automake --add-missing

if [ "x$1" = "x--autogen-recurse" ];then
  exit	# the rest will happen later
fi

#for dir in `find * -name autogen.sh -print | grep -v '^autogen.sh$' | \
#            sed 's/autogen.sh$//'`;do
#  echo "Recursively running autogen.sh in $dir"
#  pushd $dir > /dev/null
#  ./autogen.sh --autogen-recurse "$@"
#  popd > /dev/null
#done

./configure --enable-maintainer-mode --enable-plugin-srcdir --enable-debug-verbose "$@"

echo 
echo "Now type 'make' to compile $package."
