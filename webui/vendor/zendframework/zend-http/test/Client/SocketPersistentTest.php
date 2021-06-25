<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Client;

/**
 * This Testsuite includes all Zend_Http_Client that require a working web
 * server to perform. It was designed to be extendable, so that several
 * test suites could be run against several servers, with different client
 * adapters and configurations.
 *
 * Note that $this->baseuri must point to a directory on a web server
 * containing all the files under the files directory. You should symlink
 * or copy these files and set 'baseuri' properly.
 *
 * You can also set the proper constand in your test configuration file to
 * point to the right place.
 *
 * @group      Zend_Http
 * @group      Zend_Http_Client
 */
use Zend\Http\Client\Adapter\Socket;

class SocketPersistentTest extends SocketTest
{
    /**
     * Configuration array
     *
     * @var array
     */
    protected $config = [
        'adapter'    => Socket::class,
        'persistent' => true,
        'keepalive'  => true,
    ];
}
