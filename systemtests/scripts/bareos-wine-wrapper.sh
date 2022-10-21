#!/bin/bash

#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2022 Bareos GmbH & Co. KG
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

set -e
set -o pipefail
set -u

debug()
{
    printf "$*" >&2
}

should_run_in_foreground()
{
    local APP="$(basename ${0})"
    debug "$APP $@: "
    case $APP in
    bareos-dir|bareos-sd|bareos-fd)
        debug "potentially background -> "
        ;;
    bareos_dir-*|bareos_sd-*|bareos_fd-*)
        debug "potentially background -> "
        ;;
    *)
        debug "foreground\n"
        return 0
        ;;
    esac
    for i in "$@"; do
        case $i in
        -f|-t|--xc|--xs)
        debug "foreground\n"
            return 0
            ;;
        esac
    done
    debug "background\n"
    return 1
}

run_bconsole()
{
    # bconsole does not work with Wine,
    # neither interactively nor as batch
    # (wine bconsole.exe, wine start bconsole.exe, wineconsole bconsole).
    # Tested on Ubuntu 20.04 (wine-7.19), Fedora 35 (wine-6.18).
    # While it seams to connect, no input prompt appears.
    # Therefore we use the bconsole command (hopefully) installed on the system.
    # However, there are reports it works with  Fedora 36 (wine-7.18, wine-7.18-2.fc36.x86_64).

    #
    # Windows paths must be converted to Linux paths.
    #
    local array=()
    local i=0
    for p in "$@"; do
        array+=($(sed 's/^[a-zA-Z]://' <<<${p}))
    done
    exec bconsole "${array[@]}"
}

APP="$(basename ${0})"
case $APP in
bconsole-*)
    run_bconsole "$@"
esac

if should_run_in_foreground "$@"; then
    exec wine ${0}.exe "$@"
else
    exec wine start /b /unix ${0}.exe "$@"
fi

#
# doc
#

# wine or winconsole
# see https://wiki.winehq.org/Wine_User%27s_Guide#Text_mode_programs_.28CUI:_Console_User_Interface.29
# (does not work as documented, outdated?)

# $ wine start /?
# Usage:
# start [options] program_filename [...]
# 
# Options:
# "title"        Specifies the title of the child windows.
# /d directory   Start the program in the specified directory.
# /b             Don't create a new console for the program.
# /i             Start the program with fresh environment variables.
# /min           Start the program minimized.
# /max           Start the program maximized.
# [...]
# /wait          Wait for the started program to finish, then exit with its
# exit code.
# /unix          Use a Unix filename and start the file like Windows Explorer.
# /exec          Exec the specified file (for internal Wine usage).
# /ProgIDOpen    Open a document using the specified progID.
# /?             Display this help and exit.
