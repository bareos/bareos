#!/bin/bash

#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2024 Bareos GmbH & Co. KG
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
# A set of useful functions to be sourced to run mysql/mariadb.
#
# Requirements:
#   Environment variables:
#     dbHost
#     MYSQL_DAEMON_BINARY
#     MYSQL_CLIENT_BINARY
#   defaults-file named "mysqldefaults" in test directory.
#
# See tests/py2plug-fd-mariabackup/
# and especially tests/py2plug-fd-mariabackup/mysqldefaults.in
# as a reference.
#

mysql_cleanup()
{
    mysql_server_stop

    rm -Rf mysql/data/*
    mkdir -p mysql/data/
    # directory for socket, depending on length (socket length is limited)
    rm -Rf ${dbHost}
    mkdir -p ${dbHost}
}

mysql_init()
{
    # creates "my.cnf" configuration file.
    # sets MYSQL_CLIENT variable.

    mysql_cleanup

    # initialize mysql db
    if ${MYSQL_DAEMON_BINARY} --verbose --help | grep -q initialize-insecure; then
        echo "MySQL init with --initialize-insecure"
        ${MYSQL_DAEMON_BINARY} --defaults-file=mysqldefaults --initialize-insecure --user="$USER" 2>&1 | tee mysql/mysql_init.log
        printf "[client]\nsocket=%s/mysql.sock\nuser=%s\n" "${dbHost}" "root" > my.cnf
        MYSQL_CLIENT="${MYSQL_CLIENT_BINARY} --defaults-file=my.cnf"
    else
        echo "MySQL init with mysql_install_db"
        mysql_install_db --auth-root-authentication-method=socket --user=$USER --auth-root-socket-user=$USER --defaults-file=mysqldefaults > mysql/mysql_init.log
        printf "[client]\nsocket=%s/mysql.sock\nuser=%s\n" "${dbHost}" "$USER" > my.cnf
        MYSQL_CLIENT="${MYSQL_CLIENT_BINARY} --defaults-file=my.cnf"
    fi
}

mysql_server_start()
{
    if ! [ "${MYSQL_CLIENT:-}" ]; then
        echo "MYSQL_CLIENT not defined. mysql_init not called?"
        return 1
    fi

    if ! [ -e my.cnf ]; then
        echo "Config file 'my.cnf' does not exist. mysql_init not called?"
        return 1
    fi

    "${MYSQL_DAEMON_BINARY}" --defaults-file=mysqldefaults >mysql/mysql.log 2>&1 &

    tries=60
    printf "waiting for MySQL server to start "
    while ! ${MYSQL_CLIENT} <<< "select version();" >/dev/null 2>&1; do
        [ $((tries-=1)) -eq 0 ] && {
            echo "Could not start MySQL server"
            cat mysql/mysql.log
            mysql_cleanup
            exit 1
        }
        printf "."
        sleep 1
    done
    printf " OK\n"
}

get_mysql_server_pid()
{
    if [ -f "mysql/mysqld.pid" ]; then
        cat "mysql/mysqld.pid"
    fi
}

mysql_server_stop()
{
    printf "shutdown MySQL server "
    local PID=$(get_mysql_server_pid)
    if [ "$PID" ]; then
        kill $PID
        waitpid $PID
    fi
}
