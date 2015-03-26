#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(test -f $srcdir/configure.ac && test -f $srcdir/gnome-flashback.doap) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level gnome-flashback directory"
    exit 1
}

if test ! -f gnome-flashback/libsound-applet/gvc/Makefile.am;
then
	echo "+ Setting up submodules"
	git submodule init
fi
git submodule update

which gnome-autogen.sh || {
    echo "You need to install gnome-common."
    exit 1
}

. gnome-autogen.sh
