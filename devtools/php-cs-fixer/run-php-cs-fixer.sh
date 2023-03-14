#!/bin/sh
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2023-2023 Bareos GmbH & Co. KG
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

DIR="$(dirname $0)"
cd "${DIR}"

./vendor/bin/php-cs-fixer fix ../../webui \
    -v \
    --rules=@PSR12 \
    --stop-on-violation "$@"

# Exit code of the fix command is built using following bit flags:
# 0 - OK.
# 1 - General error (or PHP minimal requirement not matched).
# 4 - Some files have invalid syntax (only in dry-run mode).
# 8 - Some files need fixing (only in dry-run mode).
# 16 - Configuration error of the application.
# 32 - Configuration error of a Fixer.
# 64 - Exception raised within the application.

exit $?
