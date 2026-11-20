<?php
//   BAREOS - Backup Archiving REcovery Open Sourced
//
//   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
//
//   This program is Free Software; you can redistribute it and/or
//   modify it under the terms of version three of the GNU Affero General Public
//   License as published by the Free Software Foundation and included
//   in the file LICENSE.
//
//   This program is distributed in the hope that it will be useful, but
//   WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//   Affero General Public License for more details.
//
//   You should have received a copy of the GNU Affero General Public License
//   along with this program; if not, write to the Free Software Foundation,
//   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

namespace Bareos\BSock {
    function stream_socket_client($remote, &$error, &$message, $timeout, $flags, $context)
    {
        $error = 13;
        $message = 'Permission denied';
        return false;
    }
}

namespace {
    $loader = require __DIR__ . '/../../vendor/autoload.php';
    $loader->addPsr4('Bareos\\', __DIR__ . '/../../vendor/Bareos/library/Bareos');

    $connection = new \Bareos\BSock\BareosBSock();
    $connection->set_config_param('host', '127.0.0.1');
    $connection->set_config_param('port', 9101);
    try {
        $connection->connect_and_authenticate();
        fwrite(STDERR, "FAIL: permission-denied connection did not fail\n");
        exit(1);
    } catch (\Exception $error) {
        if (
            !($error instanceof \Bareos\BSock\ConnectionException)
            || $error->getCode() !== 13
            || strpos($error->getMessage(), 'Permission denied') === false
        ) {
            fwrite(STDERR, "FAIL: connection failure lost its category or errno\n");
            exit(1);
        }
    }
    echo "Permission-denied connection classification passed\n";
}
