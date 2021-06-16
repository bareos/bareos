<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

$user = isset($_SERVER['PHP_AUTH_USER']) ? $_SERVER['PHP_AUTH_USER'] : null;
$pass = isset($_SERVER['PHP_AUTH_PW']) ? $_SERVER['PHP_AUTH_PW'] : null;
$guser = isset($_GET['user']) ? $_GET['user'] : null;
$gpass = isset($_GET['pass']) ? $_GET['pass'] : null;
$method = isset($_GET['method']) ? $_GET['method'] : 'Basic';

if (! $user || ! $pass || $user != $guser || $pass != $gpass) {
    header('WWW-Authenticate: ' . $method . ' realm="ZendTest"');
    header('HTTP/1.0 401 Unauthorized');
}

echo serialize($_GET), "\n", $user, "\n", $pass, "\n";
