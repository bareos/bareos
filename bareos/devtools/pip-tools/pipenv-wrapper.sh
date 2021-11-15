#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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


pipenv_cmd="$(type -p pipenv)"
if [ ! -x "$pipenv_cmd" ]; then
  echo "This program requires pipenv, install using pip install pipenv" >&2
  exit 3
fi

cmd="$(basename "$0" .sh)"

PIPENV_PIPFILE="$(dirname "$(realpath "$0")")/Pipfile"
export PIPENV_PIPFILE

exec "$pipenv_cmd" run $cmd "$@"
