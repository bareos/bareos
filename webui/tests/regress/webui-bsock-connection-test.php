<?php
/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2023 Bareos GmbH & Co. KG (http://www.bareos.org/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

namespace Bareos\BSock;

require_once 'init_autoloader.php';

require_once 'vendor/Bareos/library/Bareos/BSock/BareosBase64.php';
require_once 'vendor/Bareos/library/Bareos/BSock/BareosBSockInterface.php';
require_once 'vendor/Bareos/library/Bareos/BSock/BareosBSock.php';

use Zend\ServiceManager\FactoryInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

$config = array(
      'debug' => false,
      'host' => 'localhost',
      'port' => 8101,
      'password' => 'admin',
      'console_name' => 'admin',
      'pam_password' => null,
      'pam_username' => null,
      'tls_verify_peer' => false,
      'server_can_do_tls' => false,
      'server_requires_tls' => false,
      'client_can_do_tls' => false,
      'client_requires_tls' => false,
      'ca_file' => null,
      'cert_file' => null,
      'cert_file_passphrase' => null,
      'allowed_cns' => null,
      'catalog' => null,
);

$bsock = new BareosBSock();

foreach ($config as $key => $value) {
    $bsock->set_config_param($key, $value);
}

if ($argv[1] === 1) {
    var_dump($bsock);
}

$result = $bsock->connect_and_authenticate();

if ($result) {
    exit(0);
} else {
    exit(1);
}
