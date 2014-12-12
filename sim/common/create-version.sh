#!/bin/sh

# Copyright (C) 1989-2013 Free Software Foundation, Inc.

# This file is part of GDB.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Create version.c from version.in.
# Usage:
#    create-version.sh PATH-TO-COMMON-SIM-SRCDIR OUTPUT-FILE-NAME

srcdir="$1"
output="$2"

rm -f version.c-tmp $output version.tmp
date=`sed -n -e 's/^.* BFD_VERSION_DATE \(.*\)$/\1/p' $srcdir/../../bfd/version.h`
sed -e "s/DATE/$date/" < $srcdir/../../gdb/version.in > version.tmp
echo '#include "version.h"' >> version.c-tmp
echo 'const char version[] = "'"`sed q version.tmp`"'";' >> version.c-tmp
mv version.c-tmp $output
rm -f version.tmp
