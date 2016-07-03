#!/bin/sh

set -x
rm -rf config.cache autom4te.cache

aclocal
libtoolize --automake
autoheader
automake -a 
autoconf
