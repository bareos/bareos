#!/bin/sh
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2023 Bareos GmbH & Co. KG
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

#
# This script serves as a helper to keep our POT and PO files up to date after source code changes.
#

set -e
set -u

cd "$(dirname $0)/.."

POTFILE='module/Application/language/webui.pot'
LOCDIR='module/Application/language/'

echo 'Message lookup ...'
find . \( -name "*.php" -o -name "*.phtml" \) -not -path "vendor/*" -not -path "tests/*" | sort | xargs xgettext --keyword=translate -L PHP --from-code=UTF-8 --sort-output -o $POTFILE
xgettext --keyword=gettext --from-code=UTF-8 -j -o $POTFILE public/js/bootstrap-table-formatter.js public/js/custom-functions.js


echo 'Message merge ...'
cd $LOCDIR
for i in *.po; do
    printf "%s " "$i"
    msgmerge --backup=none --sort-output --update $i webui.pot
    # msgfmt $i --output-file=$i.mo
done

echo 'Done'
