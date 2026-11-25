<?php
//   BAREOS® - Backup Archiving REcovery Open Sourced
//
//   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
//
//   This program is Free Software; you can redistribute it
//   and/or modify it under the terms of version three of the GNU Affero
//   General Public License as published by the Free Software Foundation
//   and included in the file LICENSE.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU Affero General Public License for more details.
//
//   You should have received a copy of the GNU Affero General Public License
//   along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Load test dependencies without the application-wide PHP platform check.
require_once __DIR__ . '/../../vendor/composer/ClassLoader.php';
$loader = new Composer\Autoload\ClassLoader();
foreach (require __DIR__ . '/../../vendor/composer/autoload_psr4.php' as $prefix => $paths) {
    $loader->addPsr4($prefix, $paths);
}
$loader->register();

require_once __DIR__ . '/../../module/Restore/src/Restore/Model/Restore.php';

$input = (new Restore\Model\Restore())->getInputFilter()->get('where');
foreach (array('/tmp/restores', '/' . str_repeat('directory/', 20)) as $location) {
    $input->setValue($location);
    if (!$input->isValid() || $input->getValue() !== $location) {
        fwrite(STDERR, "Restore location rejected or modified\n");
        exit(1);
    }
}
$input->setValue('');
if ($input->isValid()) {
    fwrite(STDERR, "Empty restore location accepted\n");
    exit(1);
}
