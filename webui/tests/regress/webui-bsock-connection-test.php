<?php

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

var_dump($bsock);

$result = $bsock->connect_and_authenticate();

if($result)
   exit(0);
else
   exit(1);

?>
