#!/bin/sh
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2020 Bareos GmbH & Co. KG
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

POTFILE='../module/Application/language/webui.pot'
LOCDIR='../module/Application/language/'

echo 'Message lookup ...'

find ../ -regextype posix-egrep -regex '.*(php|phtml)$$' | grep -v vendor | grep -v tests | xargs xgettext --keyword=translate -L PHP --from-code=UTF-8 -o $POTFILE;
find ../ -regextype posix-egrep -regex '.*(formatter.js|functions.js)$$' | xargs xgettext --keyword=gettext --from-code=UTF-8 -j -o $POTFILE;

echo 'Message merge ...'
cd $LOCDIR
for i in cn_CN cs_CZ de_DE en_EN es_ES fr_FR hu_HU it_IT nl_BE pl_PL pt_BR ru_RU sk_SK tr_TR uk_UA; do echo $i && msgmerge --backup=none -U $i.po webui.pot && touch $i.po; done;

#echo 'Message format ...'
#for i in cn_CN cs_CZ de_DE en_EN es_ES fr_FR hu_HU it_IT nl_BE pl_PL pt_BR ru_RU sk_SK tr_TR uk_UA; do echo $i && msgfmt $i.po --output-file=$i.mo; done;

echo 'Done'
