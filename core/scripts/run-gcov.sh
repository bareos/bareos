#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2018-2025 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

SRCDIR=$(pwd)

for FILE in $(find . -name \*.c?); do
  #echo $FILE

  FILENAME=$(basename $FILE)
  DIRECTORY=$(dirname $FILE)
  GCNO_FILE=$(find . -name $FILENAME.gcno | grep $DIRECTORY)

  if [ ! -z "$GCNO_FILE" ]; then
    pushd $DIRECTORY
    gcov $FILENAME -o $SRCDIR/$GCNO_FILE
    popd
  else
    echo "$file: no coverage gcno file found"
  fi
done
